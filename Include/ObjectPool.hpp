#pragma once
#include <cstddef>
#include "Lock.hpp"
#include <cstdlib>
#include <mutex>
#include <stdio.h>    // printf, perror
#include <stdlib.h>   // EXIT_SUCCESS, EXIT_FAILURE
#include <sys/mman.h> // mmap, munmap, PROT_*, MAP_*
#include <unistd.h>   // sysconf(_SC_PAGESIZE) 获取页大小

// 直接去堆上按页申请空间
inline static void *SystemAlloc(size_t kpage) {
#ifdef _WIN32
    void *ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void *ptr = mmap(NULL, kpage << 13, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    if (ptr == nullptr)
        throw std::bad_alloc();

    return ptr;
}

template <typename newedType>
class ObjectPool {
public:
    newedType *_new() {
        // std::lock_guard<std::mutex> lock(_mutex); // 加锁
        spinLock.lock();
        newedType *obj = nullptr;
        if (_freeList != nullptr) {           // 头删 freeList
            void *next = *(void **)_freeList; // GetNext(_freeList);
            obj = (newedType *)_freeList, _freeList = next;
        } else {                                    // 小块内存已被拿完了
            if (_remainBytes < sizeof(newedType)) { // 向堆申请大块内存
                _remainBytes = 128 * 1024;
                _memory = (char *)malloc(_remainBytes);
            }
            // 切下所需要的内存
            obj = (newedType *)_memory;
            size_t objSize = sizeof(newedType) < sizeof(void *) ? sizeof(void *) : sizeof(newedType);
            _memory += objSize, _remainBytes -= objSize;
        }
        // 初始化切下的这块内存
        new (obj) newedType(); // 定位 new
        spinLock.unlock();
        return obj;
    }
    void _delete(newedType *obj) { // 头插到 freeList
                                   // std::lock_guard<std::mutex>lock(_mutex);
        spinLock.lock();
        obj->~newedType();
        /* GetNext(obj) */ *(void **)obj = _freeList;
        _freeList = obj, _remainBytes += sizeof(newedType);
        spinLock.unlock();
    }

private:
    char *_memory = nullptr;
    void *_freeList = nullptr;
    size_t _remainBytes = 0;
    // std::mutex _mutex;
    Spinlock spinLock;
};