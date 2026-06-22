module;

#include <optional>

module object_ref;

import visit;

using namespace std;

extern "C"
void object_ref::reset() {
    if (rw) {
        object_release(rw->get());
        rw = nullopt;
    }
}
