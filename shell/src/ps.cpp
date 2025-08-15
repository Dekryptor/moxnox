#include <ntdll.h>
#include <algorithm>
#include <span>

import unique_handle;
import shelllib;
import print;
import list;

using namespace std;

struct ps_entry {
    list_entry le;
    uintptr_t pid;
};

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static nt_status ps() {
    PROCESS_BASIC_INFORMATION pbi;
    unique_handle proc;
    HANDLE h;
    list_entry list = { &list, &list };
    unsigned int num_entries = 0;

    while (true) {
        auto status = NtGetNextProcess(proc.get(), 0, 0, 0, &h);
        if (status == nt_status::NO_MORE_ENTRIES)
            break;
        else if (!success(status)) {
            print("NtGetNextProcess returned {}\n", status);
            return status;
        }

        proc.reset(h);

        status = NtQueryInformationProcess(proc.get(),
                                           PROCESSINFOCLASS::BasicInformation,
                                           &pbi, sizeof(pbi), nullptr);
        if (!success(status)) {
            print("NtQueryInformationProcess returned {}\n", status);
            return status;
        }

        auto e = (ps_entry*)malloc(sizeof(ps_entry));
        if (!e) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        e->pid = pbi.UniqueProcessId;

        // FIXME - get process name

        insert_tail_list(list, e->le);
        num_entries++;
    }

    // sort list by PID

    auto arr = (ps_entry**)malloc(sizeof(ps_entry*) * num_entries);
    if (!arr)
        return nt_status::INSUFFICIENT_RESOURCES;

    {
        auto ptr = arr;

        for (auto le = list.next; le != &list; le = le->next) {
            auto& ent = containing_record<ps_entry>(le);

            *ptr = &ent;
            ptr++;
        }
    }

    ranges::sort(span(arr, num_entries), [](ps_entry* a, ps_entry* b) -> bool {
        return a->pid < b->pid;
    });

    unsigned int pid_length = 5;

    if (num_entries > 0) {
        auto& e = containing_record<ps_entry>(list.prev);
        auto pid = e.pid;
        unsigned int sf = 0;

        while (pid > 0) {
            sf++;
            pid /= 10;
        }

        pid_length = max(pid_length, sf);
    }

    for (unsigned int i = 3; i < pid_length; i++) {
        print(" ");
    }

    print("PID\n");

    for (unsigned int i = 0; i < num_entries; i++) {
        auto& e = *arr[i];

        print("{:{}}\n", e.pid, pid_length);

        remove_entry_list(e.le);
        free(&e);
    }

    return nt_status::SUCCESS;
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    auto status = ps();

    NtTerminateThread(NtCurrentThread(), status);
}
