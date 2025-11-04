#include <optional>

import nt;
import thread;
import object_ref;
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
