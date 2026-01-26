module;

#include <span>
#include <string_view>
#include <source_location>
#include <chrono>
#include <optional>
#include <stdint.h>

export module format;

import nt;

using namespace std;

export template<unsigned_integral T>
class byte_size {
public:
    byte_size(T val) : val(val) { }

    static constexpr bool is_byte_size = true;

    T val;
};

span<char> sp_strcpy(span<char> dest, string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy);

    return span(dest.data() + to_copy, dest.size() - to_copy);
}

span<char> sp_itoa(span<char> dest, uint64_t num, unsigned int padding = 0, bool zero_pad = false) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        if (padding == 0) {
            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        } else if (!zero_pad) {
            for (unsigned int i = 0; i < padding - 1; i++) {
                dest[0] = ' ';
                dest = span(dest.data() + 1, dest.size() - 1);

                if (dest.empty())
                    return dest;
            }

            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        }
    }

    uint64_t place = 10000000000000000000ull;

#ifndef __i386 // FIXME
    for (unsigned int i = 0; i < 20; i++) {
        if (!first || padding >= 20 - i || num >= place) {
            uint64_t digit = num / place;

            if (digit == 0 && first && !zero_pad)
                dest[0] = ' ';
            else
                dest[0] = (char)(digit + '0');

            if (first && digit != 0)
                first = false;

            dest = span(dest.data() + 1, dest.size() - 1);

            if (dest.empty())
                return dest;

            num %= place;
        }

        place /= 10;
    }
#endif

    return dest;
}

span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding = 0, bool zero_pad = false) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        if (padding == 0) {
            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        } else if (!zero_pad) {
            for (unsigned int i = 0; i < padding - 1; i++) {
                dest[0] = ' ';
                dest = span(dest.data() + 1, dest.size() - 1);

                if (dest.empty())
                    return dest;
            }

            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        }
    }

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding >= 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n == 0 && first && !zero_pad)
                dest[0] = ' ';
            else if (n <= 9)
                dest[0] = (char)(n + '0');
            else
                dest[0] = (char)(n - 0xa + 'a');

            if (first && n != 0)
                first = false;

            dest = span(dest.data() + 1, dest.size() - 1);

            if (dest.empty())
                return dest;
        }

        num <<= 4;
    }

    return dest;
}

span<char> sp_itoa_oct(span<char> dest, uint64_t num, unsigned int padding = 0, bool zero_pad = false) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        if (padding == 0) {
            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        } else if (!zero_pad) {
            for (unsigned int i = 0; i < padding - 1; i++) {
                dest[0] = ' ';
                dest = span(dest.data() + 1, dest.size() - 1);

                if (dest.empty())
                    return dest;
            }

            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        }
    }

    if (num & 0x8000000000000000) {
        dest[0] = '1';
        dest = span(dest.data() + 1, dest.size() - 1);
        first = false;

        if (dest.empty())
            return dest;
    }

    for (unsigned int i = 0; i < 21; i++) {
        if (!first || padding >= 21 - i || num & 0x7000000000000000) {
            uint64_t n = (num & 0x7000000000000000) >> 60;

            if (n == 0 && first && !zero_pad)
                dest[0] = ' ';
            else
                dest[0] = (char)(n + '0');

            if (first && n != 0)
                first = false;

            dest = span(dest.data() + 1, dest.size() - 1);

            if (dest.empty())
                return dest;
        }

        num <<= 3;
    }

    return dest;
}

export template<typename T>
consteval bool format_spec_valid(string_view) {
    return false;
}

export span<char> do_format_arg(span<char> sp, string_view fmt, integral auto arg, optional<uint64_t> arg2 = nullopt) {
    enum class int_format_base {
        dec,
        hex,
        oct
    };

    auto base = int_format_base::dec;

    if (!fmt.empty() && fmt.back() == 'x') {
        base = int_format_base::hex;
        fmt = string_view(fmt.data(), fmt.size() - 1);
    } else if (!fmt.empty() && fmt.back() == 'o') {
        base = int_format_base::oct;
        fmt = string_view(fmt.data(), fmt.size() - 1);
    }

    unsigned int padding;
    bool zero_pad = false;

    if (arg2.has_value())
        padding = (unsigned int)*arg2;
    else {
        padding = 0;

        if (fmt.size() >= 2 && fmt.front() == ':') {
            for (size_t i = 1; i < fmt.size(); i++) {
                if (i == 1 && fmt[i] == '0')
                    zero_pad = true;

                padding *= 10;
                padding += (unsigned int)(fmt[i] - '0');
            }
        }
    }

    if (base == int_format_base::hex) {
        if constexpr (is_signed_v<decltype(arg)>) {
            if (arg < 0) {
                sp = sp_strcpy(sp, "-");
                return sp_itoa_hex(sp, (uint64_t)-arg, padding > 0 ? (padding - 1) : 0, zero_pad);
            }
        }

        return sp_itoa_hex(sp, (uint64_t)arg, padding, zero_pad);
    } else if (base == int_format_base::oct) {
        if constexpr (is_signed_v<decltype(arg)>) {
            if (arg < 0) {
                sp = sp_strcpy(sp, "-");
                return sp_itoa_oct(sp, (uint64_t)-arg, padding > 0 ? (padding - 1) : 0, zero_pad);
            }
        }

        return sp_itoa_oct(sp, (uint64_t)arg, padding, zero_pad);
    } else {
        if constexpr (is_signed_v<decltype(arg)>) {
            if (arg < 0) {
                sp = sp_strcpy(sp, "-");
                return sp_itoa(sp, (uint64_t)-arg, padding > 0 ? (padding - 1) : 0, zero_pad);
            }
        }

        return sp_itoa(sp, (uint64_t)arg, padding, zero_pad);
    }
}

template<typename T>
requires (integral<T>)
consteval bool format_spec_valid(string_view fmt) {
    if (fmt.empty())
        return true;

    if (fmt.size() >= 2 && fmt.front() == ':') {
        if (fmt.back() == 'x' || fmt.back() == 'o')
            fmt = string_view(fmt.data(), fmt.size() - 1);

        if (fmt.size() == 3 && fmt[1] == '{' && fmt[2] == '}')
            return true;

        for (size_t i = 1; i < fmt.size(); i++) {
            if (fmt[i] < '0' || fmt[i] > '9')
                return false;
        }

        return true;
    }

    return false;
}

export template<typename T>
requires (unsigned_integral<T>)
span<char> do_format_arg(span<char> sp, string_view, byte_size<T> arg) {
    auto val = (uint64_t)arg.val;

    static const uint16_t decimals[] = {
        0x33, // 0.05 * 1024
        0x99, // 0.15 * 1024
        0x100, // 0.25 * 1024
        0x166, // 0.35 * 1024
        0x1cc, // 0.45 * 1024
        0x233, // 0.55 * 1024
        0x299, // 0.65 * 1024
        0x300, // 0.75 * 1024
        0x366, // 0.85 * 1024
        0x3cc // 0.95 * 1024
    };

    if (val < 1024) {
        sp = sp_itoa(sp, (uint64_t)arg.val);
        sp = sp_strcpy(sp, " bytes");
        return sp;
    }

    unsigned int shift;
    string_view suffix;

    if (val < 1 << 20) {
        shift = 10;
        suffix = " KB";
    } else if (val < 1 << 30) {
        shift = 20;
        suffix = " MB";
    } else if (val < (uint64_t)1 << 40) {
        shift = 30;
        suffix = " GB";
    } else if (val < (uint64_t)1 << 50) {
        shift = 40;
        suffix = " TB";
    } else if (val < (uint64_t)1 << 60) {
        shift = 50;
        suffix = " PB";
    } else {
        shift = 60;
        suffix = " EB";
    }

    auto decimal = (val >> (shift - 10)) & 0x3ff;

    sp = sp_itoa(sp, (val >> shift) + (decimal >= decimals[9] ? 1 : 0));
    sp = sp_strcpy(sp, ".");

    if (decimal >= decimals[9] || decimal < decimals[0])
        sp = sp_strcpy(sp, "0");
    else if (decimal >= decimals[8])
        sp = sp_strcpy(sp, "9");
    else if (decimal >= decimals[7])
        sp = sp_strcpy(sp, "8");
    else if (decimal >= decimals[6])
        sp = sp_strcpy(sp, "7");
    else if (decimal >= decimals[5])
        sp = sp_strcpy(sp, "6");
    else if (decimal >= decimals[4])
        sp = sp_strcpy(sp, "5");
    else if (decimal >= decimals[3])
        sp = sp_strcpy(sp, "4");
    else if (decimal >= decimals[2])
        sp = sp_strcpy(sp, "3");
    else if (decimal >= decimals[1])
        sp = sp_strcpy(sp, "2");
    else
        sp = sp_strcpy(sp, "1");

    sp = sp_strcpy(sp, suffix);

    return sp;
}

template<typename T>
requires (T::is_byte_size)
consteval bool format_spec_valid(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, nt_status arg) {
    switch (arg) {
        case nt_status::SUCCESS:
            return sp_strcpy(sp, "SUCCESS");
        case nt_status::ALERTED:
            return sp_strcpy(sp, "ALERTED");
        case nt_status::TIMEOUT:
            return sp_strcpy(sp, "TIMEOUT");
        case nt_status::PENDING:
            return sp_strcpy(sp, "PENDING");
        case nt_status::BREAKPOINT:
            return sp_strcpy(sp, "BREAKPOINT");
        case nt_status::SINGLE_STEP:
            return sp_strcpy(sp, "SINGLE_STEP");
        case nt_status::BUFFER_OVERFLOW:
            return sp_strcpy(sp, "BUFFER_OVERFLOW");
        case nt_status::NO_MORE_FILES:
            return sp_strcpy(sp, "NO_MORE_FILES");
        case nt_status::NO_MORE_ENTRIES:
            return sp_strcpy(sp, "NO_MORE_ENTRIES");
        case nt_status::NOT_IMPLEMENTED:
            return sp_strcpy(sp, "NOT_IMPLEMENTED");
        case nt_status::INVALID_INFO_CLASS:
            return sp_strcpy(sp, "INVALID_INFO_CLASS");
        case nt_status::INFO_LENGTH_MISMATCH:
            return sp_strcpy(sp, "INFO_LENGTH_MISMATCH");
        case nt_status::ACCESS_VIOLATION:
            return sp_strcpy(sp, "ACCESS_VIOLATION");
        case nt_status::INVALID_HANDLE:
            return sp_strcpy(sp, "INVALID_HANDLE");
        case nt_status::INVALID_CID:
            return sp_strcpy(sp, "INVALID_CID");
        case nt_status::INVALID_PARAMETER:
            return sp_strcpy(sp, "INVALID_PARAMETER");
        case nt_status::END_OF_FILE:
            return sp_strcpy(sp, "END_OF_FILE");
        case nt_status::NO_MEMORY:
            return sp_strcpy(sp, "NO_MEMORY");
        case nt_status::ILLEGAL_INSTRUCTION:
            return sp_strcpy(sp, "ILLEGAL_INSTRUCTION");
        case nt_status::ACCESS_DENIED:
            return sp_strcpy(sp, "ACCESS_DENIED");
        case nt_status::BUFFER_TOO_SMALL:
            return sp_strcpy(sp, "BUFFER_TOO_SMALL");
        case nt_status::OBJECT_TYPE_MISMATCH:
            return sp_strcpy(sp, "OBJECT_TYPE_MISMATCH");
        case nt_status::INVALID_DISPOSITION:
            return sp_strcpy(sp, "INVALID_DISPOSITION");
        case nt_status::BAD_STACK:
            return sp_strcpy(sp, "BAD_STACK");
        case nt_status::DISK_CORRUPT_ERROR:
            return sp_strcpy(sp, "DISK_CORRUPT_ERROR");
        case nt_status::OBJECT_NAME_INVALID:
            return sp_strcpy(sp, "OBJECT_NAME_INVALID");
        case nt_status::OBJECT_NAME_NOT_FOUND:
            return sp_strcpy(sp, "OBJECT_NAME_NOT_FOUND");
        case nt_status::OBJECT_NAME_COLLISION:
            return sp_strcpy(sp, "OBJECT_NAME_COLLISION");
        case nt_status::OBJECT_PATH_NOT_FOUND:
            return sp_strcpy(sp, "OBJECT_PATH_NOT_FOUND");
        case nt_status::OBJECT_PATH_SYNTAX_BAD:
            return sp_strcpy(sp, "OBJECT_PATH_SYNTAX_BAD");
        case nt_status::CRC_ERROR:
            return sp_strcpy(sp, "CRC_ERROR");
        case nt_status::SHARING_VIOLATION:
            return sp_strcpy(sp, "SHARING_VIOLATION");
        case nt_status::INVALID_PAGE_PROTECTION:
            return sp_strcpy(sp, "INVALID_PAGE_PROTECTION");
        case nt_status::THREAD_IS_TERMINATING:
            return sp_strcpy(sp, "THREAD_IS_TERMINATING");
        case nt_status::PROCEDURE_NOT_FOUND:
            return sp_strcpy(sp, "PROCEDURE_NOT_FOUND");
        case nt_status::INVALID_IMAGE_FORMAT:
            return sp_strcpy(sp, "INVALID_IMAGE_FORMAT");
        case nt_status::DISK_FULL:
            return sp_strcpy(sp, "DISK_FULL");
        case nt_status::INTEGER_DIVIDE_BY_ZERO:
            return sp_strcpy(sp, "INTEGER_DIVIDE_BY_ZERO");
        case nt_status::INSUFFICIENT_RESOURCES:
            return sp_strcpy(sp, "INSUFFICIENT_RESOURCES");
        case nt_status::MEDIA_WRITE_PROTECTED:
            return sp_strcpy(sp, "MEDIA_WRITE_PROTECTED");
        case nt_status::PIPE_DISCONNECTED:
            return sp_strcpy(sp, "PIPE_DISCONNECTED");
        case nt_status::IO_TIMEOUT:
            return sp_strcpy(sp, "IO_TIMEOUT");
        case nt_status::FILE_IS_A_DIRECTORY:
            return sp_strcpy(sp, "FILE_IS_A_DIRECTORY");
        case nt_status::NOT_SUPPORTED:
            return sp_strcpy(sp, "NOT_SUPPORTED");
        case nt_status::CANT_WAIT:
            return sp_strcpy(sp, "CANT_WAIT");
        case nt_status::INTERNAL_ERROR:
            return sp_strcpy(sp, "INTERNAL_ERROR");
        case nt_status::DIRECTORY_NOT_EMPTY:
            return sp_strcpy(sp, "DIRECTORY_NOT_EMPTY");
        case nt_status::NOT_A_DIRECTORY:
            return sp_strcpy(sp, "NOT_A_DIRECTORY");
        case nt_status::PROCESS_IS_TERMINATING:
            return sp_strcpy(sp, "PROCESS_IS_TERMINATING");
        case nt_status::DLL_NOT_FOUND:
            return sp_strcpy(sp, "DLL_NOT_FOUND");
        case nt_status::ORDINAL_NOT_FOUND:
            return sp_strcpy(sp, "ORDINAL_NOT_FOUND");
        case nt_status::CONTROL_C_EXIT:
            return sp_strcpy(sp, "CONTROL_C_EXIT");
        case nt_status::DLL_INIT_FAILED:
            return sp_strcpy(sp, "DLL_INIT_FAILED");
        case nt_status::UNHANDLED_EXCEPTION:
            return sp_strcpy(sp, "UNHANDLED_EXCEPTION");
        case nt_status::IO_DEVICE_ERROR:
            return sp_strcpy(sp, "IO_DEVICE_ERROR");
        case nt_status::NOT_FOUND:
            return sp_strcpy(sp, "NOT_FOUND");
        case nt_status::ILLEGAL_DLL_RELOCATION:
            return sp_strcpy(sp, "ILLEGAL_DLL_RELOCATION");
        case nt_status::DEVICE_REMOVED:
            return sp_strcpy(sp, "DEVICE_REMOVED");
        case nt_status::SYSTEM_SHUTDOWN:
            return sp_strcpy(sp, "SYSTEM_SHUTDOWN");
        default:
            return do_format_arg(sp, ":08x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<nt_status>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, FILE_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum FILE_INFORMATION_CLASS;

        case DirectoryInformation:
            return sp_strcpy(sp, "DirectoryInformation");
        case FullDirectoryInformation:
            return sp_strcpy(sp, "FullDirectoryInformation");
        case BothDirectoryInformation:
            return sp_strcpy(sp, "BothDirectoryInformation");
        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case StandardInformation:
            return sp_strcpy(sp, "StandardInformation");
        case InternalInformation:
            return sp_strcpy(sp, "InternalInformation");
        case EaInformation:
            return sp_strcpy(sp, "EaInformation");
        case AccessInformation:
            return sp_strcpy(sp, "AccessInformation");
        case NameInformation:
            return sp_strcpy(sp, "NameInformation");
        case RenameInformation:
            return sp_strcpy(sp, "RenameInformation");
        case LinkInformation:
            return sp_strcpy(sp, "LinkInformation");
        case NamesInformation:
            return sp_strcpy(sp, "NamesInformation");
        case DispositionInformation:
            return sp_strcpy(sp, "DispositionInformation");
        case PositionInformation:
            return sp_strcpy(sp, "PositionInformation");
        case FullEaInformation:
            return sp_strcpy(sp, "FullEaInformation");
        case ModeInformation:
            return sp_strcpy(sp, "ModeInformation");
        case AlignmentInformation:
            return sp_strcpy(sp, "AlignmentInformation");
        case AllInformation:
            return sp_strcpy(sp, "AllInformation");
        case AllocationInformation:
            return sp_strcpy(sp, "AllocationInformation");
        case EndOfFileInformation:
            return sp_strcpy(sp, "EndOfFileInformation");
        case AlternateNameInformation:
            return sp_strcpy(sp, "AlternateNameInformation");
        case StreamInformation:
            return sp_strcpy(sp, "StreamInformation");
        case PipeInformation:
            return sp_strcpy(sp, "PipeInformation");
        case PipeLocalInformation:
            return sp_strcpy(sp, "PipeLocalInformation");
        case PipeRemoteInformation:
            return sp_strcpy(sp, "PipeRemoteInformation");
        case MailslotQueryInformation:
            return sp_strcpy(sp, "MailslotQueryInformation");
        case MailslotSetInformation:
            return sp_strcpy(sp, "MailslotSetInformation");
        case CompressionInformation:
            return sp_strcpy(sp, "CompressionInformation");
        case ObjectIdInformation:
            return sp_strcpy(sp, "ObjectIdInformation");
        case CompletionInformation:
            return sp_strcpy(sp, "CompletionInformation");
        case MoveClusterInformation:
            return sp_strcpy(sp, "MoveClusterInformation");
        case QuotaInformation:
            return sp_strcpy(sp, "QuotaInformation");
        case ReparsePointInformation:
            return sp_strcpy(sp, "ReparsePointInformation");
        case NetworkOpenInformation:
            return sp_strcpy(sp, "NetworkOpenInformation");
        case AttributeTagInformation:
            return sp_strcpy(sp, "AttributeTagInformation");
        case TrackingInformation:
            return sp_strcpy(sp, "TrackingInformation");
        case IdBothDirectoryInformation:
            return sp_strcpy(sp, "IdBothDirectoryInformation");
        case IdFullDirectoryInformation:
            return sp_strcpy(sp, "IdFullDirectoryInformation");
        case ValidDataLengthInformation:
            return sp_strcpy(sp, "ValidDataLengthInformation");
        case ShortNameInformation:
            return sp_strcpy(sp, "ShortNameInformation");
        case IoCompletionNotificationInformation:
            return sp_strcpy(sp, "IoCompletionNotificationInformation");
        case IoStatusBlockRangeInformation:
            return sp_strcpy(sp, "IoStatusBlockRangeInformation");
        case IoPriorityHintInformation:
            return sp_strcpy(sp, "IoPriorityHintInformation");
        case SfioReserveInformation:
            return sp_strcpy(sp, "SfioReserveInformation");
        case SfioVolumeInformation:
            return sp_strcpy(sp, "SfioVolumeInformation");
        case HardLinkInformation:
            return sp_strcpy(sp, "HardLinkInformation");
        case ProcessIdsUsingFileInformation:
            return sp_strcpy(sp, "ProcessIdsUsingFileInformation");
        case NormalizedNameInformation:
            return sp_strcpy(sp, "NormalizedNameInformation");
        case NetworkPhysicalNameInformation:
            return sp_strcpy(sp, "NetworkPhysicalNameInformation");
        case IdGlobalTxDirectoryInformation:
            return sp_strcpy(sp, "IdGlobalTxDirectoryInformation");
        case IsRemoteDeviceInformation:
            return sp_strcpy(sp, "IsRemoteDeviceInformation");
        case UnusedInformation:
            return sp_strcpy(sp, "UnusedInformation");
        case NumaNodeInformation:
            return sp_strcpy(sp, "NumaNodeInformation");
        case StandardLinkInformation:
            return sp_strcpy(sp, "StandardLinkInformation");
        case RemoteProtocolInformation:
            return sp_strcpy(sp, "RemoteProtocolInformation");
        case RenameInformationBypassAccessCheck:
            return sp_strcpy(sp, "RenameInformationBypassAccessCheck");
        case LinkInformationBypassAccessCheck:
            return sp_strcpy(sp, "LinkInformationBypassAccessCheck");
        case VolumeNameInformation:
            return sp_strcpy(sp, "VolumeNameInformation");
        case IdInformation:
            return sp_strcpy(sp, "IdInformation");
        case IdExtdDirectoryInformation:
            return sp_strcpy(sp, "IdExtdDirectoryInformation");
        case ReplaceCompletionInformation:
            return sp_strcpy(sp, "ReplaceCompletionInformation");
        case HardLinkFullIdInformation:
            return sp_strcpy(sp, "HardLinkFullIdInformation");
        case IdExtdBothDirectoryInformation:
            return sp_strcpy(sp, "IdExtdBothDirectoryInformation");
        case DispositionInformationEx:
            return sp_strcpy(sp, "DispositionInformationEx");
        case RenameInformationEx:
            return sp_strcpy(sp, "RenameInformationEx");
        case RenameInformationExBypassAccessCheck:
            return sp_strcpy(sp, "RenameInformationExBypassAccessCheck");
        case DesiredStorageClassInformation:
            return sp_strcpy(sp, "DesiredStorageClassInformation");
        case StatInformation:
            return sp_strcpy(sp, "StatInformation");
        case MemoryPartitionInformation:
            return sp_strcpy(sp, "MemoryPartitionInformation");
        case StatLxInformation:
            return sp_strcpy(sp, "StatLxInformation");
        case CaseSensitiveInformation:
            return sp_strcpy(sp, "CaseSensitiveInformation");
        case LinkInformationEx:
            return sp_strcpy(sp, "LinkInformationEx");
        case LinkInformationExBypassAccessCheck:
            return sp_strcpy(sp, "LinkInformationExBypassAccessCheck");
        case StorageReserveIdInformation:
            return sp_strcpy(sp, "StorageReserveIdInformation");
        case CaseSensitiveInformationForceAccessCheck:
            return sp_strcpy(sp, "CaseSensitiveInformationForceAccessCheck");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<FILE_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, string_view arg) {
    return sp_strcpy(sp, arg);
}

template<>
consteval bool format_spec_valid<string_view>(string_view fmt) {
    return fmt.empty();
}

template<>
consteval bool format_spec_valid<char*>(string_view fmt) {
    return format_spec_valid<string_view>(fmt);
}

template<>
consteval bool format_spec_valid<const char*>(string_view fmt) {
    return format_spec_valid<string_view>(fmt);
}

span<char> do_format_arg(span<char> sp, string_view, const AMD64_CONTEXT& ctx) {
    sp = sp_strcpy(sp, "rax=");
    sp = sp_itoa_hex(sp, ctx.Rax, 16, true);
    sp = sp_strcpy(sp, " rcx=");
    sp = sp_itoa_hex(sp, ctx.Rcx, 16, true);
    sp = sp_strcpy(sp, " rdx=");
    sp = sp_itoa_hex(sp, ctx.Rdx, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "rbx=");
    sp = sp_itoa_hex(sp, ctx.Rbx, 16, true);
    sp = sp_strcpy(sp, " rsp=");
    sp = sp_itoa_hex(sp, ctx.Rsp, 16, true);
    sp = sp_strcpy(sp, " rbp=");
    sp = sp_itoa_hex(sp, ctx.Rbp, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "rsi=");
    sp = sp_itoa_hex(sp, ctx.Rsi, 16, true);
    sp = sp_strcpy(sp, " rdi=");
    sp = sp_itoa_hex(sp, ctx.Rdi, 16, true);
    sp = sp_strcpy(sp, "  r8=");
    sp = sp_itoa_hex(sp, ctx.R8, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " r9=");
    sp = sp_itoa_hex(sp, ctx.R9, 16, true);
    sp = sp_strcpy(sp, " r10=");
    sp = sp_itoa_hex(sp, ctx.R10, 16, true);
    sp = sp_strcpy(sp, " r11=");
    sp = sp_itoa_hex(sp, ctx.R11, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "r12=");
    sp = sp_itoa_hex(sp, ctx.R12, 16, true);
    sp = sp_strcpy(sp, " r13=");
    sp = sp_itoa_hex(sp, ctx.R13, 16, true);
    sp = sp_strcpy(sp, " r14=");
    sp = sp_itoa_hex(sp, ctx.R14, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "r15=");
    sp = sp_itoa_hex(sp, ctx.R15, 16, true);
    sp = sp_strcpy(sp, " rip=");
    sp = sp_itoa_hex(sp, ctx.Rip, 16, true);
    sp = sp_strcpy(sp, " eflags=");
    sp = sp_itoa_hex(sp, ctx.EFlags, 8, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "cs=");
    sp = sp_itoa_hex(sp, ctx.SegCs, 4, true);
    sp = sp_strcpy(sp, " ds=");
    sp = sp_itoa_hex(sp, ctx.SegDs, 4, true);
    sp = sp_strcpy(sp, " es=");
    sp = sp_itoa_hex(sp, ctx.SegEs, 4, true);
    sp = sp_strcpy(sp, " fs=");
    sp = sp_itoa_hex(sp, ctx.SegFs, 4, true);
    sp = sp_strcpy(sp, " gs=");
    sp = sp_itoa_hex(sp, ctx.SegGs, 4, true);
    sp = sp_strcpy(sp, " ss=");
    sp = sp_itoa_hex(sp, ctx.SegSs, 4, true);
    sp = sp_strcpy(sp, "\n");

    return sp;
}

template<>
consteval bool format_spec_valid<AMD64_CONTEXT>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const ARM64_NT_CONTEXT& ctx) {
    sp = sp_strcpy(sp, " x0=");
    sp = sp_itoa_hex(sp, ctx.X0, 16, true);
    sp = sp_strcpy(sp, "  x1=");
    sp = sp_itoa_hex(sp, ctx.X1, 16, true);
    sp = sp_strcpy(sp, "  x2=");
    sp = sp_itoa_hex(sp, ctx.X2, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " x3=");
    sp = sp_itoa_hex(sp, ctx.X3, 16, true);
    sp = sp_strcpy(sp, "  x4=");
    sp = sp_itoa_hex(sp, ctx.X4, 16, true);
    sp = sp_strcpy(sp, "  x5=");
    sp = sp_itoa_hex(sp, ctx.X5, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " x6=");
    sp = sp_itoa_hex(sp, ctx.X6, 16, true);
    sp = sp_strcpy(sp, "  x7=");
    sp = sp_itoa_hex(sp, ctx.X7, 16, true);
    sp = sp_strcpy(sp, "  x8=");
    sp = sp_itoa_hex(sp, ctx.X8, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " x9=");
    sp = sp_itoa_hex(sp, ctx.X9, 16, true);
    sp = sp_strcpy(sp, " x10=");
    sp = sp_itoa_hex(sp, ctx.X10, 16, true);
    sp = sp_strcpy(sp, " x11=");
    sp = sp_itoa_hex(sp, ctx.X11, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x12=");
    sp = sp_itoa_hex(sp, ctx.X12, 16, true);
    sp = sp_strcpy(sp, " x13=");
    sp = sp_itoa_hex(sp, ctx.X13, 16, true);
    sp = sp_strcpy(sp, " x14=");
    sp = sp_itoa_hex(sp, ctx.X14, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x15=");
    sp = sp_itoa_hex(sp, ctx.X15, 16, true);
    sp = sp_strcpy(sp, " x16=");
    sp = sp_itoa_hex(sp, ctx.X16, 16, true);
    sp = sp_strcpy(sp, " x17=");
    sp = sp_itoa_hex(sp, ctx.X17, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x18=");
    sp = sp_itoa_hex(sp, ctx.X18, 16, true);
    sp = sp_strcpy(sp, " x19=");
    sp = sp_itoa_hex(sp, ctx.X19, 16, true);
    sp = sp_strcpy(sp, " x20=");
    sp = sp_itoa_hex(sp, ctx.X20, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x21=");
    sp = sp_itoa_hex(sp, ctx.X21, 16, true);
    sp = sp_strcpy(sp, " x22=");
    sp = sp_itoa_hex(sp, ctx.X22, 16, true);
    sp = sp_strcpy(sp, " x23=");
    sp = sp_itoa_hex(sp, ctx.X23, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x24=");
    sp = sp_itoa_hex(sp, ctx.X24, 16, true);
    sp = sp_strcpy(sp, " x25=");
    sp = sp_itoa_hex(sp, ctx.X25, 16, true);
    sp = sp_strcpy(sp, " x26=");
    sp = sp_itoa_hex(sp, ctx.X26, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x27=");
    sp = sp_itoa_hex(sp, ctx.X27, 16, true);
    sp = sp_strcpy(sp, " x28=");
    sp = sp_itoa_hex(sp, ctx.X28, 16, true);
    sp = sp_strcpy(sp, "  fp=");
    sp = sp_itoa_hex(sp, ctx.Fp, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " lr=");
    sp = sp_itoa_hex(sp, ctx.Lr, 16, true);
    sp = sp_strcpy(sp, "  sp=");
    sp = sp_itoa_hex(sp, ctx.Sp, 16, true);
    sp = sp_strcpy(sp, "  pc=");
    sp = sp_itoa_hex(sp, ctx.Pc, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "cpsr=");
    sp = sp_itoa_hex(sp, ctx.Cpsr, 8, true);
    sp = sp_strcpy(sp, " fpcr=");
    sp = sp_itoa_hex(sp, ctx.Fpcr, 8, true);
    sp = sp_strcpy(sp, " fpsr=");
    sp = sp_itoa_hex(sp, ctx.Fpsr, 8, true);
    sp = sp_strcpy(sp, "\n");

    return sp;
}

template<>
consteval bool format_spec_valid<ARM64_NT_CONTEXT>(string_view fmt) {
    return fmt.empty();
}

span<char> sp_u16string_view(span<char> buf, u16string_view u16sv) {
    if (buf.empty())
        return buf;

    for (unsigned int i = 0; i < u16sv.size(); i++) {
        uint32_t cp;

        if (u16sv[i] < 0xd800 || u16sv[i] > 0xdfff)
            cp = u16sv[i];
        else if (u16sv[i] >= 0xd800 && u16sv[i] < 0xdc00) {
            if (i == u16sv.size() - 1)
                return buf;

            auto next = u16sv[i+1];

            if (next < 0xdc00 || next > 0xdfff)
                return buf;

            cp = (((uint32_t)u16sv[i] & 0x3ff) << 10) | (next & 0x3ff);

            i++;
        } else // invalid
            return buf;

        if (cp < 0x80) {
            buf[0] = (char)cp;
            buf = buf.subspan(1);
        } else if (cp < 0x800) {
            if (buf.size() < 2)
                return buf;

            buf[0] = (char)(0xc0 | ((cp & 0x7c0) >> 6));
            buf[1] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(2);
        } else if (cp < 0x10000) {
            if (buf.size() < 3)
                return buf;

            buf[0] = (char)(0xe0 | ((cp & 0xf000) >> 12));
            buf[1] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[2] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(3);
        } else {
            if (buf.size() < 4)
                return buf;

            buf[0] = (char)(0xf0 | ((cp & 0xe0000) >> 18));
            buf[1] = (char)(0x80 | ((cp & 0x3f000) >> 12));
            buf[2] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[3] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(4);
        }
    }

    return buf;
}

span<char> do_format_arg(span<char> sp, string_view, u16string_view sv) {
    return sp_u16string_view(sp, sv);
}

template<>
consteval bool format_spec_valid<u16string_view>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view sv, const UNICODE_STRING& us) {
    return do_format_arg(sp, sv, u16string_view(us.Buffer, us.Length / sizeof(char16_t)));
}

template<>
consteval bool format_spec_valid<UNICODE_STRING>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const chrono::year_month_day& ymd) {
    sp = sp_itoa(sp, (uint64_t)(int)ymd.year());
    sp = sp_strcpy(sp, "-");

    if ((unsigned int)ymd.month() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (unsigned int)ymd.month());
    sp = sp_strcpy(sp, "-");

    if ((unsigned int)ymd.day() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (unsigned int)ymd.day());

    return sp;
}

template<>
consteval bool format_spec_valid<chrono::year_month_day>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const chrono::hh_mm_ss<chrono::seconds>& hms) {
    if (hms.hours().count() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (uint64_t)hms.hours().count());
    sp = sp_strcpy(sp, ":");

    if (hms.minutes().count() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (uint64_t)hms.minutes().count());
    sp = sp_strcpy(sp, ":");

    if (hms.seconds().count() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (uint64_t)hms.seconds().count());

    return sp;
}

template<>
consteval bool format_spec_valid<chrono::hh_mm_ss<chrono::seconds>>(string_view fmt) {
    return fmt.empty();
}

void checker_failure();

template<typename T, typename... Args>
consteval void check_specs(span<string_view, sizeof...(Args) + 1> sp,
                           tuple<T, Args...>*) {
    if (!format_spec_valid<remove_cvref_t<T>>(sp[0]))
        checker_failure();

    if constexpr (sizeof...(Args) > 0)
        check_specs(span<string_view, sizeof...(Args)>(sp.data() + 1, sp.size() - 1), (tuple<Args...>*)nullptr);
}

export
template<typename... Args>
struct checker {
    consteval checker(const char* q) : sv(q) {
        unsigned int num_entries = 0;

        // FIXME - {{ and }} escaping

        auto s = sv;

        while (!s.empty()) {
            if (s.front() == '{') {
                num_entries++;

                unsigned int depth = 1;
                size_t end = string_view::npos;

                for (unsigned int i = 1; i < s.size(); i++) {
                    if (s[i] == '{')
                        depth++;
                    else if (s[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            end = i;
                            break;
                        }
                    }
                }

                if (end == string_view::npos)
                    checker_failure();

                s = s.substr(end + 1);
                continue;
            }

            s = s.substr(1);
        }

        if (num_entries > sizeof...(Args))
            checker_failure();

        array<string_view, sizeof...(Args)> spec;

        s = sv;

        unsigned int pos = 0;

        while (!s.empty()) {
            if (s.front() == '{') {
                unsigned int depth = 1;
                size_t end = string_view::npos;

                for (unsigned int i = 1; i < s.size(); i++) {
                    if (s[i] == '{')
                        depth++;
                    else if (s[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            end = i;
                            break;
                        }
                    }
                }

                spec[pos] = s.substr(1, end - 1);

                s = s.substr(end + 1);
                pos++;
                continue;
            }

            s = s.substr(1);
        }

        if constexpr (sizeof...(Args) > 0)
            check_specs(spec, (tuple<Args...>*)nullptr);
    }

    string_view sv;
};

export
template<typename... Args>
span<char> format_to(span<char> sp, string_view fmt, const tuple<Args...>& args) {
    static constexpr size_t num_args = sizeof...(Args);

    if constexpr (num_args == 0)
        return sp_strcpy(sp, fmt);
    else {
        // FIXME - {{ and }} escaping

        auto brack = fmt.find('{');

        if (brack == string_view::npos)
            return sp_strcpy(sp, fmt);

        if (brack != 0) {
            sp = sp_strcpy(sp, string_view(fmt.data(), brack));
            fmt = string_view(fmt.data() + brack, fmt.size() - brack);
        }

        unsigned int depth = 1;
        size_t close = string_view::npos;
        bool nested = false;

        for (unsigned int i = 1; i < fmt.size(); i++) {
            if (fmt[i] == '{') {
                depth++;
                nested = true;
            } else if (fmt[i] == '}') {
                depth--;
                if (depth == 0) {
                    close = i;
                    break;
                }
            }
        }

        if (close == string_view::npos)
            return sp;

        if constexpr (num_args >= 2) {
            if constexpr (is_integral_v<tuple_element_t<0, remove_cvref_t<decltype(args)>>> && is_integral_v<tuple_element_t<1, remove_cvref_t<decltype(args)>>>) {
                if (nested)
                    sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args), get<1>(args));
                else
                    sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args));
            } else {
                sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args));
                nested = false;
            }
        } else {
            sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args));
            nested = false;
        }

        fmt = string_view(fmt.data() + close + 1, fmt.size() - close - 1);

        if (fmt.empty())
            return sp;

        if constexpr (num_args >= 2) {
            if (nested)
                return format_to(sp, fmt, apply([](auto, auto, auto... rest) { return make_tuple(rest...); }, args));
            else
                return format_to(sp, fmt, apply([](auto, auto... rest) { return make_tuple(rest...); }, args));
        } else
            return sp_strcpy(sp, fmt);
    }
}

export
template<typename... Args>
span<char> format(span<char> sp, type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    auto t = make_tuple(args...);

    return format_to(sp, fmt.sv, t);
}
