/*
 *  color.h
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
#ifndef _PQ_COLOR_H
#define _PQ_COLOR_H

#define C_NB        "nb"
#define C_OTHER     "other"
#define C_PKG       "pkg"
#define C_VER       "ver"
#define C_INSTALLED "installed"
#define C_LVER      "lver"
#define C_GRP       "grp"
#define C_OD        "od"
#define C_VOTES     "votes"
#define C_DSC       "dsc"
#define C_ORPHAN    "orphan"
#define C_NO        "no"

void color_init (void);
void color_cleanup (void);
const char * color (const char* col);
const char * color_repo (const char *repo);

#endif

/* vim: set ts=4 sw=4 noet: */
