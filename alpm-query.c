/*
 *  alpm-query.c
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
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "util.h"
#include "alpm-query.h"



	
char *ret_char (void *data)
{
	return strdup ((char *) data);
}

char *ret_depname (void *data)
{
	return alpm_dep_compute_string (data);
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
	char *(*g) (void *))
{
	int ret=0;
	alpm_list_t *t;
	alpm_list_t *i, *j;

	for(i = alpm_db_get_pkgcache(db); i; i = alpm_list_next(i)) 
	{
		pmpkg_t *pkg = alpm_list_getdata(i);
		for(j = f(pkg); j; j = alpm_list_next(j)) 
		{
			char *str = g(alpm_list_getdata(j));
			char *name, *ver;
			pmdepmod_t mod;
			split_dep_str (str, &name, &mod, &ver);
			free (str);
			for(t = targets; t; t = alpm_list_next(t)) 
			{
				char *name_c, *ver_c;
				pmdepmod_t mod_c;
				split_dep_str(alpm_list_getdata(t), &name_c, &mod_c, &ver_c);
				if ((mod_c != PM_DEP_MOD_EQ && mod_c != PM_DEP_MOD_ANY) 
					|| (mod_c == PM_DEP_MOD_EQ && ver_c == NULL))
				{
					free (name_c);
					free (ver_c);
					continue;
				}
				if (strcmp (name, name_c) == 0
					&& (mod == PM_DEP_MOD_ANY || mod_c == PM_DEP_MOD_ANY
					|| (mod == PM_DEP_MOD_EQ && alpm_pkg_vercmp (ver_c, ver) == 0)
					|| (mod == PM_DEP_MOD_GE && alpm_pkg_vercmp (ver_c, ver) >= 0)
					|| (mod == PM_DEP_MOD_GT && alpm_pkg_vercmp (ver_c, ver) > 0)
					|| (mod == PM_DEP_MOD_LE && alpm_pkg_vercmp (ver_c, ver) <= 0)
					|| (mod == PM_DEP_MOD_LT && alpm_pkg_vercmp (ver_c, ver) < 0)))
					
				{
					ret++;
					print_package (alpm_list_getdata(t), query, pkg, alpm_pkg_get_str);
				}
				free (name_c);
				free (ver_c);
			}
			free (name);
			free (ver);
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
			print_package (target, 0, pkg_found, alpm_pkg_get_str);
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


int search_pkg_by_grp (pmdb_t *db, alpm_list_t **targets, int modify, int listp)
{
	int ret=0;
	alpm_list_t *t, *targets_copy=*targets;
	pmgrp_t *grp;
	int targets_copied=0;
	for(t = *targets; t; t = alpm_list_next(t)) 
	{
		const char *grp_name = alpm_list_getdata(t);
		if (strchr (grp_name, '/'))
			continue;
		grp = alpm_db_readgrp (db, grp_name);
		if (grp)
		{
			ret++;
			if (listp)
			{
				alpm_list_t *i;
				for (i=alpm_grp_get_pkgs (grp); i; i=alpm_list_next (i))
					print_package (grp_name, 0, alpm_list_getdata(i), alpm_pkg_get_str);
			}
			else
				print_package (grp_name, 0, grp, alpm_grp_get_str);
			if (modify)
			{
				if (!targets_copied)
				{
					targets_copy = alpm_list_copy (*targets);
					targets_copied = 1;
				}
				char *data;
				targets_copy = alpm_list_remove_str (targets_copy, grp_name, &data);
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
		print_package (ts, 0, info, alpm_pkg_get_str);
		free(ts);
	}
	alpm_list_free (res);
	return ret;
}

int filter (pmpkg_t *pkg)
{
	int found=0;
	if (config.filter & F_FOREIGN)
	{
		const char *pkg_name = alpm_pkg_get_name (pkg);
		alpm_list_t *i;
		for (i = alpm_option_get_syncdbs(); i && !found; i = alpm_list_next (i))
		{
			if (alpm_db_get_pkg (alpm_list_getdata (i), pkg_name) != NULL)
				found = 1;
		}
		if (found) return 0;
	}
	if ((config.filter & F_EXPLICIT) == F_EXPLICIT && alpm_pkg_get_reason(pkg) != PM_PKG_REASON_EXPLICIT)
		return 0;
	if ((config.filter & F_DEPS) == F_DEPS && alpm_pkg_get_reason(pkg) != PM_PKG_REASON_DEPEND)
		return 0;
	if (config.filter & F_UNREQUIRED)
	{
		alpm_list_t *requiredby = alpm_pkg_compute_requiredby(pkg);
		if (requiredby)
		{
			FREELIST(requiredby);
			return 0;
		}
	}
	if (config.filter & F_UPGRADES)
		if (!alpm_sync_newversion (pkg, alpm_option_get_syncdbs()))
			return 0;
	if (config.filter & F_GROUP)
		if (!alpm_pkg_get_groups (pkg))
			return 0;
	return 1;
}

int alpm_search_local (alpm_list_t **res)
{
	alpm_list_t *i;
	int ret=0;

	for(i = alpm_db_get_pkgcache(alpm_option_get_localdb()); i; i = alpm_list_next(i)) 
	{
		if (filter (alpm_list_getdata (i)))
		{
			if (res)
				*res = alpm_list_add (*res, strdup (alpm_pkg_get_name (alpm_list_getdata (i))));
			else
				print_package ("-", 0, alpm_list_getdata (i), alpm_pkg_get_str);
			ret++;
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
			print_package (target, 0, info, alpm_pkg_get_str);
		else
			print_package ("", 0, info, alpm_pkg_get_str);
		ret++;
	}
	return ret;
}



const char *alpm_pkg_get_str (void *p, unsigned char c)
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
		case 'a': info = (char *) alpm_pkg_get_arch (pkg); break;
		case 'b': 
			info = concat_str_list (alpm_pkg_get_backup (pkg)); 
			free_info = 1;
			break;
		case 'c': 
			info = concat_str_list (alpm_pkg_get_conflicts (pkg)); 
			free_info = 1;
			break;
		case 'd': info = (char *) alpm_pkg_get_desc (pkg); break;
		case 'g':
			info = concat_str_list (alpm_pkg_get_groups (pkg)); 
			free_info = 1;
			break;
		case 'n': info = (char *) alpm_pkg_get_name (pkg); break;
		case 'r': info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg)); break;
		case 's': 
			info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg));
			if (config.db_sync && strcmp ("local", info)==0)
			{
				alpm_list_t *i;
				int found=0;
				for (i=alpm_option_get_syncdbs(); i && !found; i = alpm_list_next (i))
				{
					if (alpm_db_get_pkg (alpm_list_getdata(i), alpm_pkg_get_name (pkg)))
					{
						info = (char *) alpm_db_get_name (alpm_list_getdata(i));
						found = 1;
					}
				}
			}
			break;
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



const char *alpm_grp_get_str (void *p, unsigned char c)
{
	pmgrp_t *grp = (pmgrp_t *) p;
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
		case 'n': info = (char *) alpm_grp_get_name (grp); break;
		default: return NULL; break;
	}
	return info;
}
