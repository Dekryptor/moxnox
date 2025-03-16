#include "kernel.h"
#include "objects.h"
#include "handle.h"
#include <console.h>
#include <ioctl.h>

import print;
import inout;
import config;

using namespace std;

static const uint8_t serial_interrupt = 0x33;
static const uint16_t com1_port = 0x3f8;
static const unsigned int serial_buffer_pages = 1;
static bool init_serial_done = false;

class serial_in_pipe : public pipe {
public:
    serial_in_pipe(unsigned int buffer_pages) : pipe(buffer_pages) { }

    ~serial_in_pipe() {
        lock_guard lg(fg_proc_lock);

        if (fg_proc)
            fg_proc->get().release();
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    NTSTATUS console_set_foreground(span<const uint8_t> inbuf);

private:
    void handle_ctrl_c();

    uint32_t input_mode = ENABLE_PROCESSED_INPUT;
    spinlock fg_proc_lock;
    optional<reference_wrapper<process>> fg_proc;
};

class serial_out_pipe : public pipe {
public:
    serial_out_pipe(unsigned int buffer_pages, uint16_t port) : pipe(buffer_pages), port(port) { }
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    nt_status flush(bool go_readonly) override;

    bool ready = true;
    uint16_t port;

private:
    size_t do_write(span<const uint8_t> s);
    uint32_t output_mode = ENABLE_PROCESSED_OUTPUT;
};

class serial_duplex_pipe : public pipe {
public:
    serial_duplex_pipe(serial_in_pipe& recv_pipe, serial_out_pipe& send_pipe) : pipe(0), recv_pipe(recv_pipe), send_pipe(send_pipe) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~serial_duplex_pipe() {
        recv_pipe.release();
        send_pipe.release();
    }

    NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes) override {
        return recv_pipe.read(s, read_bytes);
    }

    nt_status write(std::span<const uint8_t> s, uint64_t& written_bytes) override {
        return send_pipe.write(s, written_bytes);
    }

    NTSTATUS ioctl(uint32_t code, span<const uint8_t> inbuf, span<uint8_t> outbuf, uintptr_t& written) override;

    serial_in_pipe& recv_pipe;
    serial_out_pipe& send_pipe;
};

static serial_out_pipe* com1_pipe = nullptr;

object_ref com1_pipe_handle;
static object_ref com1_send_handle, com1_recv_handle;

static object_ref create_serial_in_pipe(unsigned int buffer_pages) {
    auto pptr = new serial_in_pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

static object_ref create_serial_out_pipe(unsigned int buffer_pages, uint16_t port) {
    auto pptr = new serial_out_pipe(buffer_pages, port);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    add_to_flushable_pipes_list(*pptr);

    return p;
}

bool init_serial() {
    if (init_serial_done)
        return true;

    init_serial_done = true;

    // FIXME - COM2, COM3, COM4

    if (!init_serial_port(com1_port))
        return true;

    com1_send_handle = create_serial_out_pipe(serial_buffer_pages, com1_port);
    if (!com1_send_handle) {
        print("create_serial_out_pipe failed\n");
        return false;
    }

    com1_recv_handle = create_serial_in_pipe(serial_buffer_pages);
    if (!com1_recv_handle) {
        print("create_serial_in_pipe failed\n");
        return false;
    }

    com1_pipe = &static_cast<serial_out_pipe&>(com1_send_handle.get());

    auto& com1_recv_pipe = static_cast<serial_in_pipe&>(com1_recv_handle.get());

    auto pptr = new serial_duplex_pipe(com1_recv_pipe, *com1_pipe);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    com1_pipe_handle = object_ref{*pptr};

    if (!name_object(com1_pipe_handle.get(), "ttyS0")) {
        print("name_object failed\n");
        return false;
    }

    add_idt_entry(serial_interrupt, (void*)serial_isr, false, 0, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 4 = COM1 and COM3
    add_irq_handler(4, serial_interrupt, local_apic_id);

    return true;
}

size_t serial_out_pipe::do_write(span<const uint8_t> s) {
    auto size = s.size();

    if (s.empty())
        return 0;

    lock_guard l(lock);

    if (ready) {
        // send first character
        __asm __volatile (
            "out dx, al\n"
            :
            : "d" (port), "a" (s[0])
        );

        ready = false;
        s = s.subspan(1);

        if (s.empty())
            return size;
    }

    size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;
    size_t to_copy = min(s.size(), buf_left);

    if (to_copy > 0) {
        if (buf_write_pos + to_copy > buf_pages << PAGE_SHIFT) {
            auto first_bit = (buf_pages << PAGE_SHIFT) - buf_write_pos;

            memcpy(&buf[buf_write_pos], s.data(), first_bit);

            buf_write_pos = 0;
            buf_unread += first_bit;
            to_copy -= first_bit;
            s = s.subspan(first_bit);
        }

        memcpy(&buf[buf_write_pos], s.data(), to_copy);
        buf_write_pos += to_copy;
        buf_unread += to_copy;
        s = s.subspan(to_copy);
    }

    return size;
}

nt_status serial_out_pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    if (readonly) {
        written_bytes = 0;
        return nt_status::MEDIA_WRITE_PROTECTED;
    }

    if (!(output_mode & ENABLE_PROCESSED_OUTPUT)) {
        written_bytes = do_write(s);
        return nt_status::SUCCESS;
    }

    int last_bs = -1;

    static const uint8_t SEND_BS[] = { '\b', ' ', '\b' };

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\b') {
            if (last_bs == -1)
                do_write(s.subspan(0, i));
            else
                do_write(s.subspan((size_t)(last_bs + 1), i - (size_t)(last_bs + 1)));

            do_write(SEND_BS);
            last_bs = (int)i;
        }
    }

    do_write(s.subspan((size_t)(last_bs + 1), s.size() - (size_t)(last_bs + 1)));

    written_bytes = s.size();

    return nt_status::SUCCESS;
}

nt_status serial_in_pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    if (!(input_mode & ENABLE_PROCESSED_INPUT))
        return pipe::write(s, written_bytes);

    // handle VT100 weirdness

    size_t tmp;
    int last_run_start = -1;
    nt_status status;

    written_bytes = 0;

    for (size_t i = 0; i < s.size(); i++) {
        auto c = s[i];

        if (c == '\r' || c == 0x7f || c == '\x03') {
            if (last_run_start == -1)
                status = pipe::write(s.subspan(0, (size_t)i), tmp);
            else
                status = pipe::write(s.subspan((size_t)(last_run_start + 1), i - (size_t)(last_run_start + 1)), tmp);

            if (!success(status))
                return status;

            written_bytes += tmp;

            if (c == '\r') {
                c = '\n';
                status = pipe::write(span(&c, 1), tmp);

                if (!success(status))
                    return status;

                written_bytes += tmp;
            } else if (c == 0x7f) {
                c = '\b';
                status = pipe::write(span(&c, 1), tmp);

                if (!success(status))
                    return status;

                written_bytes += tmp;
            } else if (c == '\x03')
                handle_ctrl_c();

            last_run_start = (int)i;
        }
    }

    status = pipe::write(s.subspan((size_t)(last_run_start + 1), s.size() - (size_t)(last_run_start + 1)), tmp);

    if (!success(status))
        return status;

    written_bytes += tmp;

    return nt_status::SUCCESS;
}

extern "C"
void serial_isr_func(uint8_t iir) {
    if (iir & 1) // not us
        return;

    if ((iir & 0x0e) == 0x2) { // transmitter holding register empty
        auto& pipe = *com1_pipe;

        lock_guard l(pipe.lock);

        if (pipe.buf_unread == 0) {
            pipe.ready = true;
            return;
        }

        size_t buf_read_pos = pipe.buf_write_pos + (pipe.buf_pages << PAGE_SHIFT) - pipe.buf_unread;

        while (buf_read_pos >= pipe.buf_pages << PAGE_SHIFT) {
            buf_read_pos -= pipe.buf_pages << PAGE_SHIFT;
        }

        __asm __volatile (
            "out dx, al\n"
            :
            : "d" (pipe.port), "a" (pipe.buf[buf_read_pos])
        );

        pipe.buf_unread--;
    } else if ((iir & 0xe) == 0x4 || (iir & 0xe) == 0xc) { // data available or character timeout
        uint8_t c;
        uint64_t written_bytes;

        __asm __volatile (
            "in al, dx\n"
            : "=a" (c)
            : "d" (com1_port)
        );

        auto& p = static_cast<serial_in_pipe&>(com1_recv_handle.get());

        p.write(span(&c, 1), written_bytes);
    }
}

NTSTATUS serial_in_pipe::console_set_foreground(span<const uint8_t> inbuf) {
    if (inbuf.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto h = *(HANDLE*)inbuf.data();

    auto obj = deref_handle2(h);
    if (!obj)
        return nt_status::INVALID_HANDLE;

    optional<reference_wrapper<process>> procrw;

    visit_obj(obj.get(), [&](auto&& o) {
        if constexpr (is_same_v<remove_reference_t<decltype(o)>, process>)
            procrw = o;
    });

    if (!procrw.has_value())
        return nt_status::INVALID_HANDLE;

    auto& proc = procrw->get();
    proc.acquire();

    {
        lock_guard lg(fg_proc_lock);

        if (fg_proc)
            fg_proc->get().release();

        fg_proc = proc;
    }

    return nt_status::SUCCESS;
}

NTSTATUS serial_duplex_pipe::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                   span<uint8_t>, uintptr_t&) {
    switch (code) {
        case IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND:
            return recv_pipe.console_set_foreground(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

void serial_in_pipe::handle_ctrl_c() {
    optional<reference_wrapper<process>> proc;

    {
        lock_guard lg(fg_proc_lock);

        if (!fg_proc)
            return;

        proc = fg_proc;
        proc->get().acquire();
    }

    proc->get().send_signal((uint64_t)nt_status::CONTROL_C_EXIT);

    proc->get().release();
}

nt_status serial_out_pipe::flush(bool go_readonly) {
    if (go_readonly)
        readonly = true;

    lock_guard l(lock);

    while (true) {
        if (buf_unread == 0) {
            ready = true;
            return nt_status::SUCCESS;
        }

        size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

        while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
            buf_read_pos -= buf_pages << PAGE_SHIFT;
        }

        while (!(inb(port + 5) & 0x20)) { }

        outb(port, buf[buf_read_pos]);

        buf_unread--;
    }

    return nt_status::SUCCESS;
}
