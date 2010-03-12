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
	unsigned short db_target;
	unsigned short aur;
	unsigned short query;
	unsigned short information;
	unsigned short search;
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
char *concat_str_list (alpm_list_t *l);

void print_aur_package (const char * target, package_t * pkg_sync);
void print_package (const char * target, int query, pmpkg_t * pkg_sync);


