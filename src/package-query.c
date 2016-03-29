/*
 *  package-query.c
 *
 *  Copyright (c) 2010-2012 Tuxce <tuxce.net@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#if defined(GIT_VERSION)
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <locale.h>
#include <signal.h>

#include "util.h"
#include "color.h"
#include "alpm-query.h"
#include "aur.h"

#define N_DB     1
#define N_TARGET 2

#define SETQUERY(x) do { \
if (config.op) break; \
config.op = OP_QUERY; config.query = x; \
need |= N_TARGET | N_DB; } while (0)

extern char *optarg;
extern int optind;

static alpm_list_t *targets = NULL;

static void cleanup (int ret)
{
	if (config.handle && alpm_release (config.handle) == -1) {
		fprintf(stderr, "error releasing alpm library\n");
	}
	FREELIST (targets);
	FREE (config.arch);
	FREE (config.aur_url);
	FREE (config.configfile);
	FREE (config.format_out);
	FREE (config.dbpath);
	FREE (config.rootdir);
	alpm_cleanup ();
	aur_cleanup ();
	color_cleanup ();
	exit (ret);
}

static void init_config (const char *myname)
{
	memset (&config, 0, sizeof (aq_config));
	config.myname = mbasename (myname);
	config.colors = isatty(1) ? true : false;
	config.query = OP_Q_ALL;
	config.aur_url = strdup (AUR_BASE_URL);
	config.configfile = strndup (CONFFILE, PATH_MAX);
	strcpy (config.delimiter, " ");
}

static void version (void)
{
	printf ("%s %s\n", config.myname, PACKAGE_VERSION);
	cleanup (0);
}

static void usage (unsigned short _error)
{
	fprintf(stderr, "Query alpm database and/or AUR\n");
	fprintf(stderr, "Usage: %s [options] [targets ...]\n", config.myname);
	if (_error) {
		fprintf(stderr, "More information: %s --help\n\n", config.myname);
		cleanup (_error);
	}
	fprintf(stderr, "\nOptions:");
	fprintf(stderr, "\n\t-1 --just-one        show the first answer only");
	fprintf(stderr, "\n\t--delimiter          define list separator");
	fprintf(stderr, "\n\t-f --format <format>");
	fprintf(stderr, "\n\t-h --help            show this help");
	fprintf(stderr, "\n\t-q --quiet           quiet");
	fprintf(stderr, "\n\t-x --escape          escape \" on output");
	fprintf(stderr, "\n\t--nocolor            output without colors");
	fprintf(stderr, "\n\t--sort <parameter>   sort search results by a parameter");
	fprintf(stderr, "\n\t--rsort <parameter>  sort search results in reverse order");
	fprintf(stderr, "\n\t--show-size          show package size");
	fprintf(stderr, "\n\t--insecure           perform insecure ssl connection (curl)");
	fprintf(stderr, "\n");
	fprintf(stderr, "\n\t-A --aur             query AUR database");
	fprintf(stderr, "\n\t-Q --query           search in local database");
	fprintf(stderr, "\n\t-S --sync            search in sync databases");
	fprintf(stderr, "\n\t-L --list-repo       list configured repository");
	fprintf(stderr, "\n");
	fprintf(stderr, "\n\t-b --dbpath <path>   default: %s", DBPATH);
	fprintf(stderr, "\n\t-c --config <file>   default: %s", CONFFILE);
	fprintf(stderr, "\n\t-r --root <path>     default: %s", ROOTDIR);
	fprintf(stderr, "\n\nModifiers:");
	fprintf(stderr, "\n\t-i --info            search by name");
	fprintf(stderr, "\n\t--maintainer         search by maintainer (AUR only)");
	fprintf(stderr, "\n\t--pkgbase            limit info to pkgbase (AUR only)");
	fprintf(stderr, "\n\t-l --list            list repository content");
	fprintf(stderr, "\n\t-m --foreign         search for foreign packages");
	fprintf(stderr, "\n\t-n --native          search for native packages");
	fprintf(stderr, "\n\t-p --file            query a package file");
	fprintf(stderr, "\n\t--qdepends           depends on one of the targets");
	fprintf(stderr, "\n\t--qconflicts         conflicts with one of the targets");
	fprintf(stderr, "\n\t--qprovides          provides one of the targets");
	fprintf(stderr, "\n\t--qreplaces          replaces one of the targets");
	fprintf(stderr, "\n\t--qrequires          required by one of the targets");
	fprintf(stderr, "\n\t-s --search          search");
	fprintf(stderr, "\n\t--nameonly           search in package names only");
	fprintf(stderr, "\n\t-t --unrequired      search for unrequired packages (-tt for optional deps)");
	fprintf(stderr, "\n\t-u --upgrades        list updates available");
	fprintf(stderr, "\n\nFormats (man for more infos): ");
	fprintf(stderr, "\n\ta: arch");
	fprintf(stderr, "\n\td: description");
	fprintf(stderr, "\n\ti: if AUR, show the ID");
	fprintf(stderr, "\n\tl: local version");
	fprintf(stderr, "\n\tn: name");
	fprintf(stderr, "\n\to: out of date (0,1)");
	fprintf(stderr, "\n\tr: repo name");
	fprintf(stderr, "\n\ts: (sync) repo name");
	fprintf(stderr, "\n\tt: target");
	fprintf(stderr, "\n\tv: version, depends on search target");
	fprintf(stderr, "\n\tV: (sync) version");
	fprintf(stderr, "\n\tw: votes");
	fprintf(stderr, "\n\nSorting parameters: ");
	fprintf(stderr, "\n\tn, name: name");
	fprintf(stderr, "\n\tw, vote: AUR votes");
	fprintf(stderr, "\n\tp, pop:  AUR popularity");
	fprintf(stderr, "\n\tr, rel:  search relevance");
	fprintf(stderr, "\n\t1, date: install date");
	fprintf(stderr, "\n\t2, size: install size");
	fprintf(stderr, "\n");
	cleanup (0);
}

static unsigned int deal_db (alpm_db_t *db)
{
	switch (config.op) {
		case OP_LIST_REPO:
		case OP_LIST_REPO_S:
			return list_db (db, targets);
		case OP_INFO:
		case OP_INFO_P:
			return search_pkg_by_name (db, &targets);
		case OP_SEARCH:
			return search_pkg (db, targets);
		case OP_LIST_GROUP:
			return list_grp (db, targets);
		case OP_QUERY:
			return search_pkg_by_type (db, &targets);
		default:
			return 0;
	}
}

static unsigned int deal_sync_dbs (void)
{
	unsigned int ret = 0;
	for (const alpm_list_t *t = alpm_get_syncdbs (config.handle); t; t = alpm_list_next (t)) {
		ret += deal_db (t->data);
	}
	return ret;
}

int main (int argc, char **argv)
{
	unsigned int ret = 0;
	int need = 0, given = 0, db_order = 0, i;
	bool cycle_db = false;
	alpm_list_t *t;

	struct sigaction a;
	init_config (argv[0]);
	a.sa_handler = cleanup;
	sigemptyset (&a.sa_mask);
	a.sa_flags = 0;
	sigaction (SIGINT, &a, NULL);
	sigaction (SIGTERM, &a, NULL);

	int opt;
	int opt_index = 0;
	static struct option opts[] =
	{
		{"query",      no_argument,       0, 'Q'},
		{"sync",       no_argument,       0, 'S'},
		{"dbpath",     required_argument, 0, 'b'},
		{"deps",       no_argument,       0, 'd'},
		{"explicit",   no_argument,       0, 'e'},
		{"groups",     no_argument,       0, 'g'},
		{"help",       no_argument,       0, 'h'},
		{"info",       no_argument,       0, 'i'},
		{"list",       no_argument,       0, 'l'},
		{"foreign",    no_argument,       0, 'm'},
		{"native",     no_argument,       0, 'n'},
		{"file",       no_argument,       0, 'p'},
		{"quiet",      no_argument,       0, 'q'},
		{"root",       required_argument, 0, 'r'},
		{"search",     no_argument,       0, 's'},
		{"unrequired", no_argument,       0, 't'},
		{"upgrades",   no_argument,       0, 'u'},
		{"config",     required_argument, 0, 'c'},
		{"just-one",   no_argument,       0, '1'},
		{"aur",        no_argument,       0, 'A'},
		{"escape",     no_argument,       0, 'x'},
		{"format",     required_argument, 0, 'f'},
		{"list-repo",  no_argument,       0, 'L'},
		{"delimiter",  required_argument, 0, 1001},
		{"sort",       required_argument, 0, 1002},
		{"nocolor",    no_argument,       0, 1003},
		{"number",     no_argument,       0, 1004},
		{"get-res",    no_argument,       0, 1005},
		{"show-size",  no_argument,       0, 1006},
		{"aur-url",    required_argument, 0, 1007},
		{"insecure",   no_argument,       0, 1008},
		{"qdepends",   no_argument,       0, 1009},
		{"qconflicts", no_argument,       0, 1010},
		{"qprovides",  no_argument,       0, 1011},
		{"qreplaces",  no_argument,       0, 1012},
		{"qrequires",  no_argument,       0, 1013},
		{"color",      no_argument,       0, 1014},
		{"rsort",      required_argument, 0, 1015},
		{"pkgbase",    no_argument,       0, 1016},
		{"nameonly",   no_argument,       0, 1017},
		{"maintainer", no_argument,       0, 1018},
		{"version",    no_argument,       0, 'v'},

		{0, 0, 0, 0}
	};

	while ((opt = getopt_long (argc, argv, "1Ac:b:def:ghiLlmnpQqr:Sstuvx", opts, &opt_index)) != -1) {
		switch (opt) {
			case '1':
				config.just_one = true;
				break;
			case 'A':
				if (config.aur) break;
				config.aur = ++db_order;
				given |= N_DB;
				break;
			case 'b':
				FREE (config.dbpath);
				config.dbpath = strndup (optarg, PATH_MAX);
				break;
			case 'c':
				FREE (config.configfile);
				config.configfile = strndup (optarg, PATH_MAX);
				break;
			case 'd':
				config.filter |= F_DEPS;
				break;
			case 'e':
				config.filter |= F_EXPLICIT;
				break;
			case 'f':
				config.custom_out = true;
				FREE (config.format_out);
				config.format_out = strndup (optarg, PATH_MAX);
				format_str (config.format_out);
				break;
			case 'g':
				if (config.op) break;
				config.op = OP_LIST_GROUP;
				config.filter |= F_GROUP;
				cycle_db = true;
				break;
			case 'h':
				usage (0);
				break;
			case 'i':
				if (config.op) {
					if (config.op == OP_INFO) config.op = OP_INFO_P;
					break;
				}
				config.op = OP_INFO;
				need |= N_TARGET | N_DB;
				break;
			case 'L':
				config.list = true;
				break;
			case 'l':
				if (config.op) break;
				config.op = OP_LIST_REPO;
				need |= N_DB;
				cycle_db = true;
				break;
			case 'm':
				config.filter |= F_FOREIGN;
				break;
			case 'n':
				config.filter |= F_NATIVE;
				break;
			case 'p':
				config.is_file = true;
				need |= N_TARGET;
				break;
			case 'Q':
				if (config.db_local) break;
				config.db_local = ++db_order;
				given |= N_DB;
				break;
			case 'q':
				config.quiet = true;
				break;
			case 'r':
				FREE (config.rootdir);
				config.rootdir = strndup (optarg, PATH_MAX);
				break;
			case 's':
				if (config.op) break;
				config.op = OP_SEARCH;
				need |= N_DB;
				cycle_db = true;
				break;
			case 'S':
				if (config.db_sync) break;
				config.db_sync = ++db_order;
				given |= N_DB;
				break;
			case 't':
				config.filter |= (config.filter & F_UNREQUIRED) ? F_UNREQUIRED_2 : F_UNREQUIRED;
				break;
			case 'u':
				config.filter |= F_UPGRADES;
				break;
			case 'x':
				config.escape = true;
				break;
			case 'v':
				version ();
				break;
			case 1009: /* --qdepends */
				SETQUERY (OP_Q_DEPENDS);
				break;
			case 1010: /* --qconflicts */
				SETQUERY (OP_Q_CONFLICTS);
				break;
			case 1011: /* --qprovides */
				SETQUERY (OP_Q_PROVIDES);
				break;
			case 1012: /* --qreplaces */
				SETQUERY (OP_Q_REPLACES);
				break;
			case 1013: /* --qrequires */
				SETQUERY (OP_Q_REQUIRES);
				break;
			case 1001: /* --delimiter */
				strncpy (config.delimiter, optarg, SEP_LEN);
				format_str (config.delimiter);
				break;
			case 1015: /* --rsort */
				config.rsort = true;
				/* fallthrough */
			case 1002: /* --sort */
				if (strlen (optarg) > 1) {
					if (strcmp (optarg, "name") == 0)
						config.sort = S_NAME;
					else if (strcmp (optarg, "vote") == 0)
						config.sort = S_VOTE;
					else if (strcmp (optarg, "pop") == 0)
						config.sort = S_POP;
					else if (strcmp (optarg, "rel") == 0)
						config.sort = S_REL;
					else if (strcmp (optarg, "date") == 0)
						config.sort = S_IDATE;
					else if (strcmp (optarg, "size") == 0)
						config.sort = S_ISIZE;
				} else {
					config.sort = optarg[0];
				}
				break;
			case 1003: /* --nocolor */
				config.colors = false;
				break;
			case 1004: /* --number */
				config.numbering = true;
				break;
			case 1005: /* --get-res */
				if (dup2 (FD_RES, FD_RES) == FD_RES)
					config.get_res = true;
				break;
			case 1006: /* --show-size */
				config.show_size = true;
				break;
			case 1007: /* --aur-url */
				FREE (config.aur_url);
				config.aur_url = strdup (optarg);
				break;
			case 1008: /* --insecure */
				config.insecure = true;
				break;
			case 1014: /* --color */
				config.colors = true;
				break;
			case 1016: /* --pkgbase */
				config.pkgbase = true;
				break;
			case 1017: /* --nameonly */
				config.name_only = true;
				break;
			case 1018: /* --maintainer */
				config.aur_maintainer = true;
				break;
			default: /* '?' */
				usage (1);
				break;
		}
	}

	if (config.list) {
		/* -L displays respository list and exits. */
		alpm_list_t *dbs = get_db_sync ();
		if (dbs) {
			for (t = dbs; t; t = alpm_list_next (t)) {
				printf ("%s\n", (char *)t->data);
			}
			FREELIST (dbs);
		}
		cleanup (0);
	}

	if (!config.custom_out) {
		if (config.colors) {
			color_init ();
		}
#if defined(HAVE_GETTEXT) && defined(ENABLE_NLS)
		/* TODO: specific package-query locale ? */
		setlocale (LC_ALL, "");
		bindtextdomain ("yaourt", LOCALEDIR);
		textdomain ("yaourt");
#endif
	}

	if ((need & N_DB) && !(given & N_DB)) {
		fprintf (stderr, "search or information must have database target (-{Q,S,A}).\n");
		cleanup (1);
	}

	for (i = optind; i < argc; i++) {
		if (!config.just_one || !alpm_list_find_str (targets, argv[i])) {
			targets = alpm_list_add (targets, strdup (argv[i]));
		}
	}
	if (i != optind) {
		given |= N_TARGET;
	}
	if ((need & N_TARGET) && !(given & N_TARGET)) {
		fprintf (stderr, "no targets specified.\n");
		usage (1);
	}
	if (targets == NULL) {
		if (config.op == OP_SEARCH && !config.aur_maintainer) {
			config.op = OP_LIST_REPO_S;
		}
	} else if (!config.op && (given & N_DB)) {/* Show info by default */
		config.op = OP_INFO;
	}

	// init_db_sync initializes alpm after parsing [options]
	if (!init_db_sync ()) {
		cleanup (1);
	}

	if (config.is_file) {
		for (t = targets; t; t = alpm_list_next (t)) {
			alpm_pkg_t *pkg = NULL;
			const char *filename = t->data;
			if (alpm_pkg_load (config.handle, filename, 0, ALPM_SIG_USE_DEFAULT, &pkg) != 0 || pkg == NULL) {
				fprintf (stderr, "unable to read %s.\n", filename);
				continue;
			}
			print_package (filename, pkg, alpm_pkg_get_str);
			alpm_pkg_free (pkg);
			ret++;
		}
		cleanup (!ret);
	}

	if (cycle_db || targets) {
		for (i = 1; i <= db_order; i++) {
			if (config.db_sync == i) {
				ret += deal_sync_dbs ();
				if (!ret && config.op == OP_INFO_P) {
					config.op = OP_QUERY;
					config.query = OP_Q_PROVIDES;
					ret += deal_sync_dbs ();
					config.op = OP_INFO_P;
				}
			} else if (config.db_local == i) {
				alpm_db_t *localdb = alpm_get_localdb (config.handle);
				ret += deal_db (localdb);
				if (!ret && config.op == OP_INFO_P) {
					config.op = OP_QUERY;
					config.query = OP_Q_PROVIDES;
					ret += deal_db (localdb);
					config.op = OP_INFO_P;
				}
			} else if (config.aur == i) {
				if (config.op == OP_INFO || config.op == OP_INFO_P) {
					ret += aur_request (&targets, AUR_INFO);
				} else if (config.op == OP_SEARCH) {
					ret += aur_request (&targets, AUR_SEARCH);
				}
			}
		}
	} else if (!config.aur && config.db_local) {
		ret += alpm_search_local (config.filter, NULL, NULL);
	} else if (config.aur && !(given & N_TARGET)) {
		if (config.filter & F_FOREIGN) {
			/* -Am */
			config.aur_foreign = true;
			config.just_one = true;
			alpm_search_local (config.filter, "%n", &targets);
			ret += aur_request (&targets, AUR_INFO);
			if (config.db_local) {
				/* -AQm */
				ret += search_pkg_by_name (alpm_get_localdb (config.handle), &targets);
			}
		} else if (config.filter & F_UPGRADES) {
			/* -Au */
			config.aur_upgrades = true;
			if (config.db_local) {
				/* -AQu */
				ret += alpm_search_local (config.filter, NULL, NULL);
			}
			alpm_search_local (F_FOREIGN | (config.filter & ~F_UPGRADES), "%n>%v", &targets);
			ret += aur_request (&targets, AUR_INFO);
		}
	}

	if (config.sort == S_REL) {
		calculate_results_relevance (targets);
	}

	show_results ();

	/* Some cleanups */
	cleanup (!ret);
	return 0;
}

/* vim: set ts=4 sw=4 noet: */
