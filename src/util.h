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
#ifndef _PQ_UTIL_H
#define _PQ_UTIL_H
#include <limits.h>
#include <alpm.h>
#include <alpm_list.h>
#include "aur.h"


#define STRDUP(s) (s) ? strdup (s) : NULL
#define CALLOC(p, l, s) do { \
    if ((p=calloc (l, s)) == NULL) { \
      perror ("calloc"); \
      exit (1); \
    } \
  } while (0)
#define MALLOC(p, s) CALLOC (p, 1, s)
#define REALLOC(p, s) do { \
    if ((p=realloc (p, s)) == NULL) { \
      perror ("malloc"); \
      exit (1); \
    } \
  } while (0)
#define FREE(p) do { free(p); p =NULL; } while (0)



/*
 * Operations
 */
#define OP_LIST_GROUP  1
#define OP_INFO        2
#define OP_INFO_P      3
#define OP_LIST_REPO   4
#define OP_LIST_REPO_S 5
#define OP_SEARCH      6
#define OP_QUERY       7

/*
 * Query type
 */
#define ALL	0

#define OP_Q_DEPENDS   1
#define OP_Q_CONFLICTS 2
#define OP_Q_PROVIDES  3
#define OP_Q_REPLACES  4
#define OP_Q_REQUIRES  5

#define SEP_LEN 10

/*
 * Results type
 */
#define R_ALPM_PKG 1
#define R_AUR_PKG 2
 
/* Results FD */
#define FD_RES 3

/*
 * General config
 */
typedef struct _aq_config 
{
	char *myname;
	unsigned short aur;
	unsigned short aur_fetch;
	unsigned short aur_foreign;
	unsigned short aur_orphan;
	unsigned short colors;
	unsigned short custom_out;
	unsigned short db_local;
	unsigned short db_sync;
	unsigned short escape;
	unsigned short filter;
	unsigned short get_res;
	unsigned short is_file;
	unsigned short insecure;
	unsigned short just_one;
	unsigned short numbering;
	unsigned short list;
	unsigned short list_group;
	unsigned short op;
	unsigned short quiet;
	unsigned short query;
	unsigned short show_size;
	unsigned short updates;
	char *aur_url;
	char *configfile;
	char csep[SEP_LEN];
	char *dbpath;
	char format_out[PATH_MAX];
	char *rootdir;
	char sort;
} aq_config;

aq_config config;

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
 * Target passed as argument
 */

typedef void *(*ta_dup_fn)(void *);
typedef struct _target_arg_t
{
	alpm_list_t *args;
	alpm_list_t *items;
	ta_dup_fn dup_fn;
	alpm_list_fn_cmp cmp_fn;
	alpm_list_fn_free free_fn;
} target_arg_t;

target_arg_t *target_arg_init (ta_dup_fn dup_fn,
                               alpm_list_fn_cmp cmp_fn,
                               alpm_list_fn_free free_fn);
int target_arg_add (target_arg_t *t, const char *s, void *item);
target_t *target_arg_free (target_arg_t *t);
alpm_list_t *target_arg_clear (target_arg_t *t, alpm_list_t *targets);
alpm_list_t *target_arg_close (target_arg_t *t, alpm_list_t *targets);


/*
 * String helper
 */
typedef struct _string_t
{
	char *s;
	size_t size;
	size_t used;
} string_t;

string_t *string_new ();
void string_free (string_t *dest);
char *string_free2 (string_t *dest);
string_t *string_ncat (string_t *dest, const char *src, size_t n);
string_t *string_cat (string_t *dest, const char *src);
string_t *string_fcat (string_t *dest, const char *format, ...);
const char *string_cstr (string_t *str);
/* strtrim, strreplace are from pacman's code */
char *strtrim(char *str);
char *strreplace(const char *str, const char *needle, const char *replace);

char *concat_str_list (alpm_list_t *l);

/* integer/long to string */
char * itostr (int i);
char * ltostr (long i);

/*
 * Package output
 */
typedef const char *(*printpkgfn)(void *, unsigned char);
void format_str (char *s);
char *pkg_to_str (const char * target, void * pkg, printpkgfn f, const char *format);
void print_package (const char * target, void * pkg, printpkgfn f);

/* Results */
void print_or_add_result (void *pkg, unsigned short type);
void show_results ();



#endif

/* vim: set ts=4 sw=4 noet: */
