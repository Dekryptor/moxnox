#ifdef __cplusplus
extern "C" {
#endif

__attribute__((noreturn)) void _assert(const char* message, const char* file,
                                       unsigned line);

#ifdef __cplusplus
}
#endif

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) \
    ((expr) ? (void)0 : _assert(#expr, __FILE__, __LINE__))
#endif
