// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

import ucrtbase;
import print;

extern "C"
int _initialize_onexit_table(_onexit_table_t* table) {
    if (!table)
        return -1;

    if (table->_first == table->_end) {
        table->_first = nullptr;
        table->_end = nullptr;
        table->_last = nullptr;
    }

    return 0;
}

extern "C"
int _register_onexit_function(_onexit_table_t* table, _onexit_t func) {
    static const size_t INIT_SIZE = 32;

    if (!table)
        return -1;

    // FIXME - get lock

    auto& t = *table;

    if (!t._first) {
        t._first = (_PVFV*)malloc(INIT_SIZE * sizeof(void*));
        if (!t._first)
            return -1;

        t._last = t._first;
        t._end = t._first + INIT_SIZE;
    } else if (t._last == t._end) { // table is full, double size
        auto len = (size_t)(t._end - t._first);

        auto ptr = (_PVFV*)realloc(t._first, 2 * len * sizeof(void*));
        if (!ptr)
            return -1;

        t._first = ptr;
        t._end = t._first + (2 * len);
        t._last = t._first + len;
    }

    *t._last = (_PVFV)func;
    t._last++;

    return 0;
}

extern "C"
int _execute_onexit_table(_onexit_table_t* table) {
    auto& t = *table;

    if (!table)
        return -1;

    // FIXME - lock

    if (!t._first || t._first >= t._last)
        return 0;

    auto f = t._last - 1;

    while (f >= t._first) {
        if (*f)
            (*f)();

        f--;
    }

    free(t._first);

    return 0;
}
