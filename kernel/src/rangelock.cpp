module;

#include <stdint.h>
#include "rangelock.h"

export module rangelock;

import thread;

export struct rangelock_req {
    rangelock_req(rangelock& rl, uint64_t start, uint64_t length) : rl(rl) {
        this->start = start;
        this->length = length;

        this->lock();
    }

    ~rangelock_req() {
        this->unlock();
    }

    void lock();
    void unlock();

    list_entry le;
    rangelock& rl;
    uint64_t start;
    uint64_t length;
    thread* t;
};
