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

# This variable is set first, so it can be overridden
# by BoardConfigVendor.mk
BOARD_USES_GENERIC_AUDIO := true
USE_CAMERA_STUB := false

BOARD_LIB_DUMPSTATE := libdumpstate.n10

BOARD_USES_AUDIO_LEGACY := false
TARGET_USES_OLD_LIBSENSORS_HAL := false

BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/shuttle/n10/bluetooth

# Use the non-open-source parts, if they're present
-include vendor/shuttle/n10/BoardConfigVendor.mk

TARGET_NO_BOOTLOADER := true
BOOTLOADER_BOARD_NAME := p10an10


TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_SMP := true
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a
TARGET_ARCH_VARIANT_CPU := cortex-a9
TARGET_ARCH_VARIANT_FPU := vfpv3-d16
#TARGET_HAVE_TEGRA_ERRATA_657451 := true
ARCH_ARM_HAVE_TLS_REGISTER := true

#Stock CMDLINE
BOARD_KERNEL_CMDLINE := mem=1024M@0M vmalloc=192M usbcore.old_scheme_first=1 console=tty0 tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:100:800,system:1e00:2fb00:800,cache:31900:12500:800,misc:43e00:1400:800,userdata:45300:710e80:800

BOARD_KERNEL_BASE := 0x10000000
BOARD_PAGE_SIZE := 0x00000800

TARGET_NO_RADIOIMAGE := true
TARGET_BOARD_PLATFORM := tegra
TARGET_TEGRA_VERSION := t25
TARGET_BOOTLOADER_BOARD_NAME := n10
#TARGET_BOARD_INFO_FILE := device/shuttle/n10/board-info.txt

BOARD_EGL_CFG := device/shuttle/n10/files/egl.cfg

BOARD_USES_OVERLAY := true
USE_OPENGL_RENDERER := true

TARGET_OTA_ASSERT_DEVICE := n10,shuttle,P10AN10

#The P10AN01 uses an EMMC to store system/data images. Make sure to use the proper filesystem
TARGET_BOOTIMAGE_USE_EXT2 := false
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true

BOARD_BOOTIMAGE_PARTITION_SIZE := 8388608
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 8388608
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 400031744
#BOARD_USERDATAIMAGE_PARTITION_SIZE := 15174205440
BOARD_USERDATAIMAGE_PARTITION_SIZE := 8388608
BOARD_FLASH_BLOCK_SIZE := 512

TARGET_PREBUILT_KERNEL := device/shuttle/n10/kernel

# Wifi related defines
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
BOARD_WLAN_DEVICE           := bcmdhd
WIFI_DRIVER_MODULE_PATH     := "/system/lib/modules/bcmdhd.ko"
WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/bcmdhd/parameters/firmware_path"
WIFI_DRIVER_MODULE_NAME     := "bcmdhd"
WIFI_DRIVER_MODULE_ARG      := ""
WIFI_DRIVER_FW_PATH_STA     := "/system/vendor/firmware/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/system/vendor/firmware/fw_bcmdhd_p2p.bin"
WIFI_DRIVER_FW_PATH_AP      := "/system/vendor/firmware/fw_bcmdhd_apsta.bin"
 
#BOARD_WLAN_DEVICE           := bcm4329
#WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/bcm4329/parameters/firmware_path"
#WIFI_DRIVER_MODULE_PATH     := "/system/lib/modules/bcmdhd.ko"
#WIFI_DRIVER_FW_PATH_STA     := "/system/vendor/firmware/fw_bcm4329.bin"
#WIFI_DRIVER_FW_PATH_AP      := "/system/vendor/firmware/fw_bcm4329_apsta.bin"
# Following statement causes issues with compiling.
#BOARD_WLAN_DEVICE_REV := bcm4329
# These *shouldn't* be needed with bcmdhd anymore.
#WIFI_DRIVER_MODULE_NAME     := "bcmdhd"
#WIFI_DRIVER_MODULE_ARG      := "firmware_path=/system/vendor/firmware/fw_bcm4329.bin nvram_path=/system/etc/wifi/nvram.txt ifac$

# Audio
BOARD_USES_GENERIC_AUDIO := false
BOARD_USES_ALSA_AUDIO := false

# 3G
BOARD_MOBILEDATA_INTERFACE_NAME := "ppp0"

# Sensors
BOARD_USES_GENERIC_INVENSENSE := false

# BT
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true

#GPS
BOARD_HAVE_GPS := true 

#Other tweaks
BOARD_USE_SCREENCAP := true
PRODUCT_CHARACTERISTICS := tablet
BOARD_USES_SECURE_SERVICES := true

# Use a smaller subset of system fonts to keep image size lower
SMALLER_FONT_FOOTPRINT := true

# Skip droiddoc build to save build time
BOARD_SKIP_ANDROID_DOC_BUILD := true
TARGET_RECOVERY_PRE_COMMAND := "setrecovery boot-recovery recovery"
BOARD_HDMI_MIRROR_MODE := Scale

# Setting this to avoid boot locks on the system from using the "misc" partition.
BOARD_HAS_NO_MISC_PARTITION := true 

# Indicate that the board has an Internal SD Card
#BOARD_HAS_SDCARD_INTERNAL := true

BOARD_DATA_DEVICE := /dev/block/mmcblk0p6
BOARD_DATA_FILESYSTEM := ext4
BOARD_CACHE_DEVICE := /dev/block/mmcblk0p4
BOARD_CACHE_FILESYSTEM := ext4
BOARD_HAS_NO_SELECT_BUTTON := true

BOARD_VOLD_MAX_PARTITIONS := 11

BOARD_NO_ALLOW_DEQUEUE_CURRENT_BUFFER := true

# Use nicer font rendering
BOARD_USE_SKIA_LCDTEXT := true

# Avoid generating of ldrcc instructions
NEED_WORKAROUND_CORTEX_A9_745320 := true

TARGET_RECOVERY_UI_LIB := librecovery_ui_n10
TARGET_RELEASETOOLS_EXTENSIONS := device/shuttle/n10