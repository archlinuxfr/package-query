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
#include <alpm_list.h>
#include <errno.h>


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
#define AUR_RPC_SEARCH   "?type=search&arg="
#define AUR_RPC_INFO     "?type=multiinfo"
#define AUR_RPC_INFO_ARG "&arg[]="
#define AUR_URL_ID       "/packages.php?setlang=en&ID="

/*
 * AUR repo name
 */
#define AUR_REPO "aur"

/*
 * AUR package information
 */
#define AUR_MAINTAINER  "Maintainer"
#define AUR_ID          "ID"
#define AUR_NAME        "Name"
#define AUR_PKGBASE_ID  "PackageBaseID"
#define AUR_PKGBASE     "PackageBase"
#define AUR_VER         "Version"
#define AUR_CAT         "CategoryID"
#define AUR_DESC        "Description"
#define AUR_URL         "URL"
#define AUR_LICENSE     "License"
#define AUR_VOTE        "NumVotes"
#define AUR_OUT         "OutOfDate"
#define AUR_FIRST       "FirstSubmitted"
#define AUR_LAST        "LastModified"
#define AUR_URLPATH     "URLPath"

#define AUR_LAST_ID     AUR_URLPATH

/* AUR JSON error */
#define AUR_TYPE_ERROR   "error"
#define AUR_JSON_TYPE_KEY   "type"
#define AUR_JSON_RESULTS_KEY "results"

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
#define AUR_ID_LEN 	20
typedef struct _jsonpkg_t
{
	alpm_list_t *pkgs;
	aurpkg_t *pkg;
	char current_key[AUR_ID_LEN];
	int error;
	char *error_msg;
	int level;
} jsonpkg_t;



aurpkg_t *aur_pkg_new ()
{
	aurpkg_t *pkg = NULL;
	MALLOC (pkg, sizeof(aurpkg_t));
	pkg->id = 0;
	pkg->name = NULL;
	pkg->version = NULL;
	pkg->category = 0;
	pkg->desc = NULL;
	pkg->url = NULL;
	pkg->urlpath = NULL;
	pkg->license = NULL;
	pkg->votes = 0;
	pkg->outofdate = 0;
	pkg->firstsubmit = 0;
	pkg->lastmod = 0;
	pkg->maintainer = NULL;
	return pkg;
}

void aur_pkg_free (aurpkg_t *pkg)
{
	if (pkg == NULL)
		return;
	FREE (pkg->name);
	FREE (pkg->version);
	FREE (pkg->desc);
	FREE (pkg->url);
	FREE (pkg->urlpath);
	FREE (pkg->license);
	FREE (pkg->maintainer);
	FREE (pkg->pkgbase);
	FREE (pkg);
}

aurpkg_t *aur_pkg_dup (const aurpkg_t *pkg)
{
	if (pkg == NULL)
		return NULL;
	aurpkg_t *pkg_ret = aur_pkg_new();
	pkg_ret->id = pkg->id;
	pkg_ret->name = STRDUP (pkg->name);
	pkg_ret->version = STRDUP (pkg->version);
	pkg_ret->category = pkg->category;
	pkg_ret->desc = STRDUP (pkg->desc);
	pkg_ret->url = STRDUP (pkg->url);
	pkg_ret->urlpath = STRDUP (pkg->urlpath);
	pkg_ret->license = STRDUP (pkg->license);
	pkg_ret->votes = pkg->votes;
	pkg_ret->outofdate = pkg->outofdate;
	pkg_ret->firstsubmit = pkg->firstsubmit;
	pkg_ret->lastmod = pkg->lastmod;
	pkg_ret->maintainer = STRDUP (pkg->maintainer);
	return pkg_ret;
}

int aur_pkg_cmp (const aurpkg_t *pkg1, const aurpkg_t *pkg2)
{
	return strcmp (aur_pkg_get_name (pkg1), aur_pkg_get_name (pkg2));
}

int aur_pkg_votes_cmp (const aurpkg_t *pkg1, const aurpkg_t *pkg2)
{
	if (pkg1->votes > pkg2->votes) 
		return 1;
	else if (pkg1->votes < pkg2->votes)
		return -1;
	return 0;
}

unsigned int aur_pkg_get_id (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->id;
	return 0;
}

const char * aur_pkg_get_name (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->name;
	return NULL;
}

unsigned int aur_pkg_get_pkgbase_id (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->pkgbase_id;
	return 0;
}

const char * aur_pkg_get_pkgbase (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->pkgbase;
	return NULL;
}

const char * aur_pkg_get_version (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->version;
	return NULL;
}

const char * aur_pkg_get_desc (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->desc;
	return NULL;
}

const char * aur_pkg_get_url (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->url;
	return NULL;
}

const char * aur_pkg_get_urlpath (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->urlpath;
	return NULL;
}

const char * aur_pkg_get_license (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->license;
	return NULL;
}

unsigned int aur_pkg_get_votes (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->votes;
	return 0;
}

unsigned short aur_pkg_get_outofdate (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->outofdate;
	return 0;
}

time_t aur_pkg_get_firstsubmit (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->firstsubmit;
	return 0;
}

time_t aur_pkg_get_lastmod (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->lastmod;
	return 0;
}

const char * aur_pkg_get_maintainer (const aurpkg_t * pkg)
{
	if (pkg!=NULL)
		return pkg->maintainer;
	return NULL;
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
	if (pkg_json->level > 1)
		pkg_json->pkg = aur_pkg_new();

	return 1;
}

static int json_end_map (void *ctx)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;

	if (pkg_json == NULL)
		return 1;

	pkg_json->level -= 1;
	if (pkg_json->level == 1 && pkg_json->pkg != NULL)
	{
		switch (config.sort)
		{
			case S_VOTE:
				pkg_json->pkgs = alpm_list_add_sorted (pkg_json->pkgs, 
			        pkg_json->pkg, (alpm_list_fn_cmp) aur_pkg_votes_cmp);
				break;
			default:
				pkg_json->pkgs = alpm_list_add_sorted (pkg_json->pkgs, 
				    pkg_json->pkg, (alpm_list_fn_cmp) aur_pkg_cmp);
		}
		pkg_json->pkg = NULL;
	}
	return 1;
}

static int json_key (void * ctx, const unsigned char * stringVal,
                            size_t stringLen)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;
	stringLen = (stringLen>=AUR_ID_LEN) ? AUR_ID_LEN-1 : stringLen;
	strncpy (pkg_json->current_key, (const char *) stringVal, stringLen);
	pkg_json->current_key[stringLen] = '\0';
    return 1;
}

static int json_integer (void * ctx, long long val)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;
	// package info in level 2
	if (pkg_json->level<2) return 1;
	if (strcmp (pkg_json->current_key, AUR_ID)==0)
	{
		pkg_json->pkg->id = (int) val;
	}
	else if (strcmp (pkg_json->current_key, AUR_CAT)==0)
	{
		pkg_json->pkg->category = (int) val;
	}
	else if (strcmp (pkg_json->current_key, AUR_VOTE)==0)
	{
		pkg_json->pkg->votes = (int) val;
	}
	else if (strcmp (pkg_json->current_key, AUR_OUT)==0)
	{
		pkg_json->pkg->outofdate = (int) val;
	}
	else if (strcmp (pkg_json->current_key, AUR_FIRST)==0)
	{
		pkg_json->pkg->firstsubmit = (time_t) val;
	}
	else if (strcmp (pkg_json->current_key, AUR_LAST)==0)
	{
		pkg_json->pkg->lastmod = (time_t) val;
	}
    return 1;
}

static int json_string (void * ctx, const unsigned char * stringVal,
                           size_t stringLen)
{
	jsonpkg_t *pkg_json = (jsonpkg_t *) ctx;
	// package info in level 2
	if (pkg_json->level<2)
	{
		if (strcmp (pkg_json->current_key, AUR_JSON_TYPE_KEY)==0)
		{
			if (strncmp ((const char *)stringVal, AUR_TYPE_ERROR, stringLen)==0)
			{
				pkg_json->error = 1;
			}
		}
		if (pkg_json->error && strcmp (pkg_json->current_key, AUR_JSON_RESULTS_KEY)==0)
		{
			pkg_json->error_msg = strndup ((const char *)stringVal, stringLen);
		}
		return 1;
	}
	char *s = strndup ((const char *)stringVal, stringLen);
	int free_s = 1;
	if (strcmp (pkg_json->current_key, AUR_MAINTAINER)==0)
	{
		pkg_json->pkg->maintainer = s;
		free_s = 0;
	}
	if (strcmp (pkg_json->current_key, AUR_ID)==0)
	{
		pkg_json->pkg->id = atoi (s);
	}
	else if (strcmp (pkg_json->current_key, AUR_NAME)==0)
	{
		pkg_json->pkg->name = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_PKGBASE_ID)==0)
	{
		pkg_json->pkg->pkgbase_id = atoi (s);
	}
	else if (strcmp (pkg_json->current_key, AUR_PKGBASE)==0)
	{
		pkg_json->pkg->pkgbase = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_VER)==0)
	{
		pkg_json->pkg->version = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_CAT)==0)
	{
		pkg_json->pkg->category = atoi (s);
	}
	else if (strcmp (pkg_json->current_key, AUR_DESC)==0)
	{
		pkg_json->pkg->desc = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_URL)==0)
	{
		pkg_json->pkg->url = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_URLPATH)==0)
	{
		pkg_json->pkg->urlpath = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_LICENSE)==0)
	{
		pkg_json->pkg->license = s;
		free_s = 0;
	}
	else if (strcmp (pkg_json->current_key, AUR_VOTE)==0)
	{
		pkg_json->pkg->votes = atoi(s);
	}
	else if (strcmp (pkg_json->current_key, AUR_OUT)==0)
	{
		pkg_json->pkg->outofdate = atoi(s);
	}
	else if (strcmp (pkg_json->current_key, AUR_FIRST)==0)
	{
		pkg_json->pkg->firstsubmit = atol(s);
	}
	else if (strcmp (pkg_json->current_key, AUR_LAST)==0)
	{
		pkg_json->pkg->lastmod = atol(s);
	}
	if (free_s)
		free (s);
	
    return 1;
}

static yajl_callbacks callbacks = {
    NULL,
    NULL,
    json_integer,
    NULL,
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
	jsonpkg_t pkg_json = { NULL, NULL, "", 0, NULL, 0};
	yajl_handle hand;
	yajl_status stat;
	hand = yajl_alloc(&callbacks, NULL, (void *) &pkg_json);
	stat = yajl_parse(hand, (const unsigned char *) s, strlen (s));
	if (stat == yajl_status_ok)
		stat = yajl_complete_parse(hand);
	if (stat != yajl_status_ok)
	{
		unsigned char * str = yajl_get_error(hand, 1, 
		    (const unsigned char *) s, strlen (s));
		fprintf(stderr, "%s\n", (const char *) str);
		yajl_free_error(hand, str);
		alpm_list_free_inner (pkg_json.pkgs, (alpm_list_fn_free) aur_pkg_free);
		alpm_list_free (pkg_json.pkgs);
		pkg_json.pkgs = NULL;
	}
	yajl_free(hand);
	if (pkg_json.error)
	{
		fprintf(stderr, "AUR error : %s\n", pkg_json.error_msg);
		FREE (pkg_json.error_msg);
	}
	return pkg_json.pkgs;
}

static alpm_list_t *aur_fetch (CURL *curl, const char *url)
{
	CURLcode curl_code;
	long http_code;
	alpm_list_t *pkgs;
	string_t *res = string_new();
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, res);
	curl_easy_setopt (curl, CURLOPT_URL, (const char *) url);
	if ((curl_code = curl_easy_perform (curl)) != CURLE_OK)
	{
		fprintf(stderr, "curl error: %s\n", curl_easy_strerror (curl_code));
		string_free (res);
		return 0;
	}
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200)
	{
		fprintf(stderr, "The URL %s returned error : %ld\n", url, http_code);
		string_free (res);
		return 0;
	}
	pkgs = aur_json_parse (string_cstr (res));
	string_free (res);
	return pkgs;
}

static int aur_request (alpm_list_t **targets, int type)
{
	int aur_pkgs_found=0;
	alpm_list_t *real_targets=NULL;
	target_t *one_target;
	char *encoded_arg;
	string_t *url=NULL;
	alpm_list_t *pkgs;
	alpm_list_t *t,*p;
	target_arg_t *ta=NULL;
	CURL *curl;

	if (targets == NULL)
		return 0;

	if (type != AUR_SEARCH)
	{
		for(t = *targets; t; t = alpm_list_next(t))
		{
			one_target = target_parse (t->data);
			if (one_target->db && strcmp (one_target->db, AUR_REPO)!=0)
				target_free (one_target);
			else
				real_targets = alpm_list_add (real_targets, one_target);
		}
		if (real_targets == NULL)
			return 0;
	}

	if (strncmp (config.aur_url, "https", 5) == 0)
		curl_global_init(CURL_GLOBAL_SSL);
	else
		curl_global_init(CURL_GLOBAL_NOTHING);
	curl = curl_easy_init ();
	if (!curl)
	{
		perror ("curl");
		return 0;
	}
	curl_easy_setopt (curl, CURLOPT_ENCODING, "gzip");
	curl_easy_setopt (curl, CURLOPT_USERAGENT, PQ_USERAGENT);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_getdata_cb);
	if (config.insecure)
		curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0);
	url = string_new();
	if (type == AUR_SEARCH)
	{
		url = string_cat (url, config.aur_url);
		url = string_cat (url, AUR_RPC);
		url = string_cat (url, AUR_RPC_SEARCH);
		encoded_arg = curl_easy_escape (curl, (*targets)->data, 0);
		if (encoded_arg != NULL)
		{
			url = string_cat (url, encoded_arg);
			curl_free (encoded_arg);
			pkgs = aur_fetch (curl, string_cstr (url));
			for (p=pkgs; p; p=alpm_list_next (p))
			{
				int match=1;
				for (t=alpm_list_next (*targets); t; t=alpm_list_next (t))
				{
					if (strcasestr (aur_pkg_get_name (p->data),
									t->data)==NULL &&
						(aur_pkg_get_desc (p->data)==NULL ||
						strcasestr (aur_pkg_get_desc (p->data),
									t->data)==NULL))
						match=0;
				}
				if (match)
				{
					aur_pkgs_found++;
					print_or_add_result (p->data, R_AUR_PKG);
				}
			}
			alpm_list_free_inner (pkgs, (alpm_list_fn_free) aur_pkg_free);
			alpm_list_free (pkgs);
		}
	}
	else
	{
		ta = target_arg_init ((ta_dup_fn) strdup,
	        (alpm_list_fn_cmp) strcmp,
		    (alpm_list_fn_free) free);
		t=real_targets;
		while (t)
		{
			int fetch_waiting=0;
			int args_left=AUR_MAX_ARG;
			string_reset (url);
			url = string_cat (url, config.aur_url);
			url = string_cat (url, AUR_RPC);
			url = string_cat (url, AUR_RPC_INFO);
			for (; t && args_left--; t=alpm_list_next (t))
			{
				one_target = t->data;
				encoded_arg = curl_easy_escape (curl, one_target->name, 0);
				if (encoded_arg != NULL)
				{
					url = string_cat (url, AUR_RPC_INFO_ARG);
					url = string_cat (url, encoded_arg);
					curl_free (encoded_arg);
					fetch_waiting=1;
				}
				else args_left++;
			}
			if (!fetch_waiting) break;
			pkgs = aur_fetch (curl, string_cstr (url));
			for (p=pkgs; p; p=alpm_list_next (p))
			{
				aurpkg_t *pkg = p->data;
				const char *pkgname = aur_pkg_get_name (pkg);
				const char *pkgver = aur_pkg_get_version (pkg);
				one_target = alpm_list_find (real_targets,
				    pkgname, (alpm_list_fn_cmp) target_name_cmp);
				if (one_target && target_check_version (one_target, pkgver))
				{
					if (config.pkgbase && strcmp (aur_pkg_get_name (p->data), aur_pkg_get_pkgbase (p->data)) != 0)
					{
						continue;
					}
					aur_pkgs_found++;
					/* one_target->orig is not duplicated,
					 * 'ta' will use it until target_arg_close() call.
					 */
					if (target_arg_add (ta, one_target->orig, (void *) pkgname))
						print_package (one_target->orig,
						    p->data, aur_get_str);
				}
			}
			alpm_list_free_inner (pkgs, (alpm_list_fn_free) aur_pkg_free);
			alpm_list_free (pkgs);
		};
		/* target_arg_close() must be called before freeing real_targets */
		*targets = target_arg_close (ta, *targets);
		alpm_list_free_inner (real_targets, (alpm_list_fn_free) target_free);
		alpm_list_free (real_targets);
	}
	string_free (url);
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
	static char *info=NULL;
	static int free_info=0;
	if (free_info)
	{
		free (info);
		free_info = 0;
	}
	info = NULL;
	switch (c)
	{
		case 'd': info = (char *) aur_pkg_get_desc (pkg); break;
		case 'G':
			info = (char *) malloc (sizeof (char) *
				(strlen (config.aur_url) +
				strlen (aur_pkg_get_name (pkg)) +
				6 /* '/%s.git + \0 */
			));
			strcpy (info, config.aur_url);
			strcat (info, "/");
			strcat (info, aur_pkg_get_name (pkg));
			strcat (info, ".git");
			free_info = 1;
			break;
		case 'i': 
			info = itostr (aur_pkg_get_id (pkg)); 
			free_info = 1;
			break;
		case 'm': info = (char *) aur_pkg_get_maintainer (pkg); break;
		case 'n': info = (char *) aur_pkg_get_name (pkg); break;
		case 'L':
			info = ttostr (aur_pkg_get_lastmod (pkg));
			free_info=1;
			break;
		case 'o': 
			info = itostr (aur_pkg_get_outofdate (pkg)); 
			free_info = 1;
			break;
		case 's':
		case 'r': info = strdup (AUR_REPO); free_info=1; break;
		case 'u': 
			info = (char *) malloc (sizeof (char) * 
				(strlen (config.aur_url) + 
				strlen (aur_pkg_get_urlpath (pkg)) +
				2 /* '/' separate url and filename */
			));
			strcpy (info, config.aur_url);
			strcat (info, aur_pkg_get_urlpath (pkg));
			free_info = 1;
			break;
		case 'U': info = (char *) aur_pkg_get_url (pkg); break;
		case 'S':
			info = ttostr (aur_pkg_get_firstsubmit (pkg));
			free_info=1;
			break;
		case 'V':
		case 'v': info = (char *) aur_pkg_get_version (pkg); break;
		case 'w': 
			info = itostr (aur_pkg_get_votes (pkg)); 
			free_info = 1;
			break;
		default: return NULL; break;
	}
	return info;
}

void aur_cleanup ()
{
	aur_get_str (NULL, 0);
}

/* vim: set ts=4 sw=4 noet: */
