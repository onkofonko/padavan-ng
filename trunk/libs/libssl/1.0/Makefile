SRC_NAME = openssl-1.0.2u
SRC_URL  = https://github.com/openssl/openssl/releases/download/OpenSSL_1_0_2u/openssl-1.0.2u.tar.gz

# openssl use CROSS_COMPILE prefix
CC = gcc $(CPUFLAGS)

COPTS += $(CFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections
COPTS += $(if $(CONFIG_IPV6),-DOPENSSL_USE_IPV6=1,)
COPTS += -DOPENSSL_SMALL_FOOTPRINT

OPENSSL_CIPHERS := no-capieng no-cms no-gmp no-jpake no-rfc3779 no-seed \
                  no-ec2m no-err no-idea no-rc5 no-ripemd no-rmd160 \
                  no-static-engine no-hw no-md2 no-sse2 no-dso no-ts no-sha0 no-mdc2 no-krb5 \
                  no-smime no-ans1 no-cast no-whirlpool no-sctp no-srp no-ssl2 no-ssl3

ifneq ($(CONFIG_FIRMWARE_INCLUDE_TOR),y)
OPENSSL_CIPHERS += no-threads
endif

ifdef CONFIG_CRYPTO_DEV_LINUX
COPTS += -DHAVE_CRYPTODEV
else
OPENSSL_CIPHERS += no-speed
ifneq ($(CONFIG_FIRMWARE_INCLUDE_TOR),y)
OPENSSL_CIPHERS += no-engine
endif
endif

ifneq ($(CONFIG_FIRMWARE_INCLUDE_SSWAN),y)
OPENSSL_CIPHERS += no-rc2
endif

ifeq (,$(filter y,$(CONFIG_FIRMWARE_INCLUDE_OPENSSL_EC) \
                  $(CONFIG_FIRMWARE_INCLUDE_TOR) \
                  $(CONFIG_FIRMWARE_INCLUDE_OPENVPN) \
                  $(CONFIG_FIRMWARE_INCLUDE_DOH) \
                  $(CONFIG_FIRMWARE_INCLUDE_STUBBY)))
OPENSSL_CIPHERS += no-ec
endif

all: download_test extract_test config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) depend
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 -T20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL) \
			|| rm -f $(SRC_NAME).tar.gz; \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar zxf $(SRC_NAME).tar.gz; \
		find "$(shell pwd)"/patches -type f -name "*.patch" -print0 \
		  | sort -z \
		  | xargs -r -t -0 -n 1 patch -d $(SRC_NAME) -p1 -i; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME); \
	./Configure \
		linux-mipsel \
		shared \
		--prefix=/ \
		--install_prefix=$(STAGEDIR) \
		--openssldir=/etc/ssl \
		$(COPTS) \
		$(OPENSSL_CIPHERS); \
	cd ./include/openssl && ln -fs ../../crypto/ts/ts.h ts.h; \
	)

clean:
	( if [ -f $(SRC_NAME)/Makefile ]; then \
		$(MAKE) -C $(SRC_NAME) distclean; \
	fi )
	rm -rf $(SRC_NAME)/include/*
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME) install DESTDIR="$(STAGEDIR)"
	# remove unneeded
	rm -f $(DESTDIR)/lib/libssl.a
	rm -f $(DESTDIR)/lib/libcrypto.a

romfs:
	cp -fP $(DESTDIR)/lib/libssl.so* $(ROMFSDIR)/lib
	cp -fP $(DESTDIR)/lib/libcrypto.so* $(ROMFSDIR)/lib
	$(ROMFSINST) /etc_ro/openssl.cnf
ifeq ($(CONFIG_FIRMWARE_INCLUDE_OPENSSL_EXE),y)
	$(ROMFSINST) $(SRC_NAME)/apps/openssl /usr/bin/openssl
endif
