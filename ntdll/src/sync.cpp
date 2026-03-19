#include <stddef.h>
#include <string.h>
#include <atomic>

import nt;
import ntdll;
import print;
import list;
import unique_handle;

using namespace std;

// FIXME? - LockSemaphore HANDLE sticks around until end of program (is this an issue?)
static RTL_CRITICAL_SECTION waiters_lock = {
    .DebugInfo = nullptr,
    .LockCount = -1,
    .RecursionCount = 0,
    .OwningThread = 0,
    .LockSemaphore = 0,
    .SpinCount = 4000
};

struct wait_entry {
    const void* address;
    HANDLE event;
    bool removed;
    list_entry le;
};

static list_entry waiters = { &waiters, &waiters };

extern "C"
void RtlInitializeConditionVariable(RTL_CONDITION_VARIABLE* var) {
    var->Ptr = nullptr;
}

extern "C"
void RtlWakeAllConditionVariable(RTL_CONDITION_VARIABLE* var) {
    atomic_ref<uintptr_t> ptr_ref(*(uintptr_t*)&var->Ptr);

    ptr_ref.fetch_add(1, memory_order_release);
    RtlWakeAddressAll(&var->Ptr);
}

extern "C"
nt_status RtlSleepConditionVariableCS(RTL_CONDITION_VARIABLE* var,
                                      RTL_CRITICAL_SECTION* crit,
                                      const LARGE_INTEGER* timeout) {
    atomic_ref<uintptr_t> ptr_ref(*(uintptr_t*)&var->Ptr);
    auto val = ptr_ref.load(memory_order_acquire);

    RtlLeaveCriticalSection(crit);
    auto ret = RtlWaitOnAddress(&var->Ptr, &val, sizeof(val), timeout);
    RtlEnterCriticalSection(crit);

    return ret;
}

extern "C"
nt_status RtlWaitOnAddress(const void* addr, const void* cmp,
                           size_t size, const LARGE_INTEGER* timeout) {
    nt_status status;
    unique_handle event;

    if (memcmp(addr, cmp, size))
        return nt_status::SUCCESS;

    // FIXME - have thread-local event instead? (where would we free HANDLE?)

    {
        HANDLE h;

        status = NtCreateEvent(&h, 0, nullptr, EVENT_TYPE::SynchronizationEvent,
                               false);
        if (!success(status))
            return status;

        event.reset(h);
    }

    wait_entry entry;
    entry.address = addr;
    entry.event = event.get();
    entry.removed = false;

    RtlEnterCriticalSection(&waiters_lock);

    if (memcmp(addr, cmp, size)) {
        RtlLeaveCriticalSection(&waiters_lock);
        return nt_status::SUCCESS;
    }

    insert_tail_list(waiters, entry.le);

    RtlLeaveCriticalSection(&waiters_lock);

    status = NtWaitForSingleObject(event.get(), false, timeout);

    if (!entry.removed) {
        RtlEnterCriticalSection(&waiters_lock);

        if (!entry.removed)
            remove_entry_list(entry.le);

        RtlLeaveCriticalSection(&waiters_lock);
    }

    return status;
}

extern "C"
void RtlWakeConditionVariable(RTL_CONDITION_VARIABLE* var) {
    atomic_ref<uintptr_t> ptr_ref(*(uintptr_t*)&var->Ptr);

    ptr_ref.fetch_add(1, memory_order_release);
    RtlWakeAddressSingle(&var->Ptr);
}

extern "C"
void RtlWakeAddressSingle(const void* addr) {
    RtlEnterCriticalSection(&waiters_lock);

    for (auto le = waiters.next; le != &waiters; le = le->next) {
        auto& e = containing_record<wait_entry>(le);

        if (e.address == addr) {
            NtSetEvent(e.event, nullptr);
            remove_entry_list(e.le);
            e.removed = true;
            break;
        }
    }

    RtlLeaveCriticalSection(&waiters_lock);
}

extern "C"
void RtlWakeAddressAll(const void* addr) {
    RtlEnterCriticalSection(&waiters_lock);

    for (auto le = waiters.next; le != &waiters; le = le->next) {
        auto& e = containing_record<wait_entry>(le);

        if (e.address == addr) {
            auto prev = e.le.prev;

            NtSetEvent(e.event, nullptr);
            remove_entry_list(e.le);
            e.removed = true;

            le = prev;
        }
    }

    RtlLeaveCriticalSection(&waiters_lock);
}

extern "C"
void RtlAcquireSRWLockExclusive(RTL_SRWLOCK* /*lock*/) {
    print_stderr("FIXME - RtlAcquireSRWLockExclusive\n");
}

extern "C"
void RtlReleaseSRWLockExclusive(RTL_SRWLOCK* /*lock*/) {
    print_stderr("FIXME - RtlReleaseSRWLockExclusive\n");
}
