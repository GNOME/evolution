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

#ifndef E_UTIL_H
#define E_UTIL_H

#include <sys/types.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <gconf/gconf-client.h>

#include <e-util/e-marshal.h>

/* e_get_user_data_dir() used to live here, so #include its new home
 * for backward-compatibility (not that we really care about that). */
#include <libedataserver/e-data-server-util.h>

/* Convenience macro to help migrate from libglade to GtkBuilder.
 * Use it as a direct replacement for glade_xml_get_widget(). */
#define e_builder_get_widget(builder, name) \
	GTK_WIDGET (gtk_builder_get_object ((builder), (name)))

G_BEGIN_DECLS

typedef enum {
	E_FOCUS_NONE,
	E_FOCUS_CURRENT,
	E_FOCUS_START,
	E_FOCUS_END
} EFocus;

typedef void (*ETypeFunc) (GType type, gpointer user_data);

const gchar *	e_get_gnome2_user_dir		(void);
const gchar *	e_get_accels_filename		(void);
void		e_show_uri			(GtkWindow *parent,
						 const gchar *uri);
void		e_display_help			(GtkWindow *parent,
						 const gchar *link_id);
GtkAction *	e_lookup_action			(GtkUIManager *ui_manager,
						 const gchar *action_name);
GtkActionGroup *e_lookup_action_group		(GtkUIManager *ui_manager,
						 const gchar *group_name);
void		e_load_ui_builder_definition	(GtkBuilder *builder,
						 const gchar *basename);
gint		e_action_compare_by_label	(GtkAction *action1,
						 GtkAction *action2);
void		e_action_group_remove_all_actions
						(GtkActionGroup *action_group);
GtkRadioAction *e_radio_action_get_current_action
						(GtkRadioAction *radio_action);
void		e_categories_add_change_hook	(GHookFunc func,
						 gpointer object);
void		e_type_traverse			(GType parent_type,
						 ETypeFunc func,
						 gpointer user_data);

gchar *		e_str_without_underscores	(const gchar *string);
gint		e_str_compare			(gconstpointer x,
						 gconstpointer y);
gint		e_str_case_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_collate_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_int_compare                   (gconstpointer x,
						 gconstpointer y);
guint32		e_color_to_value		(GdkColor *color);

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

cairo_font_options_t *
		get_font_options		(void);

gboolean	e_file_lock_create		(void);
void		e_file_lock_destroy		(void);
gboolean	e_file_lock_exists		(void);

gchar *		e_util_guess_mime_type		(const gchar *filename,
                                                 gboolean localfile);

GSList *	e_util_get_category_filter_options
						(void);
GList *		e_util_get_searchable_categories(void);

void		e_util_set_source_combo_box_list
						(GtkWidget *source_combo_box,
						 const gchar *source_gconf_path);

G_END_DECLS

#endif /* E_UTIL_H */
