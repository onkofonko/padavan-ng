Readme 
[Padavan-ng build by Mahtab]

OS: Ubuntu-22.04 LTS (arm64v8/aarch64) [Should also work on x64]
Installation type: Docker Container [Should also work on VM and Native installation]

Installed packages:
apt install -y -q nano unzip autoconf automake bison build-essential flex gawk gettext gperf libtool zlib1g-dev doxygen pkg-config cmake libtool-bin curl gawk  htop xxd fakeroot cpio autopoint help2man libncurses5-dev libltdl-dev wget kmod locales vim libgmp3-dev libmpc-dev libmpfr-dev texinfo mc libtool-doc libudev-dev uuid uuid-dev libblkid-dev libxml2-dev libssl-dev libevent-dev libsqlite3-dev libdevmapper-dev libkeyutils-dev python pip

pip install docutils

For doh_proxy: apt install -y -q libc-ares-dev libcurl4-openssl-dev libev-dev

NB: Had to upgrade e2fsprogs 1.46.5 (latest version) to build it with latest Ubuntu.


