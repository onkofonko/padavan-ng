SRC_NAME=minisatip-git

CFLAGS  += -ffunction-sections -fdata-sections -fvisibility=hidden
CFLAGS  += -Os -Wall -g -D_GNU_SOURCE
CFLAGS  += -I$(STAGEDIR)/include

LDFLAGS += -Wl,--gc-sections
LDFLAGS += -L$(STAGEDIR)/lib


THISDIR = $(shell pwd)

all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	./autoreconf -fi ; \
	./configure \
		--prefix=/usr \
		--disable-dvbaes \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

romfs:
	$(ROMFSINST) -S $(THISDIR)/$(SRC_NAME)/html/ /share/minisatip/html/
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/minisatip /usr/sbin/minisatip
