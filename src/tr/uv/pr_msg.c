/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <jansson.h>

#include <pc_lib.h>

#include "pr_msg.h"
#include "tr_uv_tcp_i.h"

#define PC_MSG_FLAG_BYTES 1
#define PC_MSG_ROUTE_LEN_BYTES 1
#define PC_MSG_ROUTE_CODE_BYTES 2

#define PC_MSG_HAS_ID(TYPE) ((TYPE) == PC_MSG_REQUEST ||                      \
        (TYPE) == PC_MSG_RESPONSE)

#define PC_MSG_HAS_ROUTE(TYPE) ((TYPE) != PC_MSG_RESPONSE)

#define PC_MSG_VALIDATE(TYPE) ((TYPE) == PC_MSG_REQUEST ||                    \
        (TYPE) == PC_MSG_NOTIFY ||                                            \
        (TYPE) == PC_MSG_RESPONSE ||                                          \
        (TYPE) == PC_MSG_PUSH)

#define PC__MSG_CHECK_LEN(INDEX, LENGTH)                                      \
do {                                                                          \
    if((INDEX) > (LENGTH)) {                                                  \
        pc_lib_log(PC_LOG_ERROR, "pr_msg_decode - invalid length");           \
        goto error;                                                           \
    }                                                                         \
}while(0);

#define PC_PB_EVAL_FACTOR 2

/**
 * message type.
 */
typedef enum {
    PC_MSG_REQUEST = 0,
    PC_MSG_NOTIFY,
    PC_MSG_RESPONSE,
    PC_MSG_PUSH
} pc_msg_type;

typedef struct {
    uint32_t id;
    pc_msg_type type;
    uint8_t compressRoute;
    union {
        uint16_t route_code;
        const char *route_str;
    } route;
    pc_buf_t body;
} pc__msg_raw_t;

static inline const char *pc__resolve_dictionary(const json_t* code2route, uint16_t code)
{
    char code_str[16];
    memset(code_str, 0, 16);
    sprintf(code_str, "%u", code);
    return json_string_value(json_object_get(code2route, code_str));
}

static pc__msg_raw_t *pc_msg_decode_to_raw(const pc_buf_t* buf) 
{
    const char* data = buf->base;
    size_t len = buf->len;
    pc__msg_raw_t *msg = NULL;
    char *route_str = NULL;

    msg = (pc__msg_raw_t *)pc_lib_malloc(sizeof(pc__msg_raw_t));
    memset(msg, 0, sizeof(pc__msg_raw_t));

    size_t offset = 0;

    PC__MSG_CHECK_LEN(offset + PC_MSG_FLAG_BYTES, len);

    // flag
    uint8_t flag = data[offset++];

    // type
    uint8_t type = flag >> 1;

    if(!PC_MSG_VALIDATE(type)) {
        pc_lib_log(PC_LOG_ERROR, "pc_msg_decode - unknow message type");
        goto error;
    }

    msg->type = (pc_msg_type)type;

    // compress flag
    uint8_t compressRoute = flag & 0x01;
    msg->compressRoute = compressRoute;

    // invalid req id for error msg
    // notify req id for push message
    uint32_t id = PC_INVALID_REQ_ID;

    if(PC_MSG_HAS_ID(type)) {
        int i = 0;
        id = 0;
        uint8_t m;
        do{
            PC__MSG_CHECK_LEN(offset + 1, len);
            m = data[offset++];
            id = id + ((m & 0x7f) << (7 * i));
            i++;
        }while(m & 0x80);
    } else {
        id = PC_NOTIFY_PUSH_REQ_ID;
    }
    msg->id = id;

    // route
    if (PC_MSG_HAS_ROUTE(type)) {
        if(compressRoute) {
            PC__MSG_CHECK_LEN(offset + PC_MSG_ROUTE_CODE_BYTES, len);
            msg->route.route_code |= data[offset++] << 8;
            msg->route.route_code |= data[offset++];
        } else {
            PC__MSG_CHECK_LEN(offset + PC_MSG_ROUTE_LEN_BYTES, len);
            size_t route_len = data[offset++];
            PC__MSG_CHECK_LEN(offset + route_len, len);

            route_str = (char *)pc_lib_malloc(route_len + 1);
            memset(route_str, 0, route_len + 1);
            memcpy(route_str, data + offset, route_len);
            msg->route.route_str = route_str;

            offset += route_len;
        }
    }

    // borrow memory from original pc_buf_t 
    size_t body_len = len - offset;
    msg->body.base = (char* )data + offset;
    msg->body.len = body_len;

    return msg;

error:
    pc_lib_free(msg);
    pc_lib_free((char* )route_str);
    return NULL;
}

pc_msg_t pc_default_msg_decode(const json_t* code2route, const json_t* server_protos, const pc_buf_t* buf) 
{
    const char *route_str = NULL;
    json_t* json_msg = NULL;
    const char* data = NULL;

    pc_msg_t msg;
    memset(&msg, 0, sizeof(pc_msg_t));

    pc__msg_raw_t *raw_msg = pc_msg_decode_to_raw(buf);

    if (!raw_msg) {
        goto error;
    }

    msg.id = raw_msg->id;

    // route
    if(PC_MSG_HAS_ROUTE(raw_msg->type)) {
        const char *origin_route = NULL;
        // uncompress route dictionary
        if(raw_msg->compressRoute) {
            origin_route = pc__resolve_dictionary(code2route, raw_msg->route.route_code);
            if(!origin_route) {
                pc_lib_log(PC_LOG_ERROR, "pc_default_msg_decode - fail to uncompress route dictionary: %d",
                        raw_msg->route.route_code);
                goto error;
            } else {
                route_str = (char* )pc_lib_malloc(strlen(origin_route) + 1);
                strcpy((char* )route_str, origin_route);
                origin_route = route_str;
            }
        } else {
            // till now, raw_msg->route.route_str is hold by pc_msg_t
            origin_route = raw_msg->route.route_str;
            raw_msg->route.route_str = NULL;
        }

        assert(origin_route);

        msg.route = origin_route;
    } else {
        msg.route = NULL;
    }

    // body.base is within msg
    pc_buf_t body = raw_msg->body;
    if(body.len > 0) {
        // response message has no route 
        if (!msg.route) {
            json_msg = pc_body_json_decode(body.base, 0, body.len);
        } else {
            json_t *pb_def = json_object_get(server_protos, msg.route);
            if(pb_def) {
                // protobuf decode
                json_msg = pc_body_pb_decode(body.base, 0, body.len, server_protos, pb_def);
            } else {
                json_msg = pc_body_json_decode(body.base, 0, body.len);
            }
        }

        if(json_msg == NULL)
            goto error;
    }

    data = json_dumps(json_msg, JSON_COMPACT);
    msg.msg = data;

    json_decref(json_msg);
    json_msg = NULL;

    pc_lib_free(raw_msg);
    return msg;

error:
    pc_lib_free((char* )msg.route);
    pc_lib_free(raw_msg);
    msg.id = PC_INVALID_REQ_ID;
    msg.route = NULL;
    return msg;
}

static pc_buf_t pc_msg_encode_route(uint32_t id, pc_msg_type type,
        const char *route, pc_buf_t msg);
static pc_buf_t pc_msg_encode_code(uint32_t id, pc_msg_type type,
        int route_code, pc_buf_t msg);

static uint8_t pc__msg_id_length(uint32_t id);
static inline size_t pc__msg_encode_flag(pc_msg_type type, int compressRoute,
        char *base, size_t offset);
static inline size_t pc__msg_encode_id(uint32_t id, char *base, size_t offset);
static inline size_t pc__msg_encode_route(const char *route, uint16_t route_len,
        char *base, size_t offset);

pc_buf_t pc_msg_encode_route(uint32_t id, pc_msg_type type,
        const char *route, pc_buf_t msg) 
{
    pc_buf_t buf;

    memset(&buf, 0, sizeof(pc_buf_t));

    uint8_t id_len = PC_MSG_HAS_ID(type) ? pc__msg_id_length(id) : 0;
    uint16_t route_len = PC_MSG_HAS_ROUTE(type) ? strlen(route) : 0;

    size_t msg_len = PC_MSG_FLAG_BYTES + id_len + PC_MSG_ROUTE_LEN_BYTES +
        route_len + msg.len;

    char *base = buf.base = (char *)malloc(msg_len);

    if(buf.base == NULL) {
        buf.len = -1;
        goto error;
    }

    size_t offset = 0;

    // flag
    offset = pc__msg_encode_flag(type, 0, base, offset);

    // message id
    if(PC_MSG_HAS_ID(type)) {
        offset = pc__msg_encode_id(id, base, offset);
    }

    // route
    if(PC_MSG_HAS_ROUTE(type)) {
        offset = pc__msg_encode_route(route, route_len, base, offset);
    }

    // body
    memcpy(base + offset, msg.base, msg.len);

    buf.len = msg_len;

    return buf;

error:
    if(buf.len != -1) free(buf.base);
    buf.len = -1;
    return buf;
}

pc_buf_t pc_msg_encode_code(uint32_t id, pc_msg_type type,
        int routeCode, pc_buf_t body) {
    pc_buf_t buf;

    memset(&buf, 0, sizeof(pc_buf_t));

    uint8_t id_len = PC_MSG_HAS_ID(type) ? pc__msg_id_length(id) : 0;
    uint16_t route_len = PC_MSG_HAS_ROUTE(type) ? PC_MSG_ROUTE_CODE_BYTES : 0;

    size_t msg_len = PC_MSG_FLAG_BYTES + id_len + route_len + body.len;

    char *base = buf.base = (char *)pc_lib_malloc(msg_len);
    buf.len = msg_len;

    size_t offset = 0;

    // flag
    offset = pc__msg_encode_flag(type, 1, base, offset);

    // message id
    if(PC_MSG_HAS_ID(type)) {
        offset = pc__msg_encode_id(id, base, offset);
    }

    // route code
    if(PC_MSG_HAS_ROUTE(type)) {
        base[offset++] = (routeCode >> 8) & 0xff;
        base[offset++] = routeCode & 0xff;
    }

    // body
    memcpy(base + offset, body.base, body.len);

    buf.len = msg_len;

    return buf;

error:
    if(buf.len != -1) free(buf.base);
    buf.len = -1;
    return buf;
}



static inline size_t pc__msg_encode_flag(pc_msg_type type, int compressRoute,
        char *base, size_t offset) {
    base[offset++] = (type << 1) | (compressRoute ? 1 : 0);
    return offset;
}

static inline size_t pc__msg_encode_id(uint32_t id, char *base, size_t offset) {
    do{
        uint32_t tmp = id & 0x7f;
        uint32_t next = id >> 7;

        if(next != 0){
            tmp = tmp + 128;
        }
        base[offset++] = tmp;
        id = next;
    } while(id != 0);

    return offset;
}

static inline size_t pc__msg_encode_route(const char *route, uint16_t route_len,
        char *base, size_t offset) {
    base[offset++] = route_len & 0xff;

    memcpy(base + offset, route, route_len);

    return offset + route_len;
}

static uint8_t pc__msg_id_length(uint32_t id) {
    uint8_t len = 0;
    do {
        len += 1;
        id >>= 7;
    } while(id > 0);
    return len;
}

pc_buf_t pc_default_msg_encode(const json_t* route2code, const json_t* client_protos, const pc_msg_t* msg)
{
    // assert
    // TODO: error handling
    pc_buf_t msg_buf, body_buf;
    json_t* json_msg;
    json_error_t err;

    msg_buf.len = -1;
    body_buf.len = -1;

    json_msg = json_loads(msg->msg, 0, &err);
    // route encode
    int route_code = -1;
    json_t *code = json_object_get(route2code, msg->route);
    if(code) {
        // dictionary compress
        route_code = json_integer_value(code);
    }

    // encode body
    json_t *pb_def = json_object_get(client_protos, msg->route);
    if(pb_def) {
        body_buf = pc_body_pb_encode(json_msg, client_protos, pb_def);
        if(body_buf.len == -1) {
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - fail to encode message with protobuf: %s\n", msg->route);
            goto error;
        }
    } else {
        // json encode
        body_buf = pc_body_json_encode(json_msg);
        if(body_buf.len == -1) {
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - fail to encode message with json: %s\n", msg->route);
            goto error;
        }
    }

    // message type
    pc_msg_type type = PC_MSG_REQUEST;
    if(msg->id  == 0) {
        type = PC_MSG_NOTIFY;
    }

    if(route_code > 0) {
        msg_buf = pc_msg_encode_code(msg->id, type, route_code, body_buf);
        if(msg_buf.len == -1) {
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - failed to encode message with route code: %d\n",
                    route_code);
            goto error;
        }
    } else {
        msg_buf = pc_msg_encode_route(msg->id, type, msg->route, body_buf);
        if(msg_buf.len == -1) {
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - failed to encode message with route string: %s\n",
                    msg->route);
            goto error;
        }
    }

    if(body_buf.len > 0) {
        pc_lib_free(body_buf.base);
    }

    if (json_msg) {
        json_decref(json_msg);
        json_msg = NULL;
    }

    return msg_buf;

error:
    if(msg_buf.len > 0) pc_lib_free(msg_buf.base);
    if(body_buf.len > 0) pc_lib_free(body_buf.base);
    msg_buf.len = -1;

    if (json_msg) {
        json_decref(json_msg);
        json_msg = NULL;
    }

    return msg_buf;
}

// for transport plugin
uv_buf_t pr_default_msg_encoder(tr_uv_tcp_transport_t* tt, const pc_msg_t* msg)  
{
    pc_buf_t pb;
    uv_buf_t ub;

    pb = pc_default_msg_encode(tt->route_to_code, tt->client_protos, msg);
    ub.base = pb.base;
    ub.len = pb.len;
    return ub;
}

pc_msg_t pr_default_msg_decoder(tr_uv_tcp_transport_t* tt, const uv_buf_t* buf) 
{
    pc_buf_t pb;
    pb.base = buf->base;
    pb.len = buf->len;

    return pc_default_msg_decode(tt->code_to_route, tt->server_protos, &pb);
}
