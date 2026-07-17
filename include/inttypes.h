#pragma once

#include <stddef.h>
#include <stdint.h>

#define PRIx64 "llx"
#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIu64 "llu"
#define PRIX64 "llX"

#define PRIx32 "x"
#define PRId32 "d"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIu32 "u"
#define PRIX32 "X"

#define PRIx16 "x"
#define PRId16 "d"
#define PRIi16 "i"
#define PRIo16 "o"
#define PRIu16 "u"
#define PRIX16 "X"

#define PRIx8 "x"
#define PRId8 "d"
#define PRIi8 "i"
#define PRIo8 "o"
#define PRIu8 "u"
#define PRIX8 "X"

#define PRIxMAX PRIx64
#define PRIdMAX PRId64
#define PRIiMAX PRIi64
#define PRIoMAX PRIo64
#define PRIuMAX PRIu64
#define PRIXMAX PRIX64

#define SCNo64 PRIo64
#define SCNu64 PRIu64
#define SCNx64 PRIx64

#define SCNo32 PRIo32
#define SCNu32 PRIu32
#define SCNx32 PRIx32

#define SCNo16 "ho"
#define SCNu16 "hu"
#define SCNx16 "hx"

#define SCNo8 "hho"
#define SCNu8 "hhu"
#define SCNx8 "hhx"

#define SCNuMAX PRIu64

#if __SIZEOF_POINTER__ == 8
#define PRIxPTR PRIx64
#define PRIdPTR PRId64
#define PRIiPTR PRIi64
#define PRIoPTR PRIo64
#define PRIuPTR PRIu64
#define PRIXPTR PRIX64
#else
#define PRIxPTR "x"
#define PRIdPTR "d"
#define PRIiPTR "i"
#define PRIoPTR "o"
#define PRIuPTR "u"
#define PRIXPTR "X"
#endif

typedef struct {
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

#ifdef __cplusplus
extern "C" {
#endif

intmax_t imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);
intmax_t strtoimax(const char* nptr, char** endptr, int base);
uintmax_t strtoumax(const char* nptr, char** endptr, int base);
intmax_t wcstoimax(const wchar_t* nptr, wchar_t** endptr, int base);
uintmax_t wcstoumax(const wchar_t* nptr, wchar_t** endptr, int base);

#ifdef __cplusplus
}
#endif
