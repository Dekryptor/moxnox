module;

#include <span>
#include <string_view>
#include <atomic>
#include <mutex>
#include <string.h>

export module acpi;

#ifdef __x86_64__
import inout;
#endif

import format;
import print;
import mem;
import list;
import thread;
import interrupts;
import disable_interrupts;
import timer;
import scheduler;
import semaphore;
import object_ref;
import rwsem;
import spinlock;
import config;
import loaderblock;
import efi;
import kernel_time_page;
import acpica;
import gnu_efi;

using namespace std;

export uint64_t rsdp_pa;

struct acpi_irq_handler {
    list_entry le;
    ACPI_OSD_HANDLER service_routine;
    void* context;
};

struct acpi_irq_num {
    list_entry le;
    uint32_t irq;
    list_entry handlers = { &handlers, &handlers };
};

static rwsem irq_handlers_lock;
static list_entry irq_handlers = { &irq_handlers, &irq_handlers };

export struct acpi_device {
    list_entry le;
    string_view acpi_name;
    string_view hid;
    string_view uid;
    list_entry resources = { &resources, &resources };
    span<string_view> cids;
    list_entry properties = { &properties, &properties };
    ACPI_HANDLE handle;
    char buf[0];
};

export struct acpi_property {
    list_entry le;
    string_view name;
    uint64_t val;
    char buf[0];
};

export enum class acpi_device_resource_type {
    io,
    irq,
    extended_irq,
    address,
    fixed_memory32,
    dma,
    gpio,
    i2c_serial_bus,
    spi_serial_bus,
    uart_serial_bus,
    csi2_serial_bus,
};

export struct acpi_device_resource {
    list_entry le;
    enum acpi_device_resource_type type;
};

export struct acpi_device_resource_io : acpi_device_resource {
    ACPI_RESOURCE_IO io;
};

export struct acpi_device_resource_irq : acpi_device_resource {
    ACPI_RESOURCE_IRQ irq;
};

export struct acpi_device_resource_extended_irq : acpi_device_resource {
    ACPI_RESOURCE_EXTENDED_IRQ extended_irq;
};

export struct acpi_device_resource_address : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS64 address;
};

export struct acpi_device_resource_fixed_memory32 : acpi_device_resource {
    ACPI_RESOURCE_FIXED_MEMORY32 fixed_memory32;
};

export struct acpi_device_resource_dma : acpi_device_resource {
    ACPI_RESOURCE_DMA dma;
};

export struct acpi_device_resource_gpio : acpi_device_resource {
    ACPI_RESOURCE_GPIO gpio;
};

export struct acpi_device_resource_i2c_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_I2C_SERIALBUS i2c_serial_bus;
};

export struct acpi_device_resource_spi_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_SPI_SERIALBUS spi_serial_bus;
};

export struct acpi_device_resource_uart_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_UART_SERIALBUS uart_serial_bus;
};

export struct acpi_device_resource_csi2_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_CSI2_SERIALBUS csi2_serial_bus;
};

export enum class acpi_status {
    OK = 0x0000,
    ERROR = 0x0001,
    NO_ACPI_TABLES = 0x0002,
    NO_NAMESPACE = 0x0003,
    NO_MEMORY = 0x0004,
    NOT_FOUND = 0x0005,
    NOT_EXIST = 0x0006,
    ALREADY_EXISTS = 0x0007,
    TYPE = 0x0008,
    NULL_OBJECT = 0x0009,
    NULL_ENTRY = 0x000A,
    BUFFER_OVERFLOW = 0x000B,
    STACK_OVERFLOW = 0x000C,
    STACK_UNDERFLOW = 0x000D,
    NOT_IMPLEMENTED = 0x000E,
    SUPPORT = 0x000F,
    LIMIT = 0x0010,
    TIME = 0x0011,
    ACQUIRE_DEADLOCK = 0x0012,
    RELEASE_DEADLOCK = 0x0013,
    NOT_ACQUIRED = 0x0014,
    ALREADY_ACQUIRED = 0x0015,
    NO_HARDWARE_RESPONSE = 0x0016,
    NO_GLOBAL_LOCK = 0x0017,
    ABORT_METHOD = 0x0018,
    SAME_HANDLER = 0x0019,
    NO_HANDLER = 0x001A,
    OWNER_ID_LIMIT = 0x001B,
    NOT_CONFIGURED = 0x001C,
    ACCESS = 0x001D,
    IO_ERROR = 0x001E,
    NUMERIC_OVERFLOW = 0x001F,
    HEX_OVERFLOW = 0x0020,
    DECIMAL_OVERFLOW = 0x0021,
    OCTAL_OVERFLOW = 0x0022,
    END_OF_TABLE = 0x0023,
    BAD_PARAMETER = 0x1001,
    BAD_CHARACTER = 0x1002,
    BAD_PATHNAME = 0x1003,
    BAD_DATA = 0x1004,
    BAD_HEX_CONSTANT = 0x1005,
    BAD_OCTAL_CONSTANT = 0x1006,
    BAD_DECIMAL_CONSTANT = 0x1007,
    MISSING_ARGUMENTS = 0x1008,
    BAD_ADDRESS = 0x1009,
    BAD_SIGNATURE = 0x2001,
    BAD_HEADER = 0x2002,
    BAD_CHECKSUM = 0x2003,
    BAD_VALUE = 0x2004,
    INVALID_TABLE_LENGTH = 0x2005,
    AML_BAD_OPCODE = 0x3001,
    AML_NO_OPERAND = 0x3002,
    AML_OPERAND_TYPE = 0x3003,
    AML_OPERAND_VALUE = 0x3004,
    AML_UNINITIALIZED_LOCAL = 0x3005,
    AML_UNINITIALIZED_ARG = 0x3006,
    AML_UNINITIALIZED_ELEMENT = 0x3007,
    AML_NUMERIC_OVERFLOW = 0x3008,
    AML_REGION_LIMIT = 0x3009,
    AML_BUFFER_LIMIT = 0x300A,
    AML_PACKAGE_LIMIT = 0x300B,
    AML_DIVIDE_BY_ZERO = 0x300C,
    AML_BAD_NAME = 0x300D,
    AML_NAME_NOT_FOUND = 0x300E,
    AML_INTERNAL = 0x300F,
    AML_INVALID_SPACE_ID = 0x3010,
    AML_STRING_LIMIT = 0x3011,
    AML_NO_RETURN_VALUE = 0x3012,
    AML_METHOD_LIMIT = 0x3013,
    AML_NOT_OWNER = 0x3014,
    AML_MUTEX_ORDER = 0x3015,
    AML_MUTEX_NOT_ACQUIRED = 0x3016,
    AML_INVALID_RESOURCE_TYPE = 0x3017,
    AML_INVALID_INDEX = 0x3018,
    AML_REGISTER_LIMIT = 0x3019,
    AML_NO_WHILE = 0x301A,
    AML_ALIGNMENT = 0x301B,
    AML_NO_RESOURCE_END_TAG = 0x301C,
    AML_BAD_RESOURCE_VALUE = 0x301D,
    AML_CIRCULAR_REFERENCE = 0x301E,
    AML_BAD_RESOURCE_LENGTH = 0x301F,
    AML_ILLEGAL_ADDRESS = 0x3020,
    AML_LOOP_TIMEOUT = 0x3021,
    AML_UNINITIALIZED_NODE = 0x3022,
    AML_TARGET_TYPE = 0x3023,
    AML_PROTOCOL = 0x3024,
    AML_BUFFER_LENGTH = 0x3025,
    CTRL_RETURN_VALUE = 0x4001,
    CTRL_PENDING = 0x4002,
    CTRL_TERMINATE = 0x4003,
    CTRL_TRUE = 0x4004,
    CTRL_FALSE = 0x4005,
    CTRL_DEPTH = 0x4006,
    CTRL_END = 0x4007,
    CTRL_TRANSFER = 0x4008,
    CTRL_BREAK = 0x4009,
    CTRL_CONTINUE = 0x400A,
    CTRL_PARSE_CONTINUE = 0x400B,
    CTRL_PARSE_PENDING = 0x400C,
};

static list_entry acpi_devices = { &acpi_devices, &acpi_devices };

export struct acpi_recognizer {
    list_entry le;
    string_view hid;
    void (*func)(const acpi_device&);
    void (*prelim_func)(string_view);
};

export list_entry pnp_hids = { &pnp_hids, &pnp_hids };

export bool acpi_early_init_done = false;

template<>
consteval bool format_spec_valid<acpi_device_resource_type>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                acpi_device_resource_type arg) {
    switch (arg) {
        using enum acpi_device_resource_type;

        case io:
            return do_format_arg(sp, "", "io");
        case irq:
            return do_format_arg(sp, "", "irq");
        case extended_irq:
            return do_format_arg(sp, "", "extended_irq");
        case address:
            return do_format_arg(sp, "", "address");
        case fixed_memory32:
            return do_format_arg(sp, "", "fixed_memory32");
        case dma:
            return do_format_arg(sp, "", "dma");
        case gpio:
            return do_format_arg(sp, "", "gpio");
        case i2c_serial_bus:
            return do_format_arg(sp, "", "i2c_serial_bus");
        case spi_serial_bus:
            return do_format_arg(sp, "", "spi_serial_bus");
        case uart_serial_bus:
            return do_format_arg(sp, "", "uart_serial_bus");
        case csi2_serial_bus:
            return do_format_arg(sp, "", "csi2_serial_bus");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<ACPI_RESOURCE_FIXED_MEMORY32>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                const ACPI_RESOURCE_FIXED_MEMORY32& arg) {
    switch (arg.WriteProtect) {
        case ACPI_READ_ONLY_MEMORY:
            sp = do_format_arg(sp, "", "read-only");
            break;

        case ACPI_READ_WRITE_MEMORY:
            sp = do_format_arg(sp, "", "read-write");
            break;

        default:
            sp = do_format_arg(sp, "", "WriteProtect = ");
            sp = do_format_arg(sp, ":x", arg.WriteProtect);
            break;
    }

    sp = do_format_arg(sp, "", ", Address = ");
    sp = do_format_arg(sp, ":x", arg.Address);
    sp = do_format_arg(sp, "", ", AddressLength = ");
    sp = do_format_arg(sp, ":x", arg.AddressLength);

    return sp;
}

template<>
consteval bool format_spec_valid<ACPI_RESOURCE_EXTENDED_IRQ>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                const ACPI_RESOURCE_EXTENDED_IRQ& arg) {

    switch (arg.ProducerConsumer) {
        case ACPI_PRODUCER:
            sp = do_format_arg(sp, "", "Producer");
            break;

        case ACPI_CONSUMER:
            sp = do_format_arg(sp, "", "Consumer");
            break;

        default:
            sp = do_format_arg(sp, "", "ProducerConsumer = ");
            sp = do_format_arg(sp, ":x", arg.ProducerConsumer);
            break;
    }

    switch (arg.Triggering) {
        case ACPI_LEVEL_SENSITIVE:
            sp = do_format_arg(sp, "", ", Level-sensitive");
            break;

        case ACPI_EDGE_SENSITIVE:
            sp = do_format_arg(sp, "", ", Edge-sensitive");
            break;

        default:
            sp = do_format_arg(sp, "", ", Triggering = ");
            sp = do_format_arg(sp, ":x", arg.Triggering);
            break;
    }

    switch (arg.Polarity) {
        case ACPI_ACTIVE_HIGH:
            sp = do_format_arg(sp, "", ", Active high");
            break;

        case ACPI_ACTIVE_LOW:
            sp = do_format_arg(sp, "", ", Active low");
            break;

        case ACPI_ACTIVE_BOTH:
            sp = do_format_arg(sp, "", ", Active both");
            break;

        default:
            sp = do_format_arg(sp, "", ", Polarity = ");
            sp = do_format_arg(sp, ":x", arg.Polarity);
            break;
    }

    switch (arg.Shareable) {
        case ACPI_EXCLUSIVE:
            sp = do_format_arg(sp, "", ", Exclusive");
            break;

        case ACPI_SHARED:
            sp = do_format_arg(sp, "", ", Shared");
            break;

        default:
            sp = do_format_arg(sp, "", ", Shareable = ");
            sp = do_format_arg(sp, ":x", arg.Shareable);
            break;
    }

    switch (arg.WakeCapable) {
        case ACPI_NOT_WAKE_CAPABLE:
            sp = do_format_arg(sp, "", ", Not wake-capable");
            break;

        case ACPI_WAKE_CAPABLE:
            sp = do_format_arg(sp, "", ", Wake-capable");
            break;

        default:
            sp = do_format_arg(sp, "", ", WakeCapable = ");
            sp = do_format_arg(sp, ":x", arg.WakeCapable);
            break;
    }

    sp = do_format_arg(sp, "", ", InterruptCount = ");
    sp = do_format_arg(sp, ":x", arg.InterruptCount);
    sp = do_format_arg(sp, "", ", ResourceSource = (");
    sp = do_format_arg(sp, ":x", arg.ResourceSource.Index);
    sp = do_format_arg(sp, "", ", ");
    sp = do_format_arg(sp, "", string_view(arg.ResourceSource.StringPtr, arg.ResourceSource.StringLength));
    sp = do_format_arg(sp, "", "), Interrupt = ");
    sp = do_format_arg(sp, "", arg.Interrupt);

    // FIXME - multiple interrupts

    return sp;
}

template<>
consteval bool format_spec_valid<ACPI_RESOURCE_ADDRESS64>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                const ACPI_RESOURCE_ADDRESS64& arg) {
    switch (arg.ResourceType) {
        case ACPI_MEMORY_RANGE:
            sp = do_format_arg(sp, "", "Memory range");
            break;

        case ACPI_IO_RANGE:
            sp = do_format_arg(sp, "", "I/O range");
            break;

        case ACPI_BUS_NUMBER_RANGE:
            sp = do_format_arg(sp, "", "Bus number range");
            break;

        default:
            sp = do_format_arg(sp, "", "ResourceType = ");
            sp = do_format_arg(sp, ":x", arg.ResourceType);
            break;
    }

    switch (arg.ProducerConsumer) {
        case ACPI_PRODUCER:
            sp = do_format_arg(sp, "", ", Producer");
            break;

        case ACPI_CONSUMER:
            sp = do_format_arg(sp, "", ", Consumer");
            break;

        default:
            sp = do_format_arg(sp, "", ", ProducerConsumer = ");
            sp = do_format_arg(sp, ":x", arg.ProducerConsumer);
            break;
    }

    switch (arg.Decode) {
        case ACPI_POS_DECODE:
            sp = do_format_arg(sp, "", ", PosDecode");
            break;

        case ACPI_SUB_DECODE:
            sp = do_format_arg(sp, "", ", SubDecode");
            break;

        default:
            sp = do_format_arg(sp, "", ", Decode = ");
            sp = do_format_arg(sp, ":x", arg.Decode);
            break;
    }

    switch (arg.MinAddressFixed) {
        case ACPI_ADDRESS_NOT_FIXED:
            sp = do_format_arg(sp, "", ", min address not fixed");
            break;

        case ACPI_ADDRESS_FIXED:
            sp = do_format_arg(sp, "", ", min address fixed");
            break;

        default:
            sp = do_format_arg(sp, "", ", MinAddressFixed = ");
            sp = do_format_arg(sp, ":x", arg.MinAddressFixed);
            break;
    }

    switch (arg.MaxAddressFixed) {
        case ACPI_ADDRESS_NOT_FIXED:
            sp = do_format_arg(sp, "", ", max address not fixed");
            break;

        case ACPI_ADDRESS_FIXED:
            sp = do_format_arg(sp, "", ", max address fixed");
            break;

        default:
            sp = do_format_arg(sp, "", ", MaxAddressFixed = ");
            sp = do_format_arg(sp, ":x", arg.MaxAddressFixed);
            break;
    }

    sp = do_format_arg(sp, "", ", Info = (");

    switch (arg.Info.Mem.WriteProtect) {
        case ACPI_READ_ONLY_MEMORY:
            sp = do_format_arg(sp, "", "read-only");
            break;

        case ACPI_READ_WRITE_MEMORY:
            sp = do_format_arg(sp, "", "read-write");
            break;

        default:
            sp = do_format_arg(sp, "", "WriteProtect = ");
            sp = do_format_arg(sp, ":x", arg.Info.Mem.WriteProtect);
            break;
    }

    switch (arg.Info.Mem.Caching) {
        case ACPI_NON_CACHEABLE_MEMORY:
            sp = do_format_arg(sp, "", ", non-cacheable");
            break;

        case ACPI_CACHABLE_MEMORY:
            sp = do_format_arg(sp, "", ", cacheable");
            break;

        case ACPI_WRITE_COMBINING_MEMORY:
            sp = do_format_arg(sp, "", ", write-combining");
            break;

        case ACPI_PREFETCHABLE_MEMORY:
            sp = do_format_arg(sp, "", ", prefetchable");
            break;

        default:
            sp = do_format_arg(sp, "", ", Caching = ");
            sp = do_format_arg(sp, ":x", arg.Info.Mem.Caching);
            break;
    }

    sp = do_format_arg(sp, "", ", RangeType = ");
    sp = do_format_arg(sp, ":x", arg.Info.Mem.RangeType);
    sp = do_format_arg(sp, "", ", Translation = ");
    sp = do_format_arg(sp, ":x", arg.Info.Mem.Translation);
    sp = do_format_arg(sp, "", "), Granularity = ");
    sp = do_format_arg(sp, ":x", arg.Address.Granularity);
    sp = do_format_arg(sp, "", ", Minimum = ");
    sp = do_format_arg(sp, ":x", arg.Address.Minimum);
    sp = do_format_arg(sp, "", ", Maximum = ");
    sp = do_format_arg(sp, ":x", arg.Address.Maximum);
    sp = do_format_arg(sp, "", ", TranslationOffset = ");
    sp = do_format_arg(sp, ":x", arg.Address.TranslationOffset);
    sp = do_format_arg(sp, "", ", AddressLength = ");
    sp = do_format_arg(sp, ":x", arg.Address.AddressLength);

    sp = do_format_arg(sp, "", ", ResourceSource = (");
    sp = do_format_arg(sp, ":x", arg.ResourceSource.Index);
    sp = do_format_arg(sp, "", ", ");
    sp = do_format_arg(sp, "", string_view(arg.ResourceSource.StringPtr, arg.ResourceSource.StringLength));
    sp = do_format_arg(sp, "", ")");

    return sp;
}

template<>
consteval bool format_spec_valid<acpi_status>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, acpi_status arg) {
    switch (arg) {
        using enum acpi_status;

        case OK:
            return do_format_arg(sp, "", "AE_OK");
        case ERROR:
            return do_format_arg(sp, "", "AE_ERROR");
        case NO_ACPI_TABLES:
            return do_format_arg(sp, "", "AE_NO_ACPI_TABLES");
        case NO_NAMESPACE:
            return do_format_arg(sp, "", "AE_NO_NAMESPACE");
        case NO_MEMORY:
            return do_format_arg(sp, "", "AE_NO_MEMORY");
        case NOT_FOUND:
            return do_format_arg(sp, "", "AE_NOT_FOUND");
        case NOT_EXIST:
            return do_format_arg(sp, "", "AE_NOT_EXIST");
        case ALREADY_EXISTS:
            return do_format_arg(sp, "", "AE_ALREADY_EXISTS");
        case TYPE:
            return do_format_arg(sp, "", "AE_TYPE");
        case NULL_OBJECT:
            return do_format_arg(sp, "", "AE_NULL_OBJECT");
        case NULL_ENTRY:
            return do_format_arg(sp, "", "AE_NULL_ENTRY");
        case BUFFER_OVERFLOW:
            return do_format_arg(sp, "", "AE_BUFFER_OVERFLOW");
        case STACK_OVERFLOW:
            return do_format_arg(sp, "", "AE_STACK_OVERFLOW");
        case STACK_UNDERFLOW:
            return do_format_arg(sp, "", "AE_STACK_UNDERFLOW");
        case NOT_IMPLEMENTED:
            return do_format_arg(sp, "", "AE_NOT_IMPLEMENTED");
        case SUPPORT:
            return do_format_arg(sp, "", "AE_SUPPORT");
        case LIMIT:
            return do_format_arg(sp, "", "AE_LIMIT");
        case TIME:
            return do_format_arg(sp, "", "AE_TIME");
        case ACQUIRE_DEADLOCK:
            return do_format_arg(sp, "", "AE_ACQUIRE_DEADLOCK");
        case RELEASE_DEADLOCK:
            return do_format_arg(sp, "", "AE_RELEASE_DEADLOCK");
        case NOT_ACQUIRED:
            return do_format_arg(sp, "", "AE_NOT_ACQUIRED");
        case ALREADY_ACQUIRED:
            return do_format_arg(sp, "", "AE_ALREADY_ACQUIRED");
        case NO_HARDWARE_RESPONSE:
            return do_format_arg(sp, "", "AE_NO_HARDWARE_RESPONSE");
        case NO_GLOBAL_LOCK:
            return do_format_arg(sp, "", "AE_NO_GLOBAL_LOCK");
        case ABORT_METHOD:
            return do_format_arg(sp, "", "AE_ABORT_METHOD");
        case SAME_HANDLER:
            return do_format_arg(sp, "", "AE_SAME_HANDLER");
        case NO_HANDLER:
            return do_format_arg(sp, "", "AE_NO_HANDLER");
        case OWNER_ID_LIMIT:
            return do_format_arg(sp, "", "AE_OWNER_ID_LIMIT");
        case NOT_CONFIGURED:
            return do_format_arg(sp, "", "AE_NOT_CONFIGURED");
        case ACCESS:
            return do_format_arg(sp, "", "AE_ACCESS");
        case IO_ERROR:
            return do_format_arg(sp, "", "AE_IO_ERROR");
        case NUMERIC_OVERFLOW:
            return do_format_arg(sp, "", "AE_NUMERIC_OVERFLOW");
        case HEX_OVERFLOW:
            return do_format_arg(sp, "", "AE_HEX_OVERFLOW");
        case DECIMAL_OVERFLOW:
            return do_format_arg(sp, "", "AE_DECIMAL_OVERFLOW");
        case OCTAL_OVERFLOW:
            return do_format_arg(sp, "", "AE_OCTAL_OVERFLOW");
        case END_OF_TABLE:
            return do_format_arg(sp, "", "AE_END_OF_TABLE");
        case BAD_PARAMETER:
            return do_format_arg(sp, "", "AE_BAD_PARAMETER");
        case BAD_CHARACTER:
            return do_format_arg(sp, "", "AE_BAD_CHARACTER");
        case BAD_PATHNAME:
            return do_format_arg(sp, "", "AE_BAD_PATHNAME");
        case BAD_DATA:
            return do_format_arg(sp, "", "AE_BAD_DATA");
        case BAD_HEX_CONSTANT:
            return do_format_arg(sp, "", "AE_BAD_HEX_CONSTANT");
        case BAD_OCTAL_CONSTANT:
            return do_format_arg(sp, "", "AE_BAD_OCTAL_CONSTANT");
        case BAD_DECIMAL_CONSTANT:
            return do_format_arg(sp, "", "AE_BAD_DECIMAL_CONSTANT");
        case MISSING_ARGUMENTS:
            return do_format_arg(sp, "", "AE_MISSING_ARGUMENTS");
        case BAD_ADDRESS:
            return do_format_arg(sp, "", "AE_BAD_ADDRESS");
        case BAD_SIGNATURE:
            return do_format_arg(sp, "", "AE_BAD_SIGNATURE");
        case BAD_HEADER:
            return do_format_arg(sp, "", "AE_BAD_HEADER");
        case BAD_CHECKSUM:
            return do_format_arg(sp, "", "AE_BAD_CHECKSUM");
        case BAD_VALUE:
            return do_format_arg(sp, "", "AE_BAD_VALUE");
        case INVALID_TABLE_LENGTH:
            return do_format_arg(sp, "", "AE_INVALID_TABLE_LENGTH");
        case AML_BAD_OPCODE:
            return do_format_arg(sp, "", "AE_AML_BAD_OPCODE");
        case AML_NO_OPERAND:
            return do_format_arg(sp, "", "AE_AML_NO_OPERAND");
        case AML_OPERAND_TYPE:
            return do_format_arg(sp, "", "AE_AML_OPERAND_TYPE");
        case AML_OPERAND_VALUE:
            return do_format_arg(sp, "", "AE_AML_OPERAND_VALUE");
        case AML_UNINITIALIZED_LOCAL:
            return do_format_arg(sp, "", "AE_AML_UNINITIALIZED_LOCAL");
        case AML_UNINITIALIZED_ARG:
            return do_format_arg(sp, "", "AE_AML_UNINITIALIZED_ARG");
        case AML_UNINITIALIZED_ELEMENT:
            return do_format_arg(sp, "", "AE_AML_UNINITIALIZED_ELEMENT");
        case AML_NUMERIC_OVERFLOW:
            return do_format_arg(sp, "", "AE_AML_NUMERIC_OVERFLOW");
        case AML_REGION_LIMIT:
            return do_format_arg(sp, "", "AE_AML_REGION_LIMIT");
        case AML_BUFFER_LIMIT:
            return do_format_arg(sp, "", "AE_AML_BUFFER_LIMIT");
        case AML_PACKAGE_LIMIT:
            return do_format_arg(sp, "", "AE_AML_PACKAGE_LIMIT");
        case AML_DIVIDE_BY_ZERO:
            return do_format_arg(sp, "", "AE_AML_DIVIDE_BY_ZERO");
        case AML_BAD_NAME:
            return do_format_arg(sp, "", "AE_AML_BAD_NAME");
        case AML_NAME_NOT_FOUND:
            return do_format_arg(sp, "", "AE_AML_NAME_NOT_FOUND");
        case AML_INTERNAL:
            return do_format_arg(sp, "", "AE_AML_INTERNAL");
        case AML_INVALID_SPACE_ID:
            return do_format_arg(sp, "", "AE_AML_INVALID_SPACE_ID");
        case AML_STRING_LIMIT:
            return do_format_arg(sp, "", "AE_AML_STRING_LIMIT");
        case AML_NO_RETURN_VALUE:
            return do_format_arg(sp, "", "AE_AML_NO_RETURN_VALUE");
        case AML_METHOD_LIMIT:
            return do_format_arg(sp, "", "AE_AML_METHOD_LIMIT");
        case AML_NOT_OWNER:
            return do_format_arg(sp, "", "AE_AML_NOT_OWNER");
        case AML_MUTEX_ORDER:
            return do_format_arg(sp, "", "AE_AML_MUTEX_ORDER");
        case AML_MUTEX_NOT_ACQUIRED:
            return do_format_arg(sp, "", "AE_AML_MUTEX_NOT_ACQUIRED");
        case AML_INVALID_RESOURCE_TYPE:
            return do_format_arg(sp, "", "AE_AML_INVALID_RESOURCE_TYPE");
        case AML_INVALID_INDEX:
            return do_format_arg(sp, "", "AE_AML_INVALID_INDEX");
        case AML_REGISTER_LIMIT:
            return do_format_arg(sp, "", "AE_AML_REGISTER_LIMIT");
        case AML_NO_WHILE:
            return do_format_arg(sp, "", "AE_AML_NO_WHILE");
        case AML_ALIGNMENT:
            return do_format_arg(sp, "", "AE_AML_ALIGNMENT");
        case AML_NO_RESOURCE_END_TAG:
            return do_format_arg(sp, "", "AE_AML_NO_RESOURCE_END_TAG");
        case AML_BAD_RESOURCE_VALUE:
            return do_format_arg(sp, "", "AE_AML_BAD_RESOURCE_VALUE");
        case AML_CIRCULAR_REFERENCE:
            return do_format_arg(sp, "", "AE_AML_CIRCULAR_REFERENCE");
        case AML_BAD_RESOURCE_LENGTH:
            return do_format_arg(sp, "", "AE_AML_BAD_RESOURCE_LENGTH");
        case AML_ILLEGAL_ADDRESS:
            return do_format_arg(sp, "", "AE_AML_ILLEGAL_ADDRESS");
        case AML_LOOP_TIMEOUT:
            return do_format_arg(sp, "", "AE_AML_LOOP_TIMEOUT");
        case AML_UNINITIALIZED_NODE:
            return do_format_arg(sp, "", "AE_AML_UNINITIALIZED_NODE");
        case AML_TARGET_TYPE:
            return do_format_arg(sp, "", "AE_AML_TARGET_TYPE");
        case AML_PROTOCOL:
            return do_format_arg(sp, "", "AE_AML_PROTOCOL");
        case AML_BUFFER_LENGTH:
            return do_format_arg(sp, "", "AE_AML_BUFFER_LENGTH");
        case CTRL_RETURN_VALUE:
            return do_format_arg(sp, "", "AE_CTRL_RETURN_VALUE");
        case CTRL_PENDING:
            return do_format_arg(sp, "", "AE_CTRL_PENDING");
        case CTRL_TERMINATE:
            return do_format_arg(sp, "", "AE_CTRL_TERMINATE");
        case CTRL_TRUE:
            return do_format_arg(sp, "", "AE_CTRL_TRUE");
        case CTRL_FALSE:
            return do_format_arg(sp, "", "AE_CTRL_FALSE");
        case CTRL_DEPTH:
            return do_format_arg(sp, "", "AE_CTRL_DEPTH");
        case CTRL_END:
            return do_format_arg(sp, "", "AE_CTRL_END");
        case CTRL_TRANSFER:
            return do_format_arg(sp, "", "AE_CTRL_TRANSFER");
        case CTRL_BREAK:
            return do_format_arg(sp, "", "AE_CTRL_BREAK");
        case CTRL_CONTINUE:
            return do_format_arg(sp, "", "AE_CTRL_CONTINUE");
        case CTRL_PARSE_CONTINUE:
            return do_format_arg(sp, "", "AE_CTRL_PARSE_CONTINUE");
        case CTRL_PARSE_PENDING:
            return do_format_arg(sp, "", "AE_CTRL_PARSE_PENDING");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

static void acpi_notify_handler(ACPI_HANDLE device, uint32_t value, void*) {
    print("FIXME - acpi_notify_handler({}, {})\n", (uintptr_t)device, value);
}

static bool acpi_isr_func(void* ctx) {
    auto& n = *(acpi_irq_num*)ctx;

    lock_guard lock(irq_handlers_lock);

    for (auto le = n.handlers.next; le != &n.handlers; le = le->next) {
        auto& h = containing_record<acpi_irq_handler>(le);

        if (h.service_routine(h.context) == ACPI_INTERRUPT_HANDLED)
            return true;
    }

    return false;
}

export void init_acpi_early() {
    ACPI_STATUS Status;

    Status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeSubsystem returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiInitializeTables(nullptr, 16, false);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeTables returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiLoadTables();
    if (ACPI_FAILURE(Status)) {
        print("AcpiLoadTables returned {}\n", (acpi_status)Status);
        return;
    }
}

static void power_button_pressed_thread() {
    // FIXME - should be calling NtShutdownSystem (why does it not work?)

    if (ResetSystem)
        ResetSystem(EfiResetShutdown, (EFI_STATUS)0, 0, nullptr);
    else {
        AcpiEnterSleepStatePrep(5);
        disable_interrupts();
        AcpiEnterSleepState(5);
    }

    print("power off\n");
}

static uint32_t power_button_pressed(void*) {
    // interrupts are disabled here
    add_kernel_thread(power_button_pressed_thread);

    return 0;
}

#ifdef __x86_64__
static void call_pic_func() {
    ACPI_OBJECT param;
    ACPI_OBJECT_LIST params;

    param.Type = ACPI_TYPE_INTEGER;
    param.Integer.Value = 1; // APIC mode

    params.Count = 1;
    params.Pointer = &param;

    auto status = AcpiEvaluateObject(nullptr, (char*)"\\_PIC", &params,
                                     nullptr);
    if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
        print("AcpiEvaluateObject returned {} for \\_PIC\n", (acpi_status)status);
        return;
    }
}
#endif

export void init_acpi() {
    ACPI_STATUS Status;

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY,
                                      acpi_notify_handler, nullptr);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInstallNotifyHandler returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status)) {
        print("AcpiEnableSubsystem returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeObjects returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
                                          power_button_pressed, nullptr);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInstallFixedEventHandler returned {}\n", (acpi_status)Status);
        return;
    }

#ifdef __x86_64__
    call_pic_func();
#endif
}

static ACPI_STATUS find_device_resources_cb(ACPI_RESOURCE* res, void* ctx) {
    auto& d = *(acpi_device*)ctx;

    switch (res->Type) {
        case ACPI_RESOURCE_TYPE_IRQ: {
            auto r = new acpi_device_resource_irq;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::irq;
            memcpy(&r->irq, &res->Data.Irq, sizeof(ACPI_RESOURCE_IRQ));
            insert_tail_list(d.resources, r->le);
            break;
        }

        case ACPI_RESOURCE_TYPE_DMA: {
            auto r = new acpi_device_resource_dma;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::dma;
            memcpy(&r->dma, &res->Data.Dma, sizeof(ACPI_RESOURCE_DMA));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_START_DEPENDENT
        // FIXME - ACPI_RESOURCE_TYPE_END_DEPENDENT

        case ACPI_RESOURCE_TYPE_IO: {
            auto r = new acpi_device_resource_io;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::io;
            memcpy(&r->io, &res->Data.Io, sizeof(ACPI_RESOURCE_IO));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_FIXED_IO
        // FIXME - ACPI_RESOURCE_TYPE_VENDOR

        case ACPI_RESOURCE_TYPE_END_TAG:
            break;

        // FIXME - ACPI_RESOURCE_TYPE_MEMORY24
        // FIXME - ACPI_RESOURCE_TYPE_MEMORY32

        case ACPI_RESOURCE_TYPE_FIXED_MEMORY32: {
            auto r = new acpi_device_resource_fixed_memory32;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::fixed_memory32;
            memcpy(&r->fixed_memory32, &res->Data.FixedMemory32, sizeof(ACPI_RESOURCE_FIXED_MEMORY32));
            insert_tail_list(d.resources, r->le);
            break;
        }

        case ACPI_RESOURCE_TYPE_ADDRESS16:
        case ACPI_RESOURCE_TYPE_ADDRESS32:
        case ACPI_RESOURCE_TYPE_ADDRESS64: {
            auto r = new acpi_device_resource_address;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::address;

            auto status = AcpiResourceToAddress64(res, &r->address);
            if (ACPI_FAILURE(status)) {
                print("AcpiResourceToAddress64 returned {}\n", (acpi_status)status);
                delete r;
                return status;
            }

            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64

        case ACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
            auto r = new acpi_device_resource_extended_irq;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::extended_irq;
            memcpy(&r->extended_irq, &res->Data.ExtendedIrq, sizeof(ACPI_RESOURCE_EXTENDED_IRQ));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_GENERIC_REGISTER

        case ACPI_RESOURCE_TYPE_GPIO: {
            auto r = new acpi_device_resource_gpio;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::gpio;
            memcpy(&r->gpio, &res->Data.Gpio, sizeof(ACPI_RESOURCE_GPIO));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_FIXED_DMA

        case ACPI_RESOURCE_TYPE_SERIAL_BUS: {
            switch (res->Data.CommonSerialBus.Type) {
                case ACPI_RESOURCE_SERIAL_TYPE_I2C: {
                    auto r = new acpi_device_resource_i2c_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::i2c_serial_bus;
                    memcpy(&r->i2c_serial_bus, &res->Data.I2cSerialBus,
                        sizeof(ACPI_RESOURCE_I2C_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                case ACPI_RESOURCE_SERIAL_TYPE_SPI: {
                    auto r = new acpi_device_resource_spi_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::spi_serial_bus;
                    memcpy(&r->spi_serial_bus, &res->Data.SpiSerialBus,
                           sizeof(ACPI_RESOURCE_SPI_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                case ACPI_RESOURCE_SERIAL_TYPE_UART: {
                    auto r = new acpi_device_resource_uart_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::uart_serial_bus;
                    memcpy(&r->uart_serial_bus, &res->Data.UartSerialBus,
                           sizeof(ACPI_RESOURCE_UART_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                case ACPI_RESOURCE_SERIAL_TYPE_CSI2: {
                    auto r = new acpi_device_resource_csi2_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::csi2_serial_bus;
                    memcpy(&r->csi2_serial_bus, &res->Data.Csi2SerialBus,
                           sizeof(ACPI_RESOURCE_CSI2_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                default:
                    print("unrecognized ACPI serial bus type {}\n",
                          res->Data.CommonSerialBus.Type);
                    break;
            }

            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_PIN_FUNCTION
        // FIXME - ACPI_RESOURCE_TYPE_PIN_CONFIG
        // FIXME - ACPI_RESOURCE_TYPE_PIN_GROUP
        // FIXME - ACPI_RESOURCE_TYPE_PIN_GROUP_FUNCTION
        // FIXME - ACPI_RESOURCE_TYPE_PIN_GROUP_CONFIG
        // FIXME - ACPI_RESOURCE_TYPE_CLOCK_INPUT

        default:
            print("FIXME - unhandled ACPI resource type {}\n", res->Type);
            break;
    }

    return AE_OK;
}

static void find_device_resources(acpi_device& d, ACPI_HANDLE obj) {
    ACPI_STATUS status;

    status = AcpiWalkResources(obj, (char*)"_CRS", find_device_resources_cb, &d);

    if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
        print("AcpiWalkResources returned {}\n", (acpi_status)status);
        return;
    }
}

static void add_device_properties(acpi_device& dev, const ACPI_OBJECT& pkg) {
    if (pkg.Type != ACPI_TYPE_PACKAGE || pkg.Package.Count != 2)
        return;

    auto& name = pkg.Package.Elements[0];
    auto& val = pkg.Package.Elements[1];

    if (name.Type != ACPI_TYPE_STRING)
        return;

    // FIXME - value can also be string, local reference, or package
    if (val.Type == ACPI_TYPE_INTEGER) {
        size_t size = offsetof(acpi_property, buf) + name.String.Length;

        auto p = (acpi_property*)malloc(size); // FIXME?
        if (!p) {
            print("malloc failed\n");
            return;
        }

        new (p) acpi_property;

        memcpy(p->buf, name.String.Pointer, name.String.Length);
        p->name = string_view(p->buf, name.String.Length);
        p->val = val.Integer.Value;

        insert_tail_list(dev.properties, p->le);
    }
}

static void find_device_properties(acpi_device& dev, ACPI_HANDLE Object) {
    ACPI_STATUS status;
    ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, nullptr };

    static const uint8_t device_props_guid[] = { 0x14, 0xd8, 0xff, 0xda, 0xba, 0x6e, 0x8c, 0x4d, 0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01 };

    status = AcpiEvaluateObjectTyped(Object, (char*)"_DSD", nullptr, &buf,
                                     ACPI_TYPE_PACKAGE);
    if (ACPI_FAILURE(status)) {
        if (status != AE_NOT_FOUND)
            print("AcpiEvaluateObjectTyped returned {}\n", (acpi_status)status);
        return;
    }

    auto& props = *(ACPI_OBJECT*)buf.Pointer;

    if (props.Type != ACPI_TYPE_PACKAGE || props.Package.Count % 2) {
        AcpiOsFree(buf.Pointer);
        return;
    }

    for (unsigned int i = 0; i < props.Package.Count; i += 2) {
        auto& guid = props.Package.Elements[i];
        auto& properties = props.Package.Elements[i + 1];

        if (guid.Type != ACPI_TYPE_BUFFER || guid.Buffer.Length != 16)
            break;

        if (memcmp(guid.Buffer.Pointer, device_props_guid, sizeof(device_props_guid)))
            continue;

        if (properties.Type != ACPI_TYPE_PACKAGE)
            break;

        for (unsigned int j = 0; j < properties.Package.Count; j++) {
            add_device_properties(dev, properties.Package.Elements[j]);
        }
    }

    AcpiOsFree(buf.Pointer);
}

static ACPI_STATUS found_device(ACPI_HANDLE Object, uint32_t /*NestingLevel*/,
                                void* /*Context*/, void** /*ReturnValue*/) {
    ACPI_STATUS status;
    char buf[80];
    ACPI_BUFFER name;
    ACPI_DEVICE_INFO* devinfo;

    status = AcpiGetObjectInfo(Object, &devinfo);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetObjectInfo returned {}\n", (acpi_status)status);
        return AE_OK;
    }

    string_view hid, uid;
    ACPI_PNP_DEVICE_ID_LIST* cids = nullptr;

    if (devinfo->Valid & ACPI_VALID_HID) {
        hid = string_view(devinfo->HardwareId.String, devinfo->HardwareId.Length);

        if (!hid.empty() && hid.back() == 0)
            hid = string_view(hid.data(), hid.size() - 1);
    }

    if (devinfo->Valid & ACPI_VALID_CID)
        cids = &devinfo->CompatibleIdList;

    if (devinfo->Valid & ACPI_VALID_UID) {
        uid = string_view(devinfo->UniqueId.String, devinfo->UniqueId.Length);

        if (auto nul = uid.find('\0'); nul != string_view::npos)
            uid = string_view(uid.data(), nul);
    }

    name.Pointer = buf;
    name.Length = sizeof(buf);

    status = AcpiGetName(Object, ACPI_FULL_PATHNAME, &name);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetName returned {}\n", (acpi_status)status);
        AcpiOsFree(devinfo);
        return AE_OK;
    }

    auto acpi_name = string_view((char*)name.Pointer, name.Length);

    if (!acpi_name.empty() && acpi_name.back() == 0)
        acpi_name = string_view(acpi_name.data(), acpi_name.size() - 1);

    size_t bufsize = hid.size() + uid.size() + acpi_name.size();

    if (cids) {
        for (uint32_t i = 0; i < cids->Count; i++) {
            auto sv = string_view(cids->Ids[i].String, cids->Ids[i].Length);

            if (!sv.empty() && sv.back() == 0)
                sv = string_view(sv.data(), sv.size() - 1);

            bufsize += sv.size();
        }

        bufsize += sizeof(string_view) * cids->Count;
    }

    auto d = (acpi_device*)malloc(offsetof(acpi_device, buf) + bufsize);
    if (!d) {
        print("malloc failed\n");
        AcpiOsFree(devinfo);
        return AE_OK;
    }

    new (d) acpi_device;

    d->handle = Object;

    auto ptr = d->buf;

    memcpy(ptr, acpi_name.data(), acpi_name.size());
    d->acpi_name = string_view(ptr, acpi_name.size());
    ptr += acpi_name.size();

    memcpy(ptr, hid.data(), hid.size());
    d->hid = string_view(ptr, hid.size());
    ptr += hid.size();

    memcpy(ptr, uid.data(), uid.size());
    d->uid = string_view(ptr, uid.size());
    ptr += uid.size();

    if (cids) {
        d->cids = span((string_view*)ptr, cids->Count);

        ptr += sizeof(string_view) * cids->Count;

        for (uint32_t i = 0; i < cids->Count; i++) {
            auto sv = string_view(cids->Ids[i].String, cids->Ids[i].Length);

            if (!sv.empty() && sv.back() == 0)
                sv = string_view(sv.data(), sv.size() - 1);

            memcpy(ptr, sv.data(), sv.size());

            d->cids[i] = string_view(ptr, sv.size());

            ptr += sv.size();
        }
    }

    insert_tail_list(acpi_devices, d->le);

    AcpiOsFree(devinfo);

    find_device_resources(*d, Object);

    find_device_properties(*d, Object);

    return AE_OK;
}

export void find_acpi_devices() {
    ACPI_STATUS status;

    status = AcpiGetDevices(nullptr, found_device, nullptr, nullptr);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetDevices returned {}\n", (acpi_status)status);
        return;
    }

    // replace unrecognized HIDs with recognized CIDs
    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        bool found = false;
        auto& d = containing_record<acpi_device>(le);

        for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
            auto& p = containing_record<acpi_recognizer>(le);

            if (p.hid == d.hid) {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        for (auto c : d.cids) {
            for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
                auto& p = containing_record<acpi_recognizer>(le);

                if (p.hid == c) {
                    found = true;
                    break;
                }
            }

            if (found) {
                d.hid = c;
                break;
            }
        }
    }

    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        auto& d = containing_record<acpi_device>(le);

        if (d.hid.empty())
            continue;

        for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
            auto& p = containing_record<acpi_recognizer>(le);

            if (p.hid != d.hid)
                continue;

            if (p.prelim_func)
                p.prelim_func(d.uid);

            break;
        }
    }

    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        auto& d = containing_record<acpi_device>(le);

        print("found ACPI device {}, HID {}, UID {}\n",
              d.acpi_name, !d.hid.empty() ? d.hid : "(none)",
              !d.uid.empty() ? d.uid : "(none)");

        if (d.hid.empty())
            continue;

        for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
            auto& p = containing_record<acpi_recognizer>(le);

            if (p.hid != d.hid)
                continue;

            p.func(d);
            break;
        }
    }
}

extern "C"
ACPI_STATUS AcpiOsInitialize() {
    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsTerminate() {
    return AE_OK;
}

extern "C"
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    return rsdp_pa;
}

extern "C"
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* /*InitVal*/,
                                     ACPI_STRING* NewVal) {
    *NewVal = nullptr;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* /*ExistingTable*/,
                                ACPI_TABLE_HEADER** NewTable) {
    *NewTable = nullptr;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* /*ExistingTable*/,
                                        ACPI_PHYSICAL_ADDRESS* NewAddress,
                                        UINT32* /*NewTableLength*/) {
    *NewAddress = 0;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle) {
    if (!acpi_early_init_done) {
        auto ptr = malloc_aligned<true>(sizeof(spinlock), alignof(spinlock));

        *OutHandle = new (ptr) spinlock;
    } else
        *OutHandle = new spinlock;

    return AE_OK;
}

extern "C"
void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {
    auto* lock = (spinlock*)Handle;

    delete lock;
}

extern "C"
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
    if (!acpi_early_init_done)
        return 0;

    auto& lock = *(spinlock*)Handle;

    lock.lock();

    return 0;
}

extern "C"
void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS) {
    if (!acpi_early_init_done)
        return;

    auto& lock = *(spinlock*)Handle;

    lock.unlock();
}

extern "C"
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
                                  ACPI_SEMAPHORE* OutHandle) {
    object_ref ref;

    if (!acpi_early_init_done)
        ref = create_semaphore<true>(MaxUnits);
    else
        ref = create_semaphore<false>(MaxUnits);

    auto& sem = static_cast<semaphore&>(ref.get());

    sem.acquire();
    sem.current_count = InitialUnits;

    *OutHandle = &sem;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
    auto& sem = *static_cast<semaphore*>(Handle);

    sem.release();

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units,
                                UINT16 /*Timeout*/) {
    if (!acpi_early_init_done)
        return AE_OK;

    auto& sem = *static_cast<semaphore*>(Handle);

    // FIXME - Timeout

    if (Units != 1) {
        print("FIXME - AcpiOsWaitSemaphore with Units != 1\n"); // FIXME
        return AE_ERROR;
    }

    sem.acquire();
    wait_for_object(sem, true);
    sem.release();

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
    if (!acpi_early_init_done)
        return AE_OK;

    auto& sem = *static_cast<semaphore*>(Handle);

    if (Units != 1) {
        print("FIXME - AcpiOsSignalSemaphore with Units != 1\n"); // FIXME
        return AE_ERROR;
    }

    release_semaphore(sem);

    return AE_OK;
}

extern "C"
void* AcpiOsAllocate(ACPI_SIZE Size) {
    if (!acpi_early_init_done)
        return malloc_aligned<true>(Size, 1);
    else
        return malloc(Size);
}

extern "C"
void AcpiOsFree(void* Memory) {
    if (!acpi_early_init_done)
        free_no_lock(Memory);
    else
        free(Memory);
}

extern "C"
void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
    auto pa = (uint64_t)Where;

    // FIXME - check EFI memory map for whether it should be writethrough, write-combining, etc.

    mem_type t = mem_type::mmio;

    for (auto le = init_mappings.next; le != &init_mappings; le = le->next) {
        const auto& m = containing_record<init_map_entry>(le);

        if (Where >= m.addr && Where < m.addr + m.len) {
            t = m.type;
            break;
        }
    }

    if (t != mem_type::mmio && t != mem_type::reserved)
        return phys_address(Where);

    if (pa & (PAGE_SIZE - 1)) {
        Length += pa & (PAGE_SIZE - 1);
        pa &= ~(PAGE_SIZE - 1);
    }

    if (Length & (PAGE_SIZE - 1))
        Length += PAGE_SIZE - (Length & (PAGE_SIZE - 1));

    if (Length == 0)
        return nullptr;

    auto num_pages = (uint32_t)(Length >> PAGE_SHIFT);
    auto va = allocate_va(num_pages);

    if (!va) {
        print("allocate_va failed\n");
        return nullptr;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (!map_page(pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, num_pages);

            return nullptr;
        }
    }

    return (void*)(va + (Where & (PAGE_SIZE - 1)));
}

extern "C"
void AcpiOsUnmapMemory(void* LogicalAddress, ACPI_SIZE Size) {
    if (is_phys_memory(LogicalAddress))
        return;

    auto va = (uint64_t)LogicalAddress;

    if (va & (PAGE_SIZE - 1)) {
        Size += va & (PAGE_SIZE - 1);
        va &= ~(PAGE_SIZE - 1);
    }

    if (Size & (PAGE_SIZE - 1))
        Size += PAGE_SIZE - (Size & (PAGE_SIZE - 1));

    if (Size == 0)
        return;

    auto num_pages = (uint32_t)(Size >> PAGE_SHIFT);

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, num_pages);
}

extern "C"
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber,
                                          ACPI_OSD_HANDLER ServiceRoutine,
                                          void* Context) {
    auto h = new acpi_irq_handler;
    if (!h)
        return AE_NO_MEMORY;

    h->service_routine = ServiceRoutine;
    h->context = Context;

    lock_guard lock(irq_handlers_lock);

    for (auto le = irq_handlers.next; le != &irq_handlers; le = le->next) {
        auto& n = containing_record<acpi_irq_num>(le);

        if (n.irq == InterruptNumber) {
            insert_tail_list(n.handlers, h->le);
            return AE_OK;
        }
    }

    auto n = new acpi_irq_num;
    if (!n) {
        delete h;
        return AE_NO_MEMORY;
    }

    n->irq = InterruptNumber;
    insert_tail_list(irq_handlers, n->le);
    insert_tail_list(n->handlers, h->le);

    if (!add_irq_handler(InterruptNumber, false, acpi_isr_func, n)) {
        print("add_irq_handler failed\n");
        delete h;
        delete n;

        return AE_ERROR;
    }

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber,
                                         ACPI_OSD_HANDLER ServiceRoutine) {
    lock_guard lock(irq_handlers_lock);

    for (auto le = irq_handlers.next; le != &irq_handlers; le = le->next) {
        auto& n = containing_record<acpi_irq_num>(le);

        if (n.irq == InterruptNumber) {
            for (auto le2 = n.handlers.next; le2 != &n.handlers; le2 = le2->next) {
                auto& h = containing_record<acpi_irq_handler>(le2);

                if (h.service_routine == ServiceRoutine) {
                    remove_entry_list(h.le);
                    delete &h;

                    // FIXME - if n.handlers now empty, deregister IRQ handler and free n

                    return AE_OK;
                }
            }

            return AE_NOT_FOUND;
        }
    }

    return AE_NOT_FOUND;
}

// work round ICE in GCC 15
static ACPI_THREAD_ID _AcpiOsGetThreadId() {
    return (ACPI_THREAD_ID)get_current_thread();
}

extern "C"
ACPI_THREAD_ID AcpiOsGetThreadId() {
    return _AcpiOsGetThreadId();
}

extern "C"
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE /*Type*/, ACPI_OSD_EXEC_CALLBACK /*Function*/,
                          void* /*Context*/) {
    print("FIXME - AcpiOsExecute\n");

    return AE_ERROR;
}

extern "C"
void AcpiOsWaitEventsComplete() {
    print("FIXME - AcpiOsWaitEventsComplete\n");
}

extern "C"
void AcpiOsSleep(UINT64 /*Milliseconds*/) {
    print("FIXME - AcpiOsSleep\n");
}

extern "C"
void AcpiOsStall(UINT32 /*Microseconds*/) {
    print("FIXME - AcpiOsStall\n");
}

#ifdef __x86_64__
extern "C"
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width) {
    if (Width == 32)
        *Value = inl((uint16_t)Address);
    else if (Width == 16)
        *Value = inw((uint16_t)Address);
    else if (Width == 8)
        *Value = inb((uint16_t)Address);
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
}
#else
extern "C"
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS, UINT32*, UINT32) {
    return AE_NOT_IMPLEMENTED;
}
#endif

#ifdef __x86_64__
extern "C"
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    if (Width == 32)
        outl((uint16_t)Address, Value);
    else if (Width == 16)
        outw((uint16_t)Address, (uint16_t)Value);
    else if (Width == 8)
        outb((uint16_t)Address, (uint8_t)Value);
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
}
#else
extern "C"
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS, UINT32, UINT32) {
    return AE_NOT_IMPLEMENTED;
}
#endif

extern "C"
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS /*Address*/, UINT64* /*Value*/,
                             UINT32 /*Width*/) {
    print("FIXME - AcpiOsReadMemory\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value,
                              UINT32 Width) {
    if (Width != 8 && Width != 16 && Width != 32 && Width != 64)
        return AE_ERROR;

    auto ptr = AcpiOsMapMemory(Address, Width / 8);
    if (!ptr)
        return AE_ERROR;

    switch (Width) {
        case 8:
            *(uint8_t*)ptr = (uint8_t)Value;
            break;

        case 16:
            *(uint16_t*)ptr = (uint16_t)Value;
            break;

        case 32:
            *(uint32_t*)ptr = (uint32_t)Value;
            break;

        case 64:
            *(uint64_t*)ptr = Value;
            break;
    }

    AcpiOsUnmapMemory(ptr, Width / 8);

    return AE_OK;
}

export constexpr uint64_t pci_pa(const ACPI_MCFG_ALLOCATION& item, uint8_t bus, uint8_t device, uint8_t function) {
    return item.Address + ((uint64_t)((bus - item.StartBusNumber) << 20) | (uint64_t)(device << 15) | (uint64_t)(function << 12));
}

extern "C"
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg,
                                       UINT64* Value, UINT32 Width) {
    ACPI_TABLE_HEADER* tbl;

    auto status = AcpiGetTable((char*)ACPI_SIG_MCFG, 1, &tbl);
    if (ACPI_FAILURE(status))
        return status;

    if (tbl->Length < sizeof(ACPI_TABLE_MCFG))
        return AE_ERROR;

    auto items = span((ACPI_MCFG_ALLOCATION*)((uint8_t*)tbl + sizeof(ACPI_TABLE_MCFG)),
                      (tbl->Length - sizeof(ACPI_TABLE_MCFG)) / sizeof(ACPI_MCFG_ALLOCATION));

    if (items.empty())
        return AE_NOT_FOUND;

    for (auto& it : items) {
        if (it.PciSegment != PciId->Segment)
            continue;

        if (PciId->Bus < it.StartBusNumber || PciId->Bus > it.EndBusNumber)
            continue;

        auto pa = pci_pa(it, (uint8_t)PciId->Bus, (uint8_t)PciId->Device, (uint8_t)PciId->Function) + Reg;
        auto va = phys_address(pa);

        if (Width == 8) {
            *Value = *(volatile uint8_t*)va;
            return AE_OK;
        } else if (Width == 16) {
            *Value = *(volatile uint16_t*)va;
            return AE_OK;
        } else if (Width == 32) {
            *Value = *(volatile uint32_t*)va;
            return AE_OK;
        } else if (Width == 64) {
            *Value = *(volatile uint64_t*)va;
            return AE_OK;
        } else
            return AE_BAD_PARAMETER;
    }

    return AE_NOT_FOUND;
}

extern "C"
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* /*PciId*/, UINT32 /*Reg*/,
                                        UINT64 /*Value*/, UINT32 /*Width*/) {
    print("FIXME - AcpiOsWritePciConfiguration\n");

    return AE_ERROR;
}

extern "C"
UINT64 AcpiOsGetTimer() {
    // convert to units of 100ns
    return (uint64_t)(kernel_time_page.system_time * 10000000 / SYSTEM_TIME_RESOLUTION);
}

extern "C"
ACPI_STATUS AcpiOsSignal(UINT32 /*Function*/, void* /*Info*/) {
    print("FIXME - AcpiOsSignal\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    print("FIXME - AcpiOsEnterSleep({:x}, {:x}, {:x})\n", SleepState, RegaValue, RegbValue);

    return AE_OK;
}

extern "C"
void AcpiOsPrintf(const char* /*Format*/, ...) {
    print("FIXME - AcpiOsPrintf\n");
}

export ACPI_STATUS acpi_evaluate_integer(ACPI_HANDLE handle, ACPI_STRING path,
                                         uint64_t& val) {
    ACPI_OBJECT obj;
    ACPI_BUFFER buf;

    buf.Length = sizeof(obj);
    buf.Pointer = &obj;

    auto status = AcpiEvaluateObject(handle, path, nullptr, &buf);
    if (ACPI_FAILURE(status))
        return status;

    if (obj.Type != ACPI_TYPE_INTEGER)
        return AE_BAD_DATA;

    val = obj.Integer.Value;

    return AE_OK;
}
