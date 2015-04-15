/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <pomelo.h>

#include "com_netease_pomelo_Client.h"

#define JNI_BYTE_LEN (sizeof(pc_client_t*))

static JavaVM* g_vm = NULL;

static void default_handler_destructor(void* ex_data)
{
    jobject ev_cb = (jobject) ex_data;
    JNIEnv* env = NULL;
    int ret;
    int shoud_detach = 0;

    if ((*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_4) == JNI_EDETACHED) {
        assert(!env);
        ret = (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);

        assert(!ret);
        assert(env);
        shoud_detach = 1;
    }

    (*env)->DeleteGlobalRef(env, ev_cb);

    if (shoud_detach) {
        (*g_vm)->DetachCurrentThread(g_vm);
    }
}

static void default_request_cb(const pc_request_t* req, int rc, const char* resp)
{
    assert(g_vm);
    JNIEnv* env;
    int ret;
    int enable_polling;

    jclass cls;
    jmethodID m;
    jobject req_cb;
    jstring resp_str;
    pc_client_t* client;

    client = pc_request_client(req);
    req_cb = (jobject)pc_request_ex_data(req);

    assert(client && req_cb);
    enable_polling = pc_client_config(client)->enable_polling;

    if (!enable_polling) {
        ret = (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);
        assert(!ret);
    } else {
        ret = (*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_4);
        assert(!ret);
    }
    assert(env);

    cls = (*env)->GetObjectClass(env, req_cb);
    m = (*env)->GetMethodID(env, cls, "handle", "(ILjava/lang/String;)V");

    resp_str = (*env)->NewStringUTF(env, resp);

    (*env)->CallVoidMethod(env, req_cb, m, rc, resp_str);
    (*env)->DeleteGlobalRef(env, req_cb);

    if (!enable_polling) {
        ret = (*g_vm)->DetachCurrentThread(g_vm);
        assert(!ret);
    }
}

static void default_notify_cb(const pc_notify_t* notify, int rc)
{
    assert(g_vm);
    JNIEnv* env;
    int ret;
    int enable_polling;

    jclass cls;
    jmethodID m;
    jobject notify_cb;
    pc_client_t* client;

    client = pc_notify_client(notify);
    notify_cb = (jobject)pc_notify_ex_data(notify);

    assert(client && notify_cb);

    enable_polling = pc_client_config(client)->enable_polling;

    if (!enable_polling) {
        ret = (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);
        assert(!ret);
    } else {
        ret = (*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_4);
        assert(!ret);
    }
    assert(env);

    cls = (*env)->GetObjectClass(env, notify_cb);
    m = (*env)->GetMethodID(env, cls, "handle", "(I)V");

    (*env)->CallVoidMethod(env, notify_cb, m, rc);
    (*env)->DeleteGlobalRef(env, notify_cb);

    if (!enable_polling) {
        ret = (*g_vm)->DetachCurrentThread(g_vm);
        assert(!ret);
    }
}

static void default_event_cb(pc_client_t* client, int ev_type, void* ex_data, const char* arg1, const char* arg2)
{
    assert(g_vm);
    JNIEnv* env;
    int ret;
    int enable_polling;

    jclass cls;
    jmethodID m;
    jobject ev_cb;

    jstring arg1_str = NULL;
    jstring arg2_str = NULL;

    ev_cb = (jobject)ex_data;

    enable_polling = pc_client_config(client)->enable_polling;

    if (!enable_polling) {
        ret = (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);
        assert(!ret);
    } else {
        ret = (*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_4);
        assert(!ret);
    }
    assert(env);

    cls = (*env)->GetObjectClass(env, ev_cb);
    m = (*env)->GetMethodID(env, cls, "handle", "(ILjava/lang/String;Ljava/lang/String;)V");

    if (arg1) {
        arg1_str = (*env)->NewStringUTF(env, arg1);
        assert(arg1_str);
    }

    if (arg2) {
        arg2_str = (*env)->NewStringUTF(env, arg2);
        assert(arg2_str);
    }

    (*env)->CallVoidMethod(env, ev_cb, m, ev_type, arg1_str, arg2_str);

    if (!enable_polling) {
        ret = (*g_vm)->DetachCurrentThread(g_vm);
        assert(!ret);
    }
}

static int local_storage_cb(pc_local_storage_op_t op, char* data, size_t* len, void* ex_data)
{
    jobject lc_cb = (jobject)ex_data;
    jclass cls;
    jmethodID m;
    jstring data_str;
    int ret;
    JNIEnv* env = NULL;
    const char* res;
    int shoud_detach = 0;

    if ((*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_4) == JNI_EDETACHED) {
        assert(!env);
        ret = (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);
        assert(!ret);
        assert(env);
        shoud_detach = 1;
    }

    cls = (*env)->GetObjectClass(env, lc_cb);

    if (op == PC_LOCAL_STORAGE_OP_WRITE) {
        m = (*env)->GetMethodID(env, cls, "write", "(Ljava/lang/String;)I");
        data_str = (*env)->NewStringUTF(env, data);
        ret = (*env)->CallIntMethod(env, lc_cb, m, data_str);
        if (shoud_detach) {
            (*g_vm)->DetachCurrentThread(g_vm);
        }
        return ret;
    } else {
        m = (*env)->GetMethodID(env, cls, "read", "()Ljava/lang/String;");
        data_str = (jstring) (*env)->CallObjectMethod(env, lc_cb, m);

        if (!data_str) {
            res = NULL;
        } else {
            res = (*env)->GetStringUTFChars(env, data_str, NULL);
        }

        if (res) {
            *len = strlen(res);
            if (data) {
                memcpy(data, res, *len);
            }
            (*env)->ReleaseStringUTFChars(env, data_str, res);
        }

        if (shoud_detach) {
            (*g_vm)->DetachCurrentThread(g_vm);
        }

        return res ? 0 : -1;
    }

    /* never go here. */
    return -1;
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    libInit
 * Signature: (ILjava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_netease_pomelo_Client_libInit
  (JNIEnv *env, jclass clazz, jint logLevel, jstring cafile, jstring capath)
{
    const char* ca_file = NULL;
    const char* ca_path = NULL;

#if !defined(PC_NO_UV_TLS_TRANS)
    if (cafile) {
        ca_file = (*env)->GetStringUTFChars(env, cafile, NULL);
    }

    if (capath) {
        ca_path = (*env)->GetStringUTFChars(env, capath, NULL);
    }

    if (ca_file || ca_path) {
        tr_uv_tls_set_ca_file(ca_file, ca_path);
    }
#endif

    pc_lib_set_default_log_level(logLevel);
    pc_lib_init(NULL, NULL, NULL, "Java Client");

    if ((*env)->GetJavaVM(env, &g_vm)) {
        abort();
    }

    assert(g_vm);

    if (cafile) {
        (*env)->ReleaseStringUTFChars(env, cafile, ca_file);
    }
    if (capath) {
        (*env)->ReleaseStringUTFChars(env, capath, ca_path);
    }
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    libCleanup
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netease_pomelo_Client_libCleanup
  (JNIEnv *env, jclass clazz)
{
    pc_lib_cleanup();
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    evToStr
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_netease_pomelo_Client_evToStr
  (JNIEnv *env, jclass clazz, jint ev)
{
    return (*env)->NewStringUTF(env, pc_client_ev_str(ev));
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    rcToStr
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_netease_pomelo_Client_rcToStr
  (JNIEnv *env, jclass clazz, jint rc)
{
    return (*env)->NewStringUTF(env, pc_client_rc_str(rc));
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    stateToStr
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_netease_pomelo_Client_stateToStr
  (JNIEnv *env, jclass clazz, jint state)
{
    return (*env)->NewStringUTF(env, pc_client_state_str(state));
}

#define GET_CLIENT                                                           \
          jclass cls = (*env)->GetObjectClass(env, obj);                     \
          jfieldID f = (*env)->GetFieldID(env, cls, "jniUse", "[B");         \
          jbyteArray arr = (jbyteArray)(*env)->GetObjectField(env, obj, f);  \
          pc_client_t* client = NULL;                                        \
          (*env)->GetByteArrayRegion(env, arr, 0, JNI_BYTE_LEN, (jbyte*)&client);    \
          assert(client)

/*
 * Class:     com_netease_pomelo_Client
 * Method:    init
 * Signature: (ZZLcom/netease/pomelo/Client/LocalStorage;)I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_init
  (JNIEnv *env, jobject obj, jboolean enable_tls, jboolean enable_poll, jobject lc)
{
    jclass cls = (*env)->GetObjectClass(env, obj);
    jfieldID f = (*env)->GetFieldID(env, cls, "jniUse", "[B");
    jbyteArray arr;
    jobject lc_cb = NULL;
    jobject g_obj = NULL;
    pc_client_t* client = NULL;
    int ret;

    pc_client_config_t config = PC_CLIENT_CONFIG_DEFAULT;

    if (enable_tls) {
        config.transport_name = PC_TR_NAME_UV_TLS;
    }

    if (enable_poll) {
        config.enable_polling = 1;
    }

    lc_cb = (*env)->NewGlobalRef(env, lc);
    config.local_storage_cb = local_storage_cb;
    config.ls_ex_data = lc_cb;

    client = (pc_client_t* )malloc(pc_client_size());

    if (!client) {
        (*env)->DeleteGlobalRef(env, lc_cb);
        return PC_RC_ERROR;
    }

    g_obj = (*env)->NewGlobalRef(env, obj);

    ret = pc_client_init(client, g_obj, &config);

    if (ret != PC_RC_OK) {
        (*env)->DeleteGlobalRef(env, g_obj);
        (*env)->DeleteGlobalRef(env, lc_cb);
        free(client);
        return PC_RC_ERROR;
    }

    arr = (*env)->NewByteArray(env, JNI_BYTE_LEN);
    (*env)->SetByteArrayRegion(env, arr, 0, JNI_BYTE_LEN, (char*)&client);
    (*env)->SetObjectField(env, obj, f, arr);

    return PC_RC_OK;
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    connect
 * Signature: (Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_connect
  (JNIEnv *env, jobject obj, jstring host, jint port)
{
    const char* host_str;
    int ret;
    GET_CLIENT;

    host_str = (*env)->GetStringUTFChars(env, host, NULL);
    ret = pc_client_connect(client, host_str, port, NULL);
    (*env)->ReleaseStringUTFChars(env, host, host_str);
    return ret;
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    state
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_state
  (JNIEnv *env, jobject obj)
{
    GET_CLIENT;
    return pc_client_state(client);
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    request
 * Signature: (Ljava/lang/String;Ljava/lang/String;ILcom/netease/pomelo/Client/RequestCallback;)I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_request
  (JNIEnv *env, jobject obj, jstring route, jstring msg, jint timeout, jobject req_cb)
{
    const char* route_str;
    const char* msg_str;
    int ret;
    jobject req;
    GET_CLIENT;

    if (!route || !msg) {
        return PC_RC_ERROR;
    }

    route_str = (*env)->GetStringUTFChars(env, route, NULL);
    msg_str = (*env)->GetStringUTFChars(env, msg, NULL);
    assert(route_str);
    assert(msg_str);

    req = (*env)->NewGlobalRef(env, req_cb);

    ret = pc_request_with_timeout(client, route_str, msg_str, req, timeout, default_request_cb);
    if (ret != PC_RC_OK) {
        (*env)->DeleteGlobalRef(env, req);
    }
    (*env)->ReleaseStringUTFChars(env, route, route_str);
    (*env)->ReleaseStringUTFChars(env, msg, msg_str);

    return ret;
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    notify
 * Signature: (Ljava/lang/String;Ljava/lang/String;ILcom/netease/pomelo/Client/NotifyCallback;)I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_notify
  (JNIEnv *env, jobject obj, jstring route, jstring msg, jint timeout, jobject notify_cb)
{
    const char* route_str;
    const char* msg_str;
    int ret;
    jobject noti;
    GET_CLIENT;

    if (!route || !msg) {
        return PC_RC_ERROR;
    }

    route_str = (*env)->GetStringUTFChars(env, route, NULL);
    msg_str = (*env)->GetStringUTFChars(env, msg, NULL);
    assert(route_str);
    assert(msg_str);

    noti = (*env)->NewGlobalRef(env, notify_cb);

    ret = pc_notify_with_timeout(client, route_str, msg_str, noti, timeout, default_notify_cb);

    if (ret != PC_RC_OK) {
        (*env)->DeleteGlobalRef(env, noti);
    }

    (*env)->ReleaseStringUTFChars(env, route, route_str);
    (*env)->ReleaseStringUTFChars(env, msg, msg_str);

    return ret;
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    disconnect
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_disconnect
  (JNIEnv *env, jobject obj)
{
    GET_CLIENT;

    return pc_client_disconnect(client);
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    poll
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_poll
  (JNIEnv *env, jobject obj)
{
    GET_CLIENT;

    return pc_client_poll(client);
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    quality
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_quality
  (JNIEnv *env, jobject obj)
{
    GET_CLIENT;

    return pc_client_conn_quality(client);
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    destroy
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_destroy
  (JNIEnv *env, jobject obj)
{
    jobject lc_cb;
    jobject g_obj;
    int ret;
    GET_CLIENT;

    ret = pc_client_cleanup(client);

    if (ret == PC_RC_OK) {
        lc_cb = pc_client_config(client)->ls_ex_data;
        g_obj = pc_client_ex_data(client);

        assert(lc_cb);
        assert(g_obj);

        (*env)->DeleteGlobalRef(env, lc_cb);
        (*env)->DeleteGlobalRef(env, g_obj);

        free(client);
    }

    return ret;
}


/*
 * Class:     com_netease_pomelo_Client
 * Method:    addEventHandler
 * Signature: (ILjava/lang/String;Lcom/netease/pomelo/Client/EventHandler;)I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_addEventHandler
  (JNIEnv *env, jobject obj, jobject handler)
{
    jobject handler_g;
    const char* route_str = NULL;
    int ret;
    GET_CLIENT;

    handler_g = (*env)->NewGlobalRef(env, handler);

    ret = pc_client_add_ev_handler(client, default_event_cb,
            handler_g, default_handler_destructor);

    if (ret == PC_EV_INVALID_HANDLER_ID) {
        (*env)->DeleteGlobalRef(env, handler_g);
    }

    return ret;
}

/*
 * Class:     com_netease_pomelo_Client
 * Method:    rmEventHandler
 * Signature: (ILjava/lang/String;Lcom/netease/pomelo/Client/EventHandler;)I
 */
JNIEXPORT jint JNICALL Java_com_netease_pomelo_Client_rmEventHandler
  (JNIEnv *env, jobject obj, jint id)
{
    GET_CLIENT;

    return pc_client_rm_ev_handler(client, id);
}

#undef GET_CLIENT

