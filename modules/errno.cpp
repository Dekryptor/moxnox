export module errno;

import nt;

export {
    // these are the Linux values

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
    const int EAGAIN = 11; // as EWOULDBLOCK also 140 on Windows
    const int ENOMEM = 12;
    const int EACCES = 13;
    const int EFAULT = 14;
    const int ENOTBLK = 15;
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
    const int ETXTBSY = 26; // 139 on Windows
    const int EFBIG = 27;
    const int ENOSPC = 28;
    const int ESPIPE = 29;
    const int EROFS = 30;
    const int EMLINK = 31;
    const int EPIPE = 32;
    const int EDOM = 33;
    const int ERANGE = 34;
    const int EDEADLK = 35; // 36 on Windows
    const int ENAMETOOLONG = 36; // 38 on Windows
    const int ENOLCK = 37; // 39 on Windows
    const int ENOSYS = 38; // 40 on Windows
    const int ENOTEMPTY = 39; // 41 on Windows
    const int ELOOP = 40; // 114 on Windows
    const int ENOMSG = 42; // 122 on Windows
    const int EIDRM = 43; // 111 on Windows
    const int ECHRNG = 44;
    const int EL2NSYNC = 45;
    const int EL3HLT = 46;
    const int EL3RST = 47;
    const int ELNRNG = 48;
    const int EUNATCH = 49;
    const int ENOCSI = 50;
    const int EL2HLT = 51;
    const int EBADE = 52;
    const int EBADR = 53;
    const int EXFULL = 54;
    const int ENOANO = 55;
    const int EBADRQC = 56;
    const int EBADSLT = 57;
    const int EBFONT = 59;
    const int ENOSTR = 60; // 125 on Windows
    const int ENODATA = 61; // 120 on Windows
    const int ETIME = 62; // 137 on Windows
    const int ENOSR = 63; // 124 on Windows
    const int ENONET = 64;
    const int ENOPKG = 65;
    const int EREMOTE = 66;
    const int ENOLINK = 67; // 121 on Windows
    const int EADV = 68;
    const int ESRMNT = 69;
    const int ECOMM = 70;
    const int EPROTO = 71; // 134 on Windows
    const int EMULTIHOP = 72;
    const int EDOTDOT = 73;
    const int EBADMSG = 74; // 104 on Windows
    const int EOVERFLOW = 75; // 132 on Windows
    const int ENOTUNIQ = 76;
    const int EBADFD = 77;
    const int EREMCHG = 78;
    const int ELIBACC = 79;
    const int ELIBBAD = 80;
    const int ELIBSCN = 81;
    const int ELIBMAX = 82;
    const int ELIBEXEC = 83;
    const int EILSEQ = 84; // 42 on Windows
    const int ERESTART = 85;
    const int ESTRPIPE = 86;
    const int EUSERS = 87;
    const int ENOTSOCK = 88; // 128 on Windows
    const int EDESTADDRREQ = 89; // 109 on Windows
    const int EMSGSIZE = 90; // 115 on Windows
    const int EPROTOTYPE = 91; // 136 on Windows
    const int ENOPROTOOPT = 92; // 123 on Windows
    const int EPROTONOSUPPORT = 93; // 135 on Windows
    const int ESOCKTNOSUPPORT = 94;
    const int EOPNOTSUPP = 95; // 130 on Windows (or 129 as ENOTSUP)
    const int EPFNOSUPPORT = 96;
    const int EAFNOSUPPORT = 97; // 102 on Windows
    const int EADDRINUSE = 98; // 100 on Windows
    const int EADDRNOTAVAIL = 99; // 101 on Windows
    const int ENETDOWN = 100; // 116 on Windows
    const int ENETUNREACH = 101; // 118 on Windows
    const int ENETRESET = 102; // 117 on Windows
    const int ECONNABORTED = 103; // 106 on Windows
    const int ECONNRESET = 104; // 108 on Windows
    const int ENOBUFS = 105; // 119 on Windows
    const int EISCONN = 106; // 113 on Windows
    const int ENOTCONN = 107; // 126 on Windows
    const int ESHUTDOWN = 108;
    const int ETOOMANYREFS = 109;
    const int ETIMEDOUT = 110; // 138 on Windows
    const int ECONNREFUSED = 111; // 107 on Windows
    const int EHOSTDOWN = 112;
    const int EHOSTUNREACH = 113; // 110 on Windows
    const int EALREADY = 114; // 103 on Windows
    const int EINPROGRESS = 115; // 112 on Windows
    const int ESTALE = 116;
    const int EUCLEAN = 117;
    const int ENOTNAM = 118;
    const int ENAVAIL = 119;
    const int EISNAM = 120;
    const int EREMOTEIO = 121;
    const int EDQUOT = 122;
    const int ENOMEDIUM = 123;
    const int EMEDIUMTYPE = 124;
    const int ECANCELED = 125; // 105 on Windows
    const int ENOKEY = 126;
    const int EKEYEXPIRED = 127;
    const int EKEYREVOKED = 128;
    const int EKEYREJECTED = 129;
    const int EOWNERDEAD = 130; // 133 on Windows
    const int ENOTRECOVERABLE = 131; // 127 on Windows
    const int ERFKILL = 132;
    const int EHWPOISON = 133;

    // on Windows but not Linux: EOTHER (131)
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
