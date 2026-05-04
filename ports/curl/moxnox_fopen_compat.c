#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

FILE* curlx_win32_fopen(const char* filename, const char* mode) {
    return fopen(filename, mode);
}

FILE* curlx_win32_freopen(const char* filename, const char* mode, FILE* fp) {
    return freopen(filename, mode, fp);
}

/* _stati64 is a macro for _stat64 in MinGW (non-_USE_32BIT_TIME_T builds) */
int curlx_win32_stat(const char* path, struct _stat64* buffer) {
    return _stat64(path, buffer);
}

int curlx_win32_open(const char* filename, int oflag, ...) {
    va_list ap;
    int pmode = 0;

    va_start(ap, oflag);

    if(oflag & O_CREAT)
        pmode = va_arg(ap, int);

    va_end(ap);

    return open(filename, oflag, pmode);
}

int curlx_win32_rename(const char* oldpath, const char* newpath) {
    return rename(oldpath, newpath);
}
