#ifdef _WIN32

#include <corecrt_malloc.h>
inline void* mem_alloc(size_t size, size_t alignment = 8) {
    return _aligned_malloc(size, alignment);
}

inline void mem_free(void* ptr) {
    _aligned_free(ptr);
}

#else

#include <stdlib.h>
inline void* mem_alloc(size_t size, size_t alignment = 8) {
    return aligned_alloc(alignment, size);
}

inline void mem_free(void* ptr) {
    free(ptr);
}

#endif

template <class T>
inline T* alloc_array(size_t len, size_t alignment = alignof(T)) {
    return static_cast<T*>(mem_alloc(sizeof(T) * len, alignment));
}

