#pragma once

#include <string_view>

static const uint8_t CODE_LCTRL = 0x1d;
static const uint8_t CODE_LSHIFT = 0x2a;
static const uint8_t CODE_RSHIFT = 0x36;
static const uint8_t CODE_EXTD = 0xe0;

static const uint8_t CODE_MODIFIED_ENTER = 0x1c;
static const uint8_t CODE_MODIFIED_RCTRL = 0x1d;
static const uint8_t CODE_MODIFIED_SLASH = 0x35;
static const uint8_t CODE_MODIFIED_HOME = 0x47;
static const uint8_t CODE_MODIFIED_UP = 0x48;
static const uint8_t CODE_MODIFIED_PGUP = 0x49;
static const uint8_t CODE_MODIFIED_LEFT = 0x4b;
static const uint8_t CODE_MODIFIED_RIGHT = 0x4d;
static const uint8_t CODE_MODIFIED_END = 0x4f;
static const uint8_t CODE_MODIFIED_DOWN = 0x50;
static const uint8_t CODE_MODIFIED_PGDN = 0x51;
static const uint8_t CODE_MODIFIED_INS = 0x52;
static const uint8_t CODE_MODIFIED_DEL = 0x53;

static const std::u8string_view scan_codes[] = {
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
    u8"\x1bOP", // F1
    u8"\x1bOQ", // F2
    u8"\x1bOR", // F3
    u8"\x1bOS", // F4
    u8"\x1b[15~", // F5
    // 40
    u8"\x1b[17~", // F6
    u8"\x1b[18~", // F7
    u8"\x1b[19~", // F8
    u8"\x1b[20~", // F9
    u8"\x1b[21~", // F10
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
    u8"\x1b[23~", // F11
    u8"\x1b[24~" // F12
};

static const std::u8string_view scan_codes_shift[] = {
    // 00
    u8"",
    u8"\x1b", // escape
    u8"!",
    u8"\"",
    u8"£",
    u8"$",
    u8"%",
    u8"^",
    u8"&",
    u8"*",
    u8"(",
    u8")",
    u8"_",
    u8"+",
    u8"\b", // backspace
    u8"", // tab
    // 10
    u8"Q",
    u8"W",
    u8"E",
    u8"R",
    u8"T",
    u8"Y",
    u8"U",
    u8"I",
    u8"O",
    u8"P",
    u8"{",
    u8"}",
    u8"", // enter
    u8"", // left control
    u8"A",
    u8"S",
    // 20
    u8"D",
    u8"F",
    u8"G",
    u8"H",
    u8"J",
    u8"K",
    u8"L",
    u8":",
    u8"@",
    u8"¬",
    u8"", // left shift
    u8"~",
    u8"Z",
    u8"X",
    u8"C",
    u8"V",
    // 30
    u8"B",
    u8"N",
    u8"M",
    u8"<",
    u8">",
    u8"?",
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
    u8"",
    u8"",
    u8"",
    u8"-",
    u8"",
    u8"",
    u8"",
    u8"+",
    u8"",
    // 50
    u8"",
    u8"",
    u8"0",
    u8".",
    u8"",
    u8"",
    u8"|",
    u8"", // F11
    u8"" // F12
};

static const std::u8string_view scan_codes_ctrl[] = {
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
    u8"\x11",
    u8"\x17",
    u8"\x05",
    u8"\x12",
    u8"\x14",
    u8"\x19",
    u8"\x15",
    u8"\x09",
    u8"\x0f",
    u8"\x10",
    u8"\x1b",
    u8"\x1d",
    u8"\n", // enter
    u8"", // left control
    u8"\x01",
    u8"\x13",
    // 20
    u8"\x04",
    u8"\x06",
    u8"\x07",
    u8"\x08",
    u8"\x0a",
    u8"\x0b",
    u8"\x0c",
    u8";",
    u8"'",
    u8"\x00",
    u8"", // left shift
    u8"#",
    u8"\x1a",
    u8"\x18",
    u8"\x03",
    u8"\x16",
    // 30
    u8"\x02",
    u8"\x0e",
    u8"\x0d",
    u8",",
    u8".",
    u8"\x1f",
    u8"", // right shift
    u8"*",
    u8"", // left alt
    u8"\x00",
    u8"", // caps lock
    u8"\x1bOP", // F1
    u8"\x1bOQ", // F2
    u8"\x1bOR", // F3
    u8"\x1bOS", // F4
    u8"\x1b[15~", // F5
    // 40
    u8"\x1b[17~", // F6
    u8"\x1b[18~", // F7
    u8"\x1b[19~", // F8
    u8"\x1b[20~", // F9
    u8"\x1b[21~", // F10
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
    u8"\x1b[23~", // F11
    u8"\x1b[24~" // F12
};

static const enum evdev_code evdev_codes[] = {
    // 00
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_ESC,
    evdev_code::KEY_1,
    evdev_code::KEY_2,
    evdev_code::KEY_3,
    evdev_code::KEY_4,
    evdev_code::KEY_5,
    evdev_code::KEY_6,
    evdev_code::KEY_7,
    evdev_code::KEY_8,
    evdev_code::KEY_9,
    evdev_code::KEY_0,
    evdev_code::KEY_MINUS,
    evdev_code::KEY_EQUAL,
    evdev_code::KEY_BACKSPACE,
    evdev_code::KEY_TAB,
    // 10
    evdev_code::KEY_Q,
    evdev_code::KEY_W,
    evdev_code::KEY_E,
    evdev_code::KEY_R,
    evdev_code::KEY_T,
    evdev_code::KEY_Y,
    evdev_code::KEY_U,
    evdev_code::KEY_I,
    evdev_code::KEY_O,
    evdev_code::KEY_P,
    evdev_code::KEY_LEFTBRACE,
    evdev_code::KEY_RIGHTBRACE,
    evdev_code::KEY_ENTER,
    evdev_code::KEY_LEFTCTRL,
    evdev_code::KEY_A,
    evdev_code::KEY_S,
    // 20
    evdev_code::KEY_D,
    evdev_code::KEY_F,
    evdev_code::KEY_G,
    evdev_code::KEY_H,
    evdev_code::KEY_J,
    evdev_code::KEY_K,
    evdev_code::KEY_L,
    evdev_code::KEY_SEMICOLON,
    evdev_code::KEY_APOSTROPHE,
    evdev_code::KEY_GRAVE,
    evdev_code::KEY_LEFTSHIFT,
    evdev_code::KEY_BACKSLASH,
    evdev_code::KEY_Z,
    evdev_code::KEY_X,
    evdev_code::KEY_C,
    evdev_code::KEY_V,
    // 30
    evdev_code::KEY_B,
    evdev_code::KEY_N,
    evdev_code::KEY_M,
    evdev_code::KEY_COMMA,
    evdev_code::KEY_DOT,
    evdev_code::KEY_SLASH,
    evdev_code::KEY_RIGHTSHIFT,
    evdev_code::KEY_KPASTERISK,
    evdev_code::KEY_LEFTALT,
    evdev_code::KEY_SPACE,
    evdev_code::KEY_CAPSLOCK,
    evdev_code::KEY_F1,
    evdev_code::KEY_F2,
    evdev_code::KEY_F3,
    evdev_code::KEY_F4,
    evdev_code::KEY_F5,
    // 40
    evdev_code::KEY_F6,
    evdev_code::KEY_F7,
    evdev_code::KEY_F8,
    evdev_code::KEY_F9,
    evdev_code::KEY_F10,
    evdev_code::KEY_NUMLOCK,
    evdev_code::KEY_SCROLLLOCK,
    evdev_code::KEY_KP7,
    evdev_code::KEY_KP8,
    evdev_code::KEY_KP9,
    evdev_code::KEY_KPMINUS,
    evdev_code::KEY_KP4,
    evdev_code::KEY_KP5,
    evdev_code::KEY_KP6,
    evdev_code::KEY_KPPLUS,
    evdev_code::KEY_KP1,
    // 50
    evdev_code::KEY_KP2,
    evdev_code::KEY_KP3,
    evdev_code::KEY_KP0,
    evdev_code::KEY_KPDOT,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_ZENKAKUHANKAKU,
    evdev_code::KEY_102ND,
    evdev_code::KEY_F11,
    evdev_code::KEY_F12,
};
