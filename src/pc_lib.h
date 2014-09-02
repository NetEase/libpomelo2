/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifndef PC_LIB_H
#define PC_LIB_H

#include <stddef.h>

extern void (*pc_lib_log)(int level, const char* msg, ...);
extern void* (*pc_lib_malloc)(size_t len);
extern void (*pc_lib_free)(void* data);
extern const char* pc_lib_platform_type;

const char* pc_lib_strdup(const char* str);

const char* pc_client_state_str(int state);
const char* pc_client_ev_str(int ev_type);
const char* pc_client_rc_str(int rc);
#endif
