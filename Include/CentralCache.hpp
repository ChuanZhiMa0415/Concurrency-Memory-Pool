#pragma once
#include "Common.hpp"

/// @brief 一个进程只有一个 CentralCache 层，所以要用单例模式的设计方法
class CentralCache {
public:
    /// @brief TODO: 该函数是线程安全的吗
    /// @return
    static inline CentralCache &getInstance() { return _sInstance; }
    /// @brief 从 Central Cache 里的 Span 拿回一个或多个内存块
    /// @param start 一条内存块的头节点地址
    /// @param end 一条内存块的尾节点地址
    /// @param totalCount 理论上应该要申请的内存块数
    /// @param size 要申请总的字节数
    /// @return 实际可拿到的内存块数
    size_t fetchRangeObj(void *&start, void *&end, int totalCount, int size);
    /// @brief 在一条 span 链表中获取一个非空的 span
    /// @param spanList 从某一个 SpanList 取一个非空的 Span
    /// @param size 申请的总字节数(小块内存的大小)
    /// @return
    Span *getOneSpan(SpanList &spanList, int size);
    /// @brief 把从 thread cache 释放回来的多块小块内存挂到 central cache 的 Span 下
    /// @param start 多块小块内存的起始地址
    /// @param size 每块小块内存的空间大小（只能是 8B, 16B, 24B, ...）
    void releaseListToSpans(void *start, int size);

private:
    CentralCache() {}
    CentralCache(const CentralCache &obj) = delete;

private:
    std::vector<SpanList> _spanLists = std::vector<SpanList>(FREELIST_SIZE);
    static CentralCache _sInstance;
};