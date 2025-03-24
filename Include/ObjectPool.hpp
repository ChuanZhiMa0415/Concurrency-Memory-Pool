#pragma once
#include <cstddef>
#include <cstdlib>

template <typename newedType>
class ObjectPool {
public:
    newedType *_new() {
        newedType *obj = nullptr;
        if (_freeList != nullptr) { // 头删 freeList
            void *next = *(void **)_freeList;
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
        return obj;
    }
    void _delete(newedType *obj) { // 头插到 freeList
        obj->~newedType();
        *(void **)obj = _freeList;
        _freeList = obj, _remainBytes += sizeof(newedType);
    }

private:
    char *_memory = nullptr;
    void *_freeList = nullptr;
    size_t _remainBytes = 0;
};