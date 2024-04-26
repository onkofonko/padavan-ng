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

static int _posixmq_unlink(
	const char *name,
	int level); 		/* message level on error */

static int xioopen_posixmq(int argc, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

const struct addrdesc xioaddr_posixmq_bidir   = { "POSIXMQ-BIDIRECTIONAL", 1+XIO_RDWR,   xioopen_posixmq, GROUP_FD|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY,                  XIO_RDWR,   0, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_read    = { "POSIXMQ-READ",          1+XIO_RDONLY, xioopen_posixmq, GROUP_FD|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY,                  XIO_RDONLY, 0, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_receive = { "POSIXMQ-RECEIVE",       1+XIO_RDONLY, xioopen_posixmq, GROUP_FD|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY|GROUP_CHILD,      XIO_RDONLY, XIOREAD_RECV_ONESHOT, 0 HELP(":<mqname>") };
const struct addrdesc xioaddr_posixmq_send    = { "POSIXMQ-SEND",          1+XIO_WRONLY, xioopen_posixmq, GROUP_FD|GROUP_NAMED|GROUP_POSIXMQ|GROUP_RETRY|GROUP_CHILD,      XIO_WRONLY, 0, 0 HELP(":<mqname>") };

const struct optdesc opt_posixmq_priority   = { "posixmq-priority",   "mq-pri",   OPT_POSIXMQ_PRIORITY,   GROUP_POSIXMQ, PH_INIT, TYPE_BOOL, OFUNC_OFFSET, XIO_OFFSETOF(para.posixmq.prio), XIO_SIZEOF(para.posixmq.prio), 0 };

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
	int oflag;
	bool opt_o_excl = false;
	mode_t opt_mode = 0666;
	mqd_t mqd;
	int _errno;
	bool dofork = false;
	int maxchildren = 0;
	bool with_intv = false;
	int result = 0;

	if (!xioparms.experimental) {
		Error1("%s: use option --experimental to acknowledge unmature state", argv[0]);
		return STAT_NORETRY;
	}
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

	retropt_bool(opts, OPT_O_EXCL, &opt_o_excl);
	retropt_mode(opts, OPT_PERM, &opt_mode);

	retropt_bool(opts, OPT_UNLINK_EARLY, &opt_unlink_early);
	if (opt_unlink_early) {
		_posixmq_unlink(sfd->para.posixmq.name, E_INFO);
	}
	retropt_bool(opts, OPT_UNLINK_CLOSE, &sfd->opt_unlink_close);
	if (sfd->howtoend == END_UNSPEC)
	   sfd->howtoend = END_CLOSE;
	sfd->dtype = XIODATA_POSIXMQ | oneshot;

	oflag = O_CREAT;
	if (opt_o_excl)  oflag |= O_EXCL;
	switch (dirs) {
	case XIO_RDWR:   oflag |= O_RDWR;   break;
	case XIO_RDONLY: oflag |= O_RDONLY; break;
	case XIO_WRONLY: oflag |= O_WRONLY; break;
	}

	/* Now open the message queue */
	Debug3("mq_open(\"%s\", %d, "F_mode", NULL)", name, oflag, opt_mode);
	mqd = mq_open(name, oflag, opt_mode, NULL);
	_errno = errno;
	Debug1("mq_open() -> %d", mqd);
	if (mqd < 0) {
		Error3("%s: mq_open(\"%s\"): %s", argv[0], name, strerror(errno));
		errno = _errno;
		return STAT_RETRYLATER;
	}
	sfd->fd = mqd;

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
