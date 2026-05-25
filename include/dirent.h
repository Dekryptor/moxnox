#pragma once

#include <sys/types.h>

typedef struct _DIR DIR;

struct dirent {
    ino_t d_ino;
    char d_name[260];
};

#ifdef __cplusplus
extern "C" {
#endif

int closedir(DIR*);
DIR* opendir(const char*);
struct dirent* readdir(DIR*);
void rewinddir(DIR*);

#ifdef __cplusplus
}
#endif
