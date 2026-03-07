#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string_view>
#include <bit>
#include <utility>
#include <span>
#include <tuple>

#undef errno
import errno;

import nt;
import ws2_32;
import print;
import ipv6;
import unique_handle;
import kernelbase;
import format;
import env;

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

static uint32_t socket_thread(void* ctx) {
    unique_handle sock{(HANDLE)(uintptr_t)ctx};
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    // FIXME - this isn't really telnet, telnet has an actual protocol

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = sock.get();
    si.hStdOutput = sock.get();
    si.hStdError = sock.get();

    auto home = get_env(u"HOME");

    if (!CreateProcessW(u"/bin/shell.exe", nullptr, nullptr, nullptr,
                        false, 0, nullptr, !home.empty() ? home.data() : nullptr,
                        &si, &pi)) {
        print_to_handle(sock.get(), "CreateProcessW failed (last error {})\n", GetLastError());
        return 1;
    }

    NtClose(pi.hProcess);
    NtClose(pi.hThread);

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
