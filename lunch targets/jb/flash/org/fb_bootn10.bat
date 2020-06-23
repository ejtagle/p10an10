:fastboot -c "fbcon=rotate:1 mem=1024M@0M usbcore.old_scheme_first=1 console=tty0 ignore_loglevel earlyprintk initcall_debug tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:200:800,system:1f00:1dd00:800,cache:1fc00:12500:800,misc:32100:1400:800,staging:33600:1f400:800,userdata:52b00:6e0500:800" boot %1.img
:fastboot -c "fbcon=rotate:1 mem=1024M@0M usbcore.old_scheme_first=1 console=tty0 lpj=2000000 boot_delay=100 ignore_loglevel earlyprintk initcall_debug tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:200:800,system:1f00:1dd00:800,cache:1fc00:12500:800,misc:32100:1400:800,staging:33600:1f400:800,userdata:52b00:6e0500:800" boot %1.img
fastboot -c "mem=1024M@0M usbcore.old_scheme_first=1 console=tty0 tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:200:800,system:1f00:1dd00:800,cache:1fc00:12500:800,misc:32100:1400:800,staging:33600:1f400:800,userdata:52b00:6e0500:800" boot %1.img

tegrapart=
recovery:300:a00:800,
boot:d00:1000:800,
mbr:1d00:200:800,
system:1f00:1dd00:800,
cache:1fc00:12500:800,
misc:32100:1400:800,
staging:33600:1f400:800,
userdata:52b00:6e0500:800

recovery:     5242880 =    A00 * 800 , O=  300
boot:         8388608 =   1000 * 800 , O=  D00
mbr:          1048576 =    200 * 800 , O= 1D00
system:     250085376 =  1dd00 * 800 , O= 1F00
cache:      153616384 =  12500 * 800 , O=1FC00
misc:        10485760 =   1400 * 800 , O=32100
er1:           131072 =     40 * 800 , O=33500
staging:    262144000 =  1f400 * 800 , O=33600
er2:           131072 =     40 * 800 , O=52A00
userdata: 14766571520 = 6e0500 * 800 , O=52b00