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

#define LOG_TAG "QCameraParams"
#include <utils/Log.h>
#include <string.h>
#include <stdlib.h>
#include "V4L2CameraParameters.h"

namespace android {
// Parameter keys to communicate between camera application and driver.
const char V4L2CameraParameters::KEY_TOUCH_AF_AEC[] = "touch-af-aec";
const char V4L2CameraParameters::KEY_SUPPORTED_TOUCH_AF_AEC[] = "touch-af-aec-values";
const char V4L2CameraParameters::KEY_TOUCH_INDEX_AEC[] = "touch-index-aec";
const char V4L2CameraParameters::KEY_TOUCH_INDEX_AF[] = "touch-index-af";
const char V4L2CameraParameters::KEY_ISO_MODE[] = "iso";
const char V4L2CameraParameters::KEY_SUPPORTED_ISO_MODES[] = "iso-values";
const char V4L2CameraParameters::KEY_AUTO_EXPOSURE[] = "auto-exposure";
const char V4L2CameraParameters::KEY_SUPPORTED_AUTO_EXPOSURE[] = "auto-exposure-values";

// Values for auto exposure settings.
const char V4L2CameraParameters::TOUCH_AF_AEC_OFF[] = "touch-off";
const char V4L2CameraParameters::TOUCH_AF_AEC_ON[] = "touch-on";

// Values for focus mode settings.
const char V4L2CameraParameters::FOCUS_MODE_NORMAL[] = "normal";

// Values for ISO Settings
const char V4L2CameraParameters::ISO_AUTO[] = "auto";
const char V4L2CameraParameters::ISO_HJR[] = "ISO_HJR";
const char V4L2CameraParameters::ISO_100[] = "ISO100";
const char V4L2CameraParameters::ISO_200[] = "ISO200";
const char V4L2CameraParameters::ISO_400[] = "ISO400";
const char V4L2CameraParameters::ISO_800[] = "ISO800";
const char V4L2CameraParameters::ISO_1600[] = "ISO1600";

 //Values for Lens Shading
const char V4L2CameraParameters::LENSSHADE_ENABLE[] = "enable";
const char V4L2CameraParameters::LENSSHADE_DISABLE[] = "disable";

const char V4L2CameraParameters::KEY_SHARPNESS[] = "sharpness";
const char V4L2CameraParameters::KEY_MAX_SHARPNESS[] = "max-sharpness";
const char V4L2CameraParameters::KEY_CONTRAST[] = "contrast";
const char V4L2CameraParameters::KEY_MAX_CONTRAST[] = "max-contrast";
const char V4L2CameraParameters::KEY_SATURATION[] = "saturation";
const char V4L2CameraParameters::KEY_MAX_SATURATION[] = "max-saturation";


V4L2CameraParameters::~V4L2CameraParameters()
{
}

}; // namespace android

