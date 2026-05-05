#include "moxnox-git.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * .exe-aware replacement for git's locate_in_PATH (see moxnox-git.h). Walks
 * $PATH and, in each entry, tries both "<dir>/<file>" and "<dir>/<file>.exe",
 * returning a malloc'd path to the first that exists. Mirrors the PATH search
 * semantics of the POSIX locate_in_PATH it replaces.
 */
char *moxnox_locate_in_PATH(const char *file)
{
    const char *p = getenv("PATH");
    size_t filelen = strlen(file);

    if (!p || !*p)
        return NULL;

    while (1) {
        const char *end = strchrnul(p, ':');
        size_t dirlen = (size_t)(end - p);
        /* room for: <dir> '/' <file> ".exe" '\0' */
        char *path = malloc(dirlen + 1 + filelen + 4 + 1);
        char *q = path;

        if (!path)
            return NULL;

        /* POSIX: an empty PATH entry means the current directory. */
        if (dirlen) {
            memcpy(q, p, dirlen);
            q += dirlen;
            *q++ = '/';
        }
        memcpy(q, file, filelen);
        q += filelen;
        *q = 0;

        if (!access(path, F_OK))
            return path;

        memcpy(q, ".exe", 5);
        if (!access(path, F_OK))
            return path;

        free(path);

        if (!*end)
            break;
        p = end + 1;
    }

    return NULL;
}
