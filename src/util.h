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
#ifndef PQ_UTIL_H
#define PQ_UTIL_H
#include <limits.h>
#include <stdbool.h>
#include <alpm.h>
#include <alpm_list.h>

#if defined(HAVE_GETTEXT) && defined(ENABLE_NLS)
#include <libintl.h>
#define _(x) gettext(x)
#else
#define _(x) x
#endif

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
typedef enum
{
	OP_LIST_GROUP  = 1,
	OP_INFO        = 2,
	OP_INFO_P      = 3,
	OP_LIST_REPO   = 4,
	OP_LIST_REPO_S = 5,
	OP_SEARCH      = 6,
	OP_QUERY       = 7
} optype_t;

/*
 * Query type
 */
typedef enum
{
	OP_Q_ALL       = 0,
	OP_Q_DEPENDS   = 1,
	OP_Q_CONFLICTS = 2,
	OP_Q_PROVIDES  = 3,
	OP_Q_REPLACES  = 4,
	OP_Q_REQUIRES  = 5
} qtype_t;

/*
 * Results type
 */
typedef enum
{
	R_ALPM_PKG = 1,
	R_AUR_PKG  = 2
} pkgtype_t;

/* Results FD */
#define FD_RES 3

/* Sort options */
typedef enum
{
	S_NAME  = 'n',
	S_VOTE  = 'w',
	S_POP   = 'p',
	S_REL   = 'r',
	S_IDATE = '1',
	S_ISIZE = '2'
} stype_t;

#define SEP_LEN 10

/*
 * General config
 */
typedef struct _aq_config
{
	char *arch;
	char *aur_url;
	char *configfile;
	char *dbpath;
	char *format_out;
	char *rootdir;
	const char *myname;
	alpm_handle_t *handle;
	char delimiter[SEP_LEN];
	unsigned short aur;
	bool aur_foreign;
	bool aur_maintainer;
	bool aur_upgrades;
	bool colors;
	bool custom_out;
	unsigned short db_local;
	unsigned short db_sync;
	bool escape;
	unsigned short filter;
	bool get_res;
	bool insecure;
	bool is_file;
	bool just_one;
	bool list;
	bool name_only;
	bool numbering;
	optype_t op;
	bool pkgbase;
	qtype_t query;
	bool quiet;
	bool show_size;
	stype_t sort;
	bool rsort;
} aq_config;

aq_config config;

/*
 * Target type
 */
typedef struct _target_t
{
	char *orig;
	char *db;
	char *name;
	alpm_depmod_t mod;
	char *ver;
} target_t;

/* Split a target search like "db/name{<=,>=,<,>,=}ver" into
 * a target_t.
 */
target_t *target_parse (const char *str);

void target_free (target_t *t);
bool target_check_version (const target_t *t, const char *ver);
bool target_compatible (const target_t *t1, const target_t *t2);
int target_name_cmp (const target_t *t1, const char *name);

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
bool target_arg_add (target_arg_t *t, const char *s, void *item);
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

string_t *string_new (void);
void string_free (string_t *dest);
string_t *string_ncat (string_t *dest, const char *src, size_t n);
string_t *string_cat (string_t *dest, const char *src);
const char *string_cstr (const string_t *str);
/* strtrim, strreplace are from pacman's code */
char *strtrim (char *str);
char *strreplace (const char *str, const char *needle, const char *replace);

char *concat_str_list (const alpm_list_t *l);
char *concat_dep_list (const alpm_list_t *deps);
char *concat_file_list (const alpm_filelist_t *f);
char *concat_backup_list (const alpm_list_t *backups);

/* integer/long to string */
char *itostr (int i);
char *ltostr (long i);

/* time to string */
char *ttostr (time_t t);

/*
 * Package output
 */
typedef alpm_list_t *(*alpm_list_nav)(const alpm_list_t *);
typedef const char *(*printpkgfn)(void *, unsigned char);
void format_str (char *s);
char *pkg_to_str (const char *target, void *pkg, printpkgfn f, const char *format);
void print_package (const char *target, void *pkg, printpkgfn f);

/* Results */
void calculate_results_relevance (alpm_list_t *targets);
void print_or_add_result (void *pkg, pkgtype_t type);
void show_results (void);

/* Utils */
/* mbasename is from pacman's code */
const char *mbasename (const char *path);

/* Returns true if package name contains all targets; false otherwise */
/* use_regex: true for AUR search, false for pacman search */
bool does_name_contain_targets (const alpm_list_t *targets, const char *name, bool use_regex);

#endif

/* vim: set ts=4 sw=4 noet: */
