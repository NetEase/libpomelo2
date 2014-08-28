/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <string.h>
#include <jansson.h>
#include <pc_lib.h>

#include "pr_msg.h"
#include "pb.h"

pc_buf_t pc_body_pb_encode(const json_t *msg, const json_t *gprotos, const json_t *pb_def)
{
    pc_buf_t buf, json_buf;
    size_t eval_size;
    size_t written;

    memset(&buf, 0, sizeof(pc_buf_t));
    memset(&json_buf, 0, sizeof(pc_buf_t));

    json_buf = pc_body_json_encode(msg);

    if (json_buf.len == -1) { 
        // error log fprintf(stderr, "Fail to encode json for protobuf evaluate.\n");
        buf.len = -1;
        return buf;
    }

    // use double space of json_buf, it should be enough
    eval_size = json_buf.len * 2;

    pc_lib_free(json_buf.base);
    json_buf.base = NULL;
    json_buf.len = -1;

    buf.base = (char *)pc_lib_malloc(eval_size);

    written = 0;
    if (!pc_pb_encode((uint8_t *)buf.base, eval_size, &written,
                      (json_t *)gprotos, (json_t *)pb_def, (json_t *)msg)) {
        pc_lib_free(buf.base);
        buf.base = NULL;
        buf.len = -1;

        // fprintf(stderr, "Fail to do protobuf encode.\n");
        return buf;
    }

    buf.len = written;

    return buf;
}

json_t *pc_body_pb_decode(const char *data, size_t offset, size_t len,
                      const json_t *gprotos, const json_t *pb_def) 
{
    json_t *result = json_object();
    if (!result) {
        // fprintf(stderr, "Fail to create json_t for protobuf decode.\n");
        return NULL;
    }

    if (!pc_pb_decode((uint8_t *)(data + offset), len,
                      (json_t *)gprotos, (json_t *)pb_def, result)) {
        json_decref(result);
        // fprintf(stderr, "Fail to do protobuf decode.\n");
        return NULL;
    }

    return result;
}
