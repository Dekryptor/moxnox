// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
/* Minimal endian.h for compiling musl math sources with MinGW toolchain.
 * MinGW lacks endian.h; libm.h uses __BYTE_ORDER/__LITTLE_ENDIAN/__BIG_ENDIAN.
 * GCC/Clang provide __BYTE_ORDER__ and __ORDER_*_ENDIAN__ as predefined macros. */

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __PDP_ENDIAN    3412

#if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define __BYTE_ORDER __LITTLE_ENDIAN
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define __BYTE_ORDER __BIG_ENDIAN
#  endif
#else
#  define __BYTE_ORDER __LITTLE_ENDIAN
#endif
