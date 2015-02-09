#include <pomelo.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <pc_lib.h>

#ifdef _WIN32
#define CS_POMELO_EXPORT __declspec(dllexport)
#else
#define CS_POMELO_EXPORT
#endif

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


static FILE* f = NULL;
static void unity_log(int level, const char* msg, ...)
{
	char buf[256];
	va_list va;
	int n = 0;

	if (level < 0) {
		return;
	}

	switch(level) {
	case PC_LOG_DEBUG:
		n = sprintf(buf, "[DEBUG] ");
		break;
	case PC_LOG_INFO:
		n = sprintf(buf, "[INFO] ");
		break;
	case PC_LOG_WARN:
		n = sprintf(buf, "[WARN] ");
		break;
	case PC_LOG_ERROR:
		n = sprintf(buf, "[ERROR] ");
		break;
	}

	va_start(va, msg);
	vsprintf(&buf[n], msg, va);
	va_end(va);
	
	n = strlen(buf);
	buf[n] = '\n';
	buf[n+1] = '\0';

	fwrite(buf, sizeof(char), n+1, f);
	fflush(f);
}

CS_POMELO_EXPORT void native_log(const char* msg)
{
	if (!msg || strlen(msg) == 0) {
		return;
	}
	fwrite(msg, sizeof(char), strlen(msg), f);
	fflush(f);
}

CS_POMELO_EXPORT void lib_init(int log_level, const char* ca_file, const char* ca_path)
{
#if !defined(PC_NO_UV_TLS_TRANS)
	if (ca_file || ca_path) {
		tr_uv_tls_set_ca_file(ca_file, ca_path);
	}
#endif
	
	f = fopen(ca_path, "w");
	assert(f);
	pc_lib_set_default_log_level(log_level);
    pc_lib_init(unity_log, NULL, NULL, "CSharp Client");
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
		pc_lib_log(PC_LOG_DEBUG, "create client success : %p", client);
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
	
	pc_lib_log(PC_LOG_DEBUG, "DONE destroy client in unmanaged code");
}
