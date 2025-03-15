/* source: xio-udp.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for handling UDP addresses */

#include "xiosysincludes.h"

#if _WITH_UDP && (WITH_IP4 || WITH_IP6)

#include "xioopen.h"
#include "xio-socket.h"
#include "xio-ip4.h"
#include "xio-ip6.h"
#include "xio-ip.h"
#include "xio-ipapp.h"
#include "xio-tcpwrap.h"

#include "xio-udp.h"

#if WITH_UDP

const struct addrdesc xioaddr_udp_connect  = { "UDP-CONNECT",    1+XIO_RDWR,   xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP, SOCK_DGRAM, IPPROTO_UDP, PF_UNSPEC HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_udp_listen   = { "UDP-LISTEN",     1+XIO_RDWR,   xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_UNSPEC, IPPROTO_UDP, PF_UNSPEC HELP(":<port>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_udp_sendto   = { "UDP-SENDTO",     1+XIO_RDWR,   xioopen_udp_sendto,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP, PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udp_recvfrom = { "UDP-RECVFROM",   1+XIO_RDWR,   xioopen_udp_recvfrom,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_CHILD|GROUP_RANGE, PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP HELP(":<port>") };
const struct addrdesc xioaddr_udp_recv     = { "UDP-RECV",       1+XIO_RDONLY, xioopen_udp_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_RANGE,             PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP  HELP(":<port>") };
const struct addrdesc xioaddr_udp_datagram = { "UDP-DATAGRAM",   1+XIO_RDWR,   xioopen_udp_datagram,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_RANGE, PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP HELP(":<host>:<port>") };

#if WITH_IP4
const struct addrdesc xioaddr_udp4_connect = { "UDP4-CONNECT",   1+XIO_RDWR,   xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP, SOCK_DGRAM, IPPROTO_UDP, PF_INET HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_udp4_listen  = { "UDP4-LISTEN",    1+XIO_RDWR,   xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_INET, IPPROTO_UDP, PF_INET HELP(":<port>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_udp4_sendto  = { "UDP4-SENDTO",    1+XIO_RDWR,   xioopen_udp_sendto,   GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP, PF_INET, SOCK_DGRAM, IPPROTO_UDP  HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udp4_datagram = { "UDP4-DATAGRAM", 1+XIO_RDWR,   xioopen_udp_datagram,  GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_RANGE, PF_INET, SOCK_DGRAM, IPPROTO_UDP HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udp4_recvfrom= { "UDP4-RECVFROM",  1+XIO_RDWR,   xioopen_udp_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_CHILD|GROUP_RANGE, PF_INET, SOCK_DGRAM, IPPROTO_UDP  HELP(":<port>") };
const struct addrdesc xioaddr_udp4_recv    = { "UDP4-RECV",      1+XIO_RDONLY, xioopen_udp_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP4|GROUP_IP_UDP|GROUP_RANGE,             PF_INET, SOCK_DGRAM, IPPROTO_UDP  HELP(":<port>") };
#endif /* WITH_IP4 */

#if WITH_IP6
const struct addrdesc xioaddr_udp6_connect = { "UDP6-CONNECT",   1+XIO_RDWR,   xioopen_ipapp_connect, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP, SOCK_DGRAM, IPPROTO_UDP, PF_INET6 HELP(":<host>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_udp6_listen  = { "UDP6-LISTEN",    1+XIO_RDWR,   xioopen_ipdgram_listen, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_LISTEN|GROUP_CHILD|GROUP_RANGE, PF_INET6, IPPROTO_UDP, 0 HELP(":<port>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_udp6_sendto  = { "UDP6-SENDTO",    1+XIO_RDWR,   xioopen_udp_sendto, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP, PF_INET6, SOCK_DGRAM, IPPROTO_UDP HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udp6_datagram= { "UDP6-DATAGRAM",  1+XIO_RDWR,   xioopen_udp_datagram,GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_RANGE, PF_INET6, SOCK_DGRAM, IPPROTO_UDP HELP(":<host>:<port>") };
const struct addrdesc xioaddr_udp6_recvfrom= { "UDP6-RECVFROM",  1+XIO_RDWR,   xioopen_udp_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_CHILD|GROUP_RANGE, PF_INET6, SOCK_DGRAM, IPPROTO_UDP  HELP(":<port>") };
const struct addrdesc xioaddr_udp6_recv    = { "UDP6-RECV",      1+XIO_RDONLY, xioopen_udp_recv,     GROUP_FD|GROUP_SOCKET|GROUP_SOCK_IP6|GROUP_IP_UDP|GROUP_RANGE,             PF_INET6, SOCK_DGRAM, IPPROTO_UDP  HELP(":<port>") };
#endif /* WITH_IP6 */

#endif /* WITH_UDP */


int _xioopen_ipdgram_listen(struct single *sfd,
	int xioflags, union sockaddr_union *us, socklen_t uslen,
	struct opt *opts, int pf, int socktype, int ipproto) {
   union sockaddr_union themunion;
   union sockaddr_union *them = &themunion;
   struct pollfd readfd;
   bool dofork = false;
   int maxchildren = 0;
   pid_t pid;
   char *rangename;
   char infobuff[256];
   unsigned char buff1[1];
   socklen_t themlen;
   int result;

   retropt_bool(opts, OPT_FORK, &dofork);

   if (dofork) {
      if (!(xioflags & XIO_MAYFORK)) {
	 Error("option fork not allowed here");
	 return STAT_NORETRY;
      }
   }

   retropt_int(opts, OPT_MAX_CHILDREN, &maxchildren);

   if (! dofork && maxchildren) {
       Error("option max-children not allowed without option fork");
       return STAT_NORETRY;
   }

#if WITH_IP4 || WITH_IP6
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, pf, &sfd->para.socket.range,
			sfd->para.socket.ip.ai_flags)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      free(rangename);
      sfd->para.socket.dorange = true;
   }
#endif

#if WITH_LIBWRAP
   xio_retropt_tcpwrap(sfd, opts);
#endif /* WITH_LIBWRAP */

   if (retropt_ushort(opts, OPT_SOURCEPORT, &sfd->para.socket.ip.sourceport)
       >= 0) {
      sfd->para.socket.ip.dosourceport = true;
   }
   retropt_bool(opts, OPT_LOWPORT, &sfd->para.socket.ip.lowport);

   if (dofork) {
      xiosetchilddied();	/* set SIGCHLD handler */
   }

   while (true) {	/* we loop with fork or prohibited packets */
      /* now wait for some packet on this datagram socket, get its sender
	 address, connect there, and return */
      union integral notnull;
      union integral reuseaddr;
      int doreuseaddr = (dofork != 0);
      char infobuff[256];
      union sockaddr_union _sockname;
      union sockaddr_union *la = &_sockname;	/* local address */

      reuseaddr.u_int = dofork;

      if ((sfd->fd = xiosocket(opts, pf, socktype, ipproto, E_ERROR)) < 0) {
	 return STAT_RETRYLATER;
      }
      doreuseaddr |= (retropt_2integrals(opts, OPT_SO_REUSEADDR,
					 &reuseaddr, &notnull) >= 0);
      applyopts(sfd, -1, opts, PH_PASTSOCKET);

      /* SO_REUSEADDR handling of UDP sockets is helpful on Solaris */
      if (doreuseaddr) {
	 if (Setsockopt(sfd->fd, opt_so_reuseaddr.major,
			opt_so_reuseaddr.minor, &reuseaddr.u_int, sizeof(reuseaddr.u_int))
	     < 0) {
	    Warn6("setsockopt(%d, %d, %d, {%d}, "F_Zd"): %s",
		  sfd->fd, opt_so_reuseaddr.major,
		  opt_so_reuseaddr.minor, reuseaddr.u_int, sizeof(reuseaddr.u_int),
		  strerror(errno));
	 }
      }
      applyopts_cloexec(sfd->fd, opts);
      applyopts(sfd, -1, opts, PH_PREBIND);
      applyopts(sfd, -1, opts, PH_BIND);
      if (Bind(sfd->fd, &us->soa, uslen) < 0) {
	 Error4("bind(%d, {%s}, "F_socklen"): %s", sfd->fd,
		sockaddr_info(&us->soa, uslen, infobuff, sizeof(infobuff)),
		uslen, strerror(errno));
	 return STAT_RETRYLATER;
      }
      /* under some circumstances bind() fills sockaddr with interesting info. */
      if (Getsockname(sfd->fd, &us->soa, &uslen) < 0) {
	 Error4("getsockname(%d, %p, {%d}): %s",
		sfd->fd, &us->soa, uslen, strerror(errno));
      }
      applyopts(sfd, -1, opts, PH_PASTBIND);

      if (ipproto == IPPROTO_UDP) {
	 Notice1("listening on UDP %s",
		 sockaddr_info(&us->soa, uslen, infobuff, sizeof(infobuff)));
      } else {
	 Notice2("listening on PROTO%d %s", ipproto,
		 sockaddr_info(&us->soa, uslen, infobuff, sizeof(infobuff)));
      }

      readfd.fd = sfd->fd;
      readfd.events = POLLIN|POLLERR;
      while (xiopoll(&readfd, 1, NULL) < 0) {
	 if (errno != EINTR)  break;
      }

      themlen = socket_init(pf, them);
      do {
	 result = Recvfrom(sfd->fd, buff1, 1, MSG_PEEK,
			     &them->soa, &themlen);
      } while (result < 0 && errno == EINTR);
      if (result < 0) {
	 Error5("recvfrom(%d, %p, 1, MSG_PEEK, {%s}, {"F_socklen"}): %s",
		sfd->fd, buff1,
		sockaddr_info(&them->soa, themlen, infobuff, sizeof(infobuff)),
		themlen, strerror(errno));
	 return STAT_RETRYLATER;
      }

      Notice1("accepting UDP connection from %s",
	      sockaddr_info(&them->soa, themlen, infobuff, sizeof(infobuff)));

      if (xiocheckpeer(sfd, them, la) < 0) {
	 Notice1("forbidding UDP connection from %s",
		 sockaddr_info(&them->soa, themlen,
			       infobuff, sizeof(infobuff)));
	 /* drop packet */
	 char buff[512];
	 Recv(sfd->fd, buff, sizeof(buff), 0);	/* drop packet */
	 Close(sfd->fd);
	 continue;
      }
      Info1("permitting UDP connection from %s",
	    sockaddr_info(&them->soa, themlen, infobuff, sizeof(infobuff)));

      if (dofork) {
	 pid = xio_fork(false, E_ERROR, sfd->shutup);
	 if (pid < 0) {
	    return STAT_RETRYLATER;
	 }

	 if (pid == 0) {	/* child */
	    pid_t cpid = Getpid();
	    xiosetenvulong("PID", cpid, 1);
	    break;
	 }

	 /* server: continue loop with socket()+recvfrom() */
	 /* This avoids the requirement of a sync (trigger) mechanism as with
	    RECVFROM addresses */
	 /* And when we dont close this we got awkward behaviour on Linux 2.4:
	    recvfrom gives 0 bytes with invalid socket address */
	 if (Close(sfd->fd) < 0) {
	    Info2("close(%d): %s", sfd->fd, strerror(errno));
	 }

	 while (maxchildren) {
	    if (num_child < maxchildren) break;
	    Notice("maxchildren are active, waiting");
	    /* UINT_MAX would even be nicer, but Openindiana works only
	       with 31 bits */
	    while (!Sleep(INT_MAX)) ;	/* any signal lets us continue */
	 }
	 Info("still listening");
	 continue;
      }
      break;
   } /* end of the big while loop */

   applyopts(sfd, -1, opts, PH_CONNECT);
   if ((result = Connect(sfd->fd, &them->soa, themlen)) < 0) {
      Error4("connect(%d, {%s}, "F_socklen"): %s",
	     sfd->fd,
	     sockaddr_info(&them->soa, themlen, infobuff, sizeof(infobuff)),
	     themlen, strerror(errno));
      return STAT_RETRYLATER;
   }

   /* set the env vars describing the local and remote sockets */
   if (Getsockname(sfd->fd, &us->soa, &uslen) < 0) {
      Warn4("getsockname(%d, %p, {%d}): %s",
	    sfd->fd, &us->soa, uslen, strerror(errno));
   }
   xiosetsockaddrenv("SOCK", us,   uslen,   IPPROTO_UDP);
   xiosetsockaddrenv("PEER", them, themlen, IPPROTO_UDP);

   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;
   applyopts_fchown(sfd->fd, opts);
   applyopts(sfd, -1, opts, PH_LATE);

   if ((result = _xio_openlate(sfd, opts)) < 0)
      return result;

   return 0;
}

/* we expect the form: port */
int xioopen_ipdgram_listen(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   const char *portname = argv[1];
   int pf = addrdesc->arg1;
   int ipproto = addrdesc->arg2;
   union sockaddr_union us;
   int bind_rc;
   int socktype = SOCK_DGRAM;
   socklen_t uslen;
   int result;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   retropt_socket_pf(opts, &pf);

   retropt_int(opts, OPT_SO_PROTOTYPE, &ipproto);

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   uslen = socket_init(pf, &us);
   bind_rc = retropt_bind(opts, pf, socktype, ipproto,
		(struct sockaddr *)&us, &uslen, 1,
		xfd->stream.para.socket.ip.ai_flags);
   if (bind_rc == STAT_NORETRY)
      return STAT_NORETRY;
   if (pf == PF_UNSPEC && bind_rc == STAT_OK)
      pf = us.soa.sa_family;

   if (false) {
      ;
#if WITH_IP4 || WITH_IP6
   } else if (pf == PF_UNSPEC && bind_rc == STAT_NOACTION) {
      int ai_flags[2];
      ai_flags[0] = sfd->para.socket.ip.ai_flags[0];
      ai_flags[1] = sfd->para.socket.ip.ai_flags[1];
      if (!(ai_flags[1] & AI_PASSIVE))
	 ai_flags[0] |= AI_PASSIVE;
      result =
	 xioresolve(NULL, portname, pf, socktype, ipproto, &us, &uslen, ai_flags);
      if (result != STAT_OK) {
	 Error("error resolving bind option");
	 return STAT_NORETRY;
      }
      pf = us.soa.sa_family;
#endif /* WITH_IP4 || WITH_IP6*/
#if WITH_IP4
   } else if (pf == PF_INET) {
      us.ip4.sin_port = parseport(portname, ipproto);
#endif
#if WITH_IP6
   } else if (pf == PF_INET6) {
      us.ip6.sin6_port = parseport(portname, ipproto);
#endif
   } else {
      Error1("xioopen_ipdgram_listen(): unknown address family %d", pf);
   }

   return _xioopen_ipdgram_listen(&xfd->stream, xioflags, &us, uslen,
				  opts, pf, socktype, ipproto);
}

int xioopen_udp_sendto(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   int pf = addrdesc->arg1;
   int socktype = addrdesc->arg2;
   int ipproto = addrdesc->arg3;
   int result;

   if (argc != 3) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   retropt_socket_pf(opts, &pf);
   if ((result = _xioopen_udp_sendto(argv[1], argv[2], opts, xioflags, xfd,
				     addrdesc->groups, pf, socktype, ipproto))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(&xfd->stream, opts);
   return STAT_OK;
}

/*
   applies and consumes the following option:
   PH_INIT, PH_PASTSOCKET, PH_FD, PH_PREBIND, PH_BIND, PH_PASTBIND, PH_CONNECTED, PH_LATE
   OFUNC_OFFSET
   OPT_BIND, OPT_SOURCEPORT, OPT_LOWPORT, OPT_SO_TYPE, OPT_SO_PROTOTYPE, OPT_USER, OPT_GROUP, OPT_O_CLOEXEC
 */
int _xioopen_udp_sendto(const char *hostname, const char *servname,
			struct opt *opts,
		     int xioflags, xiofile_t *xxfd, groups_t groups,
		     int pf, int socktype, int ipproto) {
   struct single *sfd = &xxfd->stream;
   union sockaddr_union us;
   socklen_t uslen;
   int feats = 3;	/* option bind supports address and port */
   bool needbind = false;
   int result;

   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   /* ...res_opts[] */
   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   sfd->salen = sizeof(sfd->peersa);
   if ((result =
	xioresolve(hostname, servname, pf, socktype, ipproto,
		   &sfd->peersa, &sfd->salen,
		   sfd->para.socket.ip.ai_flags))
       != STAT_OK) {
      return result;
   }
   if (pf == PF_UNSPEC) {
      pf = sfd->peersa.soa.sa_family;
   }
   uslen = socket_init(pf, &us);
   if (retropt_bind(opts, pf, socktype, ipproto, &us.soa, &uslen, feats,
		    sfd->para.socket.ip.ai_flags)
       != STAT_NOACTION) {
      needbind = true;
   }

   if (retropt_ushort(opts, OPT_SOURCEPORT,
		      &sfd->para.socket.ip.sourceport) >= 0) {
      switch (pf) {
#if WITH_IP4
      case PF_INET:
	 us.ip4.sin_port = htons(sfd->para.socket.ip.sourceport);
	 break;
#endif
#if WITH_IP6
      case PF_INET6:
	 us.ip6.sin6_port = htons(sfd->para.socket.ip.sourceport);
	 break;
#endif
      }
      needbind = true;
   }

   retropt_bool(opts, OPT_LOWPORT, &sfd->para.socket.ip.lowport);

   sfd->dtype = XIODATA_RECVFROM;
   return _xioopen_dgram_sendto(needbind?&us:NULL, uslen,
			      opts, xioflags, sfd, groups,
				pf, socktype, ipproto,
				sfd->para.socket.ip.lowport);
}


int xioopen_udp_datagram(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   int pf = addrdesc->arg1;
   int socktype = addrdesc->arg2;
   int ipproto = addrdesc->arg3;
   char *rangename;
   char *hostname;
   int result;

   if (argc != 3) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   if ((hostname = strdup(argv[1])) == NULL) {
      Error1("strdup(\"%s\"): out of memory", argv[1]);
      return STAT_RETRYLATER;
   }

   /* only accept packets with correct remote ports */
   if (retropt_ushort(opts, OPT_SOURCEPORT, &sfd->para.socket.ip.sourceport)
       >= 0) {
      sfd->para.socket.ip.dosourceport = true;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   retropt_socket_pf(opts, &pf);

   result =
      _xioopen_udp_sendto(hostname, argv[2], opts, xioflags, xxfd,
			  addrdesc->groups, pf, socktype, ipproto);
   free(hostname);
   if (result != STAT_OK) {
      return result;
   }

   if (sfd->para.socket.ip.dosourceport) {
      switch (sfd->peersa.soa.sa_family) {
      default:
#if WITH_IP4
      case PF_INET:
	 sfd->para.socket.ip.sourceport = ntohs(sfd->peersa.ip4.sin_port);
	 break;
#endif /* WITH_IP4 */
#if WITH_IP6
      case PF_INET6:
	 sfd->para.socket.ip.sourceport = ntohs(sfd->peersa.ip6.sin6_port);
	 break;
#endif /* WITH_IP6 */
      }
   }

   sfd->dtype = XIOREAD_RECV|XIOWRITE_SENDTO;

   sfd->para.socket.la.soa.sa_family = sfd->peersa.soa.sa_family;

   /* which reply packets will be accepted - determine by range option */
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, pf, &sfd->para.socket.range,
			sfd->para.socket.ip.ai_flags)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      sfd->para.socket.dorange = true;
      sfd->dtype |= XIOREAD_RECV_CHECKRANGE;
      free(rangename);
   }

#if WITH_LIBWRAP
   xio_retropt_tcpwrap(sfd, opts);
#endif /* WITH_LIBWRAP */

   _xio_openlate(sfd, opts);
   return STAT_OK;
}


int xioopen_udp_recvfrom(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   int pf = addrdesc->arg1;
   int socktype = addrdesc->arg2;
   int ipproto = addrdesc->arg3;
   union sockaddr_union us;
   socklen_t uslen = sizeof(us);
   int ai_flags2[2];
   int result;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   retropt_socket_pf(opts, &pf);

   sfd->howtoend = END_NONE;
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_NONE;

   /* Set AI_PASSIVE, except when it is explicitly disabled */
   ai_flags2[0] = xfd->stream.para.socket.ip.ai_flags[0];
   ai_flags2[1] = xfd->stream.para.socket.ip.ai_flags[1];
   if (!(ai_flags2[1] & AI_PASSIVE))
      ai_flags2[0] |= AI_PASSIVE;

   if ((result =
	xioresolve(NULL, argv[1], pf, socktype, ipproto, &us, &uslen,
		   ai_flags2))
       != STAT_OK) {
      return result;
   }
   if (pf == PF_UNSPEC) {
      pf = us.soa.sa_family;
   }

   {
      union sockaddr_union la;
      socklen_t lalen = sizeof(la);

      if (retropt_bind(opts, pf, socktype, ipproto, &la.soa, &lalen, 1,
		       sfd->para.socket.ip.ai_flags)
	  != STAT_NOACTION) {
	 switch (pf) {
#if WITH_IP4
	 case PF_INET:  us.ip4.sin_addr  = la.ip4.sin_addr;  break;
#endif
#if WITH_IP6
	 case PF_INET6: us.ip6.sin6_addr = la.ip6.sin6_addr; break;
#endif
	 }
      }
   }

   if (retropt_ushort(opts, OPT_SOURCEPORT, &sfd->para.socket.ip.sourceport) >= 0) {
      sfd->para.socket.ip.dosourceport = true;
   }
   retropt_bool(opts, OPT_LOWPORT, &sfd->para.socket.ip.lowport);

   xfd->stream.dtype = XIODATA_RECVFROM_ONE;
   if ((result =
	_xioopen_dgram_recvfrom(sfd, xioflags, &us.soa, uslen,
				opts, pf, socktype, ipproto, E_ERROR))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(&xfd->stream, opts);
   return STAT_OK;
}


int xioopen_udp_recv(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   int pf = addrdesc->arg1;
   int socktype = addrdesc->arg2;
   int ipproto = addrdesc->arg3;
   union sockaddr_union us;
   socklen_t uslen = sizeof(us);
   char *rangename;
   int ai_flags2[2];
   int result;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   xioinit_ip(&pf, xioparms.default_ip);
   retropt_socket_pf(opts, &pf);

   /* Set AI_PASSIVE, except when it is explicitly disabled */
   ai_flags2[0] = xfd->stream.para.socket.ip.ai_flags[0];
   ai_flags2[1] = xfd->stream.para.socket.ip.ai_flags[1];
   if (!(ai_flags2[1] & AI_PASSIVE))
      ai_flags2[0] |= AI_PASSIVE;

   if ((result =
	xioresolve(NULL, argv[1], pf, socktype, ipproto, &us, &uslen,
		   ai_flags2))
       != STAT_OK) {
      return result;
   }
   if (pf == PF_UNSPEC) {
      pf = us.soa.sa_family;
   }

#if 1
   {
      union sockaddr_union la;
      socklen_t lalen = sizeof(la);

      if (retropt_bind(opts, pf, socktype, ipproto,
		       &xfd->stream.para.socket.la.soa, &lalen, 1,
		       ai_flags2)
	  != STAT_NOACTION) {
	 switch (pf) {
#if WITH_IP4
	 case PF_INET:
	    us.ip4.sin_addr  = xfd->stream.para.socket.la.ip4.sin_addr;  break;
#endif
#if WITH_IP6
	 case PF_INET6:
	    us.ip6.sin6_addr = xfd->stream.para.socket.la.ip6.sin6_addr; break;
#endif
	 }
      } else {
	 xfd->stream.para.socket.la.soa.sa_family = pf;
      }
   }
#endif

#if WITH_IP4 || WITH_IP6
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, pf, &xfd->stream.para.socket.range,
			xfd->stream.para.socket.ip.ai_flags)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      xfd->stream.para.socket.dorange = true;
   }
#endif

#if WITH_LIBWRAP
   xio_retropt_tcpwrap(&xfd->stream, opts);
#endif /* WITH_LIBWRAP */

   if (retropt_ushort(opts, OPT_SOURCEPORT,
		      &xfd->stream.para.socket.ip.sourceport)
       >= 0) {
      xfd->stream.para.socket.ip.dosourceport = true;
   }
   retropt_bool(opts, OPT_LOWPORT, &xfd->stream.para.socket.ip.lowport);

   xfd->stream.dtype = XIODATA_RECV;
   if ((result = _xioopen_dgram_recv(&xfd->stream, xioflags, &us.soa, uslen,
				     opts, pf, socktype, ipproto, E_ERROR))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(&xfd->stream, opts);
   return result;
}

#endif /* _WITH_UDP && (WITH_IP4 || WITH_IP6) */
