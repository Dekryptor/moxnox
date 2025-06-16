module;

#include "kernel.h"
#include "spinlock.h"
#include "handle.h"
import rbtree;
import event;
import pipe;
import timer;
import objects;
#include "thread.h"
import page_cache;
import rwsem;
#include "inode.h"
import volume;
#include "visit_object.h"

export module visit;

using namespace std;

export void object_acquire(object& obj) {
    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });
}

export void object_release(object& obj) {
    visit_obj(obj, [](auto&& o) {
        o.release();
    });
}

export bool object_is_sync_object(object& obj) {
    bool ret = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
    });

    return ret;
}

export void object_on_wake(object& obj) {
    visit_obj(obj, [&](auto&& o) {
        if constexpr (object_has_on_wake<remove_reference_t<decltype(o)>>)
            o.on_wake();
    });
}
