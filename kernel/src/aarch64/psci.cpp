module;

#include <span>
#include <string_view>
#include "../acpica.h"

export module psci;

import acpi;
import print;
import format;

using namespace std;

static const uint32_t PSCI_VERSION_FUNC_ID = 0x84000000;
static const uint32_t CPU_ON_FUNC_ID = 0xc4000003;

static bool use_hvc;
export uint32_t psci_version = 0;

export enum class PSCI_STATUS : int64_t {
    SUCCESS = 0,
    NOT_SUPPORTED = -1,
    INVALID_PARAMETERS = -2,
    DENIED = -3,
    ALREADY_ON = -4,
    ON_PENDING = -5,
    INTERNAL_FAILURE = -6,
    NOT_PRESENT = -7,
    DISABLED = -8,
    INVALID_ADDRESS = -9,
};

export span<char> do_format_arg(span<char> sp, string_view, PSCI_STATUS arg) {
    switch (arg) {
        using enum PSCI_STATUS;

        case SUCCESS:
            return do_format_arg(sp, "", "SUCCESS");
        case NOT_SUPPORTED:
            return do_format_arg(sp, "", "NOT_SUPPORTED");
        case INVALID_PARAMETERS:
            return do_format_arg(sp, "", "INVALID_PARAMETERS");
        case DENIED:
            return do_format_arg(sp, "", "DENIED");
        case ALREADY_ON:
            return do_format_arg(sp, "", "ALREADY_ON");
        case ON_PENDING:
            return do_format_arg(sp, "", "ON_PENDING");
        case INTERNAL_FAILURE:
            return do_format_arg(sp, "", "INTERNAL_FAILURE");
        case NOT_PRESENT:
            return do_format_arg(sp, "", "NOT_PRESENT");
        case DISABLED:
            return do_format_arg(sp, "", "DISABLED");
        case INVALID_ADDRESS:
            return do_format_arg(sp, "", "INVALID_ADDRESS");
        default:
            return do_format_arg(sp, "", (int64_t)arg);
    }
}

export template<>
consteval bool format_spec_valid<PSCI_STATUS>(string_view fmt) {
    return fmt.empty();
}

uint64_t psci_func(uint32_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;

    if (use_hvc) {
        __asm __volatile (
            "mov w0, %w[func]\n"
            "mov x1, %x[arg1]\n"
            "mov x2, %x[arg2]\n"
            "mov x3, %x[arg3]\n"
            "hvc #0\n"
            "mov %x[res], x0\n"
            : [res] "=r" (ret)
            : [func] "r" (id), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3)
            : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
              "x11", "x12", "x13", "x14", "x15", "x16", "x17"
        );
    } else {
        __asm __volatile (
            "mov w0, %w[func]\n"
            "mov x1, %x[arg1]\n"
            "mov x2, %x[arg2]\n"
            "mov x3, %x[arg3]\n"
            "smc #0\n"
            "mov %x[res], x0\n"
            : [res] "=r" (ret)
            : [func] "r" (id), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3)
            : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
              "x11", "x12", "x13", "x14", "x15", "x16", "x17"
        );
    }

    return ret;
}

static uint32_t PSCI_VERSION() {
    return (uint32_t)psci_func(PSCI_VERSION_FUNC_ID, 0, 0, 0);
}

export PSCI_STATUS CPU_ON(uint64_t target_cpu, uint64_t entry_point_address,
                          uint64_t context_id) {
    return (PSCI_STATUS)psci_func(CPU_ON_FUNC_ID, target_cpu,
                                  entry_point_address, context_id);
}

export bool init_psci() {
    ACPI_TABLE_HEADER* tbl;

    // FIXME - don't show error if not in ACPI mode (check devicetree instead)

    auto status = AcpiGetTable((char*)ACPI_SIG_FADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return false;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_FADT)) {
        print("FADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_FADT));
        return false;
    }

    auto& fadt = *(ACPI_TABLE_FADT*)tbl;

    if (!(fadt.ArmBootFlags & ACPI_FADT_PSCI_COMPLIANT))
        return false;

    use_hvc = fadt.ArmBootFlags & ACPI_FADT_PSCI_USE_HVC;

    psci_version = PSCI_VERSION();

    if ((psci_version >> 16) == 0 && (psci_version & 0xffff) < 2) {
        print("psci version was {}.{}, expected at least 0.2\n",
              psci_version >> 16, psci_version & 0xffff);
        psci_version = 0;
        return false;
    }

    print("using psci {}.{}\n", psci_version >> 16, psci_version & 0xffff);

    return true;
}
