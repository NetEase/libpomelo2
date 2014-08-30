/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <string.h>
#include <assert.h>

#include "tr_uv_tcp_aux.h"
#include "pc_pomelo_i.h"
#include "tr_uv_tcp_i.h"
#include "pc_lib.h"
#include "pr_pkg.h"

#define GET_TT(x) tr_uv_tcp_transport_t* tt = x->data; assert(tt) 

#define TR_UV_INTERNAL_PKG_TIMEOUT 30

static void tcp__reset_wi(pc_client_t* client, tr_uv_wi_t* wi)
{
    if (TR_UV_WI_IS_RESP(wi->type)) {
        pc_lib_log(PC_LOG_DEBUG, "tcp__reset_wi - reset request, req_id: %u", wi->req_id);
        pc_trans_resp(client, wi->req_id, PC_RC_RESET, NULL);
    } else if (TR_UV_WI_IS_NOTIFY(wi->type)) {
        pc_lib_log(PC_LOG_DEBUG, "tcp__reset_wi - reset notify, seq_num: %u", wi->seq_num);
        pc_trans_sent(client, wi->seq_num, PC_RC_RESET);
    }
    // drop internal write item

    pc_lib_free(wi->buf.base);
    wi->buf.base = NULL;
    wi->buf.len = 0;

    if (PC_IS_PRE_ALLOC(wi->type)) {
        PC_PRE_ALLOC_SET_IDLE(wi->type);
    } else {
        pc_lib_free(wi);
    }
}

void tcp__reset(tr_uv_tcp_transport_t* tt)
{
    int i;
    tr_uv_wi_t* wi;
    QUEUE* q;

    assert(tt);
    pc_pkg_parser_reset(&tt->pkg_parser);

    uv_timer_stop(&tt->hb_timeout_timer);
    uv_timer_stop(&tt->hb_timer);

    uv_timer_stop(&tt->check_timeout);

    uv_timer_stop(&tt->reconn_delay_timer);
    uv_timer_stop(&tt->conn_timeout);

    if (tt->is_writing) {
        uv_close((uv_handle_t* )&tt->write_req, NULL);
    }

    if (tt->is_connecting) {
        uv_close((uv_handle_t* )&tt->conn_req, NULL);
    }

    tt->is_waiting_hb = 0;
    tt->hb_rtt = -1;

    uv_read_stop((uv_stream_t* )&tt->socket);
    uv_mutex_lock(&tt->wq_mutex);

    while(!QUEUE_EMPTY(&tt->conn_pending_queue)) {
        q = QUEUE_HEAD(&tt->conn_pending_queue);
        QUEUE_REMOVE(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
        tcp__reset_wi(tt->client, wi);
    }

    while(!QUEUE_EMPTY(&tt->write_wait_queue)) {
        q = QUEUE_HEAD(&tt->write_wait_queue);
        QUEUE_REMOVE(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
        tcp__reset_wi(tt->client, wi);
    }

    while(!QUEUE_EMPTY(&tt->writing_queue)) {
        q = QUEUE_HEAD(&tt->writing_queue);
        QUEUE_REMOVE(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
        tcp__reset_wi(tt->client, wi);
    }

    while(!QUEUE_EMPTY(&tt->resp_pending_queue)) {
        q = QUEUE_HEAD(&tt->resp_pending_queue);
        QUEUE_REMOVE(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
        tcp__reset_wi(tt->client, wi);
    }

    uv_mutex_unlock(&tt->wq_mutex);

    tt->state = TR_UV_TCP_NOT_CONN;
}

void tcp__reconn_delay_timer_cb(uv_timer_t* t)
{
    GET_TT(t);

    assert(t == &tt->reconn_delay_timer);
    uv_timer_stop(t);
    uv_async_send(&tt->conn_async);
}

void tcp__reconn(tr_uv_tcp_transport_t* tt)
{
    int timeout;
    assert(tt && tt->reset_fn);

    tt->reset_fn(tt);

    tt->state == TR_UV_TCP_CONNECTING;

    const pc_client_config_t* config = tt->config;

    if (!config->enable_reconn) {
         pc_lib_log(PC_LOG_WARN, "tcp__reconn - trans want to reconn, but reconn is disabled");
         pc_trans_fire_event(tt->client, PC_EV_CONNECT_FAILED, "Reconn Disabled", NULL);
         tt->state = TR_UV_TCP_NOT_CONN;
         return;
    }

    tt->reconn_times ++;
    if (config->reconn_max_retry != PC_ALWAYS_RETRY && config->reconn_max_retry < tt->reconn_times) {
        pc_lib_log(PC_LOG_WARN, "tcp__reconn - reconn time exceeded");
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_FAILED, "Exceed Max Retry", NULL);
        tt->state = TR_UV_TCP_NOT_CONN;
        return ;
    }

    if (!config->reconn_exp_backoff) {
        timeout = config->reconn_delay * tt->reconn_times;
    } else {
        timeout = config->reconn_delay << (tt->reconn_times - 1);
    }

    if (timeout > config->reconn_delay_max) 
        timeout = config->reconn_delay_max;

    pc_lib_log(PC_LOG_DEBUG, "tcp__reconn - reconnect, delay: %d", timeout);
    
    uv_timer_start(&tt->reconn_delay_timer, tcp__reconn_delay_timer_cb, timeout * 1000, 0);
}

void tcp__conn_async_cb(uv_async_t* t)
{
    struct addrinfo hints;
    struct addrinfo* ainfo;
    struct addrinfo* rp;
    struct sockaddr_in* addr4 = NULL;
    struct sockaddr_in6* addr6 = NULL;
    struct sockaddr* addr = NULL;
    int ret;

    GET_TT(t);

    assert(t == &tt->conn_async);

    if (tt->is_connecting)
        return;

    tt->state = TR_UV_TCP_CONNECTING;

    assert(tt->host && tt->reconn_fn);

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(tt->host, NULL, &hints, &ainfo);

    if (ret) {
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_ERROR, "DNS Resolve Error", NULL);
        pc_lib_log(PC_LOG_ERROR, "tcp__conn_async - dns resolve error: %s, will reconn", tt->host);
        tt->reconn_fn(tt);
        return ;
    }

    for (rp = ainfo; rp; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            addr4 = (struct sockaddr_in* )rp->ai_addr;
            addr4->sin_port = htons(tt->port);
            break;

        } else if(rp->ai_family == AF_INET6){
            addr6 = (struct sockaddr_in6* )rp->ai_addr;
            addr6->sin6_port = htons(tt->port);
            break;

        } else {
            continue;
        }
    }

    freeaddrinfo(ainfo);

    if (!addr4 && !addr6) {
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_ERROR, "DNS Resolve Error", NULL);
        pc_lib_log(PC_LOG_ERROR, "tcp__conn_async - dns resolve error: %s, will reconn", tt->host);
        tt->reconn_fn(tt);
        return;
    }

    addr = addr4 ? (struct sockaddr* )addr4 : (struct sockaddr* )addr6;

    uv_tcp_init(&tt->uv_loop, &tt->socket);
    tt->socket.data = tt;

    tt->conn_req.data = tt;
    ret = uv_tcp_connect(&tt->conn_req, &tt->socket, addr, tt->conn_done_cb);

    if (ret) {
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_ERROR, "UV Conn Error", NULL);
        pc_lib_log(PC_LOG_ERROR, "tcp__conn_async_cb - uv tcp connect error: %s, will reconn", uv_strerror(ret));
        tt->reconn_fn(tt);
        return ;
    }

    tt->is_connecting = 1;

    if (tt->config->conn_timeout != PC_WITHOUT_TIMEOUT) {
        pc_lib_log(PC_LOG_DEBUG, "tcp__con_async_cb - start conn timeout timer");
        uv_timer_start(&tt->conn_timeout, tcp__conn_timeout_cb, tt->config->conn_timeout * 1000, 0);
    }
}

void tcp__conn_timeout_cb(uv_timer_t* t)
{
    GET_TT(t);

    assert(&tt->conn_timeout == t);
    assert(tt->is_connecting);
    uv_timer_stop(t);
    pc_lib_log(PC_LOG_INFO, "tcp__conn_timeout_cb - conn timeout, cancel it");
    uv_close((uv_handle_t* )&tt->conn_req, NULL);
}

void tcp__conn_done_cb(uv_connect_t* conn, int status)
{
    int hb_timeout;
    int ret;

    GET_TT(conn);

    assert(&tt->conn_req == conn);
    assert(tt->is_connecting);

    tt->is_connecting = 0;
    if (tt->config->conn_timeout != PC_WITHOUT_TIMEOUT) {

        // NOTE: we hack uv here to get the rest timeout value of conn_timeout,
        // and use it as the timeout value of handshake.
        //
        // it maybe lead to be non-compatiable to uv in future.
        hb_timeout = tt->conn_timeout.timeout - tt->uv_loop.time;
        uv_timer_stop(&tt->conn_timeout);
    }

    if (status == 0) {
        // tcp connected.
        tt->state = TR_UV_TCP_HANDSHAKEING;
        tt->reconn_times = 0;

        ret = uv_read_start((uv_stream_t* ) &tt->socket, tcp__alloc_cb, tt->on_tcp_read_cb); 

        if (ret) {
            pc_lib_log(PC_LOG_ERROR, "tcp__conn_done - start read from tcp error, reconn");
            tt->reconn_fn(tt);
            return ;
        }

        pc_lib_log(PC_LOG_INFO, "tcp__conn_done - tcp connected, send handshake");

        tcp__send_handshake(tt);

        if (tt->config->conn_timeout != PC_WITHOUT_TIMEOUT) {
            uv_timer_start( &tt->handshake_timer, tcp__handshake_timer_cb, hb_timeout, 0);
        }
        return ;
    } 

    if (status == UV_ECANCELED) {
        pc_lib_log(PC_LOG_DEBUG, "tcp__conn_done_cb - connect timeout");
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_ERROR, "Connect Timeout", NULL);
    } else {
        pc_lib_log(PC_LOG_DEBUG, "tcp__conn_done_cb - connect error, error: %s", uv_strerror(status));
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_ERROR, "Connect Error", NULL);
    }

    tt->reconn_fn(tt); 
}

void tcp__write_async_cb(uv_async_t* a)
{
    int buf_cnt;
    int i;
    int ret;
    QUEUE* q;
    tr_uv_wi_t* wi;
    GET_TT(a);

    assert(tt->state == TR_UV_TCP_CONNECTING || tt->state == TR_UV_TCP_HANDSHAKEING || tt->state == TR_UV_TCP_DONE);
    assert(a == &tt->write_async);

    if (tt->is_writing) {
        return;
    }

    uv_mutex_lock(&tt->wq_mutex);
    if (tt->state == TR_UV_TCP_DONE) {
        while (!QUEUE_EMPTY(&tt->conn_pending_queue)) {
            q = QUEUE_HEAD(&tt->conn_pending_queue);
            
            wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);

            if (!TR_UV_WI_IS_INTERNAL(wi->type)) {
                pc_lib_log(PC_LOG_DEBUG, "tcp__write_async_cb - move wi from conn pending to write wait,"
                    "seq_num: %u, req_id: %u", wi->seq_num, wi->req_id);
            }

            QUEUE_REMOVE(q);
            QUEUE_INIT(q);
            QUEUE_INSERT_TAIL(&tt->write_wait_queue, q);
        }
    }

    buf_cnt = 0;

    QUEUE_FOREACH(q, &tt->write_wait_queue) {
       buf_cnt++; 
    }

    if (buf_cnt == 0) {
        uv_mutex_unlock(&tt->wq_mutex);
        return ;
    }

    uv_buf_t* bufs = (uv_buf_t* )pc_lib_malloc(sizeof(uv_buf_t) * buf_cnt); 

    i = 0;
    while (!QUEUE_EMPTY(&tt->write_wait_queue)) {
        q = QUEUE_HEAD(&tt->write_wait_queue);
        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);

        if (!TR_UV_WI_IS_INTERNAL(wi->type)) {
            pc_lib_log(PC_LOG_DEBUG, "tcp__write_async_cb - move wi from write wait to writing queue,"
                    "seq_num: %u, req_id: %u", wi->seq_num, wi->req_id);
        }

        bufs[i++] = wi->buf;

        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        QUEUE_INSERT_TAIL(&tt->writing_queue, q);
    }

    assert(i == buf_cnt);

    uv_mutex_unlock(&tt->wq_mutex);

    tt->write_req.data = tt;

    ret = uv_write(&tt->write_req, (uv_stream_t* )&tt->socket, bufs, buf_cnt, tcp__write_done_cb); 

    pc_lib_free(bufs);

    if (ret) {
        pc_lib_log(PC_LOG_ERROR, "tcp__write_async_cb - uv write error: %s", uv_strerror(ret));

        uv_mutex_lock(&tt->wq_mutex);
        while(!QUEUE_EMPTY(&tt->writing_queue)) {
            q = QUEUE_HEAD(&tt->writing_queue); 
            QUEUE_REMOVE(q);
            QUEUE_INIT(q);

            wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);

            pc_lib_free(wi->buf.base);
            wi->buf.base = NULL;
            wi->buf.len = 0;

            if (TR_UV_WI_IS_NOTIFY(wi->type)) {
                pc_trans_sent(tt->client, wi->seq_num, ret);
            }

            if (TR_UV_WI_IS_RESP(wi->type)) {
                pc_trans_resp(tt->client, wi->req_id, ret, NULL);
            }
            // if internal, do nothing here.

            if (PC_IS_PRE_ALLOC(wi->type)) {
                PC_PRE_ALLOC_SET_IDLE(wi->type);
            } else {
                pc_lib_free(wi);
            }
        }
        uv_mutex_unlock(&tt->wq_mutex);
        return ;
    }

    tt->is_writing = 1;

    // enable check timeout timer
    if (!uv_is_active((uv_handle_t* )&tt->check_timeout)) {
        uv_timer_start(&tt->check_timeout, tt->write_check_timeout_cb, 
                PC_TIMEOUT_CHECK_INTERVAL * 1000, 0);
    }
}

void tcp__write_done_cb(uv_write_t* w, int status)
{
    QUEUE* q;
    tr_uv_wi_t* wi;
    GET_TT(w);

    assert(tt->is_writing);
    assert(w == &tt->write_req);

    tt->is_writing = 0;

    if (status) {
        pc_lib_log(PC_LOG_ERROR, "tcp__write_done_cb - uv_write callback error: %s", uv_strerror(status));
    }

    status = status == 0 ? PC_RC_OK : PC_RC_ERROR;


    uv_mutex_lock(&tt->wq_mutex);

    while(!QUEUE_EMPTY(&tt->writing_queue)) {
        q = QUEUE_HEAD(&tt->writing_queue); 
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);

        if (!status && TR_UV_WI_IS_RESP(wi->type)) {

            pc_lib_log(PC_LOG_DEBUG, "tcp__write_done_cb - move wi from writing to resp pending queue,"
                " req_id: %u", wi->req_id);

            QUEUE_INSERT_TAIL(&tt->resp_pending_queue, q);
            continue;
        };

        pc_lib_free(wi->buf.base);
        wi->buf.base = NULL;
        wi->buf.len = 0;

        if (TR_UV_WI_IS_NOTIFY(wi->type)) {
            pc_trans_sent(tt->client, wi->seq_num, status);
        }

        if (TR_UV_WI_IS_RESP(wi->type)) {
            pc_trans_resp(tt->client, wi->req_id, status, NULL);
        }
        // if internal, do nothing here.

        if (PC_IS_PRE_ALLOC(wi->type)) {
            PC_PRE_ALLOC_SET_IDLE(wi->type);
        } else {
            pc_lib_free(wi);
        }
    }
    uv_mutex_unlock(&tt->wq_mutex);

    uv_async_send(&tt->write_async);
}

int tcp__check_queue_timeout(QUEUE* ql, pc_client_t* client, int cont)
{
    QUEUE tmp;
    QUEUE* q;
    tr_uv_wi_t* wi;
    time_t ct = time(0);

    QUEUE_INIT(&tmp);
    while (!QUEUE_EMPTY(ql)) {
        q = QUEUE_HEAD(ql);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);

        if (wi->timeout != PC_WITHOUT_TIMEOUT) {
            if (ct > wi->ts + wi->timeout) {
                if (TR_UV_WI_IS_NOTIFY(wi->type)) {
                    pc_lib_log(PC_LOG_WARN, "tcp__checkout_timeout_queue - notify timeout, seq num: %u", wi->seq_num);
                    pc_trans_sent(client, wi->seq_num, PC_RC_TIMEOUT);
                } else if (TR_UV_WI_IS_RESP(wi->type)) {
                    pc_lib_log(PC_LOG_WARN, "tcp__checkout_timeout_queue - request timeout, req id: %u", wi->req_id);
                    pc_trans_resp(client, wi->req_id, PC_RC_TIMEOUT, NULL);
                }

                // if internal, just drop it.
                
                pc_lib_free(wi->buf.base);
                wi->buf.base = NULL;
                wi->buf.len = 0;

                if (PC_IS_PRE_ALLOC(wi->type)) { 
                    PC_PRE_ALLOC_SET_IDLE(wi->type);
                } else {
                    pc_lib_free(wi);
                }
                continue;
            } else {
                // continue to check timeout next tick
                // if there are wis has timeout configured but not triggered this time. 
                cont = 1;
            }
        } 
        // add the non-timeout wi to queue tmp
        QUEUE_INSERT_TAIL(&tmp, q);
    } // while
    // restore ql with the non-timeout wi
    QUEUE_ADD(ql, &tmp);
    QUEUE_INIT(&tmp);
    return cont;
}

void tcp__write_check_timeout_cb(uv_timer_t* w)
{
    time_t ct;
    int cont;
    GET_TT(w);

    ct = time(0);

    assert(w == &tt->check_timeout);

    cont = 0;

    uv_mutex_lock(&tt->wq_mutex);
    cont = tcp__check_queue_timeout(&tt->conn_pending_queue, tt->client, cont); 
    cont = tcp__check_queue_timeout(&tt->write_wait_queue, tt->client, cont);
    uv_mutex_unlock(&tt->wq_mutex);

    cont = tcp__check_queue_timeout(&tt->writing_queue, tt->client, cont);
    cont = tcp__check_queue_timeout(&tt->resp_pending_queue, tt->client, cont);

    if (cont && !uv_is_active((uv_handle_t* )w)) {
        uv_timer_start(w, tt->write_check_timeout_cb, PC_TIMEOUT_CHECK_INTERVAL* 1000, 0);
    }
}

static void tcp__cleanup_json_t(json_t** j)
{
    if (*j) {
        json_decref(*j);
        *j = NULL;
    }
}

void tcp__cleanup_async_cb(uv_async_t* a)
{
    GET_TT(a);

    assert(a == &tt->cleanup_async);

    tt->reset_fn(tt);

    if (tt->host) {
        pc_lib_free((char *)tt->host);
        tt->host = NULL;
    }

    tcp__cleanup_json_t(&tt->handshake_opts);

#define C(x) uv_close((uv_handle_t*)&tt->x, NULL)
    C(socket);
    C(conn_timeout);
    C(reconn_delay_timer);
    C(conn_async);
    C(handshake_timer);
    C(write_async);
    C(check_timeout);
    C(disconnect_async);
    C(cleanup_async);
    C(hb_timer);
    C(hb_timeout_timer);
#undef C

    uv_mutex_destroy(&tt->wq_mutex);

    tcp__cleanup_json_t(&tt->route_to_code);
    tcp__cleanup_json_t(&tt->code_to_route);
    tcp__cleanup_json_t(&tt->dict_ver);

    tcp__cleanup_json_t(&tt->server_protos);
    tcp__cleanup_json_t(&tt->client_protos);
    tcp__cleanup_json_t(&tt->proto_ver);
}

void tcp__disconnect_async_cb(uv_async_t* a)
{
    GET_TT(a);

    assert(a == &tt->disconnect_async);
    tt->reset_fn(tt);
    pc_trans_fire_event(tt->client, PC_EV_DISCONNECT, NULL, NULL);
}

void tcp__send_heartbeat(tr_uv_tcp_transport_t* tt) 
{ 
    uv_buf_t buf;
    int i;
    tr_uv_wi_t* wi;
    wi = NULL;

    assert(tt->state == TR_UV_TCP_DONE);

    pc_lib_log(PC_LOG_DEBUG, "tcp__send__heartbeat - send heartbeat");

    buf = pc_pkg_encode(PC_PKG_HEARBEAT, NULL, 0);

    assert(buf.len && buf.base);

    uv_mutex_lock(&tt->wq_mutex);
    for (i = 0; i < TR_UV_PRE_ALLOC_WI_SLOT_COUNT; ++i) {
        if (PC_PRE_ALLOC_IS_IDLE(tt->pre_wis[i].type)) {
            wi = &tt->pre_wis[i];
            PC_PRE_ALLOC_SET_BUSY(wi->type);
            break;
        }
    }

    if (!wi) {
        wi = (tr_uv_wi_t* )pc_lib_malloc(sizeof(tr_uv_wi_t));
        memset(wi, 0, sizeof(tr_uv_wi_t));
        wi->type = PC_DYN_ALLOC;
    }

    QUEUE_INIT(&wi->queue);
    QUEUE_INSERT_TAIL(&tt->write_wait_queue, &wi->queue);
    TR_UV_WI_SET_INTERNAL(wi->type);
    uv_mutex_unlock(&tt->wq_mutex);

    wi->buf = buf;
    wi->seq_num = -1; // internal data
    wi->req_id = -1; // internal data 
    wi->timeout = TR_UV_INTERNAL_PKG_TIMEOUT; // internal timeout
    wi->ts = time(NULL);
    uv_async_send(&tt->write_async);
}

void tcp__on_heartbeat(tr_uv_tcp_transport_t* tt)
{
    int rtt = 0;
    int start = 0;

    if (!tt->is_waiting_hb) {
        pc_lib_log(PC_LOG_WARN, "tcp__on_heartbeat - tcp is not waiting for heartbeat, ignore");
        return;
    }

    pc_lib_log(PC_LOG_DEBUG, "tcp__on_heartbeat - tcp get heartbeat");
    assert(tt->state == TR_UV_TCP_DONE);
    assert(uv_is_active((uv_handle_t*)&tt->hb_timeout_timer));

    // we hacking uv timer to get the heartbeat rtt, rtt in millisec
    start = tt->hb_timeout_timer.timeout - tt->hb_timeout * 1000;
    rtt = tt->uv_loop.time - start;

    uv_timer_stop(&tt->hb_timeout_timer);

    tt->is_waiting_hb = 0;

    if (tt->hb_rtt == -1 ) {
        tt->hb_rtt = rtt;
    } else {
        tt->hb_rtt = (tt->hb_rtt * 2 + rtt) / 3;
        pc_lib_log(PC_LOG_INFO, "tcp__on_heartbeat - calc rtt: %d", tt->hb_rtt);
    }

    uv_timer_start(&tt->hb_timer, tcp__heartbeat_timer_cb, tt->hb_interval * 1000, 0);
}

void tcp__heartbeat_timer_cb(uv_timer_t* t)
{
    GET_TT(t);

    assert(t == &tt->hb_timer);
    assert(tt->is_waiting_hb == 0);
    assert(tt->state == TR_UV_TCP_DONE);

    tcp__send_heartbeat(tt);
    tt->is_waiting_hb = 1;
    pc_lib_log(PC_LOG_DEBUG, "tcp__heartbeat_timer_cb - start heartbeat timeout timer");

    uv_timer_start(&tt->hb_timeout_timer, tcp__heartbeat_timeout_cb, tt->hb_timeout * 1000, 0);
}

void tcp__heartbeat_timeout_cb(uv_timer_t* t)
{
    GET_TT(t);

    assert(tt->is_waiting_hb);
    assert(t == &tt->hb_timeout_timer);

    pc_lib_log(PC_LOG_WARN, "tcp__heartbeat_timeout_cb - will reconn, hb timeout");
    pc_trans_fire_event(tt->client, PC_EV_UNEXPECTED_DISCONNECT, "HB Timeout", NULL);
    tt->reconn_fn(tt);
}

void tcp__handshake_timer_cb(uv_timer_t* t) 
{
    GET_TT(t);

    assert(t == &tt->handshake_timer);

    pc_lib_log(PC_LOG_ERROR, "tcp__handshake_timer_cb - tcp handshake timeout, will reconn");
    pc_trans_fire_event(tt->client, PC_EV_CONNECT_ERROR, "Connect Timeout", NULL);
    tt->reconn_fn(tt);
}

void tcp__on_tcp_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    GET_TT(stream);

    if (nread < 0) {
        pc_lib_log(PC_LOG_ERROR, "tcp__on_tcp_read_cb - read from tcp error: %s,"
                "will reconn", uv_strerror(nread));
        pc_trans_fire_event(tt->client, PC_EV_UNEXPECTED_DISCONNECT, "Read Error Or Close", NULL);
        pc_lib_free(buf->base);
        tt->reconn_fn(tt);
        return;
    }

    pc_pkg_parser_feed(&tt->pkg_parser, buf->base, nread);
    pc_lib_free(buf->base);
}

void tcp__alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    buf->base = (char* )pc_lib_malloc(suggested_size);
    buf->len = suggested_size;
}

void tcp__on_data_recieved(tr_uv_tcp_transport_t* tt, const char* data, size_t len)
{
    pc_msg_t msg;
    uv_buf_t buf;
    QUEUE* q;
    tr_uv_wi_t* wi = NULL;
    tr_uv_tcp_transport_plugin_t* plugin = (tr_uv_tcp_transport_plugin_t* )tt->base.plugin(tt);

    buf.base = (char* )data;
    buf.len = len;

    msg = plugin->pr_msg_decoder(tt, &buf);

    if (msg.id == PC_INVALID_REQ_ID || !msg.msg) {
        pc_lib_log(PC_LOG_ERROR, "tcp__on_data_recieved - decode error, will reconn");
        pc_trans_fire_event(tt->client, PC_EV_PROTO_ERROR, "Decode Error", NULL);
        tt->reconn_fn(tt);
        return ;
    }

    if (msg.id == PC_NOTIFY_PUSH_REQ_ID && !msg.route) {
        pc_lib_log(PC_LOG_ERROR, "tcp__on_data_recieved - push message without route, error, will reconn");
        pc_trans_fire_event(tt->client, PC_EV_PROTO_ERROR, "No Route Specified", NULL);
        tt->reconn_fn(tt);
        return ;
    }

    assert((msg.id == PC_NOTIFY_PUSH_REQ_ID && msg.route)
            || (msg.id != PC_NOTIFY_PUSH_REQ_ID && !msg.route));

    pc_lib_log(PC_LOG_INFO, "tcp__on_data_recieved - recived data, req_id: %d", msg.id);
    if (msg.id != PC_NOTIFY_PUSH_REQ_ID) {
        // request
        pc_trans_resp(tt->client, msg.id, PC_RC_OK, msg.msg);

        QUEUE_FOREACH(q, &tt->resp_pending_queue) {
            wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
            assert(TR_UV_WI_IS_RESP(wi->type));

            if (wi->req_id != msg.id)
                continue;

            QUEUE_REMOVE(q);
            QUEUE_INIT(q);

            pc_lib_free(wi->buf.base);
            wi->buf.base = NULL;
            wi->buf.len = 0;

            if (PC_IS_PRE_ALLOC(wi->type)) {
                PC_PRE_ALLOC_SET_IDLE(wi->type);
            } else {
                pc_lib_free(wi);
            }
            break;
        }
    } else {
        pc_trans_fire_event(tt->client, PC_EV_USER_DEFINED_PUSH, msg.route, msg.msg);
    }

    pc_lib_free((char *)msg.route);
    pc_lib_free((char *)msg.msg);
}

void tcp__on_kick_recieved(tr_uv_tcp_transport_t* tt)
{
    pc_lib_log(PC_LOG_INFO, "tcp__on_kick_recieved - kicked by server");
    pc_trans_fire_event(tt->client, PC_EV_KICKED_BY_SERVER, NULL, NULL);
    tt->reset_fn(tt);
}

void tcp__send_handshake(tr_uv_tcp_transport_t* tt)
{
    uv_buf_t buf;
    tr_uv_wi_t* wi;
    json_t* sys;
    json_t* body;
    json_t* pc_type;
    json_t* pc_version;
    char* data;
    int i;

    body = json_object();
    sys = json_object();

    assert(tt->state == TR_UV_TCP_HANDSHAKEING);

    assert((tt->proto_ver && tt->client_protos && tt->server_protos)
            || (!tt->proto_ver && !tt->client_protos && !tt->server_protos));

    assert((tt->dict_ver && tt->route_to_code && tt->code_to_route) 
            || (!tt->dict_ver && !tt->route_to_code && !tt->code_to_route));

    if (tt->proto_ver) {
        json_object_set(sys, "protoVersion", tt->proto_ver);
    }

    if (tt->dict_ver) {
        json_object_set(sys, "dictVersion", tt->dict_ver);
    }

    pc_type = json_string(pc_lib_platform_type);
    json_object_set(sys, "type", pc_type);
    json_decref(pc_type);
    pc_type = NULL;

    pc_version = json_string(pc_lib_version_str());
    json_object_set(sys, "version", pc_version);
    json_decref(pc_version);
    pc_version = NULL;

    json_object_set(body, "sys", sys);
    json_decref(sys);
    sys = NULL;

    if (tt->handshake_opts) {
        json_object_set(body, "user", tt->handshake_opts);
    }

    data = json_dumps(body, JSON_COMPACT);

    buf = pc_pkg_encode(PC_PKG_HANDSHAKE, data, strlen(data));

    pc_lib_free(data);
    json_decref(body);

    wi = NULL;
    uv_mutex_lock(&tt->wq_mutex);
    for (i = 0; i < TR_UV_PRE_ALLOC_WI_SLOT_COUNT; ++i) {
        if (PC_PRE_ALLOC_IS_IDLE(tt->pre_wis[i].type)) {
            wi = &tt->pre_wis[i];
            PC_PRE_ALLOC_SET_BUSY(wi->type);
            break;
        }
    }

    if (!wi) {
        wi = (tr_uv_wi_t* )pc_lib_malloc(sizeof(tr_uv_wi_t));
        memset(wi, 0, sizeof(tr_uv_wi_t));
        wi->type = PC_DYN_ALLOC;
    }

    QUEUE_INIT(&wi->queue);
    TR_UV_WI_SET_INTERNAL(wi->type);

    // insert to head
    QUEUE_INSERT_HEAD(&tt->write_wait_queue, &wi->queue);
    uv_mutex_unlock(&tt->wq_mutex);

    wi->buf = buf;
    wi->seq_num = -1; //internal data
    wi->req_id = -1; // internal data 
    wi->timeout = 30; // internal timeout
    wi->ts = time(NULL); // TODO: time() 

    uv_async_send(&tt->write_async);
}

#define PC_HANDSHAKE_OK 200

void tcp__on_handshake_resp(tr_uv_tcp_transport_t* tt, const char* data, size_t len)
{
    json_error_t error;
    json_int_t code;
    json_t* res;
    json_t* tmp;
    json_t* protos;
    json_int_t i;
    json_t* sys;
    int need_sync = 0;
    
    assert(tt->state == TR_UV_TCP_HANDSHAKEING);

    res = json_loadb(data, len, 0, &error);

    pc_lib_log(PC_LOG_INFO, "tcp send get handshake resp");

    if (tt->config->conn_timeout != PC_WITHOUT_TIMEOUT) {
        uv_timer_stop(&tt->handshake_timer);
    }

    if (!res) {
        pc_lib_log(PC_LOG_ERROR, "tcp__on_handshake_resp - handshake resp is not valid json, error: %s", error.text);
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_FAILED, "Handshake Error", NULL);
        tt->reset_fn(tt);
    }

    code = json_integer_value(json_object_get(res, "code"));
    if (code != PC_HANDSHAKE_OK) {
        pc_lib_log(PC_LOG_ERROR, "tcp_on_handshake_resp - handshake fail, code: %d", code);
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_FAILED, "Handshake Error", NULL);
        json_decref(res);
        tt->reset_fn(tt);
        return ;
    }

    // we just use sys here, ignore user field.
    sys = json_object_get(res, "sys");

    assert(sys);

    pc_lib_log(PC_LOG_INFO, "tcp_on_handshake_resp - handshake ok");
    // setup heartbeat
    i = json_integer_value(json_object_get(sys, "heartbeat"));

    if (i <= 0) {
        // no need heartbeat
        tt->hb_interval= -1;
        tt->hb_timeout = -1;
        pc_lib_log(PC_LOG_INFO, "tcp_on_handshake_resp - no heartbeat specified");
    } else {
        tt->hb_interval = i;
        pc_lib_log(PC_LOG_INFO, "tcp_on_handshake_resp - set heartbeat interval: %d", i);
        tt->hb_timeout = tt->hb_interval * PC_HEARTBEAT_TIMEOUT_FACTOR;
    }

    tmp = json_object_get(sys, "useDict");
    if (!tmp || json_equal(tmp, json_false())) {
        if (tt->dict_ver && tt->route_to_code && tt->code_to_route) {
            json_decref(tt->dict_ver);
            json_decref(tt->route_to_code);
            json_decref(tt->code_to_route);

            tt->dict_ver = NULL;
            tt->route_to_code = NULL;
            tt->code_to_route = NULL;
            need_sync = 1;
        }
    } else {
        json_t* route2code = json_object_get(sys, "routeToCode");
        json_t* code2route = json_object_get(sys, "codeToRoute");
        json_t* dict_ver = json_object_get(sys, "dictVersion");

        assert((dict_ver && route2code && code2route) || (!dict_ver && !route2code && !code2route));

        if (dict_ver) {
            if (tt->dict_ver && tt->route_to_code && tt->code_to_route) {
                json_decref(tt->dict_ver);
                json_decref(tt->route_to_code);
                json_decref(tt->code_to_route);
                tt->dict_ver = NULL;
                tt->route_to_code = NULL;
                tt->code_to_route = NULL;
            }

            tt->dict_ver = dict_ver;
            json_incref(dict_ver);

            tt->route_to_code = route2code;
            json_incref(route2code);

            tt->code_to_route = code2route;
            json_incref(code2route);
            need_sync = 1;
        }
        assert(tt->dict_ver && tt->route_to_code && tt->code_to_route);
    }

    tmp = json_object_get(sys, "useProto");
    if (!tmp || json_equal(tmp, json_false())) {
        if (tt->client_protos && tt->proto_ver && tt->server_protos) {
            json_decref(tt->client_protos);
            json_decref(tt->proto_ver);
            json_decref(tt->server_protos);

            tt->client_protos = NULL;
            tt->proto_ver = NULL;
            tt->server_protos = NULL;
            need_sync = 1;
        }
    } else {
        json_t* server_protos = NULL;
        json_t* client_protos = NULL;
        json_t* proto_ver = NULL;

        protos = json_object_get(sys, "protos"); 

        if (protos) {
            server_protos = json_object_get(protos, "server");
            client_protos = json_object_get(protos, "client");
            proto_ver = json_object_get(protos, "version");
        }

        assert((proto_ver && server_protos && client_protos) || (!proto_ver && !server_protos && !client_protos));

        if (proto_ver) {
            if (tt->client_protos && tt->proto_ver && tt->server_protos) {
                json_decref(tt->client_protos);
                json_decref(tt->proto_ver);
                json_decref(tt->server_protos);

                tt->client_protos = NULL;
                tt->proto_ver = NULL;
                tt->server_protos = NULL;
            }   

            tt->client_protos = client_protos;
            json_incref(client_protos);

            tt->server_protos = server_protos;
            json_incref(server_protos);

            tt->proto_ver = proto_ver;
            json_incref(proto_ver);
            need_sync = 1;
        }
        assert(tt->proto_ver && tt->server_protos && tt->client_protos);
    }

    json_decref(res);

    if (tt->config->local_storage_cb && need_sync) {
        json_t* lc = json_object();
        char* data;
        size_t len;

        if (tt->dict_ver) {
            json_object_set(lc, TR_UV_LCK_DICT_VERSION, tt->dict_ver);
        }

        if (tt->route_to_code) {
            json_object_set(lc, TR_UV_LCK_ROUTE_2_CODE, tt->route_to_code);
        }

        if (tt->code_to_route) {
            json_object_set(lc, TR_UV_LCK_CODE_2_ROUTE, tt->code_to_route);
        }

        if (tt->proto_ver) {
            json_object_set(lc, TR_UV_LCK_PROTO_VERSION, tt->proto_ver);
        }
        if (tt->client_protos) {
            json_object_set(lc, TR_UV_LCK_PROTO_CLIENT, tt->client_protos);
        }
        if (tt->server_protos) {
            json_object_set(lc, TR_UV_LCK_PROTO_SERVER, tt->server_protos);
        }
        data = json_dumps(lc, JSON_COMPACT);
        json_decref(lc);

        if (!data) {
            pc_lib_log(PC_LOG_WARN, "tcp__on_handshake_resp - serialize handshake data failed");
        } else {
            len = strlen(data);

            if (tt->config->local_storage_cb(PC_LOCAL_STORAGE_OP_WRITE, data, &len) != 0) {
                pc_lib_log(PC_LOG_WARN, "tcp__on_handshake_resp - write data to local storage error");
            }
            pc_lib_free(data);
        }
    }

    tcp__send_handshake_ack(tt);
    if (tt->hb_interval != -1) {
        pc_lib_log(PC_LOG_INFO, "tcp__on_handshake_resp - start heartbeat interval timer");
        uv_timer_start(&tt->hb_timer, tcp__heartbeat_timer_cb, tt->hb_interval * 1000, 0);
    }
    tt->state = TR_UV_TCP_DONE;
    pc_lib_log(PC_LOG_INFO, "tcp__on_handshake_resp - handshake completely");
    pc_lib_log(PC_LOG_INFO, "tcp__on_handshake_resp - client connected");
    pc_trans_fire_event(tt->client, PC_EV_CONNECTED, NULL, NULL);
    uv_async_send(&tt->write_async);
}

void tcp__send_handshake_ack(tr_uv_tcp_transport_t* tt)
{
    uv_buf_t buf;
    int i;
    tr_uv_wi_t* wi;

    buf = pc_pkg_encode(PC_PKG_HANDSHAKE_ACK, NULL, 0);

    pc_lib_log(PC_LOG_INFO, "tcp__send_handshake_ack - send handshake ack");

    assert(buf.base && buf.len);

    uv_mutex_lock(&tt->wq_mutex);
    for (i = 0; i < TR_UV_PRE_ALLOC_WI_SLOT_COUNT; ++i) {
        if (PC_PRE_ALLOC_IS_IDLE(tt->pre_wis[i].type)) {
            wi = &tt->pre_wis[i];
            PC_PRE_ALLOC_SET_BUSY(wi->type);
            break;
        }
    }

    if (!wi) {
        wi = (tr_uv_wi_t* )pc_lib_malloc(sizeof(tr_uv_wi_t));
        memset(wi, 0, sizeof(tr_uv_wi_t));
        wi->type = PC_DYN_ALLOC;
    }

    QUEUE_INIT(&wi->queue);
    QUEUE_INSERT_HEAD(&tt->write_wait_queue, &wi->queue);
    TR_UV_WI_SET_INTERNAL(wi->type);
    uv_mutex_unlock(&tt->wq_mutex);

    wi->buf = buf;
    wi->seq_num = -1; //internal data
    wi->req_id = -1; // internal data 
    wi->timeout = TR_UV_INTERNAL_PKG_TIMEOUT; // internal timeout
    wi->ts = time(NULL);
    uv_async_send(&tt->write_async);
}

#undef GET_TT

