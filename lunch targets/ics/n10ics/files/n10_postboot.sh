#!/system/bin/ash

######################################################################
#WLAN FIX
#echo 1 > /sys/devices/platform/n10-pm-wlan/keep_on_in_suspend

#BT FIX                                                                 
#echo 1 > /sys/devices/platform/n10-pm-bt/keep_on_in_suspend 

#Power Camera on, required every boot or no device
#echo 0 > /sys/devices/platform/n10-pm-camera/power_on
#echo 1 > /sys/devices/platform/n10-pm-camera/power_on

#Sleep required i before permission change
#sleep 3

#Camera device permissions
#chmod 666 /dev/video0

#ENABLE CAMERA in Suspend
#echo 1 > /sys/devices/platform/n10-pm-camera/keep_on_in_suspend

######################################################################

##########################################################################
# Misc Filesystem permissions
chmod -R 755 /system/bin
chmod 6755 /system/bin/su
chmod 6755 /system/xbin/su
chmod -R 755 /system/etc
chown 1010 /system/etc/wifi
chgrp 1010 /system/etc/wifi
chmod 777 /system/etc/wifi
chmod 755 /system/etc/wifi/wpa_supplicant.conf
chmod 777 /data/misc/wifi
touch /data/misc/wifi/ipconfig.txt
chmod 777 /data/misc/wifi/wpa_supplicant.conf
chmod 6755 /system/bin/pppd                                                     
#########################################################################
#Misc tunings

#Add value to enable VM to have free ram to service apps
sysctl -w vm.min_free_kbytes=16384

