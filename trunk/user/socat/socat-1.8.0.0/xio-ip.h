/* source: xio-ip.h */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xio_ip_h_included
#define __xio_ip_h_included 1

extern const struct optdesc opt_ip_options;
extern const struct optdesc opt_ip_pktinfo;
extern const struct optdesc opt_ip_recvtos;
extern const struct optdesc opt_ip_recvttl;
extern const struct optdesc opt_ip_recvopts;
extern const struct optdesc opt_ip_retopts;
extern const struct optdesc opt_ip_tos;
extern const struct optdesc opt_ip_ttl;
extern const struct optdesc opt_ip_hdrincl;
extern const struct optdesc opt_ip_recverr;
extern const struct optdesc opt_ip_mtu_discover;
extern const struct optdesc opt_ip_mtu;
extern const struct optdesc opt_ip_freebind;
extern const struct optdesc opt_ip_router_alert;
extern const struct optdesc opt_ip_multicast_ttl;
extern const struct optdesc opt_ip_multicast_loop;
extern const struct optdesc opt_ip_multicast_if;
extern const struct optdesc opt_ip_pktoptions;
extern const struct optdesc opt_ip_add_membership;
extern const struct optdesc opt_ip_add_source_membership;
extern const struct optdesc opt_ip_recvdstaddr;
extern const struct optdesc opt_ip_recvif;
extern const struct optdesc opt_ip_transparent;

extern const struct optdesc opt_ai_addrconfig;
extern const struct optdesc opt_ai_passive;
extern const struct optdesc opt_ai_v4mapped;

extern const struct optdesc opt_res_debug;
extern const struct optdesc opt_res_aaonly;
extern const struct optdesc opt_res_usevc;
extern const struct optdesc opt_res_primary;
extern const struct optdesc opt_res_igntc;
extern const struct optdesc opt_res_recurse;
extern const struct optdesc opt_res_defnames;
extern const struct optdesc opt_res_stayopen;
extern const struct optdesc opt_res_dnsrch;
extern const struct optdesc opt_res_retrans;
extern const struct optdesc opt_res_retry;
extern const struct optdesc opt_res_nsaddr;

extern int xioinit_ip(int *pf, char ipv);

extern int xiogetaddrinfo(const char *node, const char *service, int family, int socktype, int protocol, struct addrinfo **res, const int ai_flags[2]);
extern void xiofreeaddrinfo(struct addrinfo *res);
extern int xioresolve(const char *node, const char *service, int family, int socktype, int protocol, union sockaddr_union *addr, socklen_t *addrlen, const int ai_flags[2]);
extern int xiolog_ancillary_ip(struct single *sfd, struct cmsghdr *cmsg, int *num, char *typbuff, int typlen, char *nambuff, int namlen, char *envbuff, int envlen, char *valbuff, int vallen);
extern int xiotype_ip_add_membership(char *token, const struct optname *ent, struct opt *opt);
extern int xioapply_ip_add_membership(xiosingle_t *xfd, struct opt *opt);
extern int xiotype_ip_add_source_membership(char* token, const struct optname *ent, struct opt *opt);
extern int xioapply_ip_add_source_membership(struct single *xfd, struct opt *opt);

#if WITH_RESOLVE && HAVE_RESOLV_H
extern int xio_res_init(struct single *sfd, struct __res_state *save_res);
extern int xio_res_restore(struct __res_state *save_res);
#endif /* WITH_RESOLVE && HAVE_RESOLV_H */

#endif /* !defined(__xio_ip_h_included) */
