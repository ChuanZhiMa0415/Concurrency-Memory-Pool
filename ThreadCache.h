#pragma once
#include "Common.h"

class ThreadCache {
public:
    /// @brief ����ռ�
    /// @param memorySize Ҫ����Ŀռ�
    /// @return
    void* allocate(size_t memorySize);
    /// @brief �ͷſռ�
    /// @param memory Ҫ�ͷŵĿռ����ʼ��ַ
    /// @param size ��������ռ�Ĵ�С
    void deallocate(void* memory, size_t size);
    /// @brief �� CentralCache ������ռ�
    /// @param index
    /// @param size ����Ҫ������ֽ���
    /// @return
    void* fetchFromCentralCache(size_t index, size_t size);
    /// @brief �ͷŶ���ʱ�� freelist ����ʱ�������ڴ�ص����Ļ���
    /// @param list
    /// @param size
    void listTooLong(FreeList& freeList, size_t size);

private:
    std::vector<FreeList> _freeLists = std::vector<FreeList>(FREELIST_SIZE);
};
extern thread_local ThreadCache* pThreadCache;