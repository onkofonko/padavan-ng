/* source: xioopen.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source file of the extended open function */

#include "xiosysincludes.h"

#include "xioopen.h"
#include "xiomodes.h"
#include "xiohelp.h"
#include "nestlex.h"

static xiofile_t *xioallocfd(void);

static xiosingle_t *xioparse_single(const char **addr);
static xiofile_t *xioparse_dual(const char **addr);
static int xioopen_dual(xiofile_t *xfd, int xioflags);

const struct addrname addressnames[] = {
#if 1
#if WITH_STDIO
   { "-",			&xioaddr_stdio },
#endif
#if defined(WITH_UNIX) && defined(WITH_ABSTRACT_UNIXSOCKET)
   { "ABSTRACT",		&xioaddr_abstract_client },
   { "ABSTRACT-CLIENT",		&xioaddr_abstract_client },
   { "ABSTRACT-CONNECT",	&xioaddr_abstract_connect },
#if WITH_LISTEN
   { "ABSTRACT-LISTEN",		&xioaddr_abstract_listen },
#endif
   { "ABSTRACT-RECV",		&xioaddr_abstract_recv },
   { "ABSTRACT-RECVFROM",	&xioaddr_abstract_recvfrom },
   { "ABSTRACT-SENDTO",		&xioaddr_abstract_sendto },
#endif /* defined(WITH_UNIX) && defined(WITH_ABSTRACT_UNIXSOCKET) */
#if WITH_LISTEN && WITH_FDNUM
   { "ACCEPT", 			&xioaddr_accept_fd },
   { "ACCEPT-FD", 		&xioaddr_accept_fd },
#endif
#if WITH_CREAT
   { "CREAT",			&xioaddr_creat },
   { "CREATE",			&xioaddr_creat },
#endif
#if WITH_GENERICSOCKET
   { "DATAGRAM",		&xioaddr_socket_datagram },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_DCCP
   { "DCCP",			&xioaddr_dccp_connect },
   { "DCCP-CONNECT",		&xioaddr_dccp_connect },
#if WITH_LISTEN
   { "DCCP-L",			&xioaddr_dccp_listen },
   { "DCCP-LISTEN",		&xioaddr_dccp_listen },
#endif
#if WITH_IP4
   { "DCCP4",			&xioaddr_dccp4_connect },
   { "DCCP4-CONNECT",		&xioaddr_dccp4_connect },
#if WITH_LISTEN
   { "DCCP4-L",			&xioaddr_dccp4_listen },
   { "DCCP4-LISTEN",		&xioaddr_dccp4_listen },
#endif
#endif /* WITH_IP4 */
#if WITH_IP6
   { "DCCP6",			&xioaddr_dccp6_connect },
   { "DCCP6-CONNECT",		&xioaddr_dccp6_connect },
#if WITH_LISTEN
   { "DCCP6-L",			&xioaddr_dccp6_listen },
   { "DCCP6-LISTEN",		&xioaddr_dccp6_listen },
#endif
#endif /* WITH_IP6 */
#endif /* (WITH_IP4 || WITH_IP6) && WITH_DCCP */
#if WITH_GENERICSOCKET
   { "DGRAM",			&xioaddr_socket_datagram },
#endif
#if WITH_OPENSSL
   { "DTLS",		&xioaddr_openssl_dtls_client },
   { "DTLS-C",		&xioaddr_openssl_dtls_client },
   { "DTLS-CLIENT",	&xioaddr_openssl_dtls_client },
   { "DTLS-CONNECT",	&xioaddr_openssl_dtls_client },
#if WITH_LISTEN
   { "DTLS-L",		&xioaddr_openssl_dtls_server },
   { "DTLS-LISTEN",	&xioaddr_openssl_dtls_server },
   { "DTLS-SERVER",	&xioaddr_openssl_dtls_server },
#endif
#endif
#if WITH_PIPE
   { "ECHO",			&xioaddr_pipe },
#endif
#if WITH_EXEC
   { "EXEC",			&xioaddr_exec },
#endif
#if WITH_FDNUM
   { "FD",			&xioaddr_fd },
#endif
#if WITH_PIPE
   { "FIFO",			&xioaddr_pipe },
#endif
#if WITH_FILE
   { "FILE",			&xioaddr_open },
#endif
#if WITH_GOPEN
   { "GOPEN",			&xioaddr_gopen },
#endif
#if WITH_INTERFACE
   { "IF",		&xioaddr_interface },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_TCP
   { "INET",			&xioaddr_tcp_connect },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_TCP && WITH_LISTEN
   { "INET-L",			&xioaddr_tcp_listen },
   { "INET-LISTEN",		&xioaddr_tcp_listen },
#endif
#if WITH_IP4 && WITH_TCP
   { "INET4",			&xioaddr_tcp4_connect },
#endif
#if WITH_IP4 && WITH_TCP && WITH_LISTEN
   { "INET4-L",			&xioaddr_tcp4_listen },
   { "INET4-LISTEN",		&xioaddr_tcp4_listen },
#endif
#if WITH_IP6 && WITH_TCP
   { "INET6",			&xioaddr_tcp6_connect },
#endif
#if WITH_IP6 && WITH_TCP && WITH_LISTEN
   { "INET6-L",			&xioaddr_tcp6_listen },
   { "INET6-LISTEN",		&xioaddr_tcp6_listen },
#endif
#if WITH_INTERFACE
   { "INTERFACE",	&xioaddr_interface },
#endif
#if WITH_RAWIP
#if (WITH_IP4 || WITH_IP6)
   { "IP",			&xioaddr_rawip_sendto },
   { "IP-DATAGRAM",		&xioaddr_rawip_datagram },
   { "IP-DGRAM",		&xioaddr_rawip_datagram },
   { "IP-RECV",			&xioaddr_rawip_recv },
   { "IP-RECVFROM",		&xioaddr_rawip_recvfrom },
   { "IP-SEND",			&xioaddr_rawip_sendto },
   { "IP-SENDTO",		&xioaddr_rawip_sendto },
#endif
#if WITH_IP4
   { "IP4",			&xioaddr_rawip4_sendto },
   { "IP4-DATAGRAM",		&xioaddr_rawip4_datagram },
   { "IP4-DGRAM",		&xioaddr_rawip4_datagram },
   { "IP4-RECV",		&xioaddr_rawip4_recv },
   { "IP4-RECVFROM",		&xioaddr_rawip4_recvfrom },
   { "IP4-SEND",		&xioaddr_rawip4_sendto },
   { "IP4-SENDTO",		&xioaddr_rawip4_sendto },
#endif
#if WITH_IP6
   { "IP6",			&xioaddr_rawip6_sendto },
   { "IP6-DATAGRAM",		&xioaddr_rawip6_datagram },
   { "IP6-DGRAM",		&xioaddr_rawip6_datagram },
   { "IP6-RECV",		&xioaddr_rawip6_recv },
   { "IP6-RECVFROM",		&xioaddr_rawip6_recvfrom },
   { "IP6-SEND",		&xioaddr_rawip6_sendto },
   { "IP6-SENDTO",		&xioaddr_rawip6_sendto },
#endif
#endif /* WITH_RAWIP */
#if WITH_UNIX
   { "LOCAL",	&xioaddr_unix_connect },
#endif
#if WITH_FILE
   { "OPEN",			&xioaddr_open },
#endif
#if WITH_OPENSSL
   { "OPENSSL",		&xioaddr_openssl },
   { "OPENSSL-CONNECT",		&xioaddr_openssl },
   { "OPENSSL-DTLS-CLIENT",	&xioaddr_openssl_dtls_client },
   { "OPENSSL-DTLS-CONNECT",	&xioaddr_openssl_dtls_client },
#if WITH_LISTEN
   { "OPENSSL-DTLS-LISTEN",		&xioaddr_openssl_dtls_server },
   { "OPENSSL-DTLS-SERVER",		&xioaddr_openssl_dtls_server },
   { "OPENSSL-LISTEN",		&xioaddr_openssl_listen },
#endif
#endif
#if WITH_PIPE
   { "PIPE",			&xioaddr_pipe },
#endif
#if WITH_POSIXMQ
   { "POSIXMQ", 		&xioaddr_posixmq_bidir },
   { "POSIXMQ-BIDIRECTIONAL", 	&xioaddr_posixmq_bidir },
   { "POSIXMQ-READ", 		&xioaddr_posixmq_read },
   { "POSIXMQ-RECEIVE", 	&xioaddr_posixmq_receive },
   { "POSIXMQ-RECV", 		&xioaddr_posixmq_receive },
   { "POSIXMQ-SEND", 		&xioaddr_posixmq_send },
   { "POSIXMQ-WRITE", 		&xioaddr_posixmq_write },
#endif
#if WITH_PROXY
   { "PROXY",			&xioaddr_proxy_connect },
   { "PROXY-CONNECT",		&xioaddr_proxy_connect },
#endif
#if WITH_PTY
   { "PTY",			&xioaddr_pty },
#endif
#if WITH_READLINE
   { "READLINE",		&xioaddr_readline },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_SCTP
   { "SCTP",			&xioaddr_sctp_connect },
   { "SCTP-CONNECT",		&xioaddr_sctp_connect },
#if WITH_LISTEN
   { "SCTP-L",			&xioaddr_sctp_listen },
   { "SCTP-LISTEN",		&xioaddr_sctp_listen },
#endif
#if WITH_IP4
   { "SCTP4",			&xioaddr_sctp4_connect },
   { "SCTP4-CONNECT",		&xioaddr_sctp4_connect },
#if WITH_LISTEN
   { "SCTP4-L",			&xioaddr_sctp4_listen },
   { "SCTP4-LISTEN",		&xioaddr_sctp4_listen },
#endif
#endif /* WITH_IP4 */
#if WITH_IP6
   { "SCTP6",			&xioaddr_sctp6_connect },
   { "SCTP6-CONNECT",		&xioaddr_sctp6_connect },
#if WITH_LISTEN
   { "SCTP6-L",			&xioaddr_sctp6_listen },
   { "SCTP6-LISTEN",		&xioaddr_sctp6_listen },
#endif
#endif /* WITH_IP6 */
#endif /* (WITH_IP4 || WITH_IP6) && WITH_SCTP */
#if WITH_GENERICSOCKET
   { "SENDTO",			&xioaddr_socket_sendto },
#endif
#if WITH_SHELL
   { "SHELL",			&xioaddr_shell },
#endif
#if WITH_GENERICSOCKET
   { "SOCKET-CONNECT",		&xioaddr_socket_connect },
   { "SOCKET-DATAGRAM",		&xioaddr_socket_datagram },
#if WITH_LISTEN
   { "SOCKET-LISTEN",		&xioaddr_socket_listen },
#endif /* WITH_LISTEN */
   { "SOCKET-RECV",		&xioaddr_socket_recv },
   { "SOCKET-RECVFROM",		&xioaddr_socket_recvfrom },
   { "SOCKET-SENDTO",		&xioaddr_socket_sendto },
#endif
#if WITH_SOCKETPAIR
   { "SOCKETPAIR",		&xioaddr_socketpair },
#endif
#if WITH_SOCKS4
   { "SOCKS",			&xioaddr_socks4_connect },
   { "SOCKS4",			&xioaddr_socks4_connect },
#endif
#if WITH_SOCKS4A
   { "SOCKS4A",			&xioaddr_socks4a_connect },
#endif
#if WITH_SOCKS5
   { "SOCKS5", 		&xioaddr_socks5_connect },
   { "SOCKS5-BIND", 	&xioaddr_socks5_listen },
   { "SOCKS5-CONNECT", 	&xioaddr_socks5_connect },
   { "SOCKS5-LISTEN", 	&xioaddr_socks5_listen },
#endif
#if WITH_OPENSSL
   { "SSL",		&xioaddr_openssl },
#if WITH_LISTEN
   { "SSL-L",		&xioaddr_openssl_listen },
#endif
#endif
#if WITH_STDIO
   { "STDERR",			&xioaddr_stderr },
   { "STDIN",			&xioaddr_stdin },
   { "STDIO",			&xioaddr_stdio },
   { "STDOUT",			&xioaddr_stdout },
#endif
#if WITH_SYSTEM
   { "SYSTEM",			&xioaddr_system },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_TCP
   { "TCP",			&xioaddr_tcp_connect },
   { "TCP-CONNECT",		&xioaddr_tcp_connect },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_TCP && WITH_LISTEN
   { "TCP-L",			&xioaddr_tcp_listen },
   { "TCP-LISTEN",		&xioaddr_tcp_listen },
#endif
#if WITH_IP4 && WITH_TCP
   { "TCP4",			&xioaddr_tcp4_connect },
   { "TCP4-CONNECT",		&xioaddr_tcp4_connect },
#endif
#if WITH_IP4 && WITH_TCP && WITH_LISTEN
   { "TCP4-L",			&xioaddr_tcp4_listen },
   { "TCP4-LISTEN",		&xioaddr_tcp4_listen },
#endif
#if WITH_IP6 && WITH_TCP
   { "TCP6",			&xioaddr_tcp6_connect },
   { "TCP6-CONNECT",		&xioaddr_tcp6_connect },
#endif
#if WITH_IP6 && WITH_TCP && WITH_LISTEN
   { "TCP6-L",			&xioaddr_tcp6_listen },
   { "TCP6-LISTEN",		&xioaddr_tcp6_listen },
#endif
#if WITH_TUN
   { "TUN",		&xioaddr_tun },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDP
   { "UDP",			&xioaddr_udp_connect },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDP
   { "UDP-CONNECT",		&xioaddr_udp_connect },
   { "UDP-DATAGRAM",		&xioaddr_udp_datagram },
   { "UDP-DGRAM",		&xioaddr_udp_datagram },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDP && WITH_LISTEN
   { "UDP-L",			&xioaddr_udp_listen },
   { "UDP-LISTEN",		&xioaddr_udp_listen },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDP
   { "UDP-RECV",		&xioaddr_udp_recv },
   { "UDP-RECVFROM",		&xioaddr_udp_recvfrom },
   { "UDP-SEND",		&xioaddr_udp_sendto },
   { "UDP-SENDTO",		&xioaddr_udp_sendto },
#endif
#if WITH_IP4 && WITH_UDP
   { "UDP4",			&xioaddr_udp4_connect },
   { "UDP4-CONNECT",		&xioaddr_udp4_connect },
   { "UDP4-DATAGRAM",		&xioaddr_udp4_datagram },
   { "UDP4-DGRAM",		&xioaddr_udp4_datagram },
#endif
#if WITH_IP4 && WITH_UDP && WITH_LISTEN
   { "UDP4-L",			&xioaddr_udp4_listen },
   { "UDP4-LISTEN",		&xioaddr_udp4_listen },
#endif
#if WITH_IP4 && WITH_UDP
   { "UDP4-RECV",		&xioaddr_udp4_recv },
   { "UDP4-RECVFROM",		&xioaddr_udp4_recvfrom },
   { "UDP4-SEND",		&xioaddr_udp4_sendto },
   { "UDP4-SENDTO",		&xioaddr_udp4_sendto },
#endif
#if WITH_IP6 && WITH_UDP
   { "UDP6",			&xioaddr_udp6_connect },
   { "UDP6-CONNECT",		&xioaddr_udp6_connect },
   { "UDP6-DATAGRAM",		&xioaddr_udp6_datagram },
   { "UDP6-DGRAM",		&xioaddr_udp6_datagram },
#endif
#if WITH_IP6 && WITH_UDP && WITH_LISTEN
   { "UDP6-L",			&xioaddr_udp6_listen },
   { "UDP6-LISTEN",		&xioaddr_udp6_listen },
#endif
#if WITH_IP6 && WITH_UDP
   { "UDP6-RECV",		&xioaddr_udp6_recv },
   { "UDP6-RECVFROM",		&xioaddr_udp6_recvfrom },
   { "UDP6-SEND",		&xioaddr_udp6_sendto },
   { "UDP6-SENDTO",		&xioaddr_udp6_sendto },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDPLITE
   { "UDPLITE",			&xioaddr_udplite_connect },
   { "UDPLITE-CONNECT",		&xioaddr_udplite_connect },
   { "UDPLITE-DATAGRAM",	&xioaddr_udplite_datagram },
   { "UDPLITE-DGRAM",		&xioaddr_udplite_datagram },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDPLITE && WITH_LISTEN
   { "UDPLITE-L",		&xioaddr_udplite_listen },
   { "UDPLITE-LISTEN",		&xioaddr_udplite_listen },
#endif
#if (WITH_IP4 || WITH_IP6) && WITH_UDPLITE
   { "UDPLITE-RECV",		&xioaddr_udplite_recv },
   { "UDPLITE-RECVFROM",	&xioaddr_udplite_recvfrom },
   { "UDPLITE-SEND",		&xioaddr_udplite_sendto },
   { "UDPLITE-SENDTO",		&xioaddr_udplite_sendto },
#endif
#if WITH_IP4 && WITH_UDPLITE
   { "UDPLITE4",		&xioaddr_udplite4_connect },
   { "UDPLITE4-CONNECT",	&xioaddr_udplite4_connect },
   { "UDPLITE4-DATAGRAM",	&xioaddr_udplite4_datagram },
   { "UDPLITE4-DGRAM",		&xioaddr_udplite4_datagram },
#endif
#if WITH_IP4 && WITH_UDPLITE && WITH_LISTEN
   { "UDPLITE4-L",		&xioaddr_udplite4_listen },
   { "UDPLITE4-LISTEN",		&xioaddr_udplite4_listen },
#endif
#if WITH_IP4 && WITH_UDPLITE
   { "UDPLITE4-RECV",		&xioaddr_udplite4_recv },
   { "UDPLITE4-RECVFROM",	&xioaddr_udplite4_recvfrom },
   { "UDPLITE4-SEND",		&xioaddr_udplite4_sendto },
   { "UDPLITE4-SENDTO",		&xioaddr_udplite4_sendto },
#endif
#if WITH_IP6 && WITH_UDPLITE
   { "UDPLITE6",		&xioaddr_udplite6_connect },
   { "UDPLITE6-CONNECT",	&xioaddr_udplite6_connect },
   { "UDPLITE6-DATAGRAM",	&xioaddr_udplite6_datagram },
   { "UDPLITE6-DGRAM",		&xioaddr_udplite6_datagram },
#endif
#if WITH_IP6 && WITH_UDPLITE && WITH_LISTEN
   { "UDPLITE6-L",		&xioaddr_udplite6_listen },
   { "UDPLITE6-LISTEN",		&xioaddr_udplite6_listen },
#endif
#if WITH_IP6 && WITH_UDPLITE
   { "UDPLITE6-RECV",		&xioaddr_udplite6_recv },
   { "UDPLITE6-RECVFROM",	&xioaddr_udplite6_recvfrom },
   { "UDPLITE6-SEND",		&xioaddr_udplite6_sendto },
   { "UDPLITE6-SENDTO",		&xioaddr_udplite6_sendto },
#endif
#if WITH_UNIX
   { "UNIX",		&xioaddr_unix_client },
   { "UNIX-CLIENT",	&xioaddr_unix_client },
   { "UNIX-CONNECT",	&xioaddr_unix_connect },
#endif
#if WITH_UNIX && WITH_LISTEN
   { "UNIX-L",		&xioaddr_unix_listen },
   { "UNIX-LISTEN",	&xioaddr_unix_listen },
#endif
#if WITH_UNIX
   { "UNIX-RECV",	&xioaddr_unix_recv },
   { "UNIX-RECVFROM",	&xioaddr_unix_recvfrom },
   { "UNIX-SEND",	&xioaddr_unix_sendto },
   { "UNIX-SENDTO",	&xioaddr_unix_sendto },
#endif
#if WITH_VSOCK
   { "VSOCK",			&xioaddr_vsock_connect },
   { "VSOCK-CONNECT",		&xioaddr_vsock_connect },
#endif
#if WITH_VSOCK && WITH_LISTEN
   { "VSOCK-L",			&xioaddr_vsock_listen },
   { "VSOCK-LISTEN",		&xioaddr_vsock_listen },
#endif
#else /* !0 */
#  if WITH_INTEGRATE
#    include "xiointegrate.c"
#  else
#    include "xioaddrtab.c"
#  endif
#endif /* !0 */
   { NULL }	/* end marker */
} ;

int xioopen_single(xiofile_t *xfd, int xioflags);


/* prepares a xiofile_t record for dual address type:
   sets the tag and allocates memory for the substreams.
   returns 0 on success, or <0 if an error occurred.
*/
int xioopen_makedual(xiofile_t *file) {
   file->tag = XIO_TAG_DUAL;
   file->common.flags = XIO_RDWR;
   if ((file->dual.stream[0] = (xiosingle_t *)xioallocfd()) == NULL)
      return -1;
   file->dual.stream[0]->flags = XIO_RDONLY;
   if ((file->dual.stream[1] = (xiosingle_t *)xioallocfd()) == NULL)
      return -1;
   file->dual.stream[1]->flags = XIO_WRONLY;
   return 0;
}

static xiofile_t *xioallocfd(void) {
   xiofile_t *fd;

   if ((fd = Calloc(1, sizeof(xiofile_t))) == NULL) {
      return NULL;
   }
   /* some default values; 0's and NULL's need not be applied (calloc'ed) */
   fd->common.tag       = XIO_TAG_INVALID;
/* fd->common.addr      = NULL; */
   fd->common.flags     = XIO_RDWR;

#if WITH_RETRY
/* fd->stream.retry	= 0; */
/* fd->stream.forever	= false; */
   fd->stream.intervall.tv_sec  = 1;
/* fd->stream.intervall.tv_nsec = 0; */
#endif /* WITH_RETRY */
/* fd->common.ignoreeof = false; */
/* fd->common.eof       = 0; */
   fd->stream.triggerfd = -1;
   fd->stream.fd        = -1;
   fd->stream.dtype     = XIODATA_STREAM;
#if _WITH_SOCKET
/* fd->stream.salen     = 0; */
#endif /* _WITH_SOCKET */
   fd->stream.howtoend  = END_UNSPEC;
/* fd->stream.name      = NULL; */
   fd->stream.escape	= -1;
/* fd->stream.para.exec.pid = 0; */
   fd->stream.lineterm  = LINETERM_RAW;
#if ( _WITH_IP4 || _WITH_IP6 ) && WITH_RESOLVE
#if HAVE_RES_RETRANS
   fd->stream.para.socket.ip.res.retrans = -1;
#endif
#if HAVE_RES_RETRY
   fd->stream.para.socket.ip.res.retry   = -1;
#endif
#endif /* ( _WITH_IP4 || _WITH_IP6 ) && WITH_RESOLVE */
   return fd;
}


/* parse the argument that specifies a two-directional data stream
   and open the resulting address
 */
xiofile_t *xioopen(const char *addr,	/* address specification */
		   int xioflags) {
   xiofile_t *xfd;

   Debug1("xioopen(\"%s\")", addr);

   if ((xfd = xioparse_dual(&addr)) == NULL) {
      return NULL;
   }
   /*!! support n socks */
   if (!sock[0]) {
      sock[0] = xfd;
   } else {
      sock[1] = xfd;
   }
   if (xioopen_dual(xfd, xioflags) < 0) {
      /*!!! free something? */
      return NULL;
   }

   return xfd;
}

/* parse an address string that might contain !!
   return NULL on error */
static xiofile_t *xioparse_dual(const char **addr) {
   xiofile_t *xfd;
   xiosingle_t *sfd1;

   /* we parse a single address */
   if ((sfd1 = xioparse_single(addr)) == NULL) {
      return NULL;
   }

   /* and now we see if we reached a dual-address separator */
   if (!strncmp(*addr, xioparms.pipesep, strlen(xioparms.pipesep))) {
      /* yes we reached it, so we parse the second single address */
      *addr += strlen(xioparms.pipesep);

      if ((xfd = xioallocfd()) == NULL) {
	 free(sfd1); /*! and maybe have free some if its contents */
	 return NULL;
      }
      xfd->tag = XIO_TAG_DUAL;
      xfd->dual.stream[0] = sfd1;
      if ((xfd->dual.stream[1] = xioparse_single(addr)) == NULL) {
	 return NULL;
      }

      return xfd;
   }

   /* a truly single address */
   xfd = (xiofile_t *)sfd1; sfd1 = NULL;

   return xfd;
}

static int xioopen_dual(xiofile_t *xfd, int xioflags) {

   if (xfd->tag == XIO_TAG_DUAL) {
      /* a really dual address */
      if ((xioflags&XIO_ACCMODE) != XIO_RDWR) {
	 Warn("unidirectional open of dual address");
      }
      if (((xioflags&XIO_ACCMODE)+1) & (XIO_RDONLY+1)) {
	 if (xioopen_single((xiofile_t *)xfd->dual.stream[0], XIO_RDONLY|(xioflags&~XIO_ACCMODE&~XIO_MAYEXEC))
	     < 0) {
	    return -1;
	 }
      }
      if (((xioflags&XIO_ACCMODE)+1) & (XIO_WRONLY+1)) {
	 if (xioopen_single((xiofile_t *)xfd->dual.stream[1], XIO_WRONLY|(xioflags&~XIO_ACCMODE&~XIO_MAYEXEC))
	     < 0) {
	    xioclose((xiofile_t *)xfd->dual.stream[0]);
	    return -1;
	 }
      }
      return 0;
   }

   return xioopen_single(xfd, xioflags);
}


static xiosingle_t *xioparse_single(const char **addr) {
   const char *addr0 = *addr; 	/* save for error messages */
   xiofile_t *xfd;
   xiosingle_t *sfd;
   struct addrname *ae;
   const struct addrdesc *addrdesc = NULL;
   const char *ends[4+1];
   const char *hquotes[] = {
      "'",
      NULL
   } ;
   const char *squotes[] = {
      "\"",
      NULL
   } ;
   const char *nests[] = {
      "'", "'",
      "(", ")",
      "[", "]",
      "{", "}",
      NULL
   } ;
   char token[512], *tokp;
   size_t len;
   int i;
   int result;

   /* init */
   i = 0;
   /*ends[i++] = xioparms.chainsep;*/	/* default: "|" */
   ends[i++] = xioparms.pipesep;		/* default: "!!" */
   ends[i++] = ","/*xioparms.comma*/;		/* default: "," */
   ends[i++] = ":"/*xioparms.colon*/;		/* default: ":" */
   ends[i++] = NULL;

   if ((xfd = xioallocfd()) == NULL) {
      return NULL;
   }
   sfd = &xfd->stream;
   sfd->argc = 0;

   len = sizeof(token); tokp = token;
   result = nestlex(addr, &tokp, &len, ends, hquotes, squotes, nests,
		    true, true, false);
   if (result < 0) {
      Error2("keyword too long, in address \"%s%s\"", token, *addr);
   } else if (result > 0){
      Error1("unexpected end of address \"%s\"", *addr);
   }
   *tokp = '\0';  /*! len? */
   ae = (struct addrname *)
      keyw((struct wordent *)&addressnames, token,
	   sizeof(addressnames)/sizeof(struct addrname)-1);

   if (ae) {
      addrdesc = ae->desc;
      /* keyword */
      if ((sfd->argv[sfd->argc++] = strdup(token)) == NULL) {
	 Error1("strdup(\"%s\"): out of memory", token);
      }
   } else {
      if (false) {
	 ;
#if WITH_FDNUM
      } else if (isdigit(token[0]&0xff) && token[1] == '\0') {
	 Info1("interpreting address \"%s\" as file descriptor", token);
	 addrdesc = &xioaddr_fd;
	 if ((sfd->argv[sfd->argc++] = strdup("FD")) == NULL) {
	    Error("strdup(\"FD\"): out of memory");
	 }
	 if ((sfd->argv[sfd->argc++] = strdup(token)) == NULL) {
	    Error1("strdup(\"%s\"): out of memory", token);
	 }
	 /*! check argc overflow */
#endif /* WITH_FDNUM */
#if WITH_GOPEN
      } else if (strchr(token, '/')) {
	 Info1("interpreting address \"%s\" as file name", token);
	 addrdesc = &xioaddr_gopen;
	 if ((sfd->argv[sfd->argc++] = strdup("GOPEN")) == NULL) {
	    Error("strdup(\"GOPEN\"): out of memory");
	 }
	 if ((sfd->argv[sfd->argc++] = strdup(token)) == NULL) {
	    Error1("strdup(\"%s\"): out of memory", token);
	 }
	 /*! check argc overflow */
#endif /* WITH_GOPEN */
      } else {
	 Error1("unknown device/address \"%s\"", token);
	 /*!!! free something*/ return NULL;
      }
   }

   sfd->tag  = XIO_TAG_RDWR;
   sfd->addr = addrdesc;

   while (!strncmp(*addr, xioparms.paramsep, strlen(xioparms.paramsep))) {
      *addr += strlen(xioparms.paramsep);
      len = sizeof(token);  tokp = token;
      result = nestlex(addr, &tokp, &len, ends, hquotes, squotes, nests,
		       true, true, false);
      if (result < 0) {
	 Error1("address \"%s\" too long", addr0);
      } else if (result > 0){
	 Error1("unexpected end of address \"%s\"", addr0);
      }
      *tokp = '\0';
      if ((sfd->argv[sfd->argc++] = strdup(token)) == NULL) {
	 Error1("strdup(\"%s\"): out of memory", token);
      }
   }

   if (parseopts(addr, addrdesc->groups, &sfd->opts) < 0) {
      free(xfd);
      return NULL;
   }

   return sfd;
}

int xioopen_single(xiofile_t *xfd, int xioflags) {
   struct single *sfd = &xfd->stream;
   const struct addrdesc *addrdesc;
   const char *modetext[4] = { "none", "read-only", "write-only", "read-write" } ;
   /* Values to be saved until xioopen() is finished */
   char *orig_dir = NULL;
   bool have_umask = false;
   mode_t orig_umask, tmp_umask;
   int result;
   /* Values to be saved until xioopen() is finished */
#if ( _WITH_IP4 || _WITH_IP6 ) && WITH_RESOLVE && HAVE_RESOLV_H
   int do_res;
   struct __res_state save_res;
#endif
#if WITH_NAMESPACES
   int save_netfd = -1;
#endif

   addrdesc = xfd->stream.addr;
   if (((xioflags+1)&XIO_ACCMODE) & ~(addrdesc->directions)) {
      Warn2("address is opened in %s mode but only supports %s", modetext[(xioflags+1)&XIO_ACCMODE], modetext[addrdesc->directions]);
   }
   if ((xioflags&XIO_ACCMODE) == XIO_RDONLY) {
      xfd->tag = XIO_TAG_RDONLY;
   } else if ((xioflags&XIO_ACCMODE) == XIO_WRONLY) {
      xfd->tag = XIO_TAG_WRONLY;
   } else if ((xioflags&XIO_ACCMODE) == XIO_RDWR) {
      xfd->tag = XIO_TAG_RDWR;
   } else {
      Error1("invalid mode for address \"%s\"", xfd->stream.argv[0]);
   }
   xfd->stream.flags     &= (~XIO_ACCMODE);
   xfd->stream.flags     |= (xioflags & XIO_ACCMODE);

   /* Apply "temporary" process properties, save value for later restore */

   if (applyopts_single(sfd, sfd->opts, PH_OFFSET) < 0)
      return -1;

#if WITH_NAMESPACES 	/* netns */
   if ((save_netfd = xio_apply_namespace(sfd->opts)) < 0)
      return -1;
#endif /* WITH_NAMESPACES */

#if ( _WITH_IP4 || _WITH_IP6 ) && WITH_RESOLVE && HAVE_RESOLV_H
   if ((do_res = xio_res_init(sfd, &save_res)) < 0)
      return STAT_NORETRY;
#endif

   if (xio_chdir(sfd->opts, &orig_dir) < 0)
      return STAT_NORETRY;

   if (retropt_mode(xfd->stream.opts, OPT_UMASK, &tmp_umask) >= 0) {
      Info1("changing umask to 0%3o", tmp_umask);
      orig_umask = Umask(tmp_umask);
      have_umask = true;
   }

   /* Call the specific xioopen function */
   result = (*addrdesc->func)(xfd->stream.argc, xfd->stream.argv,
			      xfd->stream.opts, xioflags, xfd,
			      addrdesc);

   /* Restore process properties */
   if (have_umask) {
      Info1("restoring umask to 0%3o", orig_umask);
      Umask(orig_umask);
   }

   if (orig_dir != NULL) {
      if (Chdir(orig_dir) < 0) {
	 Error2("chdir(\"%s\"): %s", orig_dir, strerror(errno));
	 free(orig_dir);
	 return STAT_NORETRY;
      }
      free(orig_dir);
   }

#if ( _WITH_IP4 || _WITH_IP6 ) && WITH_RESOLVE && HAVE_RESOLV_H
   if (do_res)
      xio_res_restore(&save_res);
#endif

#if WITH_NAMESPACES
   if (save_netfd > 0) {
      xio_reset_namespace(save_netfd);
   }
#endif /* WITH_NAMESPACES */

   return result;
}

int xio_syntax(
	const char *addr,
	int expectnum,
	int isnum,
	const char *syntax)
{
	return xiohelp_syntax(addr, expectnum, isnum, syntax);
}
