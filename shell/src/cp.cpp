#include <string_view>
#include <span>
#include <utility>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>

import nt;
import ntdll;
import print;
import unique_handle;
import list;
import copy;
import unicode;

using namespace std;

static bool recursive = false;

static int cp(span<u16string_view> srcs, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle destdirh;
    list_entry to_copy = { &to_copy, &to_copy };
    u16string_view destdir, fn;

    // try to open dest

    us.Length = us.MaximumLength = (uint16_t)(dest.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dest.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);

    if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cp: target '{}': No such file or directory\n", dest);
        return 1;
    } else if (success(status))
        destdirh.reset(h);

    if (srcs.size() == 1) {
        if (!destdirh) {
            if (status != nt_status::OBJECT_NAME_NOT_FOUND &&
                status != nt_status::NOT_A_DIRECTORY) {
                print_stderr("cp: '{}': NtCreateFile returned {}\n", dest, status);
                return 1;
            }

            if (auto slash = dest.find_last_of(u'/'); slash != u16string_view::npos) {
                destdir = u16string_view(dest.data(), slash);
                fn = u16string_view(dest.data() + slash + 1, dest.size() - slash - 1);
            } else {
                destdir = u".";
                fn = dest;
            }

            us.Length = us.MaximumLength = (uint16_t)(destdir.size() * sizeof(char16_t));
            us.Buffer = (char16_t*)destdir.data();

            status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                                  FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
            if (success(status))
                destdirh.reset(h);
        }
    } else
        destdir = dest;

    if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("cp: target '{}': Not a directory\n", destdir);
        return 1;
    } else if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("cp: target '{}': No such file or directory\n", destdir);
        return 1;
    } else if (!success(status)) {
        print_stderr("cp: target '{}': NtCreateFile returned {}\n", destdir, status);
        return 1;
    }

    auto d = (dir*)malloc(offsetof(dir, destname) + (destdir.size() * sizeof(char16_t)));
    if (!d) {
        print_stderr("malloc failed\n");
        return 1;
    }

    d->src = peb->ProcessParameters->CurrentDirectory.Handle;
    d->dest = destdirh.get();
    d->handle_refcount = 1;
    d->alloc_refcount = 1;
    d->destname_len = destdir.size();
    memcpy(d->destname, destdir.data(), d->destname_len * sizeof(char16_t));

    for (auto src : srcs) {
        auto sf = (src_file*)malloc(offsetof(src_file, name) + (src.size() * sizeof(char16_t)));
        if (!sf) {
            print_stderr("malloc failed\n");
            return 1;
        }

        d->handle_refcount++;
        d->alloc_refcount++;

        sf->destdir = d;
        sf->fn = fn;
        sf->name_len = src.size();
        memcpy(sf->name, src.data(), src.size() * sizeof(char16_t));
        insert_tail_list(to_copy, sf->le);
    }

    for (auto le = to_copy.next; le != &to_copy; le = le->next) {
        auto& sf = containing_record<src_file>(le);

        auto src = u16string_view(sf.name, sf.name_len);

        auto destname = u16string_view(sf.destdir->destname, sf.destdir->destname_len);

        status = cp_recursive(src, sf.destdir->src, sf.destdir->dest,
                              sf.fn, destname, to_copy, recursive, "cp");

        sf.destdir->handle_refcount--;
        if (sf.destdir->handle_refcount == 0) {
            NtClose(sf.destdir->src);
            NtClose(sf.destdir->dest);
        }

        if (!recursive && status == nt_status::FILE_IS_A_DIRECTORY) { // ignore
            auto le2 = le->next;

            sf.destdir->alloc_refcount--;
            if (sf.destdir->alloc_refcount == 0)
                free(sf.destdir);

            remove_entry_list(sf.le);
            free(&sf);

            le = le2->prev;
            status = nt_status::SUCCESS;
        } else if (!success(status)) {
            // free remaining items
            while (le->next != &to_copy) {
                auto& sf2 = containing_record<src_file>(le->next);

                sf2.destdir->handle_refcount--;
                if (sf2.destdir->handle_refcount == 0) {
                    NtClose(sf2.destdir->src);
                    NtClose(sf2.destdir->dest);
                }

                remove_entry_list(sf2.le);
                free(&sf2);
            }

            break;
        }
    }

    while (!is_list_empty(to_copy)) {
        auto& sf = containing_record<src_file>(to_copy.prev);

        if (!success(status)) {
            auto destdir = u16string_view(sf.destdir->destname,
                                        sf.destdir->destname_len);
            auto fn = u16string_view(sf.name, sf.name_len);

            delete_file(destdir, fn, "cp");
        }

        sf.destdir->alloc_refcount--;
        if (sf.destdir->alloc_refcount == 0)
            free(sf.destdir);

        remove_entry_list(sf.le);
        free(&sf);
    }

    free(d);

    return 0;
}

int main(int argc, char** argv) {
    static const option options[] = {
        { "recursive", no_argument, nullptr, 'r' },
        { "", no_argument, nullptr, 'R' },
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "rR", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 'r':
            case 'R':
                recursive = true;
                break;

            case '?':
                return 1;
        }
    }

    if (argc == optind) {
        print_stderr("cp: missing file operand\n");
        return 1;
    }

    if (argc == optind + 1) {
        print_stderr("cp: missing destination file operand after '{}'\n", argv[0]);
        return 1;
    }

    auto arr = (u16string_view*)malloc((unsigned int)(argc - optind) * sizeof(u16string_view));
    if (!arr) {
        print_stderr("malloc failed\n");
        return 1;
    }

    for (int i = optind; i < argc; i++) {
        auto sv = string_view(argv[i]);

        auto u16len = utf8_to_utf16_len(sv);
        auto u16src = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16src) {
            print_stderr("malloc failed\n");

            for (int j = optind; j < i; j++) {
                free((void*)arr[j - optind].data());
            }
            free(arr);

            return 1;
        }

        utf8_to_utf16(span(u16src, u16len), sv);

        arr[i - optind] = u16string_view(u16src, u16len);
    }

    auto ret = cp(span(arr, argc - optind - 1), arr[argc - optind - 1]);

    for (int i = 0; i < argc - optind; i++) {
        free((void*)arr[i].data());
    }
    free(arr);

    return ret;
}
