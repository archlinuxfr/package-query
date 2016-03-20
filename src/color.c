/*
 *  color.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "color.h"
#include "util.h"

#define COLOR_ENV_VAR  "PQ_COLORS"
#define DEFAULT_COLORS "no=0:nb=1;33;7:pkg=1:ver=1;32:lver=1;31;7:installed=1;33;7:grp=1;34:od=1;33;7:votes=1;33;7:popularity=1;33;7:dsc=0:other=1;35:testing=1;31:core=1;31:extra=1;32:local=1;33:orphan=1;31"
#define COL_ID(x)      (x >= 'a' && x <= 'z')
#define COL_VAL(x)     ((x >= '0' && x <= '9') || x == ';')
#define COLOR          "0123456789;"

typedef struct _colors_t
{
	char *id;
	char *color;
} colors_t;

static alpm_list_t *colors = NULL;

static void colors_free (colors_t *c)
{
	FREE (c->id);
	FREE (c->color);
	FREE (c);
}

static int colors_cmp (const void *c1, const void *c2)
{
	if (!c1 || !c2) {
		return 0;
	}

	const char *c1id = ((colors_t *) c1)->id;
	const char *c2id = ((colors_t *) c2)->id;
	if (!c1id || !c2id) {
		return 0;
	}

	return strcmp (c1id, c2id);
}

static int colors_cmp_id (const void *c1, const void *id)
{
	if (!c1 || !id) {
		return 0;
	}

	const char *c1id = ((colors_t *) c1)->id;
	if (!c1id) {
		return -1;
	}

	return strcmp (c1id, (char *) id);
}

static void colors_set_color (const char *id, const char *color)
{
	colors_t *c = alpm_list_find (colors, id, colors_cmp_id);
	if (c) {
		FREE (c->color);
	} else {
		MALLOC (c, sizeof (colors_t));
		c->id = strdup (id);
		colors = alpm_list_add_sorted (colors, c, colors_cmp);
	}
	CALLOC (c->color, (strlen (color) + 4), sizeof (char)); /* + "\033[m" */
	sprintf (c->color, "\033[%sm", color);
}

static void parse_var (const char *s)
{
	if (!s) {
		return;
	}

	char *src = strdup (s);
	char *t = src;
	char *sid = t;
	char *sval = NULL;
	while (*t != '\0') {
		if (!sval && sid != t && *t == '=') {
			*t = '\0';
			sval = t + sizeof (char);
		} else if (sval && sval != t && *t == ':') {
			*t = '\0';
			colors_set_color (sid, sval);
			sid = t + sizeof (char);
			sval = NULL;
		} else if ((!sval && !COL_ID(*t)) || (sval && !COL_VAL(*t))) {
			sid = t + sizeof (char);
			sval = NULL;
		}
		t++;
	}
	if (sval && sval != t) {
		colors_set_color (sid, sval);
	}
	free (src);
}

void color_init (void)
{
	parse_var (DEFAULT_COLORS);
	parse_var (getenv (COLOR_ENV_VAR));
}

void color_cleanup (void)
{
	if (colors) {
		alpm_list_free_inner (colors, (alpm_list_fn_free) colors_free);
		alpm_list_free (colors);
	}
}

const char *color (const char *col)
{
	if (config.colors) {
		const colors_t *c = alpm_list_find (colors, col, (alpm_list_fn_cmp) colors_cmp_id);
		if (c) {
			return c->color;
		}
	}
	return "";
}

const char *color_repo (const char *repo)
{
	const char *res = color (repo);
	if (*res != '\0') {
		return res;
	}

	return color (C_OTHER);
}

/* vim: set ts=4 sw=4 noet: */
