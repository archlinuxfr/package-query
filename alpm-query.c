#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "util.h"
#include "alpm-query.h"



	
const char *ret_char (void *data)
{
	return (const char *) data;
}

const char *ret_depname (void *data)
{
	return alpm_dep_get_name (data);
}
		

int init_alpm (const char *root_dir, const char *db_path)
{
	if (alpm_initialize()==0)
		if (alpm_option_set_root(root_dir)==0)
			if (alpm_option_set_dbpath(db_path)==0)
				return 1;
	return 0;
}

int init_db_local ()
{
	return (alpm_db_register_local() != NULL);
}

alpm_list_t * _get_db_sync (alpm_list_t *dbs, const char * config_file)
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
				dbs = alpm_list_add (dbs, strdup (ptr));
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
					if ((dbs = _get_db_sync (dbs, ptr)) == NULL)
					{
						fclose (config);
						return NULL;
					}
				}
			}
		}
	}
	fclose (config);
	return dbs;
}

alpm_list_t * get_db_sync (const char * config_file)
{
	return _get_db_sync (NULL, config_file);
}

int init_db_sync (const char * config_file)
{
	alpm_list_t *dbs, *d;
	if ((dbs = get_db_sync (config_file)) != NULL)
	{
		for (d=dbs; d; d=alpm_list_next (d))
		{
			if (alpm_db_register_sync(alpm_list_getdata(d)) == NULL)
			{
					FREELIST (dbs);
					return 0;
			}
		}
		FREELIST (dbs);
		return 1;
	}
	return 0;
}

int _search_pkg_by (pmdb_t *db, alpm_list_t *targets, int query, 
	alpm_list_t *(*f)(pmpkg_t *), 
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

int search_pkg_by_depends (pmdb_t *db, alpm_list_t *targets)
{
	return _search_pkg_by (db, targets, DEPENDS, alpm_pkg_get_depends, ret_depname);
}

int search_pkg_by_conflicts (pmdb_t *db, alpm_list_t *targets)
{
	return _search_pkg_by (db, targets, CONFLICTS, alpm_pkg_get_conflicts, ret_char);
}

int search_pkg_by_provides (pmdb_t *db, alpm_list_t *targets)
{
	return _search_pkg_by (db, targets, PROVIDES, alpm_pkg_get_provides, ret_char);
}

int search_pkg_by_replaces (pmdb_t *db, alpm_list_t *targets)
{
	return _search_pkg_by (db, targets, REPLACES, alpm_pkg_get_replaces, ret_char);
}


int search_pkg_by_name (pmdb_t *db, alpm_list_t *targets)
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

int search_pkg (pmdb_t *db, alpm_list_t *targets)
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
	alpm_list_free (res);
	return ret;
}
