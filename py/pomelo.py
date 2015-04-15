#
#  Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
#  MIT Licensed.
#

import pypomelo

class Client:
    PC_LOCAL_STORAGE_OP_READ = 0
    PC_LOCAL_STORAGE_OP_WRITE = 1

    PC_RC_OK = 0
    PC_RC_ERROR = -1
    PC_RC_TIMEOUT = -2
    PC_RC_INVALID_JSON = -3
    PC_RC_INVALID_ARG = -4
    PC_RC_NO_TRANS = -5
    PC_RC_INVALID_THREAD = -6
    PC_RC_TRANS_ERROR = -7
    PC_RC_INVALID_ROUTE = -8
    PC_RC_INVALID_STATE = -9
    PC_RC_NOT_FOUND = -10
    PC_RC_RESET = -11

    PC_ST_NOT_INITED = 0
    PC_ST_INITED = 1
    PC_ST_CONNECTING = 2
    PC_ST_CONNECTED = 3
    PC_ST_DISCONNECTING = 4
    PC_ST_UNKNOWN = 5

    PC_LOG_DEBUG = 0
    PC_LOG_INFO = 1
    PC_LOG_WARN = 2
    PC_LOG_ERROR = 3
    PC_LOG_DISABLE = 4

    PC_EV_USER_DEFINED_PUSH = 0
    PC_EV_CONNECTED = 1
    PC_EV_CONNECT_ERROR = 2
    PC_EV_CONNECT_FAILED = 3
    PC_EV_DISCONNECT = 4
    PC_EV_KICKED_BY_SERVER = 5
    PC_EV_UNEXPECTED_DISCONNECT = 6
    PC_EV_PROTO_ERROR = 7

    PC_EV_INVALID_HANDLER_ID = -1

    PC_WITHOUT_TIMEOUT = -1

    # ca_file and ca_path both can be a string or None
    @staticmethod
    def lib_init(log_level, ca_file, ca_path):
        pypomelo.lib_init(log_level, ca_file, ca_path)

    @staticmethod
    def lib_cleanup():
        pypomelo.lib_cleanup()

    @staticmethod
    def ev_to_str(ev_type):
        return pypomelo.ev_to_str(ev_type)

    @staticmethod
    def rc_to_str(rc):
        return pypomelo.rc_to_str(rc)

    @staticmethod
    def state_to_str(st):
        return pypomelo.state_to_str(st)

    def __init__(self):
        self._internal_data = None

    # use_tls - enable tls, Boolean
    # enable_poll - Boolean
    def init(self, use_tls, enable_poll, lc_callback):
        self._internal_data = pypomelo.create(use_tls, enable_poll, lc_callback);
        return self._internal_data is None

    def connect(self, host, port):
        return pypomelo.connect(self._internal_data, host, port)

    def state(self):
        return pypomelo.state(self._internal_data);

    # handler - function (ev_type, arg1, arg2)
    #               ev_type - int
    #               arg1, arg2 - string or None
    def add_ev_handler(self, handler):
        return pypomelo.add_ev_handler(self._internal_data, handler)

    # handler - function (ev_type, arg1, arg2)
    #               ev_type - int
    #               arg1, arg2 - string or None
    def rm_ev_handler(self, handler_id):
        return pypomelo.rm_ev_handler(self._internal_data, handler_id)

    # request_cb - function (rc, resp)
    #                  rc - int
    #                  resp - string or None
    def request(self, route, msg, timeout, req_cb):
        return pypomelo.request(self._internal_data, route, msg, timeout, req_cb)

    # notify_cb - function (rc)
    #                 rc - int
    def notify(self, route, msg, timeout, notify_cb):
        return pypomelo.notify(self._internal_data, route, msg, timeout, notify_cb)

    def poll(self):
        return pypomelo.poll(self._internal_data)

    def quality(self):
        return pypomelo.quality(self._internal_data)

    def disconnect(self):
        return pypomelo.disconnect(self._internal_data)

    def destroy(self):
        if pypomelo.destroy(self._internal_data) == Client.PC_RC_OK:
            self._internal_data = None
            return True
        return False

