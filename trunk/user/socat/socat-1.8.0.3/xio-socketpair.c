/* source: xio-socketpair.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of socketpair type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-named.h"

#include "xio-socketpair.h"


#if WITH_SOCKETPAIR

static int xioopen_socketpair(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

const struct addrdesc xioaddr_socketpair   = { "SOCKETPAIR",   3, xioopen_socketpair,  GROUP_FD|GROUP_SOCKET, 0, 0, 0 HELP(":<filename>") };


/* Open a socketpair */
static int xioopen_socketpair(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   struct opt *opts2;
   int pf = PF_UNIX;
   int protocol = 0;
   int filedes[2];
   int numleft;
   int result;

   if (argc != 1) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   sfd->para.bipipe.socktype = SOCK_DGRAM;
   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts(sfd, -1, opts, PH_INIT);
   retropt_socket_pf(opts, &pf);
   retropt_int(opts, OPT_SO_TYPE, &sfd->para.bipipe.socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, &protocol);

   if (Socketpair(pf, sfd->para.bipipe.socktype, protocol, filedes) != 0) {
      Error5("socketpair(%d, %d, %d, %p): %s", pf, sfd->para.bipipe.socktype, protocol, filedes, strerror(errno));
      return -1;
   }
   Info2("socketpair({%d,%d})", filedes[0], filedes[1]);

   sfd->tag               = XIO_TAG_RDWR;
   if (sfd->para.bipipe.socktype == SOCK_STREAM) {
      sfd->dtype             = XIOREAD_STREAM|XIOWRITE_PIPE;
   } else {
      sfd->dtype             = XIOREAD_RECV|XIOREAD_RECV_NOCHECK|XIOWRITE_PIPE;
   }
   sfd->fd                = filedes[0];
   sfd->para.bipipe.fdout = filedes[1];
   applyopts_cloexec(sfd->fd,                opts);
   applyopts_cloexec(sfd->para.bipipe.fdout, opts);

   /* one-time and input-direction options, no second application */
   retropt_bool(opts, OPT_IGNOREEOF, &sfd->ignoreeof);

   /* here we copy opts! */
   if ((opts2 = copyopts(opts, GROUP_SOCKET)) == NULL) {
      return STAT_NORETRY;
   }

   /* apply options to first FD */
   if ((result = applyopts(sfd, -1, opts, PH_ALL)) < 0) {
      return result;
   }
   if ((result = applyopts_single(sfd, opts, PH_ALL)) < 0) {
      return result;
   }

   /* apply options to second FD */
   if (applyopts(sfd, sfd->para.bipipe.fdout, opts2, PH_ALL) < 0)
      return -1;

   if ((numleft = leftopts(opts)) > 0) {
      Error1("%d option(s) could not be used", numleft);
      showleft(opts);
   }
   Notice("writing to and reading from unnamed socketpair");
   return 0;
}

#endif /* WITH_SOCKETPAIR */


#if _WITH_SOCKETPAIR

/* retrieves options so-type and so-prototype from opts, calls socketpair, and
   ev. generates an appropriate error message.
   returns 0 on success or -1 if an error occurred. */
int
xiosocketpair(struct opt *opts, int pf, int socktype, int proto, int sv[2]) {
   int result;

   retropt_int(opts, OPT_SO_TYPE, &socktype);
   retropt_int(opts, OPT_SO_PROTOTYPE, &proto);
   result = Socketpair(pf, socktype, proto, sv);
   if (result < 0) {
      Error5("socketpair(%d, %d, %d, %p): %s",
	     pf, socktype, proto, sv, strerror(errno));
      return -1;
   }
   return result;
}

#endif /* _WITH_SOCKETPAIR */
