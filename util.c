#include <stdio.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "aur.h"
#include "alpm-query.h"


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




char *strtrim(char *str)
{
	char *pch = str;

	if(str == NULL || *str == '\0') {
		/* string is empty, so we're done. */
		return(str);
	}

	while(isspace(*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}

	/* check if there wasn't anything but whitespace in the string. */
	if(*str == '\0') {
		return(str);
	}

	pch = (str + (strlen(str) - 1));
	while(isspace(*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

void strins (char *dest, const char *src)
{
	char *prov;
	prov = malloc (sizeof (char) * (strlen (dest) + strlen (src) + 1));
	strcpy (prov, src);
	strcat (prov, dest);
	strcpy (dest, prov);
	free (prov);
}

char *concat_str_list (alpm_list_t *l)
{
	char *ret;
	int len=0;
	alpm_list_t *i;
	if (!l)
	{
		ret = (char *) malloc (sizeof (char) * 2);
		strcpy (ret, "-");
	}
	else
	{
		for(i = l; i; i = alpm_list_next(i)) 
		{
			len += strlen (alpm_list_getdata (i)) + 1;
		}
		if (len)
		{
			ret = (char *) malloc (sizeof (char) * len);
			strcpy (ret, "");
			for(i = l; i; i = alpm_list_next(i)) 
			{
				strcat (ret, alpm_list_getdata (i));
				strcat (ret, " ");
			}
		}
		else
		{
			ret = (char *) malloc (sizeof (char) * 2);
			strcpy (ret, "+");			
		}
	}
	strtrim (ret);
	return ret;
}


void print_aur_package (const char * target, package_t * pkg_sync)
{
	if (config.quiet) return;
	pmpkg_t * pkg_local;
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
	char *info, itostr[20];
	int free_info = 0;
	format_cpy = strdup (config.format_out);
	ptr = format_cpy;
	end = &(format_cpy[strlen(format_cpy)]);
	while ((c=strchr (ptr, '%')))
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
				case 'd': info = (char *) aur_pkg_get_desc (pkg_sync); break;
				case 'i': 
					sprintf (itostr, "%d", aur_pkg_get_id (pkg_sync));
					info = itostr; break;
				case 'l': 
					pkg_local = alpm_db_get_pkg(alpm_option_get_localdb(), aur_pkg_get_name (pkg_sync));
					if (pkg_local)
						info = (char *) alpm_pkg_get_version (pkg_local); break;
				case 'n': info = (char *) aur_pkg_get_name (pkg_sync); break;
				case 'o': 
					sprintf (itostr, "%d", aur_pkg_get_outofdate (pkg_sync));
					info = itostr; break;
				case 'r': info = strdup ("aur"); free_info=1; break;
				case 't': info = (char *) target; break;
				case 'u': 
					info = (char *) malloc (sizeof (char) * (strlen (AUR_BASE_URL) + strlen (aur_pkg_get_urlpath (pkg_sync))));
					strcpy (info, AUR_BASE_URL);
					strcat (info, aur_pkg_get_urlpath (pkg_sync));
					free_info = 1;
					break;
				case 'v': info = (char *) aur_pkg_get_version (pkg_sync); break;
				case 'w': 
					sprintf (itostr, "%d", aur_pkg_get_votes (pkg_sync));
					info = itostr; break;
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



void print_package (const char * target, int query, pmpkg_t * pkg_sync)
{
	if (config.quiet) return;
	pmpkg_t * pkg_local;
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
	char q[8] = {'d','\0', 'c', '\0', 'p', '\0', 'r', '\0'};
	char *info;
	int free_info = 0;
	format_cpy = strdup (config.format_out);
	ptr = format_cpy;
	end = &(format_cpy[strlen(format_cpy)]);
	while ((c=strchr (ptr, '%')))
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
				case 'd': info = (char *) alpm_pkg_get_desc (pkg_sync); break;
				case 'g':
					info = concat_str_list (alpm_pkg_get_groups (pkg_sync)); 
					free_info = 1;
					break;
				case 'l': 
					pkg_local = alpm_db_get_pkg(alpm_option_get_localdb(), alpm_pkg_get_name (pkg_sync));
					if (pkg_local)
						info = (char *) alpm_pkg_get_version (pkg_local); break;
				case 'n': info = (char *) alpm_pkg_get_name (pkg_sync); break;
				case 'q': if (query) info = &q[(query-1)*2]; break;
				case 'r': info = (char *) alpm_db_get_name (alpm_pkg_get_db (pkg_sync)); break;
				case 't': info = (char *) target; break;
				case 'v': info = (char *) alpm_pkg_get_version (pkg_sync); break;
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
