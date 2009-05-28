/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_UTIL_H_
#define _E_UTIL_H_

#include <sys/types.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <gconf/gconf-client.h>
#include <camel/camel-object.h>
#include <cairo.h>

#include <e-util/e-marshal.h>

G_BEGIN_DECLS

typedef enum {
	E_FOCUS_NONE,
	E_FOCUS_CURRENT,
	E_FOCUS_START,
	E_FOCUS_END
} EFocus;

const gchar *	e_get_user_data_dir		(void);
const gchar *	e_get_accels_filename		(void);
void		e_show_uri			(GtkWindow *parent,
						 const gchar *uri);
void		e_display_help			(GtkWindow *parent,
						 const gchar *link_id);
GtkAction *	e_lookup_action			(GtkUIManager *ui_manager,
						 const gchar *action_name);
GtkActionGroup *e_lookup_action_group		(GtkUIManager *ui_manager,
						 const gchar *group_name);
guint		e_load_ui_definition		(GtkUIManager *ui_manager,
						 const gchar *basename);
gint		e_action_compare_by_label	(GtkAction *action1,
						 GtkAction *action2);
void		e_action_group_remove_all_actions
						(GtkActionGroup *action_group);

gchar *		e_str_without_underscores	(const gchar *s);
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

/* Alternating gchar * and gint arguments with a NULL gchar * to end.
   Less than 0 for the gint means copy the whole string. */
gchar *		e_strdup_append_strings		(gchar *first_string,
						 ...);

cairo_font_options_t *
		get_font_options		(void);

void		e_file_update_save_path		(gchar *uri,
						 gboolean free);
gchar *		e_file_get_save_path		(void);

gboolean	e_file_lock_create		(void);
void		e_file_lock_destroy		(void);
gboolean	e_file_lock_exists		(void);

gchar *		e_util_guess_mime_type		(const gchar *filename, gboolean localfile);
gchar *		e_util_filename_to_uri		(const gchar *filename);
gchar *		e_util_uri_to_filename		(const gchar *uri);

gboolean	e_util_read_file		(const gchar *filename,
						 gboolean filename_is_uri,
						 gchar **buffer,
						 gsize *read,
						 GError **error);

GSList *e_util_get_category_filter_options      (void);

/* Camel uses its own object system, so we have to box
 * CamelObjects to safely use them as GObject properties. */
#define E_TYPE_CAMEL_OBJECT (e_camel_object_get_type ())
GType		e_camel_object_get_type		(void);

G_END_DECLS

#endif /* _E_UTIL_H_ */
