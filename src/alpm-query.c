/*
 *  alpm-query.c
 *
 *  Copyright (c) 2010-2011 Tuxce <tuxce.net@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "util.h"
#include "alpm-query.h"

typedef const char *(*retcharfn) (void *);

static int _alpm_option_set_root (const char *s)
{
	if (alpm_option_set_root (s)!=0)
	{
		fprintf (stderr, "problem setting rootdir '%s' (%s)\n",
			s, alpm_strerrorlast());
		return 0;
	}
	return 1;
}
static int _alpm_option_set_dbpath (const char *s)
{
	if (alpm_option_set_dbpath (s)!=0)
	{
		fprintf (stderr, "problem setting dbpath '%s' (%s)\n",
			s, alpm_strerrorlast());
		return 0;
	}
	return 1;
}


int init_alpm ()
{
	if (alpm_initialize()!=	0)
	{
		fprintf(stderr, "failed to initialize alpm library (%s)\n", 
			alpm_strerrorlast());
		return 0;
	}
	if (config.rootdir)
	{
		if (!_alpm_option_set_root (config.rootdir)) return 0;
		if (!config.dbpath)
		{
			char path[PATH_MAX];
			snprintf (path, PATH_MAX, "%s%s", alpm_option_get_root(), DBPATH+1);
			config.dbpath = strdup (path);
		}
	}
	else
		if (!_alpm_option_set_root (ROOTDIR)) return 0;
	if (config.dbpath)
		return _alpm_option_set_dbpath (config.dbpath);
	else
		return _alpm_option_set_dbpath (DBPATH);
	return 1;
}

/* from pacman */
static void setarch(const char *arch)
{
	if (strcmp(arch, "auto") == 0) {
		struct utsname un;
		uname(&un);
		alpm_option_set_arch(un.machine);
	} else {
		alpm_option_set_arch(arch);
	}
}

static int parse_configfile (alpm_list_t **dbs, const char *configfile, int reg)
{
	char line[PATH_MAX+1];
	char *ptr;
	char *equal;
	FILE *conf;
	static pmdb_t *db=NULL;
	static int in_option=0;
	if ((conf = fopen (configfile, "r")) == NULL)
	{
		fprintf(stderr, "Unable to open file: %s\n", configfile);
		return 0;
	}
	while (fgets (line, PATH_MAX, conf))
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
				in_option=0;
				if (reg)
				{
					if ((db = alpm_db_register_sync(ptr)) == NULL)
					{
						fprintf(stderr, 
							"could not register '%s' database (%s)\n", ptr,
							alpm_strerrorlast());
						fclose (conf);
						return 0;
					}
				}
				else
					*dbs = alpm_list_add (*dbs, strdup (ptr));
			}
			else
				in_option=1;
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
					if (!parse_configfile (dbs, ptr, reg))
					{
						fclose (conf);
						return 0;
					}
				}
				else if (reg && strcmp (line, "Server") == 0 && db != NULL)
				{
					strtrim (ptr);
					const char *arch = alpm_option_get_arch();
					char *server = strreplace (ptr, "$repo", alpm_db_get_name (db));
					if (arch)
					{
						char *temp=server;
						server = strreplace(temp, "$arch", arch);
						free(temp);
					}
					alpm_db_setserver(db, server);
					FREE (server);
				}
				else if (reg && in_option)
				{
					if (strcmp (line, "Architecture") == 0)
					{
						strtrim (ptr);
						setarch (ptr);
					}
					else if (!config.dbpath &&
						strcmp (line, "DBPath") == 0)
					{
						strtrim (ptr);
						if (!_alpm_option_set_dbpath (ptr))
						{
							fclose (conf);
							return 0;
						}					
					}
				}
			}
			else if (in_option)
			{
				if (strcmp (line, "ShowSize") == 0)
					config.show_size=1;
			}
		}
	}
	fclose (conf);
	return 1;
}


alpm_list_t *get_db_sync ()
{
	alpm_list_t *dbs=NULL;
	parse_configfile (&dbs, config.configfile, 0);
	return dbs;
}

int init_db_sync (const char *configfile)
{
	return parse_configfile (NULL, config.configfile, 1);
}

static int filter (pmpkg_t *pkg, unsigned int _filter)
{
	if (_filter & F_FOREIGN)
		return (get_sync_pkg (pkg) == NULL);
	if ((_filter & F_EXPLICIT) == F_EXPLICIT && alpm_pkg_get_reason(pkg) != PM_PKG_REASON_EXPLICIT)
		return 0;
	if ((_filter & F_DEPS) == F_DEPS && alpm_pkg_get_reason(pkg) != PM_PKG_REASON_DEPEND)
		return 0;
	if (_filter & F_UNREQUIRED)
	{
		alpm_list_t *requiredby = alpm_pkg_compute_requiredby(pkg);
		if (requiredby)
		{
			FREELIST(requiredby);
			return 0;
		}
	}
	if (_filter & F_UPGRADES)
		if (!alpm_sync_newversion (pkg, alpm_option_get_syncdbs()))
			return 0;
	if (_filter & F_GROUP)
		if (!alpm_pkg_get_groups (pkg))
			return 0;
	return 1;
}

static int filter_state (pmpkg_t *pkg)
{
	int ret=0;
	if (filter (pkg, F_FOREIGN)) ret |= F_FOREIGN;
	if (filter (pkg, F_EXPLICIT)) ret |= F_EXPLICIT;
	if (filter (pkg, F_DEPS)) ret |= F_DEPS;
	if (filter (pkg, F_UNREQUIRED)) ret |= F_UNREQUIRED;
	if (filter (pkg, F_UPGRADES)) ret |= F_UPGRADES;
	if (filter (pkg, F_GROUP)) ret |= F_GROUP;
	return ret;
}


int search_pkg_by_type (pmdb_t *db, alpm_list_t **targets, int query_type)
{
	int ret=0;
	alpm_list_t *t;
	alpm_list_t *i, *j;

	alpm_list_t *(*f)(pmpkg_t *);
	retcharfn g;
	int free_fn_ret=0;
	/* free_fn_ret=1 to free f() return
	 * free_fn_ret=2 to free g() return 
	 *            =3 to free both
	 */
	target_arg_t *ta=NULL;

	g = NULL;
	switch (query_type)
	{
		case OP_Q_DEPENDS:
			f = alpm_pkg_get_depends;
			g = (retcharfn) alpm_dep_compute_string;
			free_fn_ret = 2;
			break;
		case OP_Q_CONFLICTS: f = alpm_pkg_get_conflicts; break;
		case OP_Q_PROVIDES: f = alpm_pkg_get_provides; break;
		case OP_Q_REPLACES: f = alpm_pkg_get_replaces; break;
		case OP_Q_REQUIRES:
			f = alpm_pkg_compute_requiredby;
			free_fn_ret = 3;
			break;
		default: return 0;
	}
	ta = target_arg_init (NULL, NULL, NULL);


	for(i = alpm_db_get_pkgcache(db); i && *targets; i = alpm_list_next(i))
	{
		pmpkg_t *pkg = alpm_list_getdata(i);
		alpm_list_t *pkg_info_list = f(pkg);
		for(j = pkg_info_list; j && *targets; j = alpm_list_next(j))
		{
			char *str;
			str = (char *) ((g) ? g(alpm_list_getdata(j)) : alpm_list_getdata(j));
			target_t *t1 = target_parse (str);
			if (free_fn_ret & 2) FREE (str);
			for(t = *targets; t; t = alpm_list_next(t))
			{
				target_t *t2 = target_parse (alpm_list_getdata(t));
				if (t2->db && strcmp (t2->db, alpm_db_get_name (db))!=0)
				{
					target_free (t2);
					continue;
				}
				if (target_compatible (t1, t2) && filter (pkg, config.filter))
				{
					ret++;
					if (target_arg_add (ta, alpm_list_getdata(t), pkg))
						print_package (alpm_list_getdata(t), pkg, alpm_pkg_get_str);
				}
				target_free (t2);
			}
			*targets = target_arg_clear (ta, *targets);
			target_free (t1);
		}
		if (free_fn_ret & 1) alpm_list_free (pkg_info_list);
	}
	*targets = target_arg_close (ta, *targets);
	return ret;
}

int search_pkg_by_name (pmdb_t *db, alpm_list_t **targets)
{
	int ret=0;
	alpm_list_t *t;
	pmpkg_t *pkg_found;
	const char *db_name = alpm_db_get_name (db);
	target_arg_t *ta = target_arg_init (NULL, NULL, NULL);
	for(t = *targets; t; t = alpm_list_next(t))
	{
		const char *target=alpm_list_getdata(t);
		target_t *t1 = target_parse (target);
		if (t1->db && strcmp (t1->db, db_name)!=0)
		{
			target_free (t1);
			continue;
		}
		pkg_found = alpm_db_get_pkg (db, t1->name);
		if (pkg_found != NULL
			&& filter (pkg_found, config.filter)
			&& target_check_version (t1, alpm_pkg_get_version (pkg_found)))
		{
			ret++;
			if (target_arg_add (ta, target, pkg_found))
				print_package (target, pkg_found, alpm_pkg_get_str);
		}
		target_free (t1);
	}
	*targets = target_arg_close (ta, *targets);
	return ret;
}

int list_grp (pmdb_t *db, alpm_list_t *targets)
{
	int ret=0;
	pmgrp_t *grp;
	alpm_list_t *t;
	if (targets)
	{
		for (t=targets; t; t = alpm_list_next (t))
		{
			const char *grp_name = alpm_list_getdata (t);
			grp = alpm_db_readgrp (db, grp_name);
			if (grp) 
			{
				alpm_list_t *i;
				ret++;
				for (i=alpm_grp_get_pkgs (grp); i; i=alpm_list_next (i))
					print_package (alpm_grp_get_name (grp),
					    alpm_list_getdata(i), alpm_pkg_get_str);
			}
		}
	}
	else
	{
		for(t = alpm_db_get_grpcache(db); t; t = alpm_list_next(t)) 
		{
			ret++;
			print_package ("", alpm_list_getdata(t), alpm_grp_get_str);
		}
	}
	return ret;
}	

int search_pkg (pmdb_t *db, alpm_list_t *targets)
{
	int ret=0;
	alpm_list_t *t, *pkgs;
	pkgs = alpm_db_search(db, targets);
	for(t = pkgs; t; t = alpm_list_next(t))
	{
		pmpkg_t *info = alpm_list_getdata(t);
		if (!filter (info, config.filter)) continue;
		ret++;
		print_or_add_result ((void *) info, R_ALPM_PKG);
	}
	alpm_list_free (pkgs);
	return ret;
}

int alpm_search_local (unsigned short _filter, const char *format,
                       alpm_list_t **res)
{
	alpm_list_t *i;
	pmpkg_t *pkg;
	int ret=0;

	for(i = alpm_db_get_pkgcache(alpm_option_get_localdb());
	    i; i = alpm_list_next(i))
	{
		pkg = alpm_list_getdata (i);
		if (filter (pkg, _filter))
		{
			if (res)
				*res = alpm_list_add (*res, pkg_to_str (NULL, pkg,
				    (printpkgfn) alpm_pkg_get_str,
				    (format) ? format : "%n"));
			else
				print_or_add_result (pkg, R_ALPM_PKG);
			ret++;
		}
	}
	return ret;
}


int list_db (pmdb_t *db, alpm_list_t *targets)
{
	int ret=0;
	const char *db_name = alpm_db_get_name (db);
	alpm_list_t *i;

	if (targets && (alpm_list_find_str (targets, db_name))==NULL)
		return 0;
	for(i = alpm_db_get_pkgcache(db); i; i = alpm_list_next(i))
	{
		pmpkg_t *info = alpm_list_getdata(i);
		print_or_add_result ((void *) info, R_ALPM_PKG);
		ret++;
	}
	return ret;
}

pmpkg_t *get_sync_pkg_by_name (const char *pkgname)
{
	pmpkg_t *sync_pkg = NULL;
	alpm_list_t *i;
	for (i=alpm_option_get_syncdbs(); i; i = alpm_list_next (i))
	{
		sync_pkg = alpm_db_get_pkg (alpm_list_getdata(i), pkgname);
		if (sync_pkg) break;
	}
	return sync_pkg;
}

/* get_sync_pkg() returns the first pkg with same name in sync dbs */
pmpkg_t *get_sync_pkg (pmpkg_t *pkg)
{
	pmpkg_t *sync_pkg = NULL;
	const char *dbname;
	dbname = alpm_db_get_name (alpm_pkg_get_db (pkg));
	if (dbname==NULL || strcmp ("local", dbname)==0)
		sync_pkg = get_sync_pkg_by_name (alpm_pkg_get_name (pkg));
	else
		sync_pkg = pkg;
	return sync_pkg;
}

off_t get_size_pkg (pmpkg_t *pkg)
{
	pmpkg_t *sync_pkg = get_sync_pkg (pkg);
	if (config.filter & F_UPGRADES)
	{
		if (sync_pkg) 
			return alpm_pkg_download_size (sync_pkg);
	}
	else if (sync_pkg != pkg)
		return alpm_pkg_get_isize (pkg);
	else
		return alpm_pkg_get_size (pkg);
	return 0;
}

off_t alpm_pkg_get_realsize (pmpkg_t *pkg)
{
	alpm_list_t *f, *files;
	int len, i, j;
	ino_t *inodes=NULL;
	struct stat buf;
	off_t size=0;
	files = alpm_pkg_get_files (pkg);
	if (files)
	{
		if (config.rootdir)
			chdir (config.rootdir);
		else
			chdir (ROOTDIR);
		len = alpm_list_count (files);
		CALLOC (inodes, len, sizeof (ino_t));
		j=0;
		for (f = files; f; f = alpm_list_next (f))
		{
			int found=0;
			const char *filename=alpm_list_getdata (f);
			if (lstat (filename, &buf) == -1 ||
				!(S_ISREG (buf.st_mode) || S_ISLNK(buf.st_mode)))
				continue;
			for (i=0; i<len && i<j && !found; i++)
				if (inodes[i] == buf.st_ino)
				{
					found=1;
					break;
				}
			if (found) continue;
			inodes[j++]=buf.st_ino;
			size+=buf.st_size;
		}
		FREE (inodes);
	}
	return size;
}

const char *alpm_pkg_get_str (void *p, unsigned char c)
{
	pmpkg_t *pkg = (pmpkg_t *) p;
	static char *info=NULL;
	static int free_info=0;
	if (free_info)
	{
		free (info);
		free_info = 0;
	}
	info = NULL;
	switch (c)
	{
		case '2':
			info = ltostr (alpm_pkg_get_isize(pkg));
			free_info=1;
			break;
		case '5':
			pkg = get_sync_pkg (pkg);
			if (!pkg) break;
			info = ltostr (alpm_pkg_download_size(pkg));
			free_info=1;
			break;
		case 'a': info = (char *) alpm_pkg_get_arch (pkg); break;
		case 'b':
		case 'B':
			info = concat_str_list (alpm_pkg_get_backup (pkg)); 
			free_info = 1;
			break;
		case 'c':
		case 'C':
			info = concat_str_list (alpm_pkg_get_conflicts (pkg)); 
			free_info = 1;
			break;
		case 'd': info = (char *) alpm_pkg_get_desc (pkg); break;
		case 'D':
			{
				alpm_list_t *i, *deps=NULL;
				for (i=alpm_pkg_get_depends (pkg); i; i = alpm_list_next (i))
					deps = alpm_list_add (deps, alpm_dep_compute_string (alpm_list_getdata (i)));
				info = concat_str_list (deps);
				FREELIST (deps);
				free_info = 1;
			}
			break;
		case 'f': info = (char *) alpm_pkg_get_filename (pkg); break;
		case 'I':
			info = itostr (alpm_pkg_has_scriptlet (pkg));
			free_info = 1;
			break;
		case 'g':
			info = concat_str_list (alpm_pkg_get_groups (pkg)); 
			free_info = 1;
			break;
		case 'm': info = (char *) alpm_pkg_get_packager (pkg); break;
		case 'n': info = (char *) alpm_pkg_get_name (pkg); break;
		case 'N':
			{
				alpm_list_t *reqs=alpm_pkg_compute_requiredby (pkg);
				info = concat_str_list (reqs);
				FREELIST (reqs);
				free_info = 1;
			}
			break;
		case 'O':
			info = concat_str_list (alpm_pkg_get_optdepends (pkg));
			free_info = 1;
			break;
		case 'P':
			info = concat_str_list (alpm_pkg_get_provides (pkg));
			free_info = 1;
			break;
		case 'R':
			info = concat_str_list (alpm_pkg_get_replaces (pkg));
			free_info = 1;
			break;
		case 's': 
			pkg = get_sync_pkg (pkg);
			if (!pkg) pkg = (pmpkg_t *) p;
		case 'r': info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg)); break;
		case 'u': 
			{
			const char *dburl= alpm_db_get_url((alpm_pkg_get_db (pkg)));
			if (!dburl) return NULL;
			const char *pkgfilename = alpm_pkg_get_filename (pkg);
			CALLOC (info, strlen (dburl) + strlen(pkgfilename) + 2, sizeof (char));
			sprintf (info, "%s/%s", dburl, pkgfilename);
			free_info = 1;
			}
			break;
		case 'U': info = (char *) alpm_pkg_get_url (pkg); break;
		case 'V': 
			pkg = get_sync_pkg (pkg);
			if (!pkg) break;
		case 'v': info = (char *) alpm_pkg_get_version (pkg); break;
		default: return NULL; break;
	}
	return info;
}	


const char *alpm_local_pkg_get_str (const char *pkg_name, unsigned char c)
{
	pmpkg_t *pkg = NULL;
	static char *info=NULL;
	static int free_info=0;
	if (free_info)
	{
		free (info);
		free_info = 0;
	}
	info = NULL;
	if (!pkg_name) return NULL;
	pkg = alpm_db_get_pkg(alpm_option_get_localdb(), pkg_name);
	if (!pkg) return NULL;
	switch (c)
	{
		case 'l': info = (char *) alpm_pkg_get_version (pkg); break;
		case 'F':
			info = concat_str_list (alpm_pkg_get_files (pkg));
			free_info = 1;
			break;
		case '1':
			info = ttostr (alpm_pkg_get_installdate (pkg));
			free_info=1;
			break;
		case '3':
			info = ltostr (alpm_pkg_get_realsize (pkg));
			free_info=1;
			break;
		case '4': info = itostr (filter_state (pkg)); free_info=1; break;
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
		free_info = 0;
	}
	info = NULL;
	switch (c)
	{
		case 'n': info = (char *) alpm_grp_get_name (grp); break;
		default: return NULL; break;
	}
	return info;
}

void alpm_cleanup ()
{
	alpm_pkg_get_str (NULL, 0);
	alpm_grp_get_str (NULL, 0);
	alpm_local_pkg_get_str (NULL, 0);
}

/* vim: set ts=4 sw=4 noet: */
