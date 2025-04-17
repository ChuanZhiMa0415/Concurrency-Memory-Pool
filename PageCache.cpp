#include "PageCache.h"
#include <mutex>

PageCache PageCache::_sInst;

Span* PageCache::mapObjectToSpan(void* obj)
{
	PageID id = ((PageID)obj >> PAGE_SHIFT);
	auto ret = (Span*)_pageidMapSpan.get(id);
	assert(ret != nullptr);
	return ret;
}

 // 获取一个K页的span
 Span *PageCache::getNewSpan(size_t k) {
     assert(k > 0);

     // 大于128 page的直接向堆申请
     if (k > MAX_PAGE_SIZE - 1) {
         void *ptr = SystemAlloc(k);
         // Span* span = new Span;
         Span *span = _spanPool._new();

         span->_pageID = (PageID)ptr >> PAGE_SHIFT;
         span->_pageCount = k;

         //_idSpanMap[span->_pageId] = span;
         _pageidMapSpan.set(span->_pageID, span);

         return span;
     }

     // 先检查第k个桶里面有没有span
     if (!_spanLists[k].empty()) {
         Span *kSpan = _spanLists[k].popFront();

         // 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
         for (PageID i = 0; i < kSpan->_pageCount; ++i) {
             //_idSpanMap[kSpan->_pageId + i] = kSpan;
             _pageidMapSpan.set(kSpan->_pageID + i, kSpan);
         }

         return kSpan;
     }

     // 检查一下后面的桶里面有没有span，如果有可以把他它进行切分
     for (size_t i = k + 1; i < MAX_PAGE_SIZE; ++i) {
         if (!_spanLists[i].empty()) {
             Span *nSpan = _spanLists[i].popFront();
             // Span* kSpan = new Span;
             Span *kSpan = _spanPool._new();

             // 在nSpan的头部切一个k页下来
             // k页span返回
             // nSpan再挂到对应映射的位置
             kSpan->_pageID = nSpan->_pageID;
             kSpan->_pageCount = k;

             nSpan->_pageID += k;
             nSpan->_pageCount -= k;

             _spanLists[nSpan->_pageCount].pushFront(nSpan);
             // 存储nSpan的首位页号跟nSpan映射，方便page cache回收内存时
             // 进行的合并查找
             //_idSpanMap[nSpan->_pageId] = nSpan;
             //_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;
             _pageidMapSpan.set(nSpan->_pageID, nSpan);
             _pageidMapSpan.set(nSpan->_pageID + nSpan->_pageCount - 1, nSpan);

             // 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
             for (PageID i = 0; i < kSpan->_pageCount; ++i) {
                 //_idSpanMap[kSpan->_pageId + i] = kSpan;
                 _pageidMapSpan.set(kSpan->_pageID + i, kSpan);
             }

             return kSpan;
         }
     }

     // 走到这个位置就说明后面没有大页的span了
     // 这时就去找堆要一个128页的span
     // Span* bigSpan = new Span;
     Span *bigSpan = _spanPool._new();
     void *ptr = SystemAlloc(MAX_PAGE_SIZE - 1);
     bigSpan->_pageID = (PageID)ptr >> PAGE_SHIFT;
     bigSpan->_pageCount = MAX_PAGE_SIZE - 1;

     _spanLists[bigSpan->_pageCount].pushFront(bigSpan);

     return getNewSpan(k);
 }

 void PageCache::releaseSpanToPageCache(Span* span) {
     if (span->_pageCount >= MAX_PAGE_SIZE) { // 如果这个 span 管理的内存过多，直接还给堆
         free((void*)(span->_pageID << PAGE_SHIFT));
         _spanPool._delete(span);
         return;
     }
     // _spanLists[span->_pageCount].erase(span);
     // 向前面的 span 合并
     while (true) {
         PageID prevPageid = span->_pageID - 1;
         //if (!_pageidMapSpan.count(prevPageid) ||
         //    _pageidMapSpan[prevPageid]->_isUsed ||
         //    _pageidMapSpan[prevPageid]->_pageCount + span->_pageCount >= MAX_PAGE_SIZE) break;
         
         Span* prevSpan = (Span*)_pageidMapSpan.get(prevPageid);
         if (prevSpan == nullptr || prevSpan->_isUsed || prevSpan->_pageCount + span->_pageCount >= MAX_PAGE_SIZE)
             break;

         // 合并 span 和 prevSpan
         span->_pageID = prevSpan->_pageID, span->_pageCount += prevSpan->_pageCount;
         _spanLists[prevSpan->_pageCount].erase(prevSpan), _spanPool._delete(prevSpan);
     }
     // 向后面的 span 合并
     while (true) {
         PageID nextPageid = span->_pageID + span->_pageCount;
         //if (!_pageidMapSpan.count(nextPageid) ||
         //    _pageidMapSpan[nextPageid]->_isUsed ||
         //    _pageidMapSpan[nextPageid]->_pageCount + span->_pageCount >= MAX_PAGE_SIZE) break;
         // 
         
         Span* nextSpan = (Span*)_pageidMapSpan.get(nextPageid);
         if (nextSpan == nullptr || nextSpan->_isUsed || nextSpan->_pageCount + span->_pageCount >= MAX_PAGE_SIZE) 
             break;

         // 合并 span 和 nextSpan
         span->_pageCount += nextSpan->_pageCount;
         _spanLists[nextSpan->_pageCount].erase(nextSpan), _spanPool._delete(nextSpan);
     }
     // 调整 span 在 page cache 中的位置
     span->_isUsed = false;
     _spanLists[span->_pageCount].pushFront(span);
     //_pageidMapSpan[span->_pageID] = _pageidMapSpan[span->_pageID + span->_pageCount - 1] = span;
     _pageidMapSpan.set(span->_pageID, span);
     _pageidMapSpan.set(span->_pageID + span->_pageCount - 1, span);
 }