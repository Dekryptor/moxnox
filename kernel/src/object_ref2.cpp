module object_ref;

import visit;

extern "C"
void object_ref::reset() {
    if (rw)
        object_release(rw->get());
}
