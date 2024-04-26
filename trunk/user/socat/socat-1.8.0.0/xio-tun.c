/* source: xio-tun.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of tun/tap type */

#include "xiosysincludes.h"
#if WITH_TUN
#include "xioopen.h"

#include "xio-named.h"
#include "xio-socket.h"
#include "xio-ip.h"
#include "xio-interface.h"

#include "xio-tun.h"


static int xioopen_tun(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

/****** TUN options ******/
const struct optdesc opt_tun_device    = { "tun-device",     NULL,      OPT_TUN_DEVICE,      GROUP_TUN,       PH_OPEN, TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_tun_name      = { "tun-name",       NULL,      OPT_TUN_NAME,        GROUP_INTERFACE, PH_FD,   TYPE_STRING,   OFUNC_SPEC };
const struct optdesc opt_tun_type      = { "tun-type",       NULL,      OPT_TUN_TYPE,        GROUP_INTERFACE, PH_FD,   TYPE_STRING,   OFUNC_SPEC };
const struct optdesc opt_iff_no_pi     = { "iff-no-pi",      "no-pi",   OPT_IFF_NO_PI,       GROUP_TUN,       PH_FD,   TYPE_BOOL,     OFUNC_SPEC };

/****** TUN addresses ******/
const struct addrdesc xioaddr_tun    = { "TUN",    3, xioopen_tun, GROUP_FD|GROUP_CHR|GROUP_OPEN|GROUP_TUN, 0, 0, 0 HELP("[:<ip-addr>/<bits>]") };
/* "if-name"=tun3
// "route"=address/netmask
// "ip6-route"=address/netmask
// "iff-broadcast"
// "iff-debug"
// "iff-promisc"
// see .../linux/if.h
*/


#if LATER
/* sub options for route option */
#define IFOPT_ROUTE 1
static const struct optdesc opt_route_tos = { "route", NULL, IFOPT_ROUTE, };
static const struct optname xio_route_options[] = {
   {"tos", &xio_route_tos }
} ;
#endif

static int xioopen_tun(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   char *tundevice = NULL;
   char *tunname = NULL, *tuntype = NULL;
   int pf = /*! PF_UNSPEC*/ PF_INET;
   struct xiorange network;
   bool no_pi = false;
   const char *namedargv[] = { "tun", NULL, NULL };
   int rw = (xioflags & XIO_ACCMODE);
   bool exists;
   struct ifreq ifr;
   int sockfd;
   char *ifaddr;
   int result;

   if (argc > 2 || argc < 0) {
#if WITH_HELP
      Error3("%s: wrong number of parameters (%d instead of 0 or 1); usage: %s",
	     argv[0], argc-1, addrdesc->syntax);
#else
      Error2("%s: wrong number of parameters (%d instead of 0 or 1)",
	     argv[0], argc-1);
#endif
   }

   if (retropt_string(opts, OPT_TUN_DEVICE, &tundevice) != 0) {
      tundevice = strdup("/dev/net/tun");
   }

   /*! socket option here? */
   retropt_socket_pf(opts, &pf);

   namedargv[1] = tundevice;
   /* open the tun cloning device */
   if ((result = _xioopen_named_early(2, namedargv, xfd, addrdesc->groups,
				      &exists, opts, addrdesc->syntax)) < 0) {
      return result;
   }

   /*========================= the tunnel interface =========================*/
   Notice("creating tunnel network interface");
   applyopts_optgroup(&xfd->stream, -1, opts, GROUP_PROCESS);
   if ((result = _xioopen_open(tundevice, rw, opts)) < 0)
      return result;
   sfd->fd = result;

   /* prepare configuration of the new network interface */
   memset(&ifr, 0, sizeof(ifr));

   if (retropt_string(opts, OPT_TUN_NAME, &tunname) == 0) {
      strncpy(ifr.ifr_name, tunname, IFNAMSIZ);	/* ok */
      free(tunname);
   } else {
      ifr.ifr_name[0] = '\0';
   }

   ifr.ifr_flags = IFF_TUN;
   if (retropt_string(opts, OPT_TUN_TYPE, &tuntype) == 0) {
      if (!strcmp(tuntype, "tap")) {
	 ifr.ifr_flags = IFF_TAP;
      } else if (strcmp(tuntype, "tun")) {
	 Error1("unknown tun-type \"%s\"", tuntype);
      }
   }

   if (retropt_bool(opts, OPT_IFF_NO_PI, &no_pi) == 0) {
      if (no_pi) {
	 ifr.ifr_flags |= IFF_NO_PI;
#if 0 /* not neccessary for now */
      } else {
	 ifr.ifr_flags &= ~IFF_NO_PI;
#endif
      }
   }

   if (Ioctl(sfd->fd, TUNSETIFF, &ifr) < 0) {
      Error3("ioctl(%d, TUNSETIFF, {\"%s\"}: %s",
	     sfd->fd, ifr.ifr_name, strerror(errno));
      Close(sfd->fd);
   }
   Notice1("TUN: new device \"%s\"", ifr.ifr_name);

   /*===================== setting interface properties =====================*/

   /* we seem to need a socket for manipulating the interface */
   if ((sockfd = Socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
      Error1("socket(PF_INET, SOCK_DGRAM, 0): %s", strerror(errno));
      sockfd = sfd->fd;	/* desparate fallback attempt */
   }

   /*--------------------- setting interface address and netmask ------------*/
   if (argc == 2) {
       if ((ifaddr = strdup(argv[1])) == NULL) {
          Error1("strdup(\"%s\"): out of memory", argv[1]);
          return STAT_RETRYLATER;
       }
       if ((result = xioparsenetwork(ifaddr, pf, &network,
				     sfd->para.socket.ip.ai_flags))
	   != STAT_OK) {
          /*! recover */
          return result;
       }
       socket_init(pf, (union sockaddr_union *)&ifr.ifr_addr);
       ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr =
          network.netaddr.ip4.sin_addr;
       if (Ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
          Error4("ioctl(%d, SIOCSIFADDR, {\"%s\", \"%s\"}: %s",
             sockfd, ifr.ifr_name, ifaddr, strerror(errno));
       }
       ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr =
          network.netmask.ip4.sin_addr;
       if (Ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
          Error4("ioctl(%d, SIOCSIFNETMASK, {\"0x%08u\", \"%s\"}, %s",
             sockfd, ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr,
             ifaddr, strerror(errno));
       }
       free(ifaddr);
   }
   /*--------------------- setting interface flags --------------------------*/
   applyopts_single(sfd, opts, PH_FD);

   _xiointerface_apply_iff(sockfd, ifr.ifr_name, sfd->para.interface.iff_opts);
   if (_interface_retrieve_vlan(&xfd->stream, opts) < 0)
      return STAT_NORETRY;

   applyopts(sfd, -1, opts, PH_FD);
   applyopts_cloexec(sfd->fd, opts);

   applyopts_fchown(sfd->fd, opts);

   if ((result = _xio_openlate(sfd, opts)) < 0)
      return result;

   return 0;
}

#endif /* WITH_TUN */
