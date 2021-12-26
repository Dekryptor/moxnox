#include "kernel.h"
#include "objects.h"

void volume::probe() {
    // FIXME - partitions (GPT and MBR)

    // if no partitions found or is already partition, probe for filesystems
    btrfs_probe(*this);
}
