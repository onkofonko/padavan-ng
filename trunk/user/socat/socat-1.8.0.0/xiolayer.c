/* source: xiolayer.c */
/* Copyright Gerhard Rieger and contributors (see file CHANGES) */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for common options */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xiolayer.h"

/****** for ALL addresses - by application ******/
const struct optdesc opt_ignoreeof = { "ignoreeof", NULL, OPT_IGNOREEOF, GROUP_APPL, PH_LATE, TYPE_BOOL,  OFUNC_EXT, XIO_OFFSETOF(ignoreeof),   XIO_SIZEOF(ignoreeof) };
const struct optdesc opt_cr        = { "cr",        NULL, OPT_CR,        GROUP_APPL, PH_LATE, TYPE_CONST, OFUNC_EXT, XIO_OFFSETOF(lineterm),    XIO_SIZEOF(lineterm), LINETERM_CR };
const struct optdesc opt_crnl      = { "crnl",      NULL, OPT_CRNL,      GROUP_APPL, PH_LATE, TYPE_CONST, OFUNC_EXT, XIO_OFFSETOF(lineterm),   XIO_SIZEOF(lineterm), LINETERM_CRNL };
const struct optdesc opt_readbytes = { "readbytes", "bytes", OPT_READBYTES, GROUP_APPL, PH_LATE, TYPE_SIZE_T, OFUNC_EXT, XIO_OFFSETOF(readbytes),   XIO_SIZEOF(readbytes) };
const struct optdesc opt_lockfile  = { "lockfile",  NULL, OPT_LOCKFILE,  GROUP_APPL, PH_INIT, TYPE_FILENAME, OFUNC_EXT, 0, 0 };
const struct optdesc opt_waitlock  = { "waitlock",  NULL, OPT_WAITLOCK,  GROUP_APPL, PH_INIT,  TYPE_FILENAME, OFUNC_EXT, 0, 0 };
const struct optdesc opt_escape    = { "escape",    NULL,    OPT_ESCAPE,    GROUP_APPL, PH_INIT, TYPE_INT,   OFUNC_OFFSET, XIO_OFFSETOF(escape), sizeof(((xiosingle_t *)0)->escape) };
/****** APPL addresses ******/
#if WITH_RETRY
const struct optdesc opt_forever   = { "forever",   NULL, OPT_FOREVER,   GROUP_RETRY, PH_INIT, TYPE_BOOL, OFUNC_EXT, XIO_OFFSETOF(forever),   XIO_SIZEOF(forever) };
const struct optdesc opt_intervall = { "interval",  NULL, OPT_INTERVALL, GROUP_RETRY, PH_INIT, TYPE_TIMESPEC, OFUNC_EXT, XIO_OFFSETOF(intervall), XIO_SIZEOF(intervall) };
const struct optdesc opt_retry     = { "retry",     NULL, OPT_RETRY,     GROUP_RETRY, PH_INIT, TYPE_UINT, OFUNC_EXT, XIO_OFFSETOF(retry),     XIO_SIZEOF(retry) };
#endif

const struct optdesc opt_chdir       = { "chdir",       "cd", OPT_CHDIR,       GROUP_ADDR, PH_INIT,  TYPE_FILENAME, OFUNC_SPEC };
const struct optdesc opt_umask       = { "umask",       NULL, OPT_UMASK,       GROUP_ADDR, PH_INIT,  TYPE_MODET,    OFUNC_SPEC };


int xio_chdir(
	struct opt* opts,
	char **orig_dir)
{
	char *tmp_dir = NULL;

	if (retropt_string(opts, OPT_CHDIR, &tmp_dir) < 0)
		return 0;

	if ((*orig_dir = Malloc(PATH_MAX)) == NULL) {
		free(tmp_dir);
		return -1;
	}

	if (getcwd(*orig_dir, PATH_MAX) == NULL) {
		Error1("getcwd(<ptr>, PATH_MAX): %s", strerror(errno));
		free(*orig_dir);
		free(tmp_dir);
		return -1;
	}
	*orig_dir = Realloc(*orig_dir, strlen(*orig_dir)+1);

	if (Chdir(tmp_dir) < 0) {
		Error2("chdir(\"%s\"): %s", tmp_dir, strerror(errno));
		free(*orig_dir);
		free(tmp_dir);
		return -1;
	}

	free(tmp_dir);
	return 1;
}
