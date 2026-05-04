#include <string.h>
#include <stdlib.h>
#include <limits.h>

import ucrtbase;

#undef errno
import errno;

static char dot[] = ".";

extern "C"
char* dirname(char* path) {
    if (!path || *path == '\0')
        return dot;

    size_t len = strlen(path);

    while (len > 1 && path[len - 1] == '/') {
        len--;
        path[len] = '\0';
    }

    char* last_slash = strrchr(path, '/');

    if (!last_slash)
        return dot;

    if (last_slash == path) {
        path[1] = '\0';
        return path;
    }

    while (last_slash > path && *(last_slash - 1) == '/') {
        last_slash--;
    }

    if (last_slash == path)
        path[1] = '\0';
    else
        *last_slash = '\0';

    return path;
}

extern "C"
char* realpath(const char* path, char* resolved_path) {
    char tmp[PATH_MAX];
    char* ret;

    // FIXME - symlinks

    if (!path || *path == '\0') {
        *_errno() = EINVAL;
        return nullptr;
    }

    if (path[0] == '/') {
        if (strlen(path) >= PATH_MAX) {
            *_errno() = ENAMETOOLONG;
            return nullptr;
        }

        strcpy(tmp, path);
    } else {
        if (!_getcwd(tmp, PATH_MAX))
            return nullptr;

        size_t cwd_len = strlen(tmp);

        if (cwd_len + 1 + strlen(path) >= PATH_MAX) {
            *_errno() = ENAMETOOLONG;
            return nullptr;
        }

        tmp[cwd_len] = '/';
        strcpy(tmp + cwd_len + 1, path);
    }

    if (resolved_path)
        ret = resolved_path;
    else {
        ret = (char*)malloc(PATH_MAX);
        if (!ret) {
            *_errno() = ENOMEM;
            return nullptr;
        }
    }

    size_t ret_len = 1;

    ret[0] = '/';
    ret[1] = '\0';

    char* p = tmp;
    if (*p == '/')
        p++;

    while (*p) {
        while (*p == '/') {
            p++;
        }

        if (!*p)
            break;

        char* comp = p;
        while (*p && *p != '/') {
            p++;
        }

        size_t comp_len = p - comp;

        if (comp_len == 2 && comp[0] == '.' && comp[1] == '.') {
            char* last = ret + ret_len - 1;

            while (last > ret && *last != '/') {
                last--;
            }

            if (last == ret)
                ret_len = 1;
            else
                ret_len = (size_t)(last - ret);

            ret[ret_len] = '\0';
        } else if (comp_len != 1 || comp[0] != '.') {
            size_t need = ret_len + comp_len + 1;

            if (ret_len > 1)
                need++;

            if (need > PATH_MAX) {
                if (!resolved_path)
                    free(ret);

                *_errno() = ENAMETOOLONG;
                return nullptr;
            }

            if (ret_len > 1) {
                ret[ret_len] = '/';
                ret_len++;
            }

            memcpy(ret + ret_len, comp, comp_len);
            ret_len += comp_len;
            ret[ret_len] = '\0';
        }
    }

    return ret;
}
