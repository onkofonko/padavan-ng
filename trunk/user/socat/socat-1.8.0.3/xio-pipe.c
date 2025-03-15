/* source: xio-pipe.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of pipe type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-named.h"

#include "xio-pipe.h"


#if WITH_PIPE

static int xioopen_fifo(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *fd, const struct addrdesc *addrdesc);
static int xioopen_fifo_unnamed(xiofile_t *sock, struct opt *opts);


const struct addrdesc xioaddr_pipe   = { "PIPE",   3, xioopen_fifo,  GROUP_FD|GROUP_NAMED|GROUP_OPEN|GROUP_FIFO, 0, 0, 0 HELP("[:<filename>]") };

#if defined(F_SETPIPE_SZ)
const struct optdesc opt_f_setpipe_sz = { "f-setpipe-sz", "pipesz", OPT_F_SETPIPE_SZ, GROUP_FD, PH_FD, TYPE_INT, OFUNC_FCNTL, F_SETPIPE_SZ, 0 };
#endif

/* process an unnamed bidirectional "pipe" or "fifo" or "echo" argument with
   options */
static int xioopen_fifo_unnamed(xiofile_t *sock, struct opt *opts) {
   struct single *sfd = &sock->stream;
   struct opt *opts2;
   int filedes[2];
   int numleft;
   int result;

   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   if (Pipe(filedes) != 0) {
      Error2("pipe(%p): %s", filedes, strerror(errno));
      return -1;
   }
   /*0 Info2("pipe({%d,%d})", filedes[0], filedes[1]);*/

   sock->common.tag               = XIO_TAG_RDWR;
   sfd->dtype             = XIODATA_PIPE;
   sfd->fd                = filedes[0];
   sfd->para.bipipe.fdout = filedes[1];
   sfd->para.bipipe.socktype = SOCK_STREAM; 	/* due to socketpair reuse */
   applyopts_cloexec(sfd->fd,                opts);
   applyopts_cloexec(sfd->para.bipipe.fdout, opts);

   /* one-time and input-direction options, no second application */
   retropt_bool(opts, OPT_IGNOREEOF, &sfd->ignoreeof);

   /* here we copy opts! */
   if ((opts2 = copyopts(opts, GROUP_FIFO)) == NULL) {
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
   if (applyopts(&sock->stream, sfd->para.bipipe.fdout, opts2, PH_ALL) < 0)
      return -1;

   if ((numleft = leftopts(opts)) > 0) {
      showleft(opts);
      Error1("INTERNAL: %d option(s) remained unused", numleft);
   }

   xio_chk_pipesz(sfd->fd);

   Notice("writing to and reading from unnamed pipe");
   return 0;
}


/* open a named or unnamed pipe/fifo */
static int xioopen_fifo(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
   struct single *sfd = &xfd->stream;
   const char *pipename = argv[1];
   int rw = (xioflags & XIO_ACCMODE);
#if HAVE_STAT64
   struct stat64 pipstat;
#else
   struct stat pipstat;
#endif /* !HAVE_STAT64 */
   bool opt_unlink_early = false;
   bool opt_unlink_close = true;
   mode_t mode = 0666;
   int result;

   if (argc == 1) {
      return xioopen_fifo_unnamed(xfd, sfd->opts);
   }

   if (argc != 2) {
      xio_syntax(argv[0], 1, argc-1,addrdesc->syntax);
      return STAT_NORETRY;
   }

   if (applyopts_single(sfd, opts, PH_INIT) < 0)
      return -1;
   applyopts(sfd, -1, opts, PH_INIT);

   retropt_bool(opts, OPT_UNLINK_EARLY, &opt_unlink_early);
   applyopts_named(pipename, opts, PH_EARLY);	/* umask! */
   applyopts(sfd, -1, opts, PH_EARLY);

   if (opt_unlink_early) {
      if (Unlink(pipename) < 0) {
	 return STAT_RETRYLATER;
      }
   }

   retropt_bool(opts, OPT_UNLINK_CLOSE, &opt_unlink_close);
   retropt_modet(opts, OPT_PERM, &mode);
   if (applyopts_named(pipename, opts, PH_EARLY) < 0) {
      return STAT_RETRYLATER;
   }
   if (applyopts_named(pipename, opts, PH_PREOPEN) < 0) {
      return STAT_RETRYLATER;
   }
   if (
#if HAVE_STAT64
       Stat64(pipename, &pipstat) < 0
#else
       Stat(pipename, &pipstat) < 0
#endif /* !HAVE_STAT64 */
      ) {
      if (errno != ENOENT) {
	 Error3("stat(\"%s\", %p): %s", pipename, &pipstat, strerror(errno));
      } else {
	 Debug1("xioopen_fifo(\"%s\"): does not exist, creating fifo", pipename);
#if 0
	 result = Mknod(pipename, S_IFIFO|mode, 0);
	 if (result < 0) {
	    Error3("mknod(%s, %d, 0): %s", pipename, mode, strerror(errno));
	    return STAT_RETRYLATER;
	 }
#else
	 result = Mkfifo(pipename, mode);
	 if (result < 0) {
	    Error3("mkfifo(%s, %d): %s", pipename, mode, strerror(errno));
	    return STAT_RETRYLATER;
	 }
#endif
	 Notice2("created named pipe \"%s\" for %s", pipename, ddirection[rw]);
	 applyopts_named(pipename, opts, PH_ALL);

      }
      if (opt_unlink_close) {
	 if ((sfd->unlink_close = strdup(pipename)) == NULL) {
	    Error1("strdup(\"%s\"): out of memory", pipename);
	 }
	 sfd->opt_unlink_close = true;
      }
   } else {
      /* exists */
      Info1("xioopen_fifo(\"%s\"): already exist, opening it", pipename);
      Notice3("opening %s \"%s\" for %s",
	      filetypenames[(pipstat.st_mode&S_IFMT)>>12],
	      pipename, ddirection[rw]);
      applyopts_named(pipename, opts, PH_EARLY);
   }

   if ((result = _xioopen_open(pipename, rw, opts)) < 0) {
      return result;
   }
   sfd->fd = result;

   applyopts_named(pipename, opts, PH_FD);
   applyopts(sfd, -1, opts, PH_FD);
   applyopts_cloexec(sfd->fd, opts);
   xio_chk_pipesz(sfd->fd);

   return _xio_openlate(sfd, opts);
}


/* Checks if fd is a pipe and if its buffer is at least the blksiz.
   returns 0 if ok;
   returns 1 if unknown;
   returns -1 if not */
int xio_chk_pipesz(
	int fd)
{
	struct stat st;
	int pipesz;

	if (fstat(fd, &st) < 0) {
		Warn2("fstat(%d, ...): %s", fd, strerror(errno));
		return 1;
	}
	if ((st.st_mode&S_IFMT) != S_IFIFO) {
		return 0;
	}

#if defined(F_GETPIPE_SZ)
	if ((pipesz = Fcntl(fd, F_GETPIPE_SZ)) < 0) {
		Warn2("fcntl(%d, F_GETPIPE_SZ): %s", fd, strerror(errno));
		return 1;
	}

	if (pipesz >= xioparms.bufsiz)
		return 0;

	Warn3("xio_chk_pipesz(%d, ...): Socat block size "F_Zu" is larger than pipe size %d, might block; use option f-setpipe-sz!",
	      fd, xioparms.bufsiz, pipesz);
	return -1;
#else /* !defined(F_GETPIPE_SZ) */
	return 1;
#endif /* !defined(F_GETPIPE_SZ) */
}

#endif /* WITH_PIPE */
