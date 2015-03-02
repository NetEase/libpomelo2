/**
 * Copyright (c) 2014-2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

using UnityEngine;
using System.Collections;

namespace test
{
    public class Test : MonoBehaviour
    {
        PomeloClient client;

#if UNITY_IPHONE || UNITY_XBOX360
        [System.Runtime.InteropServices.DllImport("__Internal", EntryPoint="native_log")]
        private static extern int NLog(string msg);
        [System.Runtime.InteropServices.DllImport("__Internal", EntryPoint="init_log")]
        private static extern int InitLog(string path);
#else
        [System.Runtime.InteropServices.DllImport("cspomelo", EntryPoint="native_log")]
        private static extern int NLog(string msg);
        [System.Runtime.InteropServices.DllImport("cspomelo", EntryPoint="init_log")]
        private static extern int InitLog(string path);
#endif

        System.Text.StringBuilder log = new System.Text.StringBuilder("App Start\n\n\n");

        public void DLog(object data)
        {
            log.AppendLine(data.ToString());
            Debug.Log(data, null);
            NLog (data.ToString());
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

        void Start()
        {
            const string logFile= "/tmp/cspomelo.log";
            if(InitLog(logFile) != 0)
            {
                DLog("error open log file : " + logFile);
            }

            PomeloClient.Log = DLog;
            PomeloClient.LibInit(PomeloClient.PC_LOG_DEBUG, null, null);

            client = new PomeloClient();
            if(! client.Init(false, false))
            {
                DLog("client init has occur a fatal error");
                return;
            }
            DLog("client inited");

            client.OnConnectSuccess += OnConnect;
            client.OnConnectSuccess += ()=>{
                DLog("client connected");
            };
            client.OnConnectFail += (msg)=>{
                DLog("client connect fail : " + msg);
            };
            client.OnDisconnect += OnDisconnect;
            client.OnDisconnect += (msg)=>{
                DLog("client disconnected : " + msg);
            };
            client.OnError += (err)=>{
                DLog("client occur an error : " + err);
            };
            client.Connect(ServerHost(), ServerPort());
        }

        void OnConnect()
        {
            client.OnConnectSuccess -= OnConnect;

            StartCoroutine(KeepTestClient());
        }

        void OnDisconnect(string msg)
        {
            DLog("client prepare to reconnect");
            client.OnConnectSuccess += OnConnect;
            client.Connect(ServerHost(), ServerPort());
        }

        IEnumerator KeepTestClient()
        {
            int i = 0;
            while(++i < 3)
            {
                DLog("...keep request");
                client.Request("connector.entryHandler.entry", @"{""name"":""unity"", ""say"":""i'm request""}",  (err, res)=>{
                    DLog(string.Format("Request - connector.entryHandler.entry - err={0},res={1}",err,res));
                });

                client.Notify("connector.entryHandler.entry", @"{""name"":""unity"", ""say"":""i'm notify""}");
                yield return new WaitForSeconds(5);
            }

            client.Disconnect();
            DLog("client query disconnect");
        }

        string ServerHost()
        {
#if UNITY_EDITOR
            string host = "127.0.0.1";
#else
            string host = "192.168.0.103";
#endif
            DLog("server host : " + host);
            return host;
        }

        int ServerPort()
        {
            return 3010;
        }

        void Update()
        {
            var txt = GetComponentInChildren<UnityEngine.UI.Text>();
            txt.text = log.ToString();
        }
    }

}
