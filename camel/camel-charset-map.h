/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CAMEL_CHARSET_MAP_H
#define _CAMEL_CHARSET_MAP_H

typedef struct _CamelCharset CamelCharset;

struct _CamelCharset {
	unsigned int mask;
	int level;
};

void camel_charset_init(CamelCharset *);
void camel_charset_step(CamelCharset *, const char *in, int len);
const char *camel_charset_best_name(CamelCharset *);

/* helper function */
const char *camel_charset_best(const char *in, int len);

char *camel_charset_locale_name (void);

#endif /* ! _CAMEL_CHARSET_MAP_H */
