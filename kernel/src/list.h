#pragma once

struct list_entry {
    struct list_entry* next;
    struct list_entry* prev;
};

template<typename T>
T& containing_record(list_entry* le) {
    return *(T*)((uint8_t*)le - offsetof(T, le));
}

static void __inline insert_tail_list(list_entry& head, list_entry& entry) {
    auto* last_tail = head.prev;

    entry.next = &head;
    entry.prev = last_tail;
    last_tail->next = &entry;
    head.prev = &entry;
}

static void __inline remove_entry_list(list_entry& le) {
    list_entry* prev = le.prev;
    list_entry* next = le.next;

    prev->next = next;
    next->prev = prev;
}

static bool __inline is_list_empty(const list_entry& le) {
    return le.next == &le;
}

static void __inline swap_lists(list_entry& le1, list_entry& le2) {
    bool le1empty = is_list_empty(le1);
    bool le2empty = is_list_empty(le2);
    list_entry* prev = le1.prev;
    list_entry* next = le1.next;

    if (le1empty && le2empty)
        return;

    if (le2empty) {
        le1.next = &le1;
        le1.prev = &le1;
    } else {
        le1.next = le2.next;
        le1.prev = le2.prev;
    }

    if (le1empty) {
        le2.next = &le2;
        le2.prev = &le2;
    } else {
        le2.next = next;
        le2.prev = prev;
    }

    le1.next->prev = &le1;
    le1.prev->next = &le1;

    le2.next->prev = &le2;
    le2.prev->next = &le2;
}
