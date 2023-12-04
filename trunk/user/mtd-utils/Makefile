SRC_NAME=mtd-utils-2.1.6
SRC_URL = https://github.com/sigma-star/mtd-utils/archive/refs/tags/v2.1.6.tar.gz
THISDIR = $(shell pwd)

#all: download_test extract_test config_test
all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xzf $(SRC_NAME).tar.gz; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	autoreconf -fvi ; \
	./configure \
		--prefix= \
		--enable-static \
		--disable-shared \
		--disable-tests \
		--disable-install-tests \
		--without-jffs \
		--without-xattr \
		--without-ubifs \
		--without-zstd \
		--without-crypto \
		--without-lzo \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	$(MAKE) -C $(SRC_NAME) clean
	rm -f config_done

romfs:
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/ubiattach /sbin/ubiattach
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/ubidetach /sbin/ubidetach
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/ubiformat /sbin/ubiformat
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/ubimkvol /sbin/ubimkvol
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/ubinfo /sbin/ubinfo
