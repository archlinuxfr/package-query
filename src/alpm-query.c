/*
 *  alpm-query.c
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <glob.h>

#include "util.h"
#include "alpm-query.h"

#define ARCH_PACKAGES_URL "https://www.archlinux.org/packages/"
#define OUTOFDATE_FLAG "\"flag_date\": "

typedef const char *(*retcharfn) (void *);

/* from pacman */
static void setarch(const char *arch)
{
	if (!arch) {
		config.arch = NULL;
	} else if (strcmp (arch, "auto") == 0) {
		struct utsname un;
		uname (&un);
		config.arch = STRDUP (un.machine);
	} else {
		config.arch = strdup (arch);
	}
}

/* Init alpm */
static bool init_alpm (void)
{
	if (config.rootdir) {
		if (!config.dbpath) {
			char path[PATH_MAX];
			snprintf (path, PATH_MAX, "%s/%s", config.rootdir, DBPATH+1);
			config.dbpath = strdup (path);
		}
	} else {
		config.rootdir = strdup (ROOTDIR);
		if (!config.dbpath) {
			config.dbpath = strdup (DBPATH);
		}
	}
	enum _alpm_errno_t err;
	alpm_handle_t *handle = alpm_initialize (config.rootdir, config.dbpath, &err);
	if (!handle) {
		fprintf (stderr, "failed to initialize alpm library (%s)\n", alpm_strerror (err));
		return false;
	}
	if (!config.arch) {
		setarch ("auto");
	}
	alpm_option_set_arch (handle, config.arch);
	config.handle = handle;
	return true;
}

static bool parse_config_options (char *ptr, alpm_db_t **db, alpm_list_t **dbs, bool reg)
{
	if (reg) {
		if ((*db = alpm_register_syncdb (config.handle, ptr, ALPM_SIG_USE_DEFAULT)) == NULL) {
			fprintf (stderr, "could not register '%s' database (%s)\n", ptr,
					alpm_strerror(alpm_errno(config.handle)));
			return false;
		}
	} else {
		*dbs = alpm_list_add (*dbs, strdup (ptr));
	}
	return true;
}

static void parse_config_server (char *ptr, alpm_db_t *db)
{
	strtrim (ptr);
	const char *arch = alpm_option_get_arch (config.handle);
	char *server = strreplace (ptr, "$repo", alpm_db_get_name (db));
	if (arch) {
		char *temp = server;
		server = strreplace (temp, "$arch", arch);
		free (temp);
	}
	alpm_db_add_server (db, server);
	FREE (server);
}

static bool parse_configfile (alpm_list_t **dbs, const char *configfile, bool reg)
{
	char line[PATH_MAX+1];
	char *ptr;
	FILE *conf;
	static alpm_db_t *db = NULL;
	static int in_option = 0;
	static int global_opt_parsed = 0;
	if ((conf = fopen (configfile, "r")) == NULL) {
		fprintf (stderr, "Unable to open file: %s\n", configfile);
		return false;
	}

	while (fgets (line, PATH_MAX, conf)) {
		strtrim (line);
		if (line[0] == '\0' || line[0] == '#') {
			continue;
		}
		if ((ptr = strchr (line, '#'))) {
			*ptr = '\0';
		}
		const size_t len = strlen (line);

		if (line[0] == '[' && line[len-1] == ']') {
			db = NULL;
			line[len-1] = '\0';
			ptr = &(line[1]);
			if (strcmp (ptr, "options") != 0) {
				in_option = 0;
				if (!global_opt_parsed) {
					if (!init_alpm ()) {
						fclose (conf);
						return false;
					}
					global_opt_parsed = 1;
				}
				if (!parse_config_options (ptr, &db, dbs, reg)) {
					fclose (conf);
					return false;
				}
			} else if (!global_opt_parsed) {
				in_option = 1;
			}
			continue;
		}

		char *equal = strchr (line, '=');
		if (!equal || equal == &(line[len-1])) {
			continue;
		}

		ptr = &(equal[1]);
		*equal = '\0';
		strtrim (line);
		if (strcmp (line, "Include") == 0) {
			strtrim (ptr);
			/* Taken from pacman - logging removed */
			glob_t globbuf;
			/* Ignore include failures... assume non-critical */
			const int globret = glob (ptr, GLOB_NOCHECK, NULL, &globbuf);
			switch (globret) {
				case GLOB_NOSPACE:
				case GLOB_ABORTED:
				case GLOB_NOMATCH:
					break;
				default:
					for (size_t gindex = 0; gindex < globbuf.gl_pathc; gindex++) {
						if (!parse_configfile (dbs, globbuf.gl_pathv[gindex], reg)) {
							globfree (&globbuf);
							fclose (conf);
							return false;
						}
					}
					break;
			}
			globfree (&globbuf);
		} else if (reg && db && strcmp (line, "Server") == 0) {
			parse_config_server (ptr, db);
		} else if (reg && in_option) {
			if (strcmp (line, "Architecture") == 0) {
				strtrim (ptr);
				setarch (ptr);
			} else if (!config.dbpath && strcmp (line, "DBPath") == 0) {
				strtrim (ptr);
				FREE (config.dbpath);
				config.dbpath = STRDUP (ptr);
			}
		}
	}
	fclose (conf);
	return true;
}

alpm_list_t *get_db_sync (void)
{
	alpm_list_t *dbs = NULL;
	parse_configfile (&dbs, config.configfile, false);
	return dbs;
}

bool init_db_sync (void)
{
	return parse_configfile (NULL, config.configfile, true);
}

static alpm_pkg_t *get_sync_pkg_by_name (const char *pkgname)
{
	alpm_pkg_t *sync_pkg = NULL;
	for (const alpm_list_t *i = alpm_get_syncdbs (config.handle); i; i = alpm_list_next (i)) {
		sync_pkg = alpm_db_get_pkg (i->data, pkgname);
		if (sync_pkg) break;
	}
	return sync_pkg;
}

/* get_sync_pkg() returns the first pkg with same name in sync dbs */
static alpm_pkg_t *get_sync_pkg (alpm_pkg_t *pkg)
{
	const char *dbname = alpm_db_get_name (alpm_pkg_get_db (pkg));
	if (!dbname || strcmp ("local", dbname) == 0) {
		return get_sync_pkg_by_name (alpm_pkg_get_name (pkg));
	}
	return pkg;
}

static bool filter (alpm_pkg_t *pkg, unsigned int _filter)
{
	if ((_filter & F_FOREIGN) && get_sync_pkg (pkg))
		return false;
	if ((_filter & F_NATIVE) && !get_sync_pkg (pkg))
		return false;
	if ((_filter & F_EXPLICIT) && alpm_pkg_get_reason (pkg) != ALPM_PKG_REASON_EXPLICIT)
		return false;
	if ((_filter & F_DEPS) && alpm_pkg_get_reason (pkg) != ALPM_PKG_REASON_DEPEND)
		return false;
	if (_filter & F_UNREQUIRED) {
		alpm_list_t *requiredby = alpm_pkg_compute_requiredby (pkg);
		if (requiredby) {
			FREELIST (requiredby);
			return false;
		}
		if (!(_filter & F_UNREQUIRED_2)) {
			requiredby = alpm_pkg_compute_optionalfor (pkg);
			if (requiredby) {
				FREELIST (requiredby);
				return false;
			}
		}
	}
	if ((_filter & F_UPGRADES) && !alpm_sync_newversion (pkg, alpm_get_syncdbs(config.handle)))
		return false;
	if ((_filter & F_GROUP) && !alpm_pkg_get_groups (pkg))
		return false;
	return true;
}

static int filter_state (alpm_pkg_t *pkg)
{
	int ret = 0;
	if (filter (pkg, F_FOREIGN)) ret |= F_FOREIGN;
	if (filter (pkg, F_EXPLICIT)) ret |= F_EXPLICIT;
	if (filter (pkg, F_DEPS)) ret |= F_DEPS;
	if (filter (pkg, F_UNREQUIRED)) ret |= F_UNREQUIRED;
	if (filter (pkg, F_UPGRADES)) ret |= F_UPGRADES;
	if (filter (pkg, F_GROUP)) ret |= F_GROUP;
	if (filter (pkg, F_NATIVE)) ret |= F_NATIVE;
	return ret;
}

unsigned int search_pkg_by_type (alpm_db_t *db, alpm_list_t **targets)
{
	if (!targets) {
		return 0;
	}

	unsigned int ret = 0;
	alpm_list_t *(*f)(alpm_pkg_t *);
	int free_fn_ret = 0;
	/* free_fn_ret=1 to free f() return
	 * free_fn_ret=2 to free g() return
	 *            =3 to free both
	 */

	retcharfn g = (retcharfn) alpm_dep_compute_string;
	free_fn_ret = 2;
	switch (config.query) {
		case OP_Q_DEPENDS:   f = alpm_pkg_get_depends; break;
		case OP_Q_CONFLICTS: f = alpm_pkg_get_conflicts; break;
		case OP_Q_PROVIDES:  f = alpm_pkg_get_provides; break;
		case OP_Q_REPLACES:  f = alpm_pkg_get_replaces; break;
		case OP_Q_REQUIRES:
			f = alpm_pkg_compute_requiredby;
			g = NULL;
			free_fn_ret = 3;
			break;
		default: return 0;
	}
	target_arg_t *ta = target_arg_init (NULL, NULL, NULL);

	for (const alpm_list_t *i = alpm_db_get_pkgcache (db); i && *targets; i = alpm_list_next (i)) {
		alpm_pkg_t *pkg = i->data;
		alpm_list_t *pkg_info_list = f (pkg);
		for (const alpm_list_t *j = pkg_info_list; j && *targets; j = alpm_list_next (j)) {
			char *str = (char *) ((g) ? g(j->data) : j->data);
			target_t *t1 = target_parse (str);
			if (free_fn_ret & 2) {
				FREE (str);
			}
			for (const alpm_list_t *t = *targets; t; t = alpm_list_next (t)) {
				target_t *t2 = target_parse (t->data);
				if (t2 && t2->db && strcmp (t2->db, alpm_db_get_name (db)) != 0) {
					target_free (t2);
					continue;
				}
				if (target_compatible (t1, t2) && filter (pkg, config.filter)) {
					ret++;
					if (target_arg_add (ta, t->data, pkg)) {
						print_package (t->data, pkg, alpm_pkg_get_str);
					}
				}
				target_free (t2);
			}
			*targets = target_arg_clear (ta, *targets);
			target_free (t1);
		}
		if (free_fn_ret & 1) {
			alpm_list_free (pkg_info_list);
		}
	}
	*targets = target_arg_close (ta, *targets);
	return ret;
}

unsigned int search_pkg_by_name (alpm_db_t *db, alpm_list_t **targets)
{
	if (!targets) {
		return 0;
	}

	unsigned int ret = 0;
	const char *db_name = alpm_db_get_name (db);
	target_arg_t *ta = target_arg_init (NULL, NULL, NULL);
	for (const alpm_list_t *t = *targets; t; t = alpm_list_next (t)) {
		const char *target = t->data;
		target_t *t1 = target_parse (target);
		if (!t1) continue;
		if (t1->db && db_name && strcmp (t1->db, db_name) != 0) {
			target_free (t1);
			continue;
		}
		alpm_pkg_t *pkg_found = alpm_db_get_pkg (db, t1->name);
		if (pkg_found && filter (pkg_found, config.filter) &&
				target_check_version (t1, alpm_pkg_get_version (pkg_found))) {
			ret++;
			if (target_arg_add (ta, target, pkg_found)) {
				print_package (target, pkg_found, alpm_pkg_get_str);
			}
		}
		target_free (t1);
	}
	*targets = target_arg_close (ta, *targets);
	return ret;
}

unsigned int list_grp (alpm_db_t *db, alpm_list_t *targets)
{
	unsigned int ret = 0;
	if (targets) {
		for (const alpm_list_t *t = targets; t; t = alpm_list_next (t)) {
			const char *grp_name = t->data;
			const alpm_group_t *grp = alpm_db_get_group (db, grp_name);
			if (grp) {
				ret++;
				for (const alpm_list_t *i = grp->packages; i; i = alpm_list_next (i)) {
					print_package (grp->name, i->data, alpm_pkg_get_str);
				}
			}
		}
		return ret;
	}
	for (const alpm_list_t *t = alpm_db_get_groupcache(db); t; t = alpm_list_next(t)) {
		ret++;
		print_package ("", t->data, alpm_grp_get_str);
	}
	return ret;
}

unsigned int search_pkg (alpm_db_t *db, alpm_list_t *targets)
{
	unsigned int ret = 0;
	alpm_list_t *pkgs = alpm_db_search (db, targets);
	for (const alpm_list_t *t = pkgs; t; t = alpm_list_next (t)) {
		alpm_pkg_t *info = t->data;
		if (!filter (info, config.filter) ||
				(config.name_only &&
				!does_name_contain_targets (targets, alpm_pkg_get_name (info), true)))
			continue;
		ret++;
		print_or_add_result (info, R_ALPM_PKG);
	}
	alpm_list_free (pkgs);
	return ret;
}

unsigned int alpm_search_local (unsigned short _filter, const char *format, alpm_list_t **res)
{
	unsigned int ret = 0;
	for (const alpm_list_t *i = alpm_db_get_pkgcache (alpm_get_localdb(config.handle));
			i; i = alpm_list_next (i)) {
		alpm_pkg_t *pkg = i->data;
		if (filter (pkg, _filter)) {
			if (res) {
				*res = alpm_list_add (*res,
						pkg_to_str (NULL, pkg, alpm_pkg_get_str, (format) ? format : "%n"));
			} else {
				print_or_add_result (pkg, R_ALPM_PKG);
			}
			ret++;
		}
	}
	return ret;
}

unsigned int list_db (alpm_db_t *db, alpm_list_t *targets)
{
	const char *db_name = alpm_db_get_name (db);
	if (targets && !alpm_list_find_str (targets, db_name)) {
		return 0;
	}
	unsigned int ret = 0;
	for (const alpm_list_t *i = alpm_db_get_pkgcache (db); i; i = alpm_list_next (i)) {
		print_or_add_result (i->data, R_ALPM_PKG);
		ret++;
	}
	return ret;
}

off_t get_size_pkg (alpm_pkg_t *pkg)
{
	alpm_pkg_t *sync_pkg = get_sync_pkg (pkg);
	if (config.filter & F_UPGRADES) {
		if (sync_pkg) {
			return alpm_pkg_download_size (sync_pkg);
		}
	} else if (sync_pkg != pkg) {
		return alpm_pkg_get_isize (pkg);
	} else {
		return alpm_pkg_get_size (pkg);
	}
	return 0;
}

static off_t alpm_pkg_get_realsize (alpm_pkg_t *pkg)
{
	const alpm_filelist_t *files = alpm_pkg_get_files (pkg);
	if (!files) {
		return 0;
	}

	int ret = chdir (alpm_option_get_root (config.handle));
	if (ret != 0) {
		return 0;
	}

	off_t size = 0;
	ino_t inodes[files->count];
	memset (&inodes, 0, sizeof (inodes));
	for (size_t k = 0, j = 0; k < files->count; k++) {
		const alpm_file_t *f = files->files + k;
		if (!f) {
			continue;
		}
		struct stat buf;
		if (lstat (f->name, &buf) == -1 || !(S_ISREG (buf.st_mode) || S_ISLNK (buf.st_mode))) {
			continue;
		}
		bool found = false;
		for (size_t i = 0; i < files->count && i < j && !found; i++) {
			if (inodes[i] == buf.st_ino) {
				found = true;
				break;
			}
		}
		if (found) {
			continue;
		}
		inodes[j++] = buf.st_ino;
		size += buf.st_size;
	}
	return size;
}

static bool alpm_pkg_get_outofdate (alpm_pkg_t *pkg)
{
	CURL *curl = curl_init (CURL_GLOBAL_SSL);
	if (!curl) {
		return false;
	}

	bool flagged = false;
	char *url = NULL;
	const char *repo = alpm_db_get_name (alpm_pkg_get_db (pkg));
	const char *arch = alpm_pkg_get_arch (pkg);
	const char *name = alpm_pkg_get_name (pkg);

	if (!repo || !arch || !name) {
		return false;
	}

	/* https://www.archlinux.org/packages/$repo/$arch/$name/json/ */
	int ret = asprintf (&url, "%s%s/%s/%s/json/", ARCH_PACKAGES_URL, repo, arch, name);
	if (ret > 0) {
		char *res = curl_fetch (curl, url);
		if (res) {
			char *f = strstr (res, OUTOFDATE_FLAG);
			if (f && strncmp (f + strlen (OUTOFDATE_FLAG), "null", strlen ("null")) != 0) {
				flagged = true;
			}
			free (res);
		}
	}

	free (url);

	return flagged;
}

const char *alpm_pkg_get_str (const void *p, unsigned char c)
{
	alpm_pkg_t *pkg = (alpm_pkg_t *) p;
	static char *info = NULL;
	static bool free_info = false;
	if (free_info) {
		free (info);
		free_info = false;
	}
	info = NULL;
	switch (c) {
		case '2':
			info = ltostr (alpm_pkg_get_isize (pkg));
			free_info = true;
			break;
		case '5':
			pkg = get_sync_pkg (pkg);
			if (!pkg) break;
			info = ltostr (alpm_pkg_download_size (pkg));
			free_info = true;
			break;
		case 'a':
			info = (char *) alpm_pkg_get_arch (pkg);
			break;
		case 'b':
			info = (char *) alpm_pkg_get_base (pkg);
			break;
		case 'B':
			info = concat_backup_list (alpm_pkg_get_backup (pkg));
			free_info = true;
			break;
		case 'C':
			info = concat_dep_list (alpm_pkg_get_conflicts (pkg));
			free_info = true;
			break;
		case 'd':
			info = (char *) alpm_pkg_get_desc (pkg);
			break;
		case 'D':
			info = concat_dep_list (alpm_pkg_get_depends (pkg));
			free_info = true;
			break;
		case 'e':
			info = concat_str_list (alpm_pkg_get_licenses (pkg));
			free_info = true;
			break;
		case 'f':
			info = (char *) alpm_pkg_get_filename (pkg);
			break;
		case 'g':
			info = concat_str_list (alpm_pkg_get_groups (pkg));
			free_info = true;
			break;
		case 'I':
			info = itostr (alpm_pkg_has_scriptlet (pkg));
			free_info = true;
			break;
		case 'm':
			info = (char *) alpm_pkg_get_packager (pkg);
			break;
		case 'n':
			info = (char *) alpm_pkg_get_name (pkg);
			break;
		case 'N':
			{
				alpm_list_t *reqs = alpm_pkg_compute_requiredby (pkg);
				info = concat_str_list (reqs);
				FREELIST (reqs);
				free_info = true;
			}
			break;
		case 'o':
			pkg = get_sync_pkg (pkg);
			info = itostr (alpm_pkg_get_outofdate (pkg));
			free_info = true;
			break;
		case 'O':
			info = concat_dep_list (alpm_pkg_get_optdepends (pkg));
			free_info = true;
			break;
		case 'P':
			info = concat_dep_list (alpm_pkg_get_provides (pkg));
			free_info = true;
			break;
		case 'R':
			info = concat_dep_list (alpm_pkg_get_replaces (pkg));
			free_info = true;
			break;
		case 's':
			pkg = get_sync_pkg (pkg);
			if (!pkg) pkg = (alpm_pkg_t *) p;
		case 'r':
			info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg));
			break;
		case 'u':
			{
				const alpm_list_t *servers = alpm_db_get_servers (alpm_pkg_get_db (pkg));
				if (servers) {
					const char *dburl = servers->data;
					const char *pkgfilename = alpm_pkg_get_filename (pkg);
					if (!dburl || !pkgfilename) return NULL;
					CALLOC (info, strlen (dburl) + strlen (pkgfilename) + 2, sizeof (char));
					sprintf (info, "%s/%s", dburl, pkgfilename);
					free_info = true;
				}
			}
			break;
		case 'U':
			info = (char *) alpm_pkg_get_url (pkg);
			break;
		case 'V':
			pkg = get_sync_pkg (pkg);
			if (!pkg) break;
			// falling through
		case 'v':
			info = (char *) alpm_pkg_get_version (pkg);
			break;
		default:
			return NULL;
	}
	return info;
}

const char *alpm_local_pkg_get_str (const char *pkg_name, unsigned char c)
{
	static char *info = NULL;
	static int free_info = 0;
	if (free_info) {
		free (info);
		free_info = 0;
	}
	info = NULL;
	if (!pkg_name) {
		return NULL;
	}

	alpm_pkg_t *pkg = alpm_db_get_pkg (alpm_get_localdb (config.handle), pkg_name);
	if (!pkg) return NULL;

	switch (c) {
		case 'l':
			info = (char *) alpm_pkg_get_version (pkg);
			break;
		case 'F':
			info = concat_file_list (alpm_pkg_get_files (pkg));
			free_info = 1;
			break;
		case '1':
			info = ttostr (alpm_pkg_get_installdate (pkg));
			free_info = 1;
			break;
		case '3':
			info = ltostr (alpm_pkg_get_realsize (pkg));
			free_info = 1;
			break;
		case '4':
			info = itostr (filter_state (pkg));
			free_info = 1;
			break;
		default:
			return NULL;
	}
	return info;
}

const char *alpm_grp_get_str (const void *p, unsigned char c)
{
	const alpm_group_t *grp = (const alpm_group_t *) p;
	if (!grp) {
		return NULL;
	}

	switch (c) {
		case 'n':
			return grp->name;
		default:
			return NULL;
	}
}

void alpm_cleanup (void)
{
	alpm_pkg_get_str (NULL, 0);
	alpm_local_pkg_get_str (NULL, 0);
}

/* vim: set ts=4 sw=4 noet: */
