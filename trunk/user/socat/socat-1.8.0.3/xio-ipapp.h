/* source: xio-ipapp.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ipapp_h_included
#define __xio_ipapp_h_included 1


/* when selecting a low port, this is the lowest possible */
#define XIO_IPPORT_LOWER 640


extern const struct optdesc opt_sourceport;
/*extern const struct optdesc opt_port;*/
extern const struct optdesc opt_lowport;

extern int xioopen_ipapp_connect(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);
extern int _xioopen_ipapp_init(struct single *sfd, int xioflags, struct opt *opts, bool *dofork, int *maxchildren, int *pf, int *socktype, int *protocol);
extern int _xioopen_ipapp_prepare(struct opt **opts, struct opt *opts0, const char *hostname, const char *portname, int pf, int socktype, int protocol, const int ai_flags[2], struct addrinfo ***themarr, struct addrinfo ***bindarr, uint16_t *bindport, bool *needbind, bool *lowport);
extern int _xioopen_ipapp_connect(struct single *sfd, const char *hostname, struct opt *opts, struct addrinfo **themarr, bool needbind, struct addrinfo **bindarr, uint16_t bindport, bool lowport, int level);
extern int xioopen_ipapp_listen(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);
extern int _xioopen_ipapp_listen_prepare(struct opt *opts, struct opt **opts0, const char *portname, int *pf, int ipproto, const int ai_flags[2], union sockaddr_union *us, socklen_t *uslen, int socktype);

#endif /* !defined(__xio_ipapp_h_included) */
