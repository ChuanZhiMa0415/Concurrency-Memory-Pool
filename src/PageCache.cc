#include "../Include/PageCache.hpp"
#include "Common.hpp"
#include "ObjectPool.hpp"
#include <mutex>

PageCache PageCache::_sInst;

Span *PageCache::getNewSpan(size_t k) {
    assert(k > 0);
    if (k >= MAX_PAGE_SIZE) { // 向堆申请
        void *ptr = SystemAlloc(k);
        Span *span = spanPool._new();
        span->_pageID = (PageID)ptr >> PAGE_SHIFT, span->_pageCount = k;
        _pageidMapSpan[span->_pageID] = span; // 不用考虑 span 合并，只要让小块内存的地址找到对应的 span 地址就行了
        return span;
    }
    if (!_spanLists[k].empty()) return _spanLists[k].popFront();
    // 看看后面的 spanlist 有没有 span，如果有，切割；如果没有，向堆申请 128 页的 span
    for (int i = k + 1; i < MAX_PAGE_SIZE; i++)
        if (!_spanLists[i].empty()) { // 切割 span, kSpan 作为返回结果
            Span *span = _spanLists[i].popFront(), *kSpan = spanPool._new();
            // 切割第一步：把 span 和 kSpan 的内部属性改一下
            kSpan->_pageID = span->_pageID, kSpan->_pageCount = k;
            span->_pageCount -= k, span->_pageID += k;
            // 切割第二步：把 span 插入 _spanLists[i-k] 里
            _spanLists[span->_pageCount].pushFront(span);
            // 切割第三步：把 kSpan 所管理的页的 pageid 及其所属的 kSpan 的地址写入映射表，
            // 方便 thread cache 向 central cache 还小块内存时，每一个小块内存通过小块内存自己的地址找到所属页，
            // 再通过所属页的页号找到该小块内存的所属的 Span，然后才能真正地还给 central cache
            for (int i = 0; i < kSpan->_pageCount; i++) _pageidMapSpan[kSpan->_pageID + i] = kSpan;
            // 切割第四步：为了 page cache 合并 Span 时，可以通过当前 Span 的 pageid 属性找到前一页和后一页的编号；
            // 再通过前一页和后一页的编号和映射表找到这两页的所属 span 是否全满，如果全满，合并这两页的所属 span (因为该 span 有可能是被合并的)
            _pageidMapSpan[span->_pageID] = _pageidMapSpan[span->_pageID + span->_pageCount - 1] = span; // 存 page cache 中的 span 所管理的页的第一页和最后一页，对 span 的映射
            kSpan->_isUsed = true;
            return kSpan;
        }
    // 此时整个 page cache 没有 span，所以向堆申请
    void *ptr = SystemAlloc(MAX_PAGE_SIZE - 1);
    Span *span = spanPool._new();
    span->_pageCount = MAX_PAGE_SIZE - 1, span->_pageID = (PageID)ptr >> PAGE_SHIFT;
    _spanLists[span->_pageCount].pushFront(span);

    return getNewSpan(k);
}

Span *PageCache::mapObjectToSpan(void *obj) {
    PageID pageId = (PageID)obj >> PAGE_SHIFT;
    std::lock_guard<std::mutex> lock(_mutex); // 可以在离开时自动解锁
    if (_pageidMapSpan.count(pageId)) return _pageidMapSpan[pageId];
    assert(false);
    return nullptr;
}

void PageCache::releaseSpanToPageCache(Span *span) {
    if (span->_pageCount >= MAX_PAGE_SIZE) { // 如果这个 span 管理的内存过多，直接还给堆
        free((void *)(span->_pageID << PAGE_SHIFT));
        spanPool._delete(span);
        return;
    }
    // _spanLists[span->_pageCount].erase(span);
    // 向前面的 span 合并
    while (true) {
        PageID prevPageid = span->_pageID - 1;
        if (!_pageidMapSpan.count(prevPageid) ||
            _pageidMapSpan[prevPageid]->_isUsed ||
            _pageidMapSpan[prevPageid]->_pageCount + span->_pageCount >= MAX_PAGE_SIZE) break;
        // 合并 span 和 prevSpan
        Span *prevSpan = _pageidMapSpan[prevPageid];
        span->_pageID = prevSpan->_pageID, span->_pageCount += prevSpan->_pageCount;
        _spanLists[prevSpan->_pageCount].erase(prevSpan), spanPool._delete(prevSpan);
    }
    // 向后面的 span 合并
    while (true) {
        PageID nextPageid = span->_pageID + span->_pageCount;
        if (!_pageidMapSpan.count(nextPageid) ||
            _pageidMapSpan[nextPageid]->_isUsed ||
            _pageidMapSpan[nextPageid]->_pageCount + span->_pageCount >= MAX_PAGE_SIZE) break;
        // 合并 span 和 nextSpan
        Span *nextSpan = _pageidMapSpan[nextPageid];
        span->_pageCount += nextSpan->_pageCount;
        _spanLists[nextSpan->_pageCount].erase(nextSpan), spanPool._delete(nextSpan);
    }
    // 调整 span 在 page cache 中的位置
    span->_isUsed = false;
    _spanLists[span->_pageCount].pushFront(span);
    _pageidMapSpan[span->_pageID] = _pageidMapSpan[span->_pageID + span->_pageCount - 1] = span;
}
