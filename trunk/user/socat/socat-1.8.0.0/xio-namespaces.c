/* Source: xio-namespaces.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* This file contains Linux namespace related code */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-namespaces.h"

#if WITH_NAMESPACES

const struct optdesc opt_set_netns   = { "netns",      NULL, OPT_SET_NETNS,   GROUP_PROCESS, PH_INIT, TYPE_STRING, OFUNC_SET_NAMESPACE,   0, 0, 0 };


/* Set the given namespace. Requires root or the appropriate CAP_*-
   Returns 0 on success, or -1 on error. */
int xio_set_namespace(
	const char *nstype,
	const char *nsname)
{
	char nspath[PATH_MAX];
	int nsfd;
	int rc;

	if (!xioparms.experimental) {
		Error1("option \"%s\" requires use of --experimental", nstype);
	}

	snprintf(nspath, sizeof(nspath)-1, "/run/%s/%s", nstype, nsname);
	Info1("switching to net namespace \"%s\"", nsname);
	nsfd = Open(nspath, O_RDONLY|O_CLOEXEC, 000);
	if (nsfd < 0) {
		Error2("open(%s, O_RDONLY|O_CLOEXEC): %s", nspath, strerror(errno));
		return -1;
	}
	rc = Setns(nsfd, CLONE_NEWNET);
	if (rc < 0) {
		Error2("setns(%d, CLONE_NEWNET): %s", nsfd, strerror(errno));
		Close(nsfd);
	}
	Close(nsfd);
	return 0;
}

int xio_apply_namespace(
	struct opt *opts)
{
	int old_netfd;
	char *netns_name;
	char old_nspath[PATH_MAX];
	int rc;

	if (retropt_string(opts, OPT_SET_NETNS, &netns_name) < 0)
		return 0;

	/* Get path describing current namespace */
	snprintf(old_nspath, sizeof(old_nspath)-1, "/proc/"F_pid"/ns/net",
		 Getpid());

	/* Get a file descriptor to current ns for later reset */
	old_netfd = Open(old_nspath, O_RDONLY|O_CLOEXEC, 000);
	if (old_netfd < 0) {
		Error2("open(%s, O_RDONLY|O_CLOEXEC): %s",
		       old_nspath, strerror(errno));
		free(netns_name);
		return -1;
	}
	if (old_netfd == 0) {
		/* 0 means not netns option, oops */
		Error1("%s(): INTERNAL", __func__);
		free(netns_name);
		Close(old_netfd);
		return -1;
	}
	rc = xio_set_namespace("netns", netns_name);
	free(netns_name);
	if (rc < 0) {
		Close(old_netfd);
		return -1;
	}

	return old_netfd;
}

/* Sets the given namespace to that of process 1, this is assumed to be the
   systems default.
   Returns 0 on success, or -1 on error. */
int xio_reset_namespace(
	int saved_netfd)
{
	int rc;

	rc = Setns(saved_netfd, CLONE_NEWNET);
	if (rc < 0) {
		Error2("xio_reset_namespace(%d): %s", saved_netfd, strerror(errno));
		Close(saved_netfd);
		return STAT_NORETRY;
	}
	Close(saved_netfd);
	return 0;
}

#endif /* WITH_NAMESPACES */
