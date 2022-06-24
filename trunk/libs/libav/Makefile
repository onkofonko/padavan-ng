SRC_NAME = ffmpeg-5.0.1

ifeq ($(CONFIG_RALINK_MT7621),y)
FFMPEG_CPU = 1004kc
else ifneq (,$(filter y,$(CONFIG_RALINK_MT7620) $(CONFIG_RALINK_MT7628)))
FFMPEG_CPU = 24kec
else ifeq ($(CONFIG_RALINK_RT3883),y)
FFMPEG_CPU = 74kc
else
FFMPEG_CPU = generic
endif

PARSERS_CONFIG=--enable-parser=aac --enable-parser=ac3 --enable-parser=mpegaudio
#               --enable-parser=h264 --enable-parser=hevc --enable-parser=mpeg4video \
#               --enable-parser=mpegvideo --enable-parser=vc1 --enable-parser=flac

DEMUXERS_CONFIG=--enable-demuxer=aac --enable-demuxer=ac3 --enable-demuxer=asf \
                --enable-demuxer=avi --enable-demuxer=dts --enable-demuxer=eac3 \
                --enable-demuxer=flac --enable-demuxer=flv --enable-demuxer=h264 \
                --enable-demuxer=m4v --enable-demuxer=matroska --enable-demuxer=mov \
                --enable-demuxer=mp3 --enable-demuxer=mpegps --enable-demuxer=mpegts \
                --enable-demuxer=mpegvideo --enable-demuxer=ogg --enable-demuxer=wav \
                --enable-demuxer=vc1 --enable-demuxer=hevc
#                --enable-demuxer=wv --enable-demuxer=rm \
#                --enable-demuxer=mpc8 --enable-demuxer=mpc \
#                --enable-demuxer=ape --enable-demuxer=amr

DECODERS_CONFIG=

ifeq ($(CONFIG_FIRMWARE_INCLUDE_FIREFLY),y)
DECODERS_CONFIG+=--enable-decoder=flac --enable-decoder=alac
endif

PROTOCOLS_CONFIG=--enable-protocol=file

CFLAGS += -D_LARGEFILE64_SOURCE -D_LFS64_LARGEFILE

all: config_test
	$(MAKE) -C $(SRC_NAME)

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	./configure \
		--prefix=/ \
		--cross-prefix=$(CROSS_COMPILE) \
		--enable-cross-compile \
		--arch=mipsel \
		--cpu=$(FFMPEG_CPU) \
		--target-os=linux \
		--cc="$(CC)" \
		--as="$(AS)" \
		$(if $(LIBS_BUILD_SHARED),--disable-static,--enable-static) \
		$(if $(LIBS_BUILD_SHARED),--enable-shared,--disable-shared) \
		--enable-small \
		--enable-pthreads \
		--enable-gpl \
		--enable-zlib \
		--disable-x86asm \
		--disable-doc \
		--disable-debug \
		--disable-lzma \
		--disable-vaapi \
		--disable-vdpau \
		--disable-vsx \
		--disable-power8 \
		--disable-armv5te \
		--disable-armv6 \
		--disable-armv6t2 \
		--disable-fast-unaligned \
		--disable-runtime-cpudetect \
		--disable-altivec \
		--disable-avdevice \
		--disable-network \
		--disable-swscale \
		--disable-swscale-alpha \
		--disable-ffmpeg \
		--disable-ffplay \
		--disable-ffprobe \
		--disable-hwaccels \
		--disable-dxva2 \
		--disable-indevs \
		--disable-outdevs \
		--disable-devices \
		--disable-filters \
		--disable-encoders \
		--disable-muxers \
		--disable-decoders \
		--disable-demuxers \
		--disable-parsers \
		--disable-postproc \
		--disable-protocols \
		--disable-bsfs \
		--disable-everything \
		--disable-swresample \
		--disable-avfilter \
		--disable-programs \
		$(PROTOCOLS_CONFIG) \
		$(PARSERS_CONFIG) \
		$(DECODERS_CONFIG) \
		$(DEMUXERS_CONFIG) ; \
	)

clean:
	if [ -f $(SRC_NAME)/config.h ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean; \
	fi ; \
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME) install DESTDIR=$(STAGEDIR)

romfs:
ifeq ($(LIBS_BUILD_SHARED),y)
	cp -fP $(DESTDIR)/lib/libavcodec.so* $(ROMFSDIR)/lib ;
	cp -fP $(DESTDIR)/lib/libavformat.so* $(ROMFSDIR)/lib ;
	cp -fP $(DESTDIR)/lib/libavutil.so* $(ROMFSDIR)/lib ;
endif

