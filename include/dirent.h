#pragma once

#include <sys/types.h>

typedef struct _DIR DIR;

#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK     10
#define DT_SOCK    12

struct dirent {
    ino_t d_ino;
    unsigned char d_type;
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
