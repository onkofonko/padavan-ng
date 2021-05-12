#
# src/tools/Makefile.  Generated from Makefile.in by configure.
#
# Copyright (c) 2013, Verisign, Inc., NLNet Labs
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the names of the copyright holders nor the
#   names of its contributors may be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package = getdns
version = 1.6.0
tarname = getdns
distdir = $(tarname)-$(version)

prefix = 
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
INSTALL = /usr/bin/install -c
LIBTOOL = ../../libtool

srcdir = .

CC=/opt/padavan-ng/trunk/../toolchain/out/bin/mipsel-linux-uclibc-gcc -mips32r2 -march=mips32r2 -mtune=1004kc
WPEDANTICFLAG=-Wpedantic
CFLAGS=-I$(srcdir)/.. -I$(srcdir) -I.. $(cflags) -Os -fomit-frame-pointer -fno-stack-protector -femit-struct-debug-baseonly -fno-var-tracking -pipe  -DEMBED -I/opt/padavan-ng/trunk/stage/include -Dlinux -D__linux__ -Dunix -Wall -Wno-trigraphs -Wno-strict-aliasing -Wno-format-security -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-format-truncation -Wno-format-overflow -Wno-misleading-indentation -fconserve-stack -fno-strict-overflow -fno-var-tracking-assignments -fno-merge-all-constants -fmerge-constants -fno-stack-check -fno-delete-null-pointer-checks -Wno-pointer-sign -ffunction-sections -fdata-sections -Wall -Wextra -D_BSD_SOURCE -D_DEFAULT_SOURCE  -I/opt/padavan-ng/trunk/stage/include $(WPEDANTICFLAG) $(XTRA_CFLAGS)
LDFLAGS=-L.. -L/opt/padavan-ng/trunk/stage/lib -Wl,--gc-sections -L/opt/padavan-ng/trunk/stage/lib
LDLIBS=../libgetdns.la -lssl -lcrypto -lpthread 

ALL_OBJS=getdns_query.lo getdns_server_mon.lo

PROGRAMS=getdns_query getdns_server_mon


.SUFFIXES: .c .o .a .lo .h

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.c.lo:
	$(LIBTOOL) --quiet --tag=CC --mode=compile $(CC) $(CFLAGS) -c $< -o $@

default: all

all: $(PROGRAMS)

$(ALL_OBJS):
	$(LIBTOOL) --quiet --tag=CC --mode=compile $(CC) $(CFLAGS) -c $(srcdir)/$(@:.lo=.c) -o $@

getdns_query: getdns_query.lo
	$(LIBTOOL) --tag=CC --mode=link $(CC) $(CFLAGS) -o $@ getdns_query.lo $(LDFLAGS) $(LDLIBS)

getdns_server_mon: getdns_server_mon.lo
	$(LIBTOOL) --tag=CC --mode=link $(CC) $(CFLAGS) -o $@ getdns_server_mon.lo $(LDFLAGS) $(LDLIBS)

stubby:
	cd .. && $(MAKE) $@

install-getdns_query: getdns_query
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install cp getdns_query $(DESTDIR)$(bindir)

uninstall-getdns_query:
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(bindir)/getdns_query

install-getdns_server_mon: getdns_server_mon
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install cp getdns_server_mon $(DESTDIR)$(bindir)

uninstall-getdns_server_mon:
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(bindir)/getdns_server_mon

install-stubby:
	cd .. && $(MAKE) $@

uninstall-stubby:
	cd .. && $(MAKE) $@

clean:
	rm -f *.o *.lo $(PROGRAMS)
	rm -rf .libs

distclean : clean
	rm -f Makefile

Makefile: $(srcdir)/Makefile.in ../../config.status
	cd ../.. && ./config.status src/test/Makefile

depend:
	(cd $(srcdir) ; awk 'BEGIN{P=1}{if(P)print}/^# Dependencies/{P=0}' Makefile.in > Makefile.in.new )
	(blddir=`pwd`; cd $(srcdir) ; gcc -MM -I. -I.. -I"$$blddir"/.. *.c | \
		sed -e "s? $$blddir/? ?g" \
		    -e 's? \([a-z0-9_-]*\)\.\([ch]\)? $$(srcdir)/\1.\2?g' \
		    -e 's? \.\./\([a-z0-9_-]*\)\.h? $$(srcdir)/../\1.h?g' \
		    -e 's? \.\./\([a-z0-9_-]*\)/\([a-z0-9_-]*\)\.h? $$(srcdir)/../\1/\2.h?g' \
		    -e 's? \$$(srcdir)/config\.h? ../config.h?g' \
		    -e 's? \$$(srcdir)/\.\./config\.h? ../config.h?g' \
		    -e 's? \$$(srcdir)/\.\./getdns/getdns\.h? ../getdns/getdns.h?g' \
		    -e 's? \$$(srcdir)/\.\./getdns/getdns_extra\.h? ../getdns/getdns_extra.h?g' \
		    -e 's!\(.*\)\.o[ :]*!\1.lo \1.o: !g' >> Makefile.in.new )
	(cd $(srcdir) ; diff Makefile.in.new Makefile.in && rm Makefile.in.new \
	                                                 || mv Makefile.in.new Makefile.in )

.PHONY: clean test

# Dependencies for getdns_query
getdns_query.lo getdns_query.o: $(srcdir)/getdns_query.c \
 ../config.h \
 $(srcdir)/../debug.h \
 ../getdns/getdns.h \
 ../getdns/getdns_extra.h
getdns_server_mon.lo getdns_server_mon.o: $(srcdir)/getdns_server_mon.c \
 ../config.h \
 ../getdns/getdns.h \
 ../getdns/getdns_extra.h
