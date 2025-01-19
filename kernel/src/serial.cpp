#include "kernel.h"
#include "objects.h"
#include "handle.h"
#include <console.h>
#include <ioctl.h>

using namespace std;

static const uint8_t serial_interrupt = 0x23;
static const uint16_t com1_port = 0x3f8;
static const unsigned int serial_buffer_pages = 1;

class serial_out_pipe : public pipe {
public:
    serial_out_pipe(unsigned int buffer_pages, uint16_t port) : pipe(buffer_pages), port(port) { }
    size_t write(span<const uint8_t> s) override;

    bool ready = true;
    uint16_t port;

private:
    size_t do_write(span<const uint8_t> s);
    uint32_t output_mode = ENABLE_PROCESSED_OUTPUT;
};

class serial_duplex_pipe : public pipe {
public:
    serial_duplex_pipe(pipe& recv_pipe, pipe& send_pipe) : pipe(0), recv_pipe(recv_pipe), send_pipe(send_pipe) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~serial_duplex_pipe() {
        recv_pipe.release();
        send_pipe.release();

        {
            lock_guard lg(fg_proc_lock);

            if (fg_proc)
                fg_proc->get().release();
        }
    }

    NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes) override {
        return recv_pipe.read(s, read_bytes);
    }

    size_t write(std::span<const uint8_t> s) override {
        return send_pipe.write(s);
    }

    NTSTATUS ioctl(uint32_t code, span<const uint8_t> inbuf, span<uint8_t> outbuf, uintptr_t& written) override;

    pipe& recv_pipe;
    pipe& send_pipe;

private:
    NTSTATUS console_set_foreground(span<const uint8_t> inbuf);

    spinlock fg_proc_lock;
    optional<reference_wrapper<process>> fg_proc;
};

static serial_out_pipe* com1_pipe = nullptr;

object_ref com1_pipe_handle;
static object_ref com1_recv_handle, duplex_pipe;

static object_ref create_serial_out_pipe(unsigned int buffer_pages, uint16_t port) {
    auto pptr = new serial_out_pipe(buffer_pages, port);
    if (!pptr) {
        printk("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

bool init_serial() {
    // FIXME - COM2, COM3, COM4

    if (!init_serial_port(com1_port))
        return true;

    com1_pipe_handle = create_serial_out_pipe(serial_buffer_pages, com1_port);
    if (!com1_pipe_handle) {
        printk("create_serial_pipe failed\n");
        return false;
    }

    com1_recv_handle = create_pipe(serial_buffer_pages);
    if (!com1_recv_handle) {
        printk("create_pipe failed\n");
        return false;
    }

    com1_pipe = &static_cast<serial_out_pipe&>(com1_pipe_handle.get());

    auto& com1_recv_pipe = static_cast<pipe&>(com1_recv_handle.get());

    auto pptr = new serial_duplex_pipe(com1_recv_pipe, *com1_pipe);
    if (!pptr) {
        printk("new failed\n");
        return false;
    }

    duplex_pipe = object_ref{*pptr};

    if (!name_object(duplex_pipe.get(), "ttyS0")) {
        printk("name_object failed\n");
        return false;
    }

    add_idt_entry(serial_interrupt, (void*)serial_isr, false, 0);

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

size_t serial_out_pipe::write(span<const uint8_t> s) {
    if (!(output_mode & ENABLE_PROCESSED_OUTPUT))
        return do_write(s);

    int last_bs = -1;

    static const uint8_t SEND_BS[] = { '\b', ' ', '\b' };

    for (int i = 0; i < (int)s.size(); i++) {
        if (s[i] == '\b') {
            if (last_bs == -1)
                do_write(s.subspan(0, i));
            else
                do_write(s.subspan(last_bs + 1, i - last_bs - 1));

            do_write(SEND_BS);
            last_bs = i;
        }
    }

    do_write(s.subspan(last_bs + 1, s.size() - last_bs - 1));

    return s.size();
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

        __asm __volatile (
            "in al, dx\n"
            : "=a" (c)
            : "d" (com1_port)
        );

        auto& p = static_cast<pipe&>(com1_recv_handle.get());

        // FIXME - VT100 weirdness

        if (c == '\r')
            c = '\n';
        else if (c == 0x7f)
            c = '\b';

        p.write(span(&c, 1));

        p.release();
    }
}

NTSTATUS serial_duplex_pipe::console_set_foreground(span<const uint8_t> inbuf) {
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
            return console_set_foreground(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}
