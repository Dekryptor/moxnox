#include <ntdll.h>
#include <algorithm>
#include <span>
#include <string_view>

import unique_handle;
import shelllib;
import print;
import list;

using namespace std;

struct ps_entry {
    ~ps_entry() {
        if (image_path)
            free(image_path);
    }

    list_entry le;
    uintptr_t pid;
    char16_t* image_path = nullptr;
    u16string_view image_path_sv;
};

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

void* operator new(size_t count) {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

static void get_process_name(ps_entry& e, HANDLE proc, void* peb_address) {
    PEB peb;
    RTL_USER_PROCESS_PARAMETERS params;
    nt_status status;
    uint32_t read;

    status = NtReadVirtualMemory(proc, peb_address, &peb, sizeof(peb), &read);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    } else if (read < sizeof(peb)) {
        print("short read while reading PEB ({} bytes, expected {})\n",
              read, sizeof(peb));
        return;
    }

    status = NtReadVirtualMemory(proc, peb.ProcessParameters, &params,
                                 sizeof(params), &read);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    } else if (read < sizeof(params)) {
        print("short read while reading process params ({} bytes, expected {})\n",
              read, sizeof(params));
        return;
    }

    if (params.ImagePathName.Length == 0)
        return;

    e.image_path = (char16_t*)malloc(params.ImagePathName.Length);
    if (!e.image_path) {
        print("malloc failed\n");
        return;
    }

    status = NtReadVirtualMemory(proc, params.ImagePathName.Buffer, e.image_path,
                                 params.ImagePathName.Length, &read);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    } else if (read < params.ImagePathName.Length) {
        print("short read while reading image path name ({} bytes, expected {})\n",
              read, params.ImagePathName.Length);
        return;
    }

    e.image_path_sv = u16string_view(e.image_path,
                                     params.ImagePathName.Length / sizeof(char16_t));
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

        // auto e = (ps_entry*)malloc(sizeof(ps_entry));
        auto e = new ps_entry;
        if (!e) {
            print("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        e->pid = pbi.UniqueProcessId;

        get_process_name(*e, proc.get(), pbi.PebBaseAddress);

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

    print("PID");
    // FIXME - TTY
    // FIXME - TIME
    print(" CMD\n");

    // FIXME - all the other format specifiers

    for (unsigned int i = 0; i < num_entries; i++) {
        auto& e = *arr[i];

        print("{:{}} ", e.pid, pid_length);

        if (!e.image_path_sv.empty()) {
            auto sv = e.image_path_sv;

            if (auto bs = sv.find_last_of(u'\\'); bs != u16string_view::npos)
                sv = u16string_view(sv.data() + bs + 1, sv.size() - bs - 1);

            if (auto dot = sv.find_last_of(u'.'); dot != u16string_view::npos)
                sv = u16string_view(sv.data(), dot);

            print("{}", sv);
        }

        print("\n");

        remove_entry_list(e.le);
        delete &e;
    }

    return nt_status::SUCCESS;
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    auto status = ps();

    NtTerminateThread(NtCurrentThread(), status);
}
