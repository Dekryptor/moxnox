module;

#include "kernel.h"
#include "handle.h"

export module keyboard;

import pipe;
import print;

static const unsigned int keyboard_pipe_buffer_pages = 1;
static object_ref keyboard_pipe_obj;

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
