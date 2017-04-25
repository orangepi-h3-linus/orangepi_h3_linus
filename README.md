# Build H3 image For OrangePi

The OrangePi-Kernel contains Linux kernel sources (3.4.113 and 4.9) adapted for OrangePi H3 boards

- **make 3.4.113 uImage**
- **make 4.9 uImage and uboot**
- **make rootfs**
- **make Image**
- **how to install desktop and copy os to emmc**
-------------------------------------

- **make 3.4.113 uImage**

``` shell
cd OrangePI-Kernel                             #首先进入OrangePI-Kernel目录
sudo ./build_linux_kernel.sh clean clean       #clean kernel
sudo ./build_linux_kernel.sh plus              #plus为例，编译kernel，在build/下面生成uImage_OPI-PLUS

#如编译plus,plus2e,plus2对应是build/uImage_OPI-PLUS，其他的型号对应build/uImage_OPI-2,编译出来的modules和firmware在build/lib下面
```
- **make 4.9 uImage and uboot**

``` shell
cd OrangePI-Kernel
sudo ./build_mainline_uboot.sh [2 | pc | plus | lite | pc-plus | one | plus2e]
sudo ./build_linux_kernel_mainline.sh clean
sudo ./build_linux_kernel_mainline.sh opi

#mainline uboot的烧录方法
#sudo dd if=/dev/zero of=/dev/sdc bs=1k seek=8 count=1015
#sudo dd if=u-boot-sunxi-with-spl.bin of=/dev/sdc bs=1k seek=8
#主线内核启动方法：boot分区除了script.bin，uImage,还要有boot.scr。在编译uboot时生成，在build/uboot下面
```
- **make rootfs **

``` shell
cd OrangePi-BuildLinux
vim params.sh                                  #按照需求选择发行版等

example(例):
#distro="precise"
distro="xenial"
#distro="utopic"
#distro="vivid"
#distro="wily"
repo="http://ports.ubuntu.com/ubuntu-ports"    #需要请打开注释，不需要则注释

sudo ./create_image                            #在当前目录生成boot分区和rootfs分区
```
- **make Image**

``` shell
＃example(linux-3.4.113)
sudo ./image_from_dir ./linux-xenial orangepi ext4 opi-plus
#linux-xenial是create_image生成的文件系统分区 orangepi是镜像的名字 ext4是rootfs分区的格式 opi-plus选择板子的型号

＃example(linux-4.9)
sudo ./image_from_dir_mainline ./linux-xenial orangepi ext4 opi

sudo dd bs=4M if=orangepi.img of=/dev/sd*      #安装系统至SD卡

```
- **how to install desktop and copy os to emmc**

``` shell
fs_resize                                         #First, after BOOTING,to resize linux partition to fill sd card
reboot
install_lxde_desktop　　　　　　　　　　　　　　　#install desktop run,please wait
install_to_emmc　　　　　　　　　　　　　　　　　 #install os to emmc
```

