/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifndef TR_UV_TLS_AUX_H
#define TR_UV_TLS_AUX_H

#include "tr_uv_tcp_i.h"
#include "tr_uv_tls_i.h"

void tls__reset(tr_uv_tcp_transport_t* trans);

void tls__conn_done_cb(uv_connect_t* conn, int status);

void tls__write_async_cb(uv_async_t* a);
void tls__write_done_cb(uv_write_t* w, int status);

void tls__write_timeout_check_cb(uv_timer_t* timer);

void tls__cleanup_async_cb(uv_async_t* a);

void tls__on_tcp_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

#endif
