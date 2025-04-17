#pragma once
#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <new>
#include <stdio.h>    // printf, perror
#include <stdlib.h>   // EXIT_SUCCESS, EXIT_FAILURE
//#include <sys/mman.h> // mmap, munmap, PROT_*, MAP_*
//#include <unistd.h>   // sysconf(_SC_PAGESIZE) 获取页大小


template <typename newedType>
class ObjectPool {
public:
    newedType* _new() {
        std::unique_lock<std::mutex> lock(_mtx); // 加锁
        newedType* obj = nullptr;
        if (_freeList != nullptr) {           // 头删 freeList
            void* next = *(void**)_freeList; // GetNext(_freeList);
            obj = (newedType*)_freeList, _freeList = next;
        }
        else {                                    // 小块内存已被拿完了
            if (_remainBytes < sizeof(newedType)) { // 向堆申请大块内存
                _remainBytes = 128 * 1024;
                _memory = (char*)SystemAlloc(_remainBytes >> 13);
            }
            // 切下所需要的内存
            obj = reinterpret_cast<newedType*>(_memory);
            size_t objSize = sizeof(newedType) < sizeof(void*) ? sizeof(void*) : sizeof(newedType);
            _memory += objSize, _remainBytes -= objSize;
        }
        // 初始化切下的这块内存
        new (obj) newedType; // 定位 new
        // spinLock.unlock();
        return obj;
    }
    void _delete(newedType* obj) { // 头插到 freeList
        std::unique_lock<std::mutex> lock(_mtx);
        obj->~newedType();
        /* GetNext(obj) */ *(void**)obj = _freeList;
        _freeList = obj, _remainBytes += sizeof(newedType);
    }

private:
    char* _memory = nullptr;
    void* _freeList = nullptr;
    size_t _remainBytes = 0;
    std::mutex _mtx;
};
