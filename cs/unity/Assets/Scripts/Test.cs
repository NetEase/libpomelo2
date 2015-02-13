using UnityEngine;
using System.Collections;

namespace test 
{
	public class Test : MonoBehaviour
	{
		PomeloClient client;

		[System.Runtime.InteropServices.DllImport("cspomelo", EntryPoint="native_log")]
		private static extern int NLog(string msg);

		System.Text.StringBuilder log = new System.Text.StringBuilder("App Start\n\n\n");

		public void DLog(object data)
		{
			log.AppendLine(data.ToString());
			Debug.Log(data, null);
			NLog (data.ToString() + '\n');
		}

		IEnumerator Start()
		{
#if UNITY_EDITOR
			string host = "127.0.0.1";
#else
			string host = "10.0.2.2";
			var syshost = System.Net.Dns.GetHostEntry(System.Net.Dns.GetHostName());
			foreach(var ip in syshost.AddressList)
			{
				if(ip.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
				{
					host = ip.ToString();
					break;
				}
			}
#endif

			PomeloClient.Log = DLog;
			PomeloClient.LibInit(PomeloClient.PC_LOG_DEBUG, null, null);

			client = new PomeloClient();
			client.Init(false, false);
			client.AddEventHandler();
			client.Connect(host, 3010);

			DLog("Wait 2 second");
			yield return new WaitForSeconds(2);

			client.Request("connector.entryHandler.entry", @"{""name"":""unity"", ""say"":""i'm request""}",  (res)=>{});
			client.Notify("connector.entryHandler.entry", @"{""name"":""unity"", ""say"":""i'm notify""}");

			DLog("Wait 5 seconds");
			yield return new WaitForSeconds(5);
			
			DLog(string.Format("done!"));
			
			while(true)
			{
				DLog("...keep request");
				client.Request("connector.entryHandler.entry", @"{""name"":""unity"", ""say"":""i'm request""}",  (res)=>{});
				yield return new WaitForSeconds(5);
			}
		}

		/// <summary>
		/// cleanup the pomelo client
		/// NOTE it's not the best practice since it would not invoke on ios
		/// </summary>
		/// 
		void OnApplicationQuit()
		{
			if(client != null)
			{
				DLog("client ready to destroy");
				client.Destroy();
				client = null;
				PomeloClient.LibCleanup();
				DLog("destroy client successful");
			}
			else
			{
				DLog("no client to destroy!");
			}
		}

		void Update()
		{
			var txt = GetComponentInChildren<UnityEngine.UI.Text>();
			txt.text = log.ToString();
		}
	}

}
