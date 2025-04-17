#pragma once
#include "Common.h"

class ThreadCache {
public:
    /// @brief 申请空间
    /// @param memorySize 要申请的空间
    /// @return
    void* allocate(size_t memorySize);
    /// @brief 释放空间
    /// @param memory 要释放的空间的起始地址
    /// @param size 这块连续空间的大小
    void deallocate(void* memory, size_t size);
    /// @brief 向 CentralCache 层申请空间
    /// @param index
    /// @param size 真正要申请的字节数
    /// @return
    void* fetchFromCentralCache(size_t index, size_t size);
    /// @brief 释放对象时， freelist 过长时，回收内存回到中心缓存
    /// @param list
    /// @param size
    void listTooLong(FreeList& freeList, size_t size);

private:
    std::vector<FreeList> _freeLists = std::vector<FreeList>(FREELIST_SIZE);
};
extern thread_local ThreadCache* pThreadCache;