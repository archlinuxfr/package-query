/*
 *  alpm-query.h
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
#ifndef _PQ_ALPM_QUERY_H
#define _PQ_ALPM_QUERY_H
#include <alpm.h>
#include <alpm_list.h>


/*
 * Filter
 */
#define F_FOREIGN 1
#define F_EXPLICIT 2
#define F_DEPS	4
#define F_UNREQUIRED 8
#define F_UPGRADES 16
#define F_GROUP 32
#define F_NATIVE 64
#define F_UNREQUIRED_2 128


/*
 * Init alpm
 */
int init_alpm ();

/*
 * Parse pacman config to find sync databases
 */
/* get_db_sync() returns new list, use FREELIST() to free the list */
alpm_list_t *get_db_sync ();
/* init_db_sync()  register sync database */
int init_db_sync ();


/*
 * ALPM search functions
 * Returns number of packages found
 * Those functions call print_package()
 */
int search_pkg_by_type (alpm_db_t *db, alpm_list_t **targets, int query_type);
int search_pkg_by_name (alpm_db_t *db, alpm_list_t **targets);
int list_grp (alpm_db_t *db, alpm_list_t *targets);
int search_pkg (alpm_db_t *db, alpm_list_t *targets);
int list_db (alpm_db_t *db, alpm_list_t *targets);
int alpm_search_local (unsigned short filter, const char *format,
                       alpm_list_t **res);


off_t get_size_pkg (alpm_pkg_t *pkg);
alpm_pkg_t *get_sync_pkg_by_name (const char *pkgname);
alpm_pkg_t *get_sync_pkg (alpm_pkg_t *pkg);

/*
 * alpm_pkg_get_str() get info for package
 * alpm_local_pkg_get_str() get info for local package
 * alpm_grp_get_str() get info for group
 * str returned should not be passed to free
 */
const char *alpm_pkg_get_str (void *p, unsigned char c);
const char *alpm_local_pkg_get_str (const char *pkg_name, unsigned char c);
const char *alpm_grp_get_str (void *p, unsigned char c);


void alpm_cleanup ();

#endif

/* vim: set ts=4 sw=4 noet: */
