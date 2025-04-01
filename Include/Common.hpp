#pragma once
#include "ObjectPool.hpp"
#include <assert.h>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <vector>

using std::cin;
using std::cout;
using std::endl;

const int MAX_BYTES = 256 * 1024; // 256 Kb
const int FREELIST_SIZE = 208;
const int MAX_PAGE_SIZE = 129; // PAGE CACHE 的最大页为 128
const int PAGE_SHIFT = 13; // 8 Kb

/// @brief 获取下一个节点的地址
/// @param obj 内存块地址
/// @return 下一个节点的地址
static inline void *&GetNext(void *obj) {
    return *(void **)obj;
}

/// @brief 负责计算 ThreadCache 的哈希桶的下标和负责求出对齐后的申请空间的大小
class SizeClass {
private:
    /// @brief 返回真正需要申请的内存
    /// @return
    static inline size_t _roundUp(int bytes, int alignNumber) { return alignNumber * ceil(bytes / (float)alignNumber); }
    /// @brief 要申请 byte 个字节的空间，同时每个 freeList 的节点的大小是 alignNumber 个字节的大小，返回 freeList 的下标
    /// @param bytes
    /// @param alignNumber
    /// @return
    static inline size_t _getIndex(int bytes, int alignNumber) {
        return bytes % alignNumber == 0 ? bytes / alignNumber - 1 : bytes / alignNumber;
    }

public:
    /// @brief 返回对齐后需要申请的内存
    /// @param memory
    /// @return
    static inline int roundUp(size_t memory) {
        if (memory <= (1 << 7))
            return _roundUp(memory, 1 << 3);
        else if (memory <= (1 << 10))
            return _roundUp(memory, 1 << 4);
        else if (memory <= (1 << 13))
            return _roundUp(memory, 1 << 7);
        else if (memory <= (1 << 16))
            return _roundUp(memory, 1 << 10);
        else if (memory <= (1 << 19))
            return _roundUp(memory, 1 << 13);
        else
            return _roundUp(memory, 1 << PAGE_SHIFT);
    }
    /// @brief 获取 freeLists 的下标
    /// @param size
    /// @return
    static inline int getIndex(int memorySize) { // 为什么要这样对齐：这样对齐的内存碎片率 ≤ 12.5%
        std::vector<int> groupArray = {16, 56, 56, 56};
        if (memorySize <= (1 << 7)) {
            return _getIndex(memorySize, 1 << 3);
        } else if (memorySize <= (1 << 10)) {
            return groupArray[0] + _getIndex(memorySize - (1 << 7), 1 << 4);
        } else if (memorySize <= (1 << 13)) {
            return groupArray[0] + groupArray[1] + _getIndex(memorySize - (1 << 10), 1 << 7);
        } else if (memorySize <= (1 << 16)) {
            return groupArray[0] + groupArray[1] + groupArray[2] + _getIndex(memorySize - (1 << 13), 1 << 10);
        } else if (memorySize <= (1 << 19)) {
            return groupArray[0] + groupArray[1] + groupArray[2] + groupArray[3] + _getIndex(memorySize - (1 << 16), 1 << 13);
        } else {
            assert(false);
            return -1;
        }
    }
    /// @brief 求 thread cache 一次向 central cache 最多获取多少个小块内存
    /// @param size 每个小块内存的大小（单位：字节）
    /// @return
    static inline size_t numMoveSize(int size) { return std::max(2, std::min(512, MAX_BYTES / size)); }
    /// @brief 求 central cache 向 page cache 获取多少页
    /// @param size 要申请的字节数 (8B, 16B, 24B, ..., 256KB)
    /// @return
    static inline size_t numMovePage(int size) {
        int n = numMoveSize(size); // 求出要拿多少块大小为 size 的小块内存(规定范围为：[2, 512])
        return (n * size) >> PAGE_SHIFT == 0 ? 1 : (n * size) >> PAGE_SHIFT;
    }
};

class FreeList {
public:
    /// @brief 把 [start, end] 的 n 个链表头插到 freeList 里
    /// @param start
    /// @param end
    void pushRange(void *start, void *end, int n) {
        void *next = _head, *endNext = *(void **)end;
        _head = start, endNext = next;
        _size += n;
    }
    /// @brief 把 [start, end] 的 n 个链表头删
    /// @param start
    /// @param end
    /// @return
    void popRange(void *&start, void *&end, int n) {
        assert(n <= _size);
        start = end = _head;
        while (--n) end = GetNext(end);
        _head = GetNext(end), GetNext(end) = nullptr, _size -= n;
    }
    /// @brief 给 freeList 头插单个节点
    /// @param memory
    void push(void *memory) { // 使用头插的方式
        assert(memory != nullptr);
        void *next = _head;
        _head = memory, *(void **)memory = next;
        _size++;
    }
    void *pop() { // 使用头删的方式
        assert(_head != nullptr);
        void *del = _head, *next = *(int **)del;
        _head = next;
        _size--;
        return del;
    }
    bool empty() { return _head == nullptr; }
    size_t &getCapacity() { return _capacity; }
    size_t &getSize() { return _size; }

private:
    void *_head = nullptr;
    size_t _capacity = 1;
    size_t _size = 0;
};

#ifdef __arm64__
using PageID = unsigned long long;
#else
using PageID = size_t;
#endif
using PageID = unsigned long long;
struct Span {
    PageID _pageID = 0;   // span 的大块内存内部存的第一个页的页号
    int _pageCount = 0;   // 存储的页的数量
    int _useCount = 0;    // 统计有多少个小块被用了
    int _objSize = 0;     // central cache 中的 span 的小块内存的大小
    bool _isUsed = false; // 当前 span 有没有被正在使用（区分 page cache 中的 span 和 central cache 中满的 span）
    Span *_prev = nullptr;
    Span *_next = nullptr;
    void *_freeList = nullptr;
};

static ObjectPool<Span> spanPool;

class SpanList { // 带头双向循环链表
public:
    SpanList() {
        _head = spanPool._new();
        _head->_next = _head->_prev = _head;
    }
    inline void insert(Span *pos, Span *newSpan) { // 最终效果：prev <-> newSpan <-> pos
        assert(newSpan != nullptr && pos != nullptr);
        Span *prev = pos;
        prev->_next = newSpan, newSpan->_prev = prev, newSpan->_next = pos, pos->_prev = newSpan;
    }
    inline void erase(Span *node) {
        assert(node != nullptr);
        assert(node != _head);
        Span *prev = node->_prev, *next = node->_next;
        prev->_next = next, next->_prev = prev;
    }
    inline void pushFront(Span *node) { insert(begin(), node); }
    inline Span *popFront() {
        Span *del = begin();
        erase(del);
        return del;
    }
    inline bool empty() { return _head->_next == _head; }
    Span *begin() { return _head->_next; }
    Span *end() { return _head; }

private:
    Span *_head;

public:
    std::mutex _mtx;
};