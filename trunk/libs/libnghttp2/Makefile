SRC_NAME=nghttp2-1.51.0

CFLAGS += -fPIC
CXXFLAGS += -fPIC

all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)/build

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( mkdir -p $(SRC_NAME)/build && cd $(SRC_NAME)/build ; \
	cmake	-DCMAKE_INSTALL_PREFIX:PATH=/ \
		-DENABLE_APP=OFF \
		-DENABLE_EXAMPLES=OFF \
		-DENABLE_HPACK_TOOLS=OFF \
		-DENABLE_PYTHON_BINDINGS=OFF \
		-DENABLE_STATIC_LIB=ON \
		-DENABLE_SHARED_LIB=OFF \
		-DENABLE_STATIC_CRT=OFF \
		-DCMAKE_INSTALL_LOCAL_ONLY=ON \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=$(ROOTDIR)/include/cross-mipsel-linux.cmake .. \
	)

clean:
	if [ -d $(SRC_NAME)/build ] ; then \
		rm -rf $(SRC_NAME)/build ; \
	fi ; \
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME)/build install DESTDIR=$(STAGEDIR)

romfs:

