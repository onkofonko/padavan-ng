/* source: xio-socketpair.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_socketpair_h_included
#define __xio_socketpair_h_included 1

const extern struct addrdesc xioaddr_socketpair;

extern int xiosocketpair(struct opt *opts, int pf, int socktype, int proto, int sv[2]);

#endif /* !defined(__xio_socketpair_h_included) */
