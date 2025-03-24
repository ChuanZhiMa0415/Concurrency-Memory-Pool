#include "Common.hpp"
#include "ConcurrentAlloc.hpp"
#include <thread>

void Alloc1() {
    for (size_t i = 0; i < 5; ++i) {
        void *ptr = ConcurrentAlloc(6);
    }
}

void Alloc2() {
    for (size_t i = 0; i < 5; ++i) {
        void *ptr = ConcurrentAlloc(7);
    }
}

void Alloc3() {
    void *p1 = ConcurrentAlloc(MAX_PAGE_SIZE << PAGE_SHIFT);
    // ConcurrentFree(p1, MAX_PAGE_SIZE << PAGE_SHIFT);
    ConcurrentFree(p1);
    cout << "free succeed" << endl;
}

void TLSTest() {
    std::thread t1(Alloc1);
    t1.join();

    std::thread t2(Alloc2);
    t2.join();
}

int main() {
    // TestObjectPool();
    //  TLSTest();
    Alloc3();
    return 0;
}