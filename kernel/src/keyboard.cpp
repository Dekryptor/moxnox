#include "kernel.h"
#include "unique_handle.h"
#include "objects.h"
#include "spinlock.h"

using namespace std;

static const uint8_t keyboard_interrupt = 0x21;

unique_handle keyboard_event;
optional<reference_wrapper<event>> keyboard_event_obj;

static spinlock keyboard_lock;
static uint8_t* keyboard_buffer;
static unsigned int keyboard_buffer_write_pos = 0;
static unsigned int keyboard_buffer_read_pos = 0;

static const u8string_view scan_codes[] = {
    // 00
    u8"",
    u8"\x1b", // escape
    u8"1",
    u8"2",
    u8"3",
    u8"4",
    u8"5",
    u8"6",
    u8"7",
    u8"8",
    u8"9",
    u8"0",
    u8"-",
    u8"=",
    u8"\b", // backspace
    u8"\t", // tab
    // 10
    u8"q",
    u8"w",
    u8"e",
    u8"r",
    u8"t",
    u8"y",
    u8"u",
    u8"i",
    u8"o",
    u8"p",
    u8"[",
    u8"]",
    u8"\n", // enter
    u8"", // left control
    u8"a",
    u8"s",
    // 20
    u8"d",
    u8"f",
    u8"g",
    u8"h",
    u8"j",
    u8"k",
    u8"l",
    u8";",
    u8"'",
    u8"`",
    u8"", // left shift
    u8"#",
    u8"z",
    u8"x",
    u8"c",
    u8"v",
    // 30
    u8"b",
    u8"n",
    u8"m",
    u8",",
    u8".",
    u8"/",
    u8"", // right shift
    u8"*",
    u8"", // left alt
    u8" ",
    u8"", // caps lock
    u8"", // F1
    u8"", // F2
    u8"", // F3
    u8"", // F4
    u8"", // F5
    // 40
    u8"", // F6
    u8"", // F7
    u8"", // F8
    u8"", // F9
    u8"", // F10
    u8"", // num lock
    u8"", // scroll lock
    u8"7",
    u8"8",
    u8"9",
    u8"-",
    u8"4",
    u8"5",
    u8"6",
    u8"+",
    u8"1",
    // 50
    u8"2",
    u8"3",
    u8"0",
    u8".",
    u8"",
    u8"",
    u8"\\",
    u8"", // F11
    u8"" // F12
};

static void keyboard_event_recv(void*) {
    do {
        wait_for_object(keyboard_event.get());

        lock_guard lock(keyboard_lock);

        while (keyboard_buffer_read_pos != keyboard_buffer_write_pos) {
            auto sc = keyboard_buffer[keyboard_buffer_read_pos];

            // FIXME - multi-byte codes (e0 etc.)
            // FIXME - shift and caps lock

            if (sc < sizeof(scan_codes) / sizeof(scan_codes[0]) && !scan_codes[sc].empty()) {
                const auto& u8str = scan_codes[sc];
                auto sv = string_view((char*)u8str.data(), u8str.length());

                print(sv);
            }

            keyboard_buffer_read_pos++;
            keyboard_buffer_read_pos &= 0xfff;
        }
    } while (true);
}

void init_keyboard() {
    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return;
    }

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    if (!map_page(pa, va, mapping_type::read_write)) {
        print("map_page failed\n");
        return;
    }

    keyboard_buffer = (uint8_t*)va;

    keyboard_event = create_event(true);
    if (!keyboard_event) {
        print("create_event failed\n");
        return;
    }

    auto& obj = deref_handle(keyboard_event.get())->get();
    keyboard_event_obj = static_cast<event&>(obj);

    if (!add_kernel_thread(keyboard_event_recv)) {
        print("add_kernel_thread failed\n");
        return;
    }

    add_idt_entry(keyboard_interrupt, (void*)keyboard_isr, false, 0);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 1 = keyboard
    add_irq_handler(1, keyboard_interrupt, local_apic_id);
}

extern "C"
void keyboard_isr_func(uint8_t code) {
    {
        lock_guard lock(keyboard_lock);

        keyboard_buffer[keyboard_buffer_write_pos] = code;

        keyboard_buffer_write_pos++;
        keyboard_buffer_write_pos &= 0xfff;
    }

    set_event(keyboard_event_obj->get());
}
