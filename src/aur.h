/*
 *  aur.h
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
#ifndef _PQ_AUR_H
#define _PQ_AUR_H
#include <alpm_list.h>

/*
 * AUR package
 */
typedef struct _aurpkg_t
{
	char *desc;
	char *maintainer;
	char *name;
	char *pkgbase;
	char *url;
	char *urlpath;
	char *version;

	alpm_list_t *checkdepends;
	alpm_list_t *conflicts;
	alpm_list_t *depends;
	alpm_list_t *groups;
	alpm_list_t *keywords;
	alpm_list_t *licenses;
	alpm_list_t *makedepends;
	alpm_list_t *optdepends;
	alpm_list_t *provides;
	alpm_list_t *replaces;

	unsigned int category;
	unsigned int id;
	unsigned int pkgbase_id;
	unsigned int votes;

	unsigned short outofdate;

	time_t firstsubmit;
	time_t lastmod;

	double popularity;
} aurpkg_t;

void aur_pkg_free (aurpkg_t *pkg);
aurpkg_t *aur_pkg_dup (const aurpkg_t *pkg);

const char *aur_pkg_get_name (const aurpkg_t *pkg);
unsigned int aur_pkg_get_votes (const aurpkg_t *pkg);
double aur_pkg_get_popularity (const aurpkg_t *pkg);

/*
 * AUR search function
 * Returns number of packages found
 */
int aur_info (alpm_list_t **targets);
int aur_search (alpm_list_t *targets);

/*
 * aur_get_str() get info for package
 * str returned should not be passed to free
 */
const char *aur_get_str (void *p, unsigned char c);

void aur_cleanup (void);

#endif

/* vim: set ts=4 sw=4 noet: */
