/* source: xio-gopen.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of generic open type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"
#include "xio-unix.h"
#include "xio-gopen.h"


#if WITH_GOPEN

static int xioopen_gopen(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);


const struct addrdesc xioaddr_gopen  = { "GOPEN",  3, xioopen_gopen, GROUP_FD|GROUP_FIFO|GROUP_CHR|GROUP_BLK|GROUP_REG|GROUP_NAMED|GROUP_OPEN|GROUP_FILE|GROUP_TERMIOS|GROUP_SOCKET|GROUP_SOCK_UNIX, 0, 0, 0 HELP(":<filename>") };

static int xioopen_gopen(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xxfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xxfd->stream;
   const char *filename = argv[1];
   flags_t openflags = (xioflags & XIO_ACCMODE);
   mode_t st_mode;
   bool exists;
   bool opt_unlink_close = false;
   int result;

   if ((result =
	_xioopen_named_early(argc, argv, xxfd, GROUP_NAMED|addrdesc->groups, &exists,
			     opts, addrdesc->syntax))
       < 0) {
      return result;
   }
   st_mode = result;

   if (exists) {
      /* file (or at least named entry) exists */
      if ((xioflags&XIO_ACCMODE) != XIO_RDONLY) {
	 openflags |= O_APPEND;
      }
   } else {
      openflags |= O_CREAT;
   }

   /* note: when S_ISSOCK was undefined, it always gives 0 */
   if (exists && S_ISSOCK(st_mode)) {
#if WITH_UNIX
      union sockaddr_union us;
      socklen_t uslen = sizeof(us);
      char infobuff[256];

      Info1("\"%s\" is a socket, connecting to it", filename);

      result =
	 _xioopen_unix_client(sfd, xioflags, addrdesc->groups, 0, opts,
			      filename, addrdesc);
      if (result < 0) {
	 return result;
      }
      applyopts_named(filename, opts, PH_PASTOPEN);	/* unlink-late */

      if (Getsockname(sfd->fd, (struct sockaddr *)&us, &uslen) < 0) {
	 Warn4("getsockname(%d, %p, {%d}): %s",
	       sfd->fd, &us, uslen, strerror(errno));
      } else {
	 Notice1("successfully connected via %s",
		 sockaddr_unix_info(&us.un, uslen,
				    infobuff, sizeof(infobuff)));
      }
#else
      Error("\"%s\" is a socket, but UNIX socket support is not compiled in");
      return -1;
#endif /* WITH_UNIX */

   } else {
      /* a file name */

      Info1("\"%s\" is not a socket, open()'ing it", filename);

      retropt_bool(opts, OPT_UNLINK_CLOSE, &opt_unlink_close);
      if (opt_unlink_close) {
	 if ((sfd->unlink_close = strdup(filename)) == NULL) {
	    Error1("strdup(\"%s\"): out of memory", filename);
	 }
	 sfd->opt_unlink_close = true;
      }

      Notice3("opening %s \"%s\" for %s",
	      filetypenames[(st_mode&S_IFMT)>>12], filename, ddirection[(xioflags&XIO_ACCMODE)]);
      if ((result = _xioopen_open(filename, openflags, opts)) < 0)
	 return result;
#ifdef I_PUSH
      if (S_ISCHR(st_mode) && Ioctl(result, I_FIND, "ldterm\0") == 0) {
	 Ioctl(result, I_PUSH, "ptem\0\0\0");	/* pad string length ... */
	 Ioctl(result, I_PUSH, "ldterm\0");	/* ... to requirements of ... */
	 Ioctl(result, I_PUSH, "ttcompat");	/* ... AdressSanitizer */
      }
#endif
      sfd->fd = result;

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
      applyopts_named(filename, opts, PH_FD);
      applyopts(sfd, -1, opts, PH_FD);
      applyopts_cloexec(sfd->fd, opts);
   }

   if ((result = applyopts2(sfd, -1, opts, PH_PASTSOCKET, PH_CONNECTED)) < 0)
      return result;

   if ((result = _xio_openlate(sfd, opts)) < 0)
      return result;
   return 0;
}

#endif /* WITH_GOPEN */
