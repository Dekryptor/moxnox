#include <stdint.h>

static const uint32_t DEVICE_TREE_MAGIC = 0xd00dfeed;
static const uint32_t DEVICE_TREE_VERSION = 17;

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

enum class fdt_type : uint32_t {
    BEGIN_NODE = 1,
    END_NODE = 2,
    PROP = 3,
    NOP = 4,
    END = 5
};

struct dtb_prop {
    uint32_t len;
    uint32_t nameoff;
};

struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};
