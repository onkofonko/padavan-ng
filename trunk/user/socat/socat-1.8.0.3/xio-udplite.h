/* source: xio-udplite.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_udplite_h_included
#define __xio_udplite_h_included 1

extern const struct addrdesc xioaddr_udplite_connect;
extern const struct addrdesc xioaddr_udplite_listen;
extern const struct addrdesc xioaddr_udplite_sendto;
extern const struct addrdesc xioaddr_udplite_datagram;
extern const struct addrdesc xioaddr_udplite_recvfrom;
extern const struct addrdesc xioaddr_udplite_recv;
extern const struct addrdesc xioaddr_udplite4_connect;
extern const struct addrdesc xioaddr_udplite4_listen;
extern const struct addrdesc xioaddr_udplite4_sendto;
extern const struct addrdesc xioaddr_udplite4_datagram;
extern const struct addrdesc xioaddr_udplite4_recvfrom;
extern const struct addrdesc xioaddr_udplite4_recv;
extern const struct addrdesc xioaddr_udplite6_connect;
extern const struct addrdesc xioaddr_udplite6_listen;
extern const struct addrdesc xioaddr_udplite6_sendto;
extern const struct addrdesc xioaddr_udplite6_datagram;
extern const struct addrdesc xioaddr_udplite6_recvfrom;
extern const struct addrdesc xioaddr_udplite6_recv;

extern const struct optdesc xioopt_udplite_send_cscov;
extern const struct optdesc xioopt_udplite_recv_cscov;

#endif /* !defined(__xio_udplite_h_included) */
