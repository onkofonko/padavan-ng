/* source: xio-vsock.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Author: Stefano Garzarella <sgarzare@redhat.com */
/* Published under the GNU General Public License V.2, see file COPYING */

/* This file contains the source for opening addresses of VSOCK socket type */

#include "xiosysincludes.h"

#ifdef WITH_VSOCK
#include "xioopen.h"
#include "xio-listen.h"
#include "xio-socket.h"
#include "xio-vsock.h"

static int xioopen_vsock_connect(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xxfd, const struct addrdesc *addrdesc);
static int xioopen_vsock_listen(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xxfd, const struct addrdesc *addrdesc);

static void xiolog_vsock_cid(void);

const struct addrdesc xioaddr_vsock_connect = { "VSOCK-CONNECT", 1+XIO_RDWR, xioopen_vsock_connect, GROUP_FD|GROUP_SOCKET|GROUP_CHILD|GROUP_RETRY,              0, 0, 0 HELP(":<cid>:<port>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_vsock_listen  = { "VSOCK-LISTEN",  1+XIO_RDWR, xioopen_vsock_listen,  GROUP_FD|GROUP_SOCKET|GROUP_LISTEN|GROUP_CHILD|GROUP_RETRY, 0, 0, 0 HELP(":<port>") };
#endif /* WITH_LISTEN */


/* Initializes a sockaddr of type VSOCK */
static int vsock_addr_init(struct sockaddr_vm *sa, const char *cid_str,
	   const char *port_str, int pf) {
   int ret;

   memset(sa, 0, sizeof(*sa));

   sa->svm_family = pf;
   ret = sockaddr_vm_parse(sa, cid_str, port_str);
   if (ret < 0)
      return STAT_NORETRY;

   return STAT_OK;
}


/* Performs a few steps during opening an address of type VSOCK */
static int vsock_init(struct opt *opts, struct single *sfd) {

   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_SHUTDOWN;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return STAT_NORETRY;

   applyopts(sfd, -1, opts, PH_INIT);
   applyopts(sfd, -1, opts, PH_EARLY);

   sfd->dtype = XIODATA_STREAM;

   return STAT_OK;
}

static int xioopen_vsock_connect(
	int argc,
	const char *argv[],
	struct opt *opts,
        int xioflags,
	xiofile_t *xxfd,
        const struct addrdesc *addrdesc)
{
   /* we expect the form :cid:port */
   struct single *sfd = &xxfd->stream;
   struct sockaddr_vm sa, sa_local;
   socklen_t sa_len = sizeof(sa);
   bool needbind = false;
   int socktype = SOCK_STREAM;
   int pf = PF_VSOCK;
   int protocol = 0;
   int ret;

   if (argc != 3) {
      xio_syntax(argv[0], 2, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, &protocol);

   ret = vsock_addr_init(&sa, argv[1], argv[2], pf);
   if (ret) {
      return ret;
   }

   ret = vsock_init(opts, sfd);
   if (ret) {
      return ret;
   }

   xiolog_vsock_cid();

   ret = retropt_bind(opts, pf, socktype, protocol,
                      (struct sockaddr *)&sa_local, &sa_len, 3,
		      NULL);
   if (ret == STAT_NORETRY)
      return ret;
   if (ret == STAT_OK)
      needbind = true;

   ret = xioopen_connect(sfd, needbind ? (union sockaddr_union *)&sa_local : NULL,
                         sa_len, (struct sockaddr *)&sa, sizeof(sa),
                         opts, pf, socktype, protocol, false);
   if (ret)
      return ret;

   ret = _xio_openlate(sfd, opts);
   if (ret < 0)
       return ret;

   return STAT_OK;
}


#if WITH_LISTEN
static int xioopen_vsock_listen(
	int argc,
	const char *argv[],
	struct opt *opts,
        int xioflags,
	xiofile_t *xxfd,
        const struct addrdesc *addrdesc)
{
   /* we expect the form :port */
   struct single *sfd = &xxfd->stream;
   struct sockaddr_vm sa, sa_bind;
   socklen_t sa_len = sizeof(sa_bind);
   struct opt *opts0;
   int socktype = SOCK_STREAM;
   int pf = PF_VSOCK;
   int protocol = 0;
   int ret;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, &protocol);

   ret = vsock_addr_init(&sa, NULL, argv[1], pf);
   if (ret) {
      return ret;
   }

   ret = vsock_init(opts, sfd);
   if (ret) {
      return ret;
   }

   opts0 = copyopts(opts, GROUP_ALL);

   ret = retropt_bind(opts, pf, socktype, protocol, (struct sockaddr *)&sa_bind,
                      &sa_len, 1,
		      sfd->para.socket.ip.ai_flags);
   if (ret == STAT_NORETRY)
       return ret;
   if (ret == STAT_OK)
       sa.svm_cid = sa_bind.svm_cid;

   xiolog_vsock_cid();

   /* this may fork() */
   return xioopen_listen(sfd, xioflags, (struct sockaddr *)&sa, sizeof(sa),
                         opts, opts0, pf, socktype, protocol);
}
#endif /* WITH_LISTEN */


/* Just tries to query and log the VSOCK CID */
static void xiolog_vsock_cid(void) {
   int vsock;
   unsigned int cid;
#ifdef IOCTL_VM_SOCKETS_GET_LOCAL_CID
   if ((vsock = Open("/dev/vsock", O_RDONLY, 0)) < 0 ) {
      Warn1("open(\"/dev/vsock\", ...): %s", strerror(errno));
   } else if (Ioctl(vsock, IOCTL_VM_SOCKETS_GET_LOCAL_CID, &cid) < 0) {
      Warn2("ioctl(%d, IOCTL_VM_SOCKETS_GET_LOCAL_CID, ...): %s",
	    vsock, strerror(errno));
   } else {
      Notice1("VSOCK CID=%u", cid);
   }
   if (vsock >= 0) {
      Close(vsock);
   }
#endif /* IOCTL_VM_SOCKETS_GET_LOCAL_CID */
   return;
}


/* Returns information that can be used for constructing an environment
   variable describing the socket address.
   if idx is 0, this function writes "ADDR" into namebuff and the CID address
   into valuebuff, and returns 1 (which means that one more info is there).
   if idx is 1, it writes "PORT" into namebuff and the port number into
   valuebuff, and returns 0 (no more info)
   namelen and valuelen contain the max. allowed length of output chars in the
   respective buffer.
   on error this function returns -1.
*/
int
xiosetsockaddrenv_vsock(int idx, char *namebuff, size_t namelen,
		      char *valuebuff, size_t valuelen,
		      struct sockaddr_vm *sa, int ipproto) {
   switch (idx) {
   case 0:
      strcpy(namebuff, "ADDR");
      snprintf(valuebuff, valuelen, F_uint32_t, sa->svm_cid);
      return 1;
   case 1:
      strcpy(namebuff, "PORT");
      snprintf(valuebuff, valuelen, F_uint32_t, sa->svm_port);
      return 0;
   }
   return -1;
}

#endif /* WITH_VSOCK */
