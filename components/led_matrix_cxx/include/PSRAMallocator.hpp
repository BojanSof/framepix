#ifndef PSRAM_ALLOCATOR_HPP
#define PSRAM_ALLOCATOR_HPP

#include "sdkconfig.h"
#include <esp_heap_caps.h>

#include <exception>

template<class T> class PSRAMAllocator
{
public:
    using value_type = T;

    PSRAMAllocator() noexcept { }

    template<class U>
    constexpr PSRAMAllocator(const PSRAMAllocator<U>&) noexcept
    {
    }

    [[nodiscard]] value_type* allocate(std::size_t n)
    {
#if CONFIG_SPIRAM
        // attempt to allocate in PSRAM first
        auto p = static_cast<value_type*>(
            heap_caps_malloc(n * sizeof(value_type), MALLOC_CAP_SPIRAM));
        if (p)
        {
            return p;
        }
#endif  // CONFIG_SPIRAM

        // If the allocation in PSRAM failed (or PSRAM not enabled), try to
        // allocate from the default memory pool.
        auto p2 = static_cast<value_type*>(
            heap_caps_malloc(n * sizeof(value_type), MALLOC_CAP_DEFAULT));
        if (p2)
        {
            return p2;
        }

        return NULL;
    }

    void deallocate(value_type* p, std::size_t) noexcept { heap_caps_free(p); }
};

template<class T, class U>
bool operator==(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&)
{
    return true;
}

template<class T, class U>
bool operator!=(const PSRAMAllocator<T>& x, const PSRAMAllocator<U>& y)
{
    return !(x == y);
}
#endif  // PSRAM_ALLOCATOR_HPP
