/*
 *  util.c
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
#include <stdio.h>
#include <alpm.h>
#include <alpm_list.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "util.h"
#include "alpm-query.h"


target_t *target_parse (const char *str)
{
	target_t *ret=NULL;
	char *c, *s=(char *)str;
	if ((ret = malloc (sizeof (target_t))) == NULL)
	{
		perror ("malloc");
		exit (1);
	}

	if ((c = strchr (s, '/')) != NULL)
	{
		/* target include db ("db/pkg*") */
		ret->db = strndup (s, (c-s) / sizeof(char));
		s = ++c;
	}
	else 
		ret->db = NULL;

	if ((c=strstr (s, "<=")) != NULL)
	{
		ret->mod=PM_DEP_MOD_LE;
		ret->ver=strdup (&(c[2]));
	}
	else if ((c=strstr (s, ">=")) != NULL)
	{
		ret->mod=PM_DEP_MOD_GE;
		ret->ver=strdup (&(c[2]));
	}
	else if ((c=strchr (s, '<')) != NULL)
	{
		ret->mod=PM_DEP_MOD_LT;
		ret->ver=strdup (&(c[1]));
	}
	else if ((c=strchr (s, '>')) != NULL)
	{
		ret->mod=PM_DEP_MOD_GT;
		ret->ver=strdup (&(c[1]));
	}
	else if ((c=strchr (s, '=')) != NULL)
	{
		ret->mod=PM_DEP_MOD_EQ;
		ret->ver=strdup (&(c[1]));
	}
	else
	{
		ret->mod=PM_DEP_MOD_ANY;
		ret->ver=NULL;
	}
	if (c)
		ret->name=strndup (s, (c-s) / sizeof(char));
	else
		ret->name=strdup (s);
	return ret;
}

target_t* target_free (target_t *t)
{
	if (t == NULL)
		return NULL;
	free (t->db);
	free (t->name);
	free (t->ver);
	free (t);
	return NULL;
}
	
int target_check_version (target_t *t, const char *ver)
{
	int ret;
	if (t->mod==PM_DEP_MOD_ANY) return 1;
	ret = alpm_pkg_vercmp (ver, t->ver);
	switch (t->mod)
	{
		case PM_DEP_MOD_LE: return (ret<=0);
		case PM_DEP_MOD_GE: return (ret>=0);
		case PM_DEP_MOD_LT: return (ret<0);
		case PM_DEP_MOD_GT: return (ret>0);
		case PM_DEP_MOD_EQ: return (ret==0);
		default: return 1;
	}
}

int target_compatible (target_t *t1, target_t *t2)
{
	if (t2->mod != PM_DEP_MOD_EQ && t2->mod != PM_DEP_MOD_ANY)
		return 0;
	if (strcmp (t1->name, t2->name) == 0 &&
		(t1->mod == PM_DEP_MOD_ANY || t2->mod == PM_DEP_MOD_ANY ||
		target_check_version (t1, t2->ver)))
			return 1;
	return 0;
}

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
		return NULL;
	}
	else
	{
		for(i = l; i; i = alpm_list_next(i)) 
		{
			/* data's len + space for separator */
			len += strlen (alpm_list_getdata (i)) + strlen (config.csep);
		}
		if (len)
		{
			len++; /* '\0' at the end */
			ret = (char *) malloc (sizeof (char) * len);
			strcpy (ret, "");
			for(i = l; i; i = alpm_list_next(i)) 
			{
				strcat (ret, alpm_list_getdata (i));
				strcat (ret, config.csep);
			}
		}
		else
		{
			return NULL;
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

char * ltostr (long i)
{
	char is[20];
	sprintf (is, "%ld", i);
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

int getcols(void)
{
	if(!isatty(1)) {
		/* We will default to 80 columns if we're not a tty
		 * this seems a fairly standard file width.
		 */
		return 80;
	} else {
#ifdef TIOCGSIZE
		struct ttysize win;
		if(ioctl(1, TIOCGSIZE, &win) == 0) {
			return win.ts_cols;
		}
#elif defined(TIOCGWINSZ)
		struct winsize win;
		if(ioctl(1, TIOCGWINSZ, &win) == 0) {
			return win.ws_col;
		}
#endif
		/* If we can't figure anything out, we'll just assume 80 columns */
		/* TODO any problems caused by this assumption? */
		return 80;
	}
	/* Original envvar way - prone to display issues
	const char *cenv = getenv("COLUMNS");
	if(cenv != NULL) {
		return atoi(cenv);
	}
	return -1;
	*/
}

void indent (const char *str)
{
	char *s=NULL;
	char *c=NULL, *c1=NULL;
	int cur_col=4, cols;
	if (!str)
		return;
	s=strdup (str);
	c=s;
	cols=getcols();
	printf ("%-*s", 4, "");
	while ((c1=strchr (c, ' ')) != NULL)
	{
		c1[0]='\0';
		cur_col+=strlen (c)+1;
		if (cur_col >= cols)
		{
			printf ("\n%-*s%s ", 4, "", c);
			cur_col=4+strlen(c)+1;
		}
		else
			printf ("%s ", c);
		c=&c1[1];
	}
	cur_col+=strlen (c);
	if (cur_col >= cols && c!=s)
		printf ("\n%-*s%s\n", 4, "", c);
	else
		printf ("%s\n", c);
	free (s);
}

void yaourt_print_package (void * p, const char *(*f)(void *p, unsigned char c))
{
	static char cstr[PATH_MAX];
	const char *info, *lver;
	char **colors=config.colors;
	cstr[0]='\0';
	info = f(p, 's');
	fprintf (stderr, "%s/", info);
	if (strcmp (info, "testing")==0 || strcmp (info, "core")==0)
		strcpy (cstr, colors[4]);
	else if (strcmp (info, "local")==0)
		strcpy (cstr, colors[5]);
	else if (strcmp (info, "extra")==0)
		strcpy (cstr, colors[6]);
	else if (strcmp (info, "community")==0)
		strcpy (cstr, colors[7]);
	else
		strcpy (cstr, colors[7]);
	strcat (cstr, info);
	strcat (cstr, "/"); strcat (cstr, colors[0]); strcat (cstr, colors[1]);
	info=f(p, 'n');
	fprintf (stderr, "%s\n", info);
	strcat (cstr, info);
	strcat (cstr, " "); strcat (cstr, colors[6]);
	lver = alpm_local_pkg_get_str (info, 'l');
	info = f(p, 'v');
	strcat (cstr, info); strcat (cstr, colors[0]);
	if (lver && strcmp (f(p, 'r'), "local")!=0)
	{
		strcat (cstr, " "); strcat (cstr, colors[3]); strcat (cstr, colors[5]); strcat (cstr, "[");
		if (strcmp (info, lver)!=0)
		{
			strcat (cstr, colors[4]);
			strcat (cstr, lver);
			strcat (cstr, colors[5]); strcat (cstr, " ");
		}
		strcat (cstr, "installed]"); strcat (cstr, colors[0]);
	}
	info = f(p, 'g');
	if (info)
	{
		strcat (cstr, " "); strcat (cstr, colors[8]);
		strcat (cstr, "("); strcat (cstr, info);
		strcat (cstr, ")"); strcat (cstr, colors[0]);
	}
	info = f(p, 'o');
	if (info && info[0]=='1')
	{
		strcat (cstr, " "); strcat (cstr, colors[3]); strcat (cstr, colors[5]);
		strcat (cstr, "(Out of date)"); strcat (cstr, colors[0]);
	}
	info = f(p, 'w');
	if (info)
	{
		strcat (cstr, " ");strcat (cstr, colors[3]); strcat (cstr, colors[5]);
		strcat (cstr, "("); strcat (cstr, info); strcat (cstr, ")");
		strcat (cstr, colors[0]);
	}
	printf ("%s\n", cstr);
	if (!config.db_sync) return;
	printf ("%s", colors[2]);
	indent (f(p, 'd'));
	printf ("%s", colors[2]);
}

void print_package (const char * target, 
	void * pkg, const char *(*f)(void *p, unsigned char c))
{
	if (config.yaourt) {yaourt_print_package (pkg, f); return; }
	if (config.quiet) return;
	char *format_cpy;
	char *ptr;
	char *end;
	char *c;
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
			if (strchr ("l1234", c[1]))
				info = alpm_local_pkg_get_str (f(pkg, 'n'), c[1]);
			else if (c[1]=='t')
				info = target; 
			else
				info = f (pkg, c[1]);
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
