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

export void object_acquire(object& obj) {
    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });
}
