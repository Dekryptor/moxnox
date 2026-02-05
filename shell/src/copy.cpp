module;

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <span>
#include <string_view>
#include <utility>
#include <algorithm>

export module copy;

import nt;
import list;
import print;
import unique_handle;

using namespace std;

export struct dir {
    HANDLE src;
    HANDLE dest;
    unsigned int handle_refcount;
    unsigned int alloc_refcount;
    size_t destname_len;
    char16_t destname[0];
};

export struct src_file {
    list_entry le;
    dir* destdir;
    u16string_view fn;
    size_t name_len;
    char16_t name[0];
};

static nt_status copy_file(HANDLE srch, HANDLE desth, u16string_view src,
                           u16string_view destname, u16string_view fn,
                           string_view prog_name) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_STANDARD_INFORMATION fsi;

    static const size_t COPY_SIZE = 1048576;

    // get src file size

    status = NtQueryInformationFile(srch, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("{}: '{}': NtQueryInformationFile returned {}\n",
                     prog_name, src, status);
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
            print_stderr("{}: '{}': NtReadFile returned {}\n",
                         prog_name, src, status);
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
                print_stderr("{}: '{}/{}': NtWriteFile returned {}\n",
                             prog_name, destname, fn, status);
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

                d.handle_refcount++;
                d.alloc_refcount++;

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

export nt_status cp_recursive(u16string_view src, HANDLE srcdir,
                              HANDLE destdir, u16string_view fn,
                              u16string_view destname, list_entry& to_copy,
                              bool recursive, string_view prog_name) {
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE srch, desth;
    FILE_STANDARD_INFORMATION fsi;
    uint32_t create_options;

    // open src

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = srcdir;

    us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)src.data();

    create_options = FILE_SYNCHRONOUS_IO_ALERT;

    if (!recursive)
        create_options |= FILE_NON_DIRECTORY_FILE;

    status = NtCreateFile(&srch, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb,
                          nullptr, 0, 0, FILE_OPEN, create_options,
                          nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("{}: '{}': No such file or directory\n", prog_name, src);
        return status;
    } else if (status == nt_status::FILE_IS_A_DIRECTORY) {
        print_stderr("{}: -r not specified; omitting directory '{}'\n", prog_name, src);
        return status;
    } else if (!success(status)) {
        print_stderr("{}: '{}': NtCreateFile returned {}\n", prog_name, src, status);
        return status;
    }

    // check if file or directory

    status = NtQueryInformationFile(srch, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("{}: {}: NtQueryInformationFile returned {}\n", prog_name, src, status);
        NtClose(srch);
        return status;
    }

    // create destination

    if (fsi.Directory)
        create_options = FILE_DIRECTORY_FILE;
    else
        create_options = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT;

    oa.RootDirectory = destdir;

    if (fn.empty()) {
        if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
        else
            fn = src;
    }

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    status = NtCreateFile(&desth, FILE_WRITE_DATA | SYNCHRONIZE | DELETE, &oa,
                          &iosb, nullptr, 0, 0, FILE_OVERWRITE_IF,
                          create_options, nullptr, 0);
    if (status == nt_status::OBJECT_PATH_NOT_FOUND ||
             status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("{}: cannot create regular file '{}/{}': No such file or directory\n",
                     prog_name, destname, fn);
        NtClose(srch);
        return status;
    } else if (!success(status)) {
        print_stderr("{}: cannot create regular file '{}/{}': NtCreateFile returned {}\n",
                     prog_name, destname, fn, status);
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
        d->handle_refcount = 0;
        d->alloc_refcount = 0;

        status = search_directory(srch, *d, to_copy);

        if (d->handle_refcount == 0) {
            NtClose(d->src);
            NtClose(d->dest);
        }

        return status;
    }

    // copy

    // FIXME - show error if src and dest are same file (open dest first, and compare inode numbers and device?)

    // FIXME - --interactive option
    // FIXME - preallocate size
    // FIXME - check src is file
    // FIXME - copying things other than files (symlinks, Linux block or char devs, etc.)

    status = copy_file(srch, desth, src, destname, fn, prog_name);

    NtClose(srch);
    NtClose(desth);

    return status;
}

export void delete_file(u16string_view dir, u16string_view fn,
                        string_view prog_name) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle destdir, file;
    FILE_DISPOSITION_INFORMATION fdi;

    // open directory

    us.Length = us.MaximumLength = (uint16_t)(dir.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dir.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        print_stderr("{}: '{}': NtCreateFile returned {}\n",
                     prog_name, dir, status);
        return;
    }

    destdir.reset(h);

    // open file

    oa.RootDirectory = destdir.get();
    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("{}: '{}/{}': NtCreateFile returned {}\n",
                     prog_name, dir, fn, status);
        return;
    }

    file.reset(h);

    // delete

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file.get(), &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        print_stderr("{}: '{}/{}': NtSetInformationFile returned {} when trying to delete\n",
                     prog_name, dir, fn, status);
    }
}
