using UnityEngine;
using System.Threading;
using System.Collections;
using UnityThreading;

namespace test 
{
	public class Test : MonoBehaviour
	{
		PomeloClient client;

		[System.Runtime.InteropServices.DllImport("cspomelo", EntryPoint="native_log")]
		private static extern int NLog(string msg);

		public void DLog(object data)
		{
			Debug.Log(data, null);
			NLog (data.ToString() + '\n');
		}

		IEnumerator Start()
		{
			PomeloClient.Log = DLog;
#if UNITY_IPHONE || UNITY_ANDROID
			PomeloClient.LibInit(PomeloClient.PC_LOG_DEBUG, null, Application.temporaryCachePath + "/cspomelo.log");
#else
			PomeloClient.LibInit(PomeloClient.PC_LOG_DEBUG, null, "/Users/hbb/Desktop/cspomelo.log");
#endif

			client = new PomeloClient();
			client.Init(false, false);
			client.AddEventHandler();
			client.Connect("127.0.0.1", 3010);

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
	}

}
