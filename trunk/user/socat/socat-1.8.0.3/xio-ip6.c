/* source: xio-ip6.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for IP6 related functions */

#include "xiosysincludes.h"

#if WITH_IP6

#include "xioopen.h"
#include "xio-ascii.h"
#include "xio-socket.h"
#include "xio-ip.h"	/* xioresolve() */

#include "xio-ip6.h"
#include "nestlex.h"


static char *inet6addr_info(const struct in6_addr *sa, char *buff, size_t blen);


#ifdef IPV6_V6ONLY
const struct optdesc opt_ipv6_v6only = { "ipv6-v6only", "ipv6only", OPT_IPV6_V6ONLY, GROUP_SOCK_IP6, PH_PREBIND, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_V6ONLY };
#endif
#if defined(HAVE_STRUCT_IP_MREQ) || defined(HAVE_STRUCT_IP_MREQN)
const struct optdesc opt_ipv6_join_group = { "ipv6-join-group", "join-group", OPT_IPV6_JOIN_GROUP, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_IP_MREQN, OFUNC_SPEC, SOL_IPV6, IPV6_JOIN_GROUP };
#endif
#ifdef MCAST_JOIN_SOURCE_GROUP
const struct optdesc opt_ipv6_join_source_group = { "ipv6-join-source-group", "join-source-group", OPT_IPV6_JOIN_SOURCE_GROUP, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_GROUP_SOURCE_REQ, OFUNC_SPEC, SOL_IPV6, MCAST_JOIN_SOURCE_GROUP };
#endif
#ifdef IPV6_PKTINFO
const struct optdesc opt_ipv6_pktinfo = { "ipv6-pktinfo", "pktinfo", OPT_IPV6_PKTINFO, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_PKTINFO };
#endif
#ifdef IPV6_RECVPKTINFO
const struct optdesc opt_ipv6_recvpktinfo = { "ipv6-recvpktinfo", "recvpktinfo", OPT_IPV6_RECVPKTINFO, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVPKTINFO };
#endif
#ifdef IPV6_RTHDR
const struct optdesc opt_ipv6_rthdr   = { "ipv6-rthdr",   "rthdr",   OPT_IPV6_RTHDR,   GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RTHDR };
#endif
#ifdef IPV6_RECVRTHDR
const struct optdesc opt_ipv6_recvrthdr   = { "ipv6-recvrthdr",   "recvrthdr",   OPT_IPV6_RECVRTHDR,   GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVRTHDR };
#endif
#ifdef IPV6_AUTHHDR
const struct optdesc opt_ipv6_authhdr = { "ipv6-authhdr", "authhdr", OPT_IPV6_AUTHHDR, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_AUTHHDR };
#endif
#ifdef IPV6_DSTOPTS
const struct optdesc opt_ipv6_dstopts = { "ipv6-dstopts", "dstopts", OPT_IPV6_DSTOPTS, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_DSTOPTS };
#endif
#ifdef IPV6_RECVDSTOPTS
const struct optdesc opt_ipv6_recvdstopts = { "ipv6-recvdstopts", "recvdstopts", OPT_IPV6_RECVDSTOPTS, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVDSTOPTS };
#endif
#ifdef IPV6_HOPOPTS
const struct optdesc opt_ipv6_hopopts = { "ipv6-hopopts", "hopopts", OPT_IPV6_HOPOPTS, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_HOPOPTS };
#endif
#ifdef IPV6_RECVHOPOPTS
const struct optdesc opt_ipv6_recvhopopts = { "ipv6-recvhopopts", "recvhopopts", OPT_IPV6_RECVHOPOPTS, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVHOPOPTS };
#endif
#ifdef IPV6_FLOWINFO /* is in linux/in6.h */
const struct optdesc opt_ipv6_flowinfo= { "ipv6-flowinfo","flowinfo",OPT_IPV6_FLOWINFO,GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_FLOWINFO };
#endif
#ifdef IPV6_HOPLIMIT
const struct optdesc opt_ipv6_hoplimit= { "ipv6-hoplimit","hoplimit",OPT_IPV6_HOPLIMIT,GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_HOPLIMIT };
#endif
const struct optdesc opt_ipv6_unicast_hops= { "ipv6-unicast-hops","unicast-hops",OPT_IPV6_UNICAST_HOPS,GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_UNICAST_HOPS };
#ifdef IPV6_RECVHOPLIMIT
const struct optdesc opt_ipv6_recvhoplimit= { "ipv6-recvhoplimit","recvhoplimit",OPT_IPV6_RECVHOPLIMIT,GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVHOPLIMIT };
#endif
#ifdef IPV6_RECVERR
const struct optdesc opt_ipv6_recverr = { "ipv6-recverr", "recverr", OPT_IPV6_RECVERR, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVERR };
#endif
#ifdef IPV6_TCLASS
const struct optdesc opt_ipv6_tclass     = { "ipv6-tclass",     "tclass",     OPT_IPV6_TCLASS,     GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_IPV6, IPV6_TCLASS };
#endif
#ifdef IPV6_RECVTCLASS
const struct optdesc opt_ipv6_recvtclass = { "ipv6-recvtclass", "recvtclass", OPT_IPV6_RECVTCLASS, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVTCLASS };
#endif
#ifdef IPV6_RECVPATHMTU
const struct optdesc opt_ipv6_recvpathmtu = { "ipv6-recvpathmtu", "recvpathmtu", OPT_IPV6_RECVPATHMTU, GROUP_SOCK_IP6, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_IPV6, IPV6_RECVPATHMTU };
#endif

/* Returns canonical form of IPv6 address.
   IPv6 address may be enclose in brackets.
   Returns STAT_OK on success, STAT_NORETRY on failure. */
int xioip6_pton(
	const char *src,
	struct in6_addr *dst,
	const int ai_flags[2])
{
   union sockaddr_union sockaddr;
   socklen_t sockaddrlen = sizeof(sockaddr);

   if (src[0] == '[') {
      char plainaddr[INET6_ADDRSTRLEN];
      char *clos;

      strncpy(plainaddr, src+1, INET6_ADDRSTRLEN);
      plainaddr[INET6_ADDRSTRLEN-1] = '\0';
      if ((clos = strchr(plainaddr, ']')) != NULL)
	 *clos = '\0';
      return xioip6_pton(plainaddr, dst, ai_flags);
   }
   if (xioresolve(src, NULL, PF_INET6, 0, 0, &sockaddr, &sockaddrlen,
		  ai_flags)
       != STAT_OK) {
      return STAT_NORETRY;
   }
   *dst = sockaddr.ip6.sin6_addr;
   return STAT_OK;
}

int xioparsenetwork_ip6(
	const char *rangename,
	struct xiorange *range,
	const int ai_flags[2])
{
   char *delimpos;	/* absolute address of delimiter */
   size_t delimind;	/* index of delimiter in string */
   unsigned int bits;	/* netmask bits */
   char *endptr;
   char *baseaddr;
   union sockaddr_union sockaddr;
   socklen_t sockaddrlen = sizeof(sockaddr);
   union xioin6_u *rangeaddr = (union xioin6_u *)&range->netaddr.ip6.sin6_addr;
   union xioin6_u *rangemask = (union xioin6_u *)&range->netmask.ip6.sin6_addr;
   union xioin6_u *nameaddr = (union xioin6_u *)&sockaddr.ip6.sin6_addr;

   if ((delimpos = strchr(rangename, '/')) == NULL) {
      Error1("xioparsenetwork_ip6(\"%s\",,): missing mask bits delimiter '/'",
	     rangename);
      return STAT_NORETRY;
   }
   delimind = delimpos - rangename;
   if (rangename[0] != '[' || rangename[delimind-1] != ']') {
      Error1("missing brackets for IPv6 range definition \"%s\"",
	     rangename);
      return STAT_NORETRY;
   }

   if ((baseaddr = strndup(rangename+1,delimind-2)) == NULL) {
      Error1("strdup(\"%s\"): out of memory", rangename+1);
      return STAT_NORETRY;
   }
   baseaddr[delimind-2] = '\0';
   if (xioresolve(baseaddr, NULL, PF_INET6, 0, 0, &sockaddr, &sockaddrlen,
		  ai_flags)
       != STAT_OK) {
      return STAT_NORETRY;
   }
   rangeaddr->u6_addr32[0] = nameaddr->u6_addr32[0];
   rangeaddr->u6_addr32[1] = nameaddr->u6_addr32[1];
   rangeaddr->u6_addr32[2] = nameaddr->u6_addr32[2];
   rangeaddr->u6_addr32[3] = nameaddr->u6_addr32[3];
   bits = strtoul(delimpos+1, &endptr, 10);
   if (! ((*(delimpos+1) != '\0') && (*endptr == '\0'))) {
      Error1("not a valid netmask in \"%s\"", rangename);
      bits = 128;	/* most secure selection */
   } else if (bits > 128) {
      Error1("netmask \"%s\" is too large", rangename);
      bits = 128;
   }

   /* I am starting to dislike C...uint32_t << 32 is undefined... */
   if (bits == 0) {
      rangemask->u6_addr32[0] = 0;
      rangemask->u6_addr32[1] = 0;
      rangemask->u6_addr32[2] = 0;
      rangemask->u6_addr32[3] = 0;
   } else if (bits <= 32) {
      rangemask->u6_addr32[0] = htonl(0xffffffff << (32-bits));
      rangemask->u6_addr32[1] = 0;
      rangemask->u6_addr32[2] = 0;
      rangemask->u6_addr32[3] = 0;
   } else if (bits <= 64) {
      rangemask->u6_addr32[0] = 0xffffffff;
      rangemask->u6_addr32[1] = htonl(0xffffffff << (64-bits));
      rangemask->u6_addr32[2] = 0;
      rangemask->u6_addr32[3] = 0;
   } else if (bits <= 96) {
      rangemask->u6_addr32[0] = 0xffffffff;
      rangemask->u6_addr32[1] = 0xffffffff;
      rangemask->u6_addr32[2] = htonl(0xffffffff << (96-bits));
      rangemask->u6_addr32[3] = 0;
   } else {
      rangemask->u6_addr32[0] = 0xffffffff;
      rangemask->u6_addr32[1] = 0xffffffff;
      rangemask->u6_addr32[2] = 0xffffffff;
      rangemask->u6_addr32[3] = htonl(0xffffffff << (128-bits));
   }
   return 0;
}

int xiorange_ip6andmask(struct xiorange *range) {
   int i;
#if 0
   range->addr.s6_addr32[0] &= range->mask.s6_addr32[0];
   range->addr.s6_addr32[1] &= range->mask.s6_addr32[1];
   range->addr.s6_addr32[2] &= range->mask.s6_addr32[2];
   range->addr.s6_addr32[3] &= range->mask.s6_addr32[3];
#else
   for (i = 0; i < 16; ++i) {
      range->netaddr.ip6.sin6_addr.s6_addr[i] &=
	 range->netmask.ip6.sin6_addr.s6_addr[i];
   }
#endif
   return 0;
}

/* check if peer address is within permitted range.
   return >= 0 if so. */
int xiocheckrange_ip6(struct sockaddr_in6 *pa, struct xiorange *range) {
   union xioin6_u masked;
   int i;
   char peername[256];
   union xioin6_u *rangeaddr = (union xioin6_u *)&range->netaddr.ip6.sin6_addr;
   union xioin6_u *rangemask = (union xioin6_u *)&range->netmask.ip6.sin6_addr;

   Debug16("permitted client subnet: [%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]:[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]",
	   htons(rangeaddr->u6_addr16[0]),  htons(rangeaddr->u6_addr16[1]),
	   htons(rangeaddr->u6_addr16[2]),  htons(rangeaddr->u6_addr16[3]),
	   htons(rangeaddr->u6_addr16[4]),  htons(rangeaddr->u6_addr16[5]),
	   htons(rangeaddr->u6_addr16[6]),  htons(rangeaddr->u6_addr16[7]),
	   htons(rangemask->u6_addr16[0]),  htons(rangemask->u6_addr16[1]),
	   htons(rangemask->u6_addr16[2]),  htons(rangemask->u6_addr16[3]),
	   htons(rangemask->u6_addr16[4]),  htons(rangemask->u6_addr16[5]),
	   htons(rangemask->u6_addr16[6]),  htons(rangemask->u6_addr16[7]));
   Debug1("client address is %s",
	  sockaddr_inet6_info(pa, peername, sizeof(peername)));

   for (i = 0; i < 4; ++i) {
      masked.u6_addr32[i] = ((union xioin6_u *)&pa->sin6_addr.s6_addr[0])->u6_addr32[i] & rangemask->u6_addr32[i];
   }
   Debug8("masked address is [%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]",
	   htons(masked.u6_addr16[0]),  htons(masked.u6_addr16[1]),
	   htons(masked.u6_addr16[2]),  htons(masked.u6_addr16[3]),
	   htons(masked.u6_addr16[4]),  htons(masked.u6_addr16[5]),
	   htons(masked.u6_addr16[6]),  htons(masked.u6_addr16[7]));

   if (masked.u6_addr32[0] != rangeaddr->u6_addr32[0] ||
       masked.u6_addr32[1] != rangeaddr->u6_addr32[1] ||
       masked.u6_addr32[2] != rangeaddr->u6_addr32[2] ||
       masked.u6_addr32[3] != rangeaddr->u6_addr32[3]) {
      Debug1("client address %s is not permitted", peername);
      return -1;
   }
   return 0;
}


#if defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA)
/* provides info about the ancillary message:
   converts the ancillary message in *cmsg into a form usable for further
   processing. knows the specifics of common message types.
   returns the number of resulting syntax elements in *num
   returns a sequence of \0 terminated type strings in *typbuff
   returns a sequence of \0 terminated name strings in *nambuff
   returns a sequence of \0 terminated value strings in *valbuff
   the respective len parameters specify the available space in the buffers
   returns STAT_OK on success
 */
int xiolog_ancillary_ip6(
	struct single *sfd,
	struct cmsghdr *cmsg,
	int *num,
	char *typbuff, int typlen,
	char *nambuff, int namlen,
	char *envbuff, int envlen,
	char *valbuff, int vallen)
{
   char scratch1[42];	/* can hold an IPv6 address in ASCII */
   char scratch2[32];
   size_t msglen;

   *num = 1;	/* good for most message types */
   msglen = cmsg->cmsg_len-((char *)CMSG_DATA(cmsg)-(char *)cmsg);
      envbuff[0] = '\0';
   switch (cmsg->cmsg_type) {
#if defined(IPV6_PKTINFO) && HAVE_STRUCT_IN6_PKTINFO
   case IPV6_PKTINFO: {
      struct in6_pktinfo *pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
      *num = 2;
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_PKTINFO", typlen-1);
      snprintf(nambuff, namlen, "%s%c%s", "dstaddr", '\0', "if");
      snprintf(envbuff, envlen, "%s%c%s", "IPV6_DSTADDR", '\0', "IPV6_IF");
      snprintf(valbuff, vallen, "%s%c%s",
	       inet6addr_info(&pktinfo->ipi6_addr, scratch1, sizeof(scratch1)),
	       '\0', xiogetifname(pktinfo->ipi6_ifindex, scratch2, -1));
   }
      return STAT_OK;
#endif /* defined(IPV6_PKTINFO) && HAVE_STRUCT_IN6_PKTINFO */
#ifdef IPV6_HOPLIMIT
   case IPV6_HOPLIMIT:
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_HOPLIMIT", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "hoplimit", namlen-1);
      {
	 int *intp = (int *)CMSG_DATA(cmsg);
	 snprintf(valbuff, vallen, "%d", *intp);
      }
      return STAT_OK;
#endif /* defined(IPV6_HOPLIMIT) */
#ifdef IPV6_RTHDR
   case IPV6_RTHDR:
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_RTHDR", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "rthdr", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#endif /* defined(IPV6_RTHDR) */
#ifdef IPV6_AUTHHDR
   case IPV6_AUTHHDR:
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_AUTHHDR", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "authhdr", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#endif
#ifdef IPV6_DSTOPTS
   case IPV6_DSTOPTS:
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_DSTOPTS", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "dstopts", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#endif /* defined(IPV6_DSTOPTS) */
#ifdef IPV6_HOPOPTS
   case IPV6_HOPOPTS:
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_HOPOPTS", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "hopopts", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#endif /* defined(IPV6_HOPOPTS) */
#ifdef IPV6_FLOWINFO
   case IPV6_FLOWINFO:
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_FLOWINFO", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "flowinfo", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#endif
#ifdef IPV6_TCLASS
   case IPV6_TCLASS: {
      unsigned int u;
      typbuff[0] = '\0'; strncat(typbuff, "IPV6_TCLASS", typlen-1);
      nambuff[0] = '\0'; strncat(nambuff, "tclass", namlen-1);
      u = ntohl(*(unsigned int *)CMSG_DATA(cmsg));
      xiodump((const unsigned char *)&u, msglen, valbuff, vallen, 0);
      return STAT_OK;
   }
#endif
   default:
      snprintf(typbuff, typlen, "IPV6.%u", cmsg->cmsg_type);
      nambuff[0] = '\0'; strncat(nambuff, "data", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
   }
   return STAT_OK;
}
#endif /* defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA) */


/* convert the IP6 socket address to human readable form. buff should be at
   least 50 chars long. output includes the port number */
static char *inet6addr_info(const struct in6_addr *sa, char *buff, size_t blen) {
   if (xio_snprintf(buff, blen, "[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]",
#if HAVE_IP6_SOCKADDR==0
		(sa->s6_addr[0]<<8)+sa->s6_addr[1],
		(sa->s6_addr[2]<<8)+sa->s6_addr[3],
		(sa->s6_addr[4]<<8)+sa->s6_addr[5],
		(sa->s6_addr[6]<<8)+sa->s6_addr[7],
		(sa->s6_addr[8]<<8)+sa->s6_addr[9],
		(sa->s6_addr[10]<<8)+sa->s6_addr[11],
		(sa->s6_addr[12]<<8)+sa->s6_addr[13],
		(sa->s6_addr[14]<<8)+sa->s6_addr[15]
#elif HAVE_IP6_SOCKADDR==1
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->u6_addr.u6_addr16)[7])
#elif HAVE_IP6_SOCKADDR==2
		ntohs(((unsigned short *)&sa->u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->u6_addr16)[7])
#elif HAVE_IP6_SOCKADDR==3
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->in6_u.u6_addr16)[7])
#elif HAVE_IP6_SOCKADDR==4
		(sa->_S6_un._S6_u8[0]<<8)|(sa->_S6_un._S6_u8[1]&0xff),
		(sa->_S6_un._S6_u8[2]<<8)|(sa->_S6_un._S6_u8[3]&0xff),
		(sa->_S6_un._S6_u8[4]<<8)|(sa->_S6_un._S6_u8[5]&0xff),
		(sa->_S6_un._S6_u8[6]<<8)|(sa->_S6_un._S6_u8[7]&0xff),
		(sa->_S6_un._S6_u8[8]<<8)|(sa->_S6_un._S6_u8[9]&0xff),
		(sa->_S6_un._S6_u8[10]<<8)|(sa->_S6_un._S6_u8[11]&0xff),
		(sa->_S6_un._S6_u8[12]<<8)|(sa->_S6_un._S6_u8[13]&0xff),
		(sa->_S6_un._S6_u8[14]<<8)|(sa->_S6_un._S6_u8[15]&0xff)
#elif HAVE_IP6_SOCKADDR==5
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[0]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[1]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[2]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[3]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[4]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[5]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[6]),
		ntohs(((unsigned short *)&sa->__u6_addr.__u6_addr16)[7])
#endif
		) >= blen) {
      Warn("sockaddr_inet6_info(): buffer too short");
      buff[blen-1] = '\0';
   }
   return buff;
}


/* returns information that can be used for constructing an environment
   variable describing the socket address.
   if idx is 0, this function writes "ADDR" into namebuff and the IP address
   into valuebuff, and returns 1 (which means that one more info is there).
   if idx is 1, it writes "PORT" into namebuff and the port number into
   valuebuff, and returns 0 (no more info)
   namelen and valuelen contain the max. allowed length of output chars in the
   respective buffer.
   on error this function returns -1.
*/
int
xiosetsockaddrenv_ip6(int idx, char *namebuff, size_t namelen,
		      char *valuebuff, size_t valuelen,
		      struct sockaddr_in6 *sa, int ipproto) {
   switch (idx) {
   case 0:
      strcpy(namebuff, "ADDR");
      snprintf(valuebuff, valuelen, "[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]",
	       (sa->sin6_addr.s6_addr[0]<<8)+
	       sa->sin6_addr.s6_addr[1],
	       (sa->sin6_addr.s6_addr[2]<<8)+
	       sa->sin6_addr.s6_addr[3],
	       (sa->sin6_addr.s6_addr[4]<<8)+
	       sa->sin6_addr.s6_addr[5],
	       (sa->sin6_addr.s6_addr[6]<<8)+
	       sa->sin6_addr.s6_addr[7],
	       (sa->sin6_addr.s6_addr[8]<<8)+
	       sa->sin6_addr.s6_addr[9],
	       (sa->sin6_addr.s6_addr[10]<<8)+
	       sa->sin6_addr.s6_addr[11],
	       (sa->sin6_addr.s6_addr[12]<<8)+
	       sa->sin6_addr.s6_addr[13],
	       (sa->sin6_addr.s6_addr[14]<<8)+
	       sa->sin6_addr.s6_addr[15]);
      switch (ipproto) {
      case IPPROTO_TCP:
      case IPPROTO_UDP:
#ifdef IPPROTO_SCTP
      case IPPROTO_SCTP:
#endif
	 return 1;	/* there is port information to also be retrieved */
      default:
	 return 0;	/* no port info coming */
      }
   case 1:
      strcpy(namebuff, "PORT");
      snprintf(valuebuff, valuelen, "%u", ntohs(sa->sin6_port));
      return 0;
   }
   return -1;
}


#if defined(HAVE_STRUCT_IPV6_MREQ)
int xioapply_ipv6_join_group(
	struct single *sfd,
	struct opt *opt)
{
	struct ipv6_mreq ip6_mreq = {{{{0}}}};
	union sockaddr_union sockaddr1;
	socklen_t socklen1 = sizeof(sockaddr1.ip6);
	int res;

	/* Always two parameters */
	/* First parameter is multicast address */
	if ((res =
	     xioresolve(opt->value.u_string/*multiaddr*/, NULL,
			sfd->para.socket.la.soa.sa_family,
			SOCK_DGRAM, IPPROTO_IP,
			&sockaddr1, &socklen1,
			sfd->para.socket.ip.ai_flags))
	    != STAT_OK) {
	   return res;
	}
	ip6_mreq.ipv6mr_multiaddr = sockaddr1.ip6.sin6_addr;
	if (ifindex(opt->value2.u_string/*param2*/,
		    &ip6_mreq.ipv6mr_interface, -1)
	    < 0) {
		Error1("interface \"%s\" not found",
		       opt->value2.u_string/*param2*/);
		ip6_mreq.ipv6mr_interface = htonl(0);
	}

	if (Setsockopt(sfd->fd, opt->desc->major, opt->desc->minor,
		       &ip6_mreq, sizeof(ip6_mreq)) < 0) {
		Error6("setsockopt(%d, %d, %d, {...,0x%08x}, "F_Zu"): %s",
		       sfd->fd, opt->desc->major, opt->desc->minor,
		       ip6_mreq.ipv6mr_interface,
		       sizeof(ip6_mreq),
		       strerror(errno));
		opt->desc = ODESC_ERROR;
		return -1;
	}
	return 0;
}
#endif /* defined(HAVE_STRUCT_IPV6_MREQ) */

#if HAVE_STRUCT_GROUP_SOURCE_REQ
int xiotype_ip6_join_source_group(
	char *token, const struct optname *ent, struct opt *opt)
{
   /* We do not resolve the addresses here because we do not yet know
      if we are coping with an IPv4 or IPv6 socat address */
   const char *ends[] = { ":", NULL };
   const char *nests[] = { "[","]", NULL };
   char buff[512], *buffp=buff; size_t bufspc = sizeof(buff)-1;
   char *tokp = token;
   int parsres;

   /* Parse first IP address (mcast group), expect ':' */
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
      int _errno = errno;
      Error1("strdup(\"%s\"): out of memory", buff);
      errno = _errno;
      return -1;
   }

   ++tokp;
   /* Parse interface name/index, expect ':' or '\0'' */
   buffp = buff;
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
   if ((opt->value2.u_string/*ifindex*/ = Malloc(IF_NAMESIZE)) == NULL) {
      int _errno = errno;
      free(opt->value.u_string);
      errno = _errno;
      return -1;
   }
   strncpy(opt->value2.u_string/*ifindex*/, buff, IF_NAMESIZE);

   ++tokp;
   /* Parse second IP address (source address), expect ':' or '\0'' */
   buffp = buff;
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
      int _errno = errno;
      Error1("strdup(\"%s\"): out of memory", buff);
      free(opt->value.u_string);
      errno = _errno;
      return -1;
   }

   Info4("setting option \"%s\" to {\"%s\",\"%s\",\"%s\"}",
	 ent->desc->defname,
	 opt->value.u_string/*mcaddr*/,
	 opt->value2.u_string/*ifindex*/,
	 opt->value3.u_string/*srcaddr*/);

   if (!xioparms.experimental) {
      Warn1("option %s is experimental", opt->desc->defname);
   }

   return 0;
}

int xioapply_ip6_join_source_group(struct single *sfd, struct opt *opt) {
   struct group_source_req ip6_gsr = {0};
   union sockaddr_union sockaddr1;
   socklen_t socklen1 = sizeof(sockaddr1.ip6);
   union sockaddr_union sockaddr2;
   socklen_t socklen2 = sizeof(sockaddr2.ip6);
   int res;

   /* First parameter is always multicast address */
   if ((res =
	xioresolve(opt->value.u_string/*mcaddr*/, NULL,
		   sfd->para.socket.la.soa.sa_family,
		   SOCK_DGRAM, IPPROTO_IP, &sockaddr1, &socklen1,
		   sfd->para.socket.ip.ai_flags))
       != STAT_OK) {
      return res;
   }
   memcpy(&ip6_gsr.gsr_group, &sockaddr1.ip6, socklen1);
   /* Second parameter is interface name/index */
   if (ifindex(opt->value2.u_string/*ifindex*/,
	       &ip6_gsr.gsr_interface, -1)
       < 0) {
      Error1("interface \"%s\" not found",
	     opt->value.u_string/*ifindex*/);
      ip6_gsr.gsr_interface = 0;
   }
   /* Third parameter is source address */
   if ((res =
	xioresolve(opt->value3.u_string/*srcaddr*/, NULL,
		   sfd->para.socket.la.soa.sa_family,
		   SOCK_DGRAM, IPPROTO_IP, &sockaddr2, &socklen2,
		   sfd->para.socket.ip.ai_flags))
       != STAT_OK) {
      return res;
   }
   memcpy(&ip6_gsr.gsr_source, &sockaddr2.ip6, socklen2);
   if (Setsockopt(sfd->fd, opt->desc->major, opt->desc->minor,
		  &ip6_gsr, sizeof(ip6_gsr)) < 0) {
      Error6("setsockopt(%d, %d, %d, {%d,...}, "F_Zu"): %s",
	     sfd->fd, opt->desc->major, opt->desc->minor,
	     ip6_gsr.gsr_interface,
	     sizeof(ip6_gsr),
	     strerror(errno));
      opt->desc = ODESC_ERROR;
      return -1;
   }
   return 0;
}
#endif /* HAVE_STRUCT_GROUP_SOURCE_REQ */

#endif /* WITH_IP6 */
