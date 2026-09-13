// skip platform.h - we provide everything it would give us here
#define PLATFORM_H_MODULE

#define PLATFORM_POSIX_VERSION 202405L

#define IS_CONSOLE(stream) 0

#define SET_BINARY_MODE(file) ((void)(file))
#define SET_SPARSE_FILE_MODE(file) ((void)(file))

#ifndef ZSTD_SPARSE_DEFAULT
#  define ZSTD_SPARSE_DEFAULT 1
#endif
