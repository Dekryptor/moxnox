module;

#include <string_view>
#undef errno

export module errno;

import nt;

using namespace std;

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

        case nt_status::DEVICE_NOT_READY:
        case nt_status::CANT_WAIT:
            return EAGAIN;

        case nt_status::CONNECTION_RESET:
            return ECONNRESET;

        case nt_status::HOST_UNREACHABLE:
            return EHOSTUNREACH;

        case nt_status::CONNECTION_ACTIVE:
            return EISCONN;

        case nt_status::BUFFER_OVERFLOW:
        case nt_status::INVALID_BUFFER_SIZE:
            return EMSGSIZE;

        case nt_status::NETWORK_UNREACHABLE:
            return ENETDOWN;

        case nt_status::CONNECTION_DISCONNECTED:
            return ENOTCONN;

        case nt_status::PIPE_BROKEN:
        case nt_status::CONNECTION_ABORTED:
            return EPIPE;

        case nt_status::ACCESS_VIOLATION:
            return EFAULT;

        case nt_status::ADDRESS_ALREADY_ASSOCIATED:
            return EADDRINUSE;

        default:
            return EINVAL;
    }
}

export string_view errno_to_string_view(int err) {
    switch (err) {
        case EPERM:
            return "EPERM";
        case ENOENT:
            return "ENOENT";
        case ESRCH:
            return "ESRCH";
        case EINTR:
            return "EINTR";
        case EIO:
            return "EIO";
        case ENXIO:
            return "ENXIO";
        case E2BIG:
            return "E2BIG";
        case ENOEXEC:
            return "ENOEXEC";
        case EBADF:
            return "EBADF";
        case ECHILD:
            return "ECHILD";
        case EAGAIN:
            return "EAGAIN";
        case ENOMEM:
            return "ENOMEM";
        case EACCES:
            return "EACCES";
        case EFAULT:
            return "EFAULT";
        case ENOTBLK:
            return "ENOTBLK";
        case EBUSY:
            return "EBUSY";
        case EEXIST:
            return "EEXIST";
        case EXDEV:
            return "EXDEV";
        case ENODEV:
            return "ENODEV";
        case ENOTDIR:
            return "ENOTDIR";
        case EISDIR:
            return "EISDIR";
        case EINVAL:
            return "EINVAL";
        case ENFILE:
            return "ENFILE";
        case EMFILE:
            return "EMFILE";
        case ENOTTY:
            return "ENOTTY";
        case ETXTBSY:
            return "ETXTBSY";
        case EFBIG:
            return "EFBIG";
        case ENOSPC:
            return "ENOSPC";
        case ESPIPE:
            return "ESPIPE";
        case EROFS:
            return "EROFS";
        case EMLINK:
            return "EMLINK";
        case EPIPE:
            return "EPIPE";
        case EDOM:
            return "EDOM";
        case ERANGE:
            return "ERANGE";
        case EDEADLK:
            return "EDEADLK";
        case ENAMETOOLONG:
            return "ENAMETOOLONG";
        case ENOLCK:
            return "ENOLCK";
        case ENOSYS:
            return "ENOSYS";
        case ENOTEMPTY:
            return "ENOTEMPTY";
        case ELOOP:
            return "ELOOP";
        case ENOMSG:
            return "ENOMSG";
        case EIDRM:
            return "EIDRM";
        case ECHRNG:
            return "ECHRNG";
        case EL2NSYNC:
            return "EL2NSYNC";
        case EL3HLT:
            return "EL3HLT";
        case EL3RST:
            return "EL3RST";
        case ELNRNG:
            return "ELNRNG";
        case EUNATCH:
            return "EUNATCH";
        case ENOCSI:
            return "ENOCSI";
        case EL2HLT:
            return "EL2HLT";
        case EBADE:
            return "EBADE";
        case EBADR:
            return "EBADR";
        case EXFULL:
            return "EXFULL";
        case ENOANO:
            return "ENOANO";
        case EBADRQC:
            return "EBADRQC";
        case EBADSLT:
            return "EBADSLT";
        case EBFONT:
            return "EBFONT";
        case ENOSTR:
            return "ENOSTR";
        case ENODATA:
            return "ENODATA";
        case ETIME:
            return "ETIME";
        case ENOSR:
            return "ENOSR";
        case ENONET:
            return "ENONET";
        case ENOPKG:
            return "ENOPKG";
        case EREMOTE:
            return "EREMOTE";
        case ENOLINK:
            return "ENOLINK";
        case EADV:
            return "EADV";
        case ESRMNT:
            return "ESRMNT";
        case ECOMM:
            return "ECOMM";
        case EPROTO:
            return "EPROTO";
        case EMULTIHOP:
            return "EMULTIHOP";
        case EDOTDOT:
            return "EDOTDOT";
        case EBADMSG:
            return "EBADMSG";
        case EOVERFLOW:
            return "EOVERFLOW";
        case ENOTUNIQ:
            return "ENOTUNIQ";
        case EBADFD:
            return "EBADFD";
        case EREMCHG:
            return "EREMCHG";
        case ELIBACC:
            return "ELIBACC";
        case ELIBBAD:
            return "ELIBBAD";
        case ELIBSCN:
            return "ELIBSCN";
        case ELIBMAX:
            return "ELIBMAX";
        case ELIBEXEC:
            return "ELIBEXEC";
        case EILSEQ:
            return "EILSEQ";
        case ERESTART:
            return "ERESTART";
        case ESTRPIPE:
            return "ESTRPIPE";
        case EUSERS:
            return "EUSERS";
        case ENOTSOCK:
            return "ENOTSOCK";
        case EDESTADDRREQ:
            return "EDESTADDRREQ";
        case EMSGSIZE:
            return "EMSGSIZE";
        case EPROTOTYPE:
            return "EPROTOTYPE";
        case ENOPROTOOPT:
            return "ENOPROTOOPT";
        case EPROTONOSUPPORT:
            return "EPROTONOSUPPORT";
        case ESOCKTNOSUPPORT:
            return "ESOCKTNOSUPPORT";
        case EOPNOTSUPP:
            return "EOPNOTSUPP";
        case EPFNOSUPPORT:
            return "EPFNOSUPPORT";
        case EAFNOSUPPORT:
            return "EAFNOSUPPORT";
        case EADDRINUSE:
            return "EADDRINUSE";
        case EADDRNOTAVAIL:
            return "EADDRNOTAVAIL";
        case ENETDOWN:
            return "ENETDOWN";
        case ENETUNREACH:
            return "ENETUNREACH";
        case ENETRESET:
            return "ENETRESET";
        case ECONNABORTED:
            return "ECONNABORTED";
        case ECONNRESET:
            return "ECONNRESET";
        case ENOBUFS:
            return "ENOBUFS";
        case EISCONN:
            return "EISCONN";
        case ENOTCONN:
            return "ENOTCONN";
        case ESHUTDOWN:
            return "ESHUTDOWN";
        case ETOOMANYREFS:
            return "ETOOMANYREFS";
        case ETIMEDOUT:
            return "ETIMEDOUT";
        case ECONNREFUSED:
            return "ECONNREFUSED";
        case EHOSTDOWN:
            return "EHOSTDOWN";
        case EHOSTUNREACH:
            return "EHOSTUNREACH";
        case EALREADY:
            return "EALREADY";
        case EINPROGRESS:
            return "EINPROGRESS";
        case ESTALE:
            return "ESTALE";
        case EUCLEAN:
            return "EUCLEAN";
        case ENOTNAM:
            return "ENOTNAM";
        case ENAVAIL:
            return "ENAVAIL";
        case EISNAM:
            return "EISNAM";
        case EREMOTEIO:
            return "EREMOTEIO";
        case EDQUOT:
            return "EDQUOT";
        case ENOMEDIUM:
            return "ENOMEDIUM";
        case EMEDIUMTYPE:
            return "EMEDIUMTYPE";
        case ECANCELED:
            return "ECANCELED";
        case ENOKEY:
            return "ENOKEY";
        case EKEYEXPIRED:
            return "EKEYEXPIRED";
        case EKEYREVOKED:
            return "EKEYREVOKED";
        case EKEYREJECTED:
            return "EKEYREJECTED";
        case EOWNERDEAD:
            return "EOWNERDEAD";
        case ENOTRECOVERABLE:
            return "ENOTRECOVERABLE";
        case ERFKILL:
            return "ERFKILL";
        case EHWPOISON:
            return "EHWPOISON";
        default:
            return "?";
    }
}
