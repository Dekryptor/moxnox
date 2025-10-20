#pragma once

#include <stdint.h>

#define _LINUX
#define ACPI_MACHINE_WIDTH 64
#define ACPI_USE_SYSTEM_INTTYPES

typedef int64_t INT64;
typedef uint64_t UINT64;
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t UINT8;
#define BOOLEAN uint8_t

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wextern-c-compat"
extern "C" {
    #include <acpi.h>
}
#pragma GCC diagnostic pop

#undef BOOLEAN

#undef _LINUX
