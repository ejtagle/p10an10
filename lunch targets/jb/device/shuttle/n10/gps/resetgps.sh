#!/system/bin/sh

# Reset gps
echo 0 > /sys/devices/platform/n10-pm-gps/power_on
echo 1 > /sys/devices/platform/n10-pm-gps/power_on
 