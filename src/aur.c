/*
 *  aur.c
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
#include "config.h"
#include <string.h>
#include <alpm_list.h>

#ifdef USE_FETCH
#include <ctype.h>
#include <fetch.h>
#else
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#endif
#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#include "aur.h"
#include "util.h"

/*
 * AUR url
 */
#define AUR_RPC	"/rpc.php"
#define AUR_RPC_SEARCH "?type=search&arg="
#define AUR_RPC_INFO "?type=info&arg="

/*
 * AUR repo name
 */
#define AUR_REPO "aur"

/*
 * AUR package information
 */
#define AUR_ID		 "ID"
#define AUR_NAME 	"Name"
#define AUR_VER		"Version"
#define AUR_CAT		"CategoryID"
#define AUR_DESC	"Description"
#define AUR_LOC		"LocationID"
#define AUR_URL		"URL"
#define AUR_URLPATH "URLPath"
#define AUR_LICENSE	"License"
#define AUR_VOTE	"NumVotes"
#define AUR_OUT		"OutOfDate"
#define AUR_LAST_ID	AUR_OUT



/*
 * AUR REQUEST
 */
#define AUR_INFO	1
#define AUR_SEARCH	2
#define AUR_INFO_P	3

#ifdef USE_FETCH
#define AUR_DL_LEN 1024 * 10
#define AUR_DL_TM 10	/* Timeout */
#endif

alpm_list_t *pkgs=NULL;

aurpkg_t *aur_pkg_new ()
{
	aurpkg_t *pkg = NULL;
	if ((pkg = malloc (sizeof(aurpkg_t))) == NULL)
	{
		perror ("malloc");
		exit (1);
	}
	pkg->id = 0;
	pkg->name = NULL;
	pkg->version = NULL;
	pkg->category = 0;
	pkg->desc = NULL;
	pkg->location = 0;
	pkg->url = NULL;
	pkg->urlpath = NULL;
	pkg->license = NULL;
	pkg->votes = 0;
	pkg->outofdate = 0;
	return pkg;
}

aurpkg_t *aur_pkg_free (aurpkg_t *pkg)
{
	if (pkg == NULL)
		return NULL;
	free (pkg->name);
	free (pkg->version);
	free (pkg->desc);
	free (pkg->url);
	free (pkg->urlpath);
	free (pkg->license);
	free (pkg);
	return NULL;
}

aurpkg_t *aur_pkg_dup (const aurpkg_t *pkg)
{
	if (pkg == NULL)
		return NULL;
	aurpkg_t *pkg_ret = aur_pkg_new();
	pkg_ret->id = pkg->id;
	pkg_ret->name = strdup (pkg->name);
	pkg_ret->version = strdup (pkg->version);
	pkg_ret->category = pkg->category;
	pkg_ret->desc = strdup (pkg->desc);
	pkg_ret->location = pkg->location;
	pkg_ret->url = strdup (pkg->url);
	pkg_ret->urlpath = strdup (pkg->urlpath);
	pkg_ret->license = strdup (pkg->license);
	pkg_ret->votes = pkg->votes;
	pkg_ret->outofdate = pkg->outofdate;
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


#ifdef USE_FETCH
/* {{{ */
/* http://www.geekhideout.com/urlcode.shtml */
/* Converts an integer value to its hex character*/
char to_hex (char code) 
{
	static char hex[] = "0123456789abcdef";
	return hex[code & 15];
}

/* Returns a url-encoded version of str */
char *url_encode (const char *str) 
{
	const char *pstr = str;
	char *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
	while (*pstr) 
	{
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
			*pbuf++ = *pstr;
		else if (*pstr == ' ') 
			*pbuf++ = '+';
	 	else 
			*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}

/* }}} */
#else
/* {{{ */
size_t curl_getdata_cb (void *data, size_t size, size_t nmemb, void *userdata)
{
	string_t *s = (string_t *) userdata;
	string_ncat (s, data, nmemb);
	return nmemb;
}
/* }}} */
#endif

static int json_key (void * ctx, const unsigned char * stringVal,
                            unsigned int stringLen)
{
	package_json_t *pkg = (package_json_t *) ctx;
	strncpy (pkg->current_key, (const char *) stringVal, stringLen);
	pkg->current_key[stringLen] = '\0';
    return 1;
}


static int json_value (void * ctx, const unsigned char * stringVal,
                           unsigned int stringLen)
{
	char *s = strndup ((const char *)stringVal, stringLen);
	int free_s = 1;
	package_json_t *pkg = (package_json_t *) ctx;
	if (strcmp (pkg->current_key, AUR_ID)==0 && pkg->pkg!=NULL)
		return 0;
	if (strcmp (pkg->current_key, AUR_ID)==0)
	{
		pkg->pkg = malloc (sizeof(aurpkg_t));
		pkg->pkg->id = atoi (s);
	}
	else if (strcmp (pkg->current_key, AUR_NAME)==0)
	{
		pkg->pkg->name = s;
		free_s = 0;
	}
	else if (strcmp (pkg->current_key, AUR_VER)==0)
	{
		pkg->pkg->version = s;
		free_s = 0;
	}
	else if (strcmp (pkg->current_key, AUR_CAT)==0)
	{
		pkg->pkg->category = atoi (s);
	}
	else if (strcmp (pkg->current_key, AUR_DESC)==0)
	{
		pkg->pkg->desc = s;
		free_s = 0;
	}
	else if (strcmp (pkg->current_key, AUR_LOC)==0)
	{
		pkg->pkg->location = atoi(s);
	}
	else if (strcmp (pkg->current_key, AUR_URL)==0)
	{
		pkg->pkg->url = s;
		free_s = 0;
	}
	else if (strcmp (pkg->current_key, AUR_URLPATH)==0)
	{
		pkg->pkg->urlpath = s;
		free_s = 0;
	}
	else if (strcmp (pkg->current_key, AUR_LICENSE)==0)
	{
		pkg->pkg->license = s;
		free_s = 0;
	}
	else if (strcmp (pkg->current_key, AUR_VOTE)==0)
	{
		pkg->pkg->votes = atoi(s);
	}
	else if (strcmp (pkg->current_key, AUR_OUT)==0)
	{
		pkg->pkg->outofdate = atoi(s);
	}
	if (strcmp (pkg->current_key, AUR_LAST_ID)==0)
	{
		pkgs = alpm_list_add(pkgs,pkg->pkg);
		pkg->pkg = NULL;
	}
	if (free_s)
		free (s);
	
    return 1;
}


static yajl_callbacks callbacks = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    json_value,
    NULL,
    json_key,
    NULL,
    NULL,
    NULL,
};


int aur_request (alpm_list_t *targets, int type)
{
	int ret=0;
	alpm_list_t *list_t=NULL;
#ifdef USE_FETCH	
	struct url *aur_url;
	fetchIO *dlu = NULL;
	char buf[AUR_DL_LEN];
#else
	CURL *curl;
	CURLcode curl_code;
#endif
	yajl_handle hand;
	yajl_status stat;
	yajl_parser_config cfg = { 1, 1 };
	package_json_t pkg_json = { NULL, "" };
	char aur_rpc[PATH_MAX];
	string_t *res;
	alpm_list_t *t;
	const char *target, *pkg_name;
	char *aur_target = NULL;
	target_t *t1=NULL;

	if (targets == NULL)
		return 0;

#ifdef USE_FETCH	
	fetchTimeout = AUR_DL_TM;
#else
	curl = curl_easy_init ();
	if (!curl)
	{
		perror ("curl");
		return 0;
	}
#endif
	sprintf (aur_rpc, "%s%s", AUR_BASE_URL, AUR_RPC);
	if (type == AUR_SEARCH)
	{
		strcat (aur_rpc, AUR_RPC_SEARCH);
		list_t = targets;
		targets = alpm_list_add (NULL, strdup (alpm_list_getdata (list_t)));
	}
	else
		strcat (aur_rpc, AUR_RPC_INFO);
	
	for(t = targets; t; t = alpm_list_next(t)) 
	{
		target = alpm_list_getdata(t);
		pkg_name = target;
		if (type == AUR_SEARCH)
		{
			if (strchr (target, '*'))
			{
				char *c;
				aur_target = strdup (target);
				while ((c = strchr (aur_target, '*')) != NULL)
					*c='%';
				pkg_name = aur_target;
			}
			aur_rpc[strlen(AUR_BASE_URL) + strlen(AUR_RPC) + strlen(AUR_RPC_SEARCH)] = '\0';
		}
		else
		{
			t1 = target_parse (target);
			if (t1->db && strcmp (t1->db, AUR_REPO)!=0)
				continue;
			pkg_name = t1->name;
			aur_rpc[strlen(AUR_BASE_URL) + strlen(AUR_RPC) + strlen(AUR_RPC_INFO)] = '\0';
		}
#ifdef USE_FETCH	
/* {{{ */
		char *pkg_name_encode=url_encode (pkg_name);
 		strcat (aur_rpc, pkg_name_encode);
		free (pkg_name_encode);
 		res = string_new();
		hand = yajl_alloc(&callbacks, &cfg,  NULL, (void *) &pkg_json);
		/* According to pacman code, libfetch doesn't reset error code */
		fetchLastErrCode = 0;
		aur_url = fetchParseURL (aur_rpc);
		dlu = fetchGet (aur_url, "");
		if (fetchLastErrCode == 0 && dlu)
 		{
			ssize_t nread=0;
			while ((nread = fetchIO_read (dlu, buf, AUR_DL_LEN)) > 0)
				string_ncat (res, buf, nread);
			fetchIO_close (dlu);
/* }}} */
#else
/* {{{ */
		char *pkg_name_encode=curl_easy_escape(curl, pkg_name, 0);
		if (pkg_name_encode==NULL) 
			continue;
		strcat (aur_rpc, pkg_name_encode);
		curl_free (pkg_name_encode);
		res = string_new();
    	hand = yajl_alloc(&callbacks, &cfg,  NULL, (void *) &pkg_json);
		curl_easy_setopt (curl, CURLOPT_ENCODING, "gzip");
		curl_easy_setopt (curl, CURLOPT_WRITEDATA, res);
		curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_getdata_cb);
		curl_easy_setopt (curl, CURLOPT_URL, aur_rpc);
		if ((curl_code = curl_easy_perform (curl)) == CURLE_OK)
		{
/* }}} */
#endif		
			stat = yajl_parse(hand, (const unsigned char *) res->s, strlen (res->s));
			if (stat != yajl_status_ok && stat != yajl_status_insufficient_data)
			{
				unsigned char * str = yajl_get_error(hand, 1, (const unsigned char *) res->s, strlen (res->s));
				fprintf(stderr, (const char *) str);
				yajl_free_error(hand, str);
				target_free (t1);
				string_free (res);
				yajl_free(hand);
				break;
			}
			else
			{
 				alpm_list_t *p;
				pkg_json.pkg=NULL;
				if (type != AUR_SEARCH)
				{
					for (p = pkgs; p; p = alpm_list_next(p))
					{
						if (target_check_version (t1, aur_pkg_get_version (alpm_list_getdata (p))))
							print_package (target, alpm_list_getdata (p), aur_get_str);
					}
					if (type == AUR_INFO_P && pkgs == NULL)
					{
						aurpkg_t *pkg = aur_pkg_new();
						pkg->name = strdup (target);
						print_package (target, pkg, aur_get_str);
						aur_pkg_free (pkg);
					}
					alpm_list_free_inner (pkgs, (alpm_list_fn_free) aur_pkg_free);
					alpm_list_free (pkgs);
				}
				else if (type == AUR_SEARCH && pkgs)
				{
					alpm_list_t *l,*p;
					char *ts = concat_str_list (list_t);
					int found;
					/* Aur default sort by names */
					if (config.sort == 0)
						pkgs = alpm_list_msort (pkgs, alpm_list_count (pkgs), 
									(alpm_list_fn_cmp) aur_pkg_cmp);
					/* Filter results with others targets without making more queries */
					for (p = pkgs; p; p = alpm_list_next(p))
					{
						found=1;
						for (l = alpm_list_next (list_t); l && found; l = alpm_list_next (l))
						{
							if (strcasestr (aur_pkg_get_name (alpm_list_getdata (p)), alpm_list_getdata (l))==NULL &&
								strcasestr (aur_pkg_get_desc (alpm_list_getdata (p)), alpm_list_getdata (l))==NULL)
								found=0;
						}
						if (found)
							print_or_add_result (alpm_list_getdata (p), R_AUR_PKG);
							//print_package (ts, alpm_list_getdata (p), aur_get_str);
					}
					free (ts);
					alpm_list_free_inner (pkgs, (alpm_list_fn_free) aur_pkg_free);
					alpm_list_free (pkgs);
				}
				pkgs = NULL;
			}
		}
#ifdef USE_FETCH
		fetchFreeURL (aur_url);
#endif		
		if (type != AUR_SEARCH)
			target_free (t1);
		string_free (res);
		yajl_free(hand);
		if (aur_target)
		{
			free (aur_target);
			aur_target = NULL;
			target = NULL;
		}
#ifdef USE_FETCH
/* {{{ */
		if (fetchLastErrCode != 0)
 		{
			fprintf(stderr, "AUR rpc error: %s\n", fetchLastErrString);
 			break;
 		}
/* }}} */
#else		
/* {{{ */
		if (curl_code != CURLE_OK)
		{
			fprintf(stderr, "curl error: %s\n", curl_easy_strerror (curl_code));
			break;
		}
/* }}} */
#endif
	}
#ifndef USE_FETCH
	curl_easy_cleanup(curl);
	curl_global_cleanup();
#endif
	if (list_t)
	{
		FREELIST(targets);
		targets=list_t;
	}

	return ret;
}

int aur_info (alpm_list_t *targets)
{
	return aur_request (targets, AUR_INFO);
}

int aur_search (alpm_list_t *targets)
{
	return aur_request (targets, AUR_SEARCH);
}


int aur_info_none (alpm_list_t *targets)
{
	return aur_request (targets, AUR_INFO_P);
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
		case 'i': 
			info = itostr (aur_pkg_get_id (pkg)); 
			free_info = 1;
			break;
		case 'n': info = (char *) aur_pkg_get_name (pkg); break;
		case 'o': 
			info = itostr (aur_pkg_get_outofdate (pkg)); 
			free_info = 1;
			break;
		case 's':
		case 'r': info = strdup (AUR_REPO); free_info=1; break;
		case 'u': 
			info = (char *) malloc (sizeof (char) * 
				(strlen (AUR_BASE_URL) + 
				strlen (aur_pkg_get_urlpath (pkg)) +
				2 /* '/' separate url and filename */
				));
			strcpy (info, AUR_BASE_URL);
			strcat (info, aur_pkg_get_urlpath (pkg));
			free_info = 1;
			break;
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

/* vim: set ts=4 sw=4 noet foldmarker={{{,}}} foldmethod=marker: */
