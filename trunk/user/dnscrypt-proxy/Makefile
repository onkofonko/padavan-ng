SRC_NAME=dnscrypt-proxy-1.9.5

CFLAGS +=$(COPT) -ffunction-sections -fdata-sections -fvisibility=hidden
LDFLAGS += -Wl,--gc-sections

#all: download_test extract_test config_test
all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		unzip -qq $(SRC_NAME).zip ; \
		patch -d $(SRC_NAME) -p1 -i ../010-internal.patch; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	CPPFLAGS="-I$(STAGEDIR)/include" \
	./configure \
		--prefix=/usr \
		--sysconfdir=/etc/storage/dnscrypt-proxy \
		--disable-ssp \
		--disable-plugins \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD); \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done dnscrypt-resolvers.csv

romfs:
	wget -O dnscrypt-resolvers.csv https://raw.githubusercontent.com/DNSCrypt/dnscrypt-resolvers/master/v1/dnscrypt-resolvers.csv || cp $(SRC_NAME)/dnscrypt-resolvers.csv .
	$(ROMFSINST) $(SRC_NAME)/src/proxy/dnscrypt-proxy /usr/sbin/dnscrypt-proxy
	$(ROMFSINST) -d dnscrypt-resolvers.csv /usr/share/dnscrypt-proxy/dnscrypt-resolvers.csv
	$(ROMFSINST) /usr/bin/dnscrypt-proxy.sh
	$(ROMFSINST) -p +x /usr/bin/dnscrypt-proxy.sh
