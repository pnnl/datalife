#ifndef ATOMICHELPER_H
#define ATOMICHELPER_H

#include <atomic>


template <typename T>
static inline void init_atomic_array(std::atomic<T> *array, uint32_t numElements, T initVal) {
    for (uint32_t i = 0; i < numElements; i++) {
        std::atomic_init(&array[i], initVal);
    }
}

#endif