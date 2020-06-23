/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 

#define LOG_TAG "N10Camera"

#include <utils/Log.h>

#include <string.h>
#include <dlfcn.h>
#include <hardware/hardware.h>
#include <hardware/camera.h> 
#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include <utils/threads.h> 
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/SortedVector.h>
extern "C" {
#include <linux/version.h>
#include <linux/videodev2.h>  
}; 

#define OMX_CAM_STR "1"
#define OMX_CAM			1

extern camera_module_t HAL_MODULE_INFO_SYM;

namespace android {

static camera_module_t* getcam(int idx)
{
	/* The original camera library implementation */
	static camera_module_t* orgcam[2] = {NULL,NULL};

	if (!orgcam[1]) {
		void* handle;
		ALOGI("Resolving original NvOmx camera module...");
		
		handle = dlopen("/system/lib/libpicasso_camera.so",RTLD_LAZY);
		if(!handle) {
			ALOGE("Unable to load original camera module");
			return NULL;
		}

		orgcam[1] = (camera_module_t*) dlsym(handle,"HMI");
		if (!orgcam[1]) {
			ALOGE("Unable to resolve NvOmx HMI");
			dlclose(handle);
			handle = NULL;
			return NULL;
		}
		ALOGI("Loaded the original NvOmx camera module");
	}
	
	if (!orgcam[0]) {
		void* handle;
		ALOGI("Resolving original USB camera module...");
		
		handle = dlopen("/system/lib/libusb_camera.so",RTLD_LAZY);
		if(!handle) {
			ALOGE("Unable to load original camera module");
			return NULL;
		}

		orgcam[0] = (camera_module_t*) dlsym(handle,"HMI");
		if (!orgcam[0]) {
			ALOGE("Unable to resolve USB HMI");
			dlclose(handle);
			handle = NULL;
			return NULL;
		}
		ALOGI("Loaded the original USB camera module");
	}
	
	return orgcam[idx];
}
 
static int get_number_of_cameras(void)
{
	ALOGD("get_number_of_cameras");
	
	ALOGD("Original count of cameras: [0]=%d,[1]=%d", getcam(0)->get_number_of_cameras(),getcam(1)->get_number_of_cameras());
	
	/* We support 2 cameras */
    return 2;
}

static int get_camera_info(int camera_id, struct camera_info* info)
{
	int ret;
	ALOGD("get_camera_info");
	
	if (camera_id < 0 || camera_id > 1) {
		ALOGE("Invalid camera index: %d",camera_id);
		return -EINVAL;
	}
	
	/* Call the original camera information for the secondary camera */
	ret = getcam(camera_id)->get_camera_info(OMX_CAM,info);
	if (ret != NO_ERROR) {
		ALOGE("Unable to get camera information (err:%d)",ret);
		return ret;
	}
	
	ALOGI("[%d] Facing: %d, Orientation: %d",camera_id, info->facing, info->orientation);
	
	/* Adapt it to our camera */
	info->facing = camera_id > 0 ? CAMERA_FACING_BACK : CAMERA_FACING_FRONT;
	info->orientation = 0;
	
    return NO_ERROR;
}

static int device_open(const hw_module_t* module,
                                       const char* name,
                                       hw_device_t** device)
{
	int ret;
	int camera_id = name ? (name[0] - '0') : -1;
	
	ALOGD("device_open: name = %s", name);

	if (camera_id < 0 || camera_id > 1) {
		ALOGE("Invalid camera index: %d",camera_id);
		return -EINVAL;
	}

    /*
     * Simply verify the parameters, and dispatch the call inside the
     * CameraFactory instance.
     */

    if (module != &HAL_MODULE_INFO_SYM.common) {
        ALOGE("%s: Invalid module %p expected %p",
             __FUNCTION__, module, &HAL_MODULE_INFO_SYM.common);
        return -EINVAL;
    }
    if (name == NULL) {
        ALOGE("%s: NULL name is not expected here", __FUNCTION__);
        return -EINVAL;
    }

	/* We need to call the original camera module... */
	ret = getcam(camera_id)->common.methods->open(&getcam(camera_id)->common,OMX_CAM_STR,device);
	if (ret != NO_ERROR) {
		ALOGE("Failed to call original camera module (err:%d)", ret);
		return ret;
	}
	
	ALOGI("Redirected to the original module");
	
    return ret;
} 

/********************************************************************************
 * Initializer for the static member structure.
 *******************************************************************************/

/* Entry point for camera HAL API. */
static struct hw_module_methods_t CameraModuleMethods = {
    open: device_open
};  

}; 

/*
 * Required HAL header.
 */
camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag:           HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id:            CAMERA_HARDWARE_MODULE_ID,
         name:          "N10 Camera Module",
         author:        "The Android Open Source Project",
         methods:       &android::CameraModuleMethods,
         dso:           NULL,
         reserved:      {0},
    },
    get_number_of_cameras:  android::get_number_of_cameras,
    get_camera_info:        android::get_camera_info,
}; 



