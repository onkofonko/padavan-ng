/* source: xio-shell.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of shell type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-progcall.h"
#include "xio-shell.h"


#if WITH_SHELL

static int xioopen_shell(int arg, const char *argv[], struct opt *opts, int xioflags, xiofile_t *xfd, const struct addrdesc *addrdesc);

const struct addrdesc xioaddr_shell = { "SHELL", 3, xioopen_shell, GROUP_FD|GROUP_FORK|GROUP_EXEC|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_TERMIOS|GROUP_FIFO|GROUP_PTY|GROUP_PARENT|GROUP_SHELL, 1, 0, 0 HELP(":<shell-command>") };

const struct optdesc opt_shell = { "shell", NULL, OPT_SHELL, GROUP_SHELL, PH_PREEXEC, TYPE_STRING, OFUNC_SPEC, 0, 0 };

static int xioopen_shell(
	int argc,
	const char *argv[],
	struct opt *opts,
	int xioflags,
	xiofile_t *xfd,
	const struct addrdesc *addrdesc)
{
	struct single *sfd = &xfd->stream;
	int status;
	char *path = NULL;
	int duptostderr;
	int result;
	char *shellpath = NULL;
	const char *shellname;
	const char *string = argv[1];

	if (argc != 2) {
		xio_syntax(argv[0], 1, argc-1, addrdesc->syntax);
		return STAT_NORETRY;
	}

	shellpath = getenv("SHELL");
	retropt_string(opts, OPT_SHELL, &shellpath);
	if (shellpath == NULL) {
		Error("SHELL variable undefined");
		errno = EINVAL;
		return -1;
	}
	shellname = strrchr(shellpath, '/');
	if (shellname == NULL) {
		Error1("SHELL \"%s\" variable does not specify a path (has no '/')", shellpath);
		errno = EINVAL;
		return -1;
	}
	++shellname;

	status = _xioopen_foxec(xioflags, sfd, addrdesc->groups, &opts, &duptostderr);
	if (status < 0)  return status;
	if (status == 0) {	/* child */
		int numleft;

		if (setopt_path(opts, &path) < 0) {
			/* this could be dangerous, so let us abort this child... */
			Exit(1);
		}

		if ((numleft = leftopts(opts)) > 0) {
			Error1("%d option(s) could not be used", numleft);
			showleft(opts);
			return STAT_NORETRY;
		}

		/* only now redirect stderr */
		if (duptostderr >= 0) {
			diag_dup();
			Dup2(duptostderr, 2);
		}

		Setenv("SHELL", shellpath, 1);

		Info1("executing shell command \"%s\"", string);
		Debug3("execl(\"%s\", \"%s\", \"-c\", \"%s\", NULL",
		       shellpath, shellname, string);
		result = execl(shellpath, shellname, "-c", string, (char *)NULL);
		if (result != 0) {
			Warn2("execl(\"%s\") returned with status %d", string, result);
			Warn1("execl(): %s", strerror(errno));
		}
		Exit(0);	/* this child process */
	}

	/* parent */
	return 0;
}

#endif /* WITH_SHELL */
