#pragma once

handle_t add_kernel_handle(object& obj, uint32_t access_mask);
void close_handle(handle_t h);
std::optional<std::reference_wrapper<object>> deref_handle(handle_t h);
bool name_object(handle_t h, std::string_view name, bool persistent = false);
bool name_object(object& obj, std::string_view name, bool persistent = false);
unique_handle open_object(std::string_view name);
size_t read(handle_t h, std::span<uint8_t> s, std::optional<size_t> offset = std::nullopt);
void write(handle_t h, std::span<const uint8_t> s);
void write(handle_t h, std::string_view s);
void handle_closed(object& obj);
