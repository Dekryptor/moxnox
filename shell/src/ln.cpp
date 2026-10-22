// SPDX-License-Identifier: GPL-2.0-only
#include <string_view>
#include <memory>
#include <span>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

import nt;
import print;
import unique_handle;
import unicode;

using namespace std;

static int create_symlink(string_view target, string_view symlink) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_ptr<char16_t[]> u16;
    HANDLE h;
    unique_handle f;
    unique_ptr<char[]> rdbbuf;

    if (symlink.empty()) {
        print_stderr("ln: no filename given\n");
        return 1;
    }

    if (target.empty()) {
        print_stderr("ln: no target given\n");
        return 1;
    }

    auto u16len = utf8_to_utf16_len(symlink);
    u16.reset(new char16_t[u16len]);
    if (!u16) {
        print_stderr("malloc failed\n");
        return 1;
    }

    utf8_to_utf16(span(u16.get(), u16len), symlink);

    // try to create symlink

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16.get();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES | DELETE, &oa, &iosb, nullptr,
                          0, 0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        print_stderr("ln: '{}': file already exists\n", symlink);
        return 1;
    } else if (!success(status)) {
        print_stderr("ln: '{}': NtCreateFile returned {}\n", symlink, status);
        return 1;
    } else
        f.reset(h);

    auto u16targetlen = utf8_to_utf16_len(target);

    size_t rdblen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + (u16targetlen * sizeof(char16_t));

    rdbbuf.reset(new char[rdblen]);
    if (!rdbbuf) {
        print_stderr("malloc failed\n");
        return 1;
    }

    auto& rdb = *(REPARSE_DATA_BUFFER*)rdbbuf.get();

    rdb.ReparseTag = IO_REPARSE_TAG_SYMLINK;
    rdb.ReparseDataLength = 0;
    rdb.Reserved = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameLength = (uint16_t)(u16targetlen * sizeof(char16_t));
    rdb.SymbolicLinkReparseBuffer.PrintNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.PrintNameLength = 0;
    rdb.SymbolicLinkReparseBuffer.Flags = 0;

    utf8_to_utf16(span(rdb.SymbolicLinkReparseBuffer.PathBuffer, u16targetlen), target);

    status = NtFsControlFile(f.get(), 0, nullptr, nullptr, &iosb,
                             FSCTL_SET_REPARSE_POINT, &rdb, (uint32_t)rdblen,
                             nullptr, 0);
    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        print_stderr("ln: '{}': NtFsControlFile returned {}\n", symlink, status);

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        return 1;
    }

    return 0;
}

static int create_hardlink(string_view src, string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_ptr<char16_t[]> u16;
    HANDLE h;
    unique_handle f;
    unique_ptr<char[]> flibuf;

    if (src.empty()) {
        print_stderr("ln: no filename given\n");
        return 1;
    }

    if (dest.empty()) {
        print_stderr("ln: no target given\n");
        return 1;
    }

    auto u16len = utf8_to_utf16_len(src);
    u16.reset(new char16_t[u16len]);
    if (!u16) {
        print_stderr("malloc failed\n");
        return 1;
    }

    utf8_to_utf16(span(u16.get(), u16len), src);

    // try to open source

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16.get();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT,
                          nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("ln: '{}': No such file or directory\n", src);
        return 1;
    } else if (!success(status)) {
        print_stderr("ln: '{}': NtCreateFile returned {}\n", src, status);
        return 1;
    } else
        f.reset(h);

    auto u16destlen = utf8_to_utf16_len(dest);

    size_t flilen = offsetof(FILE_LINK_INFORMATION, FileName) + (u16destlen * sizeof(char16_t));

    flibuf.reset(new char[flilen]);
    if (!flibuf) {
        print_stderr("malloc failed\n");
        return 1;
    }

    auto& fli = *(FILE_LINK_INFORMATION*)flibuf.get();

    fli.ReplaceIfExists = false; // FIXME - option for this
    fli.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;
    fli.FileNameLength = (uint32_t)(u16destlen * sizeof(char16_t));

    utf8_to_utf16(span(fli.FileName, u16destlen), dest);

    status = NtSetInformationFile(f.get(), &iosb, &fli, (uint32_t)flilen,
                                  FILE_INFORMATION_CLASS::LinkInformation);

    if (!success(status)) {
        print_stderr("ln: '{}': NtSetInformationFile returned {}\n", src, status);
        return 1;
    }

    return 0;
}

int main(int argc, char** argv) {
    bool symbolic = false;

    static const option options[] = {
        { "symbolic", no_argument, nullptr, 's' },
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "s", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 's':
                symbolic = true;
                break;

            case '?':
                return 1;
        }
    }

    if (argc == optind) {
        print_stderr("ln: missing file operand\n");
        return 1;
    }

    if (argc == optind + 1) {
        print_stderr("ln: missing destination file operand after '{}'\n", argv[0]);
        return 1;
    }

    if (symbolic)
        return create_symlink(argv[optind], argv[optind + 1]);
    else
        return create_hardlink(argv[optind], argv[optind + 1]);
}
