/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifndef PR_MSG_H
#define PR_MSG_H

#include <jansson.h>
#include <stdint.h>

#include "pr_pkg.h"

typedef struct tr_uv_tcp_transport_s tr_uv_tcp_transport_t;

typedef struct {
    unsigned int id;
    const char* route;
    const char* msg;
} pc_msg_t;

uv_buf_t pr_default_msg_encoder(tr_uv_tcp_transport_t* tt, const pc_msg_t* msg);
pc_msg_t pr_default_msg_decoder(tr_uv_tcp_transport_t* tt, const uv_buf_t* buf);

/**
 * internal use
 */
typedef struct {
    char* base;
    int len;
} pc_buf_t;

pc_buf_t pc_default_msg_encode(const json_t* route2code, const json_t* client_protos, const pc_msg_t* msg);
pc_msg_t pc_default_msg_decode(const json_t* code2route, const json_t* server_protos, const pc_buf_t* buf);

pc_buf_t pc_body_json_encode(const json_t *msg);
json_t *pc_body_json_decode(const char *data, size_t offset, size_t len);

pc_buf_t pc_body_pb_encode(const json_t *msg, const json_t *gprotos, const json_t *pb_def);
json_t *pc_body_pb_decode(const char *data, size_t offset, size_t len,
                      const json_t *gprotos, const json_t *pb_def);

#endif

