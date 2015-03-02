/**
 * Copyright (c) 2014-2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

using UnityEngine;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityThreading;

//__________________________________________________________________________________________PINVOKE_CALLBACKS

//private delegate string NativeLCRCallback();
using NativeLCRCallback = System.Func<string>;
//private delegate int NativeLCWCallback(IntPtr data_ptr);
using NativeLCWCallback = System.Func<System.IntPtr, int>;
//private delegate void NativeRequestCallback(uint rid, int rc, IntPtr res_ptr);
using NativeRequestCallback = System.Action<uint, int, System.IntPtr>;
//private delegate void NativeNotifyCallback(IntPtr req, int rc);
using NativeNotifyCallback = System.Action<System.IntPtr, int>;
//using NativeEventCallback = System.Action<System.IntPtr, int, System.IntPtr, System.IntPtr, System.IntPtr>;
delegate void NativeEventCallback(IntPtr client, int ev, IntPtr ex_data, IntPtr arg1_ptr, IntPtr arg2_ptr);


public class PomeloClient
{
    public const int PC_RC_OK = 0;
    public const int PC_RC_ERROR = -1;
    public const int PC_RC_TIMEOUT = -2;
    public const int PC_RC_INVALID_JSON = -3;
    public const int PC_RC_INVALID_ARG = -4;
    public const int PC_RC_NO_TRANS = -5;
    public const int PC_RC_INVALID_THREAD = -6;
    public const int PC_RC_TRANS_ERROR = -7;
    public const int PC_RC_INVALID_ROUTE = -8;
    public const int PC_RC_INVALID_STATE = -9;
    public const int PC_RC_NOT_FOUND = -10;
    public const int PC_RC_RESET = -11;

    public const int PC_ST_NOT_INITED = 0;
    public const int PC_ST_INITED = 1;
    public const int PC_ST_CONNECTING = 2;
    public const int PC_ST_CONNECTED = 3;
    public const int PC_ST_DISCONNECTING = 4;
    public const int PC_ST_UNKNOWN = 5;

    public const int PC_LOG_DEBUG = 0;
    public const int PC_LOG_INFO = 1;
    public const int PC_LOG_WARN = 2;
    public const int PC_LOG_ERROR = 3;
    public const int PC_LOG_DISABLE = 4;

    public const int PC_WITHOUT_TIMEOUT = -1;

    public const int PC_EV_USER_DEFINED_PUSH = 0;
    public const int PC_EV_CONNECTED = 1;
    public const int PC_EV_CONNECT_ERROR = 2;
    public const int PC_EV_CONNECT_FAILED = 3;
    public const int PC_EV_DISCONNECT = 4;
    public const int PC_EV_KICKED_BY_SERVER = 5;
    public const int PC_EV_UNEXPECTED_DISCONNECT = 6;
    public const int PC_EV_PROTO_ERROR = 7;

    public const int PC_EV_INVALID_HANDLER_ID = -1;

    //__________________________________________________________________________________________STATIC_FUNCTIONS

    static PomeloClient()
    {
        UnityThreadHelper.EnsureHelper();
    }

    public static Action<object> Log = Nothing;
    public static Action<object> LogError = Nothing;
    static void Nothing(object msg){}

    public static void LibInit(int logLevel)
    {
        LibInit(logLevel, null, null);
    }
    public static void LibInit(int logLevel, string caFile, string caPath)
    {
        NativeLibInit(logLevel, caFile, caPath);
    }
    public static void LibCleanup()
    {
        NativeLibCleanup();
    }
    public static string EvToStr(int ev)
    {
        return Marshal.PtrToStringAuto(NativeEvToStr(ev));
    }
    public static string RcToStr(int rc)
    {
        return Marshal.PtrToStringAuto(NativeRcToStr(rc));
    }
    public static string StateToStr(int state)
    {
        return Marshal.PtrToStringAuto(NativeStateToStr(state));
    }

    //__________________________________________________________________________________________DEFAULT_CALLBACKS_(FROM_OTHER_THERAD)

    private void OnRequest(uint rid, int rc, IntPtr resp_ptr)
    {
#if DEBUG
        Log(string.Format("OnRequest - pinvoke callback START | rc={0}", RcToStr(rc)));
#endif
        string err = rc == PC_RC_OK ? null : RcToStr(rc);
        string res = rc != PC_RC_OK ? null : Marshal.PtrToStringAuto(resp_ptr);
        UnityThreadHelper.Dispatcher.Dispatch(()=>{
#if DEBUG
            Log(string.Format("OnRequest - main thread START | err={0}, res={1}", err, res));
#endif
            try{
                requestHandlers[rid](err, res);
            }catch(Exception ex){
                LogError("error on handle request(" + rid + ") :\n" + ex.ToString());
            }

            requestHandlers.Remove(rid);
#if DEBUG
            Log(string.Format("OnRequest - main thread END"));
#endif
        });
#if DEBUG
        Log(string.Format("OnRequest - pinvoke callback END"));
#endif
    }
    private void OnNotify(IntPtr req, int rc)
    {
#if DEBUG
        Log(string.Format("OnNotify - pinvoke callback START | rc={0}", RcToStr(rc)));
        UnityThreadHelper.Dispatcher.Dispatch(()=>{
            Log(string.Format("OnNotify - main thread START | rc={0}", RcToStr(rc)));
            Log(string.Format("OnNotify - main thread END"));
        });
        Log(string.Format("OnNotify - pinvoke callback END"));
#endif
        // client does not care about the response in production
    }
    private void OnEvent(IntPtr client, int ev, IntPtr ex_data, IntPtr arg1_ptr, IntPtr arg2_ptr)
    {
#if DEBUG
        Log(string.Format("OnEvent - pinvoke callback START | ev={0}", EvToStr(ev)));
#endif
        var arg1 = Marshal.PtrToStringAuto(arg1_ptr);
        var arg2 = Marshal.PtrToStringAuto(arg2_ptr);
        UnityThreadHelper.Dispatcher.Dispatch(()=>{
#if DEBUG
            Log(string.Format("OnEvent - main thread START | arg1={0}, arg2={1}", arg1, arg2));
#endif
            try {
                switch(ev)
                {
                case PC_EV_USER_DEFINED_PUSH:
                    if(eventHandlers.ContainsKey(arg1))
                    {
                        var source = eventHandlers[arg1];
                        EventBinding[] bindings = new EventBinding[source.Count];
                        source.CopyTo(bindings);

                        foreach(var bind in bindings)
                        {
                            bind.handler(arg2);
                            if(bind.once)
                                RemoveEventHandler(arg1, bind.handler, true);
                        }
                    }
                    break;
                case PC_EV_CONNECTED:
                    OnConnectSuccess();
                    break;
                case PC_EV_CONNECT_ERROR:
                    OnConnectFail(arg1);
                    break;
                case PC_EV_CONNECT_FAILED:
                    OnConnectFail(arg1);
                    break;
                case PC_EV_DISCONNECT:
                    OnDisconnect(null); // probably disconnect by client self
                    break;
                case PC_EV_KICKED_BY_SERVER:
                    OnDisconnect(EvToStr(ev));
                    break;
                case PC_EV_UNEXPECTED_DISCONNECT:
                    OnError(arg1);
                    break;
                case PC_EV_PROTO_ERROR:
                    OnError(arg1);
                    break;
                }
            } catch (Exception ex) {
                LogError("OnEvent - main thread EXCEPTION |\n" + ex.ToString());
            }
#if DEBUG
            Log(string.Format("OnEvent - main thread END"));
#endif
        });
#if DEBUG
        Log(string.Format("OnEvent - pinvoke callback END"));
#endif
    }

    private string OnLCR()
    {
#if DEBUG
        Log(string.Format("OnLCR - pinvoke START"));
#endif
        var data = lcReader();
#if DEBUG
        Log(string.Format("OnLCR - pinvoke END"));
#endif
        return data;
    }
    private int OnLCW(IntPtr data_ptr)
    {
#if DEBUG
        Log(string.Format("OnLCW- pinvoke START | &data={0}", (ulong)data_ptr));
#endif
        var data = Marshal.PtrToStringAuto(data_ptr);
        UnityThreadHelper.Dispatcher.Dispatch(()=>{
#if DEBUG
            Log(string.Format("OnLCW- main thread START | data={0}", data));
#endif
            lcWriter(data);
#if DEBUG
            Log(string.Format("OnLCW- main thread END"));
#endif
        });
#if DEBUG
        Log(string.Format("OnLCW - pinvoke END"));
#endif
        return 0;
    }

    private static string DefaultLocalConfigReader()
    {
#if DEBUG
        Log(string.Format("DefaultLocalConfigReader - no config to read"));
#endif
        return null;
    }
    private static void DefaultLocalConfigWriter(string data)
    {
#if DEBUG
        Log(string.Format("DefaultLocalConfigWriter - do not save config : {0}", data));
#endif
    }

    //__________________________________________________________________________________________USER_API

    public Action OnConnectSuccess = delegate {};
    public Action<string> OnConnectFail = delegate {};
    public Action<string> OnDisconnect = delegate {};
    public Action<string> OnError = delegate {};

    private IntPtr client = IntPtr.Zero;

    private readonly NativeLCRCallback nativeLCRCallback;
    private readonly NativeLCWCallback nativeLCWCallback;
    private readonly NativeRequestCallback nativeRequestCallback;
    private readonly NativeNotifyCallback nativeNotifyCallback;
    private readonly NativeEventCallback nativeEventCallback;

    private Func<string> lcReader;
    private Action<string> lcWriter;

    private uint reqUid = 0;
    private int evtId = -1;
    private readonly Dictionary<uint, Action<string, string>> requestHandlers;
    private readonly Dictionary<string, List<EventBinding>> eventHandlers;

    public PomeloClient()
    {
        // why doesn't pass the method directly
        // https://stackoverflow.com/questions/1681930/what-is-the-difference-between-a-delegate-instance-and-a-method-pointer
        nativeLCRCallback = OnLCR;
        nativeLCWCallback = OnLCW;
        nativeRequestCallback = OnRequest;
        nativeNotifyCallback = OnNotify;
        nativeEventCallback = OnEvent;

        requestHandlers = new Dictionary<uint, Action<string, string>>();
        eventHandlers = new Dictionary<string, List<EventBinding>>();
    }

    public bool Init(bool enableTLS, bool enablePolling)
    {
        return Init(enableTLS, enablePolling, DefaultLocalConfigReader, DefaultLocalConfigWriter);
    }
    public bool Init(bool enableTLS, bool enablePolling, Func<string> reader, Action<string> writer)
    {
        if(client != IntPtr.Zero)
            return false;

        lcReader = reader ?? DefaultLocalConfigReader;
        lcWriter = writer ?? DefaultLocalConfigWriter;

        client = NativeCreate(enableTLS, enablePolling, nativeLCRCallback, nativeLCWCallback);
#if DEBUG
        Log(string.Format("Init - create client {0}", (ulong)client));
#endif
        if(client == IntPtr.Zero)
            return false;

        evtId = NativeAddEventHandler(client, nativeEventCallback, IntPtr.Zero, IntPtr.Zero);
        return true;
    }
    public void Destroy()
    {
#if DEBUG
        Log(string.Format("Destroy - main thread START"));
#endif
        CheckClient();
        // it is not necessary for event handler will be removed when cleanup client
        // NativeRemoveEventHandler(client, evtId);
        requestHandlers.Clear();
        eventHandlers.Clear();
        reqUid = 0;
        evtId = -1;
        NativeDestroy(client);
        client = IntPtr.Zero;
#if DEBUG
        Log(string.Format("Destroy - main thread END"));
#endif
    }
    public void Connect(string host, int port)
    {
        CheckClient();
        NativeConnect(client, host, port, null);
    }
    public void Disconnect()
    {
        CheckClient();
        NativeDisconnect(client);
    }
    public void Request(string route, string msg, Action<string, string> handler)
    {
        Request(route, msg, 10, handler);
    }
    public void Request(string route, string msg, int timeout, Action<string, string> handler)
    {
        CheckClient();

        ++reqUid;
        requestHandlers.Add(reqUid, handler);

        var rc = NativeRequest(client, route, msg, reqUid, timeout, nativeRequestCallback);

        if(rc != PC_RC_OK)
        {
            handler(RcToStr(rc), null);
            requestHandlers.Remove(reqUid);
        }
    }
    public void Notify(string route, string msg)
    {
        Notify(route, msg, 10);
    }
    public void Notify(string route, string msg, int timeout)
    {
        CheckClient();
        NativeNotify(client, route, msg, IntPtr.Zero, timeout, nativeNotifyCallback);
    }
    public void Poll()
    {
        CheckClient();
        NativePoll(client);
    }


    public void AddEventHandler(string eventName, Action<string> handler, bool once = false)
    {
        var e = new EventBinding{
            handler = handler,
            once = once,
        };

        if(! eventHandlers.ContainsKey(eventName))
            eventHandlers.Add(eventName, new List<EventBinding>());
        if(! eventHandlers[eventName].Contains(e))
            eventHandlers[eventName].Add(e);
    }
    public void RemoveEventHandler()
    {
        eventHandlers.Clear();
    }
    public void RemoveEventHandler(string eventName)
    {
        eventHandlers.Remove(eventName);
    }
    public void RemoveEventHandler(string eventName, Action<string> handler, bool once = false)
    {
        if(! eventHandlers.ContainsKey(eventName))
            return;

        var e = new EventBinding{
            handler = handler,
            once = once,
        };
        eventHandlers[eventName].Remove(e);
    }


    public int Quality{
        get{
            CheckClient();
            return NativeQuality(client);
        }
    }

    public int State{
        get{
            CheckClient();
            return NativeState(client);
        }
    }

    void CheckClient()
    {
        if(client == IntPtr.Zero)
            throw new NullReferenceException("invalid client");
    }

    private class EventBinding
    {
        public Action<string> handler;
        public bool once;

        public override bool Equals (object obj)
        {
            var item = obj as EventBinding;
            if(item != null)
                return handler == item.handler && once == item.once;
            return base.Equals (obj);
        }

        public override int GetHashCode ()
        {
            return base.GetHashCode ();
        }
    }

    //__________________________________________________________________________________________PINVOKE_FUNCTIONS

#if UNITY_IPHONE || UNITY_XBOX360
    // TODO test on device
    [DllImport("__Internal", EntryPoint="lib_init")]
    private static extern void NativeLibInit(int log_level, string ca_file, string ca_path);
    [DllImport("__Internal", EntryPoint="pc_lib_cleanup")]
    private static extern void NativeLibCleanup();

    [DllImport("__Internal", EntryPoint="pc_client_ev_str")]
    private static extern IntPtr NativeEvToStr(int ev);
    [DllImport("__Internal", EntryPoint="pc_client_rc_str")]
    private static extern IntPtr NativeRcToStr(int rc);
    [DllImport("__Internal", EntryPoint="pc_client_state_str")]
    private static extern IntPtr NativeStateToStr(int state);

    [DllImport("__Internal", EntryPoint="create")]
    private static extern IntPtr NativeCreate(bool enable_tls, bool enable_poll, NativeLCRCallback read, NativeLCWCallback write);
    [DllImport("__Internal", EntryPoint="destroy")]
    private static extern int NativeDestroy(IntPtr client);

    [DllImport("__Internal", EntryPoint="pc_client_connect")]
    private static extern int NativeConnect(IntPtr client, string host, int port, string handsharkOpts);
    [DllImport("__Internal", EntryPoint="pc_client_disconnect")]
    private static extern int NativeDisconnect(IntPtr client);

    [DllImport("__Internal", EntryPoint="request")]
    private static extern int NativeRequest(IntPtr client, string route, string msg, uint cb_uid, int timeout, NativeRequestCallback callback);
    [DllImport("__Internal", EntryPoint="pc_notify_with_timeout")]
    private static extern int NativeNotify(IntPtr client, string route, string msg, IntPtr ex_data, int timeout, NativeNotifyCallback callback);
    [DllImport("__Internal", EntryPoint="pc_client_poll")]
    private static extern int NativePoll(IntPtr client);

    [DllImport("__Internal", EntryPoint="pc_client_add_ev_handler")]
    private static extern int NativeAddEventHandler(IntPtr client, NativeEventCallback callback, IntPtr ex_data, IntPtr destructor);
    [DllImport("__Internal", EntryPoint="pc_client_rm_ev_handler")]
    private static extern int NativeRemoveEventHandler(IntPtr client, int handler_id);

    [DllImport("__Internal", EntryPoint="pc_client_conn_quality")]
    private static extern int NativeQuality(IntPtr client);
    [DllImport("__Internal", EntryPoint="pc_client_state")]
    private static extern int NativeState(IntPtr client);
#else
    [DllImport("cspomelo", EntryPoint="lib_init")]
    private static extern void NativeLibInit(int log_level, string ca_file, string ca_path);
    [DllImport("cspomelo", EntryPoint="pc_lib_cleanup")]
    private static extern void NativeLibCleanup();

    [DllImport("cspomelo", EntryPoint="pc_client_ev_str")]
    private static extern IntPtr NativeEvToStr(int ev);
    [DllImport("cspomelo", EntryPoint="pc_client_rc_str")]
    private static extern IntPtr NativeRcToStr(int rc);
    [DllImport("cspomelo", EntryPoint="pc_client_state_str")]
    private static extern IntPtr NativeStateToStr(int state);

    [DllImport("cspomelo", EntryPoint="create")]
    private static extern IntPtr NativeCreate(bool enable_tls, bool enable_poll, NativeLCRCallback read, NativeLCWCallback write);
    [DllImport("cspomelo", EntryPoint="destroy")]
    private static extern int NativeDestroy(IntPtr client);

    [DllImport("cspomelo", EntryPoint="pc_client_connect")]
    private static extern int NativeConnect(IntPtr client, string host, int port, string handsharkOpts);
    [DllImport("cspomelo", EntryPoint="pc_client_disconnect")]
    private static extern int NativeDisconnect(IntPtr client);

    [DllImport("cspomelo", EntryPoint="request")]
    private static extern int NativeRequest(IntPtr client, string route, string msg, uint cb_uid, int timeout, NativeRequestCallback callback);
    [DllImport("cspomelo", EntryPoint="pc_notify_with_timeout")]
    private static extern int NativeNotify(IntPtr client, string route, string msg, IntPtr ex_data, int timeout, NativeNotifyCallback callback);
    [DllImport("cspomelo", EntryPoint="pc_client_poll")]
    private static extern int NativePoll(IntPtr client);

    [DllImport("cspomelo", EntryPoint="pc_client_add_ev_handler")]
    private static extern int NativeAddEventHandler(IntPtr client, NativeEventCallback callback, IntPtr ex_data, IntPtr destructor);
    [DllImport("cspomelo", EntryPoint="pc_client_rm_ev_handler")]
    private static extern int NativeRemoveEventHandler(IntPtr client, int handler_id);

    [DllImport("cspomelo", EntryPoint="pc_client_conn_quality")]
    private static extern int NativeQuality(IntPtr client);
    [DllImport("cspomelo", EntryPoint="pc_client_state")]
    private static extern int NativeState(IntPtr client);
#endif
}
