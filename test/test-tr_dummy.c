/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pomelo.h>
#include <pomelo_trans.h>

#include "test.h"

#include "../src/tr/dummy/tr_dummy.h"

static pc_client_t* client;

#define REQ_ROUTE "request.route.method"
#define REQ_MSG "request.msg"
#define REQ_EX ((void*)0x22)
#define REQ_TIMEOUT 10

#define NOTI_ROUTE "notify.route.method"
#define NOTI_MSG "notify.msg"
#define NOTI_EX ((void*)0x33)
#define NOTI_TIMEOUT 30

void request_cb(const pc_request_t* req, int rc, const char* resp)
{
    PC_TEST_ASSERT(rc == PC_RC_OK);
    PC_TEST_ASSERT(resp);
    PC_TEST_ASSERT(!strcmp(resp, TR_DUMMY_RESP));

    PC_TEST_ASSERT(pc_request_client(req) == client);
    PC_TEST_ASSERT(!strcmp(pc_request_route(req), REQ_ROUTE));
    PC_TEST_ASSERT(!strcmp(pc_request_msg(req), REQ_MSG));
    PC_TEST_ASSERT(pc_request_ex_data(req) == REQ_EX);
    PC_TEST_ASSERT(pc_request_timeout(req) == REQ_TIMEOUT);
}

void notify_cb(const pc_notify_t* noti, int rc)
{
    PC_TEST_ASSERT(rc == PC_RC_OK);

    PC_TEST_ASSERT(pc_notify_client(noti) == client);
    PC_TEST_ASSERT(!strcmp(pc_notify_route(noti), NOTI_ROUTE));
    PC_TEST_ASSERT(!strcmp(pc_notify_msg(noti), NOTI_MSG));
    PC_TEST_ASSERT(pc_notify_ex_data(noti) == NOTI_EX);
    PC_TEST_ASSERT(pc_notify_timeout(noti) == NOTI_TIMEOUT);
}

int main()
{
    pc_client_config_t config = PC_CLIENT_CONFIG_DEFAULT;
    pc_lib_init(NULL, NULL, NULL, NULL);

    config.transport_name = PC_TR_NAME_DUMMY;

    client = (pc_client_t*)malloc(pc_client_size());

    PC_TEST_ASSERT(client);

    pc_client_init(client, (void*)0x11, &config);

    PC_TEST_ASSERT(pc_client_ex_data(client) == (void*)0x11);
    PC_TEST_ASSERT(pc_client_state(client) == PC_ST_INITED);

    pc_client_connect(client, "127.0.0.1", 1234, NULL);

    PC_TEST_ASSERT(pc_client_state(client) == PC_ST_CONNECTED);

    pc_request_with_timeout(client, REQ_ROUTE, REQ_MSG, REQ_EX, REQ_TIMEOUT, request_cb);
    pc_notify_with_timeout(client, NOTI_ROUTE, NOTI_MSG, NOTI_EX, NOTI_TIMEOUT, notify_cb);

    pc_client_disconnect(client);

    PC_TEST_ASSERT(pc_client_state(client) == PC_ST_INITED);

    pc_client_cleanup(client);

    PC_TEST_ASSERT(pc_client_state(client) == PC_ST_NOT_INITED);

    free(client);

    pc_lib_cleanup();

    return 0;
}

