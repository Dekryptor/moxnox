#include <sys/types.h>
#include <sys/stat.h>

import nt;
import unicode;

#undef errno
import errno;

using namespace std;

extern "C"
int mkdir(const char* path, mode_t /*mode*/) {
    return _mkdir(path);
}
