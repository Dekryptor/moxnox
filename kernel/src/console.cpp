#include "kernel.h"
#include <span>
#include <cstring>
#include <bit>

using namespace std;

static constexpr unsigned int COLS = 80;
static constexpr unsigned int ROWS = 25;

static const uint64_t CONSOLE_PA = 0xb8000;

auto video = span((uint16_t*)0xfffffaa000000000, COLS * ROWS);

bool init_console() {
    if (!map_page(CONSOLE_PA, (uintptr_t)video.data()))
        return false;

    // clear console
    memset(video.data(), 0, video.size_bytes());

    return true;
}
