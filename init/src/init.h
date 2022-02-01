#pragma once

#include <string_view>
#include <span>

// init.cpp
void print(std::string_view s);
void print_hex(uint64_t num, unsigned int padding = 0);

// ls.cpp
void ls(std::string_view param);

// string.cpp
size_t utf16_to_utf8_len(std::u16string_view u16sv);
void utf16_to_utf8(std::span<char> buf, std::u16string_view u16sv);
