#pragma once

#include <string_view>

static const uint8_t CODE_LSHIFT = 0x2a;
static const uint8_t CODE_RSHIFT = 0x36;

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
