#pragma once

#include <stddef.h>
#include <stdint.h>

#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIu64 "llu"
#define PRIx64 "llx"

#define PRId16 "d"
#define PRId32 "d"

#define PRIdMAX PRId64

#define PRIo32 "o"

#define PRIoMAX PRIo64

#define PRIu8 "u"
#define PRIu32 "u"

#define PRIuMAX PRIu64

#define PRIx32 "x"

#define SCNu64 PRIu64

#define SCNuMAX PRIu64

#if __SIZEOF_POINTER__ == 8
#define PRIxPTR PRIx64
#else
#define PRIxPTR "x"
#endif

#ifdef __cplusplus
extern "C" {
#endif

intmax_t strtoimax(const char* nptr, char** endptr, int base);
uintmax_t strtoumax(const char* nptr, char** endptr, int base);

#ifdef __cplusplus
}
#endif
