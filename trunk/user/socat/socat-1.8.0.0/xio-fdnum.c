/* source: xio-fdnum.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of fdnum type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-listen.h"

#include "xio-fdnum.h"


#if WITH_FDNUM

static int xioopen_fdnum(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);
static int xioopen_accept_fd(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);


const struct addrdesc xioaddr_fd        = { "FD",        1+XIO_RDWR, xioopen_fdnum, GROUP_FD|GROUP_FIFO|GROUP_CHR|GROUP_BLK|GROUP_FILE|GROUP_SOCKET|GROUP_TERMIOS|GROUP_SOCK_UNIX|GROUP_SOCK_IP|GROUP_IPAPP, 0, 0, 0 HELP(":<fdnum>") };
#if WITH_LISTEN
const struct addrdesc xioaddr_accept_fd = { "ACCEPT-FD", 1+XIO_RDWR, xioopen_accept_fd, GROUP_FD|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_SOCK_IP|GROUP_IPAPP|GROUP_CHILD|GROUP_RANGE|GROUP_RETRY,                 0, 0, 0 HELP(":<fdnum>") };
#endif /* WITH_LISTEN */


/* use some file descriptor and apply the options. Set the FD_CLOEXEC flag. */
static int xioopen_fdnum(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   char *a1;
   int rw = (xioflags&XIO_ACCMODE);
   int numfd;
   int result;

   if (argc != 2) {
      Error2("%s: wrong number of parameters (%d instead of 1)", argv[0], argc-1);
   }

   numfd = strtoul(argv[1], &a1, 0);
   if (*a1 != '\0') {
      Error1("error in FD number \"%s\"", argv[1]);
   }
   /* we dont want to see these fds in child processes */
   if (Fcntl_l(numfd, F_SETFD, FD_CLOEXEC) < 0) {
      Warn2("fcntl(%d, F_SETFD, FD_CLOEXEC): %s", numfd, strerror(errno));
   }
   Notice2("using file descriptor %d for %s", numfd, ddirection[rw]);
   if ((result = xioopen_fd(opts, rw, &xfd->stream, numfd)) < 0) {
      return result;
   }
   return 0;
}

#if WITH_LISTEN

/* Use some file descriptor and apply the options. Set the FD_CLOEXEC flag. */
static int xioopen_accept_fd(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   char *a1;
   int rw = (xioflags&XIO_ACCMODE);
   int numfd;
   union sockaddr_union us;
   socklen_t uslen = sizeof(union sockaddr_union);
   int result;

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   numfd = strtoul(argv[1], &a1, 0);
   if (*a1 != '\0') {
      Error1("error in FD number \"%s\"", argv[1]);
   }
   /* we dont want to see these fds in child processes */
   if (Fcntl_l(numfd, F_SETFD, FD_CLOEXEC) < 0) {
      Warn2("fcntl(%d, F_SETFD, FD_CLOEXEC): %s", numfd, strerror(errno));
   }

   if (Getsockname(numfd, (struct sockaddr *)&us, &uslen) < 0) {
      Warn2("getsockname(fd=%d, ...): %s", numfd, strerror(errno));
   }
   Notice2("using file descriptor %d accepting a connection for %s", numfd, ddirection[rw]);
   xfd->stream.fd = numfd;
   if ((result = _xioopen_accept_fd(&xfd->stream, xioflags, (struct sockaddr *)&us, uslen, opts, us.soa.sa_family, 0, 0)) < 0) {
      return result;
   }
   return 0;
}

#endif /* WITH_LISTEN */
#endif /* WITH_FDNUM */


#if WITH_FD

/* Retrieves and apply options to a standard file descriptor.
   Does not set FD_CLOEXEC flag. */
int xioopen_fd(struct opt *opts, int rw, struct single *sfd, int numfd) {

   sfd->fd = numfd;
   if (sfd->howtoend == END_UNSPEC)
      sfd->howtoend = END_NONE;

#if WITH_TERMIOS
   if (Isatty(sfd->fd)) {
      if (Tcgetattr(sfd->fd, &sfd->savetty) < 0) {
	 Warn2("cannot query current terminal settings on fd %d: %s",
	       sfd->fd, strerror(errno));
      } else {
	 sfd->ttyvalid = true;
      }
   }
#endif /* WITH_TERMIOS */
   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;

   applyopts2(sfd, -1, opts, PH_INIT, PH_FD);

   return _xio_openlate(sfd, opts);
}

#endif /* WITH_FD */
