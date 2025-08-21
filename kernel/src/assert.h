#pragma once

#ifdef NDEBUG
#define assert(_Expression) ((void)0)
#else
#define assert(_Expression) \
    (void) \
    ((!!(_Expression)) || \
    (_assert(#_Expression,__FILE__,__LINE__),0))
#endif
