module;

#include <stdint.h>
#include <span>
#include <string_view>

export module aarch64;

import format;

using namespace std;

export union ARM64_SCTLR_EL1 {
    struct {
        uint64_t M : 1;
        uint64_t A : 1;
        uint64_t C : 1;
        uint64_t SA : 1;
        uint64_t SA0 : 1;
        uint64_t : 1;
        uint64_t nAA : 1;
        uint64_t : 2;
        uint64_t UMA : 1;
        uint64_t EnRCTX : 1;
        uint64_t : 1;
        uint64_t I : 1;
        uint64_t EnDB : 1;
        uint64_t DZE : 1;
        uint64_t UCT : 1;
        uint64_t nTWI : 1;
        uint64_t BR : 1;
        uint64_t nTWE : 1;
        uint64_t WXN : 1;
        uint64_t : 1;
        uint64_t IESB : 1;
        uint64_t : 1;
        uint64_t SPAN : 1;
        uint64_t E0E : 1;
        uint64_t EE : 1;
        uint64_t UCI : 1;
        uint64_t EnDA : 1;
        uint64_t : 2;
        uint64_t EnIB : 1;
        uint64_t EnIA : 1;
        uint64_t : 12;
        uint64_t DSSBS : 1;
        uint64_t : 19;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_SCTLR_EL1) == 8);

export union ARM64_TCR_EL1 {
    struct {
        uint64_t T0SZ : 6;
        uint64_t : 1;
        uint64_t EPD0 : 1;
        uint64_t IRGN0 : 2;
        uint64_t ORGN0 : 2;
        uint64_t SH0 : 2;
        uint64_t TG0 : 2;
        uint64_t T1SZ : 6;
        uint64_t A1 : 1;
        uint64_t EPD1 : 1;
        uint64_t IRGN1 : 2;
        uint64_t ORGN1 : 2;
        uint64_t SH1 : 2;
        uint64_t TG1 : 2;
        uint64_t IPS : 3;
        uint64_t : 1;
        uint64_t AS : 1;
        uint64_t TBI0 : 1;
        uint64_t TBI1 : 1;
        uint64_t HA : 1;
        uint64_t HD : 1;
        uint64_t HPD0 : 1;
        uint64_t HPD1 : 1;
        uint64_t HWU059 : 1;
        uint64_t HWU060 : 1;
        uint64_t HWU061 : 1;
        uint64_t HWU062 : 1;
        uint64_t HWU159 : 1;
        uint64_t HWU160 : 1;
        uint64_t HWU161 : 1;
        uint64_t HWU162 : 1;
        uint64_t TBID0 : 1;
        uint64_t TBID1 : 1;
        uint64_t NFD0 : 1;
        uint64_t NFD1 : 1;
        uint64_t E0PD0 : 1;
        uint64_t E0PD1 : 1;
        uint64_t TCMA0 : 1;
        uint64_t TCMA1 : 1;
        uint64_t DS : 1;
        uint64_t MTX0 : 1;
        uint64_t MTX1 : 1;
        uint64_t : 2;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_TCR_EL1) == 8);

export const uint64_t ARM64_PTE_AP_EL1RW = 0;
export const uint64_t ARM64_PTE_AP_EL0RW = 1;
export const uint64_t ARM64_PTE_AP_EL1RO = 2;
export const uint64_t ARM64_PTE_AP_EL0RO = 3;

export struct ARM64_HARDWARE_PTE {
    uint64_t valid : 1;
    uint64_t table_entry : 1;
    uint64_t AttrIndex : 3;
    uint64_t NS : 1;
    uint64_t AP : 2;
    uint64_t SH : 2;
    uint64_t AF : 1;
    uint64_t nG : 1;
    uint64_t pfn : 39;
    uint64_t DBM : 1;
    uint64_t Contig : 1;
    uint64_t PXN : 1;
    uint64_t UXN : 1;
    uint64_t allow_write : 1; // custom
    uint64_t cow : 1; // custom
    uint64_t reserved : 1; // custom
    uint64_t software : 1;
    uint64_t imp_def : 4;
    uint64_t : 1;
} __attribute((packed));

static_assert(sizeof(ARM64_HARDWARE_PTE) == 8);

export union ARM64_ESR {
    struct {
        uint64_t ISS : 25;
        uint64_t IL : 1;
        uint64_t EC : 6;
        uint64_t ISS2 : 24;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_ESR) == 8);

export union ARM64_CPACR_EL1 {
    struct {
        uint64_t : 16;
        uint64_t ZEN : 2;
        uint64_t : 2;
        uint64_t FPEN : 2;
        uint64_t : 2;
        uint64_t SMEN : 2;
        uint64_t : 2;
        uint64_t TTA : 1;
        uint64_t E0POE : 1;
        uint64_t TAM : 1;
        uint64_t TCPAC : 1;
        uint64_t : 32;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_CPACR_EL1) == 8);

export union ARM64_HCR_EL2 {
    struct {
        uint64_t VM : 1;
        uint64_t SWIO : 1;
        uint64_t PTW : 1;
        uint64_t FMO : 1;
        uint64_t IMO : 1;
        uint64_t AMO : 1;
        uint64_t VF : 1;
        uint64_t VI : 1;
        uint64_t VSE : 1;
        uint64_t FB : 1;
        uint64_t BSU : 2;
        uint64_t DC : 1;
        uint64_t TWI : 1;
        uint64_t TWE : 1;
        uint64_t TID0 : 1;
        uint64_t TID1 : 1;
        uint64_t TID2 : 1;
        uint64_t TID3 : 1;
        uint64_t TSC : 1;
        uint64_t TIDCP : 1;
        uint64_t TACR : 1;
        uint64_t TSW : 1;
        uint64_t TPCP : 1;
        uint64_t TPU : 1;
        uint64_t TTLB : 1;
        uint64_t TVM : 1;
        uint64_t TGE : 1;
        uint64_t TDZ : 1;
        uint64_t HCD : 1;
        uint64_t TRVM : 1;
        uint64_t RW : 1;
        uint64_t CD : 1;
        uint64_t ID : 1;
        uint64_t E2H : 1;
        uint64_t TLOR : 1;
        uint64_t TERR : 1;
        uint64_t TEA : 1;
        uint64_t : 2;
        uint64_t APK : 1;
        uint64_t API : 1;
        uint64_t NV : 1;
        uint64_t NV1 : 1;
        uint64_t AT : 1;
        uint64_t NV2 : 1;
        uint64_t FWB : 1;
        uint64_t FIEN : 1;
        uint64_t GPF : 1;
        uint64_t TID4 : 1;
        uint64_t TICAB : 1;
        uint64_t AMVOFFEN : 1;
        uint64_t TOCU : 1;
        uint64_t EnSCXT : 1;
        uint64_t TTLBIS : 1;
        uint64_t TTLBOS : 1;
        uint64_t ATA : 1;
        uint64_t DCT : 1;
        uint64_t TID5 : 1;
        uint64_t TWEDEn : 1;
        uint64_t TWEDEL : 4;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_HCR_EL2) == 8);

export template<>
consteval bool format_spec_valid<ARM64_SCTLR_EL1>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const ARM64_SCTLR_EL1& s) {
    sp = do_format_arg(sp, "", "M=");
    sp = do_format_arg(sp, "", s.M);
    sp = do_format_arg(sp, "", ", A=");
    sp = do_format_arg(sp, "", s.A);
    sp = do_format_arg(sp, "", ", C=");
    sp = do_format_arg(sp, "", s.C);
    sp = do_format_arg(sp, "", ", SA=");
    sp = do_format_arg(sp, "", s.SA);
    sp = do_format_arg(sp, "", ", SA0=");
    sp = do_format_arg(sp, "", s.SA0);
    sp = do_format_arg(sp, "", ", nAA=");
    sp = do_format_arg(sp, "", s.nAA);
    sp = do_format_arg(sp, "", ", UMA=");
    sp = do_format_arg(sp, "", s.UMA);
    sp = do_format_arg(sp, "", ", EnRCTX=");
    sp = do_format_arg(sp, "", s.EnRCTX);
    sp = do_format_arg(sp, "", ", I=");
    sp = do_format_arg(sp, "", s.I);
    sp = do_format_arg(sp, "", ", EnDB=");
    sp = do_format_arg(sp, "", s.EnDB);
    sp = do_format_arg(sp, "", ", DZE=");
    sp = do_format_arg(sp, "", s.DZE);
    sp = do_format_arg(sp, "", ", UCT=");
    sp = do_format_arg(sp, "", s.UCT);
    sp = do_format_arg(sp, "", ", nTWI=");
    sp = do_format_arg(sp, "", s.nTWI);
    sp = do_format_arg(sp, "", ", BR=");
    sp = do_format_arg(sp, "", s.BR);
    sp = do_format_arg(sp, "", ", nTWE=");
    sp = do_format_arg(sp, "", s.nTWE);
    sp = do_format_arg(sp, "", ", WXN=");
    sp = do_format_arg(sp, "", s.WXN);
    sp = do_format_arg(sp, "", ", IESB=");
    sp = do_format_arg(sp, "", s.IESB);
    sp = do_format_arg(sp, "", ", SPAN=");
    sp = do_format_arg(sp, "", s.SPAN);
    sp = do_format_arg(sp, "", ", E0E=");
    sp = do_format_arg(sp, "", s.E0E);
    sp = do_format_arg(sp, "", ", EE=");
    sp = do_format_arg(sp, "", s.EE);
    sp = do_format_arg(sp, "", ", UCI=");
    sp = do_format_arg(sp, "", s.UCI);
    sp = do_format_arg(sp, "", ", EnDA=");
    sp = do_format_arg(sp, "", s.EnDA);
    sp = do_format_arg(sp, "", ", EnIB=");
    sp = do_format_arg(sp, "", s.EnIB);
    sp = do_format_arg(sp, "", ", EnIA=");
    sp = do_format_arg(sp, "", s.EnIA);
    sp = do_format_arg(sp, "", ", DSSBS=");
    sp = do_format_arg(sp, "", s.DSSBS);

    return sp;
}

export template<>
consteval bool format_spec_valid<ARM64_TCR_EL1>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const ARM64_TCR_EL1& t) {
    sp = do_format_arg(sp, "", "T0SZ=");
    sp = do_format_arg(sp, "", t.T0SZ);
    sp = do_format_arg(sp, "", ", EPD0=");
    sp = do_format_arg(sp, "", t.EPD0);
    sp = do_format_arg(sp, "", ", IRGN0=");
    sp = do_format_arg(sp, "", t.IRGN0);
    sp = do_format_arg(sp, "", ", ORGN0=");
    sp = do_format_arg(sp, "", t.ORGN0);
    sp = do_format_arg(sp, "", ", SH0=");
    sp = do_format_arg(sp, "", t.SH0);
    sp = do_format_arg(sp, "", ", TG0=");
    sp = do_format_arg(sp, "", t.TG0);
    sp = do_format_arg(sp, "", ", T1SZ=");
    sp = do_format_arg(sp, "", t.T1SZ);
    sp = do_format_arg(sp, "", ", A1=");
    sp = do_format_arg(sp, "", t.A1);
    sp = do_format_arg(sp, "", ", EPD1=");
    sp = do_format_arg(sp, "", t.EPD1);
    sp = do_format_arg(sp, "", ", IRGN1=");
    sp = do_format_arg(sp, "", t.IRGN1);
    sp = do_format_arg(sp, "", ", ORGN1=");
    sp = do_format_arg(sp, "", t.ORGN1);
    sp = do_format_arg(sp, "", ", SH1=");
    sp = do_format_arg(sp, "", t.SH1);
    sp = do_format_arg(sp, "", ", TG1=");
    sp = do_format_arg(sp, "", t.TG1);
    sp = do_format_arg(sp, "", ", IPS=");
    sp = do_format_arg(sp, "", t.IPS);
    sp = do_format_arg(sp, "", ", AS=");
    sp = do_format_arg(sp, "", t.AS);
    sp = do_format_arg(sp, "", ", TBI0=");
    sp = do_format_arg(sp, "", t.TBI0);
    sp = do_format_arg(sp, "", ", TBI1=");
    sp = do_format_arg(sp, "", t.TBI1);
    sp = do_format_arg(sp, "", ", HA=");
    sp = do_format_arg(sp, "", t.HA);
    sp = do_format_arg(sp, "", ", HD=");
    sp = do_format_arg(sp, "", t.HD);
    sp = do_format_arg(sp, "", ", HPD0=");
    sp = do_format_arg(sp, "", t.HPD0);
    sp = do_format_arg(sp, "", ", HPD1=");
    sp = do_format_arg(sp, "", t.HPD1);
    sp = do_format_arg(sp, "", ", HWU059=");
    sp = do_format_arg(sp, "", t.HWU059);
    sp = do_format_arg(sp, "", ", HWU060=");
    sp = do_format_arg(sp, "", t.HWU060);
    sp = do_format_arg(sp, "", ", HWU061=");
    sp = do_format_arg(sp, "", t.HWU061);
    sp = do_format_arg(sp, "", ", HWU062=");
    sp = do_format_arg(sp, "", t.HWU062);
    sp = do_format_arg(sp, "", ", HWU159=");
    sp = do_format_arg(sp, "", t.HWU159);
    sp = do_format_arg(sp, "", ", HWU160=");
    sp = do_format_arg(sp, "", t.HWU160);
    sp = do_format_arg(sp, "", ", HWU161=");
    sp = do_format_arg(sp, "", t.HWU161);
    sp = do_format_arg(sp, "", ", HWU162=");
    sp = do_format_arg(sp, "", t.HWU162);
    sp = do_format_arg(sp, "", ", TBID0=");
    sp = do_format_arg(sp, "", t.TBID0);
    sp = do_format_arg(sp, "", ", TBID1=");
    sp = do_format_arg(sp, "", t.TBID1);
    sp = do_format_arg(sp, "", ", NFD0=");
    sp = do_format_arg(sp, "", t.NFD0);
    sp = do_format_arg(sp, "", ", NFD1=");
    sp = do_format_arg(sp, "", t.NFD1);
    sp = do_format_arg(sp, "", ", E0PD0=");
    sp = do_format_arg(sp, "", t.E0PD0);
    sp = do_format_arg(sp, "", ", E0PD1=");
    sp = do_format_arg(sp, "", t.E0PD1);
    sp = do_format_arg(sp, "", ", TCMA0=");
    sp = do_format_arg(sp, "", t.TCMA0);
    sp = do_format_arg(sp, "", ", TCMA1=");
    sp = do_format_arg(sp, "", t.TCMA1);
    sp = do_format_arg(sp, "", ", DS=");
    sp = do_format_arg(sp, "", t.DS);
    sp = do_format_arg(sp, "", ", MTX0=");
    sp = do_format_arg(sp, "", t.MTX0);
    sp = do_format_arg(sp, "", ", MTX1=");
    sp = do_format_arg(sp, "", t.MTX1);

    return sp;
}

export template<>
consteval bool format_spec_valid<ARM64_HARDWARE_PTE>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const ARM64_HARDWARE_PTE& p) {
    sp = do_format_arg(sp, "", "valid=");
    sp = do_format_arg(sp, "", p.valid);
    sp = do_format_arg(sp, "", ", table_entry=");
    sp = do_format_arg(sp, "", p.table_entry);
    sp = do_format_arg(sp, "", ", AttrIndex=");
    sp = do_format_arg(sp, "", p.AttrIndex);
    sp = do_format_arg(sp, "", ", NS=");
    sp = do_format_arg(sp, "", p.NS);
    sp = do_format_arg(sp, "", ", AP=");
    sp = do_format_arg(sp, "", p.AP);
    sp = do_format_arg(sp, "", ", SH=");
    sp = do_format_arg(sp, "", p.SH);
    sp = do_format_arg(sp, "", ", AF=");
    sp = do_format_arg(sp, "", p.AF);
    sp = do_format_arg(sp, "", ", nG=");
    sp = do_format_arg(sp, "", p.nG);
    sp = do_format_arg(sp, "", ", pfn=");
    sp = do_format_arg(sp, ":x", p.pfn);
    sp = do_format_arg(sp, "", ", DBM=");
    sp = do_format_arg(sp, "", p.DBM);
    sp = do_format_arg(sp, "", ", Contig=");
    sp = do_format_arg(sp, "", p.Contig);
    sp = do_format_arg(sp, "", ", PXN=");
    sp = do_format_arg(sp, "", p.PXN);
    sp = do_format_arg(sp, "", ", UXN=");
    sp = do_format_arg(sp, "", p.UXN);
    sp = do_format_arg(sp, "", ", allow_write=");
    sp = do_format_arg(sp, "", p.allow_write);
    sp = do_format_arg(sp, "", ", cow=");
    sp = do_format_arg(sp, "", p.cow);
    sp = do_format_arg(sp, "", ", reserved=");
    sp = do_format_arg(sp, "", p.reserved);
    sp = do_format_arg(sp, "", ", software=");
    sp = do_format_arg(sp, "", p.software);
    sp = do_format_arg(sp, "", ", imp_def=");
    sp = do_format_arg(sp, "", p.imp_def);

    return sp;
}

__attribute__((always_inline))
export uint64_t inline tcr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs\t%0, tcr_el1"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
export void inline tcr_el1_store(uint64_t v) {
    __asm __volatile (
        "msr\ttcr_el1, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
export uint64_t inline hcr_el2_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %0, hcr_el2"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
export void inline hcr_el2_store(uint64_t v) {
    __asm __volatile (
        "msr hcr_el2, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
export uint64_t sctlr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], sctlr_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
export uint64_t cpacr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], cpacr_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
export uint64_t mair_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], mair_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
export void inline mair_el1_store(uint64_t v) {
    __asm __volatile (
        "msr mair_el1, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
export void inline sctlr_el1_store(uint64_t v) {
    __asm __volatile (
        "msr sctlr_el1, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
export void inline cpacr_el1_store(uint64_t v) {
    __asm __volatile (
        "msr cpacr_el1, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
export uint64_t inline ttbr0_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], ttbr0_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
export uint64_t inline ttbr1_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], ttbr1_el1"
        : [res] "=r" (ret)
    );

    return ret;
}
