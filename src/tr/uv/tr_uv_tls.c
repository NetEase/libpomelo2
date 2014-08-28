/**
 * Copyright (c) 2014 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <assert.h>

#include "tr_uv_tls.h"
#include "tr_uv_tls_i.h"
#include "pr_msg.h"

static tr_uv_tls_transport_plugin_t instance = 
{
    {
        {
            tr_uv_tls_create,
            tr_uv_tls_release,
            tr_uv_tls_plugin_on_register,
            tr_uv_tls_plugin_on_deregister,
            PC_TR_NAME_UV_TLS
        }, // pc_transport_plugin_t
        pr_default_msg_encoder, // encoder
        pr_default_msg_decoder  // decoder
    },
    NULL, // ssl ctx
    0 // enable verify
};

pc_transport_plugin_t* pc_tr_uv_tls_trans_plugin()
{
    return (pc_transport_plugin_t* )&instance;
}

void tr_uv_tls_set_ca_file(const char* ca_file, const char* ca_path)
{
    int ret;
    if (instance.ctx) {
        ret = SSL_CTX_load_verify_locations(instance.ctx, ca_file, ca_path);
        if (!ret) {
            pc_lib_log(PC_LOG_WARN, "load verify locations error, cafile: %s, capath: %s", ca_file, ca_path);
            instance.enable_verify = 0;
        } else {
            instance.enable_verify = 1;
        }
    }
}

