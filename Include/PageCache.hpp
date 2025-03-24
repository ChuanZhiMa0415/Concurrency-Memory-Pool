#pragma once
#include "Common.hpp"

class PageCache {
public:
    static PageCache &getSingleInstance() { return _sInst; }
    /// @brief 获取一个 k 页的 Span
    /// @param k 
    /// @return Span 的地址
    Span *getNewSpan(size_t k);
    /// @brief 输入小块内存的地址，输出对应的 Span 的地址
    /// @param obj 小块内存的地址
    /// @return 对应的 Span 的地址
    Span *mapObjectToSpan(void *obj);
    /// @brief 把 span 放回 page cache 层
    /// @param span
    void releaseSpanToPageCache(Span *span);

private:
    PageCache() {}
    PageCache(const PageCache &obj) = delete;
    static PageCache _sInst;

public:
    std::mutex _mutex;

private:
    std::vector<SpanList> _spanLists = std::vector<SpanList>(MAX_PAGE_SIZE);
    std::unordered_map<PageID, Span *> _pageidMapSpan;
};