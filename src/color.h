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
#ifndef _COLOR_H
#define _COLOR_H

#define C_NB        0
#define C_TESTING   1
#define C_CORE      2
#define C_EXTRA     3
#define C_LOCAL     4
#define C_OTHER     5
#define C_PKG       6
#define C_VER       7
#define C_INSTALLED 8
#define C_LVER      9
#define C_GRP       10
#define C_OD        11
#define C_VOTES     12
#define C_DSC       13
#define C_SPACE     14


const char * color (unsigned int col);
#endif

/* vim: set ts=4 sw=4 noet: */
