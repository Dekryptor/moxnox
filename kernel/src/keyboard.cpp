module;

#include "kernel.h"
#include "handle.h"

export module keyboard;

import pipe;
import print;
import evdev;

using namespace std;

static const unsigned int keyboard_pipe_buffer_pages = 1;
static object_ref keyboard_pipe_obj;

class keyboard_context {
public:
    keyboard_context(pipe& p) : p(p) {
        p.acquire();
    }

    ~keyboard_context() {
        p.release();
    }

    void listen_thread();

private:
    pipe& p;
};

export bool init_keyboard_pipe() {
    auto pptr = new pipe(keyboard_pipe_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_pipe_obj = object_ref{*pptr};

    if (!name_object(keyboard_pipe_obj.get(), "keyboard")) {
        print("name_object failed\n");
        return false;
    }

    return true;
}

void keyboard_context::listen_thread() {
    // FIXME - make sure this thread dies if keyboard goes away

    while (true) {
        input_event ev;
        nt_status status;

        auto sp = span((uint8_t*)&ev, sizeof(ev));

        while (true) {
            uint64_t read_bytes;

            status = p.read(sp, read_bytes);
            if (!success(status))
                break;

            if (read_bytes == sp.size())
                break;

            sp = span(sp.data() + read_bytes, sp.size() - read_bytes);
        }

        if (!success(status)) {
            print("pipe::read returned {}\n", status);
            break;
        }

        if (ev.type != evdev_type::EV_KEY)
            continue;

        print("FIXME - key event\n");
    }
}

export void register_keyboard(pipe& p) {
    auto kc = new keyboard_context(p);
    if (!kc) {
        print("new failed\n");
        return;
    }

    add_kernel_thread([](void* ctx) {
        auto& kc = *(keyboard_context*)ctx;

        kc.listen_thread();

        delete &kc;
    }, kc);
}
