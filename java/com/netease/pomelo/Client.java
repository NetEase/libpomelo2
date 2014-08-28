package com.netease.pomelo;

public final class Client {
    public static final int PC_RC_OK = 0;

    public class Config {
        public int conn_timeout;
    }

    public interface RequestCallback {
        void handle(String resp, int status);
    }

    public interface NotifyCallback {
        void handle(int status);
    }

    public interface EventHandler {
        void handle(int ev_type, String arg1, String arg2);
    }

    static {
        System.loadLibrary("pomelo");
    }

    public native int init(Config c);
    public native int connect(String host, int port);
    public native int state();
    public native int disconnect();
    public native int cleanup();

    public native int addEventHandler(int ev_type, String push_route, EventHandler hander);
    public native int rmEventHandler(int ev_type, String push_route, EventHandler handler);

    public native int request(String route, String msg, int timeout, RequestCallback cb);
    public native int notify(String route, String msg, int timeout, NotifyCallback cb);

    public static void main(String[] args) {
        Client cl = new Client();
    }

    private byte[] jniUse;
}



