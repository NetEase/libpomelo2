/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <assert.h>
#include <string.h>
#include <jansson.h>

#include <pomelo.h>
#include <pc_lib.h>

#include "pr_msg.h"

pc_buf_t pc_body_json_encode(const json_t *msg)
{
    pc_buf_t buf;
    char* res;

    buf.base = NULL;
    buf.len = -1;

    assert(msg);

    res = json_dumps(msg, JSON_COMPACT);
    if (!res) {
        pc_lib_log(PC_LOG_ERROR, "pc_body_json_encode - json encode error");
    } else {
        buf.base = res;
        buf.len = strlen(res);
    }
    return buf;
}

json_t *pc_body_json_decode(const char *data, size_t offset, size_t len)
{
    json_error_t error;
    json_t *res = json_loadb(data + offset, len - offset, 0, &error);

    if (!res) {
        pc_lib_log(PC_LOG_ERROR, "pc_body_json_decode - json decode error: %s", error.text);
    }

    return res;
}
