#include <string.h>
#include <alpm_list.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

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


alpm_list_t *pkgs=NULL;

package_t *package_new ()
{
	package_t *pkg = NULL;
	if ((pkg = malloc (sizeof(package_t))) == NULL)
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

package_t *package_free (package_t *pkg)
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



unsigned int aur_pkg_get_id (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->id;
	return 0;
}

const char * aur_pkg_get_name (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->name;
	return NULL;
}

const char * aur_pkg_get_version (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->version;
	return NULL;
}

const char * aur_pkg_get_desc (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->desc;
	return NULL;
}

const char * aur_pkg_get_url (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->url;
	return NULL;
}

const char * aur_pkg_get_urlpath (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->urlpath;
	return NULL;
}

const char * aur_pkg_get_license (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->license;
	return NULL;
}

unsigned int aur_pkg_get_votes (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->votes;
	return 0;
}

unsigned short aur_pkg_get_outofdate (package_t * pkg)
{
	if (pkg!=NULL)
		return pkg->outofdate;
	return 0;
}


int curl_getdata_cb (void *data, size_t size, size_t nmemb, void *userdata)
{
	string_t *s = (string_t *) userdata;
	string_ncat (s, data, nmemb);
	return nmemb;
}


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
		pkg->pkg = malloc (sizeof(package_t));
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
	CURL *curl;
	yajl_handle hand;
	yajl_status stat;
	yajl_parser_config cfg = { 1, 1 };
	package_json_t pkg_json = { NULL, "" };
	char aur_rpc[PATH_MAX];
	string_t *res;
	alpm_list_t *t;
	const char *target;
	char *aur_target = NULL;

	if (targets == NULL)
		return 0;

	curl = curl_easy_init ();
	if (!curl)
	{
		perror ("curl");
		return 0;
	}
	strcpy (aur_rpc, AUR_BASE_URL);
	strcat (aur_rpc, AUR_RPC);
	if (type == AUR_SEARCH)
		strcat (aur_rpc, AUR_RPC_SEARCH);
	else
		strcat (aur_rpc, AUR_RPC_INFO);
	
	for(t = targets; t; t = alpm_list_next(t)) 
	{
		target = alpm_list_getdata(t);
		if (type == AUR_SEARCH)
		{
			if (strchr (target, '*'))
			{
				char *c;
				aur_target = strdup (target);
				while ((c = strchr (aur_target, '*')) != NULL)
					*c='%';
				target = aur_target;
			}
			aur_rpc[strlen(AUR_BASE_URL) + strlen(AUR_RPC) + strlen(AUR_RPC_SEARCH)] = '\0';
		}
		else
			aur_rpc[strlen(AUR_BASE_URL) + strlen(AUR_RPC) + strlen(AUR_RPC_INFO)] = '\0';
		strcat (aur_rpc, target);
		res = string_new();
    	hand = yajl_alloc(&callbacks, &cfg,  NULL, (void *) &pkg_json);
		curl_easy_setopt (curl, CURLOPT_WRITEDATA, res);
		curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_getdata_cb);
		curl_easy_setopt (curl, CURLOPT_URL, aur_rpc);
		if (curl_easy_perform (curl) == CURLE_OK)
		{
			stat = yajl_parse(hand, (const unsigned char *) res->s, strlen (res->s));
			if (stat != yajl_status_ok && stat != yajl_status_insufficient_data)
			{
				unsigned char * str = yajl_get_error(hand, 1, (const unsigned char *) res->s, strlen (res->s));
				fprintf(stderr, (const char *) str);
				yajl_free_error(hand, str);
				string_free (res);
				yajl_free(hand);
				return 0;
			}
			else
			{
 				alpm_list_t *p;
				for(p = pkgs; p; p = alpm_list_next(p)) 
				{
					ret++;
					package_t *pkg = alpm_list_getdata(p);
					print_aur_package (target, pkg);
				}
				alpm_list_free_inner (pkgs, (alpm_list_fn_free) package_free);
 				alpm_list_free (pkgs);
				pkgs=NULL;
				pkg_json.pkg=NULL;
			}
		}
		else
		{
			string_free (res);
			yajl_free(hand);
			fprintf(stderr, "curl error.\n");
			return 0;
		}
		string_free (res);
		yajl_free(hand);
		if (aur_target)
		{
			free (aur_target);
			aur_target = NULL;
			target = NULL;
		}
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

