#include <sys/mman.h>
#include <errno.h>
#include <utility>

import nt;
import ntdll;
import unique_handle;
import ucrtbase;
import pagesize;

#undef errno
import errno;

using namespace std;

void* operator new(size_t count) {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void* operator new[](size_t count) {
    return malloc(count);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    free(ptr);
}

extern "C"
void* mmap(void* /*addr*/, size_t len, int prot, int flags, int fd, off_t off) {
    nt_status status;
    LARGE_INTEGER lisize, lioff;
    HANDLE file, h;
    uint32_t allocation_attributes, page_protection, ceiling;
    size_t view_size;
    void* base_address;

    // FIXME - MAP_FIXED
    // FIXME - does PROT_NONE work?

    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
        *_errno() = EINVAL;
        return MAP_FAILED;
    }

    if (flags & ~(MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE | MAP_SHARED)) {
        *_errno() = EINVAL;
        return MAP_FAILED;
    }

    if (prot & PROT_WRITE && prot & PROT_EXEC)
        page_protection = flags & MAP_SHARED ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_WRITECOPY;
    else if (prot & PROT_EXEC)
        page_protection = PAGE_EXECUTE_READ;
    else if (prot & PROT_WRITE)
        page_protection = flags & MAP_SHARED ? PAGE_READWRITE : PAGE_WRITECOPY;
    else if (prot & PROT_READ)
        page_protection = PAGE_READONLY;
    else
        page_protection = PAGE_NOACCESS;

    if (!(flags & MAP_ANONYMOUS)) {
        file = (HANDLE)_get_osfhandle(fd);
        if (file == (HANDLE)-1) {
            *_errno() = EINVAL;
            return MAP_FAILED;
        }

        allocation_attributes = SEC_FILE;
    } else {
        file = 0;
        allocation_attributes = SEC_COMMIT;
    }

    lisize.QuadPart = (int64_t)len + (int64_t)off;

    if (flags & MAP_ANONYMOUS)
        ceiling = PAGE_EXECUTE_READWRITE;
    else
        ceiling = page_protection;

    // FIXME - access mask
    status = NtCreateSection(&h, 0, nullptr, &lisize, ceiling,
                             allocation_attributes, file);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return MAP_FAILED;
    }

    unique_handle sect{h};

    lioff.QuadPart = off;
    view_size = len;
    base_address = nullptr;

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &base_address,
                                0, 0, &lioff, &view_size, SECTION_INHERIT::ViewUnmap,
                                0, page_protection);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return MAP_FAILED;
    }

    return base_address;
}

extern "C"
int munmap(void* addr, size_t len) {
    MN_MEMORY_UNMAP params;

    params.addr = addr;
    params.length = len;

    auto status = NtSetInformationProcess(NtCurrentProcess(), PROCESS_INFORMATION_CLASS::MoxnoxMemoryUnmap,
                                          &params, sizeof(params));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int getpagesize() {
    return PAGE_SIZE;
}

extern "C"
int msync(void* addr, size_t length, int flags) {
    IO_STATUS_BLOCK iosb;

    if ((flags & ~MS_INVALIDATE) != MS_SYNC && (flags & ~MS_INVALIDATE) != MS_ASYNC) {
        *_errno() = EINVAL;
        return -1;
    }

    if (flags & MS_ASYNC) // nop
        return 0;

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &addr, &length,
                                       &iosb);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
void* aligned_alloc(size_t alignment, size_t size) {
    return mn_allocate_aligned(GetProcessHeap(), alignment, size);
}

extern "C"
int posix_memalign(void** memptr, size_t alignment, size_t size) {
    auto ptr = aligned_alloc(alignment, size);

    if (!ptr)
        return ENOMEM;

    *memptr = ptr;

    return 0;
}

extern "C"
int mprotect(void* addr, size_t size, int prot) {
    nt_status status;
    uint32_t type, old_type;

    if (prot != PROT_NONE && prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
        *_errno() = ENOTSUP;
        return -1;
    }

    if (prot == PROT_NONE)
        type = PAGE_NOACCESS;
    else {
        switch (prot) {
            case PROT_READ:
                type = PAGE_READONLY;
                break;

            case PROT_WRITE:
            case PROT_READ | PROT_WRITE:
                type = PAGE_READWRITE;
                break;

            case PROT_EXEC:
            case PROT_READ | PROT_EXEC:
                type = PAGE_EXECUTE_READ;
                break;

            case PROT_WRITE | PROT_EXEC:
            case PROT_READ | PROT_WRITE | PROT_EXEC:
                type = PAGE_EXECUTE_READWRITE;
                break;

            default:
                unreachable();
        }
    }

    status = NtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, type,
                                    &old_type);

    // if we failed, try again with COW versions
    if (status == nt_status::ACCESS_DENIED && (type == PAGE_READWRITE || type == PAGE_EXECUTE_READWRITE)) {
        if (type == PAGE_READWRITE)
            type = PAGE_WRITECOPY;
        else
            type = PAGE_EXECUTE_WRITECOPY;

        status = NtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, type,
                                        &old_type);
    }

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}
