# https://github.com/bol-van/zapret
# https://github.com/Anonym-tsk/nfqws-keenetic

SRC_NAME = nfq

all: download
	rm -rf $(SRC_NAME); \
	cp -rf src/$(SRC_NAME) $(SRC_NAME); \
	patch -d $(SRC_NAME) -p1 -i ../nfq-compat.patch; \
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

download:
	test -d src || git clone https://github.com/bol-van/zapret --depth=1 src;
	test -d src && ( cd src && git pull )

clean:
	rm -rf $(SRC_NAME)

romfs:
	$(ROMFSINST) -p +x $(SRC_NAME)/nfqws /usr/bin/nfqws
	$(ROMFSINST) -p +x /usr/bin/zapret.sh
	$(ROMFSINST) /usr/share/zapret
