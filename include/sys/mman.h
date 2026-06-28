#pragma once

#define PROT_NONE   0
#define PROT_READ   1
#define PROT_WRITE  2
#define PROT_EXEC   4

#define MAP_FIXED       1
#define MAP_ANONYMOUS   2
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_PRIVATE     4
#define MAP_SHARED      8

#define MAP_FAILED      ((void*)-1)

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void* addr, size_t len);
