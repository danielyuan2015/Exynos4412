镜像文件相关说明：

Bootloader:
|                  \   
├── fwbl1.bin ---- |
├── bl2.bin ------ | => ubootpak.bin
├── u-boot.bin --- |
└── tzsw.bin ----- /

Kernel:
|                  \
├── ramdisk.img -- | => boot.img --- \
└── zImage ------- /                 |
                                     | => update.zip
Filesystem:                          |
└── system.img --------------------- /

Tools:
├── exynos4x12-irom-sd.sh -- For burnning ubootpak.bin to external booting SDCARD
└── sd_fusing.sh ----------- For burnning fwbl1.bin bl2.bin u-boot.bin tzsw.bin to external booting SDCARD

烧写指南:
一、fastboot手动更新方式
1、系统上电，在串口终端中按空格键进入uboot命令行
2、如果系统INAND未执行过任何分区命令，则键入命令：fdisk -c 0
3、确认USB线已正确连接至PC端，在uboot的串口终端中键入命令：fastboot 以启动传输服务
4、在PC端的命令行下，按需键入如下命令:
BOOTLOADER:
	fastboot flash ubootpak ubootpak.bin
	或者
	fastboot flash fwbl1 fwbl1.bin
	fastboot flash bl2 bl2.bin
	fastboot flash bootloader u-boot.bin
	fastboot flash tzsw tzsw.bin

BOOT IMAGE:
	fastboot flash boot boot.img
	或者
	fastboot flash kernel zImage
	fastboot flash ramdisk ramdisk-uboot.img
	
FILESYSTEM:
	fastboot flash system system.img
	擦除data cache及fat分区：
	fastboot -w
	fastboot erase fat

UPDATE：
	此命令可直接烧写BOOT IMAGE及FILESYSTEM
	fastboot update update.zip

二、SD卡更新方式
1、准备一张SD卡，在其根目录下建立sdfuse文件夹
2、拷贝镜像文件：ubootpak.bin boot.img system.img至sdfuse目录下
3、系统上电，按空格键进入uboot命令行，执行命令sdfuse flashall，等待重启即可
备注，如果在系统上电时按住left键，则系统会自动执行升级功能，用于脱机升级。

三、制作量产启动SDCARD
1、准备一张SD卡，通过gparted工具将前面保留100多MB空间，后面格式化为FAT32分区
2、在FAT32分区，建立sdfuse文件夹，并拷贝ubootpak.bin boot.img system.img三个文件至此目录下
3、运行脚本exynos4x12-irom-sd.sh或者sd_fusing.sh制作启动卡
UBOOT四合一镜像ubootpak.bin烧写命令：
	sudo ./exynos4x12-irom-sd.sh /dev/sdb ubootpak.bin
UBOOT分离镜像fwbl1.bin bl2.bin u-boot.bin tzsw.bin烧写命令：
	sudo ./sd_fusing.sh /dev/sdb

备注：
1) 破坏INAND引导，操作如下：
	系统上电，按空格键进入uboot命令行，执行命令mmc erase boot 0 1 1 则INAND启动已破坏，可以从外部SDCARD启动

2) system分区以读写方式挂载：
	mount -t ext4 -o remount,rw /dev/block/mmcblk0p2 /system

3) 不烧写INAND，调试kernel及ramdisk
	fastboot boot zImage ramdisk-uboot.img
	启动DEBUG版ramdisk则使用如下命令：
	fastboot boot zImage debug-ramdisk-uboot.img

4) 切换INAND的boot及user模式
	emmc open 0
	emmc close 0

5) 烧写QT文件系统
	fastboot flash system qt-rootfs.img
	设置uboot启动参数:
	env set bootargs "root=/dev/mmcblk0p2 rw rootfstype=ext4 lcd=vs070cxn tp=ft5x06-1024x600 cam=ov2655 mac=00:09:c0:ff:ee:58"
	env set bootcmd "movi read kernel 0 40008000;bootm 40008000"
	env save

6)　烧写lubuntu系统至外部SDCARD
	制作一张sdcard并将其格式化为EXT4分区，在PC平台上执行解压到sdcard命令:
	tar xvf lubuntu-12.04-x4412-roofs.tar.bz2 -C /xxxxx/sdcard
	设置uboot启动参数:
	env set bootargs "root=/dev/mmcblk1p1 rw rootfstype=ext4 lcd=vga-1024x768 tp=ft5x06-800x480 cam=ov2655 mac=00:09:c0:ff:ee:58"
	env set bootcmd "movi read kernel 0 40008000;bootm 40008000"
	env save

7) 恢复uboot默认配置参数
   在uboot的命令行里执行如下指令,即可
   env default -f
   env save

====================================================================================
UBOOT启动内核时传递命令行参数说明如下：
1)设置网卡MAC地址为
env set bootargs "mac=00:09:c0:ff:ee:58"
env save

2)Camera模块,如果没有传递任何"cam=xxx"参数，则默认使能OV2655模块
cam变量可选参数列表如下：
	ov2655
	tvp5150
	tvp5146
示例，选择tvp5150 TVIN模块
env set bootargs "cam=tvp5150"
env save

3)选择LCD液晶屏
lcd变量可选择参数列表如下：
	ek070tn93 		(800 X 480)
	vs070cxn		(1024 X 600)
	vga-1024x768	(1024 X 768)
	vga-1280x1024	(1280 X 1024)
	vga-1920x1200	(1920 X 1200)
示例，选择EK070TN93标清屏(800 X 480)
env set bootargs "lcd=ek070tn93"
env save

4)选择触摸屏分辨率
tp变量可选择参数列表如下：
	ft5x06-800x480
	ft5x06-1024x600
示例，选择ft5x06-800x480
env set bootargs "tp=ft5x06-800x480"
env save

5)示例参数：
标清屏：
env set bootargs "lcd=ek070tn93 tp=ft5x06-800x480 cam=ov2655 mac=00:09:c0:ff:ee:58"
env save

高清屏:
env set bootargs "lcd=vs070cxn tp=ft5x06-1024x600 cam=ov2655 mac=00:09:c0:ff:ee:58"
env save

VGA-1024x768:
env set bootargs "lcd=vga-1024x768 tp=ft5x06-800x480 cam=ov2655 mac=00:09:c0:ff:ee:58"
env save

VGA-1280x1024:
env set bootargs "lcd=vga-1280x1024 tp=ft5x06-800x480 cam=ov2655 mac=00:09:c0:ff:ee:58"
env save

VGA-1920x1200:
env set bootargs "lcd=vga-1920x1200 tp=ft5x06-800x480 cam=ov2655 mac=00:09:c0:ff:ee:58"
env save

