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
