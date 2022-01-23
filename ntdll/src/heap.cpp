#include "ntdll_private.h"

static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_SHIFT = 12;

class heap {
public:
    heap(size_t reserve_size, size_t commit_size);

    void* allocate(uint32_t flags, size_t size);
    bool free(uint32_t flags, void* base_address);

    size_t reserve_size;
    size_t commit_size;
    uint8_t data[0];
};

struct alloc_header {
    uint32_t size : 31;
    uint32_t free : 1;
};

static_assert(sizeof(alloc_header) == 4);

heap::heap(size_t reserve_size, size_t commit_size) : reserve_size(reserve_size), commit_size(commit_size) {
    auto& ah = *(alloc_header*)data;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    ah.size = (uint32_t)(commit_size - offsetof(heap, data) - sizeof(alloc_header));
#pragma GCC diagnostic pop
    ah.free = 1;
}

void* heap::allocate(uint32_t flags, size_t size) {
    print("FIXME - heap::allocate(");
    print_hex(flags);
    print(", ");
    print_hex(size);
    print(")\n");

    // round to multiple of 4
    if (size & 3)
        size = ((size >> 2) + 1) << 2;

    if (size == 0 || size >= 0x80000000)
        return nullptr;

    alloc_header* ah = nullptr;

    // FIXME - lock (critical section?)

    // FIXME - if subheap full, allocate new one
    // FIXME - separate subheap for allocations larger than 1 page?

    // allocate from existing pages if possible

    auto h = (alloc_header*)data;
    alloc_header* last = nullptr;

    do {
        if (h->free) {
            if (last && last->free) { // merge adjacent free entries
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                last->size += h->size + sizeof(alloc_header);
#pragma GCC diagnostic pop
                h = last;
            }

            if (h->size == size) {
                h->free = 0;
                return (uint8_t*)h + sizeof(alloc_header);
            }

            if (h->size > size) {
                auto nh = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + size);

                if (nh < (void*)((uint8_t*)this + commit_size)) {
                    nh->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    nh->size = h->size - size - sizeof(alloc_header);
#pragma GCC diagnostic pop
                }

                h->free = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                h->size = size;
#pragma GCC diagnostic pop
                return (uint8_t*)h + sizeof(alloc_header);
            }
        }

        last = h;
        h = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + h->size);
    } while (h < (void*)((uint8_t*)this + commit_size));

    if (last && last->free)
        ah = last;

    // FIXME - extend committed memory

    return nullptr;
}

bool heap::free(uint32_t flags, void* base_address) {
    print("FIXME - heap::free(");
    print_hex(flags);
    print(", ");
    print_hex((uintptr_t)base_address);
    print(")\n");

    // FIXME

    return true;
}

static constexpr size_t round_up(size_t val, uint64_t div) {
    if (val & (div - 1)) {
        val &= ~(div - 1);
        val += div;
    }

    return val;
}

extern "C" __declspec(dllexport)
void* RtlCreateHeap(uint32_t, void*, size_t reserve_size, size_t commit_size,
                    void*, RTL_HEAP_PARAMETERS*) {
    void* addr = nullptr;

    // FIXME - flags
    // FIXME - heap_base
    // FIXME - lock
    // FIXME - parameters

    reserve_size = round_up(reserve_size, PAGE_SIZE);
    commit_size = round_up(commit_size, PAGE_SIZE);

    // see https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlcreateheap
    if (reserve_size == 0) {
        if (commit_size == 0) {
            reserve_size = 64 << PAGE_SHIFT;
            commit_size = PAGE_SIZE;
        } else
            reserve_size = round_up(commit_size, 16 * PAGE_SIZE);
    } else {
        if (commit_size == 0)
            commit_size = PAGE_SIZE;
        else if (commit_size > reserve_size)
            commit_size = reserve_size;
    }

    auto ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &reserve_size,
                                       MEM_RESERVE, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned ");
        print_hex((uint32_t)ret);
        print("\n");
        return nullptr;
    }

    ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &commit_size,
                                  MEM_COMMIT, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned ");
        print_hex((uint32_t)ret);
        print("\n");
        return nullptr;
    }

    auto h = new (addr) heap(reserve_size, commit_size);

    return h;
}

extern "C" __declspec(dllexport)
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.allocate(flags, size);
}

extern "C" __declspec(dllexport)
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.free(flags, base_address);
}

extern "C" __declspec(dllexport)
void* RtlDestroyHeap(void* heap_handle) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    h.heap::~heap();

    size_t region_size = 0;

    auto ret = NtFreeVirtualMemory(NtCurrentProcess(), &heap_handle, &region_size, MEM_RELEASE);

    if (!success(ret)) {
        print("NtFreeVirtualMemory returned ");
        print_hex((uint32_t)ret);
        print("\n");
        return nullptr;
    }

    return nullptr;
}
