#include <string_view>
#include <span>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

import nt;
import ucrtbase;
import print;
import unicode;

using namespace std;

extern "C" int* __errno();
extern "C" char*** ___p__environ();

extern "C"
char* getenv(const char* name) {
    auto namesv = string_view(name);

    auto env = *___p__environ();
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

extern "C"
errno_t _getenv_s(size_t* len, char* value, rsize_t valuesz, const char* name) {
    if (!len) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    if (!value && valuesz > 0) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    if (!name) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    auto var = getenv(name);
    if (!var) {
        *len = 0;
        *__errno() = EINVAL;
        return EINVAL;
    }

    *len = strlen(var) + 1;

    if (valuesz < *len) {
        *__errno() = ERANGE;
        return ERANGE;
    }

    strcpy(value, var);

    return 0;
}
