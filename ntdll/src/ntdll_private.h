#pragma once

#include <ntdll.h>
#include <stdint.h>
#include <string_view>
#include <span>

static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_SHIFT = 12;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

// string.cpp
size_t utf8_to_utf16_len(std::string_view sv);
void utf8_to_utf16(std::span<char16_t> buf, std::string_view sv);
std::span<char> sp_itoa_hex(std::span<char> dest, uint64_t num, unsigned int padding);

// ntdll.cpp
void print(std::string_view s);
void print_hex(uint64_t num, unsigned int padding = 0);
