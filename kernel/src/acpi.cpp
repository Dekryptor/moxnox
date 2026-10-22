// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <string_view>
#include <stdint.h>

export module acpi;

import format;
import list;
import acpica;

using namespace std;

export uint64_t rsdp_pa;

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

export constexpr uint64_t pci_pa(const ACPI_MCFG_ALLOCATION& item, uint8_t bus, uint8_t device, uint8_t function) {
    return item.Address + ((uint64_t)((bus - item.StartBusNumber) << 20) | (uint64_t)(device << 15) | (uint64_t)(function << 12));
}

export void init_acpi_early();
export void init_acpi();
export void find_acpi_devices();
export ACPI_STATUS acpi_evaluate_integer(ACPI_HANDLE handle, ACPI_STRING path,
                                         uint64_t& val);
