SRC_NAME=https_dns_proxy
#SRC_NAME=https_dns_proxy-master
#SRC_URL=https://github.com/aarond10/https_dns_proxy/archive/refs/heads/master.tar.gz
#SRC_URL=https://github.com/baranyaib90/https_dns_proxy/archive/refs/heads/master.tar.gz

CFLAGS  += -O2 -ffunction-sections -fdata-sections -fvisibility=hidden -fPIC
LDFLAGS += -Wl,--gc-sections,-rpath-link,$(STAGEDIR)/lib -lcurl -lssl -lcrypto -lz

#all: download_test extract_test config_test
all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)/build

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 -T20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL) || rm -f $(SRC_NAME).tar.gz; \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar zxf $(SRC_NAME).tar.gz; \
	fi )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( mkdir -p $(SRC_NAME)/build && cd $(SRC_NAME)/build ; \
	cmake   -DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_INSTALL_FULL_RUNSTATEDIR=/var/run \
		-DCMAKE_INSTALL_FULL_SYSCONFDIR=/etc/storage \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=$(ROOTDIR)/include/cross-mipsel-linux.cmake .. \
	)

clean:
	rm -rf $(SRC_NAME)/build
	rm -f config_done

romfs:
	$(ROMFSINST) $(SRC_NAME)/build/https_dns_proxy /usr/sbin/doh_proxy
	$(ROMFSINST) -p +x /usr/bin/doh_proxy.sh
