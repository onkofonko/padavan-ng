SRC_NAME=busybox-1.36.1
SRC_URL=https://busybox.net/downloads/$(SRC_NAME).tar.bz2

all:
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.bz2 ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.bz2 $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar jxf $(SRC_NAME).tar.bz2; \
		patch -d $(SRC_NAME) -p1 -i ../busybox-1.36.1-orig.patch ; \
	fi )

clean:
	$(MAKE) -C $(SRC_NAME) distclean

romfs:
	$(MAKE) -C $(SRC_NAME) CONFIG_PREFIX=$(ROMFSDIR) install

