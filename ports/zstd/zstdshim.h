// skip platform.h - we provide everything it would give us here
#define PLATFORM_H_MODULE

#ifdef _WIN32 // FIXME
#define PLATFORM_POSIX_VERSION 0
#else
#define PLATFORM_POSIX_VERSION 202405L
#endif

#define IS_CONSOLE(stream) 0

// FIXME HACK
// (HANDLE is used in fileio_types.h inside #ifdef _WIN32, but non-trivial to patch out)
#ifndef HANDLE
typedef void* HANDLE;
#endif

#define SET_BINARY_MODE(file) ((void)(file))
#define SET_SPARSE_FILE_MODE(file) ((void)(file))

#ifndef ZSTD_SPARSE_DEFAULT
#  define ZSTD_SPARSE_DEFAULT 1
#endif
