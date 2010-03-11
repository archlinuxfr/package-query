#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#define AUR_BASE_URL "http://aur.archlinux.org"
#define AUR_RPC	"/rpc.php"
#define AUR_RPC_SEARCH "?type=search&arg="

#define AUR_ID "ID"
#define AUR_NAME "Name"
#define AUR_VER	"Version"
#define AUR_CAT	"CategoryID"
#define AUR_DESC	"Description"
#define AUR_LOC	"LocationID"
#define AUR_URL	"URL"
#define AUR_URLPATH "URLPath"
#define AUR_LICENSE	"License"
#define AUR_VOTE	"NumVotes"
#define AUR_OUT	"OutOfDate"
#define AUR_LAST_ID AUR_OUT
#define AUR_ID_LEN 20

typedef struct _package_t
{
	unsigned int id;
	char *name;
	char *version;
	unsigned int category;
	char *description;
	unsigned int location;
	char *url;
	char *url_path;
	char *license;
	unsigned int votes;
	unsigned short out_of_date;
} package_t;


typedef struct _package_json_t
{
	package_t *pkg;
	char current_key[AUR_ID_LEN];
} package_json_t;


typedef struct _string_t
{
	char *s;
} string_t;

alpm_list_t *pkgs=NULL;
pmdb_t *db_local=NULL;

string_t *string_new ()
{
	string_t *str = NULL;
	if ((str = malloc (sizeof (string_t))) == NULL)
	{
		perror ("malloc");
		exit (1);
	}
	str->s = strdup ("");
	return str;
}

string_t *string_free (string_t *dest)
{
	if (dest == NULL)
		return NULL;
	free (dest->s);
	free (dest);
	return NULL;
}
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
	pkg->description = NULL;
	pkg->location = 0;
	pkg->url = NULL;
	pkg->url_path = NULL;
	pkg->license = NULL;
	pkg->votes = 0;
	pkg->out_of_date = 0;
	return pkg;
}

package_t *package_free (package_t *pkg)
{
	if (pkg == NULL)
		return NULL;
	free (pkg->name);
	free (pkg->version);
	free (pkg->description);
	free (pkg->url);
	free (pkg->url_path);
	free (pkg->license);
	free (pkg);
	return NULL;
}

string_t *string_ncat (string_t *dest, char *src, size_t n)
{
	dest->s = realloc (dest->s, (strlen (dest->s)+1+n) * sizeof (char));
	if (dest->s == NULL)
	{
		perror ("realloc");
		exit (1);
	}

	strncat (dest->s, src, n);
	return dest;
}

void print_format (const char * target, package_t * pkg)
{
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
	char *info;
	int free_info = 0;
	format_cpy = strdup ("%t: %n %v [%l] (%g)");
	ptr = format_cpy;
	end = &(format_cpy[strlen(format_cpy)]);
	while (c=strchr (ptr, '%'))
	{
		if (&(c[1])==end)
			break;
		c[0] = '\0'; 
		if (c[1] == '%' )
		{
			printf ("%s\%\%", ptr);
		}
		else
		{
			free_info = 0;
			info = NULL;
			switch (c[1])
			{
				case 'd': info = pkg->description; break;
				case 'n': info = pkg->name; break;
				case 'g': info = strdup ("aur");
					free_info = 1;
					break;
				case 'v': info = pkg->version; break;
				case 'l': 
					if (db_local == NULL)
						db_local = alpm_db_register_local();
					pmpkg_t *pkg_local = alpm_db_get_pkg(db_local, pkg->name);
					if (pkg_local != NULL)
					info = (char *) alpm_pkg_get_version (pkg_local); 
					break;
				case 'r': info = strdup ("aur");
					free_info = 1;
					break;
				case 't': info = (char *) target; break;
				default: ;
			}
			if (info)
				printf ("%s%s", ptr, info);
			else
				printf ("%s-", ptr);
			if (free_info)
				free (info);
		}
		ptr = &(c[2]);
	}
	if (ptr != end)
		printf ("%s", ptr);
	printf ("\n");
}


int curl_getdata_cb (void *data, size_t size, size_t nmemb, void *userdata)
{
	string_t *s = (string_t *) userdata;
	string_ncat (s, data, nmemb);
	return nmemb;
}


static int reformat_map_key(void * ctx, const unsigned char * stringVal,
                            unsigned int stringLen)
{
	package_json_t *pkg = (package_json_t *) ctx;
	strncpy (pkg->current_key, stringVal, stringLen);
	pkg->current_key[stringLen] = '\0';
    return 1;
}


static int reformat_string(void * ctx, const unsigned char * stringVal,
                           unsigned int stringLen)
{
	char *s = strndup (stringVal, stringLen);
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
		pkg->pkg->description = s;
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
		pkg->pkg->url_path = s;
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
		pkg->pkg->out_of_date = atoi(s);
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
    reformat_string,
    NULL,
    reformat_map_key,
    NULL,
    NULL,
    NULL,
};

void usage ()
{
	fprintf(stderr, "Query AUR by RPC interface\n");
	fprintf(stderr, "Usage: %s [options] [targets ...]\n", "aur-query");
	fprintf(stderr, "\n");
	exit (1);
}
	
int main (int argc, char **argv)
{
	CURL *curl;
	yajl_handle hand;
	yajl_status stat;
	yajl_parser_config cfg = { 1, 1 };
	package_json_t pkg_json = { NULL, "" };
	char aur_rpc[PATH_MAX];
	string_t *res;
	alpm_list_t *targets=NULL;
	alpm_list_t *t;
	int i;
	for (i = 1; i < argc; i++)
	{
		targets = alpm_list_add(targets, strdup(argv[i]));
	}
	if (targets == NULL)
	{
		fprintf(stderr, "no targets specified.\n");
		usage();
	}


	curl = curl_easy_init ();
	if (!curl)
	{
		perror ("curl");
		exit(1);
	}
	strcpy (aur_rpc, AUR_BASE_URL);
	strcat (aur_rpc, AUR_RPC);
	strcat (aur_rpc, AUR_RPC_SEARCH);
    hand = yajl_alloc(&callbacks, &cfg,  NULL, (void *) &pkg_json);
	
	for(t = targets; t; t = alpm_list_next(t)) 
	{
		aur_rpc[strlen(AUR_BASE_URL) + strlen(AUR_RPC) + strlen(AUR_RPC_SEARCH)] = '\0';
		strcat (aur_rpc, alpm_list_getdata(t));
		res = string_new();
		curl_easy_setopt (curl, CURLOPT_WRITEDATA, res);
		curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_getdata_cb);
		curl_easy_setopt (curl, CURLOPT_URL, aur_rpc);
		if (curl_easy_perform (curl) == CURLE_OK)
		{
			stat = yajl_parse(hand, res->s, strlen (res->s));
			if (stat != yajl_status_ok && stat != yajl_status_insufficient_data)
			{
				unsigned char * str = yajl_get_error(hand, 1, res->s, strlen (res->s));
				fprintf(stderr, (const char *) str);
				yajl_free_error(hand, str);
			}
			else
			{
 				alpm_list_t *p;
				for(p = pkgs; p; p = alpm_list_next(p)) 
				{
					package_t *pkg = alpm_list_getdata(p);
					print_format (alpm_list_getdata(t), pkg);
				}
				alpm_list_free_inner (pkgs, (alpm_list_fn_free) package_free);
 				FREELIST (pkgs);
				free (pkg_json.pkg);
			}
		}
		else
		{
			fprintf(stderr, "curl error.\n");
		}
	}



    yajl_free(hand);
	alpm_db_unregister_all();
 	FREELIST (targets);
	string_free (res);
	return 0;
}

	
	
