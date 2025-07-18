SRC_NAME=libxml2-v2.14.5
SRC_URL=https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.14.5/$(SRC_NAME).tar.gz

#all: download_test extract_test config_test
all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar zxf $(SRC_NAME).tar.gz ; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi ) 

configure:
	( cd $(SRC_NAME) ; \
	./autogen.sh -fi ; \
	./configure \
		--prefix= \
		--enable-static \
		--disable-shared \
		$(if $(CONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		--without-catalog \
		--without-debug \
		--without-docbook \
		--without-ftp \
		--without-http \
		--without-legacy \
		--without-lzma \
		--without-pattern \
		--without-push \
		--without-python \
		--without-readline \
		--without-regexps \
		--without-iconv \
		--without-iso8859x \
		--with-c14n \
		--with-html \
		--with-output \
		--with-reader \
		--with-sax1 \
		--with-schemas \
		--with-threads \
		--with-tree \
		--with-valid \
		--with-writer \
		--with-xinclude \
		--with-xpath \
		--with-xptr \
		--with-zlib=$(STAGEDIR) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME) install DESTDIR=$(STAGEDIR)
	
romfs:
