#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>


#define ALL	0

#define DEPENDS 1
#define CONFLICTS 2
#define PROVIDES 3
#define REPLACES 4

#define LOCAL 1
#define SYNC 2


#define ROOTDIR "/"
#define DBPATH "var/lib/pacman"
#define CONFFILE "etc/pacman.conf"
#define FORMAT_OUT "%n %v %l"

pmdb_t *db;
pmdb_t *db_local;
alpm_list_t *dbs=NULL;
char *myname;
char format_opt[PATH_MAX] = FORMAT_OUT;
int quiet = 0;
extern char *optarg;
extern int optind;


void print_format (const char * format, const char * target, int query, pmpkg_t * pkg_sync, pmpkg_t * pkg_local)
{
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
	char q[8] = {'d','\0', 'c', '\0', 'p', '\0', 'r', '\0'};
	const char *info;
	format_cpy = strdup (format);
	ptr = format_cpy;
	end = &(format_cpy[strlen(format)]);
	while (c=strchr (ptr, '%'))
	{
		if (&(c[1])==end)
			break;
		c[0] = '\0'; 
		if (c[1] == '%' )
		{
			printf ("%s\%\%", ptr);
		}
		else
		{
			info = NULL;
			switch (c[1])
			{
				case 'n': info = alpm_pkg_get_name (pkg_sync); break;
				case 'v': info = alpm_pkg_get_version (pkg_sync); break;
				case 'l': if (pkg_local) info = alpm_pkg_get_version (pkg_local); break;
				case 'q': info = &q[query*2]; break;
				case 'r': info = alpm_db_get_name (alpm_pkg_get_db (pkg_sync)); break;
				case 't': info = target; break;
				default: ;
			}
			if (info)
				printf ("%s%s", ptr, info);
			else
				printf ("%s-", ptr);
		}
		ptr = &(c[2]);
	}
	if (ptr != end)
		printf ("%s", ptr);
	printf ("\n");
}

void print_package (const char * format, const char * target, int query, pmpkg_t * pkg, int local)
{
	if (quiet) return;
	const char *name;
	name = alpm_pkg_get_name (pkg);
	pmpkg_t *pkg_local=NULL;
	if (!local)
	{
		pkg_local = alpm_db_get_pkg(db_local, name);
		print_format (format, target, query, pkg, pkg_local);
	}
	else
		print_format (format, target, query, pkg, pkg);
}
static char *resolve_path(const char* file)
{
	char *str = NULL;

	str = calloc(PATH_MAX+1, sizeof(char));
	if(!str) {
		return(NULL);
	}

	if(!realpath(file, str)) {
		free(str);
		return(NULL);
	}

	return(str);
}

	
const char *ret_char (void *data)
{
	return (const char *) data;
}
const char *ret_depname (void *data)
{
	return alpm_dep_get_name (data);
}


int check_pkg (alpm_list_t *targets, int query, alpm_list_t *(*f)(pmpkg_t *), 
	const char *(*g) (void *))
{
	int ret=0;
	alpm_list_t *t;
	int found;
	alpm_list_t *i, *j;

	for(i = alpm_db_get_pkgcache(db); i; i = alpm_list_next(i)) 
	{
		found = 0;
		pmpkg_t *info = alpm_list_getdata(i);
		for(j = f(info); j && !found; j = alpm_list_next(j)) 
		{
			const char *compare_to = g(alpm_list_getdata(j));
			for(t = targets; t; t = alpm_list_next(t)) 
			{
				char *pkg = alpm_list_getdata(t);
				if (strcmp (pkg, compare_to) == 0)
				{
					ret++;
					print_package (format_opt, pkg, query, info, 0);
					found = 1;
				}
			}
		}
	}
	return ret;
}

char *strtrim(char *str)
{
	char *pch = str;

	if(str == NULL || *str == '\0') {
		/* string is empty, so we're done. */
		return(str);
	}

	while(isspace(*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}

	/* check if there wasn't anything but whitespace in the string. */
	if(*str == '\0') {
		return(str);
	}

	pch = (str + (strlen(str) - 1));
	while(isspace(*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}


int parse_config (const char * config_file)
{
	char line[PATH_MAX+1];
	char *ptr;
	char *equal;
	FILE *config;
	if ((config = fopen (config_file, "r")) == NULL)
	{
		fprintf(stderr, "Unable to open file: %s\n", config_file);
		exit(2);
	}
	while (fgets (line, PATH_MAX, config))
	{
		strtrim (line);
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if((ptr = strchr(line, '#'))) {
			*ptr = '\0';
		}

		if (line[0] == '[' && line[strlen(line)-1] == ']')
		{
			line[strlen(line)-1] = '\0';
			ptr = &(line[1]);
			if (strcmp (ptr, "options") != 0)
			{
				dbs = alpm_list_add(dbs, alpm_db_register_sync(ptr));
			}
		}
		else
		{
			equal = strchr (line, '=');
			if (equal != NULL && equal != &(line[strlen (line)-1]))
			{
				ptr = &(equal[1]);
				*equal = '\0';
				strtrim (line);
				if (strcmp (line, "Include") == 0)
				{
					strtrim (ptr);
					parse_config (ptr);
				}
			}
		}
	}
	fclose (config);
	return 1;
}
			
	

void usage (const char * prog)
{
	fprintf(stderr, "Query alpm database\n");
	fprintf(stderr, "Usage: %s [options] [targets ...]\n", prog);
	fprintf(stderr, "\nwhere options include:");
	fprintf(stderr, "\n\t-c <configuration file> : default %s", CONFFILE);
	fprintf(stderr, "\n\t-b <database path> : default %s", DBPATH);
	fprintf(stderr, "\n\t-f <format> : default %s", FORMAT_OUT);
	fprintf(stderr, "\n\t-l search in local database");
	fprintf(stderr, "\n\t-r <root path> : default %s", ROOTDIR);
	fprintf(stderr, "\n\t-s <repos separated by comma> search in sync database");
	fprintf(stderr, "\n\t-t query_type");
	fprintf(stderr, "\n\t-h this help");
	fprintf(stderr, "\n\nquery type:");
	fprintf(stderr, "\n\tdepends: depends on target");
	fprintf(stderr, "\n\tconflicts: conflicts with target");
	fprintf(stderr, "\n\tprovides: provide target");
	fprintf(stderr, "\n\treplaces: replace target");
	fprintf(stderr, "\n\nformat:");
	fprintf(stderr, "\n\tl: local version");
	fprintf(stderr, "\n\tn: name");
	fprintf(stderr, "\n\tq: query");
	fprintf(stderr, "\n\tr: repo name");
	fprintf(stderr, "\n\tt: target");
	fprintf(stderr, "\n\tv: sync version");
	fprintf(stderr, "\n");
	free (myname);
	exit (1);
}
	
int main (int argc, char **argv)
{
	int ret=0;
	alpm_list_t *targets=NULL;
	alpm_list_t *t;
	char root_dir[PATH_MAX], 
		db_path[PATH_MAX]="",
		config_file[PATH_MAX]="";

	myname = strdup(argv[0]);
	strcpy (root_dir, ROOTDIR);

	int opt;
	int db_target=LOCAL;
	int query=ALL;
	//char *
	while ((opt = getopt (argc, argv, "c:b:f:hlqr:st:")) != -1) 
	{
		switch (opt) 
		{
			case 'c':
				strcpy (config_file, optarg);
				break;
			case 'b':
				strcpy (db_path, optarg);
				break;
			case 'f':
				strcpy (format_opt, optarg);
				break;
			case 'l':
				db_target = LOCAL;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'r':
				strcpy (root_dir, optarg);
				break;
			case 's':
				db_target = SYNC;
				break;
			case 't':
				if (strcmp (optarg, "depends")==0)
					query = DEPENDS;
				else if (strcmp (optarg, "conflicts")==0)
					query = CONFLICTS;
				else if (strcmp (optarg, "provides")==0)
					query = PROVIDES;
				else if (strcmp (optarg, "replaces")==0)
					query = REPLACES;
				break;
			case 'h':
			default: /* '?' */
				usage (basename(myname));
		}
	}
	int i;
	for (i = optind; i < argc; i++)
	{
		targets = alpm_list_add(targets, strdup(argv[i]));
	}
	if (targets == NULL)
	{
		fprintf(stderr, "no targets specified.\n");
		usage(basename(myname));
	}
	alpm_initialize();
	alpm_option_set_root(root_dir);
	if (db_path[0] == '\0')
	{
		strcpy (db_path, root_dir);
		strcat (db_path, DBPATH);
	}
	if (config_file[0] == '\0')
	{
		strcpy (config_file, root_dir);
		strcat (config_file, CONFFILE);
	}
	alpm_option_set_dbpath(db_path);

	db_local = alpm_db_register_local();
	if (db_target != LOCAL)
		parse_config (config_file);
	else
		dbs = alpm_list_add(dbs, db_local);
	for(t = dbs; t; t = alpm_list_next(t))
	{
		db = alpm_list_getdata(t);
		switch (query)
		{
			case ALL:
			case DEPENDS: 
				ret += check_pkg (targets, DEPENDS, alpm_pkg_get_depends, ret_depname);
				if (query != ALL) break;
			case CONFLICTS: 
				ret += check_pkg (targets, CONFLICTS, alpm_pkg_get_conflicts, ret_char);
				if (query != ALL) break;
			case PROVIDES: 
				ret += check_pkg (targets, PROVIDES, alpm_pkg_get_provides, ret_char);
				if (query != ALL) break;
			case REPLACES: 
				ret += check_pkg (targets, REPLACES, alpm_pkg_get_replaces, ret_char);
				if (query != ALL) break;
		}
	}
	FREELIST(dbs);
	FREELIST(targets);
	if (ret != 0)
		return 0;
	else
		return 1;
}

