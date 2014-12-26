LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := pomelo_static

LOCAL_MODULE_FILENAME := libpomelo

LOCAL_SRC_FILES := \
src/pc_lib.c \
src/pc_pomelo.c \
src/pc_trans.c \
src/pc_trans_repo.c \
src/tr/dummy/tr_dummy.c \
src/tr/uv/pb_decode.c \
src/tr/uv/pb_encode.c \
src/tr/uv/pb_util.c \
src/tr/uv/pr_msg.c \
src/tr/uv/pr_msg_json.c \
src/tr/uv/pr_msg_pb.c \
src/tr/uv/pr_pkg.c \
src/tr/uv/tr_uv_tcp.c \
src/tr/uv/tr_uv_tcp_aux.c \
src/tr/uv/tr_uv_tcp_i.c


LOCAL_CFLAGS := -DPC_NO_UV_TLS_TRANS



LOCAL_EXPORT_C_INCLUDES :=$(LOCAL_PATH)/include



LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/src \
					$(LOCAL_PATH)/src/tr/dummy \
					$(LOCAL_PATH)/src/tr/uv

LOCAL_WHOLE_STATIC_LIBRARIES := uv_static jansson_static



include $(BUILD_STATIC_LIBRARY)

LOCAL_CFLAGS    := -D__ANDROID__ 

$(call import-module,libpomelo2/deps/uv) \
$(call import-module,libpomelo2/deps/jansson)
