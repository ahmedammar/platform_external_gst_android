LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	surfaceflinger_wrap.cpp \
	gstsurfaceflingersink.c  
	
LOCAL_SHARED_LIBRARIES := \
	libgstreamer-0.10       \
	libglib-2.0             \
	libgthread-2.0          \
	libgmodule-2.0          \
	libgobject-2.0          \
	libgstbase-0.10         \
	libgstvideo-0.10	\
	libcutils               \
	libutils                \
	libui			\
	libsurfaceflinger 	\
	libsurfaceflinger_client \
	libbinder \
	libgstbufmeta \
	libgui	\
	libipu \
	libEGL

LOCAL_MODULE:= libgstsurfaceflinger


LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)  		\
	$(LOCAL_PATH)/../../ 	\
	external/gstreamer_aggregate/gst-plugins-fsl/libs/gstbufmeta \
	external/gstreamer_aggregate/gst-plugins-fsl/inc/plugin \
	external/linux-lib/ipu \
	frameworks/base/include \
	hardware/imx/mx5x/libgralloc

LOCAL_CFLAGS := -DANDROID_USE_GSTREAMER \
	-DHAVE_CONFIG_H \
	$(shell $(PKG_CONFIG) gstreamer-video-0.10 --cflags)

LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/gstreamer-0.10
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := eng debug

include $(BUILD_SHARED_LIBRARY)
