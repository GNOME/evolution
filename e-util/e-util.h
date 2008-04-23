/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-util.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _E_UTIL_H_
#define _E_UTIL_H_

#include <sys/types.h>
#include <glib-object.h>
#include <limits.h>
#include <gconf/gconf-client.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <e-util/e-util-marshal.h>

typedef enum {
	E_FOCUS_NONE,
	E_FOCUS_CURRENT,
	E_FOCUS_START,
	E_FOCUS_END
} EFocus;

const gchar *	e_get_user_data_dir		(void);

char *		e_str_without_underscores	(const char *s);
gint		e_str_compare			(gconstpointer x,
						 gconstpointer y);
gint		e_str_case_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_collate_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_int_compare                   (gconstpointer x,
						 gconstpointer y);
gboolean	e_write_file_uri		(const gchar *filename,
						 const gchar *data);

/* This only makes a filename safe for usage as a filename.
 * It still may have shell meta-characters in it. */
gchar *		e_format_number			(gint number);
gchar *		e_format_number_float		(gfloat number);

typedef gint	(*ESortCompareFunc)		(gconstpointer first,
				  		 gconstpointer second,
				  		 gpointer closure);

void		e_bsearch			(gconstpointer key,
						 gconstpointer base,
						 gsize nmemb,
						 gsize size,
						 ESortCompareFunc compare,
						 gpointer closure,
						 gsize *start,
						 gsize *end);

gsize		e_strftime_fix_am_pm		(gchar *str,
						 gsize max,
						 const gchar *fmt,
						 const struct tm *tm);
gsize		e_utf8_strftime_fix_am_pm	(gchar *str,
						 gsize max,
						 const gchar *fmt,
						 const struct tm *tm);
const gchar *	e_get_month_name		(GDateMonth month,
						 gboolean abbreviated);
const gchar *	e_get_weekday_name		(GDateWeekday weekday,
						 gboolean abbreviated);

/* String to/from double conversion functions */
gdouble		e_flexible_strtod		(const gchar *nptr,
						 gchar **endptr);

/* 29 bytes should enough for all possible values that
 * g_ascii_dtostr can produce with the %.17g format.
 * Then add 10 for good measure */
#define E_ASCII_DTOSTR_BUF_SIZE (DBL_DIG + 12 + 10)
gchar *		e_ascii_dtostr			(gchar *buffer,
						 gint buf_len,
						 const gchar *format,
						 gdouble d);

/* Alternating char * and int arguments with a NULL char * to end.
   Less than 0 for the int means copy the whole string. */
gchar    *	e_strdup_append_strings		(gchar *first_string,
						 ...);

cairo_font_options_t * get_font_options		(void);

void		e_file_update_save_path		(gchar *uri,
						 gboolean free);
gchar *		e_file_get_save_path		(void);

gboolean	e_file_lock_create (void);
void		e_file_lock_destroy (void);
gboolean 	e_file_lock_exists (void);

char *e_util_guess_mime_type (const char *filename);
char *e_util_filename_to_uri (const char *filename);
char *e_util_uri_to_filename (const char *uri);

gboolean e_util_read_file  (const char *filename, gboolean filename_is_uri, char **buffer, gsize *read,  GError **error);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_UTIL_H_ */
