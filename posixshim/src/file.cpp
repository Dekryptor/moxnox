#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

import nt;
import unicode;

#undef errno
import errno;

extern "C"
int mkdir(const char* path, mode_t /*mode*/) {
    return _mkdir(path);
}

extern "C"
ssize_t getline(char** lineptr, size_t* pn, FILE* stream) {
    if (!lineptr || !pn) {
        *_errno() = EINVAL;
        return -1;
    }

    auto& line = *lineptr;
    auto& n = *pn;
    size_t read = 0;

    if (!line || n == 0) {
        auto newptr = (char*)realloc(line, 128);
        if (!newptr) {
            *_errno() = ENOMEM;
            return -1;
        }

        line = newptr;
        n = 128;
    }

    auto add_char = [&read, &n, &line](char c) {
        if (read == n) {
            auto newptr = (char*)realloc(line, n * 2);
            if (!newptr) {
                *_errno() = ENOMEM;
                return false;
            }

            line = newptr;
            n *= 2;
        }

        line[read] = c;

        return true;
    };

    while (true) {
        auto c = fgetc(stream);

        if (c == EOF)
            break;

        if (!add_char((char)c))
            return -1;

        read++;

        if ((char)c == '\n')
            break;
    }

    if (read == 0)
        return -1;

    if (!add_char(0))
        return -1;

    return (ssize_t)read;
}
