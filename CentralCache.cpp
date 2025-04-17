#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInstance;

size_t CentralCache::fetchRangeObj(void*& start, void*& end, int totalCount, size_t size) {
    size_t index = SizeClass::getIndex(size);

    _spanLists[index]._mtx.lock();

    Span* pSpan = getOneSpan(_spanLists[index], size);
    assert(pSpan != nullptr && pSpan->_freeList != nullptr);
    // 从 Span 里拿 totalCount 个内存块
    end = start = pSpan->_freeList;
    int actualCount = 1;
    while (actualCount < totalCount && GetNext(end) != nullptr) end = GetNext(end), actualCount++;
    pSpan->_freeList = GetNext(end), GetNext(end) = nullptr;
    pSpan->_useCount += actualCount;

    _spanLists[index]._mtx.unlock();

    return actualCount;
}


Span* CentralCache::getOneSpan(SpanList& spanList, size_t size) {
    for (Span* it = spanList.begin(); it != spanList.end(); it = it->_next)
        if (it->_freeList != nullptr) return it;
    // 该 spanList 的所有 Span 都没有挂 size 大小的小块内存 -> 找 page cache 申请
 
    spanList._mtx.unlock(); // 这里解锁的好处：其他线程可以要把 free 掉的 span 放到这个 spanlist 里

    PageCache::getSingleInstance()._mutex.lock();
    size_t pageNum = SizeClass::numMovePage(size);
    Span* pSpan = PageCache::getSingleInstance().getNewSpan(pageNum);
    pSpan->_objSize = size;
    pSpan->_isUsed = true;
    PageCache::getSingleInstance()._mutex.unlock();

    // 切分从 page cache 拿来的 span
    // 1. 确定这个 span 的开始地址和结束地址
    char* start = (char*)(pSpan->_pageID << PAGE_SHIFT);
    size_t bytes = pSpan->_pageCount << PAGE_SHIFT;
    char* end = start + bytes;

    // 2. 切分 span 所管理的空间，切成一个个小块内存(freeList 头插)
    pSpan->_freeList = start;
    start += size;
    void* tail = pSpan->_freeList;
    while (start < end) {
        GetNext(tail) = start;
        tail = GetNext(tail);
        start += size;
    }

    GetNext(tail) = nullptr;

    // 3. 切好 span 后，把 span 挂到 spanlist 里
    spanList._mtx.lock();
    spanList.pushFront(pSpan);
    return pSpan;
}

void CentralCache::releaseListToSpans(void* start, size_t size) {
    int index = SizeClass::getIndex(size);
    _spanLists[index]._mtx.lock();

    while (start != nullptr) {
        void* next = GetNext(start);
        Span* span = PageCache::getSingleInstance().mapObjectToSpan(start);
        // 把 start 头插到对应的 span
        GetNext(start) = span->_freeList, span->_freeList = start;
        span->_useCount--;

        if (span->_useCount == 0) {                                // 此时该 span 已拿回全部小块内存，把 span 放回 page cache
            _spanLists[index].erase(span);                         // 把 span 从 central cache 释放出来，但还没放回 page cache 层
            span->_freeList = span->_prev = span->_next = nullptr; // span 只需保留 pageID & pageCount 不为 0 就能够放回 page cache

            _spanLists[index]._mtx.unlock();

            PageCache::getSingleInstance()._mutex.lock();
            PageCache::getSingleInstance().releaseSpanToPageCache(span); // 把 span 放回 page cache 层
            PageCache::getSingleInstance()._mutex.unlock();

            _spanLists[index]._mtx.lock(); // 因为 start 在 span list 上，所以访问 start 时要加锁
        }
        start = next;
    }

    _spanLists[index]._mtx.unlock();
}