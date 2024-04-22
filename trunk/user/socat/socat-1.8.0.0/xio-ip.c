/* source: xio-ip.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for IP related functions */

#include "xiosysincludes.h"

#if _WITH_IP4 || _WITH_IP6

#include "xioopen.h"

#include "xio-ascii.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-ip6.h"
#include "nestlex.h"


#if WITH_IP4 || WITH_IP6

#ifdef IP_OPTIONS
const struct optdesc opt_ip_options = { "ip-options", "ipoptions", OPT_IP_OPTIONS, GROUP_SOCK_IP, PH_PASTSOCKET,TYPE_BIN, OFUNC_SOCKOPT_APPEND, SOL_IP, IP_OPTIONS };
#endif
#ifdef IP_PKTINFO
const struct optdesc opt_ip_pktinfo = { "ip-pktinfo", "pktinfo",   OPT_IP_PKTINFO, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_PKTINFO };
#endif
#ifdef IP_RECVTOS
const struct optdesc opt_ip_recvtos = { "ip-recvtos", "recvtos",   OPT_IP_RECVTOS, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVTOS };
#endif
#ifdef IP_RECVTTL	/* -Cygwin */
const struct optdesc opt_ip_recvttl = { "ip-recvttl", "recvttl",   OPT_IP_RECVTTL, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVTTL };
#endif
#ifdef IP_RECVOPTS
const struct optdesc opt_ip_recvopts= { "ip-recvopts","recvopts",  OPT_IP_RECVOPTS,GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVOPTS };
#endif
#ifdef IP_RETOPTS
const struct optdesc opt_ip_retopts = { "ip-retopts", "retopts",   OPT_IP_RETOPTS, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RETOPTS };
#endif
const struct optdesc opt_ip_tos     = { "ip-tos",     "tos",       OPT_IP_TOS,     GROUP_SOCK_IP, PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_TOS };
const struct optdesc opt_ip_ttl     = { "ip-ttl",     "ttl",       OPT_IP_TTL,     GROUP_SOCK_IP, PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_TTL };
#ifdef IP_HDRINCL
const struct optdesc opt_ip_hdrincl = { "ip-hdrincl", "hdrincl",   OPT_IP_HDRINCL, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_HDRINCL };
#endif
#ifdef IP_RECVERR
const struct optdesc opt_ip_recverr = { "ip-recverr", "recverr",   OPT_IP_RECVERR, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVERR };
#endif
#ifdef IP_MTU_DISCOVER
const struct optdesc opt_ip_mtu_discover={"ip-mtu-discover","mtudiscover",OPT_IP_MTU_DISCOVER,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_IP,IP_MTU_DISCOVER };
#endif
#ifdef IP_MTU
const struct optdesc opt_ip_mtu     = { "ip-mtu",     "mtu",       OPT_IP_MTU,     GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_MTU };
#endif
#ifdef IP_TRANSPARENT
const struct optdesc opt_ip_transparent = {"ip-transparent", "transparent", OPT_IP_TRANSPARENT, GROUP_SOCK_IP, PH_PREBIND, TYPE_BOOL, OFUNC_SOCKOPT, SOL_IP, IP_TRANSPARENT};
#endif
#ifdef IP_FREEBIND
const struct optdesc opt_ip_freebind= { "ip-freebind","freebind",  OPT_IP_FREEBIND,GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_FREEBIND };
#endif
#ifdef IP_ROUTER_ALERT
const struct optdesc opt_ip_router_alert={"ip-router-alert","routeralert",OPT_IP_ROUTER_ALERT,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_IP,IP_ROUTER_ALERT};
#endif
/* following: Linux allows int but OpenBSD reqs char/byte */
const struct optdesc opt_ip_multicast_ttl={"ip-multicast-ttl","multicastttl",OPT_IP_MULTICAST_TTL,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_BYTE,OFUNC_SOCKOPT,SOL_IP,IP_MULTICAST_TTL};
/* following: Linux allows int but OpenBSD reqs char/byte */
const struct optdesc opt_ip_multicast_loop={"ip-multicast-loop","multicastloop",OPT_IP_MULTICAST_LOOP,GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_BYTE,OFUNC_SOCKOPT,SOL_IP,IP_MULTICAST_LOOP};
const struct optdesc opt_ip_multicast_if  ={"ip-multicast-if",  "multicast-if", OPT_IP_MULTICAST_IF,  GROUP_SOCK_IP,PH_PASTSOCKET,TYPE_IP4NAME,OFUNC_SOCKOPT,SOL_IP,IP_MULTICAST_IF};
#ifdef IP_PKTOPTIONS
const struct optdesc opt_ip_pktoptions = { "ip-pktoptions", "pktopts", OPT_IP_PKTOPTIONS, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_PKTOPTIONS };
#endif
#ifdef IP_ADD_MEMBERSHIP
const struct optdesc opt_ip_add_membership = { "ip-add-membership", "membership",OPT_IP_ADD_MEMBERSHIP, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_IP_MREQN, OFUNC_SPEC, SOL_IP, IP_ADD_MEMBERSHIP };
#endif
#if defined(HAVE_STRUCT_IP_MREQ_SOURCE) && defined(IP_ADD_SOURCE_MEMBERSHIP)
const struct optdesc opt_ip_add_source_membership = { "ip-add-source-membership", "source-membership",OPT_IP_ADD_SOURCE_MEMBERSHIP, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_IP_MREQ_SOURCE, OFUNC_SOCKOPT, SOL_IP, IP_ADD_SOURCE_MEMBERSHIP };
#endif
#ifdef IP_RECVDSTADDR
const struct optdesc opt_ip_recvdstaddr = { "ip-recvdstaddr", "recvdstaddr",OPT_IP_RECVDSTADDR, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVDSTADDR };
#endif
#ifdef IP_RECVIF
const struct optdesc opt_ip_recvif = { "ip-recvif", "recvdstaddrif",OPT_IP_RECVIF, GROUP_SOCK_IP, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IP, IP_RECVIF };
#endif

#ifdef AI_ADDRCONFIG
const struct optdesc opt_ai_addrconfig = { "ai-addrconfig", "addrconfig", OPT_AI_ADDRCONFIG, GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.ai_flags), XIO_SIZEOF(para.socket.ip.ai_flags), AI_ADDRCONFIG };
#endif
#ifdef AI_V4MAPPED
const struct optdesc opt_ai_v4mapped   = { "ai-v4mapped",   "v4mapped",   OPT_AI_V4MAPPED,   GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.ai_flags), XIO_SIZEOF(para.socket.ip.ai_flags), AI_V4MAPPED };
#endif
#ifdef AI_PASSIVE
const struct optdesc opt_ai_passive    = { "ai-passive",    "passive",    OPT_AI_PASSIVE,    GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.ai_flags), XIO_SIZEOF(para.socket.ip.ai_flags), AI_PASSIVE    };
#endif

#if WITH_RESOLVE
#if WITH_RES_DEPRECATED
#  define WITH_RES_AAONLY 1
#  define WITH_RES_PRIMARY 1
#endif /* WITH_RES_DEPRECATED */
#if HAVE_RESOLV_H
const struct optdesc opt_res_debug    = { "res-debug",    NULL,       OPT_RES_DEBUG,    GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_DEBUG };
#if WITH_RES_AAONLY
const struct optdesc opt_res_aaonly   = { "res-aaonly",   "aaonly",   OPT_RES_AAONLY,   GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_AAONLY };
#endif
const struct optdesc opt_res_usevc    = { "res-usevc",    "usevc",    OPT_RES_USEVC,    GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_USEVC };
#if WITH_RES_PRIMARY
const struct optdesc opt_res_primary  = { "res-primary",  "primary",  OPT_RES_PRIMARY,  GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_PRIMARY };
#endif
const struct optdesc opt_res_igntc    = { "res-igntc",    "igntc",    OPT_RES_IGNTC,    GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_IGNTC };
const struct optdesc opt_res_recurse  = { "res-recurse",  "recurse",  OPT_RES_RECURSE,  GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_RECURSE };
const struct optdesc opt_res_defnames = { "res-defnames", "defnames", OPT_RES_DEFNAMES, GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_DEFNAMES };
const struct optdesc opt_res_stayopen = { "res-stayopen", "stayopen", OPT_RES_STAYOPEN, GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_STAYOPEN };
const struct optdesc opt_res_dnsrch   = { "res-dnsrch",   "dnsrch",   OPT_RES_DNSRCH,   GROUP_SOCK_IP, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.socket.ip.res.opts), XIO_SIZEOF(para.socket.ip.res.opts), RES_DNSRCH };
#if HAVE_RES_RETRANS
const struct optdesc opt_res_retrans  = { "res-retrans",  "retrans",  OPT_RES_RETRANS,  GROUP_SOCK_IP, PH_OFFSET, TYPE_INT,  OFUNC_OFFSET,       XIO_OFFSETOF(para.socket.ip.res.retrans), XIO_SIZEOF(para.socket.ip.res.retrans), RES_MAXRETRANS };
#endif
#if HAVE_RES_RETRY
const struct optdesc opt_res_retry    = { "res-retry",    NULL,       OPT_RES_RETRY,    GROUP_SOCK_IP, PH_OFFSET, TYPE_INT,  OFUNC_OFFSET,       XIO_OFFSETOF(para.socket.ip.res.retry),   XIO_SIZEOF(para.socket.ip.res.retry),   RES_MAXRETRY };
#endif
#if HAVE_RES_NSADDR_LIST
const struct optdesc opt_res_nsaddr   = { "res-nsaddr",   "dns",      OPT_RES_NSADDR,   GROUP_SOCK_IP, PH_OFFSET, TYPE_IP4SOCK, OFUNC_OFFSET,    XIO_OFFSETOF(para.socket.ip.res.nsaddr),  XIO_SIZEOF(para.socket.ip.res.retry),   RES_MAXRETRY };
#endif
#endif /* HAVE_RESOLV_H */
#endif /* WITH_RESOLVE */
#endif /* WITH_IP4 || WITH_IP6 */


int xioinit_ip(
	int *pf,
	char ipv)
{
	if (*pf == PF_UNSPEC) {
		switch (ipv) {
		case '0': *pf = PF_UNSPEC; break;
#if WITH_IP4
		case '4': *pf = PF_INET; break;
#endif
#if WITH_IP6
		case '6': *pf = PF_INET6; break;
#endif
		}
	}
	return 0;
}


#if HAVE_RESOLV_H

int Res_init(void) {
   int result;
   Debug("res_init()");
   result = res_init();
   Debug1("res_init() -> %d", result);
   return result;
}

#endif /* HAVE_RESOLV_H */


/* the ultimate(?) socat resolver function
 node: the address to be resolved; supported forms:
   1.2.3.4 (IPv4 address)
   [::2]   (IPv6 address)
   hostname (hostname resolving to IPv4 or IPv6 address)
   hostname.domain (fq hostname resolving to IPv4 or IPv6 address)
 service: the port specification; may be numeric or symbolic
 family: PF_INET, PF_INET6, or PF_UNSPEC permitting both
 socktype: SOCK_STREAM, SOCK_DGRAM, ...
 protocol: IPPROTO_UDP, IPPROTO_TCP
 sau: an uninitialized storage for the resulting socket address
 returns: STAT_OK, STAT_RETRYLATER, STAT_NORETRY, prints message
*/
int xiogetaddrinfo(const char *node, const char *service,
		   int family, int socktype, int protocol,
		   struct addrinfo **res, const int ai_flags[2]) {
   char *numnode = NULL;
   size_t nodelen;
#if HAVE_GETADDRINFO
   struct addrinfo hints = {0};
#else /* HAVE_PROTOTYPE_LIB_getipnodebyname || nothing */
   struct hostent *host;
#endif
   int error_num;

   if (service && service[0]=='\0') {
      Error("empty port/service");
   }

#if LATER
#ifdef WITH_VSOCK
   if (family == AF_VSOCK) {
      error_num = sockaddr_vm_parse(&sau->vm, node, service);
      if (error_num < 0)
         return STAT_NORETRY;

      return STAT_OK;
   }
#endif /* WITH_VSOCK */
#endif /* LATER */

   /* the resolver functions might handle numeric forms of node names by
      reverse lookup, that's not what we want.
      So we detect these and handle them specially */
   if (0) { 	/* for canonical reasons */
      ;
#if WITH_IP6
   } else if (node && node[0] == '[' && node[(nodelen=strlen(node))-1]==']') {
      if ((numnode = Malloc(nodelen-1)) == NULL)
	 return STAT_NORETRY;

      strncpy(numnode, node+1, nodelen-2);	/* ok */
      numnode[nodelen-2] = '\0';
      node = numnode;
#if HAVE_GETADDRINFO
      hints.ai_flags |= AI_NUMERICHOST;
#endif /* HAVE_GETADDRINFO */
      if (family == PF_UNSPEC)  family = PF_INET6;
#endif /* WITH_IP6 */
   }
   if (family == 0)
      hints.ai_flags |= AI_ADDRCONFIG;
#if HAVE_GETADDRINFO
   if (node != NULL || service != NULL) {
      struct addrinfo *record;

      if (ai_flags != NULL) {
	 hints.ai_flags |= ai_flags[0];
	 hints.ai_flags &= ~ai_flags[1];
      }
      hints.ai_family = family;
      hints.ai_socktype = socktype;
      hints.ai_protocol = protocol;
      hints.ai_addrlen = 0;
      hints.ai_addr = NULL;
      hints.ai_canonname = NULL;
      hints.ai_next = NULL;

      do {
	error_num = Getaddrinfo(node, service, &hints, res);
	if (error_num == 0)  break;
	if (error_num == EAI_SOCKTYPE && socktype != 0) {
	   /* there are systems where kernel goes SCTP but not getaddrinfo() */
	   hints.ai_socktype = 0;
	   continue;
	}
	if (error_num == EAI_SERVICE && protocol != 0) {
	   if (hints.ai_protocol == 0) {
	      Error7("getaddrinfo(\"%s\", \"%s\", {0x%02x,%d,%d,%d}, {}): %s",
		     node?node:"NULL", service?service:"NULL",
		     hints.ai_flags, hints.ai_family,
		     hints.ai_socktype, hints.ai_protocol,
		     gai_strerror(error_num));
	      if (*res != NULL)
		 freeaddrinfo(*res);
	      if (numnode)
		 free(numnode);
	      return STAT_NORETRY;
	   }
	   /* Probably unsupported protocol (e.g. UDP-Lite), fallback to 0 */
	   hints.ai_protocol = 0;
	   continue;
	}
      if ((error_num = Getaddrinfo(node, service, &hints, res)) != 0) {
	 Error7("getaddrinfo(\"%s\", \"%s\", {0x%02x,%d,%d,%d}, {}): %s",
		node?node:"NULL", service?service:"NULL",
		hints.ai_flags, hints.ai_family,
		hints.ai_socktype, hints.ai_protocol,
		(error_num == EAI_SYSTEM)?
		strerror(errno):gai_strerror(error_num));
	 if (*res != NULL)
	    freeaddrinfo(*res);
	 if (numnode)
	    free(numnode);

	 return STAT_RETRYLATER;
	}
      } while (1);
      service = NULL;	/* do not resolve later again */

#if WITH_MSGLEVEL <= E_DEBUG
      record = *res;
      while (record) {
	 char buff[256/*!*/];
	 sockaddr_info(record->ai_addr, record->ai_addrlen, buff, sizeof(buff));
	 Debug5("getaddrinfo() -> flags=0x%02x family=%d socktype=%d protocol=%d addr=%s", record->ai_flags, record->ai_family, record->ai_socktype, record->ai_protocol, buff);
	 record = record->ai_next;
      }
#endif /* WITH_MSGLEVEL <= E_DEBUG */
   }

#elif HAVE_PROTOTYPE_LIB_getipnodebyname /* !HAVE_GETADDRINFO */

   if (node != NULL) {
      /* first fallback is getipnodebyname() */
      if (family == PF_UNSPEC) {
#if WITH_IP4 && WITH_IP6
      switch (xioparms.default_ip) {
      case '4': pf = PF_INET; break;
      case '6': pf = PF_INET6; break;
      default: break;		/* includes \0 */
      }
#elif WITH_IP6
	 family = PF_INET6;
#else
	 family = PF_INET;
#endif
      }
      host = Getipnodebyname(node, family, AI_V4MAPPED, &error_num);
      if (host == NULL) {
	 const static char ai_host_not_found[] = "Host not found";
	 const static char ai_no_address[]     = "No address";
	 const static char ai_no_recovery[]    = "No recovery";
	 const static char ai_try_again[]      = "Try again";
	 const char *error_msg = "Unknown error";
	 switch (error_num) {
	 case HOST_NOT_FOUND: error_msg = ai_host_not_found; break;
	 case NO_ADDRESS:     error_msg = ai_no_address;
	 case NO_RECOVERY:    error_msg = ai_no_recovery;
	 case TRY_AGAIN:      error_msg = ai_try_again;
	 }
	 Error2("getipnodebyname(\"%s\", ...): %s", node, error_msg);
      } else {
	 switch (family) {
#if WITH_IP4
	 case PF_INET:
	    *socklen = sizeof(sau->ip4);
	    sau->soa.sa_family = PF_INET;
	    memcpy(&sau->ip4.sin_addr, host->h_addr_list[0], 4);
	    break;
#endif
#if WITH_IP6
	 case PF_INET6:
	    *socklen = sizeof(sau->ip6);
	    sau->soa.sa_family = PF_INET6;
	    memcpy(&sau->ip6.sin6_addr, host->h_addr_list[0], 16);
	    break;
#endif
	 }
      }
      freehostent(host);
   }

#elsif 0 /* !HAVE_PROTOTYPE_LIB_getipnodebyname */

   if (node != NULL) {
      /* this is not a typical IP6 resolver function - but Linux
	 "man gethostbyname" says that the only supported address type with
	 this function is AF_INET _at present_, so maybe this fallback will
	 be useful somewhere sometimes in a future even for IP6 */
      if (family == PF_UNSPEC) {
#if WITH_IP4 && WITH_IP6
      switch (xioparms.default_ip) {
      case '4': pf = PF_INET; break;
      case '6': pf = PF_INET6; break;
      default: break;		/* includes \0 */
      }
#elif WITH_IP6
	 family = PF_INET6;
#else
	 family = PF_INET;
#endif
      }
      /*!!! try gethostbyname2 for IP6 */
      if ((host = Gethostbyname(node)) == NULL) {
	 Error2("gethostbyname(\"%s\"): %s", node,
		h_errno == NETDB_INTERNAL ? strerror(errno) :
		hstrerror(h_errno));
	 return STAT_RETRYLATER;
      }
      if (host->h_addrtype != family) {
	 Error2("xiogetaddrinfo(): \"%s\" does not resolve to %s",
		node, family==PF_INET?"IP4":"IP6");
      } else {
	 switch (family) {
#if WITH_IP4
	 case PF_INET:
	    *socklen = sizeof(sau->ip4);
	    sau->soa.sa_family = PF_INET;
	    memcpy(&sau->ip4.sin_addr, host->h_addr_list[0], 4);
	    break;
#endif /* WITH_IP4 */
#if WITH_IP6
	 case PF_INET6:
	    *socklen = sizeof(sau->ip6);
	    sau->soa.sa_family = PF_INET6;
	    memcpy(&sau->ip6.sin6_addr, host->h_addr_list[0], 16);
	    break;
#endif /* WITH_IP6 */
	 }
      }
   }

#endif

   if (numnode)  free(numnode);

   return STAT_OK;
}

void xiofreeaddrinfo(struct addrinfo *res) {
#if HAVE_GETADDRINFO
   freeaddrinfo(res);
#else
   ;
#endif
}

/* A simple resolver interface that just returns one address,
   the first found by calling xiogetaddrinfo().
   family may be AF_INET, AF_INET6, or AF_UNSPEC;
   Returns -1 when an error occurred or when no result found.
*/
int xioresolve(const char *node, const char *service,
	       int family, int socktype, int protocol,
	       union sockaddr_union *addr, socklen_t *addrlen,
	       const int ai_flags[2])
{
   struct addrinfo *res = NULL;
   struct addrinfo *aip;
   int rc;

   rc = xiogetaddrinfo(node, service, family, socktype, protocol,
		       &res, ai_flags);
   if (rc != 0) {
      xiofreeaddrinfo(res);
      return -1;
   }
   if (res == NULL) {
      Warn1("xioresolve(node=\"%s\", ...): No result", node);
      xiofreeaddrinfo(res);
      return -1;
   }
   if (res->ai_addrlen > *addrlen) {
      Warn3("xioresolve(node=\"%s\", addrlen="F_socklen", ...): "F_socklen" bytes required", node, *addrlen, res->ai_addrlen);
      xiofreeaddrinfo(res);
      return -1;
   }
   if (res->ai_next != NULL) {
      Info4("xioresolve(node=\"%s\", service=%s%s%s, ...): More than one address found", node?node:"NULL", service?"\"":"", service?service:"NULL", service?"\"":"");
   }

   aip = res;
   if (ai_flags != NULL && ai_flags[0] & AI_PASSIVE && family == PF_UNSPEC) {
      /* We select the first IPv6 address, if available,
	 because this might accept IPv4 connections too */
      while (aip != NULL) {
	 if (aip->ai_family == PF_INET6)
	    break;
	 aip = aip->ai_next;
      }
      if (aip == NULL)
	 aip = res;
   }

   memcpy(addr, aip->ai_addr, aip->ai_addrlen);
   *addrlen = aip->ai_addrlen;
   xiofreeaddrinfo(res);
   return 0;
}

#if defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA)
/* Converts the ancillary message in *cmsg into a form useable for further
   processing. knows the specifics of common message types.
   These are valid for IPv4 and IPv6
   Returns the number of resulting syntax elements in *num
   Returns a sequence of \0 terminated type strings in *typbuff
   Returns a sequence of \0 terminated name strings in *nambuff
   Returns a sequence of \0 terminated value strings in *valbuff
   The respective len parameters specify the available space in the buffers
   Returns STAT_OK on success
   Returns STAT_WARNING if a buffer was too short and data truncated.
 */
int xiolog_ancillary_ip(
	struct single *sfd,
	struct cmsghdr *cmsg,
	int *num,
	char *typbuff, int typlen,
	char *nambuff, int namlen,
	char *envbuff, int envlen,
	char *valbuff, int vallen)
{
   int cmsgctr = 0;
   const char *cmsgtype, *cmsgname = NULL, *cmsgenvn = NULL;
   size_t msglen;
   char scratch1[16];	/* can hold an IPv4 address in ASCII */
#if WITH_IP4 && defined(IP_PKTINFO) && HAVE_STRUCT_IN_PKTINFO
   char scratch2[16];
   char scratch3[16];
#endif
   int rc = 0;

   msglen = cmsg->cmsg_len-((char *)CMSG_DATA(cmsg)-(char *)cmsg);
   envbuff[0] = '\0';
   switch (cmsg->cmsg_type) {
   default:
      *num = 1;
      typbuff[0] = '\0'; strncat(typbuff, "IP", typlen-1);
      snprintf(nambuff, namlen, "type_%u", cmsg->cmsg_type);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#if WITH_IP4
#if defined(IP_PKTINFO) && HAVE_STRUCT_IN_PKTINFO
   case IP_PKTINFO: {
      struct in_pktinfo *pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
      *num = 3;
      typbuff[0] = '\0'; strncat(typbuff, "IP_PKTINFO", typlen-1);
      snprintf(nambuff, namlen, "%s%c%s%c%s", "if", '\0', "locaddr", '\0', "dstaddr");
      snprintf(envbuff, envlen, "%s%c%s%c%s", "IP_IF", '\0',
	       "IP_LOCADDR", '\0', "IP_DSTADDR");
      snprintf(valbuff, vallen, "%s%c%s%c%s",
	       xiogetifname(pktinfo->ipi_ifindex, scratch1, -1), '\0',
#if HAVE_PKTINFO_IPI_SPEC_DST
	       inet4addr_info(ntohl(pktinfo->ipi_spec_dst.s_addr),
			      scratch2, sizeof(scratch2)),
#else
	       "",
#endif
	       '\0',
	       inet4addr_info(ntohl(pktinfo->ipi_addr.s_addr),
			      scratch3, sizeof(scratch3)));
      Notice3("Ancillary message: interface \"%s\", locaddr=%s, dstaddr=%s",
	      xiogetifname(pktinfo->ipi_ifindex, scratch1, -1),
#if HAVE_PKTINFO_IPI_SPEC_DST
	      inet4addr_info(ntohl(pktinfo->ipi_spec_dst.s_addr),
			     scratch2, sizeof(scratch2)),
#else
	      "",
#endif
	      inet4addr_info(ntohl(pktinfo->ipi_addr.s_addr),
			     scratch3, sizeof(scratch3)));
   }
      return STAT_OK;
#endif /* defined(IP_PKTINFO) && HAVE_STRUCT_IN_PKTINFO */
#endif /* WITH_IP4 */
#if defined(IP_RECVERR) && HAVE_STRUCT_SOCK_EXTENDED_ERR
   case IP_RECVERR: {
      struct xio_extended_err {
	 struct sock_extended_err see;
	 __u32 data0;
	 __u32 data1;
	 __u32 data2;
	 __u32 data3;
      } ;
      struct xio_extended_err *err =
	 (struct xio_extended_err *)CMSG_DATA(cmsg);
      *num = 6;
      typbuff[0] = '\0'; strncat(typbuff, "IP_RECVERR", typlen-1);
      snprintf(nambuff, namlen, "%s%c%s%c%s%c%s%c%s%c%s",
	       "errno", '\0', "origin", '\0', "type", '\0',
	       "code", '\0', "info", '\0', "data");
      snprintf(envbuff, envlen, "%s%c%s%c%s%c%s%c%s%c%s",
	       "IP_RECVERR_ERRNO", '\0', "IP_RECVERR_ORIGIN", '\0',
	       "IP_RECVERR_TYPE", '\0', "IP_RECVERR_CODE", '\0',
	       "IP_RECVERR_INFO", '\0', "IP_RECVERR_DATA");
      snprintf(valbuff, vallen, "%u%c%u%c%u%c%u%c%u%c%u",
	       err->see.ee_errno, '\0', err->see.ee_origin, '\0', err->see.ee_type, '\0',
	       err->see.ee_code, '\0', err->see.ee_info, '\0', err->see.ee_data);
      /* semantic part */
      switch (err->see.ee_origin) {
	 char addrbuff[40];
#if WITH_IP4
      case SO_EE_ORIGIN_ICMP:
	 if (1) {
	    inet4addr_info(ntohl(err->data1), addrbuff, sizeof(addrbuff));
	    Notice6("received ICMP from %s, type %d, code %d, info %d, data %d, resulting in errno %d",
		    addrbuff, err->see.ee_type, err->see.ee_code, err->see.ee_info, err->see.ee_data, err->see.ee_errno);
	 }
	 break;
#endif /* WITH_IP4 */
#if WITH_IP6
      case SO_EE_ORIGIN_ICMP6:
	 if (1) {
	    Notice5("received ICMP type %d, code %d, info %d, data %d, resulting in errno %d",
		    err->see.ee_type, err->see.ee_code, err->see.ee_info, err->see.ee_data, err->see.ee_errno);
	 }
	 break;
#endif /* WITH_IP6 */
      default:
	 Notice6("received error message origin %d, type %d, code %d, info %d, data %d, generating errno %d",
		 err->see.ee_origin, err->see.ee_type, err->see.ee_code, err->see.ee_info, err->see.ee_data, err->see.ee_errno);
	 break;
      }
      return STAT_OK;
   }
#endif /* defined(IP_RECVERR) && HAVE_STRUCT_SOCK_EXTENDED_ERR */
#ifdef IP_RECVIF
   case IP_RECVIF: {
      /* spec in FreeBSD: /usr/include/net/if_dl.h */
      struct sockaddr_dl *sadl = (struct sockaddr_dl *)CMSG_DATA(cmsg);
      *num = 1;
      typbuff[0] = '\0'; strncat(typbuff, "IP_RECVIF", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "if", namlen-1);
      envbuff[0] = '\0'; strncat(envbuff, "IP_IF", envlen-1);
      valbuff[0] = '\0';
      strncat(valbuff,
	      xiosubstr(scratch1, sadl->sdl_data, 0, sadl->sdl_nlen), vallen-1);
      Notice1("IP_RECVIF: %s", valbuff);
      return STAT_OK;
   }
#endif /* defined(IP_RECVIF) */
#if WITH_IP4
#ifdef IP_RECVDSTADDR
   case IP_RECVDSTADDR:
      *num = 1;
      typbuff[0] = '\0'; strncat(typbuff, "IP_RECVDSTADDR", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "dstaddr", namlen-1);
      envbuff[0] = '\0'; strncat(envbuff, "IP_DSTADDR", envlen-1);
      inet4addr_info(ntohl(*(uint32_t *)CMSG_DATA(cmsg)), valbuff, vallen);
      Notice1("IP_RECVDSTADDR: %s", valbuff);
      return STAT_OK;
#endif
#endif /* WITH_IP4 */
   case IP_OPTIONS:
#ifdef IP_RECVOPTS
   case IP_RECVOPTS:
#endif
      cmsgtype = "IP_OPTIONS"; cmsgname = "options"; cmsgctr = -1;
      /*!!!*/
      break;
#if XIO_ANCILLARY_TYPE_SOLARIS
   case IP_RECVTOS:
#else
   case IP_TOS:
#endif
      cmsgtype = "IP_TOS";     cmsgname = "tos"; cmsgctr = msglen;
      break;
   case IP_TTL: /* Linux */
#ifdef IP_RECVTTL
   case IP_RECVTTL: /* FreeBSD */
#endif
      cmsgtype = "IP_TTL";     cmsgname = "ttl"; cmsgctr = msglen; break;
   }
   /* when we come here we provide a single parameter
      with name in cmsgname, value length in msglen */
   *num = 1;
   if (strlen(cmsgtype) >= typlen)  rc = STAT_WARNING;
   typbuff[0] = '\0'; strncat(typbuff, cmsgtype, typlen-1);
   if (strlen(cmsgname) >= namlen)  rc = STAT_WARNING;
   nambuff[0] = '\0'; strncat(nambuff, cmsgname, namlen-1);
   if (cmsgenvn) {
      if (strlen(cmsgenvn) >= envlen)  rc = STAT_WARNING;
      envbuff[0] = '\0'; strncat(envbuff, cmsgenvn, envlen-1);
   } else {
      envbuff[0] = '\0';
   }
   switch (cmsgctr) {
   case sizeof(char):
      snprintf(valbuff, vallen, "%u", *(unsigned char *)CMSG_DATA(cmsg));
      Notice2("Ancillary message: %s=%u", cmsgname, *(unsigned char *)CMSG_DATA(cmsg));
      break;
   case sizeof(int):
      snprintf(valbuff, vallen, "%u",    (*(unsigned int *)CMSG_DATA(cmsg)));
      Notice2("Ancillary message: %s=%u", cmsgname, *(unsigned int *)CMSG_DATA(cmsg));
      break;
   case 0:
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0); break;
   default: break;
   }
   return rc;
}
#endif /* defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA) */


#if defined(HAVE_STRUCT_IP_MREQ) || defined (HAVE_STRUCT_IP_MREQN)
int xiotype_ip_add_membership(
	char *tokp,
	const struct optname *ent,
	struct opt *opt)
{
	/* we do not resolve the addresses here because we do not yet know
	   if we are coping with a IPv4 or IPv6 socat address */
	const char *ends[] = { ":", NULL };
	const char *nests[] = { "[","]", NULL };
	char buff[512], *buffp=buff; size_t bufspc = sizeof(buff)-1;
	int parsres;

	/* parse first IP address, expect ':' */
	/*! result= */
	parsres =
		nestlex((const char **)&tokp, &buffp, &bufspc,
			ends, NULL, NULL, nests,
			true, false, false);
	if (parsres < 0) {
		Error1("option too long:  \"%s\"", tokp);
		return -1;
	} else if (parsres > 0) {
		Error1("syntax error in \"%s\"", tokp);
		return -1;
	}
	if (*tokp != ':') {
		Error1("syntax in option %s: missing ':'", tokp);
	}
	*buffp++ = '\0';
	if ((opt->value.u_string/*multiaddr*/ = strdup(buff)) == NULL) {
	   Error1("strdup(\"%s\"): out of memory", buff);
	   return -1;
	}

	++tokp;
	/* parse second IP address, expect ':' or '\0'' */
	buffp = buff;
	/*! result= */
	parsres =
		nestlex((const char **)&tokp, &buffp, &bufspc,
			ends, NULL, NULL, nests,
			true, false, false);
	if (parsres < 0) {
		Error1("option too long:  \"%s\"", tokp);
		return -1;
	} else if (parsres > 0) {
		Error1("syntax error in \"%s\"", tokp);
		return -1;
	}
	*buffp++ = '\0';
	if ((opt->value2.u_string/*param2*/ = strdup(buff)) == NULL) {
	   Error1("strdup(\"%s\"): out of memory", buff);
	   free(opt->value.u_string);
	   return -1;
	}


#if HAVE_STRUCT_IP_MREQN
	if (*tokp++ == ':') {
		strncpy(opt->value3.u_string/*ifindex*/, tokp, IF_NAMESIZE);	/* ok */
		Info4("setting option \"%s\" to {\"%s\",\"%s\",\"%s\"}",
		      ent->desc->defname,
		      opt->value.u_string/*multiaddr*/,
		      opt->value2.u_string/*param2*/,
		      opt->value3.u_string/*ifindex*/);
	} else {
		/*0 opt->value3.u_string = NULL; / * is NULL from init */
		Info3("setting option \"%s\" to {\"%s\",\"%s\"}",
		      ent->desc->defname,
		      opt->value.u_string/*multiaddr*/,
		      opt->value2.u_string/*param2*/);
	}
#else /* !HAVE_STRUCT_IP_MREQN */
	Info3("setting option \"%s\" to {\"%s\",\"%s\"}",
	      ent->desc->defname,
	      opt->value.u_string/*multiaddr*/,
	      opt->value2.u_string/*param2*/);
#endif /* !HAVE_STRUCT_IP_MREQN */
	return 0;
}
#endif /* defined(HAVE_STRUCT_IP_MREQ) || defined (HAVE_STRUCT_IP_MREQN) */

#if defined(HAVE_STRUCT_IP_MREQ) || defined (HAVE_STRUCT_IP_MREQN)
int xioapply_ip_add_membership(
	struct single *sfd,
	struct opt *opt)
{
	union {
#if HAVE_STRUCT_IP_MREQN
		struct ip_mreqn mreqn;
#endif
		struct ip_mreq  mreq;
	} ip4_mreqn = {{{0}}};
	/* IPv6 not supported - seems to have different handling */
/*
mc:addr:ifname|ifind
mc:ifname|ifind
mc:addr
*/
	union sockaddr_union sockaddr1;
	socklen_t socklen1 = sizeof(sockaddr1.ip4);
	union sockaddr_union sockaddr2;
	socklen_t socklen2 = sizeof(sockaddr2.ip4);

	/* First parameter is always multicast address */
	/*! result */
	xioresolve(opt->value.u_string/*multiaddr*/, NULL,
		   sfd->para.socket.la.soa.sa_family,
		   SOCK_DGRAM, IPPROTO_IP, &sockaddr1, &socklen1,
		   sfd->para.socket.ip.ai_flags);
	ip4_mreqn.mreq.imr_multiaddr = sockaddr1.ip4.sin_addr;
	if (0) {
		;	/* for canonical reasons */
#if HAVE_STRUCT_IP_MREQN
	} else if (opt->value3.u_string/*ifindex*/ != NULL) {
		/* three parameters */
		/* second parameter is interface address */
		xioresolve(opt->value2.u_string/*param2*/, NULL,
			   sfd->para.socket.la.soa.sa_family,
			   SOCK_DGRAM, IPPROTO_IP, &sockaddr2, &socklen2,
			   sfd->para.socket.ip.ai_flags);
		ip4_mreqn.mreq.imr_interface = sockaddr2.ip4.sin_addr;
		/* third parameter is interface */
		if (ifindex(opt->value3.u_string/*ifindex*/,
			    (unsigned int *)&ip4_mreqn.mreqn.imr_ifindex, -1)
		    < 0) {
			Error1("cannot resolve interface \"%s\"",
			       opt->value3.u_string/*ifindex*/);
		}
#endif /* HAVE_STRUCT_IP_MREQN */
	} else {
		/* two parameters */
		if (0) {
			;	/* for canonical reasons */
#if HAVE_STRUCT_IP_MREQN
			/* there is a form with two parameters that uses mreqn */
		} else if (ifindex(opt->value2.u_string/*param2*/,
				   (unsigned int *)&ip4_mreqn.mreqn.imr_ifindex,
				   -1)
			   >= 0) {
			/* yes, second param converts to interface */
			ip4_mreqn.mreq.imr_interface.s_addr = htonl(0);
#endif /* HAVE_STRUCT_IP_MREQN */
		} else {
			/*! result */
			xioresolve(opt->value2.u_string/*param2*/, NULL,
				   sfd->para.socket.la.soa.sa_family,
				   SOCK_DGRAM, IPPROTO_IP,
				   &sockaddr2, &socklen2,
				   sfd->para.socket.ip.ai_flags);
			ip4_mreqn.mreq.imr_interface = sockaddr2.ip4.sin_addr;
		}
	}

#if LATER
	if (0) {
		; /* for canonical reasons */
	} else if (sfd->para.socket.la.soa.sa_family == PF_INET) {
	} else if (sfd->para.socket.la.soa.sa_family == PF_INET6) {
		ip6_mreqn.mreq.imr_multiaddr = sockaddr1.ip6.sin6_addr;
		ip6_mreqn.mreq.imr_interface = sockaddr2.ip6.sin6_addr;
	}
#endif

#if HAVE_STRUCT_IP_MREQN
	if (Setsockopt(sfd->fd, opt->desc->major, opt->desc->minor,
		       &ip4_mreqn.mreqn, sizeof(ip4_mreqn.mreqn)) < 0) {
		Error8("setsockopt(%d, %d, %d, {0x%08x,0x%08x,%d}, "F_Zu"): %s",
		       sfd->fd, opt->desc->major, opt->desc->minor,
		       ip4_mreqn.mreqn.imr_multiaddr.s_addr,
		       ip4_mreqn.mreqn.imr_address.s_addr,
		       ip4_mreqn.mreqn.imr_ifindex,
		       sizeof(ip4_mreqn.mreqn),
		       strerror(errno));
		opt->desc = ODESC_ERROR;
		return -1;
	}
#else
	if (Setsockopt(sfd->fd, opt->desc->major, opt->desc->minor,
		       &ip4_mreqn.mreq, sizeof(ip4_mreqn.mreq)) < 0) {
		Error7("setsockopt(%d, %d, %d, {0x%08x,0x%08x}, "F_Zu"): %s",
		       sfd->fd, opt->desc->major, opt->desc->minor,
		       ip4_mreqn.mreq.imr_multiaddr,
		       ip4_mreqn.mreq.imr_interface,
		       sizeof(ip4_mreqn.mreq),
		       strerror(errno));
		opt->desc = ODESC_ERROR;
		return -1;
	}
#endif
	return 0;
}
#endif /* defined(HAVE_STRUCT_IP_MREQ) || defined (HAVE_STRUCT_IP_MREQN) */


#if HAVE_STRUCT_IP_MREQ_SOURCE
int xiotype_ip_add_source_membership(char *token, const struct optname *ent, struct opt *opt) {
   /* we do not resolve the addresses here because we do not yet know
      if we are coping with an IPv4 or IPv6 socat address */
   const char *ends[] = { ":", NULL };
   const char *nests[] = { "[","]", NULL };
   char buff[512], *buffp=buff; size_t bufspc = sizeof(buff)-1;
   char *tokp = token;
   int parsres;

   /* parse first IP address, expect ':' */
   parsres =
      nestlex((const char **)&tokp, &buffp, &bufspc,
	      ends, NULL, NULL, nests,
	      true, false, false);
   if (parsres < 0) {
      Error1("option too long:  \"%s\"", token);
      return -1;
   } else if (parsres > 0) {
      Error1("syntax error in \"%s\"", token);
      return -1;
   }
   if (*tokp != ':') {
      Error1("syntax in option %s: missing ':'", token);
   }
   *buffp++ = '\0';
   if ((opt->value.u_string/*mcaddr*/ = strdup(buff)) == NULL) {
      Error1("strdup(\"%s\"): out of memory", buff);
      return -1;
   }

   ++tokp;
   /* parse second IP address, expect ':' or '\0'' */
   buffp = buff;
   /*! result= */
   parsres =
      nestlex((const char **)&tokp, &buffp, &bufspc,
	      ends, NULL, NULL, nests,
	      true, false, false);
   if (parsres < 0) {
      Error1("option too long:  \"%s\"", token);
      return -1;
   } else if (parsres > 0) {
      Error1("syntax error in \"%s\"", token);
      return -1;
   }
   if (*tokp != ':') {
      Error1("syntax in option %s: missing ':'", token);
   }
   *buffp++ = '\0';
   if ((opt->value2.u_string/*ifaddr*/ = strdup(buff)) == NULL) {
      Error1("strdup(\"%s\"): out of memory", buff);
      free(opt->value.u_string);
      return -1;
   }

   ++tokp;
   /* parse third IP address, expect ':' or '\0'' */
   buffp = buff;
   /*! result= */
   parsres =
      nestlex((const char **)&tokp, &buffp, &bufspc,
	      ends, NULL, NULL, nests,
	      true, false, false);
   if (parsres < 0) {
      Error1("option too long:  \"%s\"", token);
      return -1;
   } else if (parsres > 0) {
      Error1("syntax error in \"%s\"", token);
      return -1;
   }
   if (*tokp) {
      Error1("syntax in option %s: trailing cruft", token);
   }
   *buffp++ = '\0';
   if ((opt->value3.u_string/*srcaddr*/ = strdup(buff)) == NULL) {
      Error1("strdup(\"%s\"): out of memory", buff);
      free(opt->value.u_string);
      free(opt->value2.u_string);
      return -1;
   }

   Info4("setting option \"%s\" to {0x%08x,0x%08x,0x%08x}",
	 ent->desc->defname,
	 ntohl(*(unsigned int *)opt->value.u_string/*mcaddr*/),
	 ntohl(*(unsigned int *)opt->value2.u_string/*ifaddr*/),
	 ntohl(*(unsigned int *)opt->value3.u_string/*srcaddr*/));
   return 0;
}

int xioapply_ip_add_source_membership(struct single *sfd, struct opt *opt) {
   struct ip_mreq_source ip4_mreq_src = {{0}};
   /* IPv6 not supported - seems to have different handling */
   union sockaddr_union sockaddr1;
   socklen_t socklen1 = sizeof(sockaddr1.ip4);
   union sockaddr_union sockaddr2;
   socklen_t socklen2 = sizeof(sockaddr2.ip4);
   union sockaddr_union sockaddr3;
   socklen_t socklen3 = sizeof(sockaddr3.ip4);
   int rc;

   /* first parameter is always multicast address */
   rc = xioresolve(opt->value.u_string/*mcaddr*/, NULL,
		   sfd->para.socket.la.soa.sa_family,
		   SOCK_DGRAM, IPPROTO_IP,
		   &sockaddr1, &socklen1, sfd->para.socket.ip.ai_flags);
   if (rc < 0) {
      return -1;
   }
   ip4_mreq_src.imr_multiaddr = sockaddr1.ip4.sin_addr;
   /* second parameter is interface address */
   rc = xioresolve(opt->value.u_string/*ifaddr*/, NULL,
		   sfd->para.socket.la.soa.sa_family,
		   SOCK_DGRAM, IPPROTO_IP,
		   &sockaddr2, &socklen2, sfd->para.socket.ip.ai_flags);
   if (rc < 0) {
      return -1;
   }
   ip4_mreq_src.imr_interface = sockaddr2.ip4.sin_addr;
   /* third parameter is source address */
   rc = xioresolve(opt->value.u_string/*srcaddr*/, NULL,
		   sfd->para.socket.la.soa.sa_family,
		   SOCK_DGRAM, IPPROTO_IP,
		   &sockaddr3, &socklen3, sfd->para.socket.ip.ai_flags);
   if (rc < 0) {
      return -1;
   }
   ip4_mreq_src.imr_sourceaddr = sockaddr3.ip4.sin_addr;
   if (Setsockopt(sfd->fd, opt->desc->major, opt->desc->minor,
		  &ip4_mreq_src, sizeof(ip4_mreq_src)) < 0) {
      Error8("setsockopt(%d, %d, %d, {0x%08x,0x%08x,0x%08x}, "F_Zu"): %s",
	     sfd->fd, opt->desc->major, opt->desc->minor,
	     htonl((uint32_t)ip4_mreq_src.imr_multiaddr.s_addr),
	     ip4_mreq_src.imr_interface.s_addr,
	     ip4_mreq_src.imr_sourceaddr.s_addr,
	     sizeof(struct ip_mreq_source),
	     strerror(errno));
      opt->desc = ODESC_ERROR;
      return -1;
   }
   return 0;
}

#endif /* HAVE_STRUCT_IP_MREQ_SOURCE */


#if WITH_RESOLVE
#if HAVE_RESOLV_H

/* When there are options for resolver then this function saves the current
   resolver settings to save_res and applies the options to resolver libs state
   in _res.
   Returns 1 when there were options (state needs to be restored later, see
   xio_res_restore());
   Returns 0 when there were no options;
   Returns -1 on error. */
int xio_res_init(
	struct single *sfd,
	struct __res_state *save_res)
{
	if (sfd->para.socket.ip.res.opts[0] ||
	    sfd->para.socket.ip.res.opts[1] ||
#if HAVE_RES_RETRANS
	    sfd->para.socket.ip.res.retrans >= 0 ||
#endif
#if HAVE_RES_RETRY
	    sfd->para.socket.ip.res.retry >= 0 ||
#endif
#if HAVE_RES_NSADDR_LIST
	    sfd->para.socket.ip.res.nsaddr.sin_family != PF_UNSPEC ||
#endif
	    0 	/* for canonical reasons */
		) {
		if (!(_res.options & RES_INIT)) {
			if (Res_init() < 0) {
				Error("res_init() failed");
				return -1;
			}
		}
		*save_res = _res;
		_res.options |=  sfd->para.socket.ip.res.opts[0];
		_res.options &= ~sfd->para.socket.ip.res.opts[1];
		Debug2("changed _res.options from 0x%lx to 0x%lx",
		       save_res->options, _res.options);

#if HAVE_RES_RETRANS
		if (sfd->para.socket.ip.res.retrans >= 0) {
			_res.retrans = sfd->para.socket.ip.res.retrans;
			Debug2("changed _res.retrans from 0x%x to 0x%x",
			       save_res->retrans, _res.retrans);
		}
#endif
#if HAVE_RES_RETRY
		if (sfd->para.socket.ip.res.retry >= 0) {
			_res.retry = sfd->para.socket.ip.res.retry;
			Debug2("changed _res.retry from 0x%x to 0x%x",
			       save_res->retry, _res.retry);
		}
#endif
#if HAVE_RES_NSADDR_LIST
		if (sfd->para.socket.ip.res.nsaddr.sin_family == PF_INET) {
			_res.nscount = 1;
			_res.nsaddr_list[0] = sfd->para.socket.ip.res.nsaddr;
			if (_res.nsaddr_list[0].sin_port == htons(0))
				_res.nsaddr_list[0].sin_port = htons(53);
			Debug10("changed _res.nsaddr_list[0] from %u.%u.%u.%u:%u to %u.%u.%u.%u:%u",
				((unsigned char *)&save_res->nsaddr_list[0].sin_addr.s_addr)[0],
				((unsigned char *)&save_res->nsaddr_list[0].sin_addr.s_addr)[1],
				((unsigned char *)&save_res->nsaddr_list[0].sin_addr.s_addr)[2],
				((unsigned char *)&save_res->nsaddr_list[0].sin_addr.s_addr)[3],
				ntohs(save_res->nsaddr_list[0].sin_port),
				((unsigned char *)&_res.nsaddr_list[0].sin_addr.s_addr)[0],
				((unsigned char *)&_res.nsaddr_list[0].sin_addr.s_addr)[1],
				((unsigned char *)&_res.nsaddr_list[0].sin_addr.s_addr)[2],
				((unsigned char *)&_res.nsaddr_list[0].sin_addr.s_addr)[3],
				ntohs(_res.nsaddr_list[0].sin_port));
		}
#endif /* HAVE_RES_NSADDR_LIST */

		return 1;
	}

	return 0;
}

int xio_res_restore(
	struct __res_state *save_res)
{
	_res = *save_res;
	return 0;
}
#endif /* HAVE_RESOLV_H */
#endif /* WITH_RESOLVE */

#endif /* _WITH_IP4 || _WITH_IP6 */
