#pragma once

#include <stdint.h>

#include_next <sys/types.h>

typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int id_t;
typedef long long ssize_t;
typedef uintptr_t pthread_t;
typedef uint64_t pthread_mutex_t;
typedef unsigned int pthread_key_t;
typedef uintptr_t pthread_cond_t;
typedef uintptr_t pthread_condattr_t; // FIXME?
typedef uintptr_t pthread_mutexattr_t; // FIXME?
typedef uintptr_t pthread_attr_t; // FIXME?
