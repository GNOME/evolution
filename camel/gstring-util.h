/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gstring-util : utilities for gstring object  */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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



#ifndef GSTRING_UTIL_H
#define GSTRING_UTIL_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

typedef enum {
    DICHOTOMY_NONE            =     0,
    DICHOTOMY_RIGHT_DIR       =     1,
    DICHOTOMY_STRIP_TRAILING  =     2,
    DICHOTOMY_STRIP_LEADING   =     4,
    
} DichotomyOption;

typedef enum {
    TRIM_NONE            =     0,
    TRIM_STRIP_TRAILING  =     1,
    TRIM_STRIP_LEADING   =     2,
    
} TrimOption;


gboolean g_string_equals(GString *string1, GString *string2);
GString *g_string_clone(GString *string);
gchar g_string_dichotomy( GString *string, gchar sep, GString **prefix, GString **suffix, DichotomyOption options);
void g_string_append_g_string(GString *dest_string, GString *other_string);

gboolean g_string_equal_for_hash (gconstpointer v, gconstpointer v2);
gboolean g_string_equal_for_glist (gconstpointer v, gconstpointer v2);
guint g_string_hash (gconstpointer v);
void g_string_list_free (GList *string_list);

GList *g_string_split (GString *string, char sep, gchar *trim_chars, TrimOption trim_options);
void g_string_trim (GString *string, gchar *chars, TrimOption options);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GSTRING_UTIL_H */
