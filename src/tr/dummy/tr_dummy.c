/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <stdlib.h>
#include <assert.h>

#include <pc_lib.h>

#include "tr_dummy.h"

typedef struct dummy_transport_s {
    pc_transport_t base;
    pc_client_t* client;
} dummy_transport_t;

static int dummy_init(pc_transport_t* trans, pc_client_t* client)
{
    dummy_transport_t* d_tr = (dummy_transport_t*) trans;
    assert(d_tr);

    d_tr->client = client;

    return PC_RC_OK;
}

static int dummy_connect(pc_transport_t* trans, const char* host, int port, const char* handshake_opt)
{
    dummy_transport_t* d_tr = (dummy_transport_t* )trans;
    assert(d_tr);

    pc_trans_fire_event(d_tr->client, PC_EV_CONNECTED, NULL, NULL);

    return PC_RC_OK;
}

static int dummy_send(pc_transport_t* trans, const char* route, unsigned int seq_num, const char* msg, unsigned int req_id, int timeout)
{
    dummy_transport_t* d_tr = (dummy_transport_t* )trans;
    assert(d_tr);

    if (req_id == PC_NOTIFY_PUSH_REQ_ID) {
        pc_trans_sent(d_tr->client, seq_num, PC_RC_OK);
    } else {
        pc_trans_resp(d_tr->client, req_id, PC_RC_OK, TR_DUMMY_RESP);
    }

    return PC_RC_OK;
}

static int dummy_disconnect(pc_transport_t* trans)
{
    dummy_transport_t* d_tr = (dummy_transport_t* )trans;
    assert(d_tr);

    pc_trans_fire_event(d_tr->client, PC_EV_DISCONNECT, NULL, NULL);

    return PC_RC_OK;
}

static int dummy_cleanup(pc_transport_t* trans)
{
    return PC_RC_OK;
}

static void* dummy_internal_data(pc_transport_t* trans)
{
    return NULL;
}

static pc_transport_plugin_t* dummy_plugin(pc_transport_t* trans)
{
    return pc_tr_dummy_trans_plugin();
}

static int dummy_conn_quality(pc_transport_t* trans)
{
    return 0;
}

static pc_transport_t* dummy_trans_create(pc_transport_plugin_t* plugin)
{
    pc_transport_t* trans = (pc_transport_t* )pc_lib_malloc(sizeof(dummy_transport_t));

    trans->init = dummy_init;
    trans->connect = dummy_connect;
    trans->send = dummy_send;
    trans->disconnect = dummy_disconnect;
    trans->cleanup = dummy_cleanup;
    trans->internal_data = dummy_internal_data;
    trans->plugin = dummy_plugin;
    trans->quality = dummy_conn_quality;

    return trans;
}

static void dummy_trans_release(pc_transport_plugin_t* plugin, pc_transport_t* trans)
{
    pc_lib_free(trans);
}

static pc_transport_plugin_t instance =
{
    dummy_trans_create,
    dummy_trans_release,
    NULL, /* on_register */
    NULL, /* on_deregister */
    PC_TR_NAME_DUMMY
};

pc_transport_plugin_t* pc_tr_dummy_trans_plugin()
{
    return &instance;
}

