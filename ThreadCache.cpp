#include "ThreadCache.h"
#include "CentralCache.h"
#include "Common.h"

thread_local ThreadCache* pThreadCache = nullptr;

void* ThreadCache::allocate(size_t memorySize) {
    // 1. 计算对齐后需要申请的空间和 freeList 的下标（为什么要内存对齐？）
    size_t realSize = SizeClass::roundUp(memorySize);
    size_t index = SizeClass::getIndex(memorySize);
    // 2. 看看 freeList 有没有，优先用 freeList 的
    if (!_freeLists[index].empty())
        return _freeLists[index].pop();
    else
        return fetchFromCentralCache(index, realSize);
}
void* ThreadCache::fetchFromCentralCache(size_t index, size_t size) {
    // 1. 获取需要的内存块数
    /*
     * 慢开始算法：
     * 1. 让 thread cache 中小字节的小块内存的最多块数多，让大字节的小块内存的最多块数少
     * 2. 为什么要取 min(_freeLists[index].getCapacity(), SizeClass::numMoveSize(size)) ?
     *    -- 因为如果没有 min，一个线程一旦申请就会把小块内存的上限申完，但其实这个线程是用不完这么多的小块内存的
     * 3. 如果你不断要 size 大小的内存块，申请的次数越多，_freeLists[index].getCapacity() 就会越大，
     *    一次从 central cache 获得的内存块数就越多
     */
    size_t totalCount = min(_freeLists[index].getCapacity(), SizeClass::numMoveSize(size));
    if (totalCount == _freeLists[index].getCapacity()) _freeLists[index].getCapacity() += 1;
    // 2. 把 Central Cache 的一些内存释放，并统计释放的块数
    void* start = nullptr, * end = nullptr;
    size_t actualCount = CentralCache::getInstance().fetchRangeObj(start, end, totalCount, size);
    if (actualCount == 1) {
        assert(start == end);
        return start;
    }
    else { // 3. 把第一个块给线程，剩下的块都插入到 Thread Cache 层的桶里
        _freeLists[index].pushRange(GetNext(start), end, actualCount - 1);
        return start;
    }
}
void ThreadCache::deallocate(void* memory, size_t size) {
    assert(memory != nullptr && size <= MAX_BYTES);
    int index = SizeClass::getIndex(size);
    _freeLists[index].push(memory);
    // 如果 freelist 的长度超过了容量，向 central cache 层还每块为 size 大小的小块内存
    if (_freeLists[index].getSize() > _freeLists[index].getCapacity()) listTooLong(_freeLists[index], size);
}
void ThreadCache::listTooLong(FreeList& freeList, size_t size) {
    void* start = nullptr, * end = nullptr;
    freeList.popRange(start, end, freeList.getCapacity());
    CentralCache::getInstance().releaseListToSpans(start, size);
}