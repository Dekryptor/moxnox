#include "kernel.h"
#include "objects.h"

unique_handle create_section(handle_t h, bool image) {
    if (!image) {
        print("FIXME - non-image sections\n");
        return {};
    }

    // FIXME - non-image sections
    // FIXME - sections backed by page file
    // FIXME - page fault tricks

    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return {};
    }

    auto& obj = objrw->get();

    if (obj.type != object_type::inode) {
        // FIXME - exception
        print("Not an inode\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return {};
    }

    // FIXME - check image format, and load

    auto sptr = new section;
    if (!sptr) {
        print("new failed\n");
        return {};
    }

    auto& s = *sptr;

    s.ino = static_cast<inode&>(obj);

    auto hv = add_handle(s, 0); // FIXME - access mask

    s.release(); // add_handle increases refcount

    return unique_handle{hv};
}

section::~section() {
    if (ino)
        ino->get().release();
}
