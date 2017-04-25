Linux Kernel for OrangePI H3 boards
===================================

About
-----

The repository contains Linux kernel sources (3.4.113 and 4.9) adapted for OrangePI H3 boards, gcc toolchain, adapted rootfs and building scripts.

Building kernel 3.4.113
--------

Kernel config files and the files specific to OPI board are placed in **build** directory.

The included build script *build_linux_kernel.sh* can be used to build the kernel<br />
`./build_linux_kernel.sh [clean | all | 2 | plus] [clean]`

**clean** as 1st parameter cleans the kernel tree and build directories<br />
**clean** as 2nd parameter cleans the kernel tree before build<br />
**all** builds the uImage for OPI-2 & OPI-PLUS<br />
**2** builds the uImage for OPI-2 OPI-PCPLUS OPI-ONE OPI-PC<br />
**plus** builds the uImage for OPI-PLUS OPI-PLUS2E<br />

After the build the resulting kernel files (uImage and kernel modules) are placed into **build** directory.

To build **script.bin** for all OPI boards and resolutions run:<br />
`./build_scripts [clean]`<br />
**clean** as 1st parameter cleans the scripts and logs<br />

After the build the *script.bin* are placed into **build** directory.

To **configure kernel** run:<br />
`./config_linux_kernel.sh`

---------------------------------------------------------------------------------------
Building u-boot-2017.03 and kernel 4.9
------------------------------------

The included build script *build_mainline_uboot.sh and build_mainline_kernel.sh* can be used to build u-boot and kernel.

`./build_mainline_uboot.sh  [2 | one | pc | pc-plus | plus | lite | plus2e]` 
`./build_mainline_kernel.sh [opi | clean]`

**2 one pc pc-plus plus lite plus2e** builds the u-boot-sunxi-with-spl.bin and boot.scr for these boards

**clean** as 1st parametercleans the kernel tree and build directories
**opi**   build the uImage for OrangePi boards


After the build the resulting kernel files (u-boot-sunxi-with-spl.bin  uImage kernel modules) are placed into **build** directory.






