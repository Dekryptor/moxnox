#include "kernel.h"
#include "objects.h"

using namespace std;

size_t file::read(span<uint8_t> s) {
    auto ret = ino.read(s, file_offset);

    if (ret > 0) {
        lock_guard l(lock);
        file_offset += ret;
    }

    return ret;
}

size_t file::read(span<uint8_t> s, uint64_t offset) {
    auto ret = ino.read(s, offset);

    if (ret > 0) {
        lock_guard l(lock);
        file_offset = offset + ret;
    }

    return ret;
}
