module;

#include "kernel.h"
#include "handle.h"
#include "spinlock.h"
import objects;
#include "process.h"
#include <ioctl.h>

export module keyboard;

import pipe;
import print;
import evdev;

using namespace std;

#define ENABLE_PROCESSED_INPUT 0x1 // FIXME

static const unsigned int keyboard_pipe_buffer_pages = 1;
static object_ref keyboard_pipe_obj;

class keyboard_pipe : public pipe {
public:
    keyboard_pipe(unsigned int buffer_pages) : pipe(buffer_pages) { }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

using set_leds_func = void (*)(void* ctx, keyboard_leds leds);

static atomic<uint32_t> next_keyboard_id = 0;
static spinlock keyboard_contexts_spinlock;
static list_entry keyboard_contexts = { &keyboard_contexts, &keyboard_contexts };

export class keyboard_context {
public:
    keyboard_context(pipe& p, set_leds_func led_func, void* ctx) : led_func(led_func), ctx(ctx), p(p) {
        p.acquire();

        id = ++next_keyboard_id;

        {
            lock_guard lock(keyboard_contexts_spinlock);
            insert_tail_list(keyboard_contexts, this->le);
        }
    }

    ~keyboard_context() {
        {
            lock_guard lock(keyboard_contexts_spinlock);
            remove_entry_list(this->le);
        }

        p.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void listen_thread();
    void unregister();

    bool dead = false;
    uint32_t id;
    list_entry le;
    set_leds_func led_func;
    void* ctx;

private:
    void key_event(input_event& ev);

    pipe& p;
    atomic<unsigned int> refcount = 1;
};

export bool init_keyboard_pipe() {
    auto pptr = new keyboard_pipe(keyboard_pipe_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_pipe_obj = object_ref{*pptr};

    auto status = name_object(keyboard_pipe_obj.get(), "keyboard");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

nt_status keyboard_pipe::ioctl(uint32_t code, span<const uint8_t> inbuf,
                               span<uint8_t>, uintptr_t& written) {
    if (code == IOCTL_MOXNOX_KEYBOARD_SET_LEDS) {
        if (inbuf.size() < sizeof(MOXNOX_SET_LEDS))
            return nt_status::INVALID_PARAMETER;

        const auto& leds = *(MOXNOX_SET_LEDS*)inbuf.data();

        keyboard_context* ctx = nullptr;

        {
            lock_guard lock(keyboard_contexts_spinlock);

            for (auto le = keyboard_contexts.next; le != &keyboard_contexts; le = le->next) {
                auto& c = containing_record<keyboard_context>(le);

                if (c.id == leds.id) {
                    c.acquire();
                    ctx = &c;
                    break;
                }
            }
        }

        if (ctx) {
            if (ctx->led_func)
                ctx->led_func(ctx->ctx, leds.leds);

            ctx->release();
        }

        written = 0;

        return nt_status::SUCCESS;
    } else
        return nt_status::NOT_SUPPORTED;
}

void keyboard_context::key_event(input_event& ev) {
    input_event_with_id ev2;

    ev2.ev = ev;
    ev2.id = this->id;

    auto sp = span((uint8_t*)&ev2, sizeof(ev2));
    auto& k = static_cast<keyboard_pipe&>(keyboard_pipe_obj.get());

    // FIXME - make sure we don't do partial writes

    while (!sp.empty()) {
        uint64_t written_bytes;
        auto status = k.write(sp, written_bytes);

        if (!success(status)) {
            print("pipe::write returned {}\n", status);
            return;
        }

        if (written_bytes == sp.size())
            break;

        sp = span(sp.data() + written_bytes, sp.size() - written_bytes);
    }
}

void keyboard_context::listen_thread() {
    if (led_func) {
        keyboard_leds leds;

        leds.caps_lock = 0;
        leds.num_lock = 0;
        leds.scroll_lock = 0;

        led_func(ctx, leds);
    }

    // FIXME - make sure this thread dies if keyboard goes away

    while (true) {
        input_event ev;
        nt_status status;

        auto sp = span((uint8_t*)&ev, sizeof(ev));

        while (true) {
            uint64_t read_bytes;

            status = p.read(sp, read_bytes);
            if (dead || !success(status))
                break;

            if (read_bytes == sp.size())
                break;

            sp = span(sp.data() + read_bytes, sp.size() - read_bytes);
        }

        if (dead)
            break;

        if (!success(status)) {
            print("pipe::read returned {}\n", status);
            break;
        }

        if (ev.type != evdev_type::EV_KEY)
            continue;

        key_event(ev);
    }
}

void keyboard_context::unregister() {
    dead = true;
}

export keyboard_context* register_keyboard(pipe& p, set_leds_func led_func, void* ctx) {
    auto kc = new keyboard_context(p, led_func, ctx);
    if (!kc) {
        print("new failed\n");
        return nullptr;
    }

    add_kernel_thread([](void* ctx) {
        auto& kc = *(keyboard_context*)ctx;

        kc.listen_thread();

        delete &kc;
    }, kc);

    return kc;
}
