/*
 *  package-query.c
 *
 *  Copyright (c) 2010 Tuxce <tuxce.net@gmail.com>
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
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>

#include "util.h"
#include "alpm-query.h"
#include "aur.h"

#define _VERSION "0.3"

#define FORMAT_OUT "%r/%n %v [%l] (%g)\n\t%d"
#define FORMAT_OUT_USAGE "%r/%n %v [%l] (%g)\\n\\t%d"

extern char *optarg;
extern int optind;

void init_config (const char *myname)
{
	char *_myname=strdup (myname);
	config.myname = strdup(basename(_myname));
	free (_myname);
	config.quiet = 0;
	config.db_local = 0;
	config.db_sync = 0;
	config.aur = 0;
	config.query=ALL;
	config.information = 0;
	config.is_file = 0;
	config.search = 0;
	config.list = 0;
	config.list_repo = 0;
	config.list_group = 0;
	config.escape = 0;
	config.just_one = 0;
	config.filter = 0;
	config.sort = AUR_SORT;
	strcpy (config.csep, " ");
	strcpy (config.root_dir, "");
	strcpy (config.dbpath, "");
	config.custom_dbpath = 0;
	strcpy (config.config_file, "");
	strcpy (config.format_out, FORMAT_OUT);
}




void version ()
{
	printf ("%s %s\n", config.myname, _VERSION);
	exit (0);
}

void usage (unsigned short _error)
{
	fprintf(stderr, "Query alpm database and/or AUR\n");
	fprintf(stderr, "Usage: %s [options] [targets ...]\n", config.myname);
	if (_error)
	{
		fprintf(stderr, "More information: %s --help\n\n", config.myname);
		exit (1);
	}
	fprintf(stderr, "\nwhere options include:");
	fprintf(stderr, "\n\t-1 --just-one show the first answer only");
	fprintf(stderr, "\n\t-A --aur query AUR database");
	fprintf(stderr, "\n\t-b --dbpath <database path> : default %s", DBPATH);
	fprintf(stderr, "\n\t-c --config <configuration file> : default %s", CONFFILE);
	fprintf(stderr, "\n\t-x --escape escape \" on output");
	fprintf(stderr, "\n\t-f --format <format> : default %s", FORMAT_OUT_USAGE);
	fprintf(stderr, "\n\t-i --info search by name");
	fprintf(stderr, "\n\t-L --list-repo list configured repository");
	fprintf(stderr, "\n\t-l --list list repository content");
	fprintf(stderr, "\n\t-m --foreign search if foreign package exist in AUR (-AQm)");
	fprintf(stderr, "\n\t-p --file query file package");
	fprintf(stderr, "\n\t-q --quiet quiet");
	fprintf(stderr, "\n\t-Q --query search in local database");
	fprintf(stderr, "\n\t-r --root <root path> : default %s", ROOTDIR);
	fprintf(stderr, "\n\t-s --search search");
	fprintf(stderr, "\n\t-S --sync search in sync database");
	fprintf(stderr, "\n\t--sort [n,v] sort aur search by name or votes");
	fprintf(stderr, "\n\t--query-type query type");
	fprintf(stderr, "\n\t-u --upgrades list updates available");
	fprintf(stderr, "\n\t-h --help show help");
	fprintf(stderr, "\n\nquery type:");
	fprintf(stderr, "\n\tdepends: depends on one of target");
	fprintf(stderr, "\n\tconflicts: conflicts with one of target");
	fprintf(stderr, "\n\tprovides: provides one of target");
	fprintf(stderr, "\n\treplaces: replaces one of target");
	fprintf(stderr, "\n\nformat:");
	fprintf(stderr, "\n\ta: arch");
	fprintf(stderr, "\n\tb: backups");
	fprintf(stderr, "\n\td: description");
	fprintf(stderr, "\n\tc: conflicts");
	fprintf(stderr, "\n\ti: if AUR, show the ID");
	fprintf(stderr, "\n\tl: local version");
	fprintf(stderr, "\n\tn: name");
	fprintf(stderr, "\n\to: out of date (0,1)");
	fprintf(stderr, "\n\tr: repo name");
	fprintf(stderr, "\n\ts: (sync) repo name");
	fprintf(stderr, "\n\tt: target");
	fprintf(stderr, "\n\tv: version, depends on search target");
	fprintf(stderr, "\n\tw: votes from AUR");
	fprintf(stderr, "\n");
	exit (0);
}





int main (int argc, char **argv)
{
	int ret=0;
	pmdb_t *db;
	alpm_list_t *targets=NULL;
	alpm_list_t *dbs=NULL;
	alpm_list_t *t;

	init_config (argv[0]);

	int opt;
	int opt_index=0;
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
		{"file",       no_argument,       0, 'p'},
		{"quiet",      no_argument,       0, 'q'},
		{"root",       required_argument, 0, 'r'},
		{"search",     no_argument,       0, 's'},
		{"unrequired", no_argument,       0, 't'},
		{"upgrades",   no_argument,       0, 'u'},
		{"config",     required_argument, 0, 'c'},
		{"just-one",     no_argument, 0, '1'},
		{"aur",     no_argument, 0, 'A'},
		{"escape",     no_argument, 0, 'x'},
		{"format",     required_argument, 0, 'f'},
		{"list-repo",     required_argument, 0, 'L'},
		{"query-type",     required_argument, 0, 1000},
		{"csep",     required_argument, 0, 1001},
		{"sort",     required_argument, 0, 1002},
		{"version",     no_argument, 0, 'v'},

		{0, 0, 0, 0}
	};

	
//	while ((opt = getopt (argc, argv, "1Ac:b:ef:ghiLlmpQqr:Sst:uv")) != -1) 
	while ((opt = getopt_long (argc, argv, "1Ac:b:def:ghiLlmpQqr:Sstuvx", opts, &opt_index)) != -1) 
	{
		switch (opt) 
		{
			case '1':
				config.just_one = 1;
				break;
			case 'A':
				config.aur = 1;
				break;
			case 'c':
				strcpy (config.config_file, optarg);
				break;
			case 'b':
				strcpy (config.dbpath, optarg);
				config.custom_dbpath = 1;
				break;
			case 'd':
				config.filter |= F_DEPS;
				break;
			case 'e':
				config.filter |= F_EXPLICIT;
				break;
			case 'x':
				config.escape = 1;
				break;
			case 'f':
				strcpy (config.format_out, optarg);
				break;
			case 'g':
				config.list_group = 1;
				config.filter |= F_GROUP;
				break;
			case 'i':
				config.information = 1;
				break;
			case 'L':
				config.list = 1;
				break;
			case 'l':
				config.list_repo = 1;
				break;
			case 'm':
				config.filter |= F_FOREIGN;
				break;
			case 'p':
				config.is_file = 1;
				break;
			case 'Q':
				config.db_local = 1;
				break;
			case 'q':
				config.quiet = 1;
				break;
			case 'r':
				strcpy(config.root_dir, optarg);
				if (config.root_dir[strlen(config.root_dir)] != '/')
					strcat (config.root_dir, "/");
				break;
			case 's':
				config.search = 1;
				break;
			case 'S':
				config.db_sync = 1;
				break;
			case 't':
				config.filter |= F_UNREQUIRED;
				break;
			case 1000:
				if (strcmp (optarg, "depends")==0)
					config.query = DEPENDS;
				else if (strcmp (optarg, "conflicts")==0)
					config.query = CONFLICTS;
				else if (strcmp (optarg, "provides")==0)
					config.query = PROVIDES;
				else if (strcmp (optarg, "replaces")==0)
					config.query = REPLACES;
				break;
			case 1001:
				strncpy (config.csep, optarg, SEP_LEN);
				break;
			case 1002:
				config.sort = optarg[0];
				break;
			case 'u':
				config.filter |= F_UPGRADES;
				break;
			case 'v':
				version(); break;
			case 'h': usage (0); break;
			default: /* '?' */
				usage (1);
		}
	}
	if ((config.search || config.information) && !config.aur && !config.db_local && !config.db_sync)
	{
		fprintf(stderr, "search or information must have database target (-{Q,S,A}).\n");
		exit(1);
	}
	int i;
	for (i = optind; i < argc; i++)
	{
		targets = alpm_list_add(targets, strdup(argv[i]));
	}
	if (targets == NULL && config.search)
	{
		config.search = 0;
		config.list_repo = 1;
	}
	if (targets == NULL && !config.list && !config.list_repo &&
		!config.db_local && !config.list_group)
	{
		fprintf(stderr, "no targets specified.\n");
		usage(1);
	}
	if (config.list)
	{
		dbs = get_db_sync ();
		if (dbs)
		{
			for(t = dbs; t; t = alpm_list_next(t))
				printf ("%s\n", (char *)alpm_list_getdata(t));
			FREELIST (dbs);
		}
		/* TODO: some cleanup function ! */
		free (config.myname);
		exit (0);
	}
	/* TODO: release alpm before exit on failure */
	if (!init_alpm()) exit(1);
	if (!init_db_sync ()) exit(1);
	/* we can add local to dbs, so copy the list instead of just get alpm's one */
	if (config.db_sync) 
		dbs = alpm_list_copy (alpm_option_get_syncdbs());
	if (!init_db_local()) exit(1);
	if (config.is_file)
	{
		for(t = targets; t; t = alpm_list_next(t))
		{
			pmpkg_t *pkg=NULL;
			const char *filename = alpm_list_getdata(t);
			if (alpm_pkg_load (filename, 0, &pkg)!=0 || pkg==NULL)
			{
				fprintf(stderr, "unable to read %s.\n", filename);
				continue;
			}
			print_package (filename, pkg, alpm_pkg_get_str);
		}
	}
	else if (config.db_local)
		dbs = alpm_list_add(dbs, alpm_option_get_localdb());
	if  (targets || config.list_repo || config.list_group)
	{
		for(t = dbs; t && (targets || config.list_repo || config.list_group); t = alpm_list_next(t))
		{
			db = alpm_list_getdata(t);
			if (config.list_repo)
			{
				ret += list_db (db, targets);
			}
			else if (config.information)
			{
				ret += search_pkg_by_name (db, &targets, config.just_one);
			}
			else if (config.search)
			{
				ret += search_pkg (db, targets);
			}
			else if (config.list_group)
			{
				ret += list_grp (db, targets,  (targets!=NULL));
			}
			else if (config.query)
			{
				switch (config.query)
				{
					case DEPENDS: 
						ret += search_pkg_by_depends (db, targets); break;
					case CONFLICTS: 
						ret += search_pkg_by_conflicts (db, targets); break;
					case PROVIDES: 
						ret += search_pkg_by_provides (db, targets); break;
					case REPLACES: 
						ret += search_pkg_by_replaces (db, targets); break;
				}
			}
		}
	}
	if (config.aur && (targets || (config.filter & F_FOREIGN)) && !config.is_file)
	{
		if (config.filter & F_FOREIGN)
		{
			FREELIST (targets);
			targets=NULL;
			alpm_search_local (&targets);
			ret += aur_info_none (targets);
		}
		else if (config.information)
			ret += aur_info (targets);
		else if (config.search)
			ret += aur_search (targets);
	}
	else if (config.db_local && !targets && !config.just_one && !config.list_repo && !config.list_group)
	{
		ret += alpm_search_local (NULL);
	}

	/* Some cleanups */
	alpm_list_free (dbs);
	if (alpm_release()==-1)
		fprintf (stderr, alpm_strerrorlast());
	FREELIST(targets);
	free (config.myname);
	alpm_cleanup ();
	aur_cleanup ();
	/* Anything left ? */
	if (ret != 0)
		return 0;
	else
		return 1;
}

