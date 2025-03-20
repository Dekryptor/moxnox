module;

#include "kernel.h"
#include "handle.h"
#include "spinlock.h"
#include "visit_object.h"
#include <ioctl.h>

export module serial;

import print;
import format;
import inout;
import config;
import interrupts;

using namespace std;

const uint32_t ENABLE_PROCESSED_INPUT = 0x1;
const uint32_t ENABLE_PROCESSED_OUTPUT = 0x1;

static const unsigned int serial_buffer_pages = 1;
static atomic<unsigned int> serial_port_number = 0;

static bool serial_isr_func(void*);

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
    serial_duplex_pipe(serial_in_pipe& recv_pipe, serial_out_pipe& send_pipe, uint16_t port) : pipe(0), recv_pipe(recv_pipe), send_pipe(send_pipe), port(port) {
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
    bool isr_func();

    serial_in_pipe& recv_pipe;
    serial_out_pipe& send_pipe;
    uint16_t port;
};

export object_ref com1_pipe_handle;

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

static bool init_serial_port(uint16_t port) {
    // disable interrupts
    outb(port + 1, 0);

    // enable DLAB
    outb(port + 3, 0x80);

    // set divisor to 3 (38400 baud)

    outb(port, 0x03);
    outb(port + 1, 0);

    // 8 bits, no parity, 1 stop bit
    outb(port + 3, 0x03);

    // enable FIFOs, clear them, and set interrupt trigger level to 14 bytes
    outb(port + 2, 0xc7);

    // put in loopback mode
    outb(port + 4, 0x10);

    // test
    outb(port, 0xa5);

    if (inb(port) != 0xa5)
        return false;

    // serial port okay, enable IRQs and put in normal mode
    outb(port + 4, 0x0f);

    // send IRQ when transmit buffer is empty or data available
    outb(port + 1, 0x03);

    return true;
}

static optional<unsigned int> parse_uid(string_view uid) {
    if (uid.empty())
        return nullopt;

    for (auto c : uid) {
        if (c < '0' || c > '9')
            return nullopt;
    }

    unsigned int val;

    from_chars(uid.data(), uid.data() + uid.size(), val);

    return val;
}

export bool init_serial(uint16_t port, uint32_t irq, string_view uid) {
    if (!init_serial_port(port))
        return false;

    auto send_handle = create_serial_out_pipe(serial_buffer_pages, port);
    if (!send_handle) {
        print("create_serial_out_pipe failed\n");
        return false;
    }

    auto recv_handle = create_serial_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_serial_in_pipe failed\n");
        return false;
    }

    auto& send_pipe = static_cast<serial_out_pipe&>(send_handle.get());

    auto& recv_pipe = static_cast<serial_in_pipe&>(recv_handle.get());

    auto pptr = new serial_duplex_pipe(recv_pipe, send_pipe, port);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto pipe_handle = object_ref{*pptr};

    // FIXME - numbering assumes that ACPI devices are started in order, i.e. all from one thread

    // use _UID - 1 as number if present, otherwise start from end of numbered range

    auto uidnum = parse_uid(uid);
    unsigned int num;

    if (uidnum.has_value())
        num = *uidnum - 1;
    else
        num = serial_port_number.fetch_add(1);

    {
        char s[10];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "ttyS{}", num);

        if (!name_object(pipe_handle.get(), string_view(s, (size_t)(sp.data() - s)))) {
            print("name_object failed\n");
            return false;
        }
    }

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    if (!add_irq_handler(irq, local_apic_id, serial_isr_func, pptr)) {
        print("add_irq_handler failed\n");
        return false;
    }

    if (num == 0)
        com1_pipe_handle.swap(pipe_handle);

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

bool serial_duplex_pipe::isr_func() {
    auto iir = inb(port + 2); // interrupt identification register

    if (iir & 1) // not us
        return false;

    if ((iir & 0x0e) == 0x2) { // transmitter holding register empty
        lock_guard l(send_pipe.lock);

        if (send_pipe.buf_unread == 0) {
            send_pipe.ready = true;
            return true;
        }

        size_t buf_read_pos = send_pipe.buf_write_pos + (send_pipe.buf_pages << PAGE_SHIFT) - send_pipe.buf_unread;

        while (buf_read_pos >= send_pipe.buf_pages << PAGE_SHIFT) {
            buf_read_pos -= send_pipe.buf_pages << PAGE_SHIFT;
        }

        outb(send_pipe.port, send_pipe.buf[buf_read_pos]);
        send_pipe.buf_unread--;
    } else if ((iir & 0xe) == 0x4 || (iir & 0xe) == 0xc) { // data available or character timeout
        uint64_t written_bytes;

        auto c = inb(port);

        recv_pipe.write(span(&c, 1), written_bytes);
    }

    return true;
}

static bool serial_isr_func(void* ctx) {
    auto& pipe = *(serial_duplex_pipe*)ctx;

    return pipe.isr_func();
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

export void prelim_found_serial_port(string_view uid) {
    auto val = parse_uid(uid);

    if (!val.has_value())
        return;

    if (*val > serial_port_number)
        serial_port_number = *val;
}
