/* source: xio-system.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of system type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-progcall.h"
#include "xio-system.h"


#if WITH_SYSTEM

static int xioopen_system(int arg, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

const struct addrdesc xioaddr_system = { "SYSTEM", 3, xioopen_system, GROUP_FD|GROUP_FORK|GROUP_EXEC|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_TERMIOS|GROUP_FIFO|GROUP_PTY|GROUP_PARENT, 1, 0, 0 HELP(":<shell-command>") };


static int xioopen_system(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,	/* XIO_RDONLY etc. */
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   int status;
   char *path = NULL;
   int duptostderr;
   int result;
   const char *string = argv[1];

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
      return STAT_NORETRY;
   }

   status =
      _xioopen_foxec(xioflags, sfd, addrdesc->groups, &opts, &duptostderr);
   if (status < 0)
      return status;
   if (status == 0) {	/* child */
      int numleft;

      /* do not shutdown connections that belong our parent */
      sock[0] = NULL;
      sock[1] = NULL;

      if (setopt_path(opts, &path) < 0) {
	 /* this could be dangerous, so let us abort this child... */
	 Exit(1);
      }

      if ((numleft = leftopts(opts)) > 0) {
	 showleft(opts);
	 Error1("INTERNAL: %d option(s) remained unused", numleft);
	 return STAT_NORETRY;
      }

      /* only now redirect stderr */
      if (duptostderr >= 0) {
	 diag_dup();
	 Dup2(duptostderr, 2);
      }
      Info1("executing shell command \"%s\"", string);
      errno=0;
      result = System(string);
      if (result != 0) {
	 Warn2("system(\"%s\") returned with status %d", string, result);
	 if (errno != 0)
	    Warn1("system(): %s", strerror(errno));
      }
      Exit(result>>8);	/* this child process */
   }

   /* parent */
   _xio_openlate(sfd, opts);
   return 0;
}

#endif /* WITH_SYSTEM */
