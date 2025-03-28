/* source: xio-proxy.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_proxy_h_included
#define __xio_proxy_h_included 1


struct proxyvars {
   bool ignorecr;
   char *version;
   bool doresolve;
   char *authstring;
   char *authfile;
   char *targetaddr;	/* name/address of host, in malloced string */
   uint16_t targetport;
} ;

extern const struct optdesc opt_proxyport;
extern const struct optdesc opt_ignorecr;
extern const struct optdesc opt_http_version;
extern const struct optdesc opt_proxy_resolve;
extern const struct optdesc opt_proxy_authorization;
extern const struct optdesc opt_proxy_authorization_file;

extern const struct addrdesc xioaddr_proxy_connect;

extern int _xioopen_proxy_init(struct proxyvars *proxyvars, struct opt *opts, const char *targetname, const char *targetport);
extern int _xioopen_proxy_prepare(struct proxyvars *proxyvars, struct opt *opts, const char *targetname, const char *targetport, const int ai_flags[2]);
extern int _xioopen_proxy_connect(struct single *xfd, struct proxyvars *proxyvars, int level);

#endif /* !defined(__xio_proxy_h_included) */
