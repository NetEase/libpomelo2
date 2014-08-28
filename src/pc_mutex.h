/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifndef PC_MUTEX_H
#define PC_MUTEX_H

#ifdef _WIN32

#include <windows.h>

typedef CRITICAL_SECTION pc_mutex_t;

static __inline void pc_mutex_init(pc_mutex_t* mutex)
{
    InitializeCriticalSection(mutex);
}

static __inline void pc_mutex_lock(pc_mutex_t* mutex)
{
    EnterCriticalSection(mutex);
}

static __inline void pc_mutex_unlock(pc_mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
}

static __inline void pc_mutex_destroy(pc_mutex_t* mutex)
{
    DeleteCriticalSection(mutex);
}

#else

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

typedef pthread_mutex_t pc_mutex_t;

static inline void pc_mutex_init(pc_mutex_t* mutex)
{
    if (pthread_mutex_init(mutex, NULL)) {
        abort();
    }
}

static inline void pc_mutex_lock(pc_mutex_t* mutex)
{
    if (pthread_mutex_lock(mutex)) {
        abort();
    }
}

static inline void pc_mutex_unlock(pc_mutex_t* mutex)
{
    if (pthread_mutex_unlock(mutex)) {
        abort();
    }
}

static inline void pc_mutex_destroy(pc_mutex_t* mutex)
{
    if (pthread_mutex_destroy(mutex)) {
        abort();
    }
}

#endif

#endif // PC_MUTEX_H

