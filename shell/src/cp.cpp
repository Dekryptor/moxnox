#include <string_view>
#include <span>
#include <utility>
#include <algorithm>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;
import ntdll;
import print;
import cmdline;
import unique_handle;
import getopt;
import list;

using namespace std;

static bool recursive = false;

struct dir {
    HANDLE src;
    HANDLE dest;
    unsigned left;
    size_t destname_len;
    char16_t destname[0];
};

struct src_file {
    list_entry le;
    dir* destdir;
    size_t name_len;
    char16_t name[0];
};

static nt_status copy_file(HANDLE srch, HANDLE desth, u16string_view src,
                           u16string_view dest) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_STANDARD_INFORMATION fsi;

    static const size_t COPY_SIZE = 1048576;

    // get src file size

    status = NtQueryInformationFile(srch, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("cp: '{}': NtQueryInformationFile returned {}\n", src, status);
        return status;
    }

    // FIXME - also copy attributes, xattrs, SD, etc.

    // FIXME - copy by mmap instead?

    auto left = (uint64_t)fsi.EndOfFile.QuadPart;

    if (left == 0)
        return nt_status::SUCCESS;

    auto bufsize = max(COPY_SIZE, left);

    auto buf = malloc(bufsize);
    if (!buf) {
        print_stderr("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    while (left > 0) {
        uint64_t read_bytes;

        status = NtReadFile(srch, 0, nullptr, nullptr, &iosb, buf,
                            (uint32_t)min(left, bufsize), nullptr, nullptr);

        if (status == nt_status::END_OF_FILE)
            break;

        if (!success(status)) {
            print_stderr("cp: '{}': NtReadFile returned {}\n", src, status);
            break;
        }

        read_bytes = iosb.Information;

        if (read_bytes == 0)
            continue;

        span sp((char*)buf, read_bytes);

        do {
            status = NtWriteFile(desth, 0, nullptr, nullptr, &iosb,
                                 sp.data(), (uint32_t)sp.size(), nullptr,
                                 nullptr);
            if (!success(status)) {
                print_stderr("cp: '{}': NtWriteFile returned {}\n", dest, status);
                break;
            }

            if (iosb.Information == sp.size())
                break;

            sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
        } while (true);

        if (!success(status))
            break;

        left -= read_bytes;
    }

    free(buf);

    if (status == nt_status::END_OF_FILE)
        status = nt_status::SUCCESS;

    return nt_status::SUCCESS;
};

static nt_status cp(u16string_view src, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle srch, desth, destdir;

    // try to open dest

    us.Length = us.MaximumLength = (uint16_t)(dest.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dest.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (success(status))
        destdir.reset(h);
    else if (status != nt_status::OBJECT_PATH_NOT_FOUND &&
             status != nt_status::OBJECT_NAME_NOT_FOUND &&
             status != nt_status::NOT_A_DIRECTORY) {
        print_stderr("cp: '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    // open src

    us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)src.data();

    status = NtCreateFile(&h, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN,
                          FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cp: '{}': No such file or directory\n", src);
        return status;
    } else if (status == nt_status::FILE_IS_A_DIRECTORY) {
        print_stderr("cp: -r not specified; omitting directory '{}'\n", src); // FIXME
        return status;
    } else if (!success(status)) {
        print_stderr("cp: '{}': NtCreateFile returned {}\n", src, status);
        return status;
    }

    srch.reset(h);

    // open dst

    u16string_view fn;

    if (destdir) {
        if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
        else
            fn = src;

        oa.RootDirectory = destdir.get();
    } else
        fn = dest;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    // FIXME - show error if src and dst are same file (open dst first, and compare inode numbers and device?)

    // FIXME - --interactive option
    // FIXME - preallocate size
    // FIXME - check src is file
    // FIXME - copying things other than files (symlinks, Linux block or char devs, etc.)

    status = NtCreateFile(&h, FILE_WRITE_DATA | SYNCHRONIZE | DELETE, &oa, &iosb,
                          nullptr, 0, 0, FILE_OVERWRITE_IF,
                          FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);

    if (success(status))
        desth.reset(h);
    else if (status == nt_status::OBJECT_PATH_NOT_FOUND ||
             status == nt_status::OBJECT_NAME_NOT_FOUND) {
        if (destdir)
            print_stderr("cp: cannot create regular file '{}/{}': No such file or directory\n", dest, fn);
        else
            print_stderr("cp: cannot create regular file '{}': No such file or directory\n", dest);
        return status;
    } else {
        if (destdir)
            print_stderr("cp: cannot create regular file '{}/{}': NtCreateFile returned {}\n", dest, fn, status);
        else
            print_stderr("cp: cannot create regular file '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    {
        u16string_view dest2;
        char16_t* fnbuf = nullptr;

        if (destdir) {
            fnbuf = (char16_t*)malloc((dest.size() + 1 + fn.size()) * sizeof(char16_t));
            if (!fnbuf) {
                print("malloc failed\n");
                // FIXME - delete dst if this happens
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            memcpy(fnbuf, dest.data(), dest.size() * sizeof(char16_t));
            fnbuf[dest.size()] = u'/';
            memcpy(fnbuf + dest.size() + 1, fn.data(),
                   fn.size() * sizeof(char16_t));

            dest2 = u16string_view(fnbuf, dest.size() + 1 + fn.size());
        } else
            dest2 = dest;

        status = copy_file(srch.get(), desth.get(), src, dest2);

        if (fnbuf)
            free(fnbuf);
    }

    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        auto status2 = NtSetInformationFile(desth.get(), &iosb, &fdi, sizeof(fdi),
                                            FILE_INFORMATION_CLASS::DispositionInformation);
        if (!success(status2))
            print_stderr("cp: '{}': NtSetInformationFile returned {} when trying to delete\n", fn, status2);
    }

    return status;
}

static nt_status cp_multiple(span<u16string_view> srcs, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle destdir;

    // try to open dest

    us.Length = us.MaximumLength = (uint16_t)(dest.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dest.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("cp: target '{}': Not a directory\n", dest);
        return status;
    } else if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
               status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cp: target '{}': No such file or directory\n", dest);
        return status;
    } else if (!success(status)) {
        print_stderr("cp: target '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    destdir.reset(h);

    for (auto src : srcs)  {
        unique_handle srch, desth;

        // open src

        oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

        us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
        us.Buffer = (char16_t*)src.data();

        status = NtCreateFile(&h, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb,
                              nullptr, 0, 0, FILE_OPEN,
                              FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                              nullptr, 0);
        if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
            status == nt_status::OBJECT_PATH_NOT_FOUND) {
            print_stderr("cp: '{}': No such file or directory\n", src);
            continue;
        } else if (status == nt_status::FILE_IS_A_DIRECTORY) {
            print_stderr("cp: -r not specified; omitting directory '{}'\n", src); // FIXME
            continue;
        } else if (!success(status)) {
            print_stderr("cp: '{}': NtCreateFile returned {}\n", src, status);
            continue;
        }

        srch.reset(h);

        // open dst

        u16string_view fn;

        if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
        else
            fn = src;

        oa.RootDirectory = destdir.get();

        us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
        us.Buffer = (char16_t*)fn.data();

        // FIXME - show error if src and dst are same file (open dst first, and compare inode numbers and device?)

        // FIXME - --interactive option
        // FIXME - preallocate size
        // FIXME - check src is file
        // FIXME - copying things other than files (symlinks, Linux block or char devs, etc.)

        status = NtCreateFile(&h, FILE_WRITE_DATA | SYNCHRONIZE | DELETE, &oa,
                              &iosb, nullptr, 0, 0, FILE_OVERWRITE_IF,
                              FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                              nullptr, 0);

        if (success(status))
            desth.reset(h);
        else if (status == nt_status::OBJECT_PATH_NOT_FOUND ||
                status == nt_status::OBJECT_NAME_NOT_FOUND) {
            print_stderr("cp: cannot create regular file '{}/{}': No such file or directory\n",
                         dest, fn);
            continue;
        } else {
            print_stderr("cp: cannot create regular file '{}/{}': NtCreateFile returned {}\n",
                         dest, fn, status);
            continue;
        }

        // copy

        {
            auto fnbuf = (char16_t*)malloc((dest.size() + 1 + fn.size()) * sizeof(char16_t));

            if (!fnbuf) {
                print("malloc failed\n");
                status = nt_status::INSUFFICIENT_RESOURCES;
            } else {
                memcpy(fnbuf, dest.data(), dest.size() * sizeof(char16_t));
                fnbuf[dest.size()] = u'/';
                memcpy(fnbuf + dest.size() + 1, fn.data(),
                    fn.size() * sizeof(char16_t));

                auto dest2 = u16string_view(fnbuf, dest.size() + 1 + fn.size());

                status = copy_file(srch.get(), desth.get(), src, dest2);

                free(fnbuf);
            }
        }

        if (!success(status)) {
            FILE_DISPOSITION_INFORMATION fdi;

            fdi.DeleteFile = true;

            auto status2 = NtSetInformationFile(desth.get(), &iosb, &fdi, sizeof(fdi),
                                                FILE_INFORMATION_CLASS::DispositionInformation);
            if (!success(status2))
                print_stderr("cp: '{}/{}': NtSetInformationFile returned {} when trying to delete\n", dest, fn, status2);
        }
    }

    // FIXME - what error should we return if some copies fail?

    return nt_status::SUCCESS;
}

static nt_status search_directory(HANDLE h, dir& d, list_entry& list) {
    IO_STATUS_BLOCK iosb;
    char buf[4096];
    bool first = true;

    while (true) {
        auto status = NtQueryDirectoryFile(h, 0, nullptr, nullptr,
                                           &iosb, buf, sizeof(buf),
                                           FILE_INFORMATION_CLASS::DirectoryInformation,
                                           false, nullptr, first);

        if (status == nt_status::NO_MORE_FILES)
            break;
        else if (!success(status)) {
            print_stderr("NtQueryDirectoryFile returned {}\n", status);
            return status;
        }

        auto fdi = (FILE_DIRECTORY_INFORMATION*)buf;

        while (true) {
            size_t sf_len = offsetof(src_file, name) + fdi->FileNameLength;
            bool ignore = false;

            if (fdi->FileNameLength == sizeof(char16_t) && fdi->FileName[0] == '.')
                ignore = true;
            else if (fdi->FileNameLength == 2 * sizeof(char16_t) &&
                     fdi->FileName[0] == '.' && fdi->FileName[1] == '.') {
                ignore = true;
            }

            if (!ignore) {
                auto sf = (src_file*)malloc(sf_len);
                if (!sf) {
                    print_stderr("malloc failed\n");
                    NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
                    unreachable();
                }

                d.left++;

                sf->destdir = &d;
                sf->name_len = fdi->FileNameLength / sizeof(char16_t);
                memcpy(&sf->name, fdi->FileName, fdi->FileNameLength);

                insert_tail_list(list, sf->le);
            }

            if (fdi->NextEntryOffset == 0)
                break;

            fdi = (FILE_DIRECTORY_INFORMATION*)((uint8_t*)fdi + fdi->NextEntryOffset);
        }

        first = false;
    }

    return nt_status::SUCCESS;
}

static void free_dir(dir& d) {
    NtClose(d.src);
    NtClose(d.dest);
    free(&d);
}

static nt_status cp_recursive2(u16string_view src, HANDLE srcdir,
                               HANDLE destdir, u16string_view destname,
                               list_entry& to_copy) {
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE srch, desth;
    FILE_STANDARD_INFORMATION fsi;
    uint32_t create_options;
    u16string_view fn;

    // open src

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = srcdir;

    us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)src.data();

    status = NtCreateFile(&srch, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb,
                          nullptr, 0, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cp: '{}': No such file or directory\n", src);
        return status;
    }

    // check if file or directory

    status = NtQueryInformationFile(srch, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("cp: {}: NtQueryInformationFile returned {}\n", src, status);
        NtClose(srch);
        return status;
    }

    // create destination

    if (fsi.Directory)
        create_options = FILE_DIRECTORY_FILE;
    else
        create_options = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT;

    oa.RootDirectory = destdir;

    if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
        fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
    else
        fn = src;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    status = NtCreateFile(&desth, FILE_WRITE_DATA | SYNCHRONIZE | DELETE, &oa,
                          &iosb, nullptr, 0, 0, FILE_OVERWRITE_IF,
                          create_options, nullptr, 0);
    if (status == nt_status::OBJECT_PATH_NOT_FOUND ||
             status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("cp: cannot create regular file '{}/{}': No such file or directory\n",
                     destname, fn);
        NtClose(srch);
        return status;
    } else if (!success(status)) {
        print_stderr("cp: cannot create regular file '{}/{}': NtCreateFile returned {}\n",
                     destname, fn, status);
        NtClose(srch);
        return status;
    }

    if (fsi.Directory) {
        size_t namelen = destname.size() + 1 + fn.size();
        auto d = (dir*)malloc(offsetof(dir, destname) + (namelen * sizeof(char16_t)));
        if (!d) {
            print_stderr("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        d->src = srch;
        d->dest = desth;
        d->destname_len = namelen;
        memcpy(d->destname, destname.data(), destname.size() * sizeof(char16_t));
        d->destname[destname.size()] = '/';
        memcpy(d->destname + destname.size() + 1, fn.data(),
               fn.size() * sizeof(char16_t));
        d->left = 0;

        status = search_directory(srch, *d, to_copy);

        if (d->left == 0)
            free_dir(*d);

        return status;
    }

    // copy

    // FIXME - show error if src and dest are same file (open dest first, and compare inode numbers and device?)

    // FIXME - --interactive option
    // FIXME - preallocate size
    // FIXME - check src is file
    // FIXME - copying things other than files (symlinks, Linux block or char devs, etc.)

    {
        auto fnbuf = (char16_t*)malloc((destname.size() + 1 + fn.size()) * sizeof(char16_t));

        if (!fnbuf) {
            print("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        memcpy(fnbuf, destname.data(), destname.size() * sizeof(char16_t));
        fnbuf[destname.size()] = u'/';
        memcpy(fnbuf + destname.size() + 1, fn.data(),
                fn.size() * sizeof(char16_t));

        auto dest2 = u16string_view(fnbuf, destname.size() + 1 + fn.size());

        status = copy_file(srch, desth, src, dest2);

        free(fnbuf);
    }

    NtClose(srch);
    NtClose(desth);

    return status;
}

static nt_status cp_recursive(span<u16string_view> srcs, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle destdir;
    list_entry to_copy = { &to_copy, &to_copy };

    // try to open dest

    us.Length = us.MaximumLength = (uint16_t)(dest.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dest.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("cp: target '{}': Not a directory\n", dest);
        return status;
    } else if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
               status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cp: target '{}': No such file or directory\n", dest);
        return status;
    } else if (!success(status)) {
        print_stderr("cp: target '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    destdir.reset(h);

    auto d = (dir*)malloc(offsetof(dir, destname) + (dest.size() * sizeof(char16_t)));
    if (!d) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    d->src = peb->ProcessParameters->CurrentDirectory.Handle;
    d->dest = destdir.get();
    d->left = 1;
    d->destname_len = dest.size();
    memcpy(d->destname, dest.data(), d->destname_len * sizeof(char16_t));

    for (auto src : srcs) {
        auto sf = (src_file*)malloc(offsetof(src_file, name) + (src.size() * sizeof(char16_t)));
        if (!sf) {
            print_stderr("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        d->left++;

        sf->destdir = d;
        sf->name_len = src.size();
        memcpy(sf->name, src.data(), src.size() * sizeof(char16_t));
        insert_tail_list(to_copy, sf->le);
    }

    for (auto le = to_copy.next; le != &to_copy; le = le->next) {
        auto& sf = containing_record<src_file>(le);

        auto src = u16string_view(sf.name, sf.name_len);

        auto destname = u16string_view(sf.destdir->destname, sf.destdir->destname_len);

        status = cp_recursive2(src, sf.destdir->src, sf.destdir->dest,
                               destname, to_copy);

        sf.destdir->left--;
        if (sf.destdir->left == 0)
            free_dir(*sf.destdir);

        if (!success(status)) {
            // free destdir of remaining items
            for (auto le2 = le->next; le2 != &to_copy; le2 = le2->next) {
                auto& sf2 = containing_record<src_file>(le2);

                sf2.destdir->left--;
                if (sf2.destdir->left == 0)
                    free_dir(*sf2.destdir);
            }

            // FIXME - delete items created so far
            break;
        }
    }

    while (!is_list_empty(to_copy)) {
        auto& sf = containing_record<src_file>(to_copy.next);

        remove_entry_list(sf.le);
        free(&sf);
    }

    free(d);

    return status;
}

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    static const option options[] = {
        { u"recursive", u'r', []{ recursive = true; } },
        { u"", u'R', []{ recursive = true; } },
    };

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    auto argc = count_cmdline_args(cmdline);
    span<u16string_view> argv;
    u16string_view* buf = nullptr;

    if (argc != 0) {
        buf = (decltype(buf))RtlAllocateHeap(GetProcessHeap(), 0, argc * sizeof(*buf));
        if (!buf) {
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        argv = span(buf, argc);
    }

    parse_cmdline(cmdline, argv);

    if (!argv.empty()) // skip program name
        argv = span(argv.data() + 1, argv.size() - 1);

    if (!getopt(argv, options)) {
        if (buf)
            RtlFreeHeap(GetProcessHeap(), 0, buf);

        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    if (argv.empty()) {
        print_stderr("cp: missing file operand\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    if (argv.size() < 2) {
        print_stderr("cp: missing destination file operand after '{}'\n", argv[0]);
        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    nt_status ret;

    if (recursive)
        ret = cp_recursive(span(argv.data(), argv.size() - 1), argv.back());
    else if (argv.size() > 2)
        ret = cp_multiple(span(argv.data(), argv.size() - 1), argv.back());
    else
        ret = cp(argv[0], argv[1]);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), ret);
}
