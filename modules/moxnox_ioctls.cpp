module;

#include <stdint.h>

export module moxnox_ioctls;

import nt;

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
    return (uint32_t)(((uint32_t)device_type << 16) | (uint32_t)((uint8_t)access << 14) | (function << 2) | (uint8_t)method);
}

static const uint16_t FILE_DEVICE_MOXNOX = 0x4d4e;

export const auto IOCTL_MOXNOX_CONSOLE_SET_CURSOR = CTL_CODE(FILE_DEVICE_MOXNOX, 1, IOCTL_METHOD::BUFFERED, IOCTL_ACCESS::ANY);
export const auto IOCTL_MOXNOX_KEYBOARD_SET_LEDS = CTL_CODE(FILE_DEVICE_MOXNOX, 2, IOCTL_METHOD::BUFFERED, IOCTL_ACCESS::ANY);
export const auto IOCTL_MOXNOX_VFSCTL_MOUNT = CTL_CODE(FILE_DEVICE_MOXNOX, 3, IOCTL_METHOD::BUFFERED, IOCTL_ACCESS::ANY);
export const auto IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS = CTL_CODE(FILE_DEVICE_MOXNOX, 4, IOCTL_METHOD::BUFFERED, IOCTL_ACCESS::ANY);
export const auto IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO = CTL_CODE(FILE_DEVICE_MOXNOX, 5, IOCTL_METHOD::BUFFERED, IOCTL_ACCESS::ANY);

export struct MOXNOX_SET_CURSOR {
    uint32_t x;
    uint32_t y;
    bool enabled;
};

export struct keyboard_leds {
    uint8_t caps_lock : 1;
    uint8_t num_lock : 1;
    uint8_t scroll_lock : 1;
    uint8_t : 5;
};

export struct MOXNOX_SET_LEDS {
    keyboard_leds leds;
    uint32_t id;
};

export struct MOXNOX_VFSCTL_MOUNT {
    uint32_t mount_type;
    HANDLE source;
    HANDLE directory;
    // FIXME - options
};

export struct MOXNOX_VFSCTL_LIST_MOUNTS {
    uint64_t num;
    uint64_t ids[0];
};

export struct MOXNOX_VFSCTL_MOUNT_INFO {
    uint32_t source;
    uint32_t directory;
    uint32_t type;
    // FIXME - options
};
