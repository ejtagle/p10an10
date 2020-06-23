/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#ifndef ANDROID_HARDWARE_V4L2CAMERA_PARAMETERS_H
#define ANDROID_HARDWARE_V4L2CAMERA_PARAMETERS_H

#include <camera/CameraParameters.h>

namespace android {

class V4L2CameraParameters: public CameraParameters
{
public:
    V4L2CameraParameters() : CameraParameters() {};
    V4L2CameraParameters(const String8 &params): CameraParameters(params) {};
    ~V4L2CameraParameters();

    //Touch Af/AEC settings.
    static const char KEY_TOUCH_AF_AEC[];
    static const char KEY_SUPPORTED_TOUCH_AF_AEC[];
    //Touch Index for AEC.
    static const char KEY_TOUCH_INDEX_AEC[];
    //Touch Index for AF.
    static const char KEY_TOUCH_INDEX_AF[];

    static const char KEY_ISO_MODE[];
    static const char KEY_SUPPORTED_ISO_MODES[];

    static const char KEY_AUTO_EXPOSURE[];
    static const char KEY_SUPPORTED_AUTO_EXPOSURE[];

    // Values for Touch AF/AEC
    static const char TOUCH_AF_AEC_OFF[] ;
    static const char TOUCH_AF_AEC_ON[] ;
	
    // Normal focus mode. Applications should call
    // CameraHardwareInterface.autoFocus to start the focus in this mode.
    static const char FOCUS_MODE_NORMAL[];
    static const char ISO_AUTO[];
    static const char ISO_HJR[] ;
    static const char ISO_100[];
    static const char ISO_200[] ;
    static const char ISO_400[];
    static const char ISO_800[];
    static const char ISO_1600[];


    static const char KEY_SHARPNESS[];
    static const char KEY_MAX_SHARPNESS[];
    static const char KEY_CONTRAST[];
    static const char KEY_MAX_CONTRAST[];
    static const char KEY_SATURATION[];
    static const char KEY_MAX_SATURATION[];


};

}; // namespace android

#endif
