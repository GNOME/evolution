/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _CAMEL_CHARSET_MAP_H
#define _CAMEL_CHARSET_MAP_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _CamelCharset CamelCharset;

struct _CamelCharset {
	unsigned int mask;
	int level;
};

void camel_charset_init(CamelCharset *);
void camel_charset_step(CamelCharset *, const char *in, int len);

const char *camel_charset_best_name (CamelCharset *);

/* helper function */
const char *camel_charset_best(const char *in, int len);

const char *camel_charset_iso_to_windows (const char *isocharset);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_CHARSET_MAP_H */
