#include "ConcurrentAlloc.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

const size_t ALLOC_SIZE = 643;      // 分配的内存大小
const size_t ALLOC_COUNT = 10000000; // 每个线程分配的次数
const size_t THREAD_COUNT = 4;     // 线程数量

void BenchmarkMallocFree() {
    for (size_t i = 0; i < ALLOC_COUNT; ++i) {
        void *ptr = malloc(ALLOC_SIZE);
        free(ptr);
    }
}

void BenchmarkConcurrentAllocFree() {
    for (size_t i = 0; i < ALLOC_COUNT; ++i) {
        void *ptr = ConcurrentAlloc(ALLOC_SIZE);
        ConcurrentFree(ptr, ALLOC_SIZE);
    }
}

void RunBenchmark(void (*benchmarkFunc)(), const std::string &testName) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(benchmarkFunc);
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << testName << " took " << duration.count() << " seconds." << std::endl;
}

void SingleThreadTest() {
    for (size_t i = 0; i < ALLOC_COUNT; ++i) {
        void *ptr = ConcurrentAlloc(ALLOC_SIZE);
        if (ptr == nullptr) {
            std::cerr << "ConcurrentAlloc failed!" << std::endl;
            return;
        }
        ConcurrentFree(ptr, ALLOC_SIZE);
    }
}

int main() {
    std::cout << "Performance Comparison: malloc/free vs ConcurrentAlloc/ConcurrentFree" << std::endl;

    RunBenchmark(BenchmarkMallocFree, "malloc/free");
    RunBenchmark(BenchmarkConcurrentAllocFree, "ConcurrentAlloc/ConcurrentFree");
    // SingleThreadTest();
    return 0;
}