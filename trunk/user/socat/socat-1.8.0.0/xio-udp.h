/* source: xio-udp.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_udp_h_included
#define __xio_udp_h_included 1

extern const struct addrdesc xioaddr_udp_connect;
extern const struct addrdesc xioaddr_udp_listen;
extern const struct addrdesc xioaddr_udp_sendto;
extern const struct addrdesc xioaddr_udp_datagram;
extern const struct addrdesc xioaddr_udp_recvfrom;
extern const struct addrdesc xioaddr_udp_recv;
extern const struct addrdesc xioaddr_udp4_connect;
extern const struct addrdesc xioaddr_udp4_listen;
extern const struct addrdesc xioaddr_udp4_sendto;
extern const struct addrdesc xioaddr_udp4_datagram;
extern const struct addrdesc xioaddr_udp4_recvfrom;
extern const struct addrdesc xioaddr_udp4_recv;
extern const struct addrdesc xioaddr_udp6_connect;
extern const struct addrdesc xioaddr_udp6_listen;
extern const struct addrdesc xioaddr_udp6_sendto;
extern const struct addrdesc xioaddr_udp6_datagram;
extern const struct addrdesc xioaddr_udp6_recvfrom;
extern const struct addrdesc xioaddr_udp6_recv;

extern int _xioopen_ipdgram_listen(struct single *sfd,
	int xioflags, union sockaddr_union *us, socklen_t uslen,
	struct opt *opts, int pf, int socktype, int ipproto);

extern int xioopen_udp_sendto(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
extern int xioopen_udp_datagram(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
extern int xioopen_udp_recvfrom(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
extern int xioopen_udp_recv(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

extern int _xioopen_udp_sendto(const char *hostname, const char *servname, struct opt *opts, int xioflags, xiofile_t *xxfd, groups_t groups, int pf, int socktype, int ipproto);

extern int xioopen_ipdgram_listen(int argc, const char *argv[], struct opt *opts, int rw, xiofile_t *xfd, const struct addrdesc *addrdesc);

#endif /* !defined(__xio_udp_h_included) */
