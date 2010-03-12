#include <alpm.h>
#include <alpm_list.h>

#define ROOTDIR "/"
#define DBPATH "var/lib/pacman"
#define CONFFILE "etc/pacman.conf"


/*
 * Init alpm
 */
int init_alpm (const char *root_dir, const char *db_path);
int init_db_local ();

/*
 * Parse pacman config to find sync databases
 */
int init_db_sync (const char * config_file);


/*
 * ALPM search functions
 * Returns number of packages found
 * Those functions call print_alpm_package()
 */
int search_pkg_by_depends (pmdb_t *db, alpm_list_t *targets);
int search_pkg_by_conflicts (pmdb_t *db, alpm_list_t *targets);
int search_pkg_by_provides (pmdb_t *db, alpm_list_t *targets);
int search_pkg_by_replaces (pmdb_t *db, alpm_list_t *targets);

int search_pkg_by_name (pmdb_t *db, alpm_list_t *targets);
int search_pkg (pmdb_t *db, alpm_list_t *targets);


