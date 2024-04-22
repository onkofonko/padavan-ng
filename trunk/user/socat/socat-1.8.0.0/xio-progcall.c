/* source: xio-progcall.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains common code dealing with program calls (exec, system) */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-process.h"
#include "xio-named.h"
#include "xio-progcall.h"

#include "xio-socket.h"


/* these options are used by address pty too */
#if HAVE_OPENPTY
const struct optdesc opt_openpty = { "openpty",   NULL, OPT_OPENPTY,     GROUP_PTY,   PH_BIGEN, TYPE_BOOL, 	OFUNC_SPEC };
#endif /* HAVE_OPENPTY */
#if HAVE_DEV_PTMX || HAVE_DEV_PTC
const struct optdesc opt_ptmx    = { "ptmx",      NULL, OPT_PTMX,        GROUP_PTY,   PH_BIGEN, TYPE_BOOL, 	OFUNC_SPEC };
#endif
const struct optdesc opt_sitout_eio = { "sitout-eio", NULL, OPT_SITOUT_EIO, GROUP_PTY, PH_OFFSET, TYPE_TIMEVAL, OFUNC_OFFSET, XIO_OFFSETOF(para.exec.sitout_eio), XIO_SIZEOF(para.exec.sitout_eio) };

#if WITH_EXEC || WITH_SYSTEM

#define MAXPTYNAMELEN 64

const struct optdesc opt_fdin    = { "fdin",      NULL, OPT_FDIN,        GROUP_FORK,   PH_PASTBIGEN,   TYPE_USHORT,	OFUNC_SPEC };
const struct optdesc opt_fdout   = { "fdout",     NULL, OPT_FDOUT,       GROUP_FORK,   PH_PASTBIGEN,   TYPE_USHORT,	OFUNC_SPEC };
const struct optdesc opt_path    = { "path",      NULL, OPT_PATH,        GROUP_EXEC,   PH_PREEXEC,     TYPE_STRING,	OFUNC_SPEC };
const struct optdesc opt_pipes   = { "pipes",     NULL, OPT_PIPES,       GROUP_FORK,   PH_BIGEN,       TYPE_BOOL, 	OFUNC_SPEC };
#if HAVE_PTY
const struct optdesc opt_pty     = { "pty",       NULL, OPT_PTY,         GROUP_FORK,   PH_BIGEN, TYPE_BOOL, 	OFUNC_SPEC };
#endif
const struct optdesc opt_stderr  = { "stderr",    NULL, OPT_STDERR,      GROUP_FORK,   PH_PASTFORK,        TYPE_BOOL,	OFUNC_SPEC };
const struct optdesc opt_nofork  = { "nofork",    NULL, OPT_NOFORK,      GROUP_FORK,   PH_BIGEN,       TYPE_BOOL,       OFUNC_SPEC };
const struct optdesc opt_sighup  = { "sighup",    NULL, OPT_SIGHUP,      GROUP_PARENT, PH_LATE,        TYPE_CONST,      OFUNC_SIGNAL, SIGHUP };
const struct optdesc opt_sigint  = { "sigint",    NULL, OPT_SIGINT,      GROUP_PARENT, PH_LATE,        TYPE_CONST,      OFUNC_SIGNAL, SIGINT };
const struct optdesc opt_sigquit = { "sigquit",   NULL, OPT_SIGQUIT,     GROUP_PARENT, PH_LATE,        TYPE_CONST,      OFUNC_SIGNAL, SIGQUIT };


/* fork for exec/system, but return before exec'ing.
   return=0: is child process
   return>0: is parent process
   return<0: error occurred, assume parent process and no child exists !!!
 */
int _xioopen_foxec(int xioflags,	/* XIO_RDONLY etc. */
		struct single *sfd,
		   groups_t groups,
		   struct opt **optsp,	/* in: opts; out: opts for parent/child */
		   int *duptostderr	/* out: redirect stderr to output fd */
		) {
   struct opt *opts;		/* common options */
   struct opt *popts = NULL;	/* parent options */
   struct opt *copts;		/* child options */
   int numleft;
   int d, sv[2], rdpip[2], wrpip[2];
   int rw = (xioflags & XIO_ACCMODE);
   bool usepipes = false;
#if HAVE_PTY
   int ptyfd = -1, ttyfd = -1;
   bool usebestpty = false;	/* use the best available way to open pty */
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   bool useptmx = false;	/* use /dev/ptmx or equivalent */
#endif
#if HAVE_OPENPTY
   bool useopenpty = false;	/* try only openpty */
#endif	/* HAVE_OPENPTY */
   bool usepty = false;		/* any of the pty options is selected */
   char ptyname[MAXPTYNAMELEN];
#endif /* HAVE_PTY */
   pid_t pid = 0;	/* mostly int */
   short fdi = 0, fdo = 1;
   short result;
   bool withstderr = false;
   bool nofork = false;
   bool withfork;
   char *tn = NULL;
   int trigger[2]; 	/* [0] watched by parent, [1] closed by child when ready */

   opts = *optsp;
   if (applyopts_single(sfd, opts, PH_INIT) < 0)  return -1;
   applyopts2(sfd, -1, opts, PH_INIT, PH_EARLY);

   retropt_bool(opts, OPT_NOFORK, &nofork);
   withfork = !nofork;

   retropt_bool(opts, OPT_PIPES, &usepipes);
#if HAVE_PTY
   retropt_bool(opts, OPT_PTY, &usebestpty);
#if HAVE_OPENPTY
   retropt_bool(opts, OPT_OPENPTY, &useopenpty);
#endif
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
   retropt_bool(opts, OPT_PTMX, &useptmx);
#endif
   usepty = (usebestpty
#if HAVE_OPENPTY
	     || useopenpty
#endif
#if defined(HAVE_DEV_PTMX) || defined(HAVE_DEV_PTC)
	     || useptmx
#endif
	     );
   if (usepipes && usepty) {
      Warn("_xioopen_foxec(): options \"pipes\" and \"pty\" must not be specified together; ignoring \"pipes\"");
      usepipes = false;
   }
#endif /* HAVE_PTY */

   if (retropt_ushort(opts, OPT_FDIN,  (unsigned short *)&fdi) >= 0) {
      if ((xioflags&XIO_ACCMODE) == XIO_RDONLY) {
	 Error("_xioopen_foxec(): option fdin is useless in read-only mode");
      }
   }
   if (retropt_ushort(opts, OPT_FDOUT, (unsigned short *)&fdo) >= 0) {
      if ((xioflags&XIO_ACCMODE) == XIO_WRONLY) {
	 Error("_xioopen_foxec(): option fdout is useless in write-only mode");
      }
   }

   if (withfork) {
      if (!(xioflags&XIO_MAYCHILD)) {
	 Error("cannot fork off child process here");
	 /*!! free something */
	 return -1;
      }
      sfd->flags |= XIO_DOESCHILD;

#if HAVE_PTY
      Notice2("forking off child, using %s for %s",
	    &("socket\0\0pipes\0\0\0pty\0\0\0\0\0"[(usepipes<<3)|(usepty<<4)]),
	      ddirection[rw]);
#else
      Notice2("forking off child, using %s for %s",
	      &("socket\0\0pipes\0\0\0"[(usepipes<<3)]),
	      ddirection[rw]);
#endif /* HAVE_PTY */
   }
   applyopts(sfd, -1, opts, PH_PREBIGEN);

   if (!withfork) {
      /*0 struct single *stream1, *stream2;*/

      if (!(xioflags & XIO_MAYEXEC /* means exec+nofork */)) {
	 Error("option nofork is not allowed here");
	 /*!! free something */
	 return -1;
      }
      sfd->flags |= XIO_DOESEXEC;

      /* Only one process, no parent,child */
      if ((copts = moveopts(opts, GROUP_ALL)) == NULL) {
	 /*!! free something */
	 return -1;
      }

#if 0 /*!! */
      if (sock1->tag == XIO_TAG_DUAL) {
	 stream1 = &sock1->dual.stream[0]->stream;
	 stream2 = &sock1->dual.stream[1]->stream;
      } else {
	 stream1 = &sock1->stream;
	 stream2 = &sock1->stream;
      }
      if (stream1->dtype == DATA_READLINE || stream2->dtype == DATA_READLINE ||
	  stream1->dtype == DATA_OPENSSL  || stream2->dtype == DATA_OPENSSL
	  ) {
	 Error("with option nofork, openssl and readline in address1 do not work");
      }
      if (stream1->lineterm != LINETERM_RAW ||
	  stream2->lineterm != LINETERM_RAW ||
	  stream1->ignoreeof || stream2->ignoreeof) {
	 Warn("due to option nofork, address1 options for lineterm and igoreeof do not apply");
      }
#endif

      /* remember: fdin is the fd where the sub program reads from, thus it is
	 sock0[]'s read fd */
      /*! problem: when fdi==WRFD(sock[0]) or fdo==RDFD(sock[0]) */
      if (rw != XIO_WRONLY) {
	 if (XIO_GETWRFD(sock[0]/*!!*/) == fdo) {
	    if (Fcntl_l(fdo, F_SETFD, 0) < 0) {
	       Warn2("fcntl(%d, F_SETFD, 0): %s", fdo, strerror(errno));
	    }
	 } else {
	    /* make sure that the internal diagnostic socket pair fds do not conflict
	       with our choices */
	    diag_reserve_fd(fdo);
	    if (Dup2(XIO_GETWRFD(sock[0]), fdo) < 0) {
	       Error3("dup2(%d, %d): %s",
		      XIO_GETWRFD(sock[0]), fdo, strerror(errno));
	    }
	 }
	 /*0 Info2("dup2(%d, %d)", XIO_GETRDFD(sock[0]), fdi);*/
      }
      if (rw != XIO_RDONLY) {
	 if (XIO_GETRDFD(sock[0]) == fdi) {
	    if (Fcntl_l(fdi, F_SETFD, 0) < 0) {
	       Warn2("fcntl(%d, F_SETFD, 0): %s", fdi, strerror(errno));
	    }
	 } else {
	    /* make sure that the internal diagnostic socket pair fds do not conflict
	       with our choices */
	    diag_reserve_fd(fdi);
	    if (Dup2(XIO_GETRDFD(sock[0]), fdi) < 0) {
	       Error3("dup2(%d, %d): %s)",
		      XIO_GETRDFD(sock[0]), fdi, strerror(errno));
	    }
	    /*0 Info2("dup2(%d, %d)", XIO_GETWRFD(sock[0]), fdo);*/
	 }
      }
      /* !withfork */
   } else /* withfork */
#if HAVE_PTY
   if (usepty) {

#if defined(HAVE_DEV_PTMX)
#  define PTMX "/dev/ptmx"	/* Linux */
#elif HAVE_DEV_PTC
#  define PTMX "/dev/ptc"	/* AIX 4.3.3 */
#endif
      sfd->dtype = XIODATA_PTY;
#if HAVE_DEV_PTMX || HAVE_DEV_PTC
      if (usebestpty || useptmx) {
	 if ((ptyfd = Open(PTMX, O_RDWR|O_NOCTTY, 0620)) < 0) {
	    Warn1("open(\""PTMX"\", O_RDWR|O_NOCTTY, 0620): %s",
		  strerror(errno));
	    /*!*/
	 } else {
	    /*0 Info2("open(\"%s\", O_RDWR|O_NOCTTY, 0620) -> %d", PTMX, ptyfd);*/
	 }
	 if (ptyfd >= 0 && ttyfd < 0) {
	    /* we used PTMX before forking */
	    extern char *ptsname(int);

#if HAVE_PROTOTYPE_LIB_ptsname	/* AIX, not Linux */
	    if ((tn = Ptsname(ptyfd)) == NULL) {
	       Warn2("ptsname(%d): %s", ptyfd, strerror(errno));
	    }
#endif /* HAVE_PROTOTYPE_LIB_ptsname */
	    if (tn == NULL) {
	       if ((tn = Ttyname(ptyfd)) == NULL) {
		  Error2("ttyname(%d): %s", ptyfd, strerror(errno));
	       }
	    }
	    ptyname[0] = '\0'; strncat(ptyname, tn, MAXPTYNAMELEN-1);
#if HAVE_GRANTPT	/* AIX, not Linux */
	    if (Grantpt(ptyfd)/*!*/ < 0) {
	       Warn2("grantpt(%d): %s", ptyfd, strerror(errno));
	    }
#endif /* HAVE_GRANTPT */
#if HAVE_UNLOCKPT
	    if (Unlockpt(ptyfd)/*!*/ < 0) {
	       Warn2("unlockpt(%d): %s", ptyfd, strerror(errno));
	    }
#endif /* HAVE_UNLOCKPT */
	 }
      }
#endif /* HAVE_DEV_PTMX || HAVE_DEV_PTC */
#if HAVE_OPENPTY
      if (ptyfd < 0) {
	 int result;
	 if ((result = Openpty(&ptyfd, &ttyfd, ptyname, NULL, NULL)) < 0) {
	    Error4("openpty(%p, %p, %p, NULL, NULL): %s",
		   &ptyfd, &ttyfd, ptyname, strerror(errno));
	    return -1;
	 }
      }
#endif /* HAVE_OPENPTY */
      /* withfork use_pty */
      if ((copts = moveopts(opts, GROUP_TERMIOS|GROUP_FORK|GROUP_EXEC|GROUP_PROCESS|GROUP_NAMED)) == NULL) {
	 return -1;
      }
      popts = opts;
      applyopts_cloexec(ptyfd, popts);/*!*/
      /* exec:...,pty did not kill child process under some circumstances */
      if (sfd->howtoend == END_UNSPEC) {
	 sfd->howtoend = END_CLOSE_KILL;
      }

      /* this for parent, was after fork */
      applyopts(sfd, ptyfd, popts, PH_FD);
      sfd->fd = ptyfd;

      /* end withfork, use_pty */
   } else /* end withfork, use_pty */
#endif /* HAVE_PTY */

   if (usepipes) {
      /* withfork usepipes */
      struct opt *popts2 = NULL;

      if (rw == XIO_RDWR)
	 sfd->dtype = XIODATA_2PIPE;
      if (rw != XIO_WRONLY) {
	 if (Pipe(rdpip) < 0) {
	    Error2("pipe(%p): %s", rdpip, strerror(errno));
	    return -1;
	 }
      }
      /*0 Info2("pipe({%d,%d})", rdpip[0], rdpip[1]);*/
      /* rdpip[0]: read by socat; rdpip[1]: write by child */

      /* withfork usepipes */
      if ((copts = moveopts(opts, GROUP_FORK|GROUP_EXEC|GROUP_PROCESS))
	  == NULL) {
	 return -1;
      }
      popts = opts;
      if (sfd->dtype == XIODATA_2PIPE)
	 popts2 = copyopts(popts, GROUP_ALL);

      if (rw != XIO_WRONLY) {
	 applyopts_cloexec(rdpip[0], popts);
	 applyopts(sfd, rdpip[0], popts, PH_FD);
	 applyopts(sfd, rdpip[1], copts, PH_FD);
      }

      if (rw != XIO_RDONLY) {
	 if (Pipe(wrpip) < 0) {
	    Error2("pipe(%p): %s", wrpip, strerror(errno));
	    return -1;
	 }
      }

      /* wrpip[1]: write by socat; wrpip[0]: read by child */
      if (rw != XIO_RDONLY) {
	 applyopts_cloexec(wrpip[1], popts);
	 if (sfd->dtype == XIODATA_2PIPE)
	    applyopts(NULL, wrpip[1], popts2, PH_FD);
	 else
	    applyopts(NULL, wrpip[1], popts, PH_FD);
	 applyopts(NULL, wrpip[0], copts, PH_FD);
      }
      if (sfd->howtoend == END_UNSPEC) {
	 sfd->howtoend = END_CLOSE_KILL;
      }

      /* this for parent, was after fork */
      switch (rw) {
      case XIO_RDONLY: sfd->fd = rdpip[0]; break;
      case XIO_WRONLY: sfd->fd = wrpip[1]; break;
      case XIO_RDWR:   sfd->fd = rdpip[0];
	 sfd->para.exec.fdout = wrpip[1];
	 break;
      }
      applyopts(sfd, -1, popts, PH_FD);
      applyopts(sfd, -1, popts, PH_LATE);
      if (applyopts_single(sfd, popts, PH_LATE) < 0)
	 return -1;

      /* end withfork, use_pipes */
   } else {
      /* withfork, socketpair */

      d = AF_UNIX;
      retropt_int(opts, OPT_PROTOCOL_FAMILY, &d);
      result = xiosocketpair(opts, d, SOCK_STREAM, 0, sv);
      if (result < 0) {
	 return -1;
      }

      /* withfork socketpair */
      if ((copts = moveopts(opts, GROUP_FORK|GROUP_EXEC|GROUP_PROCESS)) == NULL) {
	 return -1;
      }
      popts = opts;
      applyopts(sfd, sv[0], copts, PH_PASTSOCKET);
      applyopts(sfd, sv[1], popts, PH_PASTSOCKET);

      applyopts_cloexec(sv[0], copts);
      applyopts(sfd, sv[0], copts, PH_FD);
      applyopts(sfd, sv[1], popts, PH_FD);

      applyopts(sfd, sv[0], copts, PH_PREBIND);
      applyopts(sfd, sv[0], copts, PH_BIND);
      applyopts(sfd, sv[0], copts, PH_PASTBIND);
      applyopts(sfd, sv[1], popts, PH_PREBIND);
      applyopts(sfd, sv[1], popts, PH_BIND);
      applyopts(sfd, sv[1], popts, PH_PASTBIND);

      if (sfd->howtoend == END_UNSPEC) {
	 sfd->howtoend = END_SHUTDOWN_KILL;
      }

      /* this for parent, was after fork */
      sfd->fd = sv[0];
      applyopts(sfd, -1, popts, PH_FD);
      /* end withfork, socketpair */
   }
   retropt_bool(copts, OPT_STDERR, &withstderr);

   xiosetchilddied();	/* set SIGCHLD handler */

   if (withfork) {
      Socketpair(PF_UNIX, SOCK_STREAM, 0, trigger);
      pid = xio_fork(true, E_ERROR, 0);
      if (pid < 0) {
	 return -1;
      }
   }
   if (!withfork || pid == 0) {	/* in single process, or in child */
      applyopts_optgroup(sfd, -1, copts, GROUP_PROCESS);
      if (withfork) {
	 Close(trigger[0]); 	/* in child: not needed here */
	 /* The child should have default handling for SIGCHLD. */
	 /* In particular, it's not defined whether ignoring SIGCHLD is inheritable. */
	 if (Signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
	    Warn1("signal(SIGCHLD, SIG_DFL): %s", strerror(errno));
	 }

#if HAVE_PTY
	 if (usepty) {
	    applyopts_named(tn, copts, PH_PREOPEN);
	    applyopts_named(tn, copts, PH_EARLY);
	    applyopts_named(tn, copts, PH_FD);

	   if (ttyfd < 0) {
	    if ((ttyfd = Open(tn, O_RDWR|O_NOCTTY, 0620)) < 0) {
	       Warn2("open(\"%s\", O_RDWR|O_NOCTTY, 0620): %s", tn, strerror(errno));
	    } else {
	       /*0 Info2("open(\"%s\", O_RDWR|O_NOCTTY, 0620) -> %d", tn, ttyfd);*/
	    }
	   } else {
	      if ((tn = Ttyname(ttyfd)) == NULL) {
		 Warn2("ttyname(%d): %s", ttyfd, strerror(errno));
	      }
	   }

#ifdef I_PUSH
	    /* Linux: I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> -1 EINVAL */
	    /* AIX:   I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> 1 */
	    /* SunOS: I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> 0 */
	    /* HP-UX: I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> 0 */
	    if (Ioctl(ttyfd, I_FIND, "ldterm\0") == 0) {
	       Ioctl(ttyfd, I_PUSH, "ptem\0\0\0");	/* 0 */ /* padding for AdressSanitizer */
	       Ioctl(ttyfd, I_PUSH, "ldterm\0");	/* 0 */
	       Ioctl(ttyfd, I_PUSH, "ttcompat");	/* HP-UX: -1 */
	    }
#endif

	    /* this for child, was after fork */
	    applyopts(sfd, ttyfd, copts, PH_FD);

	    Info1("opened pseudo terminal %s", tn);
	    Close(ptyfd);
	    if (rw != XIO_RDONLY && fdi != ttyfd) {
	       /* make sure that the internal diagnostic socket pair fds do not conflict
		  with our choices */
	       diag_reserve_fd(fdi);
	       if (Dup2(ttyfd, fdi) < 0) {
		  Error3("dup2(%d, %d): %s", ttyfd, fdi, strerror(errno));
		  return -1; }
	       /*0 Info2("dup2(%d, %d)", ttyfd, fdi);*/
	    }
	    if (rw != XIO_WRONLY && fdo != ttyfd) {
	       /* make sure that the internal diagnostic socket pair fds do not conflict
		  with our choices */
	       diag_reserve_fd(fdo);
	       if (Dup2(ttyfd, fdo) < 0) {
		  Error3("dup2(%d, %d): %s", ttyfd, fdo, strerror(errno));
		  return -1; }
	       /*0 Info2("dup2(%d, %d)", ttyfd, fdo);*/
	    }
	    if ((rw == XIO_RDONLY || fdi != ttyfd) &&
		(rw == XIO_WRONLY || fdo != ttyfd)) {
	       applyopts_cloexec(ttyfd, copts);
	    }

	    applyopts(sfd, ttyfd, copts, PH_LATE);
	    applyopts(sfd, ttyfd, copts, PH_LATE2);
	 } else
#endif /* HAVE_PTY */
	    if (usepipes) {
	       /* we might have a temporary conflict between what FDs are
		  currently allocated, and which are to be used. We try to find
		  a graceful solution via temporary descriptors */
	       int tmpi, tmpo;

	       if (rw != XIO_WRONLY)  Close(rdpip[0]);
	       if (rw != XIO_RDONLY)  Close(wrpip[1]);
	       if (fdi == rdpip[1]) {	/* a conflict here */
		  if ((tmpi = Dup(wrpip[0])) < 0) {
		     Error2("dup(%d): %s", wrpip[0], strerror(errno));
		     return -1;
		  }
		  /*0 Info2("dup(%d) -> %d", wrpip[0], tmpi);*/
		  rdpip[1] = tmpi;
	       }
	       if (fdo == wrpip[0]) {	/* a conflict here */
		  if ((tmpo = Dup(rdpip[1])) < 0) {
		     Error2("dup(%d): %s", rdpip[1], strerror(errno));
		     return -1;
		  }
		  /*0 Info2("dup(%d) -> %d", rdpip[1], tmpo);*/
		  wrpip[0] = tmpo;
	       }

	       if (rw != XIO_WRONLY && rdpip[1] != fdo) {
		  /* make sure that the internal diagnostic socket pair fds do not conflict
		     with our choices */
		  diag_reserve_fd(fdo);
		  if (Dup2(rdpip[1], fdo) < 0) {
		     Error3("dup2(%d, %d): %s", rdpip[1], fdo, strerror(errno));
		     return -1;
		  }
		  Close(rdpip[1]);
	       }
	       if (rw != XIO_RDONLY && wrpip[0] != fdi) {
		  /* make sure that the internal diagnostic socket pair fds do not conflict
		     with our choices */
		  diag_reserve_fd(fdi);
		  if (Dup2(wrpip[0], fdi) < 0) {
		     Error3("dup2(%d, %d): %s", wrpip[0], fdi, strerror(errno));
		     return -1;
		  }
		  Close(wrpip[0]);
		  /* applyopts_cloexec(fdi, *copts);*/	/* option is already consumed! */
	       }

	       applyopts(sfd, fdi, copts, PH_LATE);
	       applyopts(sfd, fdo, copts, PH_LATE);
	       applyopts(sfd, fdi, copts, PH_LATE2);
	       applyopts(sfd, fdo, copts, PH_LATE2);

	    } else {	/* socketpair */
	       Close(sv[0]);
	       if (rw != XIO_RDONLY && fdi != sv[1]) {
		  /* make sure that the internal diagnostic socket pair fds do not conflict
		     with our choices */
		  diag_reserve_fd(fdi);
		  if (Dup2(sv[1], fdi) < 0) {
		     Error3("dup2(%d, %d): %s", sv[1], fdi, strerror(errno));
		     return -1; }
		  /*0 Info2("dup2(%d, %d)", sv[1], fdi);*/
	       }
	       if (rw != XIO_WRONLY && fdo != sv[1]) {
		  /* make sure that the internal diagnostic socket pair fds do not conflict
		     with our choices */
		  diag_reserve_fd(fdo);
		  if (Dup2(sv[1], fdo) < 0) {
		     Error3("dup2(%d, %d): %s", sv[1], fdo, strerror(errno));
		     return -1; }
		  /*0 Info2("dup2(%d, %d)", sv[1], fdo);*/
	       }
	       if (fdi != sv[1] && fdo != sv[1]) {
		  applyopts_cloexec(sv[1], copts);
		  Close(sv[1]);
	       }

	       applyopts(sfd, fdi, copts, PH_LATE);
	       applyopts(sfd, fdi, copts, PH_LATE2);
	    }
	 if (withfork) {
	    Info("notifying parent that child process is ready");
	    Close(trigger[1]); 	/* in child, notify parent that ready */
	 }
      } /* withfork */
      else {
	 applyopts(sfd, -1, copts, PH_LATE);
	 applyopts(sfd, -1, copts, PH_LATE2);
      }
      _xioopen_setdelayeduser();
      if (withstderr) {
	 *duptostderr = fdo;
      } else {
	 *duptostderr = -1;
      }

      *optsp = copts;
      return 0;	/* indicate child process */
   }

   /* for parent (this is our socat process) */
   Notice1("forked off child process "F_pid, pid);
   Close(trigger[1]); 	/* in parent */

#if HAVE_PTY
   if (usepty) {
#  if 0
      if (Close(ttyfd) < 0) {
	 Info2("close(%d): %s", ttyfd, strerror(errno));
      }
#  endif
   } else
#endif /* HAVE_PTY */
   if (usepipes) {
      if (rw == XIO_RDONLY)  Close(rdpip[1]);
      if (rw == XIO_WRONLY)  Close(wrpip[0]);
   } else {
      Close(sv[1]);
   }
   sfd->para.exec.pid = pid;

   if (applyopts_single(sfd, popts, PH_LATE) < 0)  return -1;
   applyopts(sfd, -1, popts, PH_LATE);
   applyopts(sfd, -1, popts, PH_LATE2);
   applyopts(sfd, -1, popts, PH_PASTEXEC);
   if ((numleft = leftopts(popts)) > 0) {
      showleft(popts);
      Error1("INTERNAL: %d option(s) remained unused", numleft);
      return STAT_NORETRY;
   }

   {
      struct pollfd fds[1];
      fds[0].fd = trigger[0];
      fds[0].events = POLLIN|POLLHUP;
      Poll(fds, 1, -1);
      Info("child process notified parent that it is ready");
   }

#if HAVE_PTY
   applyopts(sfd, ptyfd, popts, PH_LATE);
#endif /* HAVE_PTY */
   if (applyopts_single(sfd, popts, PH_LATE) < 0)
      return -1;

   *optsp = popts;
   return pid;	/* indicate parent (main) process */
}
#endif /* WITH_EXEC || WITH_SYSTEM */


int setopt_path(struct opt *opts, char **path) {
   if (retropt_string(opts, OPT_PATH, path) >= 0) {
      if (setenv("PATH", *path, 1) < 0) {
	 Error1("setenv(\"PATH\", \"%s\", 1): insufficient space", *path);
	 return -1;
      }
   }
   return 0;
}
