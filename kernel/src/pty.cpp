module;

#include <span>
#include <mutex>
#include <string_view>
#include <array>
#include <stdint.h>
#include <string.h>

export module pty;

import nt;
import print;
import chardev;
import handle;
import object_ref;
import moxnox_ioctls;
import pipe;
import spinlock;
import scheduler;
import thread;
import objects;
import termios;
import rwsem;

using namespace std;

class ptyctl : public chardev {
public:
    ptyctl() { }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

class pty_client : public chardev {
public:
    pty_client(pipe& recv_pipe, pipe& send_pipe);
    ~pty_client();
    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    sync_object& get_sync_object() override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;

    void server_disconnected();
    nt_status canonical_write(span<const uint8_t> s, uint64_t& written_bytes);

    pipe& recv_pipe;
    pipe& send_pipe;
    bool disconnected = false;
    termios settings;

    rwsem input_buffer_lock;
    array<char, 4096> input_buffer;
    size_t input_buffer_valid = 0;

private:
    nt_status tcgetattr(span<uint8_t> outbuf, uintptr_t& written);
    nt_status tcsetattr(span<const uint8_t> inbuf, bool flush);
    nt_status tcflush(span<const uint8_t> inbuf);
    size_t process_canonical_input(span<const uint8_t> new_data, uint8_t* echo_buf,
                                   bool& found_eof);
};

class pty_server : public chardev {
public:
    pty_server(pty_client& client);
    ~pty_server();
    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    sync_object& get_sync_object() override;
    void last_handle_closed() override;

    pty_client& client;
};

export bool create_dev_ptyctl() {
    auto pptr = new ptyctl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "ptyctl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

static nt_status create_pty(span<uint8_t> outbuf, uintptr_t& written) {
    auto& t = *get_current_thread();
    nt_status status;

    if (outbuf.size() < sizeof(MOXNOX_IOCTL_PTYCTL_CREATE_PTY))
        return nt_status::INVALID_PARAMETER;

    auto& out = *(MOXNOX_IOCTL_PTYCTL_CREATE_PTY*)outbuf.data();

    auto pptr_recv = new pipe(1);
    if (!pptr_recv) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj = object_ref{*pptr_recv};

    auto pptr_send = new pipe(1);
    if (!pptr_send) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj2 = object_ref{*pptr_send};

    auto pptr_client = new pty_client(*pptr_recv, *pptr_send);
    if (!pptr_client) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& settings = pptr_client->settings;

    memset(&settings, 0, sizeof(settings));

    settings.c_iflag = ICRNL | IXON;
    settings.c_oflag = OPOST | ONLCR;
    settings.c_cflag = B38400 | CS8 | CREAD | HUPCL;
    settings.c_lflag = ICANON | ECHO | ECHOE | ECHOK | ISIG | IEXTEN;

    settings.c_cc[VINTR] = 'C' - 0x40;
    settings.c_cc[VQUIT] = '\\' - 0x40;
    settings.c_cc[VERASE] = 0x7f;
    settings.c_cc[VKILL] = 'U' - 0x40;
    settings.c_cc[VEOF] = 'D' - 0x40;
    settings.c_cc[VSTART] = 'Q' - 0x40;
    settings.c_cc[VSTOP] = 'S' - 0x40;
    settings.c_cc[VSUSP] = 'Z' - 0x40;
    settings.c_cc[VREPRINT] = 'R' - 0x40;
    settings.c_cc[VDISCARD] = 'O' - 0x40;
    settings.c_cc[VWERASE] = 'W' - 0x40;
    settings.c_cc[VLNEXT] = 'V' - 0x40;
    settings.c_cc[VMIN] = 1;

    settings.c_ispeed = 38400;
    settings.c_ospeed = 38400;

    auto obj3 = object_ref{*pptr_client};

    auto pptr_server = new pty_server(*pptr_client);

    if (!pptr_server) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj4 = object_ref{*pptr_server};

    handle_t server_handle, client_handle;

    status = t.proc->add_handle(*pptr_server, FILE_READ_DATA | FILE_WRITE_DATA,
                                true, server_handle);
    if (!success(status))
        return status;

    status = t.proc->add_handle(*pptr_client, FILE_READ_DATA | FILE_WRITE_DATA,
                                true, client_handle);
    if (!success(status)) {
        t.proc->close_handle(server_handle);
        return status;
    }

    out.server_handle = server_handle;
    out.client_handle = client_handle;

    written = sizeof(MOXNOX_IOCTL_PTYCTL_CREATE_PTY);

    return nt_status::SUCCESS;
}

nt_status ptyctl::ioctl(uint32_t code, span<const uint8_t>,
                        span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_PTYCTL_CREATE_PTY:
            return create_pty(outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

pty_client::pty_client(pipe& recv_pipe, pipe& send_pipe) : recv_pipe(recv_pipe), send_pipe(send_pipe) {
    recv_pipe.acquire();
    send_pipe.acquire();
}

pty_client::~pty_client() {
    recv_pipe.release();
    send_pipe.release();
}

nt_status pty_client::read(span<uint8_t> s, uint64_t& read_bytes) {
    auto ret = recv_pipe.read(s, read_bytes);

    if (disconnected && read_bytes == 0) {
        lock_guard l(recv_pipe.lock);

        if (recv_pipe.buf_unread == 0)
            return nt_status::PIPE_DISCONNECTED;
    }

    if (success(ret) && settings.c_iflag & ICRNL) {
        for (uint64_t i = 0; i < read_bytes; i++) {
            if (s[i] == '\r')
                s[i] = '\n';
        }
    }

    return ret;
}

static size_t find_newline(span<const uint8_t> s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\n')
            return i;
    }

    return string_view::npos;
}

nt_status pty_client::write(span<const uint8_t> s, uint64_t& written_bytes) {
    nt_status status;

    if (disconnected)
        return nt_status::PIPE_DISCONNECTED;

    // FIXME - other c_oflags?

    if (settings.c_oflag & OPOST && settings.c_oflag & ONLCR) {
        uint64_t w = 0;

        static const array<uint8_t, 2> crlf = { '\r', '\n' };

        while (true) {
            auto nl = find_newline(s);

            if (nl == string_view::npos) {
                uint64_t sent;

                status = send_pipe.write(s, sent);

                if (disconnected)
                    return nt_status::PIPE_DISCONNECTED;

                if (!success(status))
                    return status;

                w += sent;
                written_bytes = w;
                return nt_status::SUCCESS;
            }

            if (nl != 0) {
                uint64_t sent;

                status = send_pipe.write(span(s.data(), nl), sent);

                if (disconnected)
                    return nt_status::PIPE_DISCONNECTED;

                if (!success(status)) {
                    if (w == 0)
                        return status;

                    written_bytes = w;
                    return nt_status::SUCCESS;
                }

                w += sent;
                s = span(s.data() + sent, s.size() - sent);

                if (sent < nl)
                    continue;
            }

            if (s.empty()) {
                written_bytes = w;
                return nt_status::SUCCESS;
            }

            // send CRLF

            {
                uint64_t sent;

                status = send_pipe.write(crlf, sent);

                if (disconnected)
                    return nt_status::PIPE_DISCONNECTED;

                if (!success(status)) {
                    if (w == 0)
                        return status;

                    written_bytes = w;
                    return nt_status::SUCCESS;
                }

                if (sent == 1) { // only sent CR
                    status = send_pipe.write(span(crlf.data() + 1, 1), sent);

                    if (disconnected)
                        return nt_status::PIPE_DISCONNECTED;

                    if (!success(status)) {
                        if (w == 0)
                            return status;

                        written_bytes = w;
                        return nt_status::SUCCESS;
                    }
                }
            }

            s = span(s.data() + 1, s.size() - 1);
            w++;
        }
    } else {
        status = send_pipe.write(s, written_bytes);

        if (disconnected)
            return nt_status::PIPE_DISCONNECTED;

        return status;
    }
}

sync_object& pty_client::get_sync_object() {
    return recv_pipe;
}

void pty_client::server_disconnected() {
    sync_lock sl;
    lock_guard l(sl);

    disconnected = true;

    send_pipe.readonly = true;

    if (!send_pipe.not_full_event.signalled)
        send_pipe.not_full_event.set_holding_sync_lock();

    if (!recv_pipe.signalled) {
        recv_pipe.signalled = true;

        wake_up_waiters(recv_pipe.waiters);
    }
}

pty_server::pty_server(pty_client& client) : client(client) {
    client.acquire();
}

pty_server::~pty_server() {
    client.release();
}

nt_status pty_server::read(span<uint8_t> s, uint64_t& read_bytes) {
    return client.send_pipe.read(s, read_bytes);
}

size_t pty_client::process_canonical_input(span<const uint8_t> new_data,
                                           uint8_t* echo_buf, bool& found_eof) {
    bool echoe = settings.c_lflag & ECHOE;
    auto bs = settings.c_cc[VERASE];
    auto eof = settings.c_cc[VEOF];
    auto* ptr = echo_buf;

    for (size_t i = 0; i < new_data.size(); i++) {
        if (new_data[i] == bs) {
            if (input_buffer_valid > 0) {
                input_buffer_valid--;

                while (input_buffer_valid > 0 && ((uint8_t)input_buffer[input_buffer_valid] & 0xc0) == 0x80) {
                    input_buffer_valid--;
                }

                if (ptr) {
                    if (echoe) {
                        *ptr = '\b';
                        ptr++;
                        *ptr = ' ';
                        ptr++;
                        *ptr = '\b';
                        ptr++;
                    } else {
                        *ptr = new_data[i];
                        ptr++;
                    }
                }
            }
        } else if (new_data[i] == eof) {
            found_eof = true;
            break;
        } else {
            if (input_buffer_valid < input_buffer.size()) {
                input_buffer[input_buffer_valid] = new_data[i];
                input_buffer_valid++;
            }

            if (ptr) {
                *ptr = new_data[i];
                ptr++;
            }
        }
    }

    return echo_buf ? (ptr - echo_buf) : 0;
}

nt_status pty_client::canonical_write(span<const uint8_t> s, uint64_t& written_bytes) {
    nt_status status;
    size_t orig_size = s.size(), bytes;
    bool echo = settings.c_lflag & ECHO;
    bool echonl = settings.c_lflag & ECHONL;
    bool onlcr = settings.c_oflag & OPOST && settings.c_oflag & ONLCR;

    // FIXME - Ctrl+D

    auto do_echo = [this](span<const uint8_t> sp) -> nt_status {
        size_t bytes;

        if (!sp.empty()) {
            while (true) {
                auto status = send_pipe.write(sp, bytes);
                if (!success(status))
                    return status;

                if (bytes == sp.size())
                    break;

                sp = span(sp.data() + bytes, sp.size() - bytes);
            }
        }

        return nt_status::SUCCESS;
    };

    {
        lock_guard lock(input_buffer_lock);

        auto do_line = [this](span<const uint8_t> sp) -> nt_status {
            nt_status status;
            size_t bytes;

            if (sp.empty()) {
                recv_pipe.set_eof_pending();
                return nt_status::SUCCESS;
            }

            while (true) {
                status = recv_pipe.write(sp, bytes);
                if (!success(status))
                    return status;

                if (bytes == sp.size())
                    break;

                sp = span(sp.data() + bytes, sp.size() - bytes);
            }

            return nt_status::SUCCESS;
        };

        while (!s.empty()) {
            uint8_t* echobuf = nullptr;
            auto nl = string_view((char*)s.data(), s.size()).find('\r');
            size_t linelen = nl == string_view::npos ? s.size() : (nl + 1);
            bool found_eof = false;

            if (echo) {
                echobuf = (uint8_t*)malloc(3 * linelen);
                if (!echobuf) {
                    print("malloc failed\n");
                    return nt_status::INSUFFICIENT_RESOURCES;
                }
            }

            auto echosize = process_canonical_input(span(s.data(), linelen),
                                                    echobuf, found_eof);

            if (echo) {
                status = do_echo(span(echobuf, echosize));
                if (!success(status)) {
                    free(echobuf);
                    return status;
                }

                free(echobuf);
            }

            if (nl == string_view::npos && !found_eof)
                break;

            status = do_line(span((uint8_t*)input_buffer.data(), input_buffer_valid));
            if (!success(status))
                return status;

            input_buffer_valid = 0;

            if (found_eof)
                break;

            if (!echo && echonl) {
                status = send_pipe.write("\r", bytes);
                if (!success(status))
                    return status;
            }

            if ((echo || echonl) && onlcr) {
                status = send_pipe.write("\n", bytes);
                if (!success(status))
                    return status;
            }

            s = span(s.data() + nl + 1, s.size() - nl - 1);
        }
    }

    written_bytes = orig_size; // discard excess data rather than doing a partial write

    return nt_status::SUCCESS;
}

nt_status pty_server::write(span<const uint8_t> s, uint64_t& written_bytes) {
    if (client.settings.c_lflag & ICANON) // FIXME - do we need a spinlock for this?
        return client.canonical_write(s, written_bytes);
    else
        return client.recv_pipe.write(s, written_bytes);
}

sync_object& pty_server::get_sync_object() {
    return client.send_pipe;
}

void pty_server::last_handle_closed() {
    client.server_disconnected();

    remove_object_name(client);
}

nt_status pty_client::ioctl(uint32_t code, span<const uint8_t> inbuf,
                            span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_PTY_ISATTY:
            written = 0;
            return nt_status::SUCCESS;

        case IOCTL_MOXNOX_PTY_TCGETATTR:
            return tcgetattr(outbuf, written);

        case IOCTL_MOXNOX_PTY_TCSETATTR:
            return tcsetattr(inbuf, false);

        case IOCTL_MOXNOX_PTY_TCSETATTR_FLUSH:
            return tcsetattr(inbuf, true);

        case IOCTL_MOXNOX_PTY_TCFLUSH:
            return tcflush(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status pty_client::tcgetattr(span<uint8_t> outbuf, uintptr_t& written) {
    if (outbuf.size() < sizeof(termios))
        return nt_status::BUFFER_TOO_SMALL;

    memcpy(outbuf.data(), &settings, sizeof(termios));
    written = sizeof(termios);

    return nt_status::SUCCESS;
}

nt_status pty_client::tcsetattr(span<const uint8_t> inbuf, bool flush) {
    if (inbuf.size() < sizeof(termios))
        return nt_status::BUFFER_TOO_SMALL;

    if (flush) {
        {
            lock_guard lock(input_buffer_lock);
            input_buffer_valid = 0;
        }

        recv_pipe.clear();
    }

    memcpy(&settings, inbuf.data(), sizeof(termios));

    return nt_status::SUCCESS;
}

nt_status pty_client::tcflush(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(int))
        return nt_status::BUFFER_TOO_SMALL;

    auto queue_selector = *(int*)inbuf.data();

    switch (queue_selector) {
        case TCIFLUSH:
        case TCOFLUSH:
        case TCIOFLUSH:
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (queue_selector == TCIFLUSH || queue_selector == TCIOFLUSH) {
        {
            lock_guard lock(input_buffer_lock);
            input_buffer_valid = 0;
        }

        recv_pipe.clear();
    }

    if (queue_selector == TCOFLUSH || queue_selector == TCIOFLUSH)
        send_pipe.clear();

    return nt_status::SUCCESS;
}
