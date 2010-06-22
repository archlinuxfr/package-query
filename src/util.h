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
#include <limits.h>
#include <alpm.h>
#include <alpm_list.h>
#include "aur.h"

/*
 * Operations
 */
#define OP_LIST_GROUP  1
#define OP_INFO        2
#define OP_LIST_REPO   3
#define OP_LIST_REPO_S 4
#define OP_SEARCH      5
#define OP_QUERY       6

/*
 * Query type
 */
#define ALL	0

#define OP_Q_DEPENDS   1
#define OP_Q_CONFLICTS 2
#define OP_Q_PROVIDES  3
#define OP_Q_REPLACES  4

#define SEP_LEN 10

/*
 * Results type
 */
#define R_ALPM_PKG 1
#define R_AUR_PKG 2
 

/*
 * General config
 */
typedef struct _aq_config 
{
	char *myname;
	unsigned short op;
	unsigned short quiet;
	unsigned short db_local;
	unsigned short db_sync;
	unsigned short aur;
	unsigned short query;
	unsigned short is_file;
	unsigned short list;
	unsigned short list_group;
	unsigned short escape;
	unsigned short just_one;
	unsigned short updates;
	unsigned short filter;
	unsigned short yaourt;
	unsigned short yaourt_n;
	unsigned short y_aur_foreign;
	char sort;
	char csep[SEP_LEN];
	char format_out[PATH_MAX];
	char root_dir[PATH_MAX];
	char dbpath[PATH_MAX];
	unsigned short custom_dbpath;
	char config_file[PATH_MAX];
	unsigned short colors;
} aq_config;

aq_config config;

void init_config (const char *myname);


/* Results */
typedef struct _results_t
{
	void *ele;
	unsigned short type;
} results_t;

void print_or_add_result (void *pkg, unsigned short type);
void show_results ();

/*
 * Target type
 */
typedef struct _target_t
{
	char *db;
	char *name;
	pmdepmod_t mod;
	char *ver;
} target_t;

/* Split a target search like "db/name{<=,>=,<,>,=}ver" into
 * a target_t.
 */
target_t *target_parse (const char *str);

target_t *target_free (target_t *t);
int target_check_version (target_t *t, const char *ver);
int target_compatible (target_t *t1, target_t *t2);

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
typedef const char * (*pkg_get_version)(void *);
int check_version (void *pkg, pmdepmod_t mod, const char *ver,
	pkg_get_version f);

/* integer/long to string */
char * itostr (int i);
char * ltostr (long i);

/* escape " */
void print_escape (const char *str);

void print_package (const char * target, 
	void * pkg, const char *(*f)(void *p, unsigned char c));

#endif

/* vim: set ts=4 sw=4 noet: */
