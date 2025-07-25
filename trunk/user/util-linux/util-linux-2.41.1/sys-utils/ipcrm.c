/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 1993 rishna balasubramanian
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 1999-04-02 frank zago
 * - can now remove several id's in the same call
 * 
 * 2025 Prasanna Paithankar <paithankarprasanna@gmail.com>
 * - Added POSIX IPC support
 */
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "closestream.h"

#include "ipcutils.h"

typedef enum type_id {
	SHM,
	PSHM,
	SEM,
	PSEM,
	MSG,
	PMSG,
	ALL
} type_id;

static int verbose = 0;

/* print the usage */
static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %1$s [options]\n"
		       " %1$s shm|msg|sem <id>...\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Remove certain IPC resources.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -m, --shmem-id <id>        				remove shared memory segment by id\n"), out);
	fputs(_(" -M, --shmem-key <key>      				remove shared memory segment by key\n"), out);
	fputs(_("     --posix-shmem <name>   				remove POSIX shared memory segment by name\n"), out);
	fputs(_(" -q, --queue-id <id>        				remove message queue by id\n"), out);
	fputs(_(" -Q, --queue-key <key>      				remove message queue by key\n"), out);
	fputs(_("     --posix-mqueue <name>  				remove POSIX message queue by name\n"), out);
	fputs(_(" -s, --semaphore-id <id>    				remove semaphore by id\n"), out);
	fputs(_(" -S, --semaphore-key <key>  				remove semaphore by key\n"), out);
	fputs(_("     --posix-semaphore <name> 				remove POSIX semaphore by name\n"), out);
	fputs(_(" -a, --all[=shm|pshm|msg|pmsg|sem|psem]	remove all (in the specified category)\n"), out);
	fputs(_(" -v, --verbose              				explain what is being done\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(28));
	fprintf(out, USAGE_MAN_TAIL("ipcrm(1)"));

	exit(EXIT_SUCCESS);
}

static int remove_id(int type, int iskey, int id)
{
        int ret;
	char *errmsg;
	/* needed to delete semaphores */
	union semun arg;
	arg.val = 0;

	/* do the removal */
	switch (type) {
	case SHM:
		if (verbose)
			printf(_("removing shared memory segment id `%d'\n"), id);
		ret = shmctl(id, IPC_RMID, NULL);
		break;
	case MSG:
		if (verbose)
			printf(_("removing message queue id `%d'\n"), id);
		ret = msgctl(id, IPC_RMID, NULL);
		break;
	case SEM:
		if (verbose)
			printf(_("removing semaphore id `%d'\n"), id);
		ret = semctl(id, 0, IPC_RMID, arg);
		break;
	default:
		errx(EXIT_FAILURE, "impossible occurred");
	}

	/* how did the removal go? */
	if (ret < 0) {
		switch (errno) {
		case EACCES:
		case EPERM:
			errmsg = iskey ? _("permission denied for key") : _("permission denied for id");
			break;
		case EINVAL:
			errmsg = iskey ? _("invalid key") : _("invalid id");
			break;
		case EIDRM:
			errmsg = iskey ? _("already removed key") : _("already removed id");
			break;
		default:
			err(EXIT_FAILURE, "%s", iskey ? _("key failed") : _("id failed"));
		}
		warnx("%s (%d)", errmsg, id);
		return 1;
	}
	return 0;
}

static int remove_arg_list(type_id type, int argc, char **argv)
{
	int id;
	char *end = NULL;
	int nb_errors = 0;

	do {
		errno = 0;
		id = strtoul(argv[0], &end, 10);
		if (errno || !end || *end != 0) {
			warnx(_("invalid id: %s"), argv[0]);
			nb_errors++;
		} else {
			if (remove_id(type, 0, id))
				nb_errors++;
		}
		argc--;
		argv++;
	} while (argc);
	return (nb_errors);
}

static int deprecated_main(int argc, char **argv)
{
	type_id type;

	if (!strcmp(argv[1], "shm"))
		type = SHM;
	else if (!strcmp(argv[1], "msg"))
		type = MSG;
	else if (!strcmp(argv[1], "sem"))
		type = SEM;
	else
		return 0;

	if (argc < 3) {
		warnx(_("not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	if (remove_arg_list(type, argc - 2, &argv[2]))
		exit(EXIT_FAILURE);

	printf(_("resource(s) deleted\n"));
	return 1;
}

static unsigned long strtokey(const char *str, const char *errmesg)
{
	unsigned long num;
	char *end = NULL;

	if (str == NULL || *str == '\0')
		goto err;
	errno = 0;
	/* keys are in hex or decimal */
	num = strtoul(str, &end, 0);

	if (errno || str == end || (end && *end))
		goto err;

	return num;
 err:
	if (errno)
		err(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	else
		errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	return 0;
}

static int key_to_id(type_id type, char *s)
{
	int id;
	/* keys are in hex or decimal */
	key_t key = strtokey(s, "failed to parse argument");
	if (key == IPC_PRIVATE) {
		warnx(_("illegal key (%s)"), s);
		return -1;
	}
	switch (type) {
	case SHM:
		id = shmget(key, 0, 0);
		break;
	case MSG:
		id = msgget(key, 0);
		break;
	case SEM:
		id = semget(key, 0, 0);
		break;
	case ALL:
		abort();
	default:
		errx(EXIT_FAILURE, "impossible occurred");
	}
	if (id < 0) {
		char *errmsg;
		switch (errno) {
		case EACCES:
			errmsg = _("permission denied for key");
			break;
		case EIDRM:
			errmsg = _("already removed key");
			break;
		case ENOENT:
			errmsg = _("invalid key");
			break;
		default:
			err(EXIT_FAILURE, _("key failed"));
		}
		warnx("%s (%s)", errmsg, s);
	}
	return id;
}

static int remove_name(type_id type, char *name)
{
	int ret = 0;
	
	switch (type) {
		case PSHM:
			#ifndef HAVE_SYS_MMAN_H
			warnx(_("POSIX shared memory is not supported"));
			#else
			if (verbose)
				printf(_("removing POSIX shared memory `%s'\n"), name);
			ret = shm_unlink(name);
			#endif
			break;
		case PMSG:
			#ifndef HAVE_MQUEUE_H
			warnx(_("POSIX message queues are not supported"));
			#else
			if (verbose)
				printf(_("removing POSIX message queue `%s'\n"), name);
			ret = mq_unlink(name);
			#endif
			break;
		case PSEM:
			#ifndef HAVE_SEMAPHORE_H
			warnx(_("POSIX semaphores are not supported"));
			#else
			if (verbose)
				printf(_("removing POSIX semaphore `%s'\n"), name);
			ret = sem_unlink(name);
			#endif
			break;
		default:
			errx(EXIT_FAILURE, "impossible occurred");
	}

	if (ret < 0) {
		switch (errno) {
		case EACCES:
		case EPERM:
			warnx(_("permission denied for name `%s'"), name);
			break;
		case ENOENT:
			warnx(_("name `%s' not found"), name);
			break;
		case ENAMETOOLONG:
			warnx(_("name `%s' too long"), name);
			break;
		default:
			err(EXIT_FAILURE, _("name failed"));
		}
		return 1;
	}
	return 0;
}

static int remove_all(type_id type)
{
	int ret = 0;
	int id, rm_me, maxid;

	struct shmid_ds shmseg;

	struct posix_shm_data *shmds, *shmdsp;

	struct semid_ds semary;
	struct seminfo seminfo;
	union semun arg;

	struct posix_sem_data *semds, *semdsp;

	struct msqid_ds msgque;
	struct msginfo msginfo;

	struct posix_msg_data *msgds, *msgdsp;

	if (type == SHM || type == ALL) {
		maxid = shmctl(0, SHM_INFO, &shmseg);
		if (maxid < 0)
			errx(EXIT_FAILURE,
			     _("kernel not configured for shared memory"));
		for (id = 0; id <= maxid; id++) {
			rm_me = shmctl(id, SHM_STAT, &shmseg);
			if (rm_me < 0)
				continue;
			ret |= remove_id(SHM, 0, rm_me);
		}
	}
	if (type == PSHM || type == ALL) {
		if (posix_ipc_shm_get_info(NULL, &shmds) > 0) {
			for (shmdsp = shmds; shmdsp->next != NULL; shmdsp = shmdsp->next) {
				if (verbose)
					printf(_("removing POSIX shared memory `%s'\n"), shmdsp->name);
				ret |= remove_name(PSHM, shmdsp->name);
			}
			posix_ipc_shm_free_info(shmds);
		}
	}
	if (type == SEM || type == ALL) {
		arg.array = (ushort *) (void *)&seminfo;
		maxid = semctl(0, 0, SEM_INFO, arg);
		if (maxid < 0)
			errx(EXIT_FAILURE,
			     _("kernel not configured for semaphores"));
		for (id = 0; id <= maxid; id++) {
			arg.buf = (struct semid_ds *)&semary;
			rm_me = semctl(id, 0, SEM_STAT, arg);
			if (rm_me < 0)
				continue;
			ret |= remove_id(SEM, 0, rm_me);
		}
	}
	if (type == PSEM || type == ALL) {
		if (posix_ipc_sem_get_info(NULL, &semds) > 0) {
			for (semdsp = semds; semdsp->next != NULL; semdsp = semdsp->next) {
				if (verbose)
					printf(_("removing POSIX semaphore `%s'\n"), semdsp->sname);
				ret |= remove_name(PSEM, semdsp->sname);
			}
			posix_ipc_sem_free_info(semds);
		}
	}
	if (type == MSG || type == ALL) {
		maxid =
		    msgctl(0, MSG_INFO, (struct msqid_ds *)(void *)&msginfo);
		if (maxid < 0)
			errx(EXIT_FAILURE,
			     _("kernel not configured for message queues"));
		for (id = 0; id <= maxid; id++) {
			rm_me = msgctl(id, MSG_STAT, &msgque);
			if (rm_me < 0)
				continue;
			ret |= remove_id(MSG, 0, rm_me);
		}
	}
	if (type == PMSG || type == ALL) {
		if (posix_ipc_msg_get_info(NULL, &msgds) > 0) {
			for (msgdsp = msgds; msgdsp->next != NULL; msgdsp = msgdsp->next) {
				if (verbose)
					printf(_("removing POSIX message queue `%s'\n"), msgdsp->mname);
				ret |= remove_name(PMSG, msgdsp->mname);
			}
			posix_ipc_msg_free_info(msgds);
		}
	}
	return ret;
}

int main(int argc, char **argv)
{
	int c;
	int ret = 0;
	int id = -1;
	int iskey;
	int rm_all = 0;
	type_id what_all = ALL;

	enum {
		OPT_PSHM = CHAR_MAX + 1,
		OPT_PMSG,
		OPT_PSEM
	};

	static const struct option longopts[] = {
		{"shmem-id", required_argument, NULL, 'm'},
		{"shmem-key", required_argument, NULL, 'M'},
		{"posix-shmem", required_argument, NULL, OPT_PSHM},
		{"queue-id", required_argument, NULL, 'q'},
		{"queue-key", required_argument, NULL, 'Q'},
		{"posix-mqueue", required_argument, NULL, OPT_PMSG},
		{"semaphore-id", required_argument, NULL, 's'},
		{"semaphore-key", required_argument, NULL, 'S'},
		{"posix-semaphore", required_argument, NULL, OPT_PSEM},
		{"all", optional_argument, NULL, 'a'},
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	/* if the command is executed without parameters, do nothing */
	if (argc == 1) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	/* check to see if the command is being invoked in the old way if so
	 * then remove argument list */
	if (deprecated_main(argc, argv))
		return EXIT_SUCCESS;

	/* process new syntax to conform with SYSV ipcrm */
	while((c = getopt_long(argc, argv, "q:m:s:Q:M:S:a::vhV", longopts, NULL)) != -1) {
		iskey = 0;
		switch (c) {
		case 'M':
			iskey = 1;
			id = key_to_id(SHM, optarg);
			if (id < 0) {
				ret++;
				break;
			}
			/* fallthrough */
		case 'm':
			if (!iskey)
				id = strtos32_or_err(optarg, _("failed to parse argument"));
			if (remove_id(SHM, iskey, id))
				ret++;
			break;
		case 'Q':
			iskey = 1;
			id = key_to_id(MSG, optarg);
			if (id < 0) {
				ret++;
				break;
			}
			/* fallthrough */
		case 'q':
			if (!iskey)
				id = strtos32_or_err(optarg, _("failed to parse argument"));
			if (remove_id(MSG, iskey, id))
				ret++;
			break;
		case 'S':
			iskey = 1;
			id = key_to_id(SEM, optarg);
			if (id < 0) {
				ret++;
				break;
			}
			/* fallthrough */
		case 's':
			if (!iskey)
				id = strtos32_or_err(optarg, _("failed to parse argument"));
			if (remove_id(SEM, iskey, id))
				ret++;
			break;
		case OPT_PSHM:
			if (remove_name(PSHM, optarg))
				ret++;
			break;
		case OPT_PMSG:
			if (remove_name(PMSG, optarg))
				ret++;
			break;
		case OPT_PSEM:
			if (remove_name(PSEM, optarg))
				ret++;
			break;
		case 'a':
			rm_all = 1;
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				if (!strcmp(optarg, "shm"))
					what_all = SHM;
				else if (!strcmp(optarg, "pshm"))
					what_all = PSHM;
				else if (!strcmp(optarg, "msg"))
					what_all = MSG;
				else if (!strcmp(optarg, "pmsg"))
					what_all = PMSG;
				else if (!strcmp(optarg, "sem"))
					what_all = SEM;
				else if (!strcmp(optarg, "psem"))
					what_all = PSEM;
				else
					errx(EXIT_FAILURE,
					     _("unknown argument: %s"), optarg);
			} else {
				what_all = ALL;
			}
			break;
		case 'v':
			verbose = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (rm_all && remove_all(what_all))
		ret++;

	/* print usage if we still have some arguments left over */
	if (optind < argc) {
		warnx(_("unknown argument: %s"), argv[optind]);
		errtryhelp(EXIT_FAILURE);
	}

	return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
