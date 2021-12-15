#pragma once

typedef struct _list_entry {
    struct _list_entry* prev;
    struct _list_entry* next;
} list_entry;

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
