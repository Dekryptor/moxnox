#include <syslog.h>
#include <algorithm>

using namespace std;

static int log_mask = 0;

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
    swap(log_mask, mask);

    return mask;
}
