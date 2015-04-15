/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifndef TR_UV_TLS_I_H
#define TR_UV_TLS_I_H

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "tr_uv_tcp_i.h"

#define PC_TLS_READ_BUF_SIZE 1024

typedef struct {
    tr_uv_tcp_transport_t base;

    SSL* tls;
    BIO* in;
    BIO* out;

    int is_handshake_completed;

    char rb[PC_TLS_READ_BUF_SIZE];

    char* retry_wb;
    int retry_wb_len;

    tr_uv_wi_t* should_retry;
    QUEUE when_tcp_is_writing_queue;

    void* internal[2];
} tr_uv_tls_transport_t;

typedef struct {
    tr_uv_tcp_transport_plugin_t base;

    SSL_CTX* ctx;
    int enable_verify;
} tr_uv_tls_transport_plugin_t;

pc_transport_t* tr_uv_tls_create(pc_transport_plugin_t* plugin);
void tr_uv_tls_release(pc_transport_plugin_t* plugin, pc_transport_t* trans);
void tr_uv_tls_plugin_on_register(pc_transport_plugin_t* plugin);
void tr_uv_tls_plugin_on_deregister(pc_transport_plugin_t* plugin);

int tr_uv_tls_init(pc_transport_t* trans, pc_client_t* client);

void* tr_uv_tls_internal_data(pc_transport_t* trans);
pc_transport_plugin_t* tr_uv_tls_plugin(pc_transport_t* trans);

#endif /* TR_UV_TLS_I_H */

