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

static PC_INLINE const char *pc__resolve_dictionary(const json_t* code2route, uint16_t code)
{
    char code_str[16];
    memset(code_str, 0, 16);
    sprintf(code_str, "%u", code);
    return json_string_value(json_object_get(code2route, code_str));
}

#define PC__MSG_CHECK_LEN(INDEX, LENGTH)                                      \
do {                                                                          \
    if((INDEX) > (LENGTH)) {                                                  \
        pc_lib_log(PC_LOG_ERROR, "pc_msg_decode_to_raw - invalid length");    \
        goto error;                                                           \
    }                                                                         \
} while(0)


static pc__msg_raw_t *pc_msg_decode_to_raw(const pc_buf_t* buf) 
{
    const char* data = buf->base;
    size_t len = buf->len;
    pc__msg_raw_t *msg = NULL;
    char *route_str = NULL;
    size_t body_len;
    size_t offset = 0;

    uint8_t flag; 
    uint8_t type;
    uint8_t compressRoute;

    // invalid req id for error msg
    // notify req id for push message
    uint32_t id = PC_INVALID_REQ_ID;

    if (len < PC_MSG_FLAG_BYTES) {
        return NULL;
    }

    flag = data[offset++];
    type = flag >> 1;
    compressRoute = flag & 0x01;

    if (!PC_MSG_VALIDATE(type)) {
        pc_lib_log(PC_LOG_ERROR, "pc_msg_decode_to_raw - unknow message type");
        return NULL; 
    }

    msg = (pc__msg_raw_t *)pc_lib_malloc(sizeof(pc__msg_raw_t));
    memset(msg, 0, sizeof(pc__msg_raw_t));

    msg->type = (pc_msg_type)type;
    msg->compressRoute = compressRoute;

    if(PC_MSG_HAS_ID(type)) {
        int i = 0;
        uint8_t m;
        id = 0;
     
        do{
            PC__MSG_CHECK_LEN(offset + 1, len);
            m = data[offset++];
            id = id + ((m & 0x7f) << (7 * i));
            i++;
        }while(m & 0x80);
    } else {
        id = PC_NOTIFY_PUSH_REQ_ID;
    }
    assert(id != PC_INVALID_REQ_ID);
    msg->id = id;

    // route
    if (PC_MSG_HAS_ROUTE(type)) {
        if(compressRoute) {
            PC__MSG_CHECK_LEN(offset + PC_MSG_ROUTE_CODE_BYTES, len);
            msg->route.route_code |= data[offset++] << 8;
            msg->route.route_code |= data[offset++];
        } else {
            size_t route_len;
            PC__MSG_CHECK_LEN(offset + PC_MSG_ROUTE_LEN_BYTES, len);
            route_len = data[offset++];
            PC__MSG_CHECK_LEN(offset + route_len, len);

            route_str = (char *)pc_lib_malloc(route_len + 1);
            memset(route_str, 0, route_len + 1);
            memcpy(route_str, data + offset, route_len);
            msg->route.route_str = route_str;

            offset += route_len;
        }
    }

    // borrow memory from original pc_buf_t 
    body_len = len - offset;
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
    const char *origin_route = NULL;

    json_t* json_msg = NULL;
    const char* data = NULL;
    pc_buf_t body;
    pc_msg_t msg;
    pc__msg_raw_t* raw_msg = NULL;

    memset(&msg, 0, sizeof(pc_msg_t));
    msg.id = PC_INVALID_REQ_ID;

    assert(buf && buf->base);

    raw_msg = pc_msg_decode_to_raw(buf);

    if (!raw_msg) {
        return msg;
    }

    assert(raw_msg->id != PC_INVALID_REQ_ID);

    msg.id = raw_msg->id;

    // route
    if (PC_MSG_HAS_ROUTE(raw_msg->type)) {
        // uncompress route dictionary
        route_str = NULL;
        if (raw_msg->compressRoute) {
            origin_route = pc__resolve_dictionary(code2route, raw_msg->route.route_code);
            if(!origin_route) {
                pc_lib_log(PC_LOG_ERROR, "pc_default_msg_decode - fail to uncompress route dictionary: %d",
                        raw_msg->route.route_code);
            } else {
                route_str = (char* )pc_lib_malloc(strlen(origin_route) + 1);
                strcpy((char* )route_str, origin_route);
            }
        } else {
            // till now, raw_msg->route.route_str is hold by pc_msg_t
            route_str = raw_msg->route.route_str;
            raw_msg->route.route_str = NULL;
        }

        msg.route = route_str;
    } else {
        msg.route = NULL;
    }

    if (PC_MSG_HAS_ROUTE(raw_msg->type) &&  !msg.route) {
        msg.id = PC_INVALID_REQ_ID;
        pc_lib_free(raw_msg);
        return msg;
    }

    // body.base is within msg
    body = raw_msg->body;
    if (body.len > 0) {
        // response message has no route 
        json_msg = NULL;
        if (!msg.route) {
            json_msg = pc_body_json_decode(body.base, 0, body.len);
        } else {
            json_t *pb_def = json_object_get(server_protos, msg.route);
            if (pb_def) {
                // protobuf decode
                json_msg = pc_body_pb_decode(body.base, 0, body.len, server_protos, pb_def);
            } else {
                json_msg = pc_body_json_decode(body.base, 0, body.len);
            }
        }

        if (!json_msg) {
            pc_lib_free((char*)msg.route);
            msg.id = PC_INVALID_REQ_ID;
            msg.route = NULL;
        } else {
            data = json_dumps(json_msg, JSON_COMPACT); 

            assert(data);

            msg.msg = data;
            json_decref(json_msg);
            json_msg = NULL;
        }
    }

    pc_lib_free(raw_msg);

    return msg;
}


static pc_buf_t pc_msg_encode_route(uint32_t id, pc_msg_type type,
        const char *route, const pc_buf_t msg);
static pc_buf_t pc_msg_encode_code(uint32_t id, pc_msg_type type,
        int route_code, const pc_buf_t msg);

static uint8_t pc__msg_id_length(uint32_t id);
static PC_INLINE size_t pc__msg_encode_flag(pc_msg_type type, int compressRoute,
        char *base, size_t offset);
static PC_INLINE size_t pc__msg_encode_id(uint32_t id, char *base, size_t offset);
static PC_INLINE size_t pc__msg_encode_route(const char *route, uint16_t route_len,
        char *base, size_t offset);

pc_buf_t pc_msg_encode_route(uint32_t id, pc_msg_type type,
        const char *route, const pc_buf_t msg) 
{
    pc_buf_t buf;
    uint8_t id_len = PC_MSG_HAS_ID(type) ? pc__msg_id_length(id) : 0;
    uint16_t route_len = PC_MSG_HAS_ROUTE(type) ? strlen(route) : 0;

    size_t msg_len = PC_MSG_FLAG_BYTES + id_len + 
        PC_MSG_ROUTE_LEN_BYTES + route_len + msg.len;
    char *base = NULL;
    size_t offset = 0;

    buf.base = NULL;
    buf.len = -1;

    base = buf.base = (char *)pc_lib_malloc(msg_len);
    buf.len = msg_len;

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
    return buf;
}

pc_buf_t pc_msg_encode_code(uint32_t id, pc_msg_type type,
        int routeCode, const pc_buf_t body)
{
    pc_buf_t buf;
 
    uint8_t id_len = PC_MSG_HAS_ID(type) ? pc__msg_id_length(id) : 0;
    uint16_t route_len = PC_MSG_HAS_ROUTE(type) ? PC_MSG_ROUTE_CODE_BYTES : 0;
    size_t msg_len = PC_MSG_FLAG_BYTES + id_len + route_len + body.len;
    char* base = NULL;
    size_t offset = 0;

    buf.base = NULL;
    buf.len = -1;

    base = buf.base = (char *)pc_lib_malloc(msg_len);
    buf.len = msg_len;

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
    return buf;
}

static PC_INLINE size_t pc__msg_encode_flag(pc_msg_type type, int compressRoute,
        char *base, size_t offset)
{
    base[offset++] = (type << 1) | (compressRoute ? 1 : 0);
    return offset;
}

static PC_INLINE size_t pc__msg_encode_id(uint32_t id, char *base, size_t offset)
{
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

static PC_INLINE size_t pc__msg_encode_route(const char *route, uint16_t route_len,
        char *base, size_t offset)
{
    base[offset++] = route_len & 0xff;

    memcpy(base + offset, route, route_len);

    return offset + route_len;
}

static uint8_t pc__msg_id_length(uint32_t id)
{
    uint8_t len = 0;
    do {
        len += 1;
        id >>= 7;
    } while(id > 0);
    return len;
}

pc_buf_t pc_default_msg_encode(const json_t* route2code, const json_t* client_protos, const pc_msg_t* msg)
{
    pc_buf_t msg_buf;
    pc_buf_t body_buf;
    json_t* json_msg;
    json_error_t err;
    int route_code = -1;
    json_t* code = NULL;
    json_t* pb_def = NULL;
    pc_msg_type type;

    msg_buf.base = NULL;
    msg_buf.len = -1;
    body_buf.base = NULL;
    body_buf.len = -1;

    assert(msg && msg->msg && msg->route);

    json_msg = json_loads(msg->msg, 0, &err);
    if (!json_msg) {
        pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - the msg is not invalid json, error: %s", err.text);
        return msg_buf;
    }

    assert(json_msg);

    // encode body
    pb_def = json_object_get(client_protos, msg->route);
    if (pb_def) {
        body_buf = pc_body_pb_encode(json_msg, client_protos, pb_def);
        if(body_buf.len == -1) {
            assert(body_buf.base == NULL);
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - fail to encode message with protobuf: %s\n", msg->route);
        }
    } else {
        body_buf = pc_body_json_encode(json_msg);
        if(body_buf.len == -1) {
            assert(body_buf.base == NULL);
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - fail to encode message with json: %s\n", msg->route);
        }
    }

    json_decref(json_msg);
    json_msg = NULL;

    if (body_buf.len == -1) {
        return msg_buf;
    }

    assert(body_buf.base && body_buf.len != -1);

    type = msg->id == PC_NOTIFY_PUSH_REQ_ID ? PC_MSG_NOTIFY : PC_MSG_REQUEST;
    
    code = json_object_get(route2code, msg->route);
    if (code) {
        route_code = (int)json_integer_value(code);
    }

    if (route_code > 0) {
        msg_buf = pc_msg_encode_code(msg->id, type, route_code, body_buf);
        if(msg_buf.len == -1) {
            assert(msg_buf.base == NULL);
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - failed to encode message with route code: %d\n",
                    route_code);
        }
    } else {
        msg_buf = pc_msg_encode_route(msg->id, type, msg->route, body_buf);
        if(msg_buf.len == -1) {
            assert(msg_buf.base == NULL);
            pc_lib_log(PC_LOG_ERROR, "pc_default_msg_encode - failed to encode message with route string: %s\n",
                    msg->route);
        }
    }

    pc_lib_free(body_buf.base);
    body_buf.base = NULL;
    body_buf.len = -1;

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
