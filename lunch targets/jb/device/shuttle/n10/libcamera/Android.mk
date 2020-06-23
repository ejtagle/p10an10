# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= camera.n10
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SRC_FILES := libcam.cpp
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libdl libcamera_client 
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= libusb_camera
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_CFLAGS:=-fno-short-enums -DHAVE_CONFIG_H 
LOCAL_C_INCLUDES += external/jpeg
LOCAL_SRC_FILES:= \
	CameraFactory.cpp \
	CameraHal.cpp \
	CameraHardware.cpp \
	Converter.cpp \
	Utils.cpp \
	V4L2Camera.cpp \
	V4L2CameraParameters.cpp \
	SurfaceDesc.cpp \
	SurfaceSize.cpp 

LOCAL_SHARED_LIBRARIES:= libutils libbinder libui liblog libcamera_client libcutils libmedia libandroid_runtime libhardware_legacy libc libstdc++ libm libjpeg libandroid
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY) 