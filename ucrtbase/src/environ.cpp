#include <string_view>
#include <span>
#include <stdlib.h>

import nt;
import ucrtbase;
import print;
import unicode;

using namespace std;

extern "C"
char* getenv(const char* name) {
    auto namesv = string_view(name);

    auto env = *__p__environ();
    if (!env)
        return nullptr;

    while (*env) {
        auto sv = string_view(*env);

        auto eq = sv.find('=');

        if (eq == string_view::npos) {
            env++;
            continue;
        }

        if (string_view(sv.data(), eq) == namesv)
            return (char*)(sv.data() + eq + 1);

        env++;
    }

    return nullptr;
}
