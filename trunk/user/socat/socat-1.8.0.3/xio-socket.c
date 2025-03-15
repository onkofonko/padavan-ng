/* source: xio-socket.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for socket related functions, and the
   implementation of generic socket addresses */

#include "xiosysincludes.h"

#include "xioopen.h"
#include "xio-ascii.h"
#include "xio-socket.h"
#include "xio-named.h"
#include "xio-unix.h"
#if WITH_VSOCK
#include "xio-vsock.h"
#endif
#if WITH_IP4
#include "xio-ip4.h"
#endif /* WITH_IP4 */
#if WITH_IP6
#include "xio-ip6.h"
#endif /* WITH_IP6 */
#include "xio-ip.h"
#include "xio-listen.h"
#include "xio-interface.h"
#include "xio-ipapp.h"	/*! not clean */
#include "xio-tcpwrap.h"


#if _WITH_SOCKET

static int xioopen_socket_connect(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
static int xioopen_socket_listen(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
static int xioopen_socket_sendto(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
static int xioopen_socket_datagram(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
static int xioopen_socket_recvfrom(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
static int xioopen_socket_recv(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

static
int _xioopen_socket_sendto(const char *pfname, const char *type,
			   const char *proto, const char *address,
			   struct opt *opts, int xioflags, xiofile_t *xxfd,
			   groups_t groups);

static int
xiolog_ancillary_socket(struct single *sfd, struct cmsghdr *cmsg, int *num,
			char *typbuff, int typlen,
			char *nambuff, int namlen,
			char *envbuff, int envlen,
			char *valbuff, int vallen);
static int xiobind(
	struct single *sfd,
	union sockaddr_union *us,
	size_t uslen,
	struct opt *opts,
	int pf,
	bool alt,
	int level);


#if WITH_GENERICSOCKET
/* generic socket addresses */
const struct addrdesc xioaddr_socket_connect = { "SOCKET-CONNECT",     1+XIO_RDWR,   xioopen_socket_connect,  GROUP_FD|GROUP_SOCKET|GROUP_CHILD|GROUP_RETRY, 0, 0, 0 HELP(":<domain>:<protocol>:<remote-address>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_socket_listen  = { "SOCKET-LISTEN",      1+XIO_RDWR,   xioopen_socket_listen,   GROUP_FD|GROUP_SOCKET|GROUP_LISTEN|GROUP_RANGE|GROUP_CHILD|GROUP_RETRY, 0, 0, 0 HELP(":<domain>:<protocol>:<local-address>") };
#endif /* WITH_LISTEN */
const struct addrdesc xioaddr_socket_sendto  = { "SOCKET-SENDTO",      1+XIO_RDWR,   xioopen_socket_sendto,   GROUP_FD|GROUP_SOCKET,                         0, 0, 0 HELP(":<domain>:<type>:<protocol>:<remote-address>") };
const struct addrdesc xioaddr_socket_datagram= { "SOCKET-DATAGRAM",    1+XIO_RDWR,   xioopen_socket_datagram, GROUP_FD|GROUP_SOCKET|GROUP_RANGE,             0, 0, 0 HELP(":<domain>:<type>:<protocol>:<remote-address>") };
const struct addrdesc xioaddr_socket_recvfrom= { "SOCKET-RECVFROM",    1+XIO_RDWR,   xioopen_socket_recvfrom, GROUP_FD|GROUP_SOCKET|GROUP_RANGE|GROUP_CHILD, 0, 0, 0 HELP(":<domain>:<type>:<protocol>:<local-address>") };
const struct addrdesc xioaddr_socket_recv    = { "SOCKET-RECV",        1+XIO_RDONLY, xioopen_socket_recv,     GROUP_FD|GROUP_SOCKET|GROUP_RANGE,             0, 0, 0 HELP(":<domain>:<type>:<protocol>:<local-address>") };
#endif /* WITH_GENERICSOCKET */


/* the following options apply not only to generic socket addresses but to all
   addresses that have anything to do with sockets */
const struct optdesc opt_so_debug    = { "so-debug",    "debug", OPT_SO_DEBUG,    GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_DEBUG };
#ifdef SO_ACCEPTCONN /* AIX433 */
const struct optdesc opt_so_acceptconn={ "so-acceptconn","acceptconn",OPT_SO_ACCEPTCONN,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_ACCEPTCONN};
#endif /* SO_ACCEPTCONN */
const struct optdesc opt_so_broadcast= { "so-broadcast", "broadcast", OPT_SO_BROADCAST,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_BROADCAST};
const struct optdesc opt_so_reuseaddr= { "so-reuseaddr", "reuseaddr", OPT_SO_REUSEADDR,GROUP_SOCKET, PH_PREBIND, TYPE_INT_NULL,  OFUNC_SOCKOPT, SOL_SOCKET, SO_REUSEADDR};
const struct optdesc opt_so_keepalive= { "so-keepalive", "keepalive", OPT_SO_KEEPALIVE,GROUP_SOCKET, PH_FD, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_KEEPALIVE};
#if HAVE_STRUCT_LINGER
const struct optdesc opt_so_linger   = { "so-linger",    "linger",    OPT_SO_LINGER,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_LINGER,OFUNC_SOCKOPT,SOL_SOCKET, SO_LINGER };
#else /* !HAVE_STRUCT_LINGER */
const struct optdesc opt_so_linger   = { "so-linger",    "linger",    OPT_SO_LINGER,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_LINGER };
#endif /* !HAVE_STRUCT_LINGER */
const struct optdesc opt_so_oobinline= { "so-oobinline", "oobinline", OPT_SO_OOBINLINE,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_OOBINLINE};
const struct optdesc opt_so_sndbuf   = { "so-sndbuf",    "sndbuf",    OPT_SO_SNDBUF,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_SNDBUF};
const struct optdesc opt_so_sndbuf_late={ "so-sndbuf-late","sndbuf-late",OPT_SO_SNDBUF_LATE,GROUP_SOCKET,PH_LATE,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SNDBUF };
const struct optdesc opt_so_rcvbuf   = { "so-rcvbuf",    "rcvbuf",    OPT_SO_RCVBUF,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_RCVBUF};
const struct optdesc opt_so_rcvbuf_late={"so-rcvbuf-late","rcvbuf-late",OPT_SO_RCVBUF_LATE,GROUP_SOCKET,PH_LATE,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_RCVBUF };
const struct optdesc opt_so_error    = { "so-error",     "error",     OPT_SO_ERROR,    GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_ERROR};
const struct optdesc opt_so_type     = { "so-type",      "type",      OPT_SO_TYPE,     GROUP_SOCKET, PH_SOCKET,     TYPE_INT,  OFUNC_SPEC,    SOL_SOCKET, SO_TYPE };
const struct optdesc opt_so_dontroute= { "so-dontroute", "dontroute", OPT_SO_DONTROUTE,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_DONTROUTE };
#ifdef SO_RCVLOWAT
const struct optdesc opt_so_rcvlowat = { "so-rcvlowat",  "rcvlowat", OPT_SO_RCVLOWAT, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_RCVLOWAT };
#endif
#ifdef SO_SNDLOWAT
const struct optdesc opt_so_sndlowat = { "so-sndlowat",  "sndlowat", OPT_SO_SNDLOWAT, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_SNDLOWAT };
#endif
/* end of setsockopt options of UNIX98 standard */

#ifdef SO_RCVTIMEO
const struct optdesc opt_so_rcvtimeo = { "so-rcvtimeo",  "rcvtimeo", OPT_SO_RCVTIMEO, GROUP_SOCKET, PH_PASTSOCKET, TYPE_TIMEVAL, OFUNC_SOCKOPT, SOL_SOCKET, SO_RCVTIMEO };
#endif
#ifdef SO_SNDTIMEO
const struct optdesc opt_so_sndtimeo = { "so-sndtimeo",  "sndtimeo", OPT_SO_SNDTIMEO, GROUP_SOCKET, PH_PASTSOCKET, TYPE_TIMEVAL, OFUNC_SOCKOPT, SOL_SOCKET, SO_SNDTIMEO };
#endif

#ifdef SO_AUDIT	/* AIX 4.3.3 */
const struct optdesc opt_so_audit    = { "so-audit",     "audit",    OPT_SO_AUDIT,    GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_AUDIT };
#endif /* SO_AUDIT */
#ifdef SO_ATTACH_FILTER
const struct optdesc opt_so_attach_filter={"so-attach-filter","attachfilter",OPT_SO_ATTACH_FILTER,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_ATTACH_FILTER};
#endif
#ifdef SO_DETACH_FILTER
const struct optdesc opt_so_detach_filter={"so-detach-filter","detachfilter",OPT_SO_DETACH_FILTER,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_DETACH_FILTER};
#endif
#ifdef SO_BINDTODEVICE	/* Linux: man 7 socket */
const struct optdesc opt_so_bindtodevice={"so-bindtodevice","if",OPT_SO_BINDTODEVICE,GROUP_SOCKET,PH_PASTSOCKET,TYPE_NAME,OFUNC_SOCKOPT,SOL_SOCKET,SO_BINDTODEVICE};
#endif
#ifdef SO_BSDCOMPAT
const struct optdesc opt_so_bsdcompat= { "so-bsdcompat","bsdcompat",OPT_SO_BSDCOMPAT,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_BSDCOMPAT };
#endif
#ifdef SO_CKSUMRECV
const struct optdesc opt_so_cksumrecv= { "so-cksumrecv","cksumrecv",OPT_SO_CKSUMRECV,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_CKSUMRECV };
#endif /* SO_CKSUMRECV */
#ifdef SO_TIMESTAMP
const struct optdesc opt_so_timestamp= { "so-timestamp","timestamp",OPT_SO_TIMESTAMP,GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_TIMESTAMP };
#endif
#ifdef SO_KERNACCEPT	/* AIX 4.3.3 */
const struct optdesc opt_so_kernaccept={ "so-kernaccept","kernaccept",OPT_SO_KERNACCEPT,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_KERNACCEPT};
#endif /* SO_KERNACCEPT */
#ifdef SO_NO_CHECK
const struct optdesc opt_so_no_check = { "so-no-check", "nocheck",OPT_SO_NO_CHECK, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_NO_CHECK };
#endif
#ifdef SO_NOREUSEADDR	/* AIX 4.3.3 */
const struct optdesc opt_so_noreuseaddr={"so-noreuseaddr","noreuseaddr",OPT_SO_NOREUSEADDR,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET, SO_NOREUSEADDR};
#endif /* SO_NOREUSEADDR */
#ifdef SO_PASSCRED
const struct optdesc opt_so_passcred = { "so-passcred", "passcred", OPT_SO_PASSCRED, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_PASSCRED};
#endif
#ifdef SO_PEERCRED
const struct optdesc opt_so_peercred = { "so-peercred", "peercred", OPT_SO_PEERCRED, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT3,OFUNC_SOCKOPT, SOL_SOCKET, SO_PEERCRED};
#endif
#ifdef SO_PRIORITY
const struct optdesc opt_so_priority = { "so-priority", "priority", OPT_SO_PRIORITY, GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_PRIORITY};
#endif
#ifdef SO_REUSEPORT	/* AIX 4.3.3, BSD, HP-UX, Linux >=3.9 */
const struct optdesc opt_so_reuseport = { "so-reuseport","reuseport",OPT_SO_REUSEPORT,GROUP_SOCKET, PH_PREBIND, TYPE_INT, OFUNC_SOCKOPT, SOL_SOCKET, SO_REUSEPORT };
#endif /* defined(SO_REUSEPORT) */
#ifdef SO_SECURITY_AUTHENTICATION
const struct optdesc opt_so_security_authentication={"so-security-authentication","securityauthentication",OPT_SO_SECURITY_AUTHENTICATION,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SECURITY_AUTHENTICATION};
#endif
#ifdef SO_SECURITY_ENCRYPTION_NETWORK
const struct optdesc opt_so_security_encryption_network= { "so-security-encryption-network","securityencryptionnetwork",OPT_SO_SECURITY_ENCRYPTION_NETWORK,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SECURITY_ENCRYPTION_NETWORK};
#endif
#ifdef SO_SECURITY_ENCRYPTION_TRANSPORT
const struct optdesc opt_so_security_encryption_transport= { "so-security-encryption-transport","securityencryptiontransport",OPT_SO_SECURITY_ENCRYPTION_TRANSPORT,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_SECURITY_ENCRYPTION_TRANSPORT};
#endif
#ifdef SO_USE_IFBUFS
const struct optdesc opt_so_use_ifbufs= { "so-use-ifbufs","useifbufs",OPT_SO_USE_IFBUFS,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,  OFUNC_SOCKOPT, SOL_SOCKET, SO_USE_IFBUFS};
#endif /* SO_USE_IFBUFS */
#ifdef SO_USELOOPBACK /* AIX433, Solaris, HP-UX */
const struct optdesc opt_so_useloopback= { "so-useloopback","useloopback",OPT_SO_USELOOPBACK,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT, SOL_SOCKET, SO_USELOOPBACK};
#endif /* SO_USELOOPBACK */
#ifdef SO_DGRAM_ERRIND	/* Solaris */
const struct optdesc opt_so_dgram_errind= { "so-dgram-errind","dgramerrind",OPT_SO_DGRAM_ERRIND,GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_DGRAM_ERRIND};
#endif /* SO_DGRAM_ERRIND */
#ifdef SO_DONTLINGER	/* Solaris */
const struct optdesc opt_so_dontlinger = { "so-dontlinger", "dontlinger",  OPT_SO_DONTLINGER,  GROUP_SOCKET,PH_PASTSOCKET,TYPE_INT,OFUNC_SOCKOPT,SOL_SOCKET,SO_DONTLINGER };
#endif
/* the SO_PROTOTYPE is OS defined on Solaris, HP-UX; we lend this for a more
   general purpose */
const struct optdesc opt_so_prototype  = { "so-protocol",  "protocol",   OPT_SO_PROTOTYPE,   GROUP_SOCKET,PH_SOCKET,    TYPE_INT,OFUNC_SPEC,   SOL_SOCKET,SO_PROTOCOL };
#ifdef FIOSETOWN
const struct optdesc opt_fiosetown   = { "fiosetown", NULL, OPT_FIOSETOWN,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_IOCTL,  FIOSETOWN };
#endif
#ifdef SIOCSPGRP
const struct optdesc opt_siocspgrp   = { "siocspgrp", NULL, OPT_SIOCSPGRP,   GROUP_SOCKET, PH_PASTSOCKET, TYPE_INT,  OFUNC_IOCTL,  SIOCSPGRP };
#endif
const struct optdesc opt_bind        = { "bind",      NULL, OPT_BIND,        GROUP_SOCKET, PH_BIND, TYPE_STRING,OFUNC_SPEC };
const struct optdesc opt_connect_timeout = { "connect-timeout", NULL, OPT_CONNECT_TIMEOUT, GROUP_SOCKET, PH_PASTSOCKET, TYPE_TIMEVAL, OFUNC_OFFSET, XIO_OFFSETOF(para.socket.connect_timeout) };
const struct optdesc opt_protocol_family = { "protocol-family", "pf", OPT_PROTOCOL_FAMILY, GROUP_SOCKET, PH_PRESOCKET,  TYPE_STRING,  OFUNC_SPEC };

/* generic setsockopt() options */
const struct optdesc opt_setsockopt        = { "setsockopt",        "sockopt",        OPT_SETSOCKOPT_BIN,        GROUP_SOCKET,PH_CONNECTED, TYPE_INT_INT_BIN,     OFUNC_SOCKOPT_GENERIC, 0, 0 };
const struct optdesc opt_setsockopt_int    = { "setsockopt-int",    "sockopt-int",    OPT_SETSOCKOPT_INT,        GROUP_SOCKET,PH_CONNECTED, TYPE_INT_INT_INT,     OFUNC_SOCKOPT_GENERIC, 0, 0 };
const struct optdesc opt_setsockopt_bin    = { "setsockopt-bin",    "sockopt-bin",    OPT_SETSOCKOPT_BIN,        GROUP_SOCKET,PH_CONNECTED, TYPE_INT_INT_BIN,     OFUNC_SOCKOPT_GENERIC, 0, 0 };
const struct optdesc opt_setsockopt_string = { "setsockopt-string", "sockopt-string", OPT_SETSOCKOPT_STRING,     GROUP_SOCKET,PH_CONNECTED, TYPE_INT_INT_STRING,  OFUNC_SOCKOPT_GENERIC, 0, 0 };
const struct optdesc opt_setsockopt_listen = { "setsockopt-listen", "sockopt-listen", OPT_SETSOCKOPT_LISTEN,     GROUP_SOCKET,PH_PREBIND,   TYPE_INT_INT_BIN,     OFUNC_SOCKOPT_GENERIC, 0, 0 };
const struct optdesc opt_setsockopt_socket    = { "setsockopt-socket",    "sockopt-sock", OPT_SETSOCKOPT_SOCKET,    GROUP_SOCKET,PH_PASTSOCKET, TYPE_INT_INT_BIN,    OFUNC_SOCKOPT_GENERIC, 0, 0 };
const struct optdesc opt_setsockopt_connected = { "setsockopt-connected", "sockopt-conn", OPT_SETSOCKOPT_CONNECTED, GROUP_SOCKET,PH_CONNECTED,  TYPE_INT_INT_BIN,    OFUNC_SOCKOPT_GENERIC, 0, 0 };

const struct optdesc opt_null_eof = { "null-eof", NULL, OPT_NULL_EOF, GROUP_SOCKET, PH_OFFSET, TYPE_BOOL, OFUNC_OFFSET, XIO_OFFSETOF(para.socket.null_eof) };


#if WITH_GENERICSOCKET

static int xioopen_socket_connect(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   const char *pfname = argv[1];
   const char *protname = argv[2];
   const char *address = argv[3];
   char *garbage;
   int pf;
   int proto;
   int socktype = SOCK_STREAM;
   int needbind = 0;
   union sockaddr_union them;  socklen_t themlen; size_t themsize;
   union sockaddr_union us;    socklen_t uslen = sizeof(us);
   int result;

   if (argc != 4) {
      xio_syntax(argv[0], 3, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   pf = strtoul(pfname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   proto = strtoul(protname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   /*retropt_int(opts, OPT_IP_PROTOCOL, &proto);*/
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   applyopts(sfd, -1, opts, PH_INIT);
   applyopts(sfd, -1, opts, PH_EARLY);

   themsize = 0;
   if ((result =
	dalan(address, (uint8_t *)&them.soa.sa_data, &themsize, sizeof(them), 'i'))
       < 0) {
      Error1("data too long: \"%s\"", address);
   } else if (result > 0) {
      Error1("syntax error in \"%s\"", address);
   }
   them.soa.sa_family = pf;
   themlen = themsize +
#if HAVE_STRUCT_SOCKADDR_SALEN
      sizeof(them.soa.sa_len) +
#endif
      sizeof(them.soa.sa_family);

   sfd->dtype = XIOREAD_STREAM|XIOWRITE_STREAM;

   socket_init(0, &us);
   if (retropt_bind(opts, pf, socktype, proto, (struct sockaddr *)&us, &uslen, -1,
#if _WITH_IP4 || _WITH_IP6
		    sfd->para.socket.ip.ai_flags
#else
		    NULL
#endif /* _WITH_IP4 || _WITH_IP6 */
		    )
       != STAT_NOACTION) {
      needbind = true;
      us.soa.sa_family = pf;
   }

   if ((result =
	xioopen_connect(sfd,
			needbind?&us:NULL, uslen,
			(struct sockaddr *)&them, themlen,
			opts, pf, socktype, proto, false)) != 0) {
      return result;
   }
   if ((result = _xio_openlate(sfd, opts)) < 0) {
      return result;
   }
   return STAT_OK;
}

#if WITH_LISTEN
static int xioopen_socket_listen(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   const char *pfname = argv[1];
   const char *protname = argv[2];
   const char *usname = argv[3];
   char *garbage;
   int pf;
   int proto;
   int socktype = SOCK_STREAM;
   union sockaddr_union us;  socklen_t uslen; size_t ussize;
   struct opt *opts0;
   int result;

   if (argc != 4) {
      xio_syntax(argv[0], 3, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   pf = strtoul(pfname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   proto = strtoul(protname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   /*retropt_int(opts, OPT_IP_PROTOCOL, &proto);*/
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   socket_init(0, &us);
   ussize = 0;
   if ((result =
	dalan(usname, (uint8_t *)&us.soa.sa_data, &ussize, sizeof(us), 'i'))
       < 0) {
      Error1("data too long: \"%s\"", usname);
   } else if (result > 0) {
      Error1("syntax error in \"%s\"", usname);
   }
   uslen = ussize + sizeof(us.soa.sa_family)
#if HAVE_STRUCT_SOCKADDR_SALEN
      + sizeof(us.soa.sa_len)
#endif
      ;
   us.soa.sa_family = pf;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   applyopts(sfd, -1, opts, PH_INIT);
   applyopts(sfd, -1, opts, PH_EARLY);

   opts0 = copyopts(opts, GROUP_ALL);

   if ((result =
	xioopen_listen(sfd, xioflags,
		       &us.soa, uslen,
		       opts, opts0, 0/*instead of pf*/, socktype, proto))
       != STAT_OK)
      return result;
   return STAT_OK;
}
#endif /* WITH_LISTEN */

/* we expect the form: ...:domain:type:protocol:remote-address */
static int xioopen_socket_sendto(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   int result;

   if (argc != 5) {
      xio_syntax(argv[0], 4, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }
   if ((result =
	   _xioopen_socket_sendto(argv[1], argv[2], argv[3], argv[4],
				  opts, xioflags, xxfd, addrdesc->groups))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(&xxfd->stream, opts);
   return STAT_OK;
}

static
int _xioopen_socket_sendto(const char *pfname, const char *type,
			   const char *protname, const char *address,
			   struct opt *opts, int xioflags, xiofile_t *xxfd,
			   groups_t groups) {
   xiosingle_t *sfd = &xxfd->stream;
   char *garbage;
   union sockaddr_union us = {{0}};
   socklen_t uslen = 0;   size_t ussize;
   size_t themsize;
   int pf;
   int socktype = SOCK_RAW;
   int proto;
   bool needbind = false;
   char *bindstring = NULL;
   int result;

   pf = strtoul(pfname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   socktype = strtoul(type, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   proto = strtoul(protname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   /*retropt_int(opts, OPT_IP_PROTOCOL, &proto);*/
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   sfd->peersa.soa.sa_family = pf;
   themsize = 0;
   if ((result =
	dalan(address, (uint8_t *)&sfd->peersa.soa.sa_data, &themsize,
	      sizeof(sfd->peersa), 'i'))
       < 0) {
      Error1("data too long: \"%s\"", address);
   } else if (result > 0) {
      Error1("syntax error in \"%s\"", address);
   }
   sfd->salen = themsize + sizeof(sa_family_t)
#if HAVE_STRUCT_SOCKADDR_SALEN
      + sizeof(sfd->peersa.soa.sa_len)
#endif
      ;
#if HAVE_STRUCT_SOCKADDR_SALEN
   sfd->peersa.soa.sa_len =
      sizeof(sfd->peersa.soa.sa_len) + sizeof(sfd->peersa.soa.sa_family) +
      themsize;
#endif

   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   if (pf == PF_UNSPEC) {
      pf = sfd->peersa.soa.sa_family;
   }

   sfd->dtype = XIODATA_RECVFROM;

   if (retropt_string(opts, OPT_BIND, &bindstring) == 0) {
      ussize = 0;
      if ((result =
	   dalan(bindstring, (uint8_t *)&us.soa.sa_data, &ussize, sizeof(us), 'i'))
	  < 0) {
	 Error1("data too long: \"%s\"", bindstring);
      } else if (result > 0) {
	 Error1("syntax error in \"%s\"", bindstring);
      }
      us.soa.sa_family = pf;
      uslen = ussize + sizeof(sa_family_t)
#if HAVE_STRUCT_SOCKADDR_SALEN
	 + sizeof(us.soa.sa_len)
#endif
	 ;
      needbind = true;
   }

   return
      _xioopen_dgram_sendto(needbind?&us:NULL, uslen,
			    opts, xioflags, sfd, groups, pf, socktype, proto, 0);
}


/* we expect the form: ...:domain:socktype:protocol:local-address */
static
int xioopen_socket_recvfrom(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   const char *pfname = argv[1];
   const char *typename = argv[2];
   const char *protname = argv[3];
   const char *address = argv[4];
   char *garbage;
   union sockaddr_union *us = &sfd->para.socket.la;
   socklen_t uslen; size_t ussize;
   int pf, socktype, proto;
   char *rangename;
   int result;

   if (argc != 5) {
      xio_syntax(argv[0], 4, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   pf = strtoul(pfname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   socktype = strtoul(typename, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   proto = strtoul(protname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   /*retropt_int(opts, OPT_IP_PROTOCOL, &proto);*/
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_NONE;

   ussize = 0;
   if ((result =
	dalan(address, (uint8_t *)&us->soa.sa_data, &ussize, sizeof(*us), 'i'))
       < 0) {
      Error1("data too long: \"%s\"", address);
   } else if (result > 0) {
      Error1("syntax error in \"%s\"", address);
   }
   us->soa.sa_family = pf;
   uslen = ussize + sizeof(us->soa.sa_family)
#if HAVE_STRUCT_SOCKADDR_SALEN
      + sizeof(us->soa.sa_len);
#endif
      ;
   sfd->dtype = XIOREAD_RECV|XIOWRITE_SENDTO;

   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, 0, &sfd->para.socket.range,
#if _WITH_IP4 || _WITH_IP6
			sfd->para.socket.ip.ai_flags
#else
			NULL
#endif /* _WITH_IP4 */
			)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      sfd->para.socket.dorange = true;
      free(rangename);
   }

   if ((result =
	_xioopen_dgram_recvfrom(sfd, xioflags, &us->soa, uslen,
				opts, pf, socktype, proto, E_ERROR))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(sfd, opts);
   return STAT_OK;
}

/* we expect the form: ...:domain:type:protocol:local-address */
static
int xioopen_socket_recv(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   const char *pfname = argv[1];
   const char *typename = argv[2];
   const char *protname = argv[3];
   const char *address = argv[4];
   char *garbage;
   union sockaddr_union us;
   socklen_t uslen; size_t ussize;
   int pf, socktype, proto;
   char *rangename;
   int result;

   if (argc != 5) {
      xio_syntax(argv[0], 4, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   pf = strtoul(pfname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   socktype = strtoul(typename, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   proto = strtoul(protname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   /*retropt_int(opts, OPT_IP_PROTOCOL, &proto);*/
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_NONE;

   ussize = 0;
   if ((result =
	dalan(address, (uint8_t *)&us.soa.sa_data, &ussize, sizeof(us), 'i'))
       < 0) {
      Error1("data too long: \"%s\"", address);
   } else if (result > 0) {
      Error1("syntax error in \"%s\"", address);
   }
   us.soa.sa_family = pf;
   uslen = ussize + sizeof(sa_family_t)
#if HAVE_STRUCT_SOCKADDR_SALEN
      +sizeof(us.soa.sa_len)
#endif
      ;
   sfd->dtype = XIOREAD_RECV;
   sfd->para.socket.la.soa.sa_family = pf;

   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, 0, &sfd->para.socket.range,
#if _WITH_IP4 || _WITH_IP6
			sfd->para.socket.ip.ai_flags
#else
			NULL
#endif /* _WITH_IP4 */
			)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      sfd->para.socket.dorange = true;
      free(rangename);
   }

   if ((result =
	_xioopen_dgram_recv(sfd, xioflags, &us.soa,
			    uslen, opts, pf, socktype, proto, E_ERROR))
       != STAT_OK) {
      return result;
   }
   _xio_openlate(sfd, opts);
   return STAT_OK;
}


/* we expect the form: ...:domain:type:protocol:remote-address */
static int xioopen_socket_datagram(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   xiosingle_t *sfd = &xxfd->stream;
   const char *pfname = argv[1];
   const char *typename = argv[2];
   const char *protname = argv[3];
   const char *address = argv[4];
   char *garbage;
   char *rangename;
   size_t themsize;
   int pf;
   int result;

   if (argc != 5) {
      xio_syntax(argv[0], 4, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   pf = strtoul(pfname, &garbage, 0);
   if (*garbage != '\0') {
      Warn1("garbage in parameter: \"%s\"", garbage);
   }

   retropt_socket_pf(opts, &pf);
   /*retropt_int(opts, OPT_IP_PROTOCOL, &proto);*/
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   sfd->peersa.soa.sa_family = pf;
   themsize = 0;
   if ((result =
	dalan(address, (uint8_t *)&sfd->peersa.soa.sa_data, &themsize,
	      sizeof(sfd->peersa), 'i'))
       < 0) {
      Error1("data too long: \"%s\"", address);
   } else if (result > 0) {
      Error1("syntax error in \"%s\"", address);
   }
   sfd->salen = themsize + sizeof(sa_family_t);
#if HAVE_STRUCT_SOCKADDR_SALEN
   sfd->peersa.soa.sa_len =
      sizeof(sfd->peersa.soa.sa_len) + sizeof(sfd->peersa.soa.sa_family) +
      themsize;
#endif

   if ((result =
	_xioopen_socket_sendto(pfname, typename, protname, address,
			       opts, xioflags, xxfd, addrdesc->groups))
       != STAT_OK) {
      return result;
   }

   sfd->dtype = XIOREAD_RECV|XIOWRITE_SENDTO;

   sfd->para.socket.la.soa.sa_family = sfd->peersa.soa.sa_family;

   /* which reply sockets will accept - determine by range option */
   if (retropt_string(opts, OPT_RANGE, &rangename) >= 0) {
      if (xioparserange(rangename, 0, &sfd->para.socket.range,
#if _WITH_IP4 || _WITH_IP6
			sfd->para.socket.ip.ai_flags
#else
			NULL
#endif /* _WITH_IP4 */
			)
	  < 0) {
	 free(rangename);
	 return STAT_NORETRY;
      }
      sfd->para.socket.dorange = true;
      sfd->dtype |= XIOREAD_RECV_CHECKRANGE;
      free(rangename);
   }

   _xio_openlate(sfd, opts);
   return STAT_OK;
}

#endif /* WITH_GENERICSOCKET */

/* EINTR not handled specially */
int xiogetpacketinfo(struct single *sfd, int fd)
{
#if defined(MSG_ERRQUEUE)
   int _errno = errno;
   char peername[256];
   union sockaddr_union _peername;
   /* union sockaddr_union _sockname; */
   union sockaddr_union *pa = &_peername;	/* peer address */
   /* union sockaddr_union *la = &_sockname; */	/* local address */
   socklen_t palen = sizeof(_peername);	/* peer address size */
   char ctrlbuff[1024];			/* ancillary messages */
   struct msghdr msgh = {0};

   msgh.msg_name = pa;
   msgh.msg_namelen = palen;
#if HAVE_STRUCT_MSGHDR_MSGCONTROL
   msgh.msg_control = ctrlbuff;
#endif
#if HAVE_STRUCT_MSGHDR_MSGCONTROLLEN
   msgh.msg_controllen = sizeof(ctrlbuff);
#endif
   if (xiogetancillary(fd,
		       &msgh,
		       MSG_ERRQUEUE
#ifdef MSG_TRUNC
		       |MSG_TRUNC
#endif
		       ) >= 0
       ) {
      palen = msgh.msg_namelen;

      Notice1("receiving packet from %s"/*"src"*/,
	      sockaddr_info(&pa->soa, palen, peername, sizeof(peername))/*,
									  sockaddr_info(&la->soa, sockname, sizeof(sockname))*/);

      xiodopacketinfo(sfd, &msgh, true, true);
   }
   errno = _errno;
#endif /* defined(MSG_ERRQUEUE) */
   return 0;
}



/* A subroutine that is common to all socket addresses that want to connect()
   a socket to a peer.
   Applies and consumes the following options:
   PH_PASTSOCKET, PH_FD, PH_PREBIND, PH_BIND, PH_PASTBIND, PH_CONNECT,
   PH_CONNECTED, PH_LATE,
   OFUNC_OFFSET,
   OPT_SO_TYPE, OPT_SO_PROTOTYPE, OPT_USER, OPT_GROUP, OPT_O_CLOEXEC
   Does not fork, does not retry.
   Alternate (alt) bind semantics are:
      with IP sockets: lowport (selects randomly a free port from 640 to 1023)
      with UNIX and abstract sockets: uses tmpname() to find a free file system
      entry.
   Returns STAT_OK on success, or STAT_RETRYLATER (+errno) on failure.
*/
int _xioopen_connect(struct single *sfd, union sockaddr_union *us, size_t uslen,
		     struct sockaddr *them, size_t themlen,
		     struct opt *opts, int pf, int socktype, int protocol,
		     bool alt, int level) {
   int fcntl_flags = 0;
   char infobuff[256];
   union sockaddr_union la;
   socklen_t lalen = themlen;
   int _errno;
   int result;

#if WITH_UNIX
   if (pf == PF_UNIX && us != NULL) {
      applyopts_named(us->un.sun_path, opts, PH_EARLY);
   }
#endif

   if ((sfd->fd = xiosocket(opts, pf, socktype, protocol, level)) < 0) {
      return STAT_RETRYLATER;
   }

   applyopts_offset(sfd, opts);
   applyopts(sfd, -1, opts, PH_PASTSOCKET);
   applyopts(sfd, -1, opts, PH_FD);

   applyopts_cloexec(sfd->fd, opts);

   if (xiobind(sfd, us, uslen, opts, pf, alt, level) < 0) {
      return STAT_RETRYLATER;
   }

   applyopts(sfd, -1, opts, PH_CONNECT);

   if (sfd->para.socket.connect_timeout.tv_sec  != 0 ||
       sfd->para.socket.connect_timeout.tv_usec != 0) {
      fcntl_flags = Fcntl(sfd->fd, F_GETFL);
      Fcntl_l(sfd->fd, F_SETFL, fcntl_flags|O_NONBLOCK);
   }

   result = Connect(sfd->fd, them, themlen);
   _errno = errno;
   la.soa.sa_family = them->sa_family;  lalen = sizeof(la);
   if (Getsockname(sfd->fd, &la.soa, &lalen) < 0) {
      Msg4(level-1, "getsockname(%d, %p, {%d}): %s",
	    sfd->fd, &la.soa, lalen, strerror(errno));
   }
   errno = _errno;
   if (result < 0) {
      if (errno == EINPROGRESS) {
	 if (sfd->para.socket.connect_timeout.tv_sec  != 0 ||
	     sfd->para.socket.connect_timeout.tv_usec != 0) {
	    struct timeval timeout;
	    struct pollfd writefd;
	    int err;
	    socklen_t errlen = sizeof(err);
	    int result;

	    Info4("connect(%d, %s, "F_Zd"): %s",
		  sfd->fd, sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
		  themlen, strerror(errno));
	    timeout = sfd->para.socket.connect_timeout;
	    writefd.fd = sfd->fd;
	    writefd.events = (POLLOUT|POLLERR);
	    result = xiopoll(&writefd, 1, &timeout);
	    if (result < 0) {
	       Msg4(level, "xiopoll({%d,POLLOUT|POLLERR},,{"F_tv_sec"."F_tv_usec"): %s",
		    sfd->fd, timeout.tv_sec, timeout.tv_usec, strerror(errno));
	       Close(sfd->fd);
	       errno = _errno;
	       return STAT_RETRYLATER;
	    }
	    if (result == 0) {
	       Msg2(level, "connecting to %s: %s",
		    sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
		    strerror(ETIMEDOUT));
	       Close(sfd->fd);
	       errno = _errno;
	       return STAT_RETRYLATER;
	    }
	    if (writefd.revents & POLLERR) {
#if 0
	       unsigned char dummy[1];
	       Read(sfd->fd, &dummy, 1);	/* get error message */
	       Msg2(level, "connecting to %s: %s",
		    sockaddr_info(them, infobuff, sizeof(infobuff)),
		    strerror(errno));
#else
	       Connect(sfd->fd, them, themlen);	/* get error message */
	       Msg4(level, "connect(%d, %s, "F_Zd"): %s",
		     sfd->fd, sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
		     themlen, strerror(errno));
#endif
	       _errno = errno;
	       Close(sfd->fd);
	       errno = _errno;
	       return STAT_RETRYLATER;
	    }
	    /* otherwise OK or network error */
	    result = Getsockopt(sfd->fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
	    if (result != 0) {
	       _errno = errno;
	       Msg2(level, "getsockopt(%d, SOL_SOCKET, SO_ERROR, ...): %s",
		    sfd->fd, strerror(err));
	       Close(sfd->fd);
	       errno = _errno;
	       return STAT_RETRYLATER;
	    }
	    Debug2("getsockopt(%d, SOL_SOCKET, SO_ERROR, { %d }) -> 0",
		   sfd->fd, err);
	    if (err != 0) {
	       Msg4(level, "connect(%d, %s, "F_Zd"): %s",
		     sfd->fd, sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
		     themlen, strerror(err));
	       Close(sfd->fd);
	       errno = err;
	       return STAT_RETRYLATER;
	    }
	    Fcntl_l(sfd->fd, F_SETFL, fcntl_flags);
	 } else {
	    Warn4("connect(%d, %s, "F_Zd"): %s",
		  sfd->fd, sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
		  themlen, strerror(errno));
	 }
      } else if (pf == PF_UNIX) {
	 /* this is for UNIX domain sockets: a connect attempt seems to be
	    the only way to distinguish stream and datagram sockets.
	    And no ancillary message expected
	 */
	 int _errno = errno;
	 Info4("connect(%d, %s, "F_Zd"): %s",
	       sfd->fd, sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
	       themlen, strerror(errno));
	 /* caller must handle this condition */
	 Close(sfd->fd);  sfd->fd = -1;
	 errno = _errno;
	 return STAT_RETRYLATER;
      } else {
	 /* try to find details about error, especially from ICMP */
	 xiogetpacketinfo(sfd, sfd->fd);

	 /* continue mainstream */
	 Msg4(level, "connect(%d, %s, "F_Zd"): %s",
	      sfd->fd, sockaddr_info(them, themlen, infobuff, sizeof(infobuff)),
	      themlen, strerror(errno));
	 Close(sfd->fd);
	 errno = _errno;
	 return STAT_RETRYLATER;
      }
   } else {	/* result >= 0 */
      Notice1("successfully connected from local address %s",
	      sockaddr_info(&la.soa, themlen, infobuff, sizeof(infobuff)));
   }

   applyopts_fchown(sfd->fd, opts);	/* OPT_USER, OPT_GROUP */
   applyopts(sfd, -1, opts, PH_CONNECTED);
#if WITH_UNIX
   if (pf == PF_UNIX && us != NULL) {
      applyopts_named(us->un.sun_path, opts, PH_LATE);
   }
#endif
   applyopts(sfd, -1, opts, PH_LATE);

   return STAT_OK;
}


/* a subroutine that is common to all socket addresses that want to connect
   to a peer address.
   might fork.
   applies and consumes the following option:
   PH_PASTSOCKET, PH_FD, PH_PREBIND, PH_BIND, PH_PASTBIND, PH_CONNECT,
   PH_CONNECTED, PH_LATE,
   OFUNC_OFFSET,
   OPT_FORK, OPT_SO_TYPE, OPT_SO_PROTOTYPE, OPT_USER, OPT_GROUP, OPT_O_CLOEXEC
   returns 0 on success.
*/
int xioopen_connect(struct single *sfd, union sockaddr_union *us, size_t uslen,
		    struct sockaddr *them, size_t themlen,
		    struct opt *opts, int pf, int socktype, int protocol,
		    bool alt) {
   bool dofork = false;
   struct opt *opts0;
   char infobuff[256];
   int level;
   int result;

   retropt_bool(opts, OPT_FORK, &dofork);

   opts0 = copyopts(opts, GROUP_ALL);

   Notice1("opening connection to %s",
	   sockaddr_info(them, themlen, infobuff, sizeof(infobuff)));

   do {	/* loop over retries and forks */

#if WITH_RETRY
      if (sfd->forever || sfd->retry) {
	 level = E_INFO;
      } else
#endif /* WITH_RETRY */
	 level = E_ERROR;
      result =
	 _xioopen_connect(sfd, us, uslen, them, themlen, opts,
			  pf, socktype, protocol, alt, level);
      switch (result) {
      case STAT_OK: break;
#if WITH_RETRY
      case STAT_RETRYLATER:
	 if (sfd->forever || sfd->retry) {
	    --sfd->retry;
	    if (result == STAT_RETRYLATER) {
	       Nanosleep(&sfd->intervall, NULL);
	    }
	    dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	    continue;
	 }
	 return STAT_NORETRY;
#endif /* WITH_RETRY */
      default:
	 return result;
      }

      if (dofork) {
	 xiosetchilddied();	/* set SIGCHLD handler */
      }

#if WITH_RETRY
      if (dofork) {
	 pid_t pid;
	 int level = E_ERROR;
	 if (sfd->forever || sfd->retry) {
	    level = E_WARN;	/* most users won't expect a problem here,
				   so Notice is too weak */
	 }

	 while ((pid = xio_fork(false, level, sfd->shutup)) < 0) {
	    --sfd->retry;
	    if (sfd->forever || sfd->retry) {
	       dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	       Nanosleep(&sfd->intervall, NULL); continue;
	    }
	    return STAT_RETRYLATER;
	 }

	 if (pid == 0) {	/* child process */
	    break;
	 }

	 /* parent process */
	 Close(sfd->fd);
	 /* with and without retry */
	 Nanosleep(&sfd->intervall, NULL);
	 dropopts(opts, PH_ALL); opts = copyopts(opts0, GROUP_ALL);
	 continue;	/* with next socket() bind() connect() */
      } else
#endif /* WITH_RETRY */
      {
	 break;
      }
#if 0
      if ((result = _xio_openlate(fd, opts)) < 0)
	 return result;
#endif
   } while (true);

   return 0;
}


/* common to xioopen_udp_sendto, ..unix_sendto, ..rawip
   applies and consumes the following option:
   PH_PASTSOCKET, PH_FD, PH_PREBIND, PH_BIND, PH_PASTBIND, PH_CONNECTED, PH_LATE
   OFUNC_OFFSET
   OPT_SO_TYPE, OPT_SO_PROTOTYPE, OPT_USER, OPT_GROUP, OPT_O_CLOEXEC
 */
int _xioopen_dgram_sendto(/* them is already in xfd->peersa */
			union sockaddr_union *us, socklen_t uslen,
			struct opt *opts,
			int xioflags, xiosingle_t *sfd, groups_t groups,
			int pf, int socktype, int ipproto, bool alt) {
   int level = E_ERROR;
   union sockaddr_union la; socklen_t lalen = sizeof(la);
   char infobuff[256];

#if WITH_UNIX
   if (pf == PF_UNIX && us != NULL) {
      applyopts_named(us->un.sun_path, opts, PH_EARLY);
   }
#endif

   if ((sfd->fd = xiosocket(opts, pf, socktype, ipproto, level)) < 0) {
      return STAT_RETRYLATER;
   }

   applyopts_offset(sfd, opts);
   applyopts_single(sfd, opts, PH_PASTSOCKET);
   applyopts(sfd, -1, opts, PH_PASTSOCKET);
   applyopts_single(sfd, opts, PH_FD);
   applyopts(sfd, -1, opts, PH_FD);

   applyopts_cloexec(sfd->fd, opts);

   if (xiobind(sfd, us, uslen, opts, pf, alt, level) < 0) {
      return -1;
   }

   /*applyopts(sfd, -1, opts, PH_CONNECT);*/

   if (Getsockname(sfd->fd, &la.soa, &lalen) < 0) {
      Warn4("getsockname(%d, %p, {%d}): %s",
	    sfd->fd, &la.soa, lalen, strerror(errno));
   }

   applyopts_fchown(sfd->fd, opts);
   applyopts(sfd, -1, opts, PH_CONNECTED);
#if WITH_UNIX
   if (pf == PF_UNIX && us != NULL) {
      applyopts_named(us->un.sun_path, opts, PH_LATE);
   }
#endif
   /*0 applyopts(sfd, -1, opts, PH_LATE); */

   /* sfd->dtype = DATA_RECVFROM; *//* no, the caller must set this (ev _SKIPIP) */
   Notice1("successfully prepared local socket %s",
	   sockaddr_info(&la.soa, lalen, infobuff, sizeof(infobuff)));

   return STAT_OK;
}


/* when the recvfrom address (with option fork) receives a packet it keeps this
   packet in the IP stacks input queue and forks a sub process. The sub process
   then reads this packet for processing its data.
   There is a problem because the parent process would find the same packet
   again if it calls select()/poll() before the child process has read the
   packet.
   To solve this problem we implement the following mechanism:
   Before forking an unnamed pipe (fifo) is created. The sub process closes the
   write side when it has read the packet. The parent process waits until the
   read side of the pipe gives EOF and only then continues to listen.
*/

/* waits for incoming packet, checks its source address and port. Depending
   on fork option, it may fork a subprocess.
   Returns STAT_OK if a packet was accepted; with fork option, this is already in
   a new subprocess!
   Other return values indicate a problem; this can happen in the master
   process or in a subprocess.
   This function does not retry. If you need retries, handle this is a
   loop in the calling function.
   after fork, we set the forever/retry of the child process to 0
   applies and consumes the following options:
   PH_INIT, PH_PREBIND, PH_BIND, PH_PASTBIND, PH_EARLY, PH_PREOPEN, PH_FD,
   PH_CONNECTED, PH_LATE, PH_LATE2
   OPT_FORK, OPT_SO_TYPE, OPT_SO_PROTOTYPE, cloexec, OPT_RANGE, tcpwrap
   EINTR is not handled specially.
 */
int _xioopen_dgram_recvfrom(struct single *sfd, int xioflags,
			  struct sockaddr *us, socklen_t uslen,
			  struct opt *opts,
			  int pf, int socktype, int proto, int level) {
   char *rangename;
   bool dofork = false;
   pid_t pid;	/* mostly int; only used with fork */
   char infobuff[256];
   char lisname[256];
   bool drop = false;	/* true if current packet must be dropped */
   int result;

   retropt_bool(opts, OPT_FORK, &dofork);

   if (dofork) {
      if (!(xioflags & XIO_MAYFORK)) {
	 Error("option fork not allowed here");
	 return STAT_NORETRY;
      }
      sfd->flags |= XIO_DOESFORK;
   }

   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return STAT_NORETRY;

   if ((sfd->fd = xiosocket(opts, pf, socktype, proto, level)) < 0) {
      return STAT_RETRYLATER;
   }

   applyopts(sfd, -1, opts, PH_PASTSOCKET);
   /*! applyopts(sfd, -1, opts, PH_FD); */

   applyopts_cloexec(sfd->fd, opts);

   if (xiobind(sfd, (union sockaddr_union *)us, uslen,
	       opts, pf, 0, level) < 0) {
      return -1;
   }

   applyopts(sfd, -1, opts, PH_PASTBIND);

#if WITH_UNIX
   if (pf == AF_UNIX && us != NULL) {
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_FD);
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_EARLY);
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_PREOPEN);
   }
#endif /* WITH_UNIX */

#if WITH_IP4 /*|| WITH_IP6*/
   switch (proto) {
   case IPPROTO_UDP:
#ifdef IPPROTO_UDPLITE
   case IPPROTO_UDPLITE:
#endif
      if (pf == PF_INET && ((struct sockaddr_in *)us)->sin_port == 0 ||
	  pf == PF_INET6 && ((struct sockaddr_in6 *)us)->sin6_port == 0) {
	 struct sockaddr_storage bound;
	 socklen_t bndlen = sizeof(bound);
	 char sockbuff[256];
	 Getsockname(sfd->fd, (struct sockaddr *)&bound, &bndlen);
	 sockaddr_info((struct sockaddr *)&bound, sizeof(struct sockaddr_storage), sockbuff, sizeof(sockbuff));
	 Notice1("_xioopen_dgram_recvfrom(): bound to %s", sockbuff);
      }
   }
#endif

#if WITH_IP4 || WITH_IP6
   /* for generic sockets, this has already been retrieved */
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
#endif /* WITH_IP4 || WITH_IP6 */

#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
   xio_retropt_tcpwrap(sfd, opts);
#endif /* && (WITH_TCP || WITH_UDP) && WITH_LIBWRAP */

   if (xioparms.logopt == 'm') {
      Info("starting recvfrom loop, switching to syslog");
      diag_set('y', xioparms.syslogfac);  xioparms.logopt = 'y';
   } else {
      Info("starting recvfrom loop");
   }

   if (dofork) {
      xiosetchilddied();
   }

   while (true) {	/* but we only loop if fork option is set */
      char peername[256];
      union sockaddr_union _peername;
      union sockaddr_union _sockname;
      union sockaddr_union *pa = &_peername;	/* peer address */
      union sockaddr_union *la = &_sockname;	/* local address */
      socklen_t palen = sizeof(_peername);	/* peer address size */
      char ctrlbuff[1024];			/* ancillary messages */
      struct msghdr msgh = {0};
      int trigger[2]; 	/* for socketpair that indicates consumption of packet */
      int rc;

      socket_init(pf, pa);

      if (drop) {
	 char *dummy[2];

	 Recv(sfd->fd, dummy, sizeof(dummy), 0);
	 drop = true;
      }

      Info("Recvfrom: Checking/waiting for next packet");
      /* loop until select()/poll() returns valid */
      do {
	 struct pollfd readfd;
	 /*? int level = E_ERROR;*/
	 if (us != NULL) {
	    Notice1("receiving on %s", sockaddr_info(us, uslen, lisname, sizeof(lisname)));
	 } else {
	    Notice1("receiving IP protocol %u", proto);
	 }
	 readfd.fd = sfd->fd;
	 readfd.events = POLLIN;
	 if (xiopoll(&readfd, 1, NULL) > 0) {
	    break;
	 }

	 if (errno == EINTR) {
	    continue;
	 }

	 Msg2(level, "poll({%d,,},,-1): %s", sfd->fd, strerror(errno));
	 Close(sfd->fd);
	 return STAT_RETRYLATER;
      } while (true);

      msgh.msg_name = pa;
      msgh.msg_namelen = palen;
#if HAVE_STRUCT_MSGHDR_MSGCONTROL
      msgh.msg_control = ctrlbuff;
#endif
#if HAVE_STRUCT_MSGHDR_MSGCONTROLLEN
      msgh.msg_controllen = sizeof(ctrlbuff);
#endif
      while ((rc = xiogetancillary(sfd->fd,
			  &msgh,
			  MSG_PEEK
#ifdef MSG_TRUNC
			  |MSG_TRUNC
#endif
				   )) < 0 &&
	     errno == EINTR) ;
      if (rc < 0)  return STAT_RETRYLATER;
      palen = msgh.msg_namelen;

      Notice1("receiving packet from %s"/*"src"*/,
	      sockaddr_info(&pa->soa, palen, peername, sizeof(peername))/*,
							     sockaddr_info(&la->soa, sockname, sizeof(sockname))*/);

      xiodopacketinfo(sfd, &msgh, true, true);

      if (xiocheckpeer(sfd, pa, la) < 0) {
	 /* drop packet */
	 char buff[512];
	 Recv(sfd->fd, buff, sizeof(buff), 0);
	 continue;
      }
      Info1("permitting packet from %s",
	    sockaddr_info(&pa->soa, palen,
			  infobuff, sizeof(infobuff)));

      /* set the env vars describing the local and remote sockets */
      /*xiosetsockaddrenv("SOCK", la, lalen, proto);*/
      xiosetsockaddrenv("PEER", pa, palen, proto);

      applyopts(sfd, -1, opts, PH_FD);

      applyopts(sfd, -1, opts, PH_CONNECTED);

      sfd->peersa = *(union sockaddr_union *)pa;
      sfd->salen = palen;

      if (dofork) {
	 Info("Generating socketpair that triggers parent when packet has been consumed");
	 if (Socketpair(PF_UNIX, SOCK_STREAM, 0, trigger) < 0) {
	    Error1("socketpair(PF_UNIX, SOCK_STREAM, 0, ...): %s", strerror(errno));
	 }

	 if ((pid = xio_fork(false, level, sfd->shutup)) < 0) {
	    Close(trigger[0]);
	    Close(trigger[1]);
	    Close(sfd->fd);
	    return STAT_RETRYLATER;
	 }

	 if (pid == 0) {	/* child */
	    Close(trigger[0]);
	    sfd->triggerfd = trigger[1];
	    Fcntl_l(sfd->triggerfd, F_SETFD, FD_CLOEXEC);

#if WITH_RETRY
	    /* !? */
	    sfd->retry = 0;
	    sfd->forever = 0;
	    level = E_ERROR;
#endif /* WITH_RETRY */

#if WITH_UNIX
	    /* with UNIX sockets: only listening parent is allowed to remove
	       the socket file */
	    sfd->opt_unlink_close = false;
#endif /* WITH_UNIX */

	    break;
	 }

	 /* Parent */
	 Close(trigger[1]);

	 {
	    char buf[1];
	    while (Read(trigger[0], buf, 1) < 0 && errno == EINTR) ;
	 }
	 Close(trigger[0]);

	 Info("continue listening");
      } else {
	break;
      }
   }
   if ((result = _xio_openlate(sfd, opts)) != 0)
      return STAT_NORETRY;

   return STAT_OK;
}


/* returns STAT_* */
int _xioopen_dgram_recv(struct single *sfd, int xioflags,
			struct sockaddr *us, socklen_t uslen,
			struct opt *opts, int pf, int socktype, int proto,
			int level) {
   char *rangename;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return STAT_NORETRY;

   if ((sfd->fd = xiosocket(opts, pf, socktype, proto, level)) < 0) {
      return STAT_RETRYLATER;
   }

   applyopts(sfd, -1, opts, PH_PASTSOCKET);
   /*! applyopts(sfd, -1, opts, PH_FD); */

   applyopts_cloexec(sfd->fd, opts);

   if (xiobind(sfd, (union sockaddr_union *)us, uslen, opts, pf, 0, level) < 0) {
      return -1;
   }

#if WITH_UNIX
   if (pf == AF_UNIX && us != NULL) {
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_FD);
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_EARLY);
      applyopts_named(((struct sockaddr_un *)us)->sun_path, opts, PH_PREOPEN);
   }
#endif /* WITH_UNIX */

#if WITH_IP4 /*|| WITH_IP6*/
   switch (proto) {
   case IPPROTO_UDP:
#ifdef IPPROTO_UDPLITE
   case IPPROTO_UDPLITE:
#endif
      if (pf == PF_INET && ((struct sockaddr_in *)us)->sin_port == 0 ||
	  pf == PF_INET6 && ((struct sockaddr_in6 *)us)->sin6_port == 0) {
	 struct sockaddr_storage bound;
	 socklen_t bndlen = sizeof(bound);
	 char sockbuff[256];
	 Getsockname(sfd->fd, (struct sockaddr *)&bound, &bndlen);
	 sockaddr_info((struct sockaddr *)&bound, sizeof(struct sockaddr_storage), sockbuff, sizeof(sockbuff));
	 Notice1("_xioopen_dgram_recv(): bound to %s", sockbuff);
      }
   }
#endif

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
#endif /* WITH_IP4 || WITH_IP6 */

#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
   xio_retropt_tcpwrap(sfd, opts);
#endif /* && (WITH_TCP || WITH_UDP) && WITH_LIBWRAP */

   if (xioparms.logopt == 'm') {
      Info("starting recv loop, switching to syslog");
      diag_set('y', xioparms.syslogfac);  xioparms.logopt = 'y';
   } else {
      Info("starting recv loop");
   }

   return STAT_OK;
}

#endif /* _WITH_SOCKET */


#if _WITH_SOCKET || _WITH_SOCKETPAIR
int retropt_socket_pf(struct opt *opts, int *pf) {
   char *pfname;

   if (retropt_string(opts, OPT_PROTOCOL_FAMILY, &pfname) >= 0) {
      if (isdigit((unsigned char)pfname[0])) {
	 *pf = strtoul(pfname, NULL /*!*/, 0);
#if WITH_IP4
      } else if (!strcasecmp("inet", pfname) ||
	  !strcasecmp("inet4", pfname) ||
	  !strcasecmp("ip4", pfname) ||
	  !strcasecmp("ipv4", pfname)) {
	 *pf = PF_INET;
#endif /* WITH_IP4 */
#if WITH_IP6
      } else if (!strcasecmp("inet6", pfname) ||
		 !strcasecmp("ip6", pfname) ||
		 !strcasecmp("ipv6", pfname)) {
	 *pf = PF_INET6;
#endif /* WITH_IP6 */
      } else {
	 Error1("unknown protocol family \"%s\"", pfname);
	 /*! Warn("falling back to INET");*/
      }
      free(pfname);
      return 0;
   }
   return -1;
}
#endif /* _WITH_SOCKET || _WITH_SOCKETPAIR */


#if _WITH_SOCKET

/* This function calls recvmsg(..., MSG_PEEK, ...) to obtain information about
   the arriving packet, thus it does not "consume" the packet.
   In msgh the msg_name pointer must refer to an (empty) sockaddr storage.
   Returns STAT_OK on success, or STAT_RETRYLATER when an error occurred,
   including EINTR.
   (recvmsg() retrieves just meta info, not the data)
 */
int xiogetancillary(int fd, struct msghdr *msgh, int flags) {
   char peekbuff[1];
#if HAVE_STRUCT_IOVEC
   struct iovec iovec;
#endif

#if HAVE_STRUCT_IOVEC
   iovec.iov_base = peekbuff;
   iovec.iov_len  = sizeof(peekbuff);
   msgh->msg_iov = &iovec;
   msgh->msg_iovlen = 1;
#endif
#if HAVE_STRUCT_MSGHDR_MSGFLAGS
   msgh->msg_flags = 0;
#endif
   if (Recvmsg(fd, msgh, flags) < 0) {
      Info1("recvmsg(): %s", strerror(errno));
      return STAT_RETRYLATER;
   }
   return STAT_OK;
}


/* works through the ancillary messages found in the given socket header record
   and logs the relevant information (E_DEBUG, E_INFO).
   calls protocol/layer specific functions for handling the messages
   creates appropriate environment vars if withenv is set */
int xiodopacketinfo(
	struct single *sfd,
	struct msghdr *msgh,
	bool withlog,
	bool withenv)
{
#if defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA)
   struct cmsghdr *cmsg;

   /* parse ancillary messages */
   cmsg = CMSG_FIRSTHDR(msgh);
   while (cmsg != NULL) {
      int num = 0;	/* number of data components of a ancill.msg */
      int i;
      char typbuff[16],  *typp;
      char nambuff[128], *namp;
      char valbuff[256], *valp;
      char envbuff[256], *envp;

      Info3("ancillary message in xiodopacketinfo(): len="F_Zu", level=%d, type=%d",
	    cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type);
      if (withlog) {
	 xiodump(CMSG_DATA(cmsg),
		 cmsg->cmsg_len-((char *)CMSG_DATA(cmsg)-(char *)cmsg),
		 valbuff, sizeof(valbuff)-1, 0);
	 Debug4("ancillary message: len="F_cmsg_len", level=%d, type=%d, data=%s",
		cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type,
		valbuff);
      }

      /* try to get the anc.msg. contents in handy components, protocol/level
	 dependent */
      switch (cmsg->cmsg_level) {
      case SOL_SOCKET:
	 xiolog_ancillary_socket(sfd, cmsg, &num, typbuff, sizeof(typbuff)-1,
				 nambuff, sizeof(nambuff)-1,
				 envbuff, sizeof(envbuff)-1,
				 valbuff, sizeof(valbuff)-1);
	 break;
#if WITH_IP4 || WITH_IP6
      case SOL_IP:
	 xiolog_ancillary_ip(sfd, cmsg, &num, typbuff, sizeof(typbuff)-1,
			     nambuff, sizeof(nambuff)-1,
			     envbuff, sizeof(envbuff)-1,
			     valbuff, sizeof(valbuff)-1);
	 break;
#endif /* WITH_IP4 || WITH_IP6 */
#if WITH_IP6
      case SOL_IPV6:
	 xiolog_ancillary_ip6(sfd, cmsg, &num, typbuff, sizeof(typbuff)-1,
			      nambuff, sizeof(nambuff)-1,
			      envbuff, sizeof(envbuff)-1,
			      valbuff, sizeof(valbuff)-1);
	 break;
#endif /* WITH_IP6 */
#if _WITH_INTERFACE && HAVE_STRUCT_CMSGHDR && HAVE_STRUCT_TPACKET_AUXDATA
      case SOL_PACKET:
	 xiolog_ancillary_packet(sfd, cmsg, &num, typbuff, sizeof(typbuff)-1,
			      nambuff, sizeof(nambuff)-1,
			      envbuff, sizeof(envbuff)-1,
			      valbuff, sizeof(valbuff)-1);
	 break;
#endif /* HAVE_STRUCT_CMSGHDR && HAVE_STRUCT_TPACKET_AUXDATA */
      default:
	 num = 1;
	 snprintf(typbuff, sizeof(typbuff)-1, "LEVEL%u", cmsg->cmsg_level);
	 snprintf(nambuff, sizeof(nambuff)-1, "type%u", cmsg->cmsg_type);
	 xiodump(CMSG_DATA(cmsg),
		 cmsg->cmsg_len-((char *)CMSG_DATA(cmsg)-(char *)cmsg),
		 valbuff, sizeof(valbuff)-1, 0);
      }
      /* here the info is in typbuff (one string), nambuff (num consecutive
	 strings), and valbuff (num consecutive strings) */
      i = 0;
      typp = typbuff;  namp = nambuff;  envp = envbuff;  valp = valbuff;
      while (i < num) {
	 if (withlog) {
	    Info3("ancillary message: %s: %s=%s", typp, namp, valp);
	 }
	 if (withenv) {
	    if (*envp) {
	       xiosetenv(envp, valp, 1, NULL);
	    } else if (!strcasecmp(typp+strlen(typp)-strlen(namp), namp)) {
	       xiosetenv(typp, valp, 1, NULL);
	    } else	{
	       xiosetenv2(typp, namp, valp, 1, NULL);
	    }
	 }
	 if (++i == num)  break;
	 namp = strchr(namp, '\0')+1;
	 envp = strchr(envp, '\0')+1;
	 valp = strchr(valp, '\0')+1;
      }
      cmsg = CMSG_NXTHDR(msgh, cmsg);
   }
   return 0;
#else /* !(defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA)) */
   return -1;
#endif /* !(defined(HAVE_STRUCT_CMSGHDR) && defined(CMSG_DATA)) */
}


/* check if peer address is within permitted range.
   return >= 0 if so. */
int xiocheckrange(union sockaddr_union *sa, struct xiorange *range) {
   switch (sa->soa.sa_family) {
#if WITH_IP4
   case PF_INET:
      return
	 xiocheckrange_ip4(&sa->ip4, range);
#endif /* WITH_IP4 */
#if WITH_IP6
   case PF_INET6:
      return
	 xiocheckrange_ip6(&sa->ip6, range);
#endif /* WITH_IP6 */
#if 0
   case PF_UNSPEC:
     {
      socklen_t i;
      for (i = 0; i < sizeof(sa->soa.sa_data); ++i) {
	 if ((range->netmask.soa.sa_data[i] & sa->soa.sa_data[i]) != range->netaddr.soa.sa_data[i]) {
	    return -1;
	 }
      }
      return 0;
     }
#endif
   }
   return -1;
}

int xiocheckpeer(xiosingle_t *sfd,
		 union sockaddr_union *pa, union sockaddr_union *la) {
   char infobuff[256];
   int result;

#if WITH_IP4 || WITH_IP6
   if (sfd->para.socket.dorange) {
      if (pa == NULL)  { return -1; }
      if (xiocheckrange(pa, &sfd->para.socket.range) < 0) {
	 char infobuff[256];
	 Warn1("refusing connection from %s due to range option",
	       sockaddr_info(&pa->soa, 0,
			     infobuff, sizeof(infobuff)));
	 return -1;
      }
      Info1("permitting connection from %s due to range option",
	    sockaddr_info(&pa->soa, 0,
			  infobuff, sizeof(infobuff)));
   }
#endif /* WITH_IP4 */

#if WITH_TCP || WITH_UDP
   if (sfd->para.socket.ip.dosourceport) {
      if (pa == NULL)  { return -1; }
#if WITH_IP4
      if (pa->soa.sa_family == AF_INET &&
	  ntohs(((struct sockaddr_in *)pa)->sin_port) != sfd->para.socket.ip.sourceport) {
	 Warn1("refusing connection from %s due to wrong sourceport",
	       sockaddr_info(&pa->soa, 0,
			     infobuff, sizeof(infobuff)));
	 return -1;
      }
#endif /* WITH_IP4 */
#if WITH_IP6
      if (pa->soa.sa_family == AF_INET6 &&
	  ntohs(((struct sockaddr_in6 *)pa)->sin6_port) != sfd->para.socket.ip.sourceport) {
	 Warn1("refusing connection from %s due to wrong sourceport",
	       sockaddr_info(&pa->soa, 0,
			     infobuff, sizeof(infobuff)));
	 return -1;
      }
#endif /* WITH_IP6 */
      Info1("permitting connection from %s due to sourceport option",
	    sockaddr_info(&pa->soa, 0,
			  infobuff, sizeof(infobuff)));
   } else if (sfd->para.socket.ip.lowport) {
      if (pa == NULL)  { return -1; }
      if (pa->soa.sa_family == AF_INET &&
	  ntohs(((struct sockaddr_in *)pa)->sin_port) >= IPPORT_RESERVED) {
	 Warn1("refusing connection from %s due to lowport option",
	       sockaddr_info(&pa->soa, 0,
			     infobuff, sizeof(infobuff)));
	 return -1;
      }
#if WITH_IP6
      else if (pa->soa.sa_family == AF_INET6 &&
	       ntohs(((struct sockaddr_in6 *)pa)->sin6_port) >=
	       IPPORT_RESERVED) {
	 Warn1("refusing connection from %s due to lowport option",
	       sockaddr_info(&pa->soa, 0,
			     infobuff, sizeof(infobuff)));
	 return -1;
      }
#endif /* WITH_IP6 */
      Info1("permitting connection from %s due to lowport option",
	    sockaddr_info(&pa->soa, 0,
			  infobuff, sizeof(infobuff)));
   }
#endif /* WITH_TCP || WITH_UDP */

#if (WITH_TCP || WITH_UDP) && WITH_LIBWRAP
   result = xio_tcpwrap_check(sfd, la, pa);
   if (result < 0) {
      char infobuff[256];
      Warn1("refusing connection from %s due to tcpwrapper option",
	    sockaddr_info(&pa->soa, 0,
			  infobuff, sizeof(infobuff)));
      return -1;
   } else if (result > 0) {
      Info1("permitting connection from %s due to tcpwrapper option",
	    sockaddr_info(&pa->soa, 0,
			  infobuff, sizeof(infobuff)));
   }
#endif /* (WITH_TCP || WITH_UDP) && WITH_LIBWRAP */

   return 0;	/* permitted */
}


#if HAVE_STRUCT_CMSGHDR
/* Converts the ancillary message in *cmsg into a form usable for further
   processing. knows the specifics of common message types.
   returns the number of resulting syntax elements in *num
   returns a sequence of \0 terminated type strings in *typbuff
   returns a sequence of \0 terminated name strings in *nambuff
   returns a sequence of \0 terminated value strings in *valbuff
   the respective len parameters specify the available space in the buffers
   returns STAT_OK or other STAT_*
 */
static int
xiolog_ancillary_socket(
	struct single *sfd,
	struct cmsghdr *cmsg,
	int *num,
	char *typbuff, int typlen,
	char *nambuff, int namlen,
	char *envbuff, int envlen,
	char *valbuff, int vallen)
{
   const char *cmsgtype, *cmsgname, *cmsgenvn;
   size_t msglen;
   struct timeval *tv;
   int rc = STAT_OK;

#if defined(CMSG_DATA)

   msglen = cmsg->cmsg_len-((char *)CMSG_DATA(cmsg)-(char *)cmsg);
   switch (cmsg->cmsg_type) {
#ifdef SO_PASSCRED
   case SO_PASSCRED:	/* this is really a UNIX/LOCAL message */
      /*! needs implementation */
#endif /* SO_PASSCRED */
#ifdef SO_RIGHTS
   case SO_RIGHTS:	/* this is really a UNIX/LOCAL message */
      /*! needs implementation */
#endif
   default:	/* binary data */
      snprintf(typbuff, typlen, "SOCKET.%u", cmsg->cmsg_type);
      nambuff[0] = '\0'; strncat(nambuff, "data", namlen-1);
      xiodump(CMSG_DATA(cmsg), msglen, valbuff, vallen, 0);
      return STAT_OK;
#ifdef SO_TIMESTAMP
#  ifdef SCM_TIMESTAMP
   case SCM_TIMESTAMP:
#  else
   case SO_TIMESTAMP:
#  endif
      tv = (struct timeval *)CMSG_DATA(cmsg);
      cmsgtype =
#ifdef SCM_TIMESTAMP
	 "SCM_TIMESTAMP"	/* FreeBSD */
#else
	 "SO_TIMESTAMP"		/* Linux */
#endif
	    ;
      cmsgname = "timestamp";
      cmsgenvn = "TIMESTAMP";
      { time_t t = tv->tv_sec; ctime_r(&t, valbuff); }
      snprintf(strchr(valbuff, '\0')-1/*del \n*/, vallen-strlen(valbuff)+1, ", %06ld usecs", (long)tv->tv_usec);
      break;
#endif /* defined(SO_TIMESTAMP) */
      ;
   }
   /* when we come here we provide a single parameter
      with type in cmsgtype, name in cmsgname,
      and value already in valbuff */
   *num = 1;
   if (strlen(cmsgtype) >= typlen)  rc = STAT_WARNING;
   typbuff[0] = '\0'; strncat(typbuff, cmsgtype, typlen-1);
   if (strlen(cmsgname) >= namlen)  rc = STAT_WARNING;
   nambuff[0] = '\0'; strncat(nambuff, cmsgname, namlen-1);
   if (strlen(cmsgenvn) >= envlen)  rc = STAT_WARNING;
   envbuff[0] = '\0'; strncat(envbuff, cmsgenvn, envlen-1);
   return rc;

#else /* !defined(CMSG_DATA) */

   return STAT_NORETRY;

#endif /* !defined(CMSG_DATA) */
}
#endif /* HAVE_STRUCT_CMSGHDR */


/* return the name of the interface with given index
   or NULL if is fails
   The system call requires an arbitrary socket; the calling program may
   provide one in parameter ins to avoid creation of a dummy socket. ins must
   be <0 if it does not specify a socket fd. */
char *xiogetifname(int ind, char *val, int ins) {
#if !HAVE_PROTOTYPE_LIB_if_indextoname
   int s;
   struct ifreq ifr;

   if (ins >= 0) {
      s = ins;
   } else {
      if ((s = Socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
	 Error1("socket(PF_INET, SOCK_DGRAM, IPPROTO_IP): %s", strerror(errno));
	 return NULL;
      }
   }

#if HAVE_STRUCT_IFREQ_IFR_INDEX
   ifr.ifr_index = ind;
#elif HAVE_STRUCT_IFREQ_IFR_IFINDEX
   ifr.ifr_ifindex = ind;
#endif
#ifdef SIOCGIFNAME
   if(Ioctl(s, SIOCGIFNAME, &ifr) < 0) {
#if HAVE_STRUCT_IFREQ_IFR_INDEX
      Info3("ioctl(%d, SIOCGIFNAME, {..., ifr_index=%d, ...}: %s",
	    s, ifr.ifr_index, strerror(errno));
#elif HAVE_STRUCT_IFREQ_IFR_IFINDEX
      Info3("ioctl(%d, SIOCGIFNAME, {..., ifr_ifindex=%d, ...}: %s",
	    s, ifr.ifr_ifindex, strerror(errno));
#endif
      if (ins < 0)  Close(s);
      return NULL;
   }
#endif /* SIOCGIFNAME */
   if (ins < 0)  Close(s);
   strcpy(val, ifr.ifr_name);
   return val;
#else /* HAVE_PROTOTYPE_LIB_if_indextoname */
   return if_indextoname(ind, val);
#endif /* HAVE_PROTOTYPE_LIB_if_indextoname */
}


/* parses a network specification consisting of an address and a mask. */
int xioparsenetwork(
	const char *rangename,
	int pf,
	struct xiorange *range,
	const int ai_flags[2])
{
   size_t addrlen = 0, masklen = 0;
   int result;

  switch (pf) {
#if WITH_IP4
  case PF_INET:
     return xioparsenetwork_ip4(rangename, range, ai_flags);
   break;
#endif /* WITH_IP4 */
#if WITH_IP6
   case PF_INET6:
      return xioparsenetwork_ip6(rangename, range, ai_flags);
      break;
#endif /* WITH_IP6 */
  case PF_UNSPEC:
    {
     char *addrname;
     const char *maskname;
     if ((maskname = strchr(rangename, ':')) == NULL) {
	Error1("syntax error in range \"%s\" of unspecified address family: use <addr>:<mask>", rangename);
	return STAT_NORETRY;
     }
     ++maskname;	/* skip ':' */
     if ((addrname = Malloc(maskname-rangename)) == NULL) {
	return STAT_NORETRY;
     }
     strncpy(addrname, rangename, maskname-rangename-1);	/* ok */
     addrname[maskname-rangename-1] = '\0';
     result =
	dalan(addrname, (uint8_t *)&range->netaddr.soa.sa_data, &addrlen,
	      sizeof(range->netaddr)-(size_t)(&((struct sockaddr *)0)->sa_data)
	      /* data length */, 'i');
     if (result < 0) {
	Error1("data too long: \"%s\"", addrname);
	free(addrname); return STAT_NORETRY;
     } else if (result > 0) {
	Error1("syntax error in \"%s\"", addrname);
	free(addrname); return STAT_NORETRY;
     }
     free(addrname);
     result =
	dalan(maskname, (uint8_t *)&range->netmask.soa.sa_data, &masklen,
	      sizeof(range->netaddr)-(size_t)(&((struct sockaddr *)0)->sa_data)
	      /* data length */, 'i');
     if (result < 0) {
	Error1("data too long: \"%s\"", maskname);
	return STAT_NORETRY;
     } else if (result > 0) {
	Error1("syntax error in \"%s\"", maskname);
	return STAT_NORETRY;
     }
	 if (addrlen != masklen) {
	    Error2("network address is "F_Zu" bytes long, mask is "F_Zu" bytes long",
		   addrlen, masklen);
	    /* recover by padding the shorter component with 0 */
	    memset((char *)&range->netaddr.soa.sa_data+addrlen, 0,
		   MAX(0, addrlen-masklen));
	    memset((char *)&range->netmask.soa.sa_data+masklen, 0,
		   MAX(0, masklen-addrlen));
	 }
    }
    break;
  default:
     Error1("range option not supported with address family %d", pf);
     return STAT_NORETRY;
  }
  return STAT_OK;
}


/* parses a string of form address/bits or address:mask, and fills the fields
   of the range union. The addr component is masked with mask. */
int xioparserange(
	const char *rangename,
	int pf,
	struct xiorange *range,
	const int ai_flags[2])
{
   int i;
   if (xioparsenetwork(rangename, pf, range, ai_flags) < 0) {
      Error2("failed to parse or resolve range \"%s\" (pf=%d)", rangename, pf);
      return -1;
   }
   /* we have parsed the address and mask; now we make sure that the stored
      address has 0 where mask is 0, to simplify comparisons */
   switch (pf) {
#if WITH_IP4
   case PF_INET:
      range->netaddr.ip4.sin_addr.s_addr &= range->netmask.ip4.sin_addr.s_addr;
      break;
#endif /* WITH_IP4 */
#if WITH_IP6
   case PF_INET6:
      return xiorange_ip6andmask(range);
      break;
#endif /* WITH_IP6 */
   case PF_UNSPEC:
      for (i = 0; i < sizeof(range->netaddr); ++i) {
	 ((char *)&range->netaddr)[i] &= ((char *)&range->netmask)[i];
      }
      break;
   default:
      Error1("range option not supported with address family %d", pf);
      return STAT_NORETRY;
   }
   return 0;
}


/* set environment variables describing (part of) a socket address, e.g.
   SOCAT_SOCKADDR. lr (local/remote) specifies a string like  "SOCK" or "PEER".
   proto should correspond to the third parameter of socket(2) and is used to
   determine the presence of port information. */
int xiosetsockaddrenv(const char *lr,
		      union sockaddr_union *sau, socklen_t salen,
		      int proto) {
#  define XIOSOCKADDRENVLEN 256
   char namebuff[XIOSOCKADDRENVLEN];
   char valuebuff[XIOSOCKADDRENVLEN];
   int idx = 0, result;

   strcpy(namebuff, lr);
   switch (sau->soa.sa_family) {
#if WITH_UNIX
   case PF_UNIX:
      result =
	 xiosetsockaddrenv_unix(idx, strchr(namebuff, '\0'), XIOSOCKADDRENVLEN-strlen(lr),
				valuebuff, XIOSOCKADDRENVLEN,
				&sau->un, salen, proto);
      xiosetenv(namebuff, valuebuff, 1, NULL);
      break;
#endif /* WITH_UNIX */
#if WITH_IP4
   case PF_INET:
      do {
	 result =
	    xiosetsockaddrenv_ip4(idx, strchr(namebuff, '\0'), XIOSOCKADDRENVLEN-strlen(lr),
				  valuebuff, XIOSOCKADDRENVLEN,
				  &sau->ip4, proto);
	 xiosetenv(namebuff, valuebuff, 1, NULL);
	 namebuff[strlen(lr)] = '\0';  ++idx;
      } while (result > 0);
      break;
#endif /* WITH_IP4 */
#if WITH_IP6
   case PF_INET6:
      strcpy(namebuff, lr);
      do {
	 result =
	    xiosetsockaddrenv_ip6(idx, strchr(namebuff, '\0'), XIOSOCKADDRENVLEN-strlen(lr),
				  valuebuff, XIOSOCKADDRENVLEN,
				  &sau->ip6, proto);
	 xiosetenv(namebuff, valuebuff, 1, NULL);
	 namebuff[strlen(lr)] = '\0';  ++idx;
      } while (result > 0);
      break;
#endif /* WITH_IP6 */
#if WITH_VSOCK
   case PF_VSOCK:
      strcpy(namebuff, lr);
      do {
	 result =
	    xiosetsockaddrenv_vsock(idx, strchr(namebuff, '\0'), XIOSOCKADDRENVLEN-strlen(lr),
				  valuebuff, XIOSOCKADDRENVLEN,
				  &sau->vm, proto);
	 xiosetenv(namebuff, valuebuff, 1, NULL);
	 namebuff[strlen(lr)] = '\0';  ++idx;
      } while (result > 0);
      break;
#endif /* WITH_VSOCK */
#if LATER
   case PF_PACKET:
      result = xiosetsockaddrenv_packet(lr, (void *)sau, proto); break;
#endif
   default:
      result = -1;
      break;
   }
   return result;
#  undef XIOSOCKADDRENVLEN
}

/* These do sockets internally */

/* retrieves options so-type and so-prototype from opts, calls socket, and
   ev. generates an appropriate error message.
   returns 0 on success or -1 (and errno) if an error occurred. */
int xiosocket(struct opt *opts, int pf, int socktype, int proto, int msglevel) {
   int result;

   retropt_int(opts, OPT_SO_TYPE, &socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, &proto);
   applyopts(NULL, -1, opts, PH_PRESOCKET);
   result = Socket(pf, socktype, proto);
   if (result < 0) {
      int _errno = errno;
      Msg4(msglevel, "socket(%d, %d, %d): %s",
	     pf, socktype, proto, strerror(errno));
      errno = _errno;
      return -1;
   }
   return result;
}

/* Binds a socket to a socket address. Handles IP (internet protocol), UNIX
   domain, Linux abstract UNIX domain.
   The bind address us may be NULL in which case no bind() happens, except with
   alt (on option unix-bind-tempname (bind-tempname)).
   Alternate (atl) bind semantics are:
      with IP sockets: lowport (selects randomly a free port from 640 to 1023)
      with UNIX and abstract sockets: uses a method similar to tmpname() to
      find a free file system entry.
   On success returns STAT_OK, otherwise errno is set.
*/
int xiobind(
	struct single *sfd,
	union sockaddr_union *us,
	size_t uslen,
	struct opt *opts,
	int pf,
	bool alt,
	int level)
{
   char infobuff[256];
   int result;

   if (false /* for canonical reasons */) {
      ;
#if WITH_UNIX
   } else if (pf == PF_UNIX) {
      if (alt && us != NULL) {
	 bool abstract = false;
	 char *usrname = NULL, *sockname;

#if WITH_ABSTRACT_UNIXSOCKET
	 abstract = (us->un.sun_path[0] == '\0');
#endif

	 if (uslen == ((char *)&us->un.sun_path-(char *)us)) {
	    usrname = NULL;
	 } else {
#if WITH_ABSTRACT_UNIXSOCKET
	    if (abstract)
	       usrname = strndup(us->un.sun_path+1, sizeof(us->un.sun_path)-1);
	    else
#endif
	       usrname = strndup(us->un.sun_path, sizeof(us->un.sun_path));
	    if (usrname	== NULL) {
	       int _errno = errno;
	       Msg2(level, "strndup(\"%s\", "F_Zu"): out of memory",
		      us->un.sun_path, sizeof(us->un.sun_path));
	       errno = _errno;
	       return STAT_RETRYLATER;
	    }
	 }

	 do {	/* loop over tempnam bind() attempts */
	    sockname = xio_tempnam(usrname, abstract);
	    if (sockname == NULL) {
	       int _errno = errno;
	       Error2("tempnam(\"%s\"): %s", usrname, strerror(errno));
	       free(usrname);
	       errno = _errno;
	       return -1;
	    }
	    strncpy(us->un.sun_path+(abstract?1:0), sockname, sizeof(us->un.sun_path));
	    uslen = sizeof(&((struct sockaddr_un *)0)->sun_path) +
	       Min(strlen(sockname), sizeof(us->un.sun_path)); /*?*/
	    free(sockname);

	    if (Bind(sfd->fd, (struct sockaddr *)us, uslen) < 0) {
	       Msg4(errno==EADDRINUSE?E_INFO:level, "bind(%d, {%s}, "F_Zd"): %s",
		    sfd->fd, sockaddr_info((struct sockaddr *)us, uslen,
					   infobuff, sizeof(infobuff)),
		    uslen, strerror(errno));
	       if (errno != EADDRINUSE) {
		  int _errno = errno;
		  free(usrname);
		  Close(sfd->fd);
		  errno = _errno;
		  return STAT_RETRYLATER;
	       }
	    } else {
	       break;	/* could bind to path, good, continue past loop */
	    }
	 } while (true);
	 free(usrname);
	 applyopts_named(us->un.sun_path, opts, PH_PREOPEN);
      } else

      if (us != NULL) {
	 if (Bind(sfd->fd, &us->soa, uslen) < 0) {
	    int _errno = errno;
	    Msg4(level, "bind(%d, {%s}, "F_Zd"): %s",
		 sfd->fd, sockaddr_info(&us->soa, uslen, infobuff, sizeof(infobuff)),
		 uslen, strerror(errno));
	    Close(sfd->fd);
	    errno = _errno;
	    return STAT_RETRYLATER;
	 }
	 applyopts_named(us->un.sun_path, opts, PH_PREOPEN);
      }

      applyopts(sfd, sfd->fd, opts, PH_PREBIND);
      applyopts(sfd, sfd->fd, opts, PH_BIND);
#endif /* WITH_UNIX */

#if WITH_TCP || WITH_UDP
   } else if (alt) {
      union sockaddr_union sin, *sinp;
      unsigned short *port, i, N;
      div_t dv;

      applyopts(sfd, sfd->fd, opts, PH_PREBIND);
      applyopts(sfd, sfd->fd, opts, PH_BIND);
      /* prepare sockaddr for bind probing */
      if (us) {
	 sinp = us;
      } else {
	 if (0) {
	    ;
#if WITH_IP4
	 } else if (pf == AF_INET) {
	    socket_in_init(&sin.ip4);
#endif
#if WITH_IP6
	 } else {
	    socket_in6_init(&sin.ip6);
#endif
	 }
	 sinp = &sin;
      }
      if (pf == AF_INET) {
	 port = &sin.ip4.sin_port;
#if WITH_IP6
      } else if (pf == AF_INET6) {
	 port = &sin.ip6.sin6_port;
#endif
      } else {
	 port = 0;	/* just to make compiler happy */
      }
      /* combine random+step variant to quickly find a free port when only
	 few are in use, and certainly find a free port in defined time even
	 if there are almost all in use */
      /* dirt 1: having tcp/udp code in socket function */
      /* dirt 2: using a time related system call for init of random */
      {
	 /* generate a random port, with millisecond random init */
#if 0
	 struct timeb tb;
	 ftime(&tb);
	 srandom(tb.time*1000+tb.millitm);
#else
	 struct timeval tv;
	 struct timezone tz;
	 tz.tz_minuteswest = 0;
	 tz.tz_dsttime = 0;
	 if ((result = Gettimeofday(&tv, &tz)) < 0) {
	    Warn2("gettimeofday(%p, {0,0}): %s", &tv, strerror(errno));
	 }
	 srandom(tv.tv_sec*1000000+tv.tv_usec);
#endif
      }
      /* Note: IPPORT_RESERVED is from includes, 1024 */
      dv = div(random(), IPPORT_RESERVED-XIO_IPPORT_LOWER);
      i = N = XIO_IPPORT_LOWER + dv.rem;
      do {	/* loop over lowport bind() attempts */
	 *port = htons(i);
	 if (Bind(sfd->fd, &sinp->soa, sizeof(*sinp)) < 0) {
	    int _errno = errno;
	    Msg4(errno==EADDRINUSE?E_INFO:level,
		 "bind(%d, {%s}, "F_Zd"): %s", sfd->fd,
		 sockaddr_info(&sinp->soa, sizeof(*sinp), infobuff, sizeof(infobuff)),
		 sizeof(*sinp), strerror(errno));
	    if (_errno != EADDRINUSE) {
	       Close(sfd->fd);
	       errno = _errno;
	       return STAT_RETRYLATER;
	    }
	 } else {
	    break;	/* could bind to port, good, continue past loop */
	 }
	 --i;  if (i < XIO_IPPORT_LOWER)  i = IPPORT_RESERVED-1;
	 if (i == N) {
	    Msg(level, "no low port available");
	    Close(sfd->fd);
	    errno = EADDRINUSE;
	    return STAT_RETRYLATER;
	 }
      } while (i != N);
#endif /* WITH_TCP || WITH_UDP */

   } else {
      applyopts(sfd, sfd->fd, opts, PH_PREBIND);
      if (us) {
	 applyopts(sfd, sfd->fd, opts, PH_BIND);
	 if (Bind(sfd->fd, &us->soa, uslen) < 0) {
	    int _errno = errno;
	    Msg4(level, "bind(%d, {%s}, "F_Zd"): %s",
		 sfd->fd, sockaddr_info(&us->soa, uslen, infobuff, sizeof(infobuff)),
		 uslen, strerror(errno));
	    Close(sfd->fd);
	    errno = _errno;
	    return STAT_RETRYLATER;
	 }
      }
   }

   applyopts(sfd, -1, opts, PH_PASTBIND);
   return STAT_OK;
}

/* Handles the SO_REUSEADDR socket option for TCP LISTEN addresses depending on
   Socat option so-reuseaddr:
   Option not applied: set it to 1
   Option applied with a value: set it to the value
   Option applied with empty value "so-reuseaddr=": do not call setsockopt() for
   SO_REUSEADDR
   Return 0 on success, or -1 with errno when an error occurred.
 */
int xiosock_reuseaddr(int fd, int ipproto, struct opt *opts)
{
	union integral val;
	union integral notnull;
	int result;
	int _errno;

	val.u_int = 0;
	notnull.u_bool = false;
#if WITH_TCP
	if (ipproto == IPPROTO_TCP) {
		val.u_int = 1;
		notnull.u_bool = true;
	}
#endif /* WITH_TCP */
	result = retropt_2integrals(opts, OPT_SO_REUSEADDR, &val, &notnull);
#if WITH_TCP
	if (ipproto == IPPROTO_TCP && result < 0) {
		Info("Setting SO_REUSADDR on TCP listen socket implicitly");
		val.u_int = 1;
		notnull.u_bool = true;
	}
#endif /* WITH_TCP */
	if (notnull.u_bool) {
		if (Setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val.u_int, sizeof(int))
		    != 0) {
			_errno = errno;
			Error4("setsockopt(%d, SOL_SOCKET, SO_REUSEADDR, { %d }, "F_Zu"): %s",
			       fd, val.u_int, sizeof(val.u_int), strerror(errno));
			errno = _errno;
			return -1;
		}
	}
	return 0;
}

#endif /* _WITH_SOCKET */

