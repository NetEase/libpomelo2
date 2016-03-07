/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <assert.h>

#include <pc_lib.h>

#include "tr_uv_tcp_aux.h"
#include "tr_uv_tls.h"
#include "tr_uv_tls_i.h"
#include "tr_uv_tls_aux.h"

pc_transport_t* tr_uv_tls_create(pc_transport_plugin_t* plugin)
{
    size_t len = sizeof(tr_uv_tls_transport_t);
    tr_uv_tls_transport_t* tls = (tr_uv_tls_transport_t* )pc_lib_malloc(len);
    memset(tls, 0, len);

    /* inherit from tr_uv_tcp */
    tls->base.base.connect = tr_uv_tcp_connect;
    tls->base.base.send = tr_uv_tcp_send;
    tls->base.base.disconnect = tr_uv_tcp_disconnect;
    tls->base.base.cleanup = tr_uv_tcp_cleanup;
    tls->base.base.quality = tr_uv_tcp_quality;
    tls->base.reconn_fn = tcp__reconn;

    /* reimplemetating method */
    tls->base.base.init = tr_uv_tls_init;
    tls->base.base.internal_data = tr_uv_tls_internal_data;
    tls->base.base.plugin = tr_uv_tls_plugin;

    tls->base.reset_fn = tls__reset;
    tls->base.conn_done_cb = tls__conn_done_cb;
    tls->base.write_async_cb = tls__write_async_cb;
    tls->base.cleanup_async_cb = tls__cleanup_async_cb;
    tls->base.on_tcp_read_cb = tls__on_tcp_read_cb;
    tls->base.write_check_timeout_cb = tls__write_timeout_check_cb;

    return (pc_transport_t*)tls;
}

void tr_uv_tls_release(pc_transport_plugin_t* plugin, pc_transport_t* trans)
{
    pc_lib_free(trans);
}

/*
 * the initilization code for openssl lib is extracted from nodejs
 */
static uv_rwlock_t* locks;
static int n;

static void crypto_lock_cb(int mode, int n, const char* file, int line)
{
    assert((mode & CRYPTO_LOCK) || (mode & CRYPTO_UNLOCK));
    assert((mode & CRYPTO_READ) || (mode & CRYPTO_WRITE));

    if (mode & CRYPTO_LOCK) {
        if (mode & CRYPTO_READ)
            uv_rwlock_rdlock(locks + n);
        else
            uv_rwlock_wrlock(locks + n);
    } else {
        if (mode & CRYPTO_READ)
            uv_rwlock_rdunlock(locks + n);
        else
            uv_rwlock_wrunlock(locks + n);
    }
}

static void crypto_lock_init(void)
{
    int i;

    n = CRYPTO_num_locks();
    locks = (uv_rwlock_t* )pc_lib_malloc(n * sizeof(uv_rwlock_t));

    for (i = 0; i < n; i++)
        uv_rwlock_init(locks + i);
}

static void crypto_threadid_cb(CRYPTO_THREADID* tid)
{
    CRYPTO_THREADID_set_numeric(tid, (unsigned long)uv_thread_self());
}

void tr_uv_tls_plugin_on_register(pc_transport_plugin_t* plugin)
{
    tr_uv_tls_transport_plugin_t* pl;
    tr_uv_tcp_plugin_on_register(plugin);

    SSL_load_error_strings();
    ERR_load_BIO_strings();
    SSL_library_init();

    crypto_lock_init();
    CRYPTO_set_locking_callback(crypto_lock_cb);
    CRYPTO_THREADID_set_callback(crypto_threadid_cb);

    pl = (tr_uv_tls_transport_plugin_t* )plugin;
    pl->ctx = SSL_CTX_new(TLSv1_2_client_method());
    if (!pl->ctx) {
        pc_lib_log(PC_LOG_ERROR, "tr_uv_tls_plugin_on_register - tls error: %s",
                ERR_error_string(ERR_get_error(), NULL));
    }
}

void tr_uv_tls_plugin_on_deregister(pc_transport_plugin_t* plugin)
{
    int i;
    tr_uv_tls_transport_plugin_t* pl = (tr_uv_tls_transport_plugin_t* )plugin;

    if (pl->ctx) {
        SSL_CTX_free(pl->ctx);
        pl->ctx = NULL;
    }

    for (i = 0; i < n; i++)
        uv_rwlock_destroy(locks + i);

    pc_lib_free(locks);

    tr_uv_tcp_plugin_on_deregister(plugin);
}

int tr_uv_tls_init(pc_transport_t* trans, pc_client_t* client)
{
    int ret;
    tr_uv_tls_transport_t* tls;
    tr_uv_tcp_transport_t* tt;
    tr_uv_tls_transport_plugin_t* plugin;

    ret = tr_uv_tcp_init(trans, client);

    if (ret != PC_RC_OK) {
        pc_lib_log(PC_LOG_ERROR, "tr_uv_tls_init - init uv tcp error");
        return ret;
    }

    tls = (tr_uv_tls_transport_t* )trans;
    tt = (tr_uv_tcp_transport_t* )trans;
    plugin = (tr_uv_tls_transport_plugin_t*) pc_tr_uv_tls_trans_plugin();

    assert(plugin && tls && tt);

    if (!plugin->ctx) {
        pc_lib_log(PC_LOG_ERROR, "tr_uv_tls_init - the SSL_CTX is null, maybe register tls plugin failed");
        tt->reset_fn(tt);
        return PC_RC_ERROR;
    }

    tls->tls = SSL_new(plugin->ctx);
    if (!tls->tls) {
        ret = ERR_get_error();
        pc_lib_log(PC_LOG_ERROR, "tr_uv_tls_init - create ssl error: %s", ERR_error_string(ret, NULL));
        tt->reset_fn(tt);
        return PC_RC_ERROR;
    }

    if (plugin->enable_verify) {
        SSL_set_verify(tls->tls, SSL_VERIFY_PEER, NULL);
    } else {
        SSL_set_verify(tls->tls, SSL_VERIFY_NONE, NULL);
    }

    SSL_set_connect_state(tls->tls);

    tls->in = BIO_new(BIO_s_mem());
    tls->out = BIO_new(BIO_s_mem());

    tls->is_handshake_completed = 0;

    /* oom, non-handling */
    if (!tls->in || !tls->out)
        abort();

    SSL_set_bio(tls->tls, tls->in, tls->out);

    tls->retry_wb_len = 0;
    tls->retry_wb = NULL;

    tls->should_retry = NULL;
    QUEUE_INIT(&tls->when_tcp_is_writing_queue);

    tls->internal[0] = &tt->uv_loop;
    tls->internal[1] = tls->tls;

    return PC_RC_OK;
}

void* tr_uv_tls_internal_data(pc_transport_t* trans)
{
    tr_uv_tls_transport_t* tls = (tr_uv_tls_transport_t*)trans;
    return tls->internal;
}

pc_transport_plugin_t* tr_uv_tls_plugin(pc_transport_t* trans)
{
    return pc_tr_uv_tls_trans_plugin();
}

