/* Source: xio-posixmq.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* This file contains the source for opening addresses of POSIX MQ type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-socket.h"
#include "xio-listen.h"
#include "xio-posixmq.h"
#include "xio-named.h"


#if WITH_POSIXMQ

static int _posixmq_flush(struct single *sfd);
static int _posixmq_unlink(const char *name, int level);

static int xioopen_posixmq(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

const struct addrdesc xioaddr_posixmq_bidir   = { "POSIXMQ-BIDIRECTIONAL", 1+XIO_RDWR,   xioopen_posixmq, GROUP_FD|GROUP_OPEN|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY,                  XIO_RDWR,   0, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_read    = { "POSIXMQ-READ",          1+XIO_RDONLY, xioopen_posixmq, GROUP_FD|GROUP_OPEN|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY,                  XIO_RDONLY, 0, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_receive = { "POSIXMQ-RECEIVE",       1+XIO_RDONLY, xioopen_posixmq, GROUP_FD|GROUP_OPEN|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY|GROUP_CHILD,      XIO_RDONLY, XIOREAD_RECV_ONESHOT, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_send    = { "POSIXMQ-SEND",          1+XIO_WRONLY, xioopen_posixmq, GROUP_FD|GROUP_OPEN|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY|GROUP_CHILD,      XIO_WRONLY, 0, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_write   = { "POSIXMQ-WRITE",         1+XIO_WRONLY, xioopen_posixmq, GROUP_FD|GROUP_OPEN|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY|GROUP_CHILD,      XIO_WRONLY, 0, 0 HELP(":<mqname>") };

const struct optdesc opt_posixmq_priority   = { "posixmq-priority",   "mq-prio",  OPT_POSIXMQ_PRIORITY,   GROUP_POSIXMQ, PH_INIT, TYPE_BOOL, OFUNC_OFFSET, XIO_OFFSETOF(para.posixmq.prio), XIO_SIZEOF(para.posixmq.prio), 0 };
const struct optdesc opt_posixmq_flush      = { "posixmq-flush",      "mq-flush", OPT_POSIXMQ_FLUSH,      GROUP_POSIXMQ, PH_EARLY, TYPE_BOOL, OFUNC_SPEC,   0,                               0,                             0 };
const struct optdesc opt_posixmq_maxmsg     = { "posixmq-maxmsg",     "mq-maxmsg",  OPT_POSIXMQ_MAXMSG,   GROUP_POSIXMQ, PH_OPEN,  TYPE_LONG, OFUNC_SPEC,   0,                               0,                             0 };
const struct optdesc opt_posixmq_msgsize    = { "posixmq-msgsize",    "mq-msgsize", OPT_POSIXMQ_MSGSIZE,  GROUP_POSIXMQ, PH_OPEN,  TYPE_LONG, OFUNC_SPEC,   0,                               0,                             0 };

/* _read(): open immediately, stay in transfer loop
   _recv(): wait until data (how we know there is??), oneshot, opt.fork
*/
static int xioopen_posixmq(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
	/* We expect the form: /mqname */
	xiosingle_t *sfd = &xfd->stream;
	const char *name;
	int dirs = addrdesc->arg1;
	int oneshot = addrdesc->arg2;
	bool opt_unlink_early = false;
	bool nonblock = 0;
	bool flush = false;
	long maxmsg;
	long msgsize;
	struct mq_attr attr = { 0 };
	bool setopts = false;
	int oflag;
	bool opt_o_creat = true;
	bool opt_o_excl = false;
#ifdef O_CLOEXEC
	bool opt_o_cloexec = true;
#endif
	mode_t opt_mode = 0666;
	mqd_t mqd;
	int _errno;
	bool dofork = false;
	int maxchildren = 0;
	bool with_intv = false;
	int result = 0;

	if (argc != 2) {
		xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
		return STAT_NORETRY;
	}

	name = argv[1];

	retropt_bool(opts, OPT_FORK, &dofork);
	if (dofork) {
		if (!(xioflags & XIO_MAYFORK)) {
			Error1("%s: option fork not allowed in this context", argv[0]);
			return STAT_NORETRY;
		}
		sfd->flags |= XIO_DOESFORK;
		if (dirs == XIO_WRONLY) {
			with_intv = true;
		}
	}
	if (dirs == XIO_RDWR) {
		/* Bidirectional ADDRESS in unidirectional mode? Adapt dirs */
		dirs = (xioflags & XIO_ACCMODE);
	}
	retropt_int(opts, OPT_MAX_CHILDREN, &maxchildren);
	if (! dofork && maxchildren) {
		Error("option max-children not allowed without option fork");
		return STAT_NORETRY;
	}
	if (maxchildren) {
		xiosetchilddied(); 	/* set SIGCHLD handler */
	}
	applyopts_offset(sfd, opts);
	if (applyopts_single(sfd, opts, PH_INIT) < 0)  return STAT_NORETRY;
	applyopts(sfd, -1, opts, PH_INIT);

	if ((sfd->para.posixmq.name = strdup(name)) == NULL) {
		Error1("strdup(\"%s\"): out of memory", name);
	}

	retropt_bool(opts, OPT_O_CREAT,   &opt_o_creat);
	retropt_bool(opts, OPT_O_EXCL,    &opt_o_excl);
#ifdef O_CLOEXEC
	retropt_bool(opts, OPT_O_CLOEXEC, &opt_o_cloexec);
#endif
	retropt_mode(opts, OPT_PERM,      &opt_mode);
	retropt_bool(opts, OPT_POSIXMQ_FLUSH, &flush);
	retropt_long(opts, OPT_POSIXMQ_MAXMSG,  &maxmsg) ||
		(setopts = true);
	retropt_long(opts, OPT_POSIXMQ_MSGSIZE, &msgsize) ||
		(setopts = true);

	/* When just one of mq-maxmsg and mq-msgsize options has been provided,
	   we must nevertheless set the other option value in struct mq_attr.
	   For this we have to find the default, read it from /proc fs */
	if (setopts) {
		int pfd;
		const static char *PROC_MAXMSG  = "/proc/sys/fs/mqueue/msg_default";
		const static char *PROC_MSGSIZE = "/proc/sys/fs/mqueue/msgsize_default";
		char buff[21]; 	/* fit a 64bit num in decimal */
		ssize_t bytes;

		if (maxmsg == 0) {
			if ((pfd = Open(PROC_MAXMSG, O_RDONLY, 0)) < 0) {
				Warn2("open(\"%s\", O_RDONLY, 0): %s", PROC_MAXMSG, strerror(errno));
			} else if ((bytes = Read(pfd, buff, sizeof(buff)-1)) < 0) {
				Warn4("read(%d /* \"%s\" */, buff, "F_Zd"): %s",
				      pfd, PROC_MAXMSG, sizeof(buff)-1, strerror (errno));
				Close(pfd);
			} else {
				sscanf(buff, "%ld", &maxmsg);
				Close(pfd);
			}
		}

		if (msgsize == 0) {
			if ((pfd = Open(PROC_MSGSIZE, O_RDONLY, 0)) < 0) {
				Warn2("open(\"%s\", O_RDONLY, 0): %s", PROC_MSGSIZE, strerror(errno));
			} else if ((bytes = Read(pfd, buff, sizeof(buff)-1)) < 0) {
				Warn4("read(%d /* \"%s\" */, buff, "F_Zd"): %s",
				      pfd, PROC_MSGSIZE, sizeof(buff)-1, strerror (errno));
				Close(pfd);
			} else {
				sscanf(buff, "%ld", &msgsize);
				Close(pfd);
			}
		}
	}

	retropt_bool(opts, OPT_UNLINK_EARLY, &opt_unlink_early);
	if (opt_unlink_early) {
		_posixmq_unlink(sfd->para.posixmq.name, E_INFO);
	}
	retropt_bool(opts, OPT_UNLINK_CLOSE, &sfd->opt_unlink_close);
	if (sfd->howtoend == END_UNSPEC)
	   sfd->howtoend = END_CLOSE;
	sfd->dtype = XIODATA_POSIXMQ | oneshot;

	oflag = 0;
	if (opt_o_creat)   oflag |= O_CREAT;
	if (opt_o_excl)    oflag |= O_EXCL;
#ifdef O_CLOEXEC
	if (opt_o_cloexec) oflag |= O_CLOEXEC; 	/* does not seem to work (Ubuntu-20) */
#endif
	switch (dirs) {
	case XIO_RDWR:   oflag |= O_RDWR;   break;
	case XIO_RDONLY: oflag |= O_RDONLY; break;
	case XIO_WRONLY: oflag |= O_WRONLY; break;
	}
	if (retropt_bool(opts, OPT_O_NONBLOCK, &nonblock) >= 0 && nonblock)
		oflag |= O_NONBLOCK;

	if (flush) {
		if (_posixmq_flush(sfd) != STAT_OK)
			return STAT_NORETRY;
	}

	/* Now open the message queue */
	if (setopts) {
		attr.mq_maxmsg  = maxmsg;
		attr.mq_msgsize = msgsize;
		Debug8("%s: mq_open(\"%s\", "F_mode", "F_mode", {flags=%ld, maxmsg=%ld, msgsize=%ld, curmsgs=%ld} )", argv[0], name, oflag, opt_mode, attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
	} else {
		Debug4("%s: mq_open(\"%s\", "F_mode", "F_mode", NULL)", argv[0], name, oflag, opt_mode);
	}
	mqd = mq_open(name, oflag, opt_mode, setopts ? &attr : NULL);
	_errno = errno;
	Debug1("mq_open() -> %d", mqd);
	if (mqd < 0) {
		if (setopts)
			Error9("%s: mq_open(\"%s\", "F_mode", "F_mode", {flags=%ld, maxmsg=%ld, msgsize=%ld, curmsgs=%ld} ): %s", argv[0], name, oflag, opt_mode, attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs, strerror(errno));
		else
			Error5("%s: mq_open(\"%s\", "F_mode", "F_mode", NULL): %s", argv[0], name, oflag, opt_mode, strerror(errno));
		errno = _errno;
		return STAT_RETRYLATER;
	}
	/* applyopts_cloexec(mqd, opts); */	/* does not seem to work too (Ubuntu-20) */
	sfd->fd = mqd;

	Debug1("mq_getattr(%d, ...)", mqd);
	if (mq_getattr(mqd, &attr) < 0) {
		Warn4("mq_getattr(%d[\"%s\"], %p): %s",
		      mqd, sfd->para.posixmq.name, &attr, strerror(errno));
		mq_close(mqd);
		return STAT_NORETRY;
	}
	Info5("POSIXMQ queue \"%s\" attrs: { flags=%ld, maxmsg=%ld, msgsize=%ld, curmsgs=%ld }",
	      name, attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
	if (setopts) {
		if (attr.mq_maxmsg != maxmsg)
			Warn2("mq_open(): requested maxmsg=%ld, but result is %ld",
			      maxmsg, attr.mq_maxmsg);
		if (attr.mq_msgsize != msgsize)
			Warn2("mq_open(): requested msgsize=%ld, but result is %ld",
			      msgsize, attr.mq_msgsize);
	}

	if (!dofork && !oneshot) {
		return STAT_OK;
	}
	/* Continue with modes that open only when data available */

	if (!oneshot) {
		if (xioparms.logopt == 'm') {
			Info("starting POSIX-MQ fork loop, switching to syslog");
			diag_set('y', xioparms.syslogfac);  xioparms.logopt = 'y';
		} else {
			Info("starting POSIX-MQ fork loop");
		}
	}

	/* Wait until a message is available (or until interval has expired),
	   then fork a sub process that handles this single message. Here we
	   continue waiting for more.
	   The trigger mechanism is described with function
	   _xioopen_dgram_recvfrom()
	*/
	while (true) {
		int trigger[2];
		pid_t pid; 	/* mostly int; only used with fork */
		sigset_t mask_sigchld;

		Info1("%s: waiting for data or interval", argv[0]);
		do {
			struct pollfd pollfd;

			if (oflag & O_NONBLOCK)
				break;
			pollfd.fd = sfd->fd;
			pollfd.events = (dirs==XIO_RDONLY?POLLIN:POLLOUT);
			if (xiopoll(&pollfd, 1, NULL) > 0) {
				break;
			}
			if (errno == EINTR) {
				continue;
			}
			Warn2("poll({%d,,},,-1): %s", sfd->fd, strerror(errno));
			Sleep(1);
		} while (true);
		if (!dofork)  return STAT_OK;

		Info("generating pipe that triggers parent when packet has been consumed");
		if (dirs == XIO_RDONLY) {
			if (Pipe(trigger) < 0) {
				Error1("pipe(): %s", strerror(errno));
			}
		}

		/* Block SIGCHLD until parent is ready to react */
		sigemptyset(&mask_sigchld);
		sigaddset(&mask_sigchld, SIGCHLD);
		Sigprocmask(SIG_BLOCK, &mask_sigchld, NULL);

		if ((pid = xio_fork(false, E_ERROR, xfd->stream.shutup)) < 0) {
			Sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL);
			if (dirs==XIO_RDONLY) {
				Close(trigger[0]);
				Close(trigger[1]);
			}
			xioclose_posixmq(sfd);
			return STAT_RETRYLATER;
		}
		if (pid == 0) {  	/* child */
			pid_t cpid = Getpid();
			Sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL);
			xiosetenvulong("PID", cpid, 1);

			if (dirs == XIO_RDONLY) {
				Close(trigger[0]);
				Fcntl_l(trigger[1], F_SETFD, FD_CLOEXEC);
				sfd->triggerfd = trigger[1];
			}
			break;
		}

		/* Parent */
		if (dirs == XIO_RDONLY) {
			char buf[1];
			Close(trigger[1]);
			while (Read(trigger[0], buf, 1) < 0 && errno == EINTR)
				;
		}

#if WITH_RETRY
		if (with_intv) {
			Nanosleep(&sfd->intervall, NULL);
		}
#endif

		/* now we are ready to handle signals */
		Sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL);
		while (maxchildren) {
			if (num_child < maxchildren)  break;
			Notice1("max of %d children is active, waiting", num_child);
			while (!Sleep(UINT_MAX)) ;   /* any signal lets us continue */
		}
		Info("continue listening");
	}

	_xio_openlate(sfd, opts);
	return result;
}


/* With option flush try to open the queue and "consume" its current contents */
static int _posixmq_flush(
	struct single *sfd)
{
	mqd_t mqd;
	struct mq_attr attr;
	void *buff;
	size_t bufsiz;
	int _errno;
	int p = 0; 		/* number of messages flushed */
	size_t b = 0; 	/* number of bytes flushed */

	Info1("flushing POSIXMQ queue \"%s\"", sfd->para.posixmq.name);
	Debug1("mq_open(\"%s\", O_RDONLY|O_NONBLOCK, 0, NULL)",
	       sfd->para.posixmq.name);
	mqd = mq_open(sfd->para.posixmq.name, O_RDONLY|O_NONBLOCK, 0, NULL);
	_errno = errno;
	Debug1("mq_open() -> %d", mqd);
	if (mqd < 0 && _errno == ENOENT) {
		Info("this queue does not exist, no need to flush it");
		return STAT_OK;
	}
	if (mqd < 0) {
		Warn2("mq_open(\"%s\", ...): %s", sfd->para.posixmq.name,
		      strerror(_errno));
		return STAT_NORETRY;
	}

	Debug1("mq_getattr(%d, ...)", mqd);
	if (mq_getattr(mqd, &attr) < 0) {
		Warn4("mq_getattr(%d[\"%s\"], %p): %s",
		      mqd, sfd->para.posixmq.name, &attr, strerror(errno));
		mq_close(mqd);
		return STAT_NORETRY;
	}
	if (attr.mq_curmsgs == 0) {
		Info1("POSIXMQ \"%s\" is empty", sfd->para.posixmq.name);
		mq_close(mqd);
		return STAT_OK;
	}
	bufsiz = attr.mq_msgsize;
	if ((buff = Malloc(bufsiz)) == NULL) {
		mq_close(mqd);
		return STAT_RETRYLATER;
	}

	/* Now read all messages to null */
	while (true) {
		ssize_t bytes;

		Debug3("mq_receive(mqd=%d, %p, "F_Zu", {} )", mqd, buff, bufsiz);
		bytes = mq_receive(mqd, buff, bufsiz, &sfd->para.posixmq.prio);
		_errno = errno;
		Debug1("mq_receive() -> "F_Zd, bytes);
		errno = _errno;
		if (bytes == 0 || (bytes < 0 && _errno == EAGAIN)) {
			break;
		}
		if (bytes < 0) {
			Warn2("flushing POSIXMQ \"%s\" failed: %s",
			      sfd->para.posixmq.name, strerror(_errno));
			free(buff);
			mq_close(mqd);
			return STAT_NORETRY;
		}
		++p;
		b += bytes;
	}
	Info3("flushed "F_Zu" bytes in %u packets from queue \"%s\"", b, p,
	      sfd->para.posixmq.name);
	free(buff);
	mq_close(mqd);
	return STAT_OK;
}

ssize_t xiowrite_posixmq(
	struct single *sfd,
	const void *buff,
	size_t bufsiz)
{
	int res;
	int _errno;

	Debug4("mq_send(mqd=%d, %p, "F_Zu", %u)", sfd->fd, buff, bufsiz, sfd->para.posixmq.prio);
	res = mq_send(sfd->fd, buff, bufsiz, sfd->para.posixmq.prio);
	_errno = errno;
	Debug1("mq_send() -> %d", res);
	errno = _errno;
	if (res < 0) {
		Error2("mq_send(mqd=%d): %s", sfd->fd, strerror(errno));
		return -1;
	}
	return bufsiz; 	/* success */
}

ssize_t xioread_posixmq(
	struct single *sfd,
	void *buff,
	size_t bufsiz)
{
	ssize_t res;
	int _errno;

	Debug3("mq_receive(mqd=%d, %p, "F_Zu", {} )", sfd->fd, buff, bufsiz);
	res = mq_receive(sfd->fd, buff, bufsiz, &sfd->para.posixmq.prio);
	_errno = errno;
	Debug1("mq_receive() -> "F_Zd, res);
	errno = _errno;
	if (res < 0) {
		Error2("mq_receive(mqd=%d): %s", sfd->fd, strerror(errno));
		return -1;
	}
	if (sfd->triggerfd > 0) {
		Close(sfd->triggerfd);
		sfd->triggerfd = -1;
	}
	Info1("mq_receive() ->  {prio=%u}", sfd->para.posixmq.prio);
	xiosetenvulong("POSIXMQ_PRIO", (unsigned long)sfd->para.posixmq.prio, 1);
	return res;
}

ssize_t xiopending_posixmq(struct single *sfd);

ssize_t xioclose_posixmq(
	struct single *sfd)
{
	int res;

	if (sfd->fd < 0)
		return 0;
	Debug1("xioclose_posixmq(): mq_close(%d)", sfd->fd);
	res = mq_close(sfd->fd);
	if (res < 0) {
		Warn2("xioclose_posixmq(): mq_close(%d) -> -1: %s", sfd->fd, strerror(errno));
	} else {
		Debug("xioclose_posixmq(): mq_close() -> 0");
	}
	if (sfd->opt_unlink_close) {
		_posixmq_unlink(sfd->para.posixmq.name, E_WARN);
	}
	free((void *)sfd->para.posixmq.name);
	return 0;
}

static int _posixmq_unlink(
	const char *name,
	int level) 		/* message level on error */
{
	int _errno;
	int res;

	Debug1("mq_unlink(\"%s\")", name);
	res = mq_unlink(name);
	_errno = errno;
	Debug1("mq_unlink() -> %d", res);
	errno = _errno;
	if (res < 0) {
		Msg2(level, "mq_unlink(\"%s\"): %s",name, strerror(errno));
	}
	return res;
}

#endif /* WITH_POSIXMQ */
