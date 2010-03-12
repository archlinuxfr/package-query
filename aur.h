#ifndef _AUR_H
#define _AUR_H
#include <alpm_list.h>

#define AUR_BASE_URL "http://aur.archlinux.org"


/*
 * AUR package
 */
typedef struct _package_t
{
	unsigned int id;
	char *name;
	char *version;
	unsigned int category;
	char *desc;
	unsigned int location;
	char *url;
	char *urlpath;
	char *license;
	unsigned int votes;
	unsigned short outofdate;
} package_t;

#define AUR_ID_LEN 	20

/*
 * JSON parse packages
 */
typedef struct _package_json_t
{
	package_t *pkg;
	char current_key[AUR_ID_LEN];
} package_json_t;

package_t *package_new ();
package_t *package_free (package_t *pkg);

unsigned int aur_pkg_get_id (package_t * pkg);
const char * aur_pkg_get_name (package_t * pkg);
const char * aur_pkg_get_version (package_t * pkg);
const char * aur_pkg_get_desc (package_t * pkg);
const char * aur_pkg_get_url (package_t * pkg);
const char * aur_pkg_get_urlpath (package_t * pkg);
const char * aur_pkg_get_license (package_t * pkg);
unsigned int aur_pkg_get_votes (package_t * pkg);
unsigned short aur_pkg_get_outofdate (package_t * pkg);



/*
 * AUR search function
 * Returns number of packages found
 * This function call print_aur_package()
 */
int aur_search (alpm_list_t *targets);
int aur_info (alpm_list_t *targets);

#endif
