#include <stdint.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "repo/lib/tre-internal.h"

#undef ALIGN
#define ALIGN(ptr, type) \
  ((((uintptr_t)(ptr)) % sizeof(type)) \
   ? (sizeof(type) - (((uintptr_t)(ptr)) % sizeof(type))) \
   : 0)
