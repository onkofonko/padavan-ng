SRC_NAME=tcpdump-4.99.5
SRC_URL=https://www.tcpdump.org/release/$(SRC_NAME).tar.gz

CFLAGS += -Os -ffunction-sections -fdata-sections -fvisibility=hidden -DTCPDUMP_MINI
LDFLAGS += -Wl,--gc-sections

#all: download_test extract_test config_test
all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) TCPDUMP_MINI=1

download_test:
	( if [ ! -f $(SRC_NAME).tar.xz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xzf $(SRC_NAME).tar.gz; \
		patch -d $(SRC_NAME) -p1 -i ../001-remove_pcap_debug.patch; \
		patch -d $(SRC_NAME) -p1 -i ../100-tcpdump_mini.patch; \
	fi )
	
config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	ac_cv_linux_vers=3 \
	ac_cv_header_rpc_rpcent_h=no \
	ac_cv_lib_rpc_main=no \
	ac_cv_path_PCAP_CONFIG="$(STAGEDIR)/bin/pcap-config" \
	td_cv_buggygetaddrinfo=no \
	./configure \
		--prefix=/usr \
		--without-crypto \
		--without-smi \
		--without-cap-ng \
		--disable-smb \
		$(if $(CONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done
	rm -f tcpdump

romfs:
	cp $(SRC_NAME)/tcpdump .
	$(STRIP) tcpdump
	$(ROMFSINST) /usr/sbin/tcpdump
