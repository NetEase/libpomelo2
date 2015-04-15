/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

package com.netease.pomelo;

public class Client {

    public static final int PC_RC_OK = 0;
    public static final int PC_RC_ERROR = -1;
    public static final int PC_RC_TIMEOUT = -2;
    public static final int PC_RC_INVALID_JSON = -3;
    public static final int PC_RC_INVALID_ARG = -4;
    public static final int PC_RC_NO_TRANS = -5;
    public static final int PC_RC_INVALID_THREAD = -6;
    public static final int PC_RC_TRANS_ERROR = -7;
    public static final int PC_RC_INVALID_ROUTE = -8;
    public static final int PC_RC_INVALID_STATE = -9;
    public static final int PC_RC_NOT_FOUND = -10;
    public static final int PC_RC_RESET = -11;

    public static final int PC_ST_NOT_INITED = 0;
    public static final int PC_ST_INITED = 1;
    public static final int PC_ST_CONNECTING = 2;
    public static final int PC_ST_CONNECTED = 3;
    public static final int PC_ST_DISCONNECTING = 4;
    public static final int PC_ST_UNKNOWN = 5;

    public static final int PC_LOG_DEBUG = 0;
    public static final int PC_LOG_INFO = 1;
    public static final int PC_LOG_WARN = 2;
    public static final int PC_LOG_ERROR = 3;
    public static final int PC_LOG_DISABLE = 4;

    public static final int PC_EV_USER_DEFINED_PUSH = 0;
    public static final int PC_EV_CONNECTED = 1;
    public static final int PC_EV_CONNECT_ERROR = 2;
    public static final int PC_EV_CONNECT_FAILED = 3;
    public static final int PC_EV_DISCONNECT = 4;
    public static final int PC_EV_KICKED_BY_SERVER = 5;
    public static final int PC_EV_UNEXPECTED_DISCONNECT = 6;
    public static final int PC_EV_PROTO_ERROR = 7;

    public static final int PC_EV_INVALID_HANDLER_ID = -1;

    public static final int PC_WITHOUT_TIMEOUT = -1;

    public static native void libInit(int logLevel, String caFile, String caPath);
    public static native void libCleanup();

    public static native String evToStr(int ev);
    public static native String rcToStr(int rc);
    public static native String stateToStr(int st);

    public interface RequestCallback {
        public void handle(int rc, String resp);
    }

    public interface LocalStorage {
        public String read();
        public int write(String lc);
    }

    public interface NotifyCallback {
        public void handle(int rc);
    }

    public interface EventHandler {
        public void handle(int ev, String arg1, String arg2);
    }

    static {
        System.loadLibrary("jpomelo");
    }

    public native int init(boolean enableTLS, boolean enablePoll, LocalStorage lc);
    public native int connect(String host, int port);
    public native int state();
    public native int request(String route, String msg, int timeout, RequestCallback cb);
    public native int notify(String route, String msg, int timeout, NotifyCallback cb);
    public native int disconnect();
    public native int poll();
    public native int quality();
    public native int destroy();

    public native int addEventHandler(EventHandler handler);
    public native int rmEventHandler(int id);

    private byte[] jniUse;
}

