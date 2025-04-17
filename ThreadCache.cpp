#include "ThreadCache.h"
#include "CentralCache.h"
#include "Common.h"

thread_local ThreadCache* pThreadCache = nullptr;

void* ThreadCache::allocate(size_t memorySize) {
    // 1. ����������Ҫ����Ŀռ�� freeList ���±꣨ΪʲôҪ�ڴ���룿��
    size_t realSize = SizeClass::roundUp(memorySize);
    size_t index = SizeClass::getIndex(memorySize);
    // 2. ���� freeList ��û�У������� freeList ��
    if (!_freeLists[index].empty())
        return _freeLists[index].pop();
    else
        return fetchFromCentralCache(index, realSize);
}
void* ThreadCache::fetchFromCentralCache(size_t index, size_t size) {
    // 1. ��ȡ��Ҫ���ڴ����
    /*
     * ����ʼ�㷨��
     * 1. �� thread cache ��С�ֽڵ�С���ڴ���������࣬�ô��ֽڵ�С���ڴ����������
     * 2. ΪʲôҪȡ min(_freeLists[index].getCapacity(), SizeClass::numMoveSize(size)) ?
     *    -- ��Ϊ���û�� min��һ���߳�һ������ͻ��С���ڴ���������꣬����ʵ����߳����ò�����ô���С���ڴ��
     * 3. ����㲻��Ҫ size ��С���ڴ�飬����Ĵ���Խ�࣬_freeLists[index].getCapacity() �ͻ�Խ��
     *    һ�δ� central cache ��õ��ڴ������Խ��
     */
    size_t totalCount = min(_freeLists[index].getCapacity(), SizeClass::numMoveSize(size));
    if (totalCount == _freeLists[index].getCapacity()) _freeLists[index].getCapacity() += 1;
    // 2. �� Central Cache ��һЩ�ڴ��ͷţ���ͳ���ͷŵĿ���
    void* start = nullptr, * end = nullptr;
    size_t actualCount = CentralCache::getInstance().fetchRangeObj(start, end, totalCount, size);
    if (actualCount == 1) {
        assert(start == end);
        return start;
    }
    else { // 3. �ѵ�һ������̣߳�ʣ�µĿ鶼���뵽 Thread Cache ���Ͱ��
        _freeLists[index].pushRange(GetNext(start), end, actualCount - 1);
        return start;
    }
}
void ThreadCache::deallocate(void* memory, size_t size) {
    assert(memory != nullptr && size <= MAX_BYTES);
    int index = SizeClass::getIndex(size);
    _freeLists[index].push(memory);
    // ��� freelist �ĳ��ȳ������������� central cache �㻹ÿ��Ϊ size ��С��С���ڴ�
    if (_freeLists[index].getSize() > _freeLists[index].getCapacity()) listTooLong(_freeLists[index], size);
}
void ThreadCache::listTooLong(FreeList& freeList, size_t size) {
    void* start = nullptr, * end = nullptr;
    freeList.popRange(start, end, freeList.getCapacity());
    CentralCache::getInstance().releaseListToSpans(start, size);
}