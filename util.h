/*
 *  util.h
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
#ifndef _UTIL_H
#define _UTIL_H
#include <alpm.h>
#include <alpm_list.h>
#include "aur.h"


/*
 * Query type
 */
#define ALL	0

#define DEPENDS 1
#define CONFLICTS 2
#define PROVIDES 3
#define REPLACES 4

/*
 * Search Database
 */
#define NONE 0
#define LOCAL 1
#define SYNC 2
#define AUR	3

/*
 * General config
 */
typedef struct _aq_config 
{
	char *myname;
	unsigned short quiet;
	unsigned short db_local;
	unsigned short db_sync;
	unsigned short aur;
	unsigned short query;
	unsigned short information;
	unsigned short search;
	unsigned short list;
	unsigned short list_repo;
	unsigned short list_group;
	unsigned short escape;
	unsigned short just_one;
	unsigned short updates;
	unsigned short foreign;
	unsigned short check;
	char format_out[PATH_MAX];
	char root_dir[PATH_MAX];
	char db_path[PATH_MAX];
	char config_file[PATH_MAX];
} aq_config;

aq_config config;

void init_config (const char *myname);


/*
 * String for curl usage
 */
typedef struct _string_t
{
	char *s;
} string_t;

string_t *string_new ();
string_t *string_free (string_t *dest);
string_t *string_ncat (string_t *dest, char *src, size_t n);

char *strtrim(char *str);
void strins (char *dest, const char *src);
char *strreplace(const char *str, const char *needle, const char *replace);
char *concat_str_list (alpm_list_t *l);

/* Split a dependance like string ("x{<=,>=,<,>,=}y") into:
 * name=x, mod={<=,>=,<,>,=}, ver=y
 */
void split_dep_str (const char *dep, char **name, pmdepmod_t *mod, char **ver);

/* integer to string */
char * itostr (int i);

/* escape " */
void print_escape (const char *str);

void print_package (const char * target, int query, 
	void * pkg, const char *(*f)(void *p, unsigned char c));

#endif
