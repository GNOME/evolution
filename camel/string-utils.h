/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* string-util : utilities for normal gchar * strings  */

/* 
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *          Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */



#ifndef STRING_UTIL_H
#define STRING_UTIL_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

typedef enum {
	STRING_TRIM_NONE            =     0,
	STRING_TRIM_STRIP_TRAILING  =     1,
	STRING_TRIM_STRIP_LEADING   =     2
} StringTrimOption;



gboolean string_equal_for_glist (gconstpointer v, gconstpointer v2);

void     string_list_free       (GList *string_list);

GList   *string_split           (const gchar *string, char sep,
				 const gchar *trim_chars, StringTrimOption trim_options);
void     string_trim            (gchar *string, const gchar *chars,
				 StringTrimOption options);

gchar   *string_prefix (const gchar *s, const gchar *suffix, gboolean *suffix_found);

gchar   *strstrcase (const gchar *haystack, const gchar *needle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STRING_UTIL_H */
