#include <span>
#include <optional>
#include <string_view>
#include <mutex>

import nt;
import print;
import visit;
import list;
import thread;
import objects;
import object_ref;
import mem;
import spinlock;
import handle;

using namespace std;

object_ref deref_handle(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h);
}

object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h, access_mask);
}

void object_ref::reset() {
    if (rw)
        object_release(rw->get());
}
