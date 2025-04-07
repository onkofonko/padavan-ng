SRC_VERSION = 1.8.7
SRC_NAME = wsdd2-$(SRC_VERSION)
SRC_URL = https://github.com/Netgear/wsdd2/archive/refs/tags/$(SRC_VERSION).tar.gz
THISDIR = $(shell pwd)

# all: download_test extract_test config_test
all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xf $(SRC_NAME).tar.gz; \
		patch -d $(SRC_NAME) -p1 -i ../uclibc.compat.patch; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		touch config_done; \
	fi )

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) clean ; \
	fi ; \
	rm -f config_done

romfs:
	$(ROMFSINST) $(SRC_NAME)/wsdd2 /sbin/wsdd2
