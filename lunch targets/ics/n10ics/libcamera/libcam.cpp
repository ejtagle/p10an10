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


extern camera_module_t HAL_MODULE_INFO_SYM;

namespace android {

static camera_module_t* getcam(void)
{
	/* The original camera library implementation */
	static camera_module_t* orgcam = NULL;

	if (!orgcam) {
		void* handle;
		LOGI("Resolving original camera module...");	
		
		handle = dlopen("/system/lib/libpicasso_camera.so",RTLD_LAZY);
		if(!handle) {
			LOGE("Unable to load original camera module");
			return NULL;
		}

		orgcam = (camera_module_t*) dlsym(handle,"HMI");
		if (!orgcam) {
			LOGE("Unable to resolve HMI");
			dlclose(handle);
			handle = NULL;
			return NULL;
		}
		LOGI("Loaded the original camera module");
	}
	return orgcam;
}
 
static int get_number_of_cameras(void)
{
	LOGD("get_number_of_cameras");
	
	LOGD("Original count of cameras: %d", getcam()->get_number_of_cameras());
	
	/* We support 2 cameras */
    return 2;
}

static int get_camera_info(int camera_id, struct camera_info* info)
{
	int ret;
	LOGD("get_camera_info");
	
	/* Call the original camera information for the secondary camera */
	ret = getcam()->get_camera_info(1,info);
	if (ret != NO_ERROR) {
		LOGE("Unable to get camera information (err:%d)",ret);
		return ret;
	}
	
	LOGI("Facing: %d",info->facing);
	LOGI("Orientation: %d",info->orientation);
	
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
	char namemod[] = {'0',0};
	
	LOGD("device_open: name = %s", name);
	
    /*
     * Simply verify the parameters, and dispatch the call inside the
     * CameraFactory instance.
     */

    if (module != &HAL_MODULE_INFO_SYM.common) {
        LOGE("%s: Invalid module %p expected %p",
             __FUNCTION__, module, &HAL_MODULE_INFO_SYM.common);
        return -EINVAL;
    }
    if (name == NULL) {
        LOGE("%s: NULL name is not expected here", __FUNCTION__);
        return -EINVAL;
    }

	/* We need to call the original camera module for the secondary camera ... */
	namemod[0] = name[0] + 1;
	ret = getcam()->common.methods->open(&getcam()->common,namemod,device);
	if (ret != NO_ERROR) {
		LOGE("Failed to call original camera module (err:%d)", ret);
		return ret;
	}
	
	LOGI("Redirected to the original module");
	
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



