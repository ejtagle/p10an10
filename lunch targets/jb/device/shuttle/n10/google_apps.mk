# Copyright (C) 2011 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := device/shuttle/n10

# Copy Google Apps
PRODUCT_COPY_FILES += \
	$(call find-copy-subdir-files,*,device/shuttle/n10/gapps/system/lib,system/lib) \
	$(call find-copy-subdir-files,*,device/shuttle/n10/gapps/system/etc,system/etc) \
	$(call find-copy-subdir-files,*,device/shuttle/n10/gapps/system/framework,system/framework) \
	$(call find-copy-subdir-files,*,device/shuttle/n10/gapps/system/media,system/media)	

PRODUCT_PACKAGES += \
	CalendarGoogle \
	ChromeBookmarksSyncAdapter \
	ConfigUpdater \
	GalleryGoogle \
	Gmail2 \
    GmsCore \
	GoogleBackupTransport \
	GoogleContactsSyncAdapter \
    GoogleEars \
	GoogleFeedback \
	GoogleLoginService \
	GooglePartnerSetup \
	GoogleServicesFramework \
	GoogleTTS \
	LatinImeGoogle \
	MediaUploader \
	Music2 \
	NetworkLocation \
	OneTimeInitializer \
	Phonesky \
	SetupWizard \
	Talk \
    Velvet \
	VoiceSearchStub 
