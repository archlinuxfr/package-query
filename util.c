#include <stdio.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include "util.h"


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
			/* data's len + space for separator */
			len += strlen (alpm_list_getdata (i)) + 1;
		}
		if (len)
		{
			len++; /* '\0' at the end */
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

void print_escape (const char *str)
{
	const char *c=str;
	while (*c!='\0')
	{
		if (*c == '"')
			putchar ('\\');
		putchar (*c);
		c++;
	}
}

char * itostr (int i)
{
	char is[20];
	sprintf (is, "%d", i);
	return strdup (is);
}
/* Helper function for strreplace */
static void _strnadd(char **str, const char *append, unsigned int count)
{
	if(*str) {
		*str = realloc(*str, strlen(*str) + count + 1);
	} else {
		*str = calloc(sizeof(char), count + 1);
	}

	strncat(*str, append, count);
}

/* Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd) */
char *strreplace(const char *str, const char *needle, const char *replace)
{
	const char *p, *q;
	p = q = str;

	char *newstr = NULL;
	unsigned int needlesz = strlen(needle),
							 replacesz = strlen(replace);

	while (1) {
		q = strstr(p, needle);
		if(!q) { /* not found */
			if(*p) {
				/* add the rest of 'p' */
				_strnadd(&newstr, p, strlen(p));
			}
			break;
		} else { /* found match */
			if(q > p){
				/* add chars between this occurance and last occurance, if any */
				_strnadd(&newstr, p, q - p);
			}
			_strnadd(&newstr, replace, replacesz);
			p = q + needlesz;
		}
	}

	return newstr;
}


void print_package (const char * target, int query, 
	void * pkg, const char *(*f)(void *p, unsigned char c))
{
	if (config.quiet) return;
	pmpkg_t * pkg_local;
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
	char q[8] = {'d','\0', 'c', '\0', 'p', '\0', 'r', '\0'};
	const char *info;
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
			info = NULL;
			switch (c[1])
			{
				case 'l': 
					pkg_local = alpm_db_get_pkg(alpm_option_get_localdb(), f (pkg, 'n'));
					if (pkg_local)
						info = (const char *) alpm_pkg_get_version (pkg_local); break;
				case 'q': if (query) info = &q[(query-1)*2]; break;
				case 't': info = (char *) target; break;
				default: info = f (pkg, c[1]);
			}
			printf ("%s", ptr);
			if (info)
			{
				if (config.escape)
					print_escape (info);
				else
					printf ("%s", info);
			}
			else
				printf ("-");
		}
		ptr = &(c[2]);
	}
	if (ptr != end)
		printf ("%s", ptr);
	free (format_cpy);
	printf ("\n");
	fflush (NULL);
		
}
