#pragma once

static void __inline InitializeListHead(LIST_ENTRY* head) {
    head->Flink = head;
    head->Blink = head;
}

static void __inline InsertTailList(LIST_ENTRY* head, LIST_ENTRY* le) {
    auto last = head->Blink;

    le->Flink = head;
    le->Blink = last;

    last->Flink = le;
    head->Blink = le;
}

static void __inline InsertHeadList(LIST_ENTRY* head, LIST_ENTRY* le) {
    auto first = head->Flink;

    le->Flink = first;
    le->Blink = head;

    first->Blink = le;
    head->Flink = le;
}

#define CONTAINING_RECORD(address, type, field) ((type*)((uint8_t*)address - (uintptr_t)(&((type*)0)->field)))
