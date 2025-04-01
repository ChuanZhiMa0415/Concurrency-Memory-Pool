#pragma once
#include "Common.hpp"
#include "PageCache.hpp"
#include "ThreadCache.hpp"
#include <mutex>

inline void *ConcurrentAlloc(int size) {
    if (size > MAX_BYTES) {
        int actualSize = SizeClass::roundUp(size), pageCount = SizeClass::numMovePage(actualSize);
        PageCache::getSingleInstance()._mutex.lock();
        Span *span = PageCache::getSingleInstance().getNewSpan(pageCount);
        PageCache::getSingleInstance()._mutex.unlock();
        span->_objSize = actualSize;
        return (void *)(span->_pageID << PAGE_SHIFT);
    } else {
        if (pThreadCache == nullptr) {
            static ObjectPool<ThreadCache> tcPool;
            pThreadCache = tcPool._new();
        }
        return pThreadCache->allocate(size);
    }
}

inline void ConcurrentFree(void *obj, int size) {
    // int size = (PageCache::getSingleInstance().mapObjectToSpan(obj))->_objSize;
    if (size > MAX_BYTES) {
        Span *span = PageCache::getSingleInstance().mapObjectToSpan(obj);
        PageCache::getSingleInstance()._mutex.lock();
        PageCache::getSingleInstance().releaseSpanToPageCache(span);
        PageCache::getSingleInstance()._mutex.unlock();
    } else {
        assert(obj != nullptr);
        pThreadCache->deallocate(obj, size);
    }
}