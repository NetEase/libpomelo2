#include <pomelo.h>
#include <pc_pomelo_i.h>
#include <assert.h>



#ifdef _WIN32
#define CS_POMELO_EXPORT __declspec(dllexport)
#else
#define CS_POMELO_EXPORT
#endif


/////////
#include <pc_lib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


#if defined(__ANDROID__)

#include <android/log.h>

#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "cspomelo", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG ,"cspomelo", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO  ,"cspomelo", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN  ,"cspomelo", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR ,"cspomelo", __VA_ARGS__)

void android_log(int level, const char* msg, ...)
{
	time_t t = time(NULL);
	char buf[512];
	va_list va;
	int n = 0;
	
	if (level < 0) {
		return;
	}
	
	strftime(buf, 32, "[%Y-%m-%d %H:%M:%S]", localtime(&t));
	n = strlen(buf);
	
	va_start(va, msg);
	vsprintf(&buf[n], msg, va);
	va_end(va);
	
	n = strlen(buf);
	buf[n] = '\n';
	buf[n+1] = '\0';
	
	switch(level) {
		case PC_LOG_DEBUG:
			LOGD("%s", buf);
			break;
		case PC_LOG_INFO:
			LOGI("%s", buf);
			break;
		case PC_LOG_WARN:
			LOGW("%s", buf);
			break;
		case PC_LOG_ERROR:
			LOGE("%s", buf);
			break;
		default:
			LOGV("%s", buf);
	}
}

#elif defined(__UNITYEDITOR__)


void unity_log(int level, const char* msg, ...)
{
	time_t t = time(NULL);
	char buf[512];
	va_list va;
	int n = 0;
	
	if (level < 0) {
		return;
	}
	
	strftime(buf, 32, "[%Y-%m-%d %H:%M:%S]", localtime(&t));
	n = strlen(buf);
	switch(level) {
		case PC_LOG_DEBUG:
			n += sprintf(&buf[n], "[DEBUG] ");
			break;
		case PC_LOG_INFO:
			n += sprintf(&buf[n], "[INFO] ");
			break;
		case PC_LOG_WARN:
			n += sprintf(&buf[n], "[WARN] ");
			break;
		case PC_LOG_ERROR:
			n += sprintf(&buf[n], "[ERROR] ");
			break;
	}
	
	va_start(va, msg);
	vsprintf(&buf[n], msg, va);
	va_end(va);
	
	n = strlen(buf);
	buf[n] = '\n';
	buf[n+1] = '\0';
	
	FILE* f = fopen("/tmp/cspomelo.log", "a");
	assert(f);
	fwrite(buf, sizeof(char), strlen(buf), f);
	fflush(f);
	fclose(f);
}
#endif

CS_POMELO_EXPORT void native_log(const char* msg)
{
	if (!msg || strlen(msg) == 0) {
		return;
	}
	pc_lib_log(PC_LOG_DEBUG, msg);
}

/////////


typedef void (*request_callback)(const char* route, int rc, const char* msg);


typedef struct
{
	char* (* read) ();
	int   (* write)(char* data);
} lc_callback;


static int local_storage_cb(pc_local_storage_op_t op, char* data, size_t* len, void* ex_data)
{
	lc_callback* lc_cb = (lc_callback* )ex_data;
	char* res = NULL;
    
	if (op == PC_LOCAL_STORAGE_OP_WRITE) {
		return lc_cb->write(data);
	} else {
		res = lc_cb->read();
		if (!res) {
			return -1;
		}
		
		*len = strlen(res);
		if (*len == 0) {
			return -1;
		}

		if (data) {
			strcpy(data, res);
		}
		return 0;
	}
	// never go to here
	return -1;
}

static void default_request_cb(const pc_request_t* req, int rc, const char* resp)
{
	request_callback cb = (request_callback)pc_request_ex_data(req);
	cb(req->base.route, rc, resp);
}


CS_POMELO_EXPORT void lib_init(int log_level, const char* ca_file, const char* ca_path)
{
#if !defined(PC_NO_UV_TLS_TRANS)
	if (ca_file || ca_path) {
		tr_uv_tls_set_ca_file(ca_file, ca_path);
	}
#endif
	
	pc_lib_set_default_log_level(log_level);
#if defined(__ANDROID__)
	pc_lib_init(android_log, NULL, NULL, "CSharp Client");
#elif defined(__UNITYEDITOR__)
	pc_lib_init(unity_log, NULL, NULL, "CSharp Client");
#else
	pc_lib_init(NULL, NULL, NULL, "CSharp Client");
#endif

}

CS_POMELO_EXPORT pc_client_t* create(int enable_tls, int enable_poll, char* (*read)(), int (*write)(char*))
{
	pc_client_t* client = NULL;
	pc_client_config_t config = PC_CLIENT_CONFIG_DEFAULT;

	if (enable_tls) {
		config.transport_name = PC_TR_NAME_UV_TLS;
	}
	if (enable_poll) {
		config.enable_polling = 1;
	}

	lc_callback* lc_cb= (lc_callback*)malloc(sizeof(lc_callback));
	if (!lc_cb) {
		return NULL;
	}
	lc_cb->read = read;
	lc_cb->write = write;
	config.ls_ex_data = lc_cb;
	config.local_storage_cb = local_storage_cb;

	client = (pc_client_t *)malloc(pc_client_size());
	if (pc_client_init(client, NULL, &config) == PC_RC_OK) {
		return client;
	}
	
	return NULL;
}

CS_POMELO_EXPORT void destroy(pc_client_t* client)
{
	lc_callback* lc_cb;
	
	if (pc_client_cleanup(client) == PC_RC_OK) {
		lc_cb = (lc_callback*)pc_client_config(client)->ls_ex_data;
		if (lc_cb) {
			free(lc_cb);
		}
		free(client);
	}
}

CS_POMELO_EXPORT int request(pc_client_t* client, const char* route, const char* msg, void* ex_data, int timeout, request_callback cb)
{
	(void)ex_data; // unused
	return pc_request_with_timeout(client, route, msg, cb, timeout, default_request_cb);
}

