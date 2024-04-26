SRC_NAME=miniupnpd-2.3.6
SRC_URL=https://miniupnp.tuxfamily.org/files/download.php?file=$(SRC_NAME).tar.gz
THISDIR = $(shell pwd)

# reference to iptables package
IPT_VERSION:=iptables-1.8.10

# disable support IPv6 yet, because no profit w/o IGDv2
# e.g. Windows not supported IGDv2 and UPnP not worked
ENABLE_IPV6:=0

IPTABLESPATH=$(ROOTDIR)/user/iptables/$(IPT_VERSION)

#all: download_test extract_test
all:
	cd $(SRC_NAME) && ./configure
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) IPTABLESPATH=$(IPTABLESPATH) ENABLE_IPV6=$(ENABLE_IPV6)

download_test:
	( if [ ! -f $(SRC_NAME).tar.gz ]; then \
		wget -t5 --timeout=20 --no-check-certificate -O $(SRC_NAME).tar.gz $(SRC_URL); \
	fi )

extract_test:
	( if [ ! -d $(SRC_NAME) ]; then \
		tar xzf $(SRC_NAME).tar.gz; \
		find $(THISDIR)/patches -type f -name '*.patch' -print0 \
		  | sort -z \
		  | xargs -t -0 -n 1 patch -d $(SRC_NAME)  -p1 -i; \
	fi )

clean:
	$(MAKE) -C $(SRC_NAME) clean
	rm -f miniupnpd

romfs:
	cp $(SRC_NAME)/miniupnpd .
	$(ROMFSINST) /usr/bin/miniupnpd
