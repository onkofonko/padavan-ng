# README #

Welcome to the rt-n56u project

[Please follow this instrustion: https://gitlab.com/hadzhioglu/padavan-ng/-/blob/master/Readme-Hadzhioglu.txt]

This project aims to improve the rt-n56u and other supported devices on the software part, allowing power user to take full control over their hardware.
This project was created in hope to be useful, but comes without warranty or support. Installing it will probably void your warranty. 
Contributors of this project are not responsible for what happens next. Flash at your own risk!

### How do I get set up? ###

* [Get the tools to build the system](https://bitbucket.org/padavan/rt-n56u/wiki/EN/HowToMakeFirmware) or [Download pre-built system image](https://bitbucket.org/padavan/rt-n56u/downloads)
* Feed the device with the system image file (Follow instructions of updating your current system)
* Perform factory reset
* Open web browser on http://my.router to configure the services

### Contribution ###

Feel free to send in improvements/fixes. I'll keep the issue/pull request system open for that purpose.
NOTE: if and when a possible interesting change will get added depends on a verification/test of the particular change and if i have time to do it.

### Compilation Instructions ###

* Install dependencies

```shell
# I recommend building only on OS: Ubuntu-22.04.3 LTS (Jammy Jellyfish) and Before building the firmware, select "App Updates" and install them. Next, update the packages
sudo apt update
sudo apt upgrade
sudo apt install autoconf autoconf-archive automake autopoint bison build-essential ca-certificates cmake cpio curl doxygen fakeroot flex gawk gettext git gperf help2man htop kmod libblkid-dev libc-ares-dev libcurl4-openssl-dev libdevmapper-dev libev-dev libevent-dev libexif-dev libflac-dev libgmp3-dev libid3tag0-dev libidn2-dev libjpeg-dev libkeyutils-dev libltdl-dev libmpc-dev libmpfr-dev libncurses5-dev libogg-dev libsqlite3-dev libssl-dev libtool libtool-bin libudev-dev libunbound-dev libvorbis-dev libxml2-dev locales mc nano pkg-config ppp-dev python3 python3-docutils texinfo unzip uuid uuid-dev vim wget xxd zlib1g-dev

```
### Firmware management ###
```shell 
Login details
IP: 192.168.1.1
User: admin
Password: admin
WiFi name 2.4GHz: Padavan_2.4GHz
WiFi name 5GHz: Padavan_5GHz
WiFi Password 2.4/5GHz: 1234567890
```

### DISCLAIMER ###
IMPORTANT NOTE!! PLEASE READ IT CAREFULLY!!
# NO WARRANTY OR SUPPORT
This product includes copyrighted third-party software licensed under the terms of the GNU General Public License. Please see The GNU General Public License for the exact terms
and conditions of this license. The firmware or any other product designed or produced by this project may contain in whole or in part pre-release, untested, or not fully tested works.
This may contain errors that could cause failures or loss of data, and may be incomplete or contain inaccuracies. You expressly acknowledge and agree that use of software or any part,
produced by this project, is at Your sole and entire risk.

ANY PRODUCT IS PROVIDED 'AS IS' AND WITHOUT WARRANTY, UPGRADES OR SUPPORT OF ANY KIND. ALL CONTRIBUTORS EXPRESSLY DISCLAIM ALL WARRANTIES AND/OR CONDITIONS, EXPRESS OR IMPLIED,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES AND/OR CONDITIONS OF SATISFACTORY QUALITY, OF FITNESS FOR A PARTICULAR PURPOSE, OF ACCURACY, OF QUIET ENJOYMENT, AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS.
