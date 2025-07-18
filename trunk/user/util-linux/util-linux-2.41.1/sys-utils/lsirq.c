/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * lsirq - utility to display kernel interrupt information.
 *
 * Copyright (C) 2019 zhenwei pi <pizhenwei@bytedance.com>
 * Copyright (C) 2020 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2024 Robin Jarry <robin@jarry.cc>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <libsmartcols.h>

#include "closestream.h"
#include "optutils.h"
#include "strutils.h"
#include "xalloc.h"

#include "irq-common.h"

static int print_irq_data(struct irq_output *out,
			  int softirq, unsigned long threshold,
			  size_t setsize, cpu_set_t *cpuset)
{
	struct libscols_table *table;

	table = get_scols_table(out, NULL, NULL, softirq, threshold, setsize, cpuset);
	if (!table)
		return -1;

	scols_print_table(table);
	scols_unref_table(table);
	return 0;
}

static void __attribute__((__noreturn__)) usage(void)
{
	fputs(USAGE_HEADER, stdout);
	printf(_(" %s [options]\n"), program_invocation_short_name);
	fputs(USAGE_SEPARATOR, stdout);

	fputsln(_("Utility to display kernel interrupt information."), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(" -J, --json           use JSON output format\n"), stdout);
	fputs(_(" -P, --pairs          use key=\"value\" output format\n"), stdout);
	fputs(_(" -n, --noheadings     don't print headings\n"), stdout);
	fputs(_(" -o, --output <list>  define which output columns to use\n"), stdout);
	fputs(_(" -s, --sort <column>  specify sort column\n"), stdout);
	fputs(_(" -S, --softirq        show softirqs instead of interrupts\n"), stdout);
	fputs(_(" -t, --threshold <N>  only IRQs with counters above <N>\n"), stdout);
	fputs(_(" -C, --cpu-list <list> only show counters for these CPUs\n"), stdout);
	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(22));

	fputs(USAGE_COLUMNS, stdout);
	irq_print_columns(stdout, 1);

	fprintf(stdout, USAGE_MAN_TAIL("lsirq(1)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct irq_output out = {
		.ncolumns = 0
	};
	static const struct option longopts[] = {
		{"sort", required_argument, NULL, 's'},
		{"noheadings", no_argument, NULL, 'n'},
		{"output", required_argument, NULL, 'o'},
		{"threshold", required_argument, NULL, 't'},
		{"cpu-list", required_argument, NULL, 'C'},
		{"softirq", no_argument, NULL, 'S'},
		{"json", no_argument, NULL, 'J'},
		{"pairs", no_argument, NULL, 'P'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	int c;
	const char *outarg = NULL;
	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{'J', 'P'},
		{0}
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	uintmax_t threshold = 0;
	cpu_set_t *cpuset = NULL;
	size_t setsize = 0;
	int softirq = 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "no:s:t:C:ShJPV", longopts, NULL)) != -1) {
		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'J':
			out.json = 1;
			break;
		case 'P':
			out.pairs = 1;
			break;
		case 'n':
			out.no_headings = 1;
			break;
		case 'o':
			outarg = optarg;
			break;
		case 's':
			set_sort_func_by_name(&out, optarg);
			break;
		case 'S':
			softirq = 1;
			break;
		case 't':
			threshold = strtosize_or_err(optarg, _("error: --threshold"));
			break;
		case 'C': {
			int ncpus = get_max_number_of_cpus();
			if (ncpus <= 0)
				errx(EXIT_FAILURE, _("cannot determine NR_CPUS; aborting"));

			if (cpuset != NULL)
				cpuset_free(cpuset);

			cpuset = cpuset_alloc(ncpus, &setsize, NULL);
			if (!cpuset)
				err(EXIT_FAILURE, _("cpuset_alloc failed"));

			if (cpulist_parse(optarg, cpuset, setsize, 0))
				errx(EXIT_FAILURE, _("failed to parse CPU list: %s"),
					optarg);
			break;
		}
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	/* default */
	if (!out.ncolumns) {
		out.columns[out.ncolumns++] = COL_IRQ;
		out.columns[out.ncolumns++] = COL_TOTAL;
		out.columns[out.ncolumns++] = COL_NAME;
	}

	/* add -o [+]<list> to putput */
	if (outarg && string_add_to_idarray(outarg, out.columns,
				ARRAY_SIZE(out.columns),
				&out.ncolumns,
				irq_column_name_to_id) < 0)
		exit(EXIT_FAILURE);

	scols_init_debug(0);

	if (print_irq_data(&out, softirq, threshold, setsize, cpuset) < 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
