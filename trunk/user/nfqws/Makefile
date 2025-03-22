# https://github.com/bol-van/zapret

SRC_VER = 70.4
SRC_NAME = zapret-$(SRC_VER)
SRC_URL = https://github.com/bol-van/zapret/archive/refs/tags/v$(SRC_VER).tar.gz
THISDIR = $(shell pwd)

CFLAGS += -DZAPRET_GH_VER=v$(SRC_VER) -DZAPRET_GH_HASH=release

all: download_test extract_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 -T20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL) || rm -f $(SRC_NAME).tar.gz; \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar zxf $(SRC_NAME).tar.gz; \
		[ ! -d $(THISDIR)/patches ] || \
		find $(THISDIR)/patches -type f -name '*.patch' -print0 \
		  | sort -z \
		  | xargs -r -t -0 -n 1 patch -d $(SRC_NAME) -p1 -i; \
	fi )

clean:
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) clean

romfs:
	$(ROMFSINST) -p +x $(SRC_NAME)/binaries/my/nfqws /usr/bin/nfqws
	$(ROMFSINST) -p +x /usr/bin/zapret.sh
	$(ROMFSINST) -d config /usr/share/zapret
