SRC_VER = 71.1.1
SRC_NAME = zapret-$(SRC_VER)
SRC_URL = https://github.com/bol-van/zapret/archive/refs/tags/v$(SRC_VER).tar.gz
SRC_URL_SH = https://github.com/nilabsent/zapretsh/archive/refs/heads/main.tar.gz
SRC_NAME_SH = zapretsh-main

CFLAGS += -DZAPRET_GH_VER=v$(SRC_VER) -DZAPRET_GH_HASH=release
CFLAGS += $(if $(CONFIG_IPV6),-DUSE_IPV6,)

all: download_test extract_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 -T20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL) || rm -f $(SRC_NAME).tar.gz; \
	fi )
	( if [ ! -f $(SRC_NAME_SH).tar.gz ]; then \
		wget -t5 -T20 --no-check-certificate -O $(SRC_NAME_SH).tar.gz $(SRC_URL_SH) || rm -f $(SRC_NAME_SH).tar.gz; \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar zxf $(SRC_NAME).tar.gz; \
		find "$(shell pwd)"/patches -type f -name '*.patch' -print0 \
		  | sort -z \
		  | xargs -r -t -0 -n 1 patch -d $(SRC_NAME) -p1 -i; \
	fi )
	( if [ ! -d $(SRC_NAME_SH) ]; then \
		tar zxf $(SRC_NAME_SH).tar.gz; \
	fi )

clean:
	( if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) clean ; \
	fi )

romfs:
	$(ROMFSINST) -p +x $(SRC_NAME)/binaries/my/nfqws /usr/bin/nfqws
	$(ROMFSINST) -p +x $(SRC_NAME_SH)/zapret/usr/bin/zapret.sh /usr/bin/zapret.sh
	$(ROMFSINST) -d $(SRC_NAME_SH)/zapret/usr/share/zapret /usr/share/zapret
