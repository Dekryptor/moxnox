#include "kernel.h"
#include "unique_handle.h"
#include "objects.h"

using namespace std;

static const uint8_t serial_interrupt = 0x23;
static const uint16_t com1_port = 0x3f8;
static const unsigned int serial_buffer_pages = 1;

class serial_pipe : public pipe {
public:
    serial_pipe(unsigned int buffer_pages, uint16_t port) : pipe(buffer_pages), port(port) { }
    size_t write(span<const uint8_t> s) override;

    bool ready = true;
    uint16_t port;
};

static serial_pipe* com1_pipe = nullptr;

unique_handle com1_pipe_handle;
static unique_handle com1_recv_handle;
static object_ref duplex_pipe;

static unique_handle create_serial_pipe(unsigned int buffer_pages, uint16_t port) {
    auto pptr = new serial_pipe(buffer_pages, port);
    if (!pptr) {
        printk("new failed\n");
        return {};
    }

    auto& p = *pptr;

    auto hv = add_kernel_handle(p, 0); // FIXME - access mask

    p.release(); // add_kernel_handle increases refcount

    return unique_handle{hv};
}

bool init_serial() {
    // FIXME - COM2, COM3, COM4

    if (!init_serial_port(com1_port))
        return true;

    com1_pipe_handle = create_serial_pipe(serial_buffer_pages, com1_port);
    if (!com1_pipe_handle) {
        printk("create_serial_pipe failed\n");
        return false;
    }

    com1_recv_handle = create_pipe(serial_buffer_pages);
    if (!com1_recv_handle) {
        printk("create_pipe failed\n");
        return false;
    }

    com1_pipe = &static_cast<serial_pipe&>(deref_handle(com1_pipe_handle.get())->get());

    auto& com1_recv_pipe = static_cast<pipe&>(deref_handle(com1_recv_handle.get())->get());

    duplex_pipe = create_duplex_pipe(com1_recv_pipe, *com1_pipe);
    if (!duplex_pipe) {
        printk("create_duplex_pipe failed\n");
        return false;
    }

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

size_t serial_pipe::write(span<const uint8_t> s) {
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

        auto& p = static_cast<pipe&>(deref_handle(com1_recv_handle.get())->get());

        // FIXME - VT100 weirdness

        if (c == '\r')
            c = '\n';
        else if (c == 0x7f)
            c = '\b';

        p.write(span(&c, 1));

        p.release();
    }
}
