#pragma once

/* Atomic primitives: use C11 atomics (GCC with -std=gnu11) */
#define HAVE_C11_ATOMIC_PRIMITIVES 1

/* Standard headers available */
#define HAVE_ALLOCA_H    1
#define HAVE_STDINT_H    1
#define HAVE_INTTYPES_H  1
#define HAVE_SIGNAL_H    1
#define HAVE_SYS_STAT_H  1
#define HAVE_IO_H        1
#define HAVE_TIME_H      1
#define HAVE_SETJMP_H    1
#define HAVE_FENV_H      1
#define HAVE_TERMIOS_H   1

/* Library availability */
#define HAVE_ZLIB 1

/* Functions */
/* HAVE_STRNDUP not set -- cairo has a built-in fallback */
#define HAVE_RAISE 1

/* uint64_t is available (MinGW/stdint.h) */
#define HAVE_UINT64_T 1

/* pixman features: we have pixman 0.46.4 which is >= 0.42.3 */
#define HAS_PIXMAN_GLYPHS       1
#define HAS_PIXMAN_r8g8b8_sRGB  1

/* FreeType 2.14.1 features (all require >= 2.13) */
#define HAVE_FT_SVG_DOCUMENT  1
#define HAVE_FT_LOAD_NO_SVG   1
#define HAVE_FT_COLR_V1       1

/* No pthreads -- cairo will use single-threaded (nil mutex) mode */
/* CAIRO_HAS_PTHREAD not set */

/* Package info */
#define PACKAGE_NAME    "cairo"
#define PACKAGE_VERSION "1.18.4"
#define PACKAGE_BUGREPORT "https://gitlab.freedesktop.org/cairo/cairo/-/issues"
