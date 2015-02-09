using UnityEngine;
using System;
using System.Runtime.InteropServices;
using UnityThreading;

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

	//__________________________________________________________________________________________PINVOKE_CALLBACKS

	private delegate string NativeLCRCallback();
	private delegate int NativeLCWCallback(IntPtr data_str);
	private delegate void NativeRequestCallback(IntPtr req, int rc, IntPtr res_str);
	private delegate void NativeNotifyCallback(IntPtr req, int rc);
	private delegate void NativeEventCallback(IntPtr client, int ev, IntPtr ex_data, IntPtr arg1_str, IntPtr arg2_str);

	public delegate void Respond(string res);

	//__________________________________________________________________________________________DEFAULT_CALLBACKS

	private void OnRequest(IntPtr req, int rc, IntPtr res_str)
	{
		Log(string.Format("OnRequest - pinvoke callback START | rc={0}, &res={1}", RcToStr(rc), (ulong)res_str));
		var res = Marshal.PtrToStringAuto(res_str);
		UnityThreadHelper.Dispatcher.Dispatch(()=>{
			Log(string.Format("OnRequest - main thread START | rc={0}, res={1}", RcToStr(rc), res));
			// TODO
			Log(string.Format("OnRequest - main thread END"));
		});
		Log(string.Format("OnRequest - pinvoke callback END"));
	}
	private void OnNotify(IntPtr req, int rc)
	{
		Log(string.Format("OnNotify - pinvoke callback START | rc={0}", RcToStr(rc)));
		UnityThreadHelper.Dispatcher.Dispatch(()=>{
			Log(string.Format("OnNotify - main thread START | rc={0}", RcToStr(rc)));
			// TODO
			Log(string.Format("OnNotify - main thread END"));
		});
		Log(string.Format("OnNotify - pinvoke callback END"));
	}
	private void OnEvent(IntPtr client, int ev, IntPtr ex_data, IntPtr arg1_str, IntPtr arg2_str)
	{
		Log(string.Format("OnEvent - pinvoke callback START | ev={0}, &arg1={1}, &arg2={2}", EvToStr(ev), (ulong)arg1_str, (ulong)arg2_str));
		var arg1 = Marshal.PtrToStringAuto(arg1_str);
		var arg2 = Marshal.PtrToStringAuto(arg2_str);
		UnityThreadHelper.Dispatcher.Dispatch(()=>{
			Log(string.Format("OnEvent - main thread START | ev={0}, arg1={1}, arg2={2}", EvToStr(ev), arg1, arg2));
			// TODO
			Log(string.Format("OnEvent - main thread END"));
		});
		Log(string.Format("OnEvent - pinvoke callback END"));
	}

	private string OnLCR()
	{
		Log("OnLCR - pinvoke START");
		var data = lcReader();
		Log("OnLCR - pinvoke END");
		return data;
	}
	private int OnLCW(IntPtr data_str)
	{
		Log("OnLCW- pinvoke START | &data=" + (ulong)data_str);
		var data = Marshal.PtrToStringAuto(data_str);
		UnityThreadHelper.Dispatcher.Dispatch(()=>{
			Log("OnLCW- main thread START | data=" + data);
			lcWriter(data);
			Log("OnLCW- main thread END");
		});
		Log("OnLCW - pinvoke END");
		return 0;
	}
	
	private static string DefaultLocalConfigReader()
	{
		Log("DefaultLocalConfigReader - no config to read");
		return null;
	}
	private static void DefaultLocalConfigWriter(string data)
	{
		Log("DefaultLocalConfigWriter - do not save config : " + data);
	}

	//__________________________________________________________________________________________USER_FUNCTIONS
	
	private IntPtr client = IntPtr.Zero;

	private readonly NativeLCRCallback nativeLCRCallback;
	private readonly NativeLCWCallback nativeLCWCallback;
	private readonly NativeRequestCallback nativeRequestCallback;
	private readonly NativeNotifyCallback nativeNotifyCallback;
	private readonly NativeEventCallback nativeEventCallback;

	private Func<string> lcReader;
	private Action<string> lcWriter;

	public PomeloClient()
	{
		nativeLCRCallback = OnLCR;
		nativeLCWCallback = OnLCW;
		nativeRequestCallback = OnRequest;
		nativeNotifyCallback = OnNotify;
		nativeEventCallback = OnEvent;
	}
	
	public bool Init(bool enableTLS, bool enablePolling)
	{
		return Init(enableTLS, enablePolling, DefaultLocalConfigReader, DefaultLocalConfigWriter);
	}
	public bool Init(bool enableTLS, bool enablePolling, Func<string> reader, Action<string> writer)
	{
		lcReader = reader ?? DefaultLocalConfigReader;
		lcWriter = writer ?? DefaultLocalConfigWriter;

		client = NativeCreate(enableTLS, enablePolling, nativeLCRCallback, nativeLCWCallback);

		if(client != IntPtr.Zero)
			return true;

		Log("Init - create client failed");
		return false;
	}
	public void Destroy()
	{
		Log(string.Format("Destroy - main thread START"));
		NativeDestroy(client);
		client = IntPtr.Zero;
		Log(string.Format("Destroy - main thread END"));
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
	public void Request(string route, string msg, Action<string> respond)
	{
		Request(route, msg, 10, respond);
	}
	public void Request(string route, string msg, int timeout, Action<string> respond)
	{
		CheckClient();
		NativeRequest(client, route, msg, IntPtr.Zero, timeout, nativeRequestCallback);
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
		// TODO
		throw new NotImplementedException();
	}
	public void AddEventHandler(/*TODO*/)
	{
		CheckClient();
		NativeAddEventHandler(client, nativeEventCallback, IntPtr.Zero, IntPtr.Zero);
	}
	public void RemoveEventHandler(int handlerId)
	{
		CheckClient();
		NativeRemoveEventHandler(client, handlerId);
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

	//__________________________________________________________________________________________PINVOKE_FUNCTIONS

#if UNITY_IPHONE || UNITY_XBOX360
	// TODO
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
	
	[DllImport("cspomelo", EntryPoint="pc_request_with_timeout")]
	private static extern int NativeRequest(IntPtr client, string route, string msg, IntPtr ex_data, int timeout, NativeRequestCallback callback);
	[DllImport("cspomelo", EntryPoint="pc_notify_with_timeout")]
	private static extern int NativeNotify(IntPtr client, string route, string msg, IntPtr ex_data, int timeout, NativeNotifyCallback callback);
	//	[DllImport("cspomelo", EntryPoint="poll")]
	//	private static extern int NativePoll(int client);
	
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