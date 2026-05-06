#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string_view>
#include <bit>
#include <utility>
#include <span>
#include <tuple>
#include <optional>

#undef errno
import errno;

import nt;
import ws2_32;
import print;
import ipv6;
import unique_handle;
import kernelbase;
import format;
import unicode;
import moxnox_ioctls;

using namespace std;

static void print_to_handle2(HANDLE h, string_view msg) {
    auto msg2 = span(msg.data(), msg.size());

    while (true) {
        nt_status status;
        IO_STATUS_BLOCK iosb;

        status = NtWriteFile((HANDLE)h, 0, nullptr, nullptr, &iosb, msg2.data(),
                                (uint32_t)msg2.size(), nullptr, nullptr);
        if (status == nt_status::END_OF_FILE)
            return;
        else if (!success(status)) {
            print_stderr("NtWriteFile returned {}\n", status);
            return;
        }

        if (iosb.Information == msg2.size())
            break;

        msg2 = span(msg2.data() + iosb.Information,
                    msg2.size() - iosb.Information);
    }
}

template<typename... Args>
void print_to_handle(HANDLE h, type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];

    auto sp = span(buf, sizeof(buf));

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    print_to_handle2(h, sv);
}

static optional<pair<unique_handle, unique_handle>> create_pty(HANDLE out) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    nt_status status;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY ptyinfo;
    unique_handle ptyctl;

    static const u16string_view ptyctl_fn = u"/dev/ptyctl";

    // open ptyctl

    us.Length = us.MaximumLength = (uint16_t)(ptyctl_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)ptyctl_fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print_to_handle(out, "NtOpenFile returned {} for /dev/ptyctl\n", status);
        return nullopt;
    }

    ptyctl.reset(h);

    // create pty

    status = NtDeviceIoControlFile(ptyctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTYCTL_CREATE_PTY, nullptr,
                                   0, &ptyinfo, sizeof(ptyinfo));
    if (!success(status)) {
        print_to_handle(out, "IOCTL_MOXNOX_PTYCTL_CREATE_PTY returned {}\n", status);
        return nullopt;
    }

    ptyctl.reset();

    return make_pair(unique_handle{ptyinfo.client_handle}, unique_handle{ptyinfo.server_handle});
}

static uint32_t socket_thread(void* ctx) {
    unique_handle sock{(HANDLE)(uintptr_t)ctx};
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    char16_t* u16home = nullptr;

    // FIXME - this isn't really telnet, telnet has an actual protocol

    auto ptyinfo = create_pty(sock.get());
    if (!ptyinfo.has_value())
        return 1;

    auto& client_handle = ptyinfo->first;
    auto& server_handle = ptyinfo->second;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = client_handle.get();
    si.hStdOutput = client_handle.get();
    si.hStdError = client_handle.get();

    auto home = getenv("HOME");
    if (home && home[0]) {
        auto len = utf8_to_utf16_len(home);

        u16home = (char16_t*)malloc((len + 1) * sizeof(char16_t));
        if (!u16home) {
            print_stderr("malloc failed\n");
            return 1;
        }

        utf8_to_utf16(span(u16home, len), home);
        u16home[len] = 0;
    }

    if (!CreateProcessW(u"/bin/shell.exe", nullptr, nullptr, nullptr,
                        false, 0, nullptr, u16home, &si, &pi)) {
        print_to_handle(sock.get(), "CreateProcessW failed (last error {})\n", GetLastError());

        if (u16home)
            free(u16home);

        return 1;
    }

    if (u16home)
        free(u16home);

    NtClose(pi.hThread);

    unique_handle proc{pi.hProcess};

    bool program_running = true;

    while (program_running) {
        array<HANDLE, 3> objs;
        nt_status status;
        IO_STATUS_BLOCK iosb;
        char s[4096];

        objs[0] = server_handle.get();
        objs[1] = proc.get();
        objs[2] = sock.get();

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          false, nullptr);
        if (!success(status)) {
            print_to_handle(sock.get(), "NtWaitForMultipleObjects returned {}\n", status);
            break;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: { // program wrote to stdout or stderr, forward to socket
                status = NtReadFile(server_handle.get(), 0, nullptr, nullptr, &iosb,
                                    s, sizeof(s), nullptr, nullptr);

                if (!success(status)) {
                    print_to_handle(sock.get(), "NtReadFile returned {}\n", status);
                    program_running = false;
                    break;
                }

                if (iosb.Information == 0)
                    continue;

                auto sp = span(s, iosb.Information);

                while (!sp.empty()) {
                    status = NtWriteFile(sock.get(), 0, nullptr, nullptr,
                                         &iosb, sp.data(), (uint32_t)sp.size(), nullptr, nullptr);
                    if (!success(status)) {
                        print_to_handle(sock.get(), "NtReadFile returned {}\n", status);
                        program_running = false;
                        break;
                    }

                    sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
                }

                break;
            }

            case 1: // program finished
                program_running = false;
                break;

            case 2: { // received data from socket, forward to program stdin
                status = NtReadFile(sock.get(), 0, nullptr, nullptr, &iosb,
                                    s, sizeof(s), nullptr, nullptr);

                if (!success(status)) {
                    print_to_handle(sock.get(), "NtReadFile returned {}\n", status);
                    program_running = false;
                    break;
                }

                if (iosb.Information == 0)
                    continue;

                auto sp = span(s, iosb.Information);

                while (!sp.empty()) {
                    status = NtWriteFile(server_handle.get(), 0, nullptr, nullptr,
                                         &iosb, sp.data(), (uint32_t)sp.size(), nullptr, nullptr);
                    if (!success(status)) {
                        print_to_handle(sock.get(), "NtReadFile returned {}\n", status);
                        program_running = false;
                        break;
                    }

                    sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
                }

                break;
            }
        }
    }

    return 0;
}

int main() {
    struct sockaddr_in6 addr;

    auto server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_sock < 0) {
        print_stderr("socket returned {} (errno {})\n", server_sock, errno_to_string_view(*_errno()));
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = byteswap((uint16_t)23);

    int ret = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret) {
        print_stderr("bind returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    ret = listen(server_sock, 5);
    if (ret) {
        print_stderr("listen returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    print("Telnet server listening on port 23...\n");

    while (true) {
        struct sockaddr_in6 client_addr;
        int client_addrlen = sizeof(client_addr);

        auto client_sock = accept(server_sock, (struct sockaddr*)&client_addr,
                                  &client_addrlen);
        if (client_sock < 0) {
            print_stderr("accept returned {} (errno {})\n", client_sock,
                         errno_to_string_view(*_errno()));
            closesocket(server_sock);
            return 1;
        }

        if (client_addrlen != sizeof(client_addr)) {
            print_stderr("client_addrlen was {}, expected {}\n", client_addrlen,
                         sizeof(client_addr));
            closesocket(client_sock);
            closesocket(server_sock);
            return 1;
        }

        print("Connected from [{}]:{}, socket {}\n", *(ipv6_address*)client_addr.sin6_addr,
              byteswap(client_addr.sin6_port), (uintptr_t)client_sock);

        if (!CreateThread(nullptr, 0, socket_thread, (void*)(uintptr_t)client_sock,
                          0, nullptr)) {
            print_stderr("CreateThread failed (last error {})\n", GetLastError());
            closesocket(client_sock);
        }
    }

    closesocket(server_sock);

    return 0;
}
