module;

#include <stdint.h>
#include <stddef.h>
#include "list.h"

export module list;

export template<typename T>
T& containing_record(list_entry* le) {
    return *(T*)((uint8_t*)le - offsetof(T, le));
}

export void __inline insert_tail_list(list_entry& head, list_entry& entry) {
    auto* last_tail = head.prev;

    entry.next = &head;
    entry.prev = last_tail;
    last_tail->next = &entry;
    head.prev = &entry;
}

export void __inline insert_head_list(list_entry& head, list_entry& entry) {
    auto* last_head = head.next;

    entry.next = last_head;
    entry.prev = &head;
    last_head->prev = &entry;
    head.next = &entry;
}

export void __inline remove_entry_list(list_entry& le) {
    list_entry* prev = le.prev;
    list_entry* next = le.next;

    prev->next = next;
    next->prev = prev;
}

export bool __inline is_list_empty(const list_entry& le) {
    return le.next == &le;
}

export void __inline swap_lists(list_entry& le1, list_entry& le2) {
    bool le1empty = is_list_empty(le1);
    bool le2empty = is_list_empty(le2);

    if (le1empty && le2empty)
        return;

    if (le2empty) {
        le2.next = le1.next;
        le2.prev = le1.prev;
        le2.next->prev = &le2;
        le2.prev->next = &le2;

        le1.next = &le1;
        le1.prev = &le1;

        return;
    }

    if (le1empty) {
        le1.next = le2.next;
        le1.prev = le2.prev;
        le1.next->prev = &le1;
        le1.prev->next = &le1;

        le2.next = &le2;
        le2.prev = &le2;

        return;
    }

    list_entry* le1head = le1.next;
    list_entry* le1tail = le1.prev;
    list_entry* le2head = le2.next;
    list_entry* le2tail = le2.prev;

    le1.next = le2head;
    le1.prev = le2tail;
    le2.next = le1head;
    le2.prev = le1tail;

    le1.next->prev = &le1;
    le1.prev->next = &le1;
    le2.next->prev = &le2;
    le2.prev->next = &le2;
}
