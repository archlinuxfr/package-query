/*
 *  color.c
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
#include "color.h"
#include "util.h"

/* Normal color */
char *colors_1[16] = { "\033[0;1m\033[33;7m", "\033[31;1m", "\033[31;1m", \
  "\033[32;1m", "\033[33;1m", "\033[35;1m", "\033[0;1m", "\033[32;1m", \
  "\033[33;7m", "\033[31;7m", "\033[34;1m", "\033[33;7m", "\033[33;7m", \
  "\033[0;3m", "\033[0;1m", "\033[0m"
};

/* Light background */
char *colors_2[16] = { "\033[0;1m\033[36;7m", "\033[31;1m", "\033[31;1m", \
  "\033[32;1m", "\033[36;1m", "\033[35;1m", "\033[0;1m", "\033[32;1m", \
  "\033[36;7m", "\033[31;7m", "\033[34;1m", "\033[36;7m", "\033[36;7m", \
  "\033[0;3m", "\033[0;1m", "\033[0m"
};

char ** colors[2] = { colors_1, colors_2 };

const char * color (unsigned int col)
{
	if (config.colors)
		return colors[config.colors - 1][col];
	else
		return "";
}



/* vim: set ts=4 sw=4 noet: */
