/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#include <assert.h>
#include <string.h>

#include <Python.h>

#include <pomelo.h>

#ifdef _WIN32
#define PY_POMELO_EXPORT __declspec(dllexport)
#else
#define PY_POMELO_EXPORT
#endif

static void default_destructor(void* ex_data)
{
    PyObject* ev_cb = (PyObject*) ex_data;

    PyGILState_STATE state;
    state = PyGILState_Ensure();

    Py_XDECREF(ev_cb);

    PyGILState_Release(state);
}

static void default_request_cb(const pc_request_t* req, int rc, const char* resp)
{
    PyGILState_STATE state;
    int enable_poll;
    pc_client_t* client = pc_request_client(req);
    PyObject* req_cb = (PyObject* )pc_request_ex_data(req);
    PyObject* args;
    PyObject* result;

    assert(client && req_cb);
    enable_poll = pc_client_config(client)->enable_polling;

    if (!enable_poll) {
        state = PyGILState_Ensure();
    }

    args = Py_BuildValue("(iz)", rc, resp);
    assert(args);

    result = PyEval_CallObject(req_cb, args);
    if (result == NULL) {
        PyErr_Print();
        abort();
    }

    Py_XDECREF(args);
    Py_XDECREF(req_cb);
    Py_XDECREF(result);

    if (!enable_poll) {
        PyGILState_Release(state);
    }
}

static void default_notify_cb(const pc_notify_t* notify, int rc)
{
    PyGILState_STATE state;
    int enable_poll;
    pc_client_t* client = pc_notify_client(notify);
    PyObject* notify_cb = (PyObject* )pc_notify_ex_data(notify);
    PyObject* args;
    PyObject* result;

    assert(client && notify_cb);
    enable_poll = pc_client_config(client)->enable_polling;

    if (!enable_poll) {
        state = PyGILState_Ensure();
    }

    args = Py_BuildValue("(i)", rc);
    assert(args);

    result = PyEval_CallObject(notify_cb, args);

    if (result == NULL) {
        PyErr_Print();
        abort();
    }

    Py_XDECREF(args);
    Py_XDECREF(notify_cb);
    Py_XDECREF(result);

    if (!enable_poll) {
        PyGILState_Release(state);
    }
}

static void default_event_cb(pc_client_t* client, int ev_type, void* ex_data, const char* arg1, const char* arg2)
{
    PyGILState_STATE state;
    int enable_poll;
    PyObject* ev_cb = (PyObject*)ex_data;
    PyObject* args;
    PyObject* result;

    assert(client && ev_cb);
    enable_poll = pc_client_config(client)->enable_polling;

    if (!enable_poll) {
        state = PyGILState_Ensure();
    }

    args = Py_BuildValue("(izz)", ev_type, arg1, arg2);
    assert(args);

    result = PyEval_CallObject(ev_cb, args);

    if (result == NULL) {
        PyErr_Print();
        abort();
    }

    Py_XDECREF(args);
    Py_XDECREF(result);

    if (!enable_poll) {
        PyGILState_Release(state);
    }
}

static int local_storage_cb(pc_local_storage_op_t op, char* data, size_t* len, void* ex_data)
{
    PyObject* lc_cb = (PyObject*)ex_data;
    PyObject* args = NULL;
    PyObject* result = NULL;
    int ret = -1;
    char* res = NULL;

    PyGILState_STATE state;
    state = PyGILState_Ensure();

    if (op == PC_LOCAL_STORAGE_OP_WRITE) {
        args = Py_BuildValue("(is#)", op, data, *len);

        assert(args);

        result = PyEval_CallObject(lc_cb, args);
        if (result == NULL) {
            PyErr_Print();
            abort();
        } else {
            assert(PyInt_Check(result));
            ret = PyInt_AsLong(result);
        }

    } else {

        args = Py_BuildValue("(i)", op);
        assert(args);
        result = PyEval_CallObject(lc_cb, args);

        if (result == NULL) {
            PyErr_Print();
            abort();
        } else {
            assert(PyString_Check(result) || result == Py_None);
            if (result != Py_None) {
                res = PyString_AsString(result);
                if (res) {
                    *len = strlen(res);
                    if (*len != 0) {
                        if (data) {
                            memcpy(data, res, *len);
                        }
                        ret = 0;
                    }
                } /* if (res) */
            }
        }
    }

    Py_XDECREF(result);
    Py_XDECREF(args);
    PyGILState_Release(state);

    return ret;
}

static PyObject* lib_init(PyObject* self, PyObject* args)
{
    int log_level;
    char* ca_file = NULL;
    char* ca_path = NULL;
    if (!PyArg_ParseTuple(args, "i|zz:lib_init",
                &log_level, &ca_file, &ca_path)) {
       return NULL;
    }

#if !defined(PC_NO_UV_TLS_TRANS)
    if (ca_file || ca_path) {
        tr_uv_tls_set_ca_file(ca_file, ca_path);
    }
#endif

    pc_lib_set_default_log_level(log_level);
    pc_lib_init(NULL, NULL, NULL, "Python Client");

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* lib_cleanup(PyObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ":lib_cleanup")) {
       return NULL;
    }

    pc_lib_cleanup();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* ev_to_str(PyObject* self, PyObject* args)
{
    int ev = 0;

    if (!PyArg_ParseTuple(args, "i:ev_to_str", &ev)) {
       return NULL;
    }

    return Py_BuildValue("s", pc_client_ev_str(ev));
}

static PyObject* rc_to_str(PyObject* self, PyObject* args)
{
    int rc = 0;

    if (!PyArg_ParseTuple(args, "i:rc_to_str", &rc)) {
       return NULL;
    }

    return Py_BuildValue("s", pc_client_rc_str(rc));
}

static PyObject* state_to_str(PyObject* self, PyObject* args)
{
    int state = 0;

    if (!PyArg_ParseTuple(args, "i:state_to_str", &state)) {
       return NULL;
    }

    return Py_BuildValue("s", pc_client_state_str(state));
}

static PyObject* create(PyObject* self, PyObject* args)
{
    int tls = 0;
    int polling = 0;
    pc_client_t* client = NULL;
    PyObject* lc_callback = NULL;
    int ret;
    pc_client_config_t config = PC_CLIENT_CONFIG_DEFAULT;

    if (!PyArg_ParseTuple(args, "|iiO:init", &tls, &polling, &lc_callback)) {
       return NULL;
    }

    if (!PyCallable_Check(lc_callback)) {
        PyErr_SetString(PyExc_TypeError, "parameter lc_callback must be callable");
        return NULL;
    }

    if (tls) {
        config.transport_name = PC_TR_NAME_UV_TLS;
    }
    if (polling) {
        config.enable_polling = 1;
    }

    Py_XINCREF(lc_callback);
    config.local_storage_cb = local_storage_cb;
    config.ls_ex_data = lc_callback;

    client = (pc_client_t* )malloc(pc_client_size());
    if (!client) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    ret = pc_client_init(client, NULL, &config);
    if (ret != PC_RC_OK) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return Py_BuildValue("k", (unsigned long)(client));
}

static PyObject* connect(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    char* host = NULL;
    int port = 0;
    int ret;

    if (!PyArg_ParseTuple(args, "ksi:connect", &addr, &host, &port)) {
       return NULL;
    }

    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    ret = pc_client_connect(client, host, port, NULL);

    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", ret);
}

static PyObject* state(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    int state;

    if (!PyArg_ParseTuple(args, "k:state", &addr)) {
       return NULL;
    }
    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    state = pc_client_state(client);

    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", state);
}

static PyObject* add_ev_handler(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    PyObject* ev_cb = NULL;
    int ret;

    if (!PyArg_ParseTuple(args, "kO:add_ev_handler", &addr, &ev_cb)) {
        return NULL;
    }

    if (!PyCallable_Check(ev_cb)) {
        PyErr_SetString(PyExc_TypeError, "parameter ev_cb must be callable");
        return NULL;
    }

    assert(ev_cb);

    client = (pc_client_t* )addr;
    Py_XINCREF(ev_cb);

    Py_BEGIN_ALLOW_THREADS

    ret = pc_client_add_ev_handler(client, default_event_cb, ev_cb, default_destructor);

    Py_END_ALLOW_THREADS

    if (ret == PC_EV_INVALID_HANDLER_ID) {
        Py_XDECREF(ev_cb);
    }

    return Py_BuildValue("i", ret);
}

static PyObject* rm_ev_handler(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    int handler_id;
    int ret;

    if (!PyArg_ParseTuple(args, "ki:rm_ev_handler", &addr, &handler_id)) {
        return NULL;
    }

    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    ret = pc_client_rm_ev_handler(client, handler_id);

    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", ret);
}

static PyObject* request(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    char* route = NULL;
    char* msg = NULL;
    int timeout = 0;
    PyObject* req_cb = NULL;
    int ret;

    if (!PyArg_ParseTuple(args, "kssiO:request", &addr,
                &route, &msg, &timeout, &req_cb)) {
        return NULL;
    }

    if (!PyCallable_Check(req_cb)) {
        PyErr_SetString(PyExc_TypeError, "parameter req_cb must be callable");
        return NULL;
    }

    assert(req_cb);

    Py_XINCREF(req_cb);
    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    ret = pc_request_with_timeout(client, route, msg,
                req_cb, timeout, default_request_cb);

    Py_END_ALLOW_THREADS

    if (ret != PC_RC_OK) {
        Py_XDECREF(req_cb);
    }

    return Py_BuildValue("i", ret);
}

static PyObject* notify(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    char* route = NULL;
    char* msg = NULL;
    int timeout = 0;
    PyObject* notify_cb = NULL;
    int ret;

    if (!PyArg_ParseTuple(args, "kssiO:request", &addr,
                &route, &msg, &timeout, &notify_cb)) {
        return NULL;
    }

    if (!PyCallable_Check(notify_cb)) {
        PyErr_SetString(PyExc_TypeError, "parameter notify_cb must be callable");
        return NULL;
    }

    assert(notify_cb);

    Py_XINCREF(notify_cb);
    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    ret = pc_notify_with_timeout(client, route, msg,
                notify_cb, timeout, default_notify_cb);

    Py_END_ALLOW_THREADS

    if (ret != PC_RC_OK) {
        Py_XDECREF(notify_cb);
    }

    return Py_BuildValue("i", ret);
}

static PyObject* poll(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    int ret;

    if (!PyArg_ParseTuple(args, "k:poll", &addr)) {
       return NULL;
    }
    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    ret = pc_client_poll(client);

    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", ret);
}

static PyObject* quality(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;

    if (!PyArg_ParseTuple(args, "k:quality", &addr)) {
       return NULL;
    }
    client = (pc_client_t* )addr;

    return Py_BuildValue("i", pc_client_conn_quality(client));
}

static PyObject* disconnect(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    int ret;

    if (!PyArg_ParseTuple(args, "k:disconnect", &addr)) {
       return NULL;
    }
    client = (pc_client_t* )addr;

    Py_BEGIN_ALLOW_THREADS

    ret = pc_client_disconnect(client);

    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", ret);
}

static PyObject* destroy(PyObject* self, PyObject* args)
{
    unsigned long addr;
    pc_client_t* client;
    PyObject* lc_cb;
    int ret;

    if (!PyArg_ParseTuple(args, "k:destroy", &addr)) {
       return NULL;
    }
    client = (pc_client_t* )addr;
    assert(client);

    Py_BEGIN_ALLOW_THREADS

    ret = pc_client_cleanup(client);

    Py_END_ALLOW_THREADS

    if (ret == PC_RC_OK) {
        lc_cb = (PyObject*)pc_client_config(client)->ls_ex_data;
        Py_XDECREF(lc_cb);

        free(client);
    }

    return Py_BuildValue("i", ret);
}

static PyMethodDef pypomelo_meths[] = {
    {"lib_init", lib_init, METH_VARARGS, "lib init"},
    {"lib_cleanup", lib_cleanup, METH_VARARGS, "lib cleanup"},
    {"ev_to_str", ev_to_str, METH_VARARGS, "convert ev to str"},
    {"rc_to_str", rc_to_str, METH_VARARGS, "convert rc to str"},
    {"state_to_str", state_to_str, METH_VARARGS, "convert state to str"},
    {"create", create, METH_VARARGS, "init a client"},
    {"connect", connect, METH_VARARGS, "connect to server"},
    {"state", state, METH_VARARGS, "get client state"},
    {"add_ev_handler", add_ev_handler, METH_VARARGS, "add ev handler"},
    {"rm_ev_handler", rm_ev_handler, METH_VARARGS, "rm ev handler"},
    {"request", request, METH_VARARGS, "send request"},
    {"notify", notify, METH_VARARGS, "send notify"},
    {"poll", poll, METH_VARARGS, "poll pending event"},
    {"quality", quality, METH_VARARGS, "get connection quality"},
    {"disconnect", disconnect, METH_VARARGS, "disconnect from server"},
    {"destroy", destroy, METH_VARARGS, "destroy client"},
    {NULL, NULL}
};

#ifdef __cplusplus
extern "C" {
#endif

PY_POMELO_EXPORT void initpypomelo()
{
    if (!PyEval_ThreadsInitialized()) {
        PyEval_InitThreads();
    }

    Py_InitModule("pypomelo", pypomelo_meths);
}

#ifdef __cplusplus
}
#endif
