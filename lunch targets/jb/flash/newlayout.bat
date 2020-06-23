:fastboot -c "fbcon=rotate:1 mem=1024M@0M usbcore.old_scheme_first=1 console=tty0 ignore_loglevel earlyprintk initcall_debug tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:100:800,system:1e00:2fb00:800,cache:31900:12500:800,misc:43e00:1400:800,userdata:45300:710e80:800" boot %1.img
:fastboot -c "fbcon=rotate:1 mem=1024M@0M usbcore.old_scheme_first=1 console=tty0 lpj=2000000 boot_delay=100 ignore_loglevel earlyprintk initcall_debug tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:100:800,system:1e00:2fb00:800,cache:31900:12500:800,misc:43e00:1400:800,userdata:45300:710e80:800" boot %1.img
fastboot -c "mem=1024M@0M usbcore.old_scheme_first=1 console=tty0 tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:100:800,system:1e00:2fb00:800,cache:31900:12500:800,misc:43e00:1400:800,userdata:45300:710e80:800" boot %1.img

tegrapart=recovery:300:a00:800,boot:d00:1000:800,mbr:1d00:100:800,system:1e00:2fb00:800,cache:31900:12500:800,misc:43e00:1400:800,userdata:45300:710e80:800

recovery:     5242880 =    A00 * 800 , O=  300
boot:         8388608 =   1000 * 800 , O=  D00
mbr:           131072 =    100 * 800 , O= 1D00
system:     400031744 =  2fb00 * 800 , O= 1E00
cache:      153616384 =  12500 * 800 , O=31900
misc:        10485760 =   1400 * 800 , O=43E00
er1:           131072 =    100 * 800 , O=45200
userdata: 15174205440 = 710E80 * 800 , O=45300

                        733800=total