#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>
#include <string.h>

import shelllib;
import print;

using namespace std;

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static void take_screenshot() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    HANDLE sect, file;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    void* addr;
    size_t size;

    static const u16string_view fb = u"\\dev\\framebuffer";
    static const u16string_view fn = u"screenshot-out";

    us.Length = us.MaximumLength = (uint16_t)(fb.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fb.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenSection(&sect, SECTION_MAP_READ, &oa);
    if (!success(status)) {
        print("NtOpenSection returned {}\n", status);
        return;
    }

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);
    if (!success(status)) {
        print("NtMapViewOfSection returned {}\n", status);
        return;
    }

    NtClose(sect);

    print("mapped framebuffer at {:x} (size {:x})\n", (uintptr_t)addr, size);

    // save to file

    // FIXME - put timestamp in filename

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&file, FILE_WRITE_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_CREATE,
                          0, nullptr, 0);

    if (!success(status)) {
        print("NtCreateFile returned {}\n", status); // FIXME - stderr
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return;
    }

    span framebuffer((uint8_t*)addr, size / sizeof(uint8_t));

    // FIXME - write as png

    // FIXME - do writes in one call once we can

    auto buf = framebuffer;
    while (!buf.empty()) {
        auto to_write = (uint32_t)min((size_t)4096, buf.size());

        status = NtWriteFile(file, 0, nullptr, nullptr, &iosb, buf.data(),
                             to_write, nullptr, nullptr);
        if (!success(status)) {
            print("NtWriteFile returned {}\n", status);
            NtClose(file);
            NtUnmapViewOfSection(NtCurrentProcess(), addr);
            return;
        }

        buf = span(buf.data() + iosb.Information, buf.size() - iosb.Information);
    }

    NtClose(file);

    NtUnmapViewOfSection(NtCurrentProcess(), addr);

}

extern "C"
void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    auto argc = count_cmdline_args(cmdline);
    span<u16string_view> argv;
    u16string_view* buf = nullptr;

    if (argc != 0) {
        buf = (decltype(buf))RtlAllocateHeap(GetProcessHeap(), 0, argc * sizeof(*buf));
        if (!buf)
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

        argv = span(buf, argc);
    }

    parse_cmdline(cmdline, argv);

    take_screenshot();

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
