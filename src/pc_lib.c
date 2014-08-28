/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "pomelo.h"
#include "pomelo_trans.h"
#include "pc_lib.h"

#if !defined(PC_NO_DUMMY_TRANS)
#  include "tr/dummy/tr_dummy.h"
#endif

#if !defined(PC_NO_UV_TCP_TRANS)
#  include "tr/uv/tr_uv_tcp.h"

#  if !defined(PC_NO_UV_TLS_TRANS)
#    include "tr/uv/tr_uv_tls.h"
#  endif // tls

#endif // tcp

void (*pc_lib_log)(int level, const char* msg, ...) = NULL;
void* (*pc_lib_malloc)(size_t len) = NULL;
void (*pc_lib_free)(void* data) = NULL;

const char* pc_lib_platform_type = NULL;

/**
 * default malloc never return NULL
 * so we don't have to check its return value 
 *
 * if you customize malloc, please make sure that it never return NULL
 */
static void* default_malloc(size_t len)
{
    void* d = malloc(len);

    // if oom, just abort
    if (!d)
        abort();

    return d;
}

static void default_log(int level, const char* msg, ...) 
{
    time_t t = time(NULL);
    char buf[32];
    strftime(buf, 32, "[ %x %T ]", localtime(&t));
    printf(buf);
    switch(level) {
    case PC_LOG_DEBUG:
        printf("[DEBUG] ");
        break;
    case PC_LOG_INFO:
        printf("[INFO] ");
        break;
    case PC_LOG_WARN:
        printf("[WARN] ");
        break;
    case PC_LOG_ERROR:
        printf("[ERROR] ");
        break;
    }

    va_list va;
    va_start(va, msg);
    vprintf(msg, va);
    va_end(va);

    printf("\n");

    fflush(stdout);
}

void pc_lib_init(void (*pc_log)(int level, const char* msg, ...), void* (*pc_alloc)(size_t), void (*pc_free)(void* ), const char* platform)
{
    pc_transport_plugin_t* tp;

    pc_lib_log = pc_log ? pc_log : default_log;
    pc_lib_malloc = pc_alloc ? pc_alloc : default_malloc;
    pc_lib_free = pc_free ? pc_free: free;
    pc_lib_platform_type = platform ? pc_lib_strdup(platform) : pc_lib_strdup("desktop");

#if !defined(PC_NO_DUMMY_TRANS)
    tp = pc_tr_dummy_trans_plugin();
    pc_transport_plugin_register(tp);
#endif

#if !defined(PC_NO_UV_TCP_TRANS)
    tp = pc_tr_uv_tcp_trans_plugin();
    pc_transport_plugin_register(tp);

#if !defined(PC_NO_UV_TLS_TRANS)
    tp = pc_tr_uv_tls_trans_plugin();
    pc_transport_plugin_register(tp);
#endif

#endif
}

void pc_lib_cleanup() 
{
#if !defined(PC_NO_DUMMY_TRANS)
    pc_transport_plugin_deregister(PC_TR_NAME_DUMMY);
#endif

#if !defined(PC_NO_UV_TCP_TRANS)
    pc_transport_plugin_deregister(PC_TR_NAME_UV_TCP);

#if !defined(PC_NO_UV_TLS_TRANS)
    pc_transport_plugin_deregister(PC_TR_NAME_UV_TLS);
#endif

#endif
    pc_lib_free((char*)pc_lib_platform_type);
}

const char* pc_lib_strdup(const char* str)
{
    char* buf;
    size_t len = strlen(str);

    buf = (char* )pc_lib_malloc(len + 1);
    strcpy(buf, str);
    buf[len] = '\0';

    return buf;
}

