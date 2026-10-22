// SPDX-License-Identifier: LGPL-2.1-or-later
#include <syslog.h>
#include <algorithm>

using namespace std;

static int log_mask = 0xff;

extern "C"
void openlog(const char* /*ident*/, int /*logopt*/, int /*facility*/) {
    // FIXME
}

extern "C"
void syslog(int /*priority*/, const char* /*message*/, ...) {
    // FIXME
}

extern "C"
void closelog() {
    // FIXME
}

extern "C"
int setlogmask(int mask) {
    if (mask == 0)
        return log_mask;

    swap(log_mask, mask);

    return mask;
}
