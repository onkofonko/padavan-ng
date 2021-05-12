# $Id: Makefile.in 4267 2012-05-24 17:37:42Z themiron.ru $
# Makefile for LNS handlers
# LIC: GPL

prefix=/usr
exec_prefix=${prefix}
mandir=${datarootdir}/man
docdir=/usr/doc/l2tp-$(VERSION)
install=/usr/bin/install -c
install_dir=/usr/bin/install -c -d
sbindir=${exec_prefix}/sbin
datarootdir=${prefix}/share

HANDLERS=sync-pppd.so cmd.so

OBJS=pty.o sync-pppd.o dstring.o
SRCS=$(OBJS:.o=.c)
CFLAGS=-I.. -I../libevent -I. -Os -fomit-frame-pointer -fno-stack-protector -femit-struct-debug-baseonly -fno-var-tracking -pipe  -Dlinux -D__linux__ -Dunix -DEMBED -I/opt/padavan-ng/trunk/stage/include -Wall -Wno-trigraphs -Wno-strict-aliasing -Wno-format-security -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-format-truncation -Wno-format-overflow -Wno-misleading-indentation -fconserve-stack -fno-strict-overflow -fno-var-tracking-assignments -fno-merge-all-constants -fmerge-constants -fno-stack-check -fno-delete-null-pointer-checks -Wno-pointer-sign -I/opt/padavan-ng/trunk/../toolchain/out/mipsel-linux-uclibc/sysroot/usr/include -DPPPOL2TP_V1 -ffunction-sections -fdata-sections -I/opt/padavan-ng/trunk/../toolchain/out/mipsel-linux-uclibc/sysroot/usr/include -DPPPOL2TP_V1 -ffunction-sections -fdata-sections

all: $(HANDLERS) l2tp-control

l2tp-control: cmd-control.o
	/opt/padavan-ng/trunk/../toolchain/out/bin/mipsel-linux-uclibc-gcc -mips32r2 -march=mips32r2 -mtune=1004kc -o l2tp-control cmd-control.o

cmd-control.o: cmd-control.c
	/opt/padavan-ng/trunk/../toolchain/out/bin/mipsel-linux-uclibc-gcc -mips32r2 -march=mips32r2 -mtune=1004kc $(CFLAGS) -c -o $@ $^

%.o: %.c
	/opt/padavan-ng/trunk/../toolchain/out/bin/mipsel-linux-uclibc-gcc -mips32r2 -march=mips32r2 -mtune=1004kc $(CFLAGS) -fPIC -c -o $@ $<

sync-pppd.so: pty.o sync-pppd.o
	/opt/padavan-ng/trunk/../toolchain/out/bin/mipsel-linux-uclibc-gcc -mips32r2 -march=mips32r2 -mtune=1004kc -shared -o $@ $^

cmd.so: cmd.o dstring.o
	/opt/padavan-ng/trunk/../toolchain/out/bin/mipsel-linux-uclibc-gcc -mips32r2 -march=mips32r2 -mtune=1004kc -shared -o $@ $^

clean:
	rm -f *.so *.o *~
	rm -f l2tp-control

install: all
	-mkdir -p $(RPM_INSTALL_ROOT)$(sbindir)
	-mkdir -p $(RPM_INSTALL_ROOT)$(prefix)/lib/l2tp/plugins
	$(install) -m 755 -s l2tp-control $(RPM_INSTALL_ROOT)$(sbindir)
	$(install) -m 755 $(HANDLERS) $(RPM_INSTALL_ROOT)$(prefix)/lib/l2tp/plugins

distclean: clean
	rm -f Makefile
