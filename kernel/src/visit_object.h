#pragma once

import objects;
import semaphore;

template<typename T>
static void visit_obj(object& obj, const T& func);

#include "thread.h"
#include "inode.h"
#include "process.h"
#include "section.h"
#include "file.h"

// FIXME - concept to make sure func is invocable
template<typename T>
static void visit_obj(object& obj, const T& func) {
    switch (obj.type) {
        case object_type::thread:
            func(static_cast<thread&>(obj));
        break;

        case object_type::timer:
            func(static_cast<timer&>(obj));
        break;

        case object_type::event:
            func(static_cast<event&>(obj));
        break;

        case object_type::pipe:
            func(static_cast<pipe&>(obj));
        break;

        case object_type::semaphore:
            func(static_cast<semaphore&>(obj));
        break;

        case object_type::volume:
            func(static_cast<volume&>(obj));
        break;

        case object_type::inode:
            func(static_cast<inode&>(obj));
        break;

        case object_type::process:
            func(static_cast<process&>(obj));
        break;

        case object_type::section:
            func(static_cast<section&>(obj));
        break;

        case object_type::file:
            func(static_cast<file&>(obj));
        break;
    }
}
