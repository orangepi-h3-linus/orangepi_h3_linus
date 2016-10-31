Building Ubuntu/Debian installation for OrangePI H3 boards using debootstrap
============================================================================

build image with kernel 3.4.112
- Read and edit **params.sh** to adjust the parameters to your needs.<br/>
- Run `./create_image` to build the Linux installation according to settings in params.sh<br/>
- Run `./image_from_dir <directory> <img_name> [format] [opi-2|opi-PC|opi-plus|opi-one|opi-lite|opi-pcplus|opi-plus2e]`

============================================================================

build image with kernel 4.9
- Read and edit **params.sh** to adjust the parameters to your needs.<br/>
- Run `./create_image` to build the Linux installation according to settings in params.sh<br/>
- Run `./image_from_dir_mainline <directory> <img_name> [format] [opi-2|opi-PC|opi-plus|opi-one|opi-lite|opi-pcplus|opi-plus2e]`
