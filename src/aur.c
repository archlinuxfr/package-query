/*
 *  aur.c
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
#include "config.h"
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#include "aur.h"
#include "alpm-query.h"
#include "util.h"

/*
 * User agent
 */
#define PQ_USERAGENT "package-query/" PACKAGE_VERSION

/*
 * AUR url
 */
#define AUR_RPC          "/rpc.php"
#define AUR_RPC_VERSION  "?v=4"
#define AUR_RPC_SEARCH   "&type=search&arg="
#define AUR_RPC_INFO     "&type=multiinfo"
#define AUR_RPC_INFO_ARG "&arg[]="
#define AUR_URL_ID       "/packages.php?setlang=en&ID="

/*
 * AUR repo name
 */
#define AUR_REPO "aur"

/*
 * AUR package information
 */
#define AUR_CAT              1
#define AUR_CHECKDEPENDS     2
#define AUR_CONFLICTS        3
#define AUR_DEPENDS          4
#define AUR_DESC             5
#define AUR_FIRST            6
#define AUR_GROUPS           7
#define AUR_ID               8
#define AUR_LAST             9
#define AUR_LICENSE          10
#define AUR_MAINTAINER       11
#define AUR_MAKEDEPENDS      12
#define AUR_NAME             13
#define AUR_NUMVOTE          14
#define AUR_OPTDEPENDS       15
#define AUR_OUT              16
#define AUR_PKGBASE          17
#define AUR_PKGBASE_ID       18
#define AUR_POPULARITY       19
#define AUR_PROVIDES         20
#define AUR_REPLACES         21
#define AUR_URL              22
#define AUR_URLPATH          23
#define AUR_VER              24
#define AUR_JSON_RESULTS_KEY 25
#define AUR_JSON_TYPE_KEY    26

#define AUR_LAST_ID AUR_JSON_TYPE_KEY

const char *aur_key_types_names[] =
{
	"",
	"CategoryID",
	"CheckDepends",
	"Conflicts",
	"Depends",
	"Description",
	"FirstSubmitted",
	"Groups",
	"ID",
	"LastModified",
	"License",
	"Maintainer",
	"MakeDepends",
	"Name",
	"NumVotes",
	"OptDepends",
	"OutOfDate",
	"PackageBase",
	"PackageBaseID",
	"Popularity",
	"Provides",
	"Replaces",
	"URL",
	"URLPath",
	"Version",
	"results",
	"type"
};

/* AUR JSON error */
#define AUR_TYPE_ERROR   "error"

/*
 * AUR REQUEST
 */
#define AUR_INFO        1
#define AUR_SEARCH      2

/*
 * AUR max argument for info query
 */
#ifndef AUR_MAX_ARG
#define AUR_MAX_ARG 20
#endif

/*
 * JSON parse packages
 */
#define AUR_ID_LEN 20
typedef struct _jsonpkg_t
{
	alpm_list_t *pkgs;
	aurpkg_t *pkg;
	int current_key;
	int error;
	char *error_msg;
	int level;
} jsonpkg_t;



aurpkg_t *aur_pkg_new ()
{
	aurpkg_t *pkg = NULL;
	MALLOC (pkg, sizeof(aurpkg_t));
	return pkg;
}

void aur_pkg_free (aurpkg_t *pkg)
{
	if (pkg == NULL)
		return;
	FREE (pkg->desc);
	FREE (pkg->maintainer);
	FREE (pkg->name);
	FREE (pkg->pkgbase);
	FREE (pkg->url);
	FREE (pkg->urlpath);
	FREE (pkg->version);

	FREELIST (pkg->checkdepends);
	FREELIST (pkg->conflicts);
	FREELIST (pkg->depends);
	FREELIST (pkg->groups);
	FREELIST (pkg->license);
	FREELIST (pkg->makedepends);
	FREELIST (pkg->optdepends);
	FREELIST (pkg->provides);
	FREELIST (pkg->replaces);

	FREE (pkg);
}

aurpkg_t *aur_pkg_dup (const aurpkg_t *pkg)
{
	if (pkg == NULL)
		return NULL;
	aurpkg_t *pkg_ret = aur_pkg_new();

	pkg_ret->category = pkg->category;
	pkg_ret->firstsubmit = pkg->firstsubmit;
	pkg_ret->id = pkg->id;
	pkg_ret->lastmod = pkg->lastmod;
	pkg_ret->outofdate = pkg->outofdate;
	pkg_ret->popularity = pkg->popularity;
	pkg_ret->votes = pkg->votes;

	pkg_ret->desc = STRDUP (pkg->desc);
	pkg_ret->maintainer = STRDUP (pkg->maintainer);
	pkg_ret->name = STRDUP (pkg->name);
	pkg_ret->url = STRDUP (pkg->url);
	pkg_ret->urlpath = STRDUP (pkg->urlpath);
	pkg_ret->version = STRDUP (pkg->version);

	pkg_ret->checkdepends = alpm_list_copy (pkg->checkdepends);
	pkg_ret->conflicts = alpm_list_copy (pkg->conflicts);
	pkg_ret->depends = alpm_list_copy (pkg->depends);
	pkg_ret->groups = alpm_list_copy (pkg->groups);
	pkg_ret->license = alpm_list_copy (pkg->license);
	pkg_ret->makedepends = alpm_list_copy (pkg->makedepends);
	pkg_ret->optdepends = alpm_list_copy (pkg->optdepends);
	pkg_ret->provides = alpm_list_copy (pkg->provides);
	pkg_ret->replaces = alpm_list_copy (pkg->replaces);

	return pkg_ret;
}

int aur_pkg_cmp (const aurpkg_t *pkg1, const aurpkg_t *pkg2)
{
	return strcmp (aur_pkg_get_name (pkg1), aur_pkg_get_name (pkg2));
}

int aur_pkg_votes_cmp (const aurpkg_t *pkg1, const aurpkg_t *pkg2)
{
	if (pkg1 && pkg2 && (pkg1->votes > pkg2->votes))
		return 1;
	if (pkg1 && pkg2 && (pkg1->votes < pkg2->votes))
		return -1;
	return 0;
}

const char *aur_pkg_get_desc (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->desc;
	return NULL;
}

const char *aur_pkg_get_maintainer (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->maintainer;
	return NULL;
}

const char *aur_pkg_get_name (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->name;
	return NULL;
}

const char *aur_pkg_get_pkgbase (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->pkgbase;
	return NULL;
}

const char *aur_pkg_get_url (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->url;
	return NULL;
}

const char *aur_pkg_get_urlpath (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->urlpath;
	return NULL;
}

const char *aur_pkg_get_version (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->version;
	return NULL;
}

const alpm_list_t *aur_pkg_get_checkdepends (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->checkdepends;
	return NULL;
}

const alpm_list_t *aur_pkg_get_conflicts (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->conflicts;
	return NULL;
}

const alpm_list_t *aur_pkg_get_depends (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->depends;
	return NULL;
}

const alpm_list_t *aur_pkg_get_groups (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->groups;
	return NULL;
}

const alpm_list_t *aur_pkg_get_licenses (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->license;
	return NULL;
}

const alpm_list_t *aur_pkg_get_makedepends (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->makedepends;
	return NULL;
}

const alpm_list_t *aur_pkg_get_optdepends (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->optdepends;
	return NULL;
}

const alpm_list_t *aur_pkg_get_provides (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->provides;
	return NULL;
}

const alpm_list_t *aur_pkg_get_replaces (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->replaces;
	return NULL;
}

unsigned int aur_pkg_get_id (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->id;
	return 0;
}

unsigned int aur_pkg_get_pkgbase_id (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->pkgbase_id;
	return 0;
}

unsigned int aur_pkg_get_votes (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->votes;
	return 0;
}

unsigned short aur_pkg_get_outofdate (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->outofdate;
	return 0;
}

time_t aur_pkg_get_firstsubmit (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->firstsubmit;
	return 0;
}

time_t aur_pkg_get_lastmod (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->lastmod;
	return 0;
}

double aur_pkg_get_popularity (const aurpkg_t *pkg)
{
	if (pkg != NULL)
		return pkg->popularity;
	return 0.0;
}

static size_t curl_getdata_cb (void *data, size_t size, size_t nmemb, void *userdata)
{
	string_t *s = (string_t *) userdata;
	string_ncat (s, data, nmemb);
	return nmemb;
}

static int json_start_map (void *ctx)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL)
		return 1;

	pkg_json->level += 1;
	if (pkg_json->level > 1) {
		aur_pkg_free (pkg_json->pkg);
		pkg_json->pkg = aur_pkg_new();
	}

	return 1;
}

static int json_end_map (void *ctx)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL)
		return 1;

	pkg_json->level -= 1;
	if (pkg_json->level == 1 && pkg_json->pkg != NULL) {
		switch (config.sort) {
			case S_VOTE:
				pkg_json->pkgs = alpm_list_add_sorted (pkg_json->pkgs,
					pkg_json->pkg, (alpm_list_fn_cmp) aur_pkg_votes_cmp);
				break;
			default:
				pkg_json->pkgs = alpm_list_add_sorted (pkg_json->pkgs,
					pkg_json->pkg, (alpm_list_fn_cmp) aur_pkg_cmp);
				break;
		}
		pkg_json->pkg = NULL;
	}
	return 1;
}

static int json_key (void *ctx, const unsigned char *stringVal, size_t stringLen)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL || stringVal == NULL)
		return 1;

	stringLen = (stringLen >= AUR_ID_LEN) ? AUR_ID_LEN - 1 : stringLen;
	for (int i = 0; i <= AUR_LAST_ID; ++i) {
		if (strlen(aur_key_types_names[i]) == stringLen &&
				strncmp(aur_key_types_names[i], (const char*)stringVal, stringLen) == 0) {
			pkg_json->current_key = i;
			break;
		}
	}

	return 1;
}

static int json_integer (void *ctx, long long val)
{
	const jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL || pkg_json->pkg == NULL)
		return 1;

	// package info in level 2
	if (pkg_json->level < 2)
		return 1;

	switch (pkg_json->current_key) {
		case AUR_CAT:
			pkg_json->pkg->category = (int) val;
			break;
		case AUR_FIRST:
			pkg_json->pkg->firstsubmit = (time_t) val;
			break;
		case AUR_ID:
			pkg_json->pkg->id = (int) val;
			break;
		case AUR_LAST:
			pkg_json->pkg->lastmod = (time_t) val;
			break;
		case AUR_NUMVOTE:
			pkg_json->pkg->votes = (int) val;
			break;
		case AUR_OUT:
			pkg_json->pkg->outofdate = 1;
			break;
		case AUR_PKGBASE_ID:
			pkg_json->pkg->pkgbase_id = (int) val;
			break;
		default:
			break;
	}

	return 1;
}

static int json_double (void *ctx, double val)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL || pkg_json->pkg == NULL)
		return 1;

	// package info in level 2
	if (pkg_json->level < 2)
		return 1;

	switch (pkg_json->current_key) {
		case AUR_POPULARITY:
			pkg_json->pkg->popularity = val;
			break;
		default:
			break;
	}

	return 1;
}

static int json_string (void *ctx, const unsigned char *stringVal, size_t stringLen)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL || stringVal == NULL)
		return 1;

	// package info in level 2
	if (pkg_json->level < 2) {
		if (pkg_json->current_key == AUR_JSON_TYPE_KEY &&
				strncmp ((const char *)stringVal, AUR_TYPE_ERROR, stringLen) == 0) {
			pkg_json->error = 1;
		}
		if (pkg_json->error && pkg_json->current_key == AUR_JSON_RESULTS_KEY) {
			FREE (pkg_json->error_msg);
			pkg_json->error_msg = strndup ((const char *)stringVal, stringLen);
		}
		return 1;
	}

	if (pkg_json->pkg == NULL)
		return 1;

	char *s = strndup ((const char *)stringVal, stringLen);
	switch (pkg_json->current_key) {
		case AUR_DESC:
			FREE (pkg_json->pkg->desc);
			pkg_json->pkg->desc = s;
			break;
		case AUR_MAINTAINER:
			FREE (pkg_json->pkg->maintainer);
			pkg_json->pkg->maintainer = s;
			break;
		case AUR_NAME:
			FREE (pkg_json->pkg->name);
			pkg_json->pkg->name = s;
			break;
		case AUR_PKGBASE:
			FREE (pkg_json->pkg->pkgbase);
			pkg_json->pkg->pkgbase = s;
			break;
		case AUR_URL:
			FREE (pkg_json->pkg->url);
			pkg_json->pkg->url = s;
			break;
		case AUR_URLPATH:
			FREE (pkg_json->pkg->urlpath);
			pkg_json->pkg->urlpath = s;
			break;
		case AUR_VER:
			FREE (pkg_json->pkg->version);
			pkg_json->pkg->version = s;
			break;
		case AUR_CHECKDEPENDS:
			pkg_json->pkg->checkdepends = alpm_list_add (pkg_json->pkg->checkdepends, s);
			break;
		case AUR_CONFLICTS:
			pkg_json->pkg->conflicts = alpm_list_add (pkg_json->pkg->conflicts, s);
			break;
		case AUR_DEPENDS:
			pkg_json->pkg->depends = alpm_list_add (pkg_json->pkg->depends, s);
			break;
		case AUR_GROUPS:
			pkg_json->pkg->groups = alpm_list_add (pkg_json->pkg->groups, s);
			break;
		case AUR_LICENSE:
			pkg_json->pkg->license = alpm_list_add (pkg_json->pkg->license, s);
			break;
		case AUR_MAKEDEPENDS:
			pkg_json->pkg->makedepends = alpm_list_add (pkg_json->pkg->makedepends, s);
			break;
		case AUR_OPTDEPENDS:
			pkg_json->pkg->optdepends = alpm_list_add (pkg_json->pkg->optdepends, s);
			break;
		case AUR_PROVIDES:
			pkg_json->pkg->provides = alpm_list_add (pkg_json->pkg->provides, s);
			break;
		case AUR_REPLACES:
			pkg_json->pkg->replaces = alpm_list_add (pkg_json->pkg->replaces, s);
			break;
		default:
			FREE (s);
			break;
	}

	return 1;
}

static yajl_callbacks callbacks = {
    NULL,
    NULL,
    json_integer,
    json_double,
    NULL,
    json_string,
    json_start_map,
    json_key,
    json_end_map,
    NULL,
    NULL,
};

static alpm_list_t *aur_json_parse (const char *s)
{
	if (s == NULL)
		return NULL;

	const size_t len = strlen (s);
	jsonpkg_t pkg_json = { NULL, NULL, 0, 0, NULL, 0};
	yajl_handle hand = yajl_alloc(&callbacks, NULL, (void *) &pkg_json);
	yajl_status stat = yajl_parse(hand, (const unsigned char *) s, len);

	if (stat == yajl_status_ok) {
		stat = yajl_complete_parse(hand);
	}
	if (stat != yajl_status_ok) {
		unsigned char * str = yajl_get_error(hand, 1, (const unsigned char *) s, len);
		fprintf(stderr, "%s\n", (const char *) str);
		yajl_free_error(hand, str);
		alpm_list_free_inner (pkg_json.pkgs, (alpm_list_fn_free) aur_pkg_free);
		alpm_list_free (pkg_json.pkgs);
		pkg_json.pkgs = NULL;
	}

	yajl_free(hand);
	if (pkg_json.error) {
		fprintf(stderr, "AUR error : %s\n", pkg_json.error_msg);
		FREE (pkg_json.error_msg);
	}

	return pkg_json.pkgs;
}

static alpm_list_t *aur_fetch (CURL *curl, const char *url)
{
	string_t *res = string_new();
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, res);
	curl_easy_setopt (curl, CURLOPT_URL, url);

	CURLcode curl_code;
	if ((curl_code = curl_easy_perform (curl)) != CURLE_OK) {
		fprintf(stderr, "curl error: %s\n", curl_easy_strerror (curl_code));
		string_free (res);
		return NULL;
	}

	long http_code;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200) {
		fprintf(stderr, "The URL %s returned error : %ld\n", url, http_code);
		string_free (res);
		return NULL;
	}

	// this setlocale() hack is a workaround for the yajl issue:
	// https://github.com/lloyd/yajl/issues/79
	setlocale(LC_ALL, "C");
	alpm_list_t *pkgs = aur_json_parse (string_cstr (res));
	setlocale(LC_ALL, "");

	string_free (res);

	return pkgs;
}

static string_t *aur_prepare_url (const char *aur_rpc_type)
{
	string_t *url = string_new();
	url = string_cat (url, config.aur_url);
	url = string_cat (url, AUR_RPC);
	url = string_cat (url, AUR_RPC_VERSION);
	url = string_cat (url, aur_rpc_type);
	return url;
}

static int aur_request_search (alpm_list_t **targets, CURL *curl)
{
	char *encoded_arg = curl_easy_escape (curl, (*targets)->data, 0);
	if (encoded_arg == NULL) {
		return 0;
	}

	int pkgs_found = 0;
	string_t *url = aur_prepare_url (AUR_RPC_SEARCH);
	url = string_cat (url, encoded_arg);
	curl_free (encoded_arg);
	alpm_list_t *pkgs = aur_fetch (curl, string_cstr (url));
	string_free (url);

	for (const alpm_list_t *p = pkgs; p; p = alpm_list_next (p)) {
		int match = 1;
		if (config.name_only) {
			if (!does_name_contain_targets (*targets, aur_pkg_get_name (p->data), 0)) {
				match = 0;
			}
		} else {
			for (const alpm_list_t *t = alpm_list_next (*targets); t; t = alpm_list_next (t)) {
				if (strcasestr (aur_pkg_get_name (p->data), t->data) == NULL &&
						(aur_pkg_get_desc (p->data) == NULL ||
						strcasestr (aur_pkg_get_desc (p->data), t->data) == NULL)) {
					match = 0;
					break;
				}
			}
		}
		if (match) {
			pkgs_found++;
			print_or_add_result (p->data, R_AUR_PKG);
		}
	}

	alpm_list_free_inner (pkgs, (alpm_list_fn_free) aur_pkg_free);
	alpm_list_free (pkgs);

	return pkgs_found;
}

static int aur_request_info (alpm_list_t **targets, CURL *curl)
{
	alpm_list_t *real_targets = NULL;
	for (const alpm_list_t *t = *targets; t; t = alpm_list_next(t)) {
		target_t *one_target = target_parse (t->data);
		if (one_target->db && strcmp (one_target->db, AUR_REPO) !=0 ) {
			target_free (one_target);
		} else {
			real_targets = alpm_list_add (real_targets, one_target);
		}
	}

	if (real_targets == NULL) {
		return 0;
	}

	int pkgs_found = 0;
	target_arg_t *ta = target_arg_init ((ta_dup_fn) strdup,
		(alpm_list_fn_cmp) strcmp,
		(alpm_list_fn_free) free);
	const alpm_list_t *t = real_targets;

	while (t) {
		int fetch_waiting = 0;
		int args_left = AUR_MAX_ARG;
		string_t *url = aur_prepare_url (AUR_RPC_INFO);

		for (; t && args_left--; t = alpm_list_next (t)) {
			const target_t *one_target = t->data;
			char *encoded_arg = curl_easy_escape (curl, one_target->name, 0);
			if (encoded_arg != NULL) {
				url = string_cat (url, AUR_RPC_INFO_ARG);
				url = string_cat (url, encoded_arg);
				curl_free (encoded_arg);
				fetch_waiting = 1;
			} else {
				args_left++;
			}
		}

		if (!fetch_waiting) {
			string_free (url);
			break;
		}

		alpm_list_t *pkgs = aur_fetch (curl, string_cstr (url));
		string_free (url);

		for (const alpm_list_t *p = pkgs; p; p = alpm_list_next (p)) {
			const aurpkg_t *pkg = p->data;
			const char *pkgname = aur_pkg_get_name (pkg);
			const char *pkgver = aur_pkg_get_version (pkg);
			const target_t *one_target = alpm_list_find (real_targets,
					pkgname, (alpm_list_fn_cmp) target_name_cmp);
			if (one_target && target_check_version (one_target, pkgver)) {
				if (config.pkgbase && strcmp (aur_pkg_get_name (p->data), aur_pkg_get_pkgbase (p->data)) != 0) {
					continue;
				}
				pkgs_found++;
				/* one_target->orig is not duplicated,
				 * 'ta' will use it until target_arg_close() call.
				 */
				if (target_arg_add (ta, one_target->orig, (void *) pkgname)) {
					print_package (one_target->orig, p->data, aur_get_str);
				}
			}
		}

		alpm_list_free_inner (pkgs, (alpm_list_fn_free) aur_pkg_free);
		alpm_list_free (pkgs);
	}

	/* target_arg_close() must be called before freeing real_targets */
	*targets = target_arg_close (ta, *targets);
	alpm_list_free_inner (real_targets, (alpm_list_fn_free) target_free);
	alpm_list_free (real_targets);

	return pkgs_found;
}

static int aur_request (alpm_list_t **targets, int type)
{
	if (targets == NULL) {
		return 0;
	}

	if (strncmp (config.aur_url, "https", strlen ("https")) == 0) {
		curl_global_init(CURL_GLOBAL_SSL);
	} else {
		curl_global_init(CURL_GLOBAL_NOTHING);
	}

	CURL *curl = curl_easy_init ();
	if (!curl) {
		perror ("curl");
		return 0;
	}

	curl_easy_setopt (curl, CURLOPT_ENCODING, "gzip");
	curl_easy_setopt (curl, CURLOPT_USERAGENT, PQ_USERAGENT);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_getdata_cb);
	if (config.insecure) {
		curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0);
	}
	int aur_pkgs_found = (type == AUR_SEARCH)
			? aur_request_search (targets, curl)
			: aur_request_info (targets, curl);

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return aur_pkgs_found;
}

int aur_info (alpm_list_t **targets)
{
	return aur_request (targets, AUR_INFO);
}

int aur_search (alpm_list_t *targets)
{
	return aur_request (&targets, AUR_SEARCH);
}

const char *aur_get_str (void *p, unsigned char c)
{
	aurpkg_t *pkg = (aurpkg_t *) p;
	static char *info = NULL;
	static int free_info = 0;
	if (free_info) {
		free (info);
		free_info = 0;
	}
	info = NULL;
	switch (c)
	{
		case 'c':
			info = concat_str_list (aur_pkg_get_checkdepends (pkg));
			free_info = 1;
			break;
		case 'C':
			info = concat_str_list (aur_pkg_get_conflicts (pkg));
			free_info = 1;
			break;
		case 'd':
			info = (char *) aur_pkg_get_desc (pkg);
			break;
		case 'D':
			info = concat_str_list (aur_pkg_get_depends (pkg));
			free_info = 1;
			break;
		case 'e':
			info = concat_str_list (aur_pkg_get_licenses (pkg));
			free_info = 1;
			break;
		case 'g':
			info = concat_str_list (aur_pkg_get_groups (pkg));
			free_info = 1;
			break;
		case 'G':
			if (config.aur_url && aur_pkg_get_pkgbase (pkg)) {
				info = (char *) malloc (sizeof (char) *
					(strlen (config.aur_url) +
					strlen (aur_pkg_get_pkgbase (pkg)) +
					6 /* '/%s.git + \0 */
				));
				strcpy (info, config.aur_url);
				strcat (info, "/");
				strcat (info, aur_pkg_get_pkgbase (pkg));
				strcat (info, ".git");
				free_info = 1;
			}
			break;
		case 'i':
			info = itostr (aur_pkg_get_id (pkg));
			free_info = 1;
			break;
		case 'm':
			info = (char *) aur_pkg_get_maintainer (pkg);
			break;
		case 'M':
			info = concat_str_list (aur_pkg_get_makedepends (pkg));
			free_info = 1;
			break;
		case 'n':
			info = (char *) aur_pkg_get_name (pkg);
			break;
		case 'L':
			info = ttostr (aur_pkg_get_lastmod (pkg));
			free_info = 1;
			break;
		case 'o':
			info = itostr (aur_pkg_get_outofdate (pkg));
			free_info = 1;
			break;
		case 'O':
			info = concat_str_list (aur_pkg_get_optdepends (pkg));
			free_info = 1;
			break;
		case 'p':
			{
				int ret = asprintf (&info, "%.2f", aur_pkg_get_popularity (pkg));
				if (ret < 0) {
					FREE (info);
				} else {
					free_info = 1;
				}
			}
			break;
		case 'P':
			info = concat_str_list (aur_pkg_get_provides (pkg));
			free_info = 1;
			break;
		case 's':
		case 'r':
			info = strdup (AUR_REPO);
			free_info = 1;
			break;
		case 'R':
			info = concat_str_list (aur_pkg_get_replaces (pkg));
			free_info = 1;
			break;
		case 'u':
			if (config.aur_url && aur_pkg_get_urlpath (pkg)) {
				info = (char *) malloc (sizeof (char) *
					(strlen (config.aur_url) +
					strlen (aur_pkg_get_urlpath (pkg)) +
					2 /* '/' separate url and filename */
				));
				strcpy (info, config.aur_url);
				strcat (info, aur_pkg_get_urlpath (pkg));
				free_info = 1;
			}
			break;
		case 'U':
			info = (char *) aur_pkg_get_url (pkg);
			break;
		case 'S':
			info = ttostr (aur_pkg_get_firstsubmit (pkg));
			free_info = 1;
			break;
		case 'V':
		case 'v':
			info = (char *) aur_pkg_get_version (pkg);
			break;
		case 'w':
			info = itostr (aur_pkg_get_votes (pkg));
			free_info = 1;
			break;
		default:
			return NULL;
			break;
	}
	return info;
}

void aur_cleanup ()
{
	aur_get_str (NULL, 0);
}

/* vim: set ts=4 sw=4 noet: */
