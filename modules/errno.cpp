export module errno;

import nt;

export {
    const int EPERM = 1;
    const int ENOENT = 2;
    const int ESRCH = 3;
    const int EINTR = 4;
    const int EIO = 5;
    const int ENXIO = 6;
    const int E2BIG = 7;
    const int ENOEXEC = 8;
    const int EBADF = 9;
    const int ECHILD = 10;
    const int EAGAIN = 11;
    const int ENOMEM = 12;
    const int EACCES = 13;
    const int EFAULT = 14;
    const int EBUSY = 16;
    const int EEXIST = 17;
    const int EXDEV = 18;
    const int ENODEV = 19;
    const int ENOTDIR = 20;
    const int EISDIR = 21;
    const int EINVAL = 22;
    const int ENFILE = 23;
    const int EMFILE = 24;
    const int ENOTTY = 25;
    const int EFBIG = 27;
    const int ENOSPC = 28;
    const int ESPIPE = 29;
    const int EROFS = 30;
    const int EMLINK = 31;
    const int EPIPE = 32;
    const int EDOM = 33;
    const int ERANGE = 34;
    const int EDEADLK = 36;
    const int ENAMETOOLONG = 38;
    const int ENOLCK = 39;
    const int ENOSYS = 40;
    const int ENOTEMPTY = 41;
    const int EILSEQ = 42;
};

export int nt_status_to_errno(nt_status status) {
    switch (status) {
        case nt_status::ACCESS_DENIED:
        case nt_status::SHARING_VIOLATION:
            return EACCES;

        case nt_status::NO_MORE_FILES:
        case nt_status::OBJECT_NAME_NOT_FOUND:
        case nt_status::OBJECT_PATH_NOT_FOUND:
            return ENOENT;

        case nt_status::DISK_CORRUPT_ERROR:
        case nt_status::CRC_ERROR:
        case nt_status::IO_DEVICE_ERROR:
            return EIO;

        case nt_status::INVALID_HANDLE:
            return EBADF;

        case nt_status::INSUFFICIENT_RESOURCES:
        case nt_status::NO_MEMORY:
            return ENOMEM;

        case nt_status::DISK_FULL:
            return ENOSPC;

        default:
            return EINVAL;
    }
}
