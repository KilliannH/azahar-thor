// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

namespace Common {

/**
 * Allocate memory aligned to a specific boundary (default 64 bytes for ARM cache lines)
 * @param size Size in bytes to allocate
 * @param alignment Alignment boundary (must be power of 2)
 * @return Pointer to aligned memory
 */
    inline void* AlignedAlloc(size_t size, size_t alignment = 64) {
        // Align size up to the next multiple of alignment
        size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

        void* ptr = nullptr;

#if defined(_WIN32)
        ptr = _aligned_malloc(aligned_size, alignment);
#elif defined(__ANDROID__) || defined(__linux__)
        ptr = aligned_alloc(alignment, aligned_size);
#else
        // Fallback for other platforms
        if (posix_memalign(&ptr, alignment, aligned_size) != 0) {
            ptr = nullptr;
        }
#endif

        if (!ptr) {
            throw std::bad_alloc();
        }

        return ptr;
    }

/**
 * Free memory allocated with AlignedAlloc
 * @param ptr Pointer to free
 */
    inline void AlignedFree(void* ptr) {
        if (!ptr) {
            return;
        }

#if defined(_WIN32)
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }

/**
 * Custom allocator for STL containers using aligned allocation
 */
    template<typename T, size_t Alignment = 64>
    class AlignedAllocator {
    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        template<typename U>
        struct rebind {
            using other = AlignedAllocator<U, Alignment>;
        };

        AlignedAllocator() noexcept = default;

        template<typename U>
        AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

        T* allocate(size_t n) {
            return static_cast<T*>(AlignedAlloc(n * sizeof(T), Alignment));
        }

        void deallocate(T* p, size_t) noexcept {
            AlignedFree(p);
        }

        template<typename U>
        bool operator==(const AlignedAllocator<U, Alignment>&) const noexcept {
            return true;
        }

        template<typename U>
        bool operator!=(const AlignedAllocator<U, Alignment>&) const noexcept {
            return false;
        }
    };

/**
 * Helper macro to align a struct/class to cache line boundaries
 */
#define CACHE_LINE_ALIGNED alignas(64)

/**
 * Prefetch data into CPU cache
 * Hints the CPU to load data before it's actually needed
 * @param addr Address to prefetch
 * @param rw 0 for read, 1 for write
 * @param locality 0-3, higher = keep in cache longer
 */
    inline void Prefetch(const void* addr, int rw = 0, int locality = 3) {
#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(addr, rw, locality);
#elif defined(_MSC_VER)
        #include <xmmintrin.h>
    _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
#else
        // No-op on unsupported compilers
        (void)addr;
        (void)rw;
        (void)locality;
#endif
    }

/**
 * Prefetch a cache line worth of data (64 bytes for ARM)
 */
    inline void PrefetchCacheLine(const void* addr) {
        Prefetch(addr, 0, 3);
    }

/**
 * Prefetch multiple cache lines
 * Useful for prefetching large memory regions before processing
 */
    inline void PrefetchRange(const void* start, size_t size) {
        constexpr size_t cache_line_size = 64;
        const char* ptr = static_cast<const char*>(start);
        const char* end = ptr + size;

        for (; ptr < end; ptr += cache_line_size) {
            Prefetch(ptr);
        }
    }
}