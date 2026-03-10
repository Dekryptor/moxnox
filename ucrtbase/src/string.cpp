#include <stdlib.h>
#include <string_view>

import ucrtbase;

using namespace std;

extern "C"
char* _strdup(const char* s) {
    auto sv = string_view(s);

    auto ret = (char*)malloc(sv.size() + 1);
    if (!ret)
        return nullptr;

    memcpy(ret, s, sv.size() + 1);

    return ret;
}
