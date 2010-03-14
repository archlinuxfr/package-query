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

int _get_db_sync (alpm_list_t **dbs, const char * config_file, int reg, int first)
{
	char line[PATH_MAX+1];
	char *ptr;
	char *equal;
	FILE *config;
	static pmdb_t *db=NULL;
	if (first)
		db = NULL;
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
				if (reg)
				{
					if ((db = alpm_db_register_sync(ptr)) == NULL)
						return 0;
				}
				else
					*dbs = alpm_list_add (*dbs, strdup (ptr));
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
					if (!_get_db_sync (dbs, ptr, reg, 0))
					{
						fclose (config);
						return 0;
					}
				}
				else if (reg && strcmp (line, "Server") == 0 && db != NULL)
				{
					strtrim (ptr);
					char *server = strreplace (ptr, "$repo", alpm_db_get_name (db));
					alpm_db_setserver(db, server);
					free (server);
				}
			}
		}
	}
	fclose (config);
	return 1;
}

alpm_list_t * get_db_sync (const char * config_file)
{
	alpm_list_t *dbs=NULL;
	_get_db_sync (&dbs, config_file, 0, 1);
	return dbs;
}

int init_db_sync (const char * config_file)
{
	return _get_db_sync (NULL, config_file, 1, 1);
}

int _search_pkg_by (pmdb_t *db, alpm_list_t *targets, int query, 
	alpm_list_t *(*f)(pmpkg_t *), 
	const char *(*g) (void *))
{
	int ret=0;
	alpm_list_t *t;
	alpm_list_t *i, *j;

	for(i = alpm_db_get_pkgcache(db); i; i = alpm_list_next(i)) 
	{
		pmpkg_t *info = alpm_list_getdata(i);
		for(j = f(info); j; j = alpm_list_next(j)) 
		{
			int len=0;
			const char *compare_to = g(alpm_list_getdata(j));
			const char *c = strchr (compare_to, '<');
			if (!c)	c = strchr (compare_to, '>');
			if (!c)	c = strchr (compare_to, '=');
			if (c)
				len = (c-compare_to) / sizeof (char);
			for(t = targets; t; t = alpm_list_next(t)) 
			{
				char *pkg_name = alpm_list_getdata(t);
				if ((len && strncmp (pkg_name, compare_to, len) == 0) ||
					(strcmp (pkg_name, compare_to) == 0))
				{
					ret++;
					print_package (pkg_name, query, info, alpm_get_str);
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


int search_pkg_by_name (pmdb_t *db, alpm_list_t **targets, int modify)
{
	int ret=0;
	alpm_list_t *t, *targets_copy=*targets;
	int targets_copied=0;
	pmpkg_t *pkg_found;
	const char *db_name = alpm_db_get_name (db);
	for(t = *targets; t; t = alpm_list_next(t)) 
	{
		const char *target = alpm_list_getdata(t);
		const char *pkg_name = target;
		const char *c = strchr (target, '/');
		if (c)
		{
			/* package name include db ("db/pkg") */
			int len = (c-target) / sizeof(char);
			if (strlen (db_name) != len || strncmp (target, db_name, len)!=0)
			{
				continue;
			}
			pkg_name = ++c;
		}
		pkg_found = alpm_db_get_pkg (db, pkg_name);
		if (pkg_found != NULL)
		{
			ret++;
			print_package (target, 0, pkg_found, alpm_get_str);
			if (modify)
			{
				if (!targets_copied)
				{
					targets_copy = alpm_list_copy (*targets);
					targets_copied = 1;
				}
				char *data;
				targets_copy = alpm_list_remove_str (targets_copy, pkg_name, &data);
				if (data)
					free (data);
			}			
		}
	}
	if (modify)
	{
		if (targets_copied)
			alpm_list_free (*targets);
		*targets=targets_copy;
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
		print_package (ts, 0, info, alpm_get_str);
		free(ts);
	}
	alpm_list_free (res);
	return ret;
}


int search_updates ()
{
	int ret=0;
	alpm_list_t *i;
	pmpkg_t *pkg;

	for(i = alpm_db_get_pkgcache(alpm_option_get_localdb()); i; i = alpm_list_next(i)) 
	{
		if ((pkg = alpm_sync_newversion(alpm_list_getdata(i), alpm_option_get_syncdbs())) != NULL)
		{
			ret++;
			print_package ("", 0, pkg, alpm_get_str);
		}
	}
	return ret;
}


int list_db (pmdb_t *db, alpm_list_t *targets)
{
	int ret=0;
	const char *db_name = alpm_db_get_name (db);
	const char *target=NULL;
	int db_present=0;
	alpm_list_t *t;
	alpm_list_t *i;

	if (targets)
	{
		for (t=targets; t; t = alpm_list_next (t))
		{
			target = alpm_list_getdata (t);
			if (strcmp (db_name, target) == 0)
			{
				db_present=1;
				break;
			}
		}
	}
	else
		db_present=1;
	if (!db_present) return 0;
	for(i = alpm_db_get_pkgcache(db); i; i = alpm_list_next(i)) 
	{
		pmpkg_t *info = alpm_list_getdata(i);
		if (target)
			print_package (target, 0, info, alpm_get_str);
		else
			print_package ("", 0, info, alpm_get_str);
		ret++;
	}
	return ret;
}


const char *alpm_get_str (void *p, unsigned char c)
{
	pmpkg_t *pkg = (pmpkg_t *) p;
	static char *info=NULL;
	static int free_info=0;
	if (free_info)
	{
		free (info);
		info = NULL;
		free_info = 0;
	}
	switch (c)
	{
		case 'd': info = (char *) alpm_pkg_get_desc (pkg); break;
		case 'g':
			info = concat_str_list (alpm_pkg_get_groups (pkg)); 
			free_info = 1;
			break;
		case 'n': info = (char *) alpm_pkg_get_name (pkg); break;
		case 'r': info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg)); break;
		case 'u': 
			{
			const char *dburl= alpm_db_get_url((alpm_pkg_get_db (pkg)));
			if (!dburl) return NULL;
			const char *pkgfilename = alpm_pkg_get_filename (pkg);
			info = (char *) malloc (sizeof (char) * (strlen (dburl) + strlen(pkgfilename) + 1));
			strcpy (info, dburl);
			strcat (info, pkgfilename);
			free_info = 1;
			}
			break;
		case 'v': info = (char *) alpm_pkg_get_version (pkg); break;
		default: return NULL; break;
	}
	return info;
}	
