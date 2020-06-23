#
# Copyright (C) 2011 The Android Open-Source Project
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
#

# This file includes all definitions that apply to ALL n10 devices, and
# are also specific to n10 devices
#
# Everything in this directory will become public

LOCAL_PATH := device/shuttle/n10

LOCAL_KERNEL := $(LOCAL_PATH)/kernel/zImage

DEVICE_PACKAGE_OVERLAYS := $(LOCAL_PATH)/overlay 

# uses mdpi artwork where available
PRODUCT_AAPT_CONFIG := normal mdpi
PRODUCT_AAPT_PREF_CONFIG := mdpi
PRODUCT_LOCALES += mdpi

# These are the hardware-specific feature permissions
PRODUCT_COPY_FILES += \
    frameworks/base/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/base/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
    frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/base/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/base/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/base/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
	frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/base/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	frameworks/base/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/base/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml \
    frameworks/base/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
    frameworks/base/data/etc/android.hardware.location.xml:system/etc/permissions/android.hardware.location.xml
    
#	frameworks/base/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml
#	frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml

	
# Keychars
# Keylayout
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/keychars/zinitix-kbd.kcm:system/usr/keychars/zinitix-kbd.kcm \
    $(LOCAL_PATH)/keychars/gpio-keys.kcm:system/usr/keychars/gpio-keys.kcm \
    $(LOCAL_PATH)/keylayout/zinitix-kbd.kl:system/usr/keylayout/zinitix-kbd.kl \
    $(LOCAL_PATH)/keylayout/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl 

# Vold
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/files/vold.fstab:system/etc/vold.fstab

# N10 Configs
PRODUCT_COPY_FILES += \
    $(LOCAL_KERNEL):kernel \
    $(LOCAL_PATH)/files/init.n10.rc:root/init.n10.rc \
    $(LOCAL_PATH)/files/init.n10.usb.rc:root/init.n10.usb.rc \
    $(LOCAL_PATH)/files/ueventd.n10.rc:root/ueventd.n10.rc \
    $(LOCAL_PATH)/files/initlogo.rle:root/initlogo.rle

# Backlight
PRODUCT_PACKAGES := \
	lights.n10

# Accelerometer (uses NVidia ventana binary blobs)
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/sensors/libakmd.so:system/lib/libakmd.so \
	$(LOCAL_PATH)/sensors/libinvensense_hal.so:system/lib/libinvensense_hal.so \
	$(LOCAL_PATH)/sensors/libmllite.so:system/lib/libmllite.so \
	$(LOCAL_PATH)/sensors/libmlplatform.so:system/lib/libmlplatform.so \
	$(LOCAL_PATH)/sensors/libmplmpu.so:system/lib/libmplmpu.so \
	$(LOCAL_PATH)/sensors/libsensors.mpl.so:system/lib/libsensors.mpl.so \
	$(LOCAL_PATH)/sensors/libsensors.base.so:system/lib/libsensors.base.so \
	$(LOCAL_PATH)/sensors/sensors.n10.so:system/lib/hw/sensors.n10.so
PRODUCT_PACKAGES += \
	libsensors.isl29018 

# Camera
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/libcamera/libpicasso_camera.so:system/lib/libpicasso_camera.so \
	$(LOCAL_PATH)/libcamera/nvcamera.conf:system/etc/nvcamera.conf
PRODUCT_PACKAGES += \
	camera.n10 
ADDITIONAL_DEFAULT_PROPERTIES += \
	persist.tegra.nvlog.level=4 \
	persist.tegra.nvlog.14.level=5 \
	enableLogs=2147483647 
	
# GPS
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/gps/M4.2-X205A1-51082.51144.img:system/etc/firmware/M4.2-X205A1-51082.51144.img \
	$(LOCAL_PATH)/gps/Orion.ini:system/bin/Orion.ini \
	$(LOCAL_PATH)/gps/OrionSys.so:system/lib/OrionSys.so \
	$(LOCAL_PATH)/gps/ingsvcd:system/bin/ingsvcd \
	$(LOCAL_PATH)/gps/resetgps.sh:system/bin/resetgps.sh \
	$(LOCAL_PATH)/gps/libOrionCtl.so:system/lib/libOrionCtl.so
PRODUCT_PACKAGES += \
	gps.n10 

# DisplayLink
PRODUCT_PACKAGES += \
	fbmirror
	
# Audio
PRODUCT_PACKAGES += \
	audio.primary.n10 \
	audio.a2dp.default \
	libaudioutils 
	
# Touchscreen
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/files/zinitix.idc:system/usr/idc/zinitix.idc 

# Graphics
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/files/media_profiles.xml:system/etc/media_profiles.xml

# Huawei 3G modem propietary files and PPP scripts
#PRODUCT_PACKAGES += \
#	libhuaweigeneric-ril
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/files/libhuawei-ril.so:system/lib/libhuawei-ril.so \
	$(LOCAL_PATH)/files/etc/init.gprs-pppd:system/etc/init.gprs-pppd \
	$(LOCAL_PATH)/files/etc/ppp/chap-secrets:system/etc/ppp/chap-secrets \
	$(LOCAL_PATH)/files/etc/ppp/gprs-connect-chat:system/etc/ppp/gprs-connect-chat \
	$(LOCAL_PATH)/files/etc/ppp/ip-down:system/etc/ppp/ip-down \
	$(LOCAL_PATH)/files/etc/ppp/ip-down-HUAWEI:system/etc/ppp/ip-down-HUAWEI \
	$(LOCAL_PATH)/files/etc/ppp/ip-up:system/etc/ppp/ip-up \
	$(LOCAL_PATH)/files/etc/ppp/ip-up-HUAWEI:system/etc/ppp/ip-up-HUAWEI \
	$(LOCAL_PATH)/files/etc/ppp/options.huawei:system/etc/ppp/options.huawei \
	$(LOCAL_PATH)/files/etc/ppp/pap-secrets:system/etc/ppp/pap-secrets \
	$(LOCAL_PATH)/files/etc/ppp/peers/pppd-ril.options:system/etc/ppp/peers/pppd-ril.options

PRODUCT_PROPERTY_OVERRIDES := \
    keyguard.no_require_sim=true

# Tun
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/kernel/tun.ko:system/lib/modules/tun.ko

# Generic
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/files/su:system/xbin/su \
	$(LOCAL_PATH)/files/su:system/bin/su \
	$(LOCAL_PATH)/files/busybox:system/bin/busybox \
	$(LOCAL_PATH)/files/n10_postboot.sh:system/etc/n10_postboot.sh \
	$(LOCAL_PATH)/files/setrecovery:system/bin/setrecovery \
	$(LOCAL_PATH)/files/recovery:system/bin/recovery 
   
# APNs list
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/files/apns-conf.xml:system/etc/apns-conf.xml 
 
PRODUCT_PACKAGES += \
	n10_hdcp_keys

# NVidia binary blobs
$(call inherit-product, device/shuttle/n10/nvidia_3_1.mk)
 
# Modules
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/kernel/scsi_wait_scan.ko:system/lib/modules/scsi_wait_scan.ko 

# Bluetooth configuration files
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/files/main.conf:system/etc/bluetooth/main.conf \
   $(LOCAL_PATH)/kernel/bcm4329.hcd:system/etc/firmware/bcm4329.hcd
	
# Wifi

PRODUCT_PROPERTY_OVERRIDES := \
	net.dns1=8.8.8.8 \
	net.dns2=8.8.4.4

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/files/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf

PRODUCT_PROPERTY_OVERRIDES := \
    wifi.interface=wlan0 \
    wifi.supplicant_scan_interval=15

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/kernel/bcmdhd.cal:system/etc/wifi/bcmdhd.cal \
    $(LOCAL_PATH)/kernel/nvram.txt:system/etc/wifi/nvram.txt \
	$(LOCAL_PATH)/kernel/bcmdhd.ko:system/lib/modules/bcmdhd.ko
#fw is already copied by AOSP
	
#USB

PRODUCT_PACKAGES += \
	com.android.future.usb.accessory 

	# Set default USB interface. By default we enable adb, to be able to debug the boot process
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mtp

# NFC
PRODUCT_PACKAGES += \
	libnfc \
	libnfc_jni \
	Nfc \
	Tag

# Live Wallpapers
PRODUCT_PACKAGES += \
	HoloSpiralWallpaper \
	LiveWallpapers \
	LiveWallpapersPicker \
	MagicSmokeWallpapers \
	VisualizationWallpapers
	
# Input device calibration files
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/zinitix.idc:system/usr/idc/zinitix.idc 

PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=131072 \
	hwui.render_dirty_regions=false \
    ro.sf.lcd_density=160 
	
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.secure=0 \
    persist.sys.strictmode.visual=0

ADDITIONAL_DEFAULT_PROPERTIES += ro.secure=0
ADDITIONAL_DEFAULT_PROPERTIES += persist.sys.strictmode.visual=0

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PACKAGES += \
    librs_jni \
    liba2dp \
    libpkip \
    tinyplay \
    tinycap \
    tinymix \
    libhuaweigeneric-ril \
    wmiconfig

# Filesystem management tools
PRODUCT_PACKAGES += \
	make_ext4fs \
	setup_fs
	
# Add prebuild apks and superuser
PRODUCT_PACKAGES += \
	recovery-reboot \
	Superuser \
	su 
	
$(call inherit-product-if-exists, hardware/broadcom/wlan/bcmdhd/firmware/bcm4329/device-bcm.mk)

# for bugmailer
ifneq ($(TARGET_BUILD_VARIANT),user)
	PRODUCT_PACKAGES += send_bug
	PRODUCT_COPY_FILES += \
		system/extras/bugmailer/bugmailer.sh:system/bin/bugmailer.sh \
		system/extras/bugmailer/send_bug:system/bin/send_bug
endif

$(call inherit-product, frameworks/base/build/tablet-dalvik-heap.mk)
#$(call inherit-product, vendor/shuttle/n10/device-vendor.mk)
