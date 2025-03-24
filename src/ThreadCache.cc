#include "../Include/ThreadCache.hpp"
#include "../Include/CentralCache.hpp"
#include <cstddef>

thread_local ThreadCache *pThreadCache = nullptr;

void *ThreadCache::allocate(int memorySize) {
    // 1. 计算对齐后需要申请的空间和 freeList 的下标
    int realSize = SizeClass::roundUp(memorySize), index = SizeClass::getIndex(realSize);
    // 2. 看看 freeList 有没有，优先用 freeList 的
    if (!_freeLists[index].empty())
        return _freeLists[index].pop();
    else
        return fetchFromCentralCache(index, realSize);
}
void *ThreadCache::fetchFromCentralCache(int index, int size) {
    // 1. 获取需要的内存块数
    size_t totalCount = std::min(_freeLists[index].getCapacity(), SizeClass::numMoveSize(size));
    if (totalCount == _freeLists[index].getCapacity()) _freeLists[index].getCapacity()++;
    // 2. 把 Central Cache 的一些内存释放，并统计释放的块数
    void *start = nullptr, *end = nullptr;
    size_t actualCount = CentralCache::getInstance().fetchRangeObj(start, end, totalCount, size);
    // 3. 把拿回的块都插入到 Thread Cache 层的桶里
    _freeLists[index].pushRange(start, end, actualCount);
    /*
        TODO: 为什么要用下面这段代码代替上面的第 29 行？
        if (actualNum == 1)
        {
            assert(start == end);
            return start;
        }
        else
        {
            _freeLists[index].PushRange(NextObj(start), end);
            return start;
        }
    */
    return start;
}
void ThreadCache::deallocate(void *memory, int size) {
    assert(memory != nullptr && size <= MAX_BYTES);
    int index = SizeClass::getIndex(size);
    _freeLists[index].push(memory);
    // 如果 freelist 的长度超过了容量，向 central cache 层还每块为 size 大小的小块内存
    if (_freeLists[index].getSize() > _freeLists[index].getCapacity()) listTooLong(_freeLists[index], size);
}
void ThreadCache::listTooLong(FreeList &freeList, size_t size) {
    void *start = nullptr, *end = nullptr;
    freeList.popRange(start, end, freeList.getCapacity());
    CentralCache::getInstance().releaseListToSpans(start, size);
}