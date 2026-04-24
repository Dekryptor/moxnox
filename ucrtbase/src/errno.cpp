#include <string.h>

import print;
import ucrtbase;
import errno;

static thread_local int errno = 0;
static thread_local char strerror_buf[256];

extern "C"
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler /*handler*/) {
    // FIXME?

    return nullptr;
}

extern "C"
int* __errno() {
    return &errno;
}

extern "C"
char* __strerror(int errnum) {
    switch (errnum) {
        case 0:
            strcpy(strerror_buf, "Success");
            break;

        case EPERM:
            strcpy(strerror_buf, "Operation not permitted");
            break;

        case ENOENT:
            strcpy(strerror_buf, "No such file or directory");
            break;

        case ESRCH:
            strcpy(strerror_buf, "No such process");
            break;

        case EINTR:
            strcpy(strerror_buf, "Interrupted system call");
            break;

        case EIO:
            strcpy(strerror_buf, "Input/output error");
            break;

        case ENXIO:
            strcpy(strerror_buf, "No such device or address");
            break;

        case E2BIG:
            strcpy(strerror_buf, "Argument list too long");
            break;

        case ENOEXEC:
            strcpy(strerror_buf, "Exec format error");
            break;

        case EBADF:
            strcpy(strerror_buf, "Bad file descriptor");
            break;

        case ECHILD:
            strcpy(strerror_buf, "No child processes");
            break;

        case EAGAIN:
            strcpy(strerror_buf, "Resource temporarily unavailable");
            break;

        case ENOMEM:
            strcpy(strerror_buf, "Cannot allocate memory");
            break;

        case EACCES:
            strcpy(strerror_buf, "Permission denied");
            break;

        case EFAULT:
            strcpy(strerror_buf, "Bad address");
            break;

        case EBUSY:
            strcpy(strerror_buf, "Device or resource busy");
            break;

        case EEXIST:
            strcpy(strerror_buf, "File exists");
            break;

        case EXDEV:
            strcpy(strerror_buf, "Invalid cross-device link");
            break;

        case ENODEV:
            strcpy(strerror_buf, "No such device");
            break;

        case ENOTDIR:
            strcpy(strerror_buf, "Not a directory");
            break;

        case EISDIR:
            strcpy(strerror_buf, "Is a directory");
            break;

        case EINVAL:
            strcpy(strerror_buf, "Invalid argument");
            break;

        case ENFILE:
            strcpy(strerror_buf, "Too many open files in system");
            break;

        case EMFILE:
            strcpy(strerror_buf, "Too many open files");
            break;

        case ENOTTY:
            strcpy(strerror_buf, "Inappropriate ioctl for device");
            break;

        case EFBIG:
            strcpy(strerror_buf, "File too large");
            break;

        case ENOSPC:
            strcpy(strerror_buf, "No space left on device");
            break;

        case ESPIPE:
            strcpy(strerror_buf, "Illegal seek");
            break;

        case EROFS:
            strcpy(strerror_buf, "Read-only file system");
            break;

        case EMLINK:
            strcpy(strerror_buf, "Too many links");
            break;

        case EPIPE:
            strcpy(strerror_buf, "Broken pipe");
            break;

        case EDOM:
            strcpy(strerror_buf, "Numerical argument out of domain");
            break;

        case ERANGE:
            strcpy(strerror_buf, "Numerical result out of range");
            break;

        case EDEADLK:
            strcpy(strerror_buf, "Resource deadlock avoided");
            break;

        case ENAMETOOLONG:
            strcpy(strerror_buf, "File name too long");
            break;

        case ENOLCK:
            strcpy(strerror_buf, "No locks available");
            break;

        case ENOSYS:
            strcpy(strerror_buf, "Function not implemented");
            break;

        case ENOTEMPTY:
            strcpy(strerror_buf, "Directory not empty");
            break;

        case EILSEQ:
            strcpy(strerror_buf, "Invalid or incomplete multibyte or wide character");
            break;

        default:
            strcpy(strerror_buf, "Unknown error");
            break;
    }

    return strerror_buf;
}
