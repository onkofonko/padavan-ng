/* source: xio-unix.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_unix_h_included
#define __xio_unix_h_included 1

extern const struct addrdesc xioaddr_unix_connect;
extern const struct addrdesc xioaddr_unix_listen;
extern const struct addrdesc xioaddr_unix_sendto;
extern const struct addrdesc xioaddr_unix_recvfrom;
extern const struct addrdesc xioaddr_unix_recv;
extern const struct addrdesc xioaddr_unix_client;
extern const struct addrdesc xioaddr_abstract_connect;
extern const struct addrdesc xioaddr_abstract_listen;
extern const struct addrdesc xioaddr_abstract_sendto;
extern const struct addrdesc xioaddr_abstract_recvfrom;
extern const struct addrdesc xioaddr_abstract_recv;
extern const struct addrdesc xioaddr_abstract_client;

extern const struct optdesc xioopt_unix_bind_tempname;
extern const struct optdesc xioopt_unix_tightsocklen;

extern socklen_t
xiosetunix(int pf,
	   struct sockaddr_un *saun,
	   const char *path,
	   bool abstract,
	   bool tight);
extern int
xiosetsockaddrenv_unix(int idx, char *namebuff, size_t namelen,
		       char *valuebuff, size_t valuelen,
		       struct sockaddr_un *sa, socklen_t salen, int ipproto);

extern int
_xioopen_unix_client(xiosingle_t *xfd, int xioflags, groups_t groups, int abstract, struct opt *opts, const char *name, const struct addrdesc *addrdesc);

extern char *xio_tempnam(const char *pathx, bool donttry);

#endif /* !defined(__xio_unix_h_included) */
