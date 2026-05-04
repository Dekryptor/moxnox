#include <string.h>

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
