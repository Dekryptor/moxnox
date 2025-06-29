module;

#include "kernel.h"
#include <charconv>
#include "handle.h"
#include "spinlock.h"
import pipe;
import chardev;
import objects;
#include "thread.h"
#include "process.h"
#include "acpica.h"
#include <ioctl.h>

export module serial;

import print;
import format;
import inout;
import config;
import interrupts;
import acpi;

using namespace std;

#define ENABLE_PROCESSED_INPUT 0x1 // FIXME
const uint32_t ENABLE_PROCESSED_OUTPUT = 0x1;

static const unsigned int serial_buffer_pages = 1;
static atomic<unsigned int> serial_port_number = 0;

static bool serial_isr_func(void*);

static void prelim_found_serial_port(string_view uid);
static void found_serial_port(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "PNP0500", found_serial_port, prelim_found_serial_port },
    { {}, "PNP0501", found_serial_port, prelim_found_serial_port }
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_serial(init_acpi_hids);

export class serial_console {
public:
    nt_status console_set_foreground(span<const uint8_t> inbuf);
    void handle_ctrl_c();
    nt_status vt100_recv(span<const uint8_t> s, auto func);

    spinlock fg_proc_lock;
    optional<reference_wrapper<process>> fg_proc;
    uint32_t input_mode = ENABLE_PROCESSED_INPUT;
};

class serial_in_pipe : public pipe, public serial_console {
public:
    serial_in_pipe(unsigned int buffer_pages) : pipe(buffer_pages) { }

    ~serial_in_pipe() {
        lock_guard lg(fg_proc_lock);

        if (fg_proc)
            fg_proc->get().release();
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
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

class serial_duplex_pipe : public chardev {
public:
    serial_duplex_pipe(serial_in_pipe& recv_pipe, serial_out_pipe& send_pipe, uint16_t port) : recv_pipe(recv_pipe), send_pipe(send_pipe), port(port) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~serial_duplex_pipe() {
        recv_pipe.release();
        send_pipe.release();
    }

    NTSTATUS read(span<uint8_t> s, uint64_t& read_bytes) override {
        return recv_pipe.read(s, read_bytes);
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override {
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

        auto status = name_object(pipe_handle.get(), string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
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
    auto orig_s = s;

    if (s.empty())
        return 0;

    do {
        size_t to_copy;

        {
            lock_guard l(lock);

            if (ready) {
                // send first character
                outb(port, s[0]);

                ready = false;
                s = s.subspan(1);

                if (s.empty())
                    break;
            }

            size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;

            to_copy = min(s.size(), buf_left);

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
        }

        if (s.empty())
            break;

        // block while we wait for space to become available

        if (wait_for_object(not_full_event, true) == nt_status::ALERTED)
            break;
    } while (true);

    return (uint64_t)(s.data() - orig_s.data());
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

    written_bytes = 0;

    static const uint8_t SEND_BS[] = { '\b', ' ', '\b' };

    auto current_thread = get_current_thread();

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\b') {
            if (last_bs == -1)
                written_bytes += do_write(s.subspan(0, i));
            else
                written_bytes += do_write(s.subspan((size_t)(last_bs + 1), i - (size_t)(last_bs + 1)));

            if (current_thread->pending_signal)
                break;

            if (do_write(SEND_BS) > 0)
                written_bytes++;

            if (current_thread->pending_signal)
                break;

            last_bs = (int)i;
        }
    }

    written_bytes += do_write(s.subspan((size_t)(last_bs + 1), s.size() - (size_t)(last_bs + 1)));

    return nt_status::SUCCESS;
}

nt_status serial_console::vt100_recv(span<const uint8_t> s, auto func) {
    if (!(input_mode & ENABLE_PROCESSED_INPUT))
        return func(s);

    // handle VT100 weirdness

    int last_run_start = -1;
    nt_status status;

    for (size_t i = 0; i < s.size(); i++) {
        auto c = s[i];

        if (c == '\r' || c == 0x7f || c == '\x03') {
            if (last_run_start == -1)
                status = func(s.subspan(0, (size_t)i));
            else
                status = func(s.subspan((size_t)(last_run_start + 1), i - (size_t)(last_run_start + 1)));

            if (!success(status))
                return status;

            if (c == '\r') {
                c = '\n';
                status = func(span(&c, 1));

                if (!success(status))
                    return status;
            } else if (c == 0x7f) {
                c = '\b';
                status = func(span(&c, 1));

                if (!success(status))
                    return status;
            } else if (c == '\x03')
                handle_ctrl_c();

            last_run_start = (int)i;
        }
    }

    return func(s.subspan((size_t)(last_run_start + 1), s.size() - (size_t)(last_run_start + 1)));
}

nt_status serial_in_pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    written_bytes = 0;

    return vt100_recv(s, [&](span<const uint8_t> s) {
        size_t tmp;

        auto status = pipe::write(s, tmp);

        if (success(status))
            written_bytes += tmp;

        return status;
    });
}

bool serial_duplex_pipe::isr_func() {
    auto iir = inb(port + 2); // interrupt identification register

    if (iir & 1) // not us
        return false;

    if (iir & 0x8 || ((iir & 0x6) == 0x4)) { // timeout, or data available
        uint64_t written_bytes;

        auto c = inb(port);

        recv_pipe.write(span(&c, 1), written_bytes);
    }

    if (iir & 0x8 || (iir & 0x6) == 0x2) { // timeout, or transmitter holding register empty
        sync_lock sl;
        lock_guard l(sl);
        lock_guard l2(send_pipe.lock);

        if (send_pipe.buf_unread == 0)
            send_pipe.ready = true;
        else {
            size_t buf_read_pos = send_pipe.buf_write_pos + (send_pipe.buf_pages << PAGE_SHIFT) - send_pipe.buf_unread;

            while (buf_read_pos >= send_pipe.buf_pages << PAGE_SHIFT) {
                buf_read_pos -= send_pipe.buf_pages << PAGE_SHIFT;
            }

            outb(send_pipe.port, send_pipe.buf[buf_read_pos]);
            send_pipe.buf_unread--;
        }
    }

    return true;
}

static bool serial_isr_func(void* ctx) {
    auto& pipe = *(serial_duplex_pipe*)ctx;

    return pipe.isr_func();
}

nt_status serial_console::console_set_foreground(span<const uint8_t> inbuf) {
    if (inbuf.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto h = *(HANDLE*)inbuf.data();

    auto obj = deref_handle(h);
    if (!obj)
        return nt_status::INVALID_HANDLE;

    if (obj.get().type != object_type::process)
        return nt_status::INVALID_HANDLE;

    auto& proc = static_cast<process&>(obj.get());

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

void serial_console::handle_ctrl_c() {
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

static void prelim_found_serial_port(string_view uid) {
    auto val = parse_uid(uid);

    if (!val.has_value())
        return;

    if (*val > serial_port_number)
        serial_port_number = *val;
}

static void found_serial_port(const acpi_device& d) {
    optional<uint16_t> port;
    optional<uint32_t> irq;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type == acpi_device_resource_type::io) {
            auto& r2 = static_cast<acpi_device_resource_io&>(r);

            if (r2.io.IoDecode == ACPI_DECODE_16 && !port.has_value())
                port = r2.io.Minimum;
        } else if (r.type == acpi_device_resource_type::irq) {
            auto& r2 = static_cast<acpi_device_resource_irq&>(r);

            irq = r2.irq.Interrupt;
        } else if (r.type == acpi_device_resource_type::extended_irq) {
            auto& r2 = static_cast<acpi_device_resource_extended_irq&>(r);

            irq = r2.extended_irq.Interrupt;
        }
    }

    if (!port.has_value() || !irq.has_value())
        return;

    if (!init_serial(*port, *irq, d.uid))
        print("init_serial failed for port {:x}, irq {:x}\n", *port, *irq);
}
