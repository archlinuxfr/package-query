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
#define FORMAT_OUT "%t (%q): %n %v"


typedef struct _aq_config 
{
	char *myname;
	unsigned short quiet;
	unsigned short db_target;
	unsigned short query;
	unsigned short information;
	unsigned short search;
	char format_out[PATH_MAX];
	char root_dir[PATH_MAX];
	char db_path[PATH_MAX];
	char config_file[PATH_MAX];
} aq_config;


pmdb_t *db;
pmdb_t *db_local;
alpm_list_t *dbs=NULL;

aq_config config;

extern char *optarg;
extern int optind;


void init (const char *myname)
{
	char *_myname=strdup (myname);
	config.myname = strdup(basename(_myname));
	free (_myname);
	config.quiet = 0;
	config.db_target = LOCAL;
	config.query=ALL;
	config.information = 0;
	config.search = 0;
	strcpy (config.root_dir, ROOTDIR);
	strcpy (config.db_path, DBPATH);
	strcpy (config.config_file, CONFFILE);
	strcpy (config.format_out, FORMAT_OUT);
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

void strins (char *dest, const char *src)
{
	char *prov;
	prov = malloc (sizeof (char) * (strlen (dest) + strlen (src) + 1));
	strcpy (prov, src);
	strcat (prov, dest);
	strcpy (dest, prov);
	free (prov);
}

char *concat_str_list (alpm_list_t *l)
{
	char *ret;
	int len=0;
	alpm_list_t *i;
	if (!l)
	{
		ret = (char *) malloc (sizeof (char) * 2);
		strcpy (ret, "-");
	}
	else
	{
		for(i = l; i; i = alpm_list_next(i)) 
		{
			len += strlen (alpm_list_getdata (i)) + 1;
		}
		if (len)
		{
			ret = (char *) malloc (sizeof (char) * len);
			strcpy (ret, "");
			for(i = l; i; i = alpm_list_next(i)) 
			{
				strcat (ret, alpm_list_getdata (i));
				strcat (ret, " ");
			}
		}
		else
		{
			ret = (char *) malloc (sizeof (char) * 2);
			strcpy (ret, "+");			
		}
	}
	strtrim (ret);
	return ret;
}


void init_path ()
{
	if (config.db_path[0] != '/')
		strins (config.db_path, config.root_dir);
	if (config.config_file[0] != '/')
		strins (config.config_file, config.root_dir);
}

void print_format (const char * target, int query, pmpkg_t * pkg_sync, pmpkg_t * pkg_local)
{
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
	char q[8] = {'d','\0', 'c', '\0', 'p', '\0', 'r', '\0'};
	char *info;
	int free_info = 0;
	format_cpy = strdup (config.format_out);
	ptr = format_cpy;
	end = &(format_cpy[strlen(format_cpy)]);
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
			free_info = 0;
			info = NULL;
			switch (c[1])
			{
				case 'd': info = (char *) alpm_pkg_get_desc (pkg_sync); break;
				case 'n': info = (char *) alpm_pkg_get_name (pkg_sync); break;
				case 'g':
					info = concat_str_list (alpm_pkg_get_groups (pkg_sync)); 
					free_info = 1;
					break;
				case 'v': info = (char *) alpm_pkg_get_version (pkg_sync); break;
				case 'l': if (pkg_local) info = (char *) alpm_pkg_get_version (pkg_local); break;
				case 'q': if (query) info = &q[(query-1)*2]; break;
				case 'r': info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg_sync)); break;
				case 't': info = (char *) target; break;
				default: ;
			}
			if (info)
				printf ("%s%s", ptr, info);
			else
				printf ("%s-", ptr);
			if (free_info)
				free (info);
		}
		ptr = &(c[2]);
	}
	if (ptr != end)
		printf ("%s", ptr);
	printf ("\n");
}

void print_package (const char * target, int query, pmpkg_t * pkg)
{
	if (config.quiet) return;
	const char *name;
	name = alpm_pkg_get_name (pkg);
	pmpkg_t *pkg_local=NULL;
	if (config.db_target != LOCAL)
	{
		pkg_local = alpm_db_get_pkg(db_local, name);
		print_format (target, query, pkg, pkg_local);
	}
	else
		print_format (target, query, pkg, pkg);
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
			
int search_pkg (alpm_list_t *targets)
{
	int ret=0;
	alpm_list_t *res, *t;

	res = alpm_db_search(db, targets);
	for(t = res; t; t = alpm_list_next(t)) 
	{
		pmpkg_t *info = alpm_list_getdata(t);
		char *ts;
		ret++;
		ts = concat_str_list (targets);
		print_package (ts, 0, info);
		free(ts);
	}
	return ret;
}

int check_pkg_name (alpm_list_t *targets)
{
	int ret=0;
	alpm_list_t *t;
	pmpkg_t *pkg_found;
	for(t = targets; t; t = alpm_list_next(t)) 
	{
		char *pkg_name = alpm_list_getdata(t);
		pkg_found = alpm_db_get_pkg (db, pkg_name);
		if (pkg_found != NULL)
		{
			ret++;
			print_package (pkg_name, 0, pkg_found);
		}
	}
	return ret;
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
				char *pkg_name = alpm_list_getdata(t);
				if (strcmp (pkg_name, compare_to) == 0)
				{
					ret++;
					print_package (pkg_name, query, info);
					found = 1;
				}
			}
		}
	}
	return ret;
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
			
	

void usage ()
{
	fprintf(stderr, "Query alpm database\n");
	fprintf(stderr, "Usage: %s [options] [targets ...]\n", config.myname);
	fprintf(stderr, "\nwhere options include:");
	fprintf(stderr, "\n\t-c <configuration file> : default %s", CONFFILE);
	fprintf(stderr, "\n\t-b <database path> : default %s", DBPATH);
	fprintf(stderr, "\n\t-f <format> : default %s", FORMAT_OUT);
	fprintf(stderr, "\n\t-i <information> show information on targets");
	fprintf(stderr, "\n\t-q quiet");
	fprintf(stderr, "\n\t-Q search in local database");
	fprintf(stderr, "\n\t-r <root path> : default %s", ROOTDIR);
	fprintf(stderr, "\n\t-s search");
	fprintf(stderr, "\n\t-S search in sync database");
	fprintf(stderr, "\n\t-t query type");
	fprintf(stderr, "\n\t-h this help");
	fprintf(stderr, "\n\ninformation type: (one information by line)");
	fprintf(stderr, "\n\tnone: display suits format");
	fprintf(stderr, "\n\tdepends: depends");
	fprintf(stderr, "\n\tconflicts: conflicts");
	fprintf(stderr, "\n\tprovides: provides");
	fprintf(stderr, "\n\treplaces: replaces");
	fprintf(stderr, "\n\nquery type:");
	fprintf(stderr, "\n\tdepends: depends on one of target");
	fprintf(stderr, "\n\tconflicts: conflicts with one of target");
	fprintf(stderr, "\n\tprovides: provides one of target");
	fprintf(stderr, "\n\treplaces: replaces one of target");
	fprintf(stderr, "\n\nformat:");
	fprintf(stderr, "\n\tl: local version");
	fprintf(stderr, "\n\tn: name");
	fprintf(stderr, "\n\tq: query");
	fprintf(stderr, "\n\tr: repo name");
	fprintf(stderr, "\n\tt: target");
	fprintf(stderr, "\n\tv: sync version");
	fprintf(stderr, "\n");
	exit (1);
}
	
int main (int argc, char **argv)
{
	int ret=0;
	alpm_list_t *targets=NULL;
	alpm_list_t *t;

	init (argv[0]);

	int opt;
	while ((opt = getopt (argc, argv, "c:b:f:hi::Qqr:Sst:")) != -1) 
	{
		switch (opt) 
		{
			case 'c':
				strcpy (config.config_file, optarg);
				break;
			case 'b':
				strcpy (config.db_path, optarg);
				break;
			case 'f':
				strcpy (config.format_out, optarg);
				break;
			case 'i':
				config.information = 1;
				break;
			case 'Q':
				config.db_target = LOCAL;
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
				config.db_target = SYNC;
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
			case 'h':
			default: /* '?' */
				usage ();
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
		usage();
	}
	init_path ();
	alpm_initialize();
	alpm_option_set_root(config.root_dir);
	alpm_option_set_dbpath(config.db_path);

	db_local = alpm_db_register_local();
	if (config.db_target != LOCAL)
		parse_config (config.config_file);
	else
		dbs = alpm_list_add(dbs, db_local);
	for(t = dbs; t; t = alpm_list_next(t))
	{
		db = alpm_list_getdata(t);
		if (config.information)
		{
			ret += check_pkg_name (targets);
		}
		else if (config.search)
		{
			ret += search_pkg (targets);
		}
		else
		{
			switch (config.query)
			{
				case ALL:
				case DEPENDS: 
					ret += check_pkg (targets, DEPENDS, alpm_pkg_get_depends, ret_depname);
					if (config.query != ALL) break;
				case CONFLICTS: 
					ret += check_pkg (targets, CONFLICTS, alpm_pkg_get_conflicts, ret_char);
					if (config.query != ALL) break;
				case PROVIDES: 
					ret += check_pkg (targets, PROVIDES, alpm_pkg_get_provides, ret_char);
					if (config.query != ALL) break;
				case REPLACES: 
					ret += check_pkg (targets, REPLACES, alpm_pkg_get_replaces, ret_char);
					if (config.query != ALL) break;
			}
		}
	}
	FREELIST(dbs);
	FREELIST(targets);
	if (ret != 0)
		return 0;
	else
		return 1;
}

