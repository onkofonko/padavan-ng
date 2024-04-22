/* source: xio-interface.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of raw socket type */

#include "xiosysincludes.h"

#if _WITH_INTERFACE

#include "xioopen.h"
#include "xio-socket.h"
#include "xio-ascii.h"

#include "xio-interface.h"


static int xioopen_interface(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

/*0 const struct optdesc opt_interface_addr    = { "interface-addr",    "address", OPT_INTERFACE_ADDR,    GROUP_INTERFACE, PH_FD, TYPE_STRING,   OFUNC_SPEC };*/
/*0 const struct optdesc opt_interface_netmask = { "interface-netmask", "netmask", OPT_INTERFACE_NETMASK, GROUP_INTERFACE, PH_FD, TYPE_STRING,   OFUNC_SPEC };*/
const struct optdesc opt_iff_up          = { "iff-up",          "up",          OPT_IFF_UP,          GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_UP };
const struct optdesc opt_iff_broadcast   = { "iff-broadcast",   NULL,          OPT_IFF_BROADCAST,   GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_BROADCAST };
const struct optdesc opt_iff_debug       = { "iff-debug"    ,   NULL,          OPT_IFF_DEBUG,       GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_DEBUG };
const struct optdesc opt_iff_loopback    = { "iff-loopback" ,   "loopback",    OPT_IFF_LOOPBACK,    GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_LOOPBACK };
const struct optdesc opt_iff_pointopoint = { "iff-pointopoint", "pointopoint",OPT_IFF_POINTOPOINT, GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_POINTOPOINT };
const struct optdesc opt_iff_notrailers  = { "iff-notrailers",  "notrailers",  OPT_IFF_NOTRAILERS,  GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_NOTRAILERS };
const struct optdesc opt_iff_running     = { "iff-running",     "running",     OPT_IFF_RUNNING,     GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_RUNNING };
const struct optdesc opt_iff_noarp       = { "iff-noarp",       "noarp",       OPT_IFF_NOARP,       GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_NOARP };
const struct optdesc opt_iff_promisc     = { "iff-promisc",     "promisc",     OPT_IFF_PROMISC,     GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_PROMISC };
const struct optdesc opt_iff_allmulti    = { "iff-allmulti",    "allmulti",    OPT_IFF_ALLMULTI,    GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_ALLMULTI };
#ifdef IFF_MASTER
const struct optdesc opt_iff_master      = { "iff-master",      "master",      OPT_IFF_MASTER,      GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_MASTER };
#endif
#ifdef IFF_SLAVE
const struct optdesc opt_iff_slave       = { "iff-slave",       "slave",       OPT_IFF_SLAVE,       GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_SLAVE };
#endif
const struct optdesc opt_iff_multicast   = { "iff-multicast",   NULL,          OPT_IFF_MULTICAST,   GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_MULTICAST };
#ifdef IFF_PORTSEL
const struct optdesc opt_iff_portsel     = { "iff-portsel",     "portsel",     OPT_IFF_PORTSEL,     GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_PORTSEL };
#endif
#ifdef IFF_AUTOMEDIA
const struct optdesc opt_iff_automedia   = { "iff-automedia",   "automedia",   OPT_IFF_AUTOMEDIA,   GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(para.interface.iff_opts), IFF_AUTOMEDIA };
#endif
/*const struct optdesc opt_iff_dynamic   = { "iff-dynamic",     "dynamic",     OPT_IFF_DYNAMIC,     GROUP_INTERFACE, PH_OFFSET, TYPE_BOOL,     OFUNC_OFFSET_MASKS, XIO_OFFSETOF(para.interface.iff_opts), XIO_SIZEOF(short), IFF_DYNAMIC };*/
#ifdef PACKET_AUXDATA
const struct optdesc opt_retrieve_vlan   = { "retrieve-vlan",   NULL,          OPT_RETRIEVE_VLAN,   GROUP_INTERFACE, PH_LATE, TYPE_CONST,    OFUNC_SPEC };
#endif
#if LATER
const struct optdesc opt_route           = { "route",           NULL,          OPT_ROUTE,           GROUP_INTERFACE, PH_INIT, TYPE_STRING,   OFUNC_SPEC };
#endif

#if WITH_INTERFACE
const struct addrdesc xioaddr_interface = { "INTERFACE",    3, xioopen_interface, GROUP_FD|GROUP_SOCKET|GROUP_INTERFACE, PF_PACKET, 0, 0 HELP(":<interface>") };
#endif /* WITH_INTERFACE */


static
int _xioopen_interface(const char *ifname,
		       struct opt *opts, int xioflags, xiofile_t *xxfd,
		       groups_t groups, int pf) {
   xiosingle_t *sfd = &xxfd->stream;
   union sockaddr_union us = {{0}};
   socklen_t uslen;
   int socktype = SOCK_RAW;
   unsigned int ifidx;
   bool needbind = false;
   char *bindstring = NULL;
   struct sockaddr_ll sall = { 0 };
   int rc;

   if (ifindex(ifname, &ifidx, -1) < 0) {
      Error1("unknown interface \"%s\"", ifname);
      ifidx = 0;	/* desparate attempt to continue */
   }

   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_INTERFACE;
   retropt_int(opts, OPT_SO_TYPE, &socktype);

   retropt_socket_pf(opts, &pf);

   /* ...res_opts[] */
   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   sfd->salen = sizeof(sfd->peersa);
   if (pf == PF_UNSPEC) {
      pf = sfd->peersa.soa.sa_family;
   }

   sfd->dtype = XIODATA_RECVFROM_SKIPIP;

   if (retropt_string(opts, OPT_BIND, &bindstring)) {
      needbind = true;
   }
   /*!!! parse by ':' */
   us.ll.sll_family = pf;
   us.ll.sll_protocol = htons(ETH_P_ALL);
   us.ll.sll_ifindex = ifidx;
   uslen = sizeof(sall);
   needbind = true;
   sfd->peersa = (union sockaddr_union)us;

   rc =
      _xioopen_dgram_sendto(needbind?&us:NULL, uslen,
			    opts, xioflags, sfd, groups, pf, socktype, 0, 0);
   if (rc < 0)
      return rc;

   strncpy(sfd->para.interface.name, ifname, IFNAMSIZ);
   _xiointerface_get_iff(sfd->fd, ifname, &sfd->para.interface.save_iff);
   _xiointerface_apply_iff(sfd->fd, ifname, sfd->para.interface.iff_opts);
   if (_interface_retrieve_vlan(sfd, opts) < 0)
      return STAT_NORETRY;

#ifdef PACKET_IGNORE_OUTGOING
   /* Raw socket might also provide packets that are outbound - we are not
      interested in these and disable this "feature" in kernel if possible */
   if (Setsockopt(sfd->fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &one, sizeof(one)) < 0) {
      Warn2("setsockopt(%d, SOL_PACKET, PACKET_IGNORE_OUTGOING, {1}): %s",
	    sfd->fd, strerror(errno));
   }
#endif /*defined(PACKET_IGNORE_OUTGOING) */

   return 0;
}


int _interface_retrieve_vlan(struct single *sfd, struct opt *opts) {
#if HAVE_STRUCT_TPACKET_AUXDATA
   if (retropt_bool(opts, OPT_RETRIEVE_VLAN,
		    &sfd->para.socket.retrieve_vlan)
       == 0) {
      if (!xioparms.experimental) {
	 Warn1("option %s is experimental", opts->desc->defname);
      }
   }
   if (sfd->para.socket.retrieve_vlan) {
      if (_interface_setsockopt_auxdata(sfd->fd, 1) < 0) {
	 return -1;
      }
   }
#endif /* HAVE_STRUCT_TPACKET_AUXDATA */
   return 0;
}

int _interface_setsockopt_auxdata(int fd, int auxdata) {
#ifdef PACKET_AUXDATA
   /* Linux strips VLAN tag off incoming packets and makes it available per
      ancillary data as auxdata. Apply option packet-auxdata if you want the
      VLAN tag to be restored by Socat in the received packet */
   if (auxdata) {
      int rc;
      Info1("setsockopt(fd=%d, level=SOL_PACKET, optname=PACKET_AUXDATA)", fd);
      rc = Setsockopt(fd, SOL_PACKET, PACKET_AUXDATA, &auxdata, sizeof(auxdata));
      if (rc < 0) {
	 Error3("setsockopt(%d, SOL_PACKET, PACKET_AUXDATA, , {%d}): %s",
		fd, auxdata, strerror(errno));
      }
   }
#endif /* defined(PACKET_AUXDATA) */
   return 0;
}

static
int xioopen_interface(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   xiosingle_t *sfd = &xxfd->stream;
   int result;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   if ((result =
	_xioopen_interface(argv[1], opts, xioflags, xxfd, addrdesc->groups,
			   addrdesc->arg1))
       != STAT_OK) {
      return result;
   }

   sfd->dtype = XIOREAD_RECV|XIOWRITE_SENDTO;
   if (addrdesc->arg1 == PF_INET) {
      sfd->dtype |= XIOREAD_RECV_SKIPIP;
   }

   sfd->para.socket.la.soa.sa_family = sfd->peersa.soa.sa_family;

   _xio_openlate(sfd, opts);
   return STAT_OK;
}


/* Retrieves the interface flags related to sockfd */
int _xiointerface_get_iff(
	int sockfd,
	const char *name,
	short *save_iff)
{
   struct ifreq ifr;

   memset(&ifr, 0, sizeof(ifr));
   strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
   if (Ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
      Error3("ioctl(%d, SIOCGIFFLAGS, {\"%s\"}: %s",
	     sockfd, ifr.ifr_name, strerror(errno));
   }
   *save_iff = ifr.ifr_flags;
   return 0;
}

/* Applies the interface flags to the socket FD.
   Used by INTERFACE and TUN
*/
int _xiointerface_set_iff(
	int sockfd,
	const char *name,
	short new_iff)
{
   struct ifreq ifr;

   memset(&ifr, 0, sizeof(ifr));
   strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
   if (Ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
      Error3("ioctl(%d, SIOCGIFFLAGS, {\"%s\"}: %s",
	     sockfd, ifr.ifr_name, strerror(errno));
   }
   ifr.ifr_flags = new_iff;
   if (Ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
      Error4("ioctl(%d, SIOCSIFFLAGS, {\"%s\", %hd}: %s",
	     sockfd, ifr.ifr_name, ifr.ifr_flags, strerror(errno));
   }
   return 0;
}

/* Applies the interface flags to the socket FD
   Used by INTERFACE and TUN
 */
int _xiointerface_apply_iff(
	int sockfd,
	const char *name,
	short iff_opts[2])
{
   struct ifreq ifr;

   memset(&ifr, 0, sizeof(ifr));
   strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
   if (Ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
      Error3("ioctl(%d, SIOCGIFFLAGS, {\"%s\"}: %s",
	     sockfd, ifr.ifr_name, strerror(errno));
   }
   Debug2("\"%s\": system set flags: 0x%hx", ifr.ifr_name, ifr.ifr_flags);
   ifr.ifr_flags |= iff_opts[0];
   ifr.ifr_flags &= ~iff_opts[1];
   Debug2("\"%s\": xio merged flags: 0x%hx", ifr.ifr_name, ifr.ifr_flags);
   if (Ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
      Error4("ioctl(%d, SIOCSIFFLAGS, {\"%s\", %hd}: %s",
	     sockfd, ifr.ifr_name, ifr.ifr_flags, strerror(errno));
   }
   ifr.ifr_flags = 0;
   if (Ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
      Error3("ioctl(%d, SIOCGIFFLAGS, {\"%s\"}: %s",
	     sockfd, ifr.ifr_name, strerror(errno));
   }
   Debug2("\"%s\": resulting flags: 0x%hx", ifr.ifr_name, ifr.ifr_flags);
   return 0;
}


#if HAVE_STRUCT_CMSGHDR && HAVE_STRUCT_TPACKET_AUXDATA
/* Converts the ancillary message in *cmsg into a form useable for further
   processing. Knows the specifics of common message types.
   On PACKET_AUXDATA it stored the ancillary data in the XFD.
   For other types:
   returns the number of resulting syntax elements in *num,
   returns a sequence of \0 terminated type strings in *typbuff,
   returns a sequence of \0 terminated name strings in *nambuff,
   returns a sequence of \0 terminated value strings in *valbuff,
   the respective len parameters specify the available space in the buffers
   returns STAT_OK or other STAT_*
 */
int
xiolog_ancillary_packet(struct single *sfd,
			struct cmsghdr *cmsg, int *num,
			char *typbuff, int typlen,
			char *nambuff, int namlen,
			char *envbuff, int envlen,
			char *valbuff, int vallen) {
#if LATER
   const char *cmsgtype, *cmsgname, *cmsgenvn;
   size_t msglen;
#endif
   struct tpacket_auxdata *auxp;
   int rc = STAT_OK;

   *num = 0;

#if defined(CMSG_DATA)

#if LATER
   msglen = cmsg->cmsg_len-((char *)CMSG_DATA(cmsg)-(char *)cmsg);
#endif
   switch (cmsg->cmsg_type) {
#if HAVE_STRUCT_TPACKET_AUXDATA_TP_VLAN_TPID
   case PACKET_AUXDATA:
#if LATER
      cmsgname = "packet_auxdata";
      cmsgtype = "auxdata";
      cmsgenvn = "AUXDATA";
#endif
      auxp = (struct tpacket_auxdata *)CMSG_DATA(cmsg);
      Info8("%s(): Ancillary message: PACKET_AUXDATA: status="F_uint32_t", len="F_uint32_t", snaplen="F_uint32_t", mac="F_uint16_t", net="F_uint16_t", vlan_tci="F_uint16_t", vlan_tpid="F_uint16_t"", __func__, auxp->tp_status, auxp->tp_len, auxp->tp_snaplen, auxp->tp_mac, auxp->tp_net, auxp->tp_vlan_tci, auxp->tp_vlan_tpid);
      sfd->para.socket.ancill_data_packet_auxdata = *auxp;
      sfd->para.socket.ancill_flag.packet_auxdata = 1;
      snprintf(typbuff, typlen, "PACKET.%u", cmsg->cmsg_type);
      nambuff[0] = '\0'; strncat(nambuff, "vlan", namlen-1);
      snprintf(strchr(valbuff, '\0')-1/*def \n*/, vallen-strlen(valbuff)+1, ", %d", auxp->tp_vlan_tci);
      break;
#endif /* HAVE_STRUCT_TPACKET_AUXDATA_TP_VLAN_TPID */
   default:	/* binary data */
      Warn1("xiolog_ancillary_packet(): INTERNAL: cmsg_type=%d not handled", cmsg->cmsg_type);
      return rc;
   }
   return rc;

#else /* !defined(CMSG_DATA) */

   return STAT_NORETRY;

#endif /* !defined(CMSG_DATA) */
}
#endif /* HAVE_STRUCT_CMSGHDR && HAVE_STRUCT_TPACKET_AUXDATA */

#endif /* _WITH_INTERFACE */
