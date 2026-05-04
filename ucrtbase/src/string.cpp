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

extern "C"
int _stricmp(const char* s1, const char* s2) {
    size_t i = 0;

    // FIXME - Unicode?

    while (true) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 >= 'A' && c1 <= 'Z')
            c1 = c1 - 'A' + 'a';

        if (c2 >= 'A' && c2 <= 'Z')
            c2 = c2 - 'A' + 'a';

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        i++;
    }
}
