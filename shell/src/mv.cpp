#include <string_view>
#include <span>
#include <utility>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;
import ntdll;
import print;
import unique_handle;
import copy;
import list;
import unicode;

using namespace std;

static int cross_device_move(u16string_view src, u16string_view destdir,
                             u16string_view fn) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    unique_handle destdirh;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    list_entry to_copy = { &to_copy, &to_copy };

    us.Length = us.MaximumLength = (uint16_t)(destdir.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)destdir.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mv: target '{}': No such file or directory\n", destdir);
        return 1;
    } else if (!success(status)) {
        print_stderr("mv: target '{}': NtCreateFile returned {}\n", destdir,
                     status);
        return 1;
    }

    destdirh.reset(h);

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

    for (auto le = to_copy.next; le != &to_copy; le = le->next) {
        auto& sf = containing_record<src_file>(le);

        auto src = u16string_view(sf.name, sf.name_len);

        auto destname = u16string_view(sf.destdir->destname, sf.destdir->destname_len);

        status = cp_recursive(src, sf.destdir->src, sf.destdir->dest,
                              sf.fn, destname, to_copy, true, "mv");

        sf.destdir->handle_refcount--;
        if (sf.destdir->handle_refcount == 0)
            NtClose(sf.destdir->dest);

        if (!success(status)) {
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

        if (success(status)) {
            delete_file_by_handle(sf.destdir->src,
                                  u16string_view(sf.name, sf.name_len), "mv");
        } else {
            auto destdir = u16string_view(sf.destdir->destname,
                                          sf.destdir->destname_len);
            auto fn = u16string_view(sf.name, sf.name_len);

            delete_file(destdir, fn, "mv");
        }

        sf.destdir->alloc_refcount--;
        if (sf.destdir->alloc_refcount == 0) {
            NtClose(sf.destdir->src);
            free(sf.destdir);
        }

        remove_entry_list(sf.le);
        free(&sf);
    }

    free(d);

    return 0;
}

static int mv(u16string_view src, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle file, desth;

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
        desth.reset(h);
    else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mv: cannot move '{}' to '{}': No such file or directory\n",
                     src, dest);
        return 1;
    } else if (status != nt_status::OBJECT_NAME_NOT_FOUND && status != nt_status::NOT_A_DIRECTORY) {
        print_stderr("mv: '{}': NtCreateFile returned {}\n", dest, status);
        return 1;
    }

    // open src

    us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)src.data();

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mv: '{}': No such file or directory\n", src);
        return 1;
    } else if (!success(status)) {
        print_stderr("mv: '{}': NtCreateFile returned {}\n", src, status);
        return 1;
    }

    file.reset(h);

    // try rename

    u16string_view fn;

    if (desth) {
        if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
        else
            fn = src;

        oa.RootDirectory = desth.get();
    } else
        fn = dest;

    size_t frilen = offsetof(FILE_RENAME_INFORMATION, FileName) + (fn.size() * sizeof(char16_t));

    auto fri = (FILE_RENAME_INFORMATION*)malloc(frilen);
    if (!fri) {
        print_stderr("malloc failed\n");
        return 1;
    }

    fri->ReplaceIfExists = true; // FIXME? --interactive option
    fri->RootDirectory = desth ? desth.get() : peb->ProcessParameters->CurrentDirectory.Handle;
    fri->FileNameLength = (uint32_t)(fn.size() * sizeof(char16_t));
    memcpy(fri->FileName, fn.data(), fn.size() * sizeof(char16_t));

    status = NtSetInformationFile(file.get(), &iosb, fri, (uint32_t)frilen,
                                  FILE_INFORMATION_CLASS::RenameInformation);
    if (status == nt_status::NOT_SAME_DEVICE) {
        u16string_view destdir;

        if (!desth) {
            if (auto slash = dest.find_last_of(u'/'); slash != u16string_view::npos) {
                destdir = u16string_view(dest.data(), slash);
                fn = u16string_view(dest.data() + slash + 1, dest.size() - slash - 1);
            } else {
                destdir = u".";
                fn = dest;
            }
        } else
            destdir = dest;

        return cross_device_move(src, destdir, fn);
    } else if (!success(status)) {
        print("mv: NtSetInformationFile returned {}\n", status);
        return 1;
    }

    return 0;
}

static int mv_multiple(span<u16string_view> srcs, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle desth;

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
        print_stderr("mv: target '{}': Not a directory\n", dest);
        return 1;
    } else if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
               status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mv: target '{}': No such file or directory\n", dest);
        return 1;
    } else if (!success(status)) {
        print_stderr("mv: target '{}': NtCreateFile returned {}\n", dest, status);
        return 1;
    }

    desth.reset(h);

    for (auto src : srcs)  {
        unique_handle file;

        // open src

        oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

        us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
        us.Buffer = (char16_t*)src.data();

        status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                              0, nullptr, 0);
        if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
            status == nt_status::OBJECT_PATH_NOT_FOUND) {
            print_stderr("mv: '{}': No such file or directory\n", src);
            continue;
        } else if (!success(status)) {
            print_stderr("mv: '{}': NtCreateFile returned {}\n", src, status);
            continue;
        }

        file.reset(h);

        // rename

        u16string_view fn;

        if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
        else
            fn = src;

        oa.RootDirectory = desth.get();

        size_t frilen = offsetof(FILE_RENAME_INFORMATION, FileName) + (fn.size() * sizeof(char16_t));

        auto fri = (FILE_RENAME_INFORMATION*)malloc(frilen);
        if (!fri) {
            print_stderr("malloc failed\n");
            return 1;
        }

        fri->ReplaceIfExists = true; // FIXME? --interactive option
        fri->RootDirectory = desth.get();
        fri->FileNameLength = (uint32_t)(fn.size() * sizeof(char16_t));
        memcpy(fri->FileName, fn.data(), fn.size() * sizeof(char16_t));

        // FIXME - emulate if we get nt_status::NOT_SAME_DEVICE
        status = NtSetInformationFile(file.get(), &iosb, fri, (uint32_t)frilen,
                                      FILE_INFORMATION_CLASS::RenameInformation);
        if (!success(status)) {
            print("mv: NtSetInformationFile returned {}\n", status);
            continue;
        }
    }

    // FIXME - what error should we return if some moves fail?

    return 0;
}

int main(int argc, char** argv) {
    int ret;

    if (argc < 2) {
        print_stderr("mv: missing operand\n");
        return 1;
    }

    if (argc < 3) {
        print_stderr("mv: missing destination file operand after '{}'\n", argv[1]);
        return 1;
    }

    auto arr = (u16string_view*)malloc((argc - 1) * sizeof(u16string_view));
    if (!arr) {
        print_stderr("malloc failed\n");
        return 1;
    }

    for (int i = 0; i < argc - 1; i++) {
        auto sv = string_view(argv[i + 1]);

        auto u16len = utf8_to_utf16_len(sv);
        auto u16src = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16src) {
            print_stderr("malloc failed\n");

            for (int j = 0; j < i; j++) {
                free((void*)arr[j].data());
            }
            free(arr);

            return 1;
        }

        utf8_to_utf16(span(u16src, u16len), sv);

        arr[i] = u16string_view(u16src, u16len);
    }

    if (argc > 3)
        ret = mv_multiple(span(&arr[0], argc - 2), arr[argc - 2]);
    else
        ret = mv(arr[0], arr[1]);

    for (int i = 0; i < argc - 1; i++) {
        free((void*)arr[i].data());
    }
    free(arr);

    return ret;
}
