SRC_NAME1=squashfs-4.6.1
SRC_NAME2=xz-5.8.1
SRC_URL2=https://github.com/tukaani-project/xz/releases/download/v5.8.1/$(SRC_NAME2).tar.xz

#all: download_test extract_test config_xz build_xz build_squashfs
all: config_xz build_xz build_squashfs

download_test:
	( if [ ! -f $(SRC_NAME2).tar.xz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME2).tar.xz $(SRC_URL2); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME2) ]; then \
		tar xJf $(SRC_NAME2).tar.xz; \
	fi )

config_xz:
	( if [ -f ./config_xz_done ]; then \
		echo "the same configuration"; \
	else \
		make configure_xz && touch config_xz_done; \
	fi )

configure_xz:
	( cd $(SRC_NAME2) ; \
	./autogen.sh ; \
	./configure \
		--disable-doc \
		--disable-shared \
		--disable-werror \
		--disable-xz \
		--disable-xzdec \
		--disable-lzmadec \
		--disable-lzmainfo \
		CFLAGS="-std=c99" ; \
	)

build_xz:
	make -j$(HOST_NCPU) -C $(SRC_NAME2)

build_squashfs:
	make -j$(HOST_NCPU) -C $(SRC_NAME1) XZUTILSDIR=$(shell pwd)/$(SRC_NAME2) ; \
	cp $(SRC_NAME1)/mksquashfs ./
	cp $(SRC_NAME1)/unsquashfs ./

clean:
	if [ -f $(SRC_NAME2)/Makefile ] ; then \
		make -C $(SRC_NAME2) distclean ; \
	fi
	make -C $(SRC_NAME1) clean
	rm -f config_xz_done
	rm -f mksquashfs
	rm -f mksquashfs unsquashfs
