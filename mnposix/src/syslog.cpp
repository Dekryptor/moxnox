#include <syslog.h>

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
