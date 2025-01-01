SRC_NAME=busybox-1.33.1
SRC_URL=https://busybox.net/downloads/$(SRC_NAME).tar.bz2
THISDIR = $(shell pwd)

#all: download_test extract_test
all:
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.bz2 ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.bz2 $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xjf $(SRC_NAME).tar.bz2; \
		find $(THISDIR)/patches -type f -name '*.patch' -print0 \
		  | sort -z \
		  | xargs -t -0 -n 1 patch -d $(SRC_NAME)  -p1 -i; \
	fi )

clean:
	$(MAKE) -C $(SRC_NAME) distclean

romfs:
	$(MAKE) -C $(SRC_NAME) CONFIG_PREFIX=$(ROMFSDIR) install
