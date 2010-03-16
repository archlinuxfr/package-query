#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <unistd.h>

#include "util.h"
#include "alpm-query.h"
#include "aur.h"

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
	config.search = 0;
	config.list = 0;
	config.list_repo = 0;
	config.escape = 0;
	config.just_one = 0;
	config.updates = 0;
	config.foreign = 0;
	strcpy (config.root_dir, ROOTDIR);
	strcpy (config.db_path, DBPATH);
	strcpy (config.config_file, CONFFILE);
	strcpy (config.format_out, FORMAT_OUT);
}


void init_path ()
{
	if (config.db_path[0] != '/')
		strins (config.db_path, config.root_dir);
	if (config.config_file[0] != '/')
		strins (config.config_file, config.root_dir);
}



void usage ()
{
	fprintf(stderr, "Query alpm database and/or AUR\n");
	fprintf(stderr, "Usage: %s [options] [targets ...]\n", config.myname);
	fprintf(stderr, "\nwhere options include:");
	fprintf(stderr, "\n\t-1 show the first answer only");
	fprintf(stderr, "\n\t-A query AUR database");
	fprintf(stderr, "\n\t-b <database path> : default %s", DBPATH);
	fprintf(stderr, "\n\t-c <configuration file> : default %s", CONFFILE);
	fprintf(stderr, "\n\t-e escape \" on output");
	fprintf(stderr, "\n\t-f <format> : default %s", FORMAT_OUT_USAGE);
	fprintf(stderr, "\n\t-i search by name");
	fprintf(stderr, "\n\t-L list configured repository");
	fprintf(stderr, "\n\t-l list repository content");
	fprintf(stderr, "\n\t-m search if foreign package exist in AUR (-AQm)");
	fprintf(stderr, "\n\t-q quiet");
	fprintf(stderr, "\n\t-Q search in local database");
	fprintf(stderr, "\n\t-r <root path> : default %s", ROOTDIR);
	fprintf(stderr, "\n\t-s search");
	fprintf(stderr, "\n\t-S search in sync database");
	fprintf(stderr, "\n\t-t query type");
	fprintf(stderr, "\n\t-u list updates available");
	fprintf(stderr, "\n\t-h this help");
	fprintf(stderr, "\n\nquery type:");
	fprintf(stderr, "\n\tdepends: depends on one of target");
	fprintf(stderr, "\n\tconflicts: conflicts with one of target");
	fprintf(stderr, "\n\tprovides: provides one of target");
	fprintf(stderr, "\n\treplaces: replaces one of target");
	fprintf(stderr, "\n\nformat:");
	fprintf(stderr, "\n\td: description");
	fprintf(stderr, "\n\ti: if AUR, show the ID");
	fprintf(stderr, "\n\tl: local version");
	fprintf(stderr, "\n\tn: name");
	fprintf(stderr, "\n\to: out of date (0,1)");
	fprintf(stderr, "\n\tr: repo name");
	fprintf(stderr, "\n\tq: query");
	fprintf(stderr, "\n\tt: target");
	fprintf(stderr, "\n\tv: version, depends on search target");
	fprintf(stderr, "\n\tw: votes from AUR");
	fprintf(stderr, "\n");
	exit (1);
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
	while ((opt = getopt (argc, argv, "1Ac:b:ef:hiLlmQqr:Sst:u")) != -1) 
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
				strcpy (config.db_path, optarg);
				break;
			case 'e':
				config.escape = 1;
				break;
			case 'f':
				strcpy (config.format_out, optarg);
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
				config.db_sync = 1;
				config.foreign = 1;
				break;
			case 'Q':
				config.db_local = 1;
				break;
			case 'q':
				config.quiet = 1;
				break;
			case 'r':
				strcpy(config.root_dir, optarg);
				break;
			case 's':
				config.search = 1;
				break;
			case 'S':
				config.db_sync = 1;
				break;
			case 't':
				if (strcmp (optarg, "depends")==0)
					config.query = DEPENDS;
				else if (strcmp (optarg, "conflicts")==0)
					config.query = CONFLICTS;
				else if (strcmp (optarg, "provides")==0)
					config.query = PROVIDES;
				else if (strcmp (optarg, "replaces")==0)
					config.query = REPLACES;
				break;
			case 'u':
				config.updates = 1;
				break;
			case 'h':
			default: /* '?' */
				usage ();
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
	if (targets == NULL && !config.list && !config.updates && !config.list_repo && !config.foreign)
	{
		fprintf(stderr, "no targets specified.\n");
		usage();
	}
	init_path ();
	if (config.list)
	{
		dbs = get_db_sync (config.config_file);
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
	if (!init_alpm (config.root_dir, config.db_path))
	{
		fprintf(stderr, "unable to initialise alpm.\n");
		exit(1);
	}
	if (!init_db_local())
	{
		fprintf(stderr, "unable to register local database.\n");
		exit(1);
	}
	if (config.db_sync || config.updates)
	{
		if (!init_db_sync (config.config_file))
		{
			fprintf(stderr, "unable to register sync database.\n");
			exit(1);
		}
		/* we can add local to dbs, so copy the list instead of just get alpm's one */
		if (!config.foreign) dbs = alpm_list_copy (alpm_option_get_syncdbs());
	}
	if (config.db_local)
		dbs = alpm_list_add(dbs, alpm_option_get_localdb());
	if (config.updates)
		search_updates ();
	else if (!config.foreign)
	{
		for(t = dbs; t && (targets || config.list_repo); t = alpm_list_next(t))
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
			else
			{
				switch (config.query)
				{
					case ALL:
					case DEPENDS: 
						ret += search_pkg_by_depends (db, targets);
						if (config.query != ALL) break;
					case CONFLICTS: 
						ret += search_pkg_by_conflicts (db, targets);
						if (config.query != ALL) break;
					case PROVIDES: 
						ret += search_pkg_by_provides (db, targets);
						if (config.query != ALL) break;
					case REPLACES: 
						ret += search_pkg_by_replaces (db, targets);
						if (config.query != ALL) break;
				}
			}
		}
	}
	if (config.aur && (targets || config.foreign))
	{
		if (config.foreign)
		{
			FREELIST (targets);
			targets = search_foreign();
			ret += aur_info_none (targets);
		}
		else if (config.information)
			ret += aur_info (targets);
		else if (config.search)
			ret += aur_search (targets);
	}

	/* Some cleanups */
	alpm_list_free (dbs);
	if (alpm_release()==-1)
		fprintf (stderr, alpm_strerrorlast());
	FREELIST(targets);
	free (config.myname);
	/* Anything left ? */
	if (ret != 0)
		return 0;
	else
		return 1;
}

