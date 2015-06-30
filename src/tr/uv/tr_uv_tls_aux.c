/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <assert.h>

#include <pc_lib.h>
#include <pc_pomelo_i.h>

#include "tr_uv_tcp_aux.h"
#include "tr_uv_tls_aux.h"

#define GET_TLS(x) tr_uv_tls_transport_t* tls; \
    tr_uv_tcp_transport_t * tt;          \
    tls = (tr_uv_tls_transport_t* )x->data; \
    tt = &tls->base; assert(tt && tls)

#define GET_TT tr_uv_tcp_transport_t* tt = &tls->base; assert(tt && tls)

static void tls__read_from_bio(tr_uv_tls_transport_t* tls);
static int tls__get_error(SSL* tls, int status);
static void tls__write_to_tcp(tr_uv_tls_transport_t* tls);
static void tls__cycle(tr_uv_tls_transport_t* tls);
static void tls__emit_error_event(tr_uv_tls_transport_t* tls);
static void tls__info_callback(const SSL* tls, int where, int ret);

void tls__reset(tr_uv_tcp_transport_t* tt)
{
    int ret;
    QUEUE* q;

    tr_uv_tls_transport_t* tls = (tr_uv_tls_transport_t* )tt;

    pc_lib_log(PC_LOG_DEBUG, "tls__reset - reset ssl");

    SSL_shutdown(tls->tls);

    /*
     * here tls__write_to_tcp will write close_notify alert
     * or some other fatal alert that leads to error/disconnect
     */
    tls__write_to_tcp(tls);
    tls->is_handshake_completed = 0;

    if (!SSL_clear(tls->tls)) {
        pc_lib_log(PC_LOG_WARN, "tls__reset - ssl clear error: %s",
                ERR_error_string(ERR_get_error(), NULL));
    }

    ret = BIO_reset(tls->in);
    assert(ret == 1);

    ret = BIO_reset(tls->out);
    assert(ret == 1);

    /*
     * write should retry remained, insert it to writing queue
     * then tcp__reset will recycle it.
     */
    if (tls->should_retry) {
        pc_lib_log(PC_LOG_DEBUG, "tls__reset - move should retry wi to writing queue, seq_num: %u, req_id: %u",
                tls->should_retry->seq_num, tls->should_retry->req_id);

        QUEUE_INIT(&tls->should_retry->queue);
        QUEUE_INSERT_TAIL(&tt->writing_queue, &tls->should_retry->queue);

        tls->should_retry = NULL;
    }

    if (tls->retry_wb) {
        pc_lib_free(tls->retry_wb);
        tls->retry_wb = NULL;
        tls->retry_wb_len = 0;
    }

    /* tcp reset will recycle following write item */
    while(!QUEUE_EMPTY(&tls->when_tcp_is_writing_queue)) {
        q = QUEUE_HEAD(&tls->when_tcp_is_writing_queue);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        QUEUE_INSERT_TAIL(&tt->writing_queue, q);
    }

    tcp__reset(tt);
}

void tls__conn_done_cb(uv_connect_t* conn, int status)
{
    GET_TLS(conn);

    tcp__conn_done_cb(conn, status);

    if (!status) {
        pc_lib_log(PC_LOG_INFO, "tls__conn_done_cb - send client hello");

        SSL_set_info_callback(tls->tls, tls__info_callback);
        /* SSL_read will write ClientHello to bio. */
        SSL_set_connect_state(tls->tls);
        tls__read_from_bio(tls);

        /* write ClientHello out */
        tls__write_to_tcp(tls);
    }
}

static void tls__info_callback(const SSL* tls, int where, int ret)
{
    char* str;

    if (!(where & (SSL_CB_HANDSHAKE_START
                    | SSL_CB_HANDSHAKE_DONE
                    | SSL_CB_ALERT
                    | SSL_CB_EXIT)))
        return;

    if (where & SSL_CB_HANDSHAKE_START) {

        pc_lib_log(PC_LOG_DEBUG, "tls__info_callback - handshake start");

    } else if (where & SSL_CB_HANDSHAKE_DONE) {

        pc_lib_log(PC_LOG_DEBUG, "tls__info_callback - handshake done");

    } else if (where & SSL_CB_ALERT) {

        str = (where & SSL_CB_READ) ? "read" : "write";
        pc_lib_log(PC_LOG_DEBUG, "tls__info_callback - alert: %s %s %s",
                str, SSL_alert_type_string_long(ret), SSL_alert_desc_string_long(ret));

    } else if (where & SSL_CB_EXIT) {
        if (ret == 0) {

            pc_lib_log(PC_LOG_DEBUG, "tls__info_callback - tls failed in %s",
                    SSL_state_string_long(tls));

        } else if (ret < 0) {
            pc_lib_log(PC_LOG_DEBUG, "tls__info_callback - tls error in %s",
                    SSL_state_string_long(tls));
        }
    }

}

static void tls__emit_error_event(tr_uv_tls_transport_t* tls)
{
    GET_TT;

    if (!tls->is_handshake_completed) {
        /* won't reconnect if tls handshake failed */
        pc_trans_fire_event(tt->client, PC_EV_CONNECT_FAILED, "TLS Handshake Error", NULL);
        tt->reset_fn(tt);
    } else {
        pc_trans_fire_event(tt->client, PC_EV_UNEXPECTED_DISCONNECT, "TLS Error", NULL);
        tt->reconn_fn(tt);
    }

}

static void tls__write_to_bio(tr_uv_tls_transport_t* tls)
{
    int ret = 0;
    QUEUE* head;
    QUEUE* q;
    tr_uv_wi_t* wi = NULL;
    int flag = 0;

    tr_uv_tcp_transport_t* tt = (tr_uv_tcp_transport_t* )tls;

    if (tt->state == TR_UV_TCP_NOT_CONN)
        return ;

    if (tt->is_writing) {
        pc_lib_log(PC_LOG_DEBUG, "tls__write_to_bio - use tcp is writing queue");
        head = &tls->when_tcp_is_writing_queue;
    } else {
        pc_lib_log(PC_LOG_DEBUG, "tls__write_to_bio - use writing queue");
        head = &tt->writing_queue;
    }

    pc_mutex_lock(&tt->wq_mutex);

    if (tt->state == TR_UV_TCP_DONE) {
        while (!QUEUE_EMPTY(&tt->conn_pending_queue)) {
            q = QUEUE_HEAD(&tt->conn_pending_queue);
            QUEUE_REMOVE(q);
            QUEUE_INIT(q);
            QUEUE_INSERT_TAIL(&tt->write_wait_queue, q);
        }
    }

    pc_mutex_unlock(&tt->wq_mutex);

    if (tls->retry_wb) {
        ret = SSL_write(tls->tls, tls->retry_wb, tls->retry_wb_len);
        assert(ret == -1 || ret == tls->retry_wb_len);

        if (ret == -1) {
            if (tls__get_error(tls->tls, ret)) {
                pc_lib_log(PC_LOG_ERROR, "tls__write_to_bio - SSL_write error, will reconn");
                pc_lib_free(tls->retry_wb);
                tls->retry_wb = NULL;
                tls->retry_wb_len = 0;

                tls__emit_error_event(tls);

                return ;
            } else {
                /* retry fails, do nothing. */
            }
        } else {

            if (!tls->is_handshake_completed) {
                tls->is_handshake_completed = 1;
            }

            /* retry succeeds */
            if (tls->should_retry) {
                QUEUE_INIT(&tls->should_retry->queue);
                QUEUE_INSERT_TAIL(head, &tls->should_retry->queue);

                tls->should_retry = NULL;
            }
            pc_lib_free(tls->retry_wb);
            tls->retry_wb = NULL;
            tls->retry_wb_len = 0;
            /* write to bio success. */
            flag = 1;
        }
    }

    /* retry write buf has been written, try to write more data to bio. */
    if (!tls->retry_wb) {
        while(!QUEUE_EMPTY(&tt->write_wait_queue)) {
            q = QUEUE_HEAD(&tt->write_wait_queue);
            QUEUE_REMOVE(q);
            QUEUE_INIT(q);

            wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
            ret = SSL_write(tls->tls, wi->buf.base, wi->buf.len);
            assert(ret == -1 || ret == (int)(wi->buf.len));
            if (ret == -1) {
                tls->should_retry = wi;
                if (tls__get_error(tls->tls, ret)) {
                    pc_lib_log(PC_LOG_ERROR, "tls__write_to_bio - SSL_write error, will reconn");

                    tls__emit_error_event(tls);

                    return ;
                } else {
                    tls->retry_wb = (char* )pc_lib_malloc(wi->buf.len);
                    memcpy(tls->retry_wb, wi->buf.base, wi->buf.len);
                    tls->retry_wb_len = tls->should_retry->buf.len;
                    break;
                }
            } else {
                if (!tls->is_handshake_completed) {
                    tls->is_handshake_completed = 1;
                }

                pc_lib_log(PC_LOG_DEBUG, "tls__write_to_bio - move wi to writing queue or tcp write queue, seq_num: %u, req_id: %u", wi->seq_num, wi->req_id);
                QUEUE_INIT(&wi->queue);
                QUEUE_INSERT_TAIL(head, &wi->queue);
                flag = 1;
            }
        }
    }

    /* enable check timeout timer */
    if (!uv_is_active((uv_handle_t* )&tt->check_timeout)) {
        uv_timer_start(&tt->check_timeout, tt->write_check_timeout_cb,
                PC_TIMEOUT_CHECK_INTERVAL * 1000, 0);
    }
    if (flag)
        tls__write_to_tcp(tls);
}

static void tls__read_from_bio(tr_uv_tls_transport_t* tls)
{
    int read;
    tr_uv_tcp_transport_t* tt = (tr_uv_tcp_transport_t* )tls;

    do {
        read = SSL_read(tls->tls, tls->rb, PC_TLS_READ_BUF_SIZE);
        if (read > 0) {

            if (!tls->is_handshake_completed) {
                tls->is_handshake_completed = 1;
            }

            pc_pkg_parser_feed(&tt->pkg_parser, tls->rb, read);
        }
    } while (read > 0);

    if (tls__get_error(tls->tls, read)) {
        pc_lib_log(PC_LOG_ERROR, "tls__read_from_bio - SSL_read error, will reconn");

        tls__emit_error_event(tls);
    }
}

/*
 * return 0 if it is not an error
 * otherwise return 1
 */
static int tls__get_error(SSL* ssl, int status)
{
    int err;

    err = SSL_get_error(ssl, status);
    switch (err) {
        case SSL_ERROR_NONE:
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return 0;
        case SSL_ERROR_ZERO_RETURN:
            pc_lib_log(PC_LOG_WARN, "tls__get_error - tls detect shutdown, reconn");
            return 1;
        default:
            assert(err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL);
            pc_lib_log(PC_LOG_ERROR, "tls__get_error - tls error: %s", ERR_error_string(ERR_get_error(), NULL));
            break;
    }
    return 1;
}

static void tls__write_to_tcp(tr_uv_tls_transport_t* tls)
{
    int ret;
    QUEUE* q;
    char* ptr;
    size_t len;
    uv_buf_t buf;
    tr_uv_wi_t* wi = NULL;
    tr_uv_tcp_transport_t* tt = (tr_uv_tcp_transport_t*)tls;

    if (tt->is_writing)
        return;

    len = BIO_pending(tls->out);

    if (len == 0) {
        assert(QUEUE_EMPTY(&tls->when_tcp_is_writing_queue));
        uv_async_send(&tt->write_async);
        return ;
    }

    while(!QUEUE_EMPTY(&tls->when_tcp_is_writing_queue)) {
        q = QUEUE_HEAD(&tls->when_tcp_is_writing_queue);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);
        pc_lib_log(PC_LOG_DEBUG, "tls__write_to_tcp - move wi from when tcp is writing queue to writing queue,"
                " seq_num: %u, req_id: %u", wi->seq_num, wi->req_id);

        QUEUE_INSERT_TAIL(&tt->writing_queue, q);
    }

    BIO_get_mem_data(tls->out, &ptr);

    buf.base = ptr;
    buf.len = len;

    tt->write_req.data = tls;
    ret = uv_write(&tt->write_req, (uv_stream_t* )&tt->socket, &buf, 1, tls__write_done_cb);

    /*
     * Just ignore error returned by `uv_write` here, and it is safe.
     *
     * it just occurs when SSL wants to write close_notify alert
     * after `uv_tcp_t` is closed.
     */
    if (!ret) {
        tt->is_writing = 1;
    }

    BIO_reset(tls->out);
}

void tls__write_done_cb(uv_write_t* w, int status)
{
    tr_uv_wi_t* wi = NULL;
    QUEUE* q;
    GET_TLS(w);

    tt->is_writing = 0;

    if (status) {
        pc_lib_log(PC_LOG_ERROR, "tcp__write_done_cb - uv_write callback error: %s", uv_strerror(status));
    }

    status = status ? PC_RC_ERROR : PC_RC_OK;

    pc_mutex_lock(&tt->wq_mutex);
    while(!QUEUE_EMPTY(&tt->writing_queue)) {
        q = QUEUE_HEAD(&tt->writing_queue);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        wi = (tr_uv_wi_t* )QUEUE_DATA(q, tr_uv_wi_t, queue);

        if (!status && TR_UV_WI_IS_RESP(wi->type)) {
            pc_lib_log(PC_LOG_DEBUG, "tls__write_to_tcp - move wi from  writing queue to resp pending queue,"
                " seq_num: %u, req_id: %u", wi->seq_num, wi->req_id);
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
        /* if internal, do nothing here. */

        if (PC_IS_PRE_ALLOC(wi->type)) {
            PC_PRE_ALLOC_SET_IDLE(wi->type);
        } else {
            pc_lib_free(wi);
        }
    }
    pc_mutex_unlock(&tt->wq_mutex);
    tls__write_to_tcp(tls);
}

static void tls__cycle(tr_uv_tls_transport_t* tls)
{
    tls__write_to_bio(tls);
    tls__read_from_bio(tls);
    tls__write_to_tcp(tls);
}

void tls__write_async_cb(uv_async_t* a)
{
    GET_TLS(a);

    tls__write_to_bio(tls);
}

void tls__write_timeout_check_cb(uv_timer_t* t)
{
    tr_uv_wi_t* wi = NULL;
    int cont = 0;
    time_t ct = time(0); /* TODO: */
    GET_TLS(t);

    wi = tls->should_retry;
    if (wi && wi->timeout != PC_WITHOUT_TIMEOUT && ct > wi->ts + wi->timeout) {
        if (TR_UV_WI_IS_NOTIFY(wi->type)) {
            pc_lib_log(PC_LOG_WARN, "tls__write_timeout_check_cb - notify timeout, seq num: %u", wi->seq_num);
            pc_trans_sent(tt->client, wi->seq_num, PC_RC_TIMEOUT);
        } else if (TR_UV_WI_IS_RESP(wi->type)) {
            pc_lib_log(PC_LOG_WARN, "tls__write_timeout_check_cb - request timeout, req id: %u", wi->req_id);
            pc_trans_resp(tt->client, wi->req_id, PC_RC_TIMEOUT, NULL);
        }

        /* if internal, just drop it. */

        pc_lib_free(wi->buf.base);
        wi->buf.base = NULL;
        wi->buf.len = 0;

        if (PC_IS_PRE_ALLOC(wi->type)) {
            pc_mutex_lock(&tt->wq_mutex);
            PC_PRE_ALLOC_SET_IDLE(wi->type);
            pc_mutex_unlock(&tt->wq_mutex);
        } else {
            pc_lib_free(wi);
        }
        tls->should_retry = NULL;
    }

    pc_mutex_lock(&tt->wq_mutex);
    cont = tcp__check_queue_timeout(&tls->when_tcp_is_writing_queue, tt->client, cont);
    pc_mutex_unlock(&tt->wq_mutex);

    if (cont && !uv_is_active((uv_handle_t* )t)) {
        uv_timer_start(t, tt->write_check_timeout_cb, PC_TIMEOUT_CHECK_INTERVAL* 1000, 0);
    }

    tcp__write_check_timeout_cb(t);
}

void tls__cleanup_async_cb(uv_async_t* a)
{
    GET_TLS(a);

    tcp__cleanup_async_cb(a);

    if (tls->tls) {
        SSL_free(tls->tls);
        tls->tls = NULL;
        /* BIO in and out will be freed by SSL_free */
        tls->in = NULL;
        tls->out = NULL;
    }
}

void tls__on_tcp_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    GET_TLS(stream);

    if ( nread < 0) {
        tcp__on_tcp_read_cb(stream, nread, buf);
        return ;
    }

    BIO_write(tls->in, buf->base, nread);
    tls__cycle(tls);
}
