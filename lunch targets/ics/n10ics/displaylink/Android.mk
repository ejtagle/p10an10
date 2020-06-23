# Copyright 2005 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	fbmirror.c 

LOCAL_MODULE := fbmirror
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libc

include $(BUILD_EXECUTABLE)

