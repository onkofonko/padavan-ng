# This is _NOT_ the library release version, it's an API version.
# Extracted from Chapter 6 "Library interface versions" of the libtool docs.
#
# <snippet>
# Here are a set of rules to help you update your library version information:
#
# 1. Start with version information of `0:0:0' for each libtool library.
# 2. Update the version information only immediately before a public release
# of your software. More frequent updates are unnecessary, and only guarantee
# that the current interface number gets larger faster.
# 3. If the library source code has changed at all since the last update,
# then increment revision (`c:r:a' becomes `c:r+1:a').
# 4. If any interfaces have been added, removed, or changed since the last
# update, increment current, and set revision to 0.
# 5. If any interfaces have been added since the last public release, then
# increment age.
# 6. If any interfaces have been removed since the last public release, then
# set age to 0.
# </snippet>
#
LIBVERSION=6:0:5

include ${top_srcdir}/Make_global.am

lib_LTLIBRARIES = libnetfilter_queue.la

noinst_HEADERS = internal.h

libnetfilter_queue_la_LDFLAGS = -Wc,-nostartfiles -lnfnetlink \
				-version-info $(LIBVERSION)
libnetfilter_queue_la_SOURCES = libnetfilter_queue.c	\
				nlmsg.c			\
				extra/checksum.c	\
				extra/icmp.c		\
				extra/ipv6.c		\
				extra/tcp.c		\
				extra/ipv4.c		\
				extra/pktbuff.c		\
				extra/udp.c

libnetfilter_queue_la_LIBADD  = ${LIBNFNETLINK_LIBS} ${LIBMNL_LIBS}
