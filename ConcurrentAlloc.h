#pragma once

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache {
public:
    static PageCache& getSingleInstance() { return _sInst; }
    /// @brief 获取一个 k 页的 Span
    /// @param k 
    /// @return Span 的地址
    Span* getNewSpan(size_t k);
    /// @brief 输入小块内存的地址，输出对应的 Span 的地址
    /// @param obj 小块内存的地址
    /// @return 对应的 Span 的地址
    Span* mapObjectToSpan(void* obj);
    /// @brief 把 span 放回 page cache 层
    /// @param span
    void releaseSpanToPageCache(Span* span);

private:
    PageCache() {}
    PageCache(const PageCache& obj) = delete;
    static PageCache _sInst;

public:
    std::mutex _mutex;
    ObjectPool<Span> _spanPool;

private:
    // std::vector<SpanList> _spanLists = std::vector<SpanList>(MAX_PAGE_SIZE);
    SpanList _spanLists[MAX_PAGE_SIZE];
    // std::unordered_map<PageID, Span *> _pageidMapSpan;
    TCMalloc_PageMap2<32 - PAGE_SHIFT> _pageidMapSpan;
};

// inline void ConcurrentFree(void *obj, int size) {
//     // int size = (PageCache::getSingleInstance().mapObjectToSpan(obj))->_objSize;
//     if (size > MAX_BYTES) {
//         Span *span = PageCache::getSingleInstance().mapObjectToSpan(obj);
//         PageCache::getSingleInstance()._mutex.lock();
//         PageCache::getSingleInstance().releaseSpanToPageCache(span);
//         PageCache::getSingleInstance()._mutex.unlock();
//     } else {
//         assert(obj != nullptr);
//         pThreadCache->deallocate(obj, size);
//     }
// }
