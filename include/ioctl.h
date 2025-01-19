#pragma once

enum class IOCTL_METHOD : uint8_t {
    BUFFERED = 0,
    IN_DIRECT = 1,
    OUT_DIRECT = 2,
    NEITHER = 3
};

enum class IOCTL_ACCESS : uint8_t {
    ANY = 0,
    READ = 1,
    WRITE = 2,
    READ_AND_WRITE = 3
};

static consteval uint32_t CTL_CODE(uint16_t device_type, uint32_t function,
                                   enum IOCTL_METHOD method, enum IOCTL_ACCESS access) {
    return (uint32_t)(((uint32_t)device_type << 16) | ((uint8_t)access << 14) | (function << 2) | (uint8_t)method);
}

static const uint16_t FILE_DEVICE_MOXNOX = 0x4d4e;

static const auto IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND = CTL_CODE(FILE_DEVICE_MOXNOX, 0, IOCTL_METHOD::BUFFERED, IOCTL_ACCESS::ANY);
