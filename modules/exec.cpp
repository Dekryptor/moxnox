module;

#include <string_view>
#include <span>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <spawn.h>

export module exec;

import nt;
import ntdll;
import unique_handle;
import unicode;
import pagesize;
import list;

#undef errno
import errno;

using namespace std;

export enum class exec_info_type {
    inherited_fd,
    open,
    close,
    dup2,
    chdir,
    fchdir,
};

export struct exec_info {
    enum exec_info_type type;

    union {
        struct {
            HANDLE h;
            int fd;
        } inherited_fd;
        struct {
            int fd;
            char* path;
            int oflag;
            mode_t mode;
        } open;
        struct {
            int fd;
        } close;
        struct {
            int fd;
            int newfd;
        } dup2;
        struct {
            char* path;
        } chdir;
        struct {
            int fd;
        } fchdir;
    };
};

export struct exec_info_list {
    list_entry le;
    exec_info ei;
};

export struct file_actions_list {
    list_entry list = { &list, &list };
};

static_assert(sizeof(file_actions_list) == sizeof(posix_spawn_file_actions_t));
static_assert(alignof(file_actions_list) == alignof(posix_spawn_file_actions_t));

// ucrtbase
extern "C" nt_status mn_prepare_exec_block(HANDLE proc, exec_info** exec_block,
                                           size_t* num_exec_block_entries);

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static bool arg_needs_quotes(string_view s) {
    if (s.empty())
        return true;

    for (auto c : s) {
        if (c == ' ' || c == '\t' || c == '"')
            return true;
    }

    return false;
}

static size_t get_cmdline_len(char* const* argv) {
    size_t ret = 0;

    for (size_t i = 0; argv[i]; i++) {
        if (i != 0)
            ret++; // separating space

        auto s = string_view(argv[i]);

        if (i == 0) {
            // program name: quoted only if it contains whitespace, no escaping
            bool quote = s.find(' ') != string_view::npos ||
                         s.find('\t') != string_view::npos;

            ret += utf8_to_utf16_len(s) + (quote ? 2 : 0);
            continue;
        }

        if (!arg_needs_quotes(s)) {
            ret += utf8_to_utf16_len(s);
            continue;
        }

        ret += 2; // surrounding quotes

        size_t j = 0;
        while (j < s.size()) {
            if (s[j] == '\\') {
                size_t nbs = 0;

                while (j < s.size() && s[j] == '\\') {
                    nbs++;
                    j++;
                }

                if (j == s.size())
                    ret += 2 * nbs; // trailing backslashes are doubled
                else if (s[j] == '"') {
                    ret += (2 * nbs) + 2; // doubled backslashes plus the escaped quote
                    j++;
                } else
                    ret += nbs; // literal backslashes
            } else if (s[j] == '"') {
                ret += 2; // escaped quote
                j++;
            } else {
                size_t start = j;

                while (j < s.size() && s[j] != '\\' && s[j] != '"') {
                    j++;
                }

                ret += utf8_to_utf16_len(string_view(s.data() + start, j - start));
            }
        }
    }

    return ret;
}

static void do_cmdline(char16_t* buf, char* const* argv) {
    auto out = buf;

    auto emit_utf16 = [&](string_view s) {
        auto len = utf8_to_utf16_len(s);

        utf8_to_utf16(span(out, len), s);
        out += len;
    };

    for (size_t i = 0; argv[i]; i++) {
        if (i != 0) {
            *out = u' ';
            out++;
        }

        auto s = string_view(argv[i]);

        if (i == 0) {
            bool quote = s.find(' ') != string_view::npos ||
                         s.find('\t') != string_view::npos;

            if (quote) {
                *out = u'"';
                out++;
            }

            emit_utf16(s);

            if (quote) {
                *out = u'"';
                out++;
            }

            continue;
        }

        if (!arg_needs_quotes(s)) {
            emit_utf16(s);
            continue;
        }

        *out = u'"';
        out++;

        size_t j = 0;
        while (j < s.size()) {
            if (s[j] == '\\') {
                size_t nbs = 0;

                while (j < s.size() && s[j] == '\\') {
                    nbs++;
                    j++;
                }

                size_t count;

                if (j == s.size())
                    count = 2 * nbs; // trailing backslashes doubled
                else if (s[j] == '"')
                    count = 2 * nbs + 1; // doubled, plus one to escape the quote
                else
                    count = nbs; // literal

                for (size_t k = 0; k < count; k++) {
                    *out = u'\\';
                    out++;
                }

                if (j < s.size() && s[j] == '"') {
                    *out = u'"';
                    out++;
                    j++;
                }
            } else if (s[j] == '"') {
                *out = u'\\';
                out++;
                *out = u'"';
                out++;
                j++;
            } else {
                size_t start = j;

                while (j < s.size() && s[j] != '\\' && s[j] != '"') {
                    j++;
                }

                emit_utf16(string_view(s.data() + start, j - start));
            }
        }

        *out = u'"';
        out++;
    }

    *out = 0;
}

static size_t get_environ_len(char* const* envp) {
    size_t ret = 0;

    for (size_t i = 0; envp[i]; i++) {
        ret += utf8_to_utf16_len(envp[i]) + 1;
    }

    ret++;

    return ret;
}

static void do_environ(span<char16_t> buf, char* const* envp) {
    for (size_t i = 0; envp[i]; i++) {
        auto sv = string_view(envp[i]);
        auto len = utf8_to_utf16_len(sv);

        utf8_to_utf16(buf, sv);

        buf = span(buf.data() + len, buf.size() - len);

        buf[0] = 0;
        buf = span(buf.data() + 1, buf.size() - 1);
    }

    buf[0] = 0;
}

export int do_exec(const char* path, char* const* argv, char* const* envp,
                   unique_handle& proc, unique_handle& thread,
                   CLIENT_ID& client_id, file_actions_list* file_actions) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle file, sect;
    PROCESS_BASIC_INFORMATION pbi;
    SECTION_IMAGE_INFORMATION sii;
    RTL_USER_PROCESS_PARAMETERS params;
    PEB newpeb;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    nt_status status;
    HANDLE stdinh, stdouth, stderrh, curdirh;
    uintptr_t our_ntdll, their_ntdll;
    ANSI_STRING as;
    void* our_mn_exec;
    uintptr_t their_mn_exec;
    exec_info* our_exec_block;
    size_t num_exec_block_entries;
    void* their_exec_block = nullptr;
    size_t exec_block_extra = 0;

    static const string_view mn_exec = "mn_exec";

    string_view pathsv = path;
    auto u16pathlen = utf8_to_utf16_len(pathsv);

    auto u16path = (char16_t*)malloc(u16pathlen * sizeof(char16_t));
    if (!u16path)
        return ENOMEM;

    utf8_to_utf16(span(u16path, u16pathlen), pathsv);

    auto u16pathsv = u16string_view(u16path, u16pathlen);

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(u16pathsv.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)u16pathsv.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);

    free(u16path);

    if (!success(status))
        return nt_status_to_errno(status);

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status))
        return nt_status_to_errno(status);

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);
    if (!success(status))
        return nt_status_to_errno(status);

    proc.reset(h);

    // duplicate standard handles (FIXME - these should actually be our fd 0, 1, and 2?)

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->StandardInput,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->StandardOutput,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->StandardError,
                               proc.get(), &stderrh, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->CurrentDirectory.Handle,
                               proc.get(), &curdirh, 0, 0,
                               DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status))
        return nt_status_to_errno(status);

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // do exec block

    status = mn_prepare_exec_block(proc.get(), &our_exec_block,
                                   &num_exec_block_entries);
    if (!success(status))
        return nt_status_to_errno(status);

    if (file_actions) {
        size_t new_entries = 0;

        for (auto le = file_actions->list.next; le != &file_actions->list; le = le->next) {
            auto& eil = containing_record<exec_info_list>(le);

            if (eil.ei.type == exec_info_type::open)
                exec_block_extra += strlen(eil.ei.open.path) + 1;
            else if (eil.ei.type == exec_info_type::chdir)
                exec_block_extra += strlen(eil.ei.chdir.path) + 1;

            new_entries++;
        }

        if (new_entries > 0) {
            our_exec_block = (exec_info*)realloc(our_exec_block, ((num_exec_block_entries + new_entries) * sizeof(exec_info)) + exec_block_extra);
            if (!our_exec_block)
                return ENOMEM;

            auto p = &our_exec_block[num_exec_block_entries];
            auto extra = (char*)&our_exec_block[num_exec_block_entries + new_entries];

            for (auto le = file_actions->list.next; le != &file_actions->list; le = le->next) {
                auto& eil = containing_record<exec_info_list>(le);

                memcpy(p, &eil.ei, sizeof(exec_info));

                if (eil.ei.type == exec_info_type::open) {
                    auto len = strlen(eil.ei.open.path);

                    memcpy(extra, eil.ei.open.path, len + 1);
                    p->open.path = extra;
                    extra += len + 1;
                } else if (eil.ei.type == exec_info_type::chdir) {
                    auto len = strlen(eil.ei.chdir.path);

                    memcpy(extra, eil.ei.chdir.path, len + 1);
                    p->chdir.path = extra;
                    extra += len + 1;
                }

                p++;
            }

            num_exec_block_entries += new_entries;
        }
    }

    if (num_exec_block_entries > 0) {
        size_t exec_block_size = bytes_to_pages((num_exec_block_entries * sizeof(exec_info)) + exec_block_extra) << PAGE_SHIFT;

        status = NtAllocateVirtualMemory(proc.get(), &their_exec_block, 0,
                                         &exec_block_size,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!success(status)) {
            free(our_exec_block);
            return nt_status_to_errno(status);
        }

        for (size_t i = 0; i < num_exec_block_entries; i++) {
            if (our_exec_block[i].type == exec_info_type::open)
                our_exec_block[i].open.path = our_exec_block[i].open.path - (char*)our_exec_block + (char*)their_exec_block;
            else if (our_exec_block[i].type == exec_info_type::chdir)
                our_exec_block[i].chdir.path = our_exec_block[i].chdir.path - (char*)our_exec_block + (char*)their_exec_block;
        }

        status = NtWriteVirtualMemory(proc.get(), their_exec_block, our_exec_block,
                                      (uint32_t)((num_exec_block_entries * sizeof(exec_info)) + exec_block_extra),
                                      nullptr);
        if (!success(status)) {
            free(our_exec_block);
            return nt_status_to_errno(status);
        }

        free(our_exec_block);
    } else
        their_exec_block = 0;

    // change handles and current directory in PEB

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stderrh;
    params.CurrentDirectory.Handle = curdirh;

    // FIXME - CurrentDirectory.DosPath?

    // allocate misc block

    auto cmdline_len = get_cmdline_len(argv);
    auto env_len = get_environ_len(envp);

    size_t misc_block_size = (cmdline_len + 1 + env_len) * sizeof(char16_t);
    uint8_t* misc_block;
    void* proc_misc_block = nullptr;

    misc_block = (uint8_t*)malloc(misc_block_size);
    if (!misc_block)
        return ENOMEM;

    // allocate misc block in process

    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                     &proc_misc_block_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        free(misc_block);
        return nt_status_to_errno(status);
    }

    // copy cmdline

    span sp = span((char16_t*)misc_block, misc_block_size / sizeof(char16_t));

    do_cmdline(sp.data(), argv);

    params.CommandLine.Length = (uint16_t)(cmdline_len * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((cmdline_len + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    sp = span(sp.data() + (params.CommandLine.MaximumLength / sizeof(char16_t)),
              sp.size() - (params.CommandLine.MaximumLength / sizeof(char16_t)));

    do_environ(sp, envp);

    params.Environment = (uint8_t*)proc_misc_block + params.CommandLine.MaximumLength;
    params.EnvironmentSize = env_len * sizeof(char16_t);

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        free(misc_block);
        return nt_status_to_errno(status);
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // find their ntdll

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxFindNtdll,
                                       &their_ntdll, sizeof(their_ntdll), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // find our ntdll

    status = NtQueryInformationProcess(NtCurrentProcess(), PROCESS_INFORMATION_CLASS::MoxnoxFindNtdll,
                                       &our_ntdll, sizeof(our_ntdll), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    as.Buffer = (char*)mn_exec.data();
    as.Length = as.MaximumLength = (uint16_t)mn_exec.size();

    status = LdrGetProcedureAddress((void*)our_ntdll, &as, 0, &our_mn_exec);
    if (!success(status))
        return nt_status_to_errno(status);

    their_mn_exec = (uintptr_t)our_mn_exec - our_ntdll + their_ntdll;

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000; // FIXME - should come from PE header?

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status))
        return nt_status_to_errno(status);

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = their_mn_exec;
    context.X0 = (uintptr_t)sii.TransferAddress;
    context.X1 = (uintptr_t)their_exec_block;
    context.X2 = num_exec_block_entries;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10;
#elifdef __x86_64__
    context.Rip = their_mn_exec;
    context.Rcx = (uintptr_t)sii.TransferAddress;
    context.Rdx = (uintptr_t)their_exec_block;
    context.R8 = num_exec_block_entries;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status))
        return nt_status_to_errno(status);

    thread.reset(h);

    return 0;
}

export int search_path(const char* file, invocable<const char*> auto func,
                       invocable<const char*> auto check_access) {
    auto path = getenv("PATH");
    auto env_path = string_view(path ? path : "");
    auto program = string_view(file);
    bool found_eacces = false;

    if (program.find('/') != string_view::npos)
        env_path = "";

    if (program.empty())
        return EINVAL;

    while (true) {
        auto col = env_path.find(':');
        string_view dir;

        if (col == string_view::npos)
            dir = env_path;
        else
            dir = string_view(env_path.data(), col);

        if (dir.empty() && program.front() != '/')
            dir = ".";

        for (unsigned int i = 0; i < 2; i++) {
            static const string_view suffix = ".exe";

            if (i == 1 && program.size() >= suffix.size() && string_view(program.data() + program.size() - suffix.size(), suffix.size()) == suffix)
                break;

            bool add_suffix = i == 1;

            auto exe = (char*)malloc(dir.size() + 1 + program.size() + (add_suffix ? suffix.size() : 0) + 1);
            if (!exe)
                return ENOMEM;

            memcpy(exe, dir.data(), dir.size());
            exe[dir.size()] = '/';
            memcpy(exe + dir.size() + 1, program.data(), program.size());

            if (add_suffix) {
                memcpy(exe + dir.size() + 1 + program.size(), suffix.data(),
                       suffix.size());
                exe[dir.size() + 1 + program.size() + suffix.size()] = 0;
            } else
                exe[dir.size() + 1 + program.size()] = 0;

            auto acc = check_access(exe);

            if (!acc) {
                auto ret = func(exe);

                free(exe);

                return ret;
            } else {
                free(exe);

                if (acc == EACCES)
                    found_eacces = true;
                else if (acc != ENOENT)
                    return acc;
            }
        }

        if (col == string_view::npos)
            break;

        env_path = string_view(env_path.data() + col + 1,
                               env_path.size() - col - 1);
    }

    return found_eacces ? EACCES : ENOENT;
}
