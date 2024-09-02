SRC_NAME=obfs4
SRC_ARCH=mipselsf-k3.4
SRC_ARCH_PREFIX=mipsel
#SRC_ARCH=mipssf-k3.4
#SRC_ARCH_PREFIX=mips
SRC_URL=https:\/\/bin.entware.net\/$(SRC_ARCH)\/
SRC_VERSION=0.0.14-1

all: config_test

config_test:
	wget $(SRC_URL)obfs4_$(SRC_VERSION)_$(SRC_ARCH_PREFIX)-3.4.ipk -P ./ && bsdtar -vxf ./obfs4_$(SRC_VERSION)_$(SRC_ARCH_PREFIX)-3.4.ipk && bsdtar -vxf ./data.tar.gz && mv ./opt/sbin/obfs4proxy ./ && rm -rf opt/ control.tar.gz debian-binary obfs4_$(SRC_VERSION)_$(SRC_ARCH_PREFIX)-3.4.ipk data.tar.gz && touch ./.no_configure && echo "No configuration required!"; \

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
                $(MAKE) -C $(SRC_NAME) clean ; \
	fi ; \
	rm -f ./.no_configure ;
	rm -f ./obfs4proxy
romfs:
	$(ROMFSINST) ./obfs4proxy /usr/sbin/obfs4proxy
