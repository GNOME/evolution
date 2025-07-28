/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MISC_UTILS_H
#define E_MISC_UTILS_H

#include <sys/types.h>
#include <gtk/gtk.h>
#include <limits.h>

#include <libedataserver/libedataserver.h>

#include <e-util/e-marshal.h>
#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

typedef enum {
	E_FOCUS_NONE,
	E_FOCUS_CURRENT,
	E_FOCUS_START,
	E_FOCUS_END
} EFocus;

typedef enum {
	E_RESTORE_WINDOW_SIZE = 1 << 0,
	E_RESTORE_WINDOW_POSITION = 1 << 1
} ERestoreWindowFlags;

typedef void	(*EForeachFunc)			(gint model_row,
						 gpointer closure);

void		e_show_uri			(GtkWindow *parent,
						 const gchar *uri);
void		e_display_help			(GtkWindow *parent,
						 const gchar *link_id);
void		e_restore_window		(GtkWindow *window,
						 const gchar *settings_path,
						 ERestoreWindowFlags flags);
GtkWidget *	e_builder_get_widget		(GtkBuilder *builder,
						 const gchar *widget_name);
void		e_load_ui_builder_definition	(GtkBuilder *builder,
						 const gchar *basename);
void		e_categories_add_change_hook	(GHookFunc func,
						 gpointer object);

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

gchar *		e_str_without_underscores	(const gchar *string);
GString *	e_str_replace_string		(const gchar *text,
						 const gchar *find,
						 const gchar *replace);
gboolean	e_str_is_empty			(const gchar *value);
gint		e_str_compare			(gconstpointer x,
						 gconstpointer y);
gint		e_str_case_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_collate_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_int_compare                   (gconstpointer x,
						 gconstpointer y);

guint32		e_rgba_to_value			(const GdkRGBA *rgba);

void		e_utils_get_theme_color		(GtkWidget *widget,
						 const gchar *color_names,
						 const gchar *fallback_color_ident,
						 GdkRGBA *rgba);

#define E_UTILS_LIGHTNESS_MULT	1.3
#define E_UTILS_DARKNESS_MULT	0.7
#define E_UTILS_DEFAULT_THEME_BG_COLOR				"#AAAAAA"
#define E_UTILS_DEFAULT_THEME_BASE_COLOR			"#FFFFFF"
#define E_UTILS_DEFAULT_THEME_FG_COLOR				"#000000"
#define E_UTILS_DEFAULT_THEME_TEXT_COLOR			"#000000"
#define E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR			"#729fcf"
#define E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR			"#000000"
#define E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR	"#808080"
#define E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_FG_COLOR	"#000000"

void		e_utils_shade_color		(const GdkRGBA *a,
						 GdkRGBA *b,
						 gdouble mult);
gdouble		e_utils_get_color_brightness	(const GdkRGBA *rgba);
GdkRGBA		e_utils_get_text_color_for_background
						(const GdkRGBA *bg_rgba);

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
gsize		e_utf8_strftime_match_lc_messages
						(gchar *string,
						 gsize max,
						 const gchar *fmt,
						 const struct tm *tm);
const gchar *	e_get_month_name		(GDateMonth month,
						 gboolean abbreviated);
const gchar *	e_get_weekday_name		(GDateWeekday weekday,
						 gboolean abbreviated);
GDateWeekday	e_weekday_get_next		(GDateWeekday weekday);
GDateWeekday	e_weekday_get_prev		(GDateWeekday weekday);
GDateWeekday	e_weekday_add_days		(GDateWeekday weekday,
						 guint n_days);
guint		e_weekday_get_days_between	(GDateWeekday weekday1,
						 GDateWeekday weekday2);
gint		e_weekday_to_tm_wday		(GDateWeekday weekday);
GDateWeekday	e_weekday_from_tm_wday		(gint tm_wday);

gboolean	e_file_lock_create		(void);
void		e_file_lock_destroy		(void);
gboolean	e_file_lock_exists		(void);
GPid		e_file_lock_get_pid		(void);

gchar *		e_util_guess_mime_type		(const gchar *filename,
                                                 gboolean localfile);

GSList *	e_util_get_category_filter_options
						(void);
GList *		e_util_dup_searchable_categories (void);

gboolean	e_util_get_open_source_job_info	(const gchar *extension_name,
						 const gchar *source_display_name,
						 gchar **description,
						 gchar **alert_ident,
						 gchar **alert_arg_0);
struct _EAlertSinkThreadJobData;
void		e_util_propagate_open_source_job_error
						(struct _EAlertSinkThreadJobData *job_data,
						 const gchar *extension_name,
						 GError *local_error,
						 GError **error);
struct _EClientCache;
EClient *	e_util_open_client_sync		(struct _EAlertSinkThreadJobData *job_data,
						 struct _EClientCache *client_cache,
						 const gchar *extension_name,
						 ESource *source,
						 guint32 wait_for_connected_seconds,
						 GCancellable *cancellable,
						 GError **error);

/* Useful GBinding transform functions */
gboolean	e_binding_transform_rgba_to_string
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer not_used);
gboolean	e_binding_transform_string_to_rgba
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer not_used);

gboolean	e_binding_transform_text_non_null
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer user_data);
gboolean	e_binding_transform_text_to_uri	(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer not_used);
gboolean	e_binding_transform_uri_to_text	(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer not_used);

GBinding *	e_binding_bind_object_text_property
						(gpointer source,
						 const gchar *source_property,
						 gpointer target,
						 const gchar *target_property,
						 GBindingFlags flags);

gulong		e_signal_connect_notify		(gpointer instance,
						 const gchar *notify_name,
						 GCallback c_handler,
						 gpointer user_data);
gulong		e_signal_connect_notify_after	(gpointer instance,
						 const gchar *notify_name,
						 GCallback c_handler,
						 gpointer user_data);
gulong		e_signal_connect_notify_swapped	(gpointer instance,
						 const gchar *notify_name,
						 GCallback c_handler,
						 gpointer user_data);
gulong		e_signal_connect_notify_object	(gpointer instance,
						 const gchar *notify_name,
						 GCallback c_handler,
						 gpointer gobject,
						 GConnectFlags connect_flags);
void		e_signal_disconnect_notify_handler
						(gpointer instance,
						 gulong *handler_id);

GSettings *	e_util_ref_settings		(const gchar *schema_id);
void		e_util_cleanup_settings		(void);
GdkPixbuf *	e_misc_util_ref_pixbuf		(const gchar *filename,
						 GError **error);
gboolean	e_util_prompt_user		(GtkWindow *parent,
						 const gchar *settings_schema,
						 const gchar *promptkey,
						 const gchar *tag,
						 ...);
gboolean	e_util_is_running_gnome		(void);
gboolean	e_util_is_running_flatpak	(void);
void		e_util_set_entry_issue_hint	(GtkWidget *entry,
						 const gchar *hint);

void		e_util_init_main_thread		(GThread *thread);
gboolean	e_util_is_main_thread		(GThread *thread);
gchar *		e_util_save_image_from_clipboard
						(GtkClipboard *clipboard);
void		e_util_save_file_chooser_folder	(GtkFileChooser *file_chooser);
void		e_util_load_file_chooser_folder	(GtkFileChooser *file_chooser);
gboolean	e_util_get_webkit_developer_mode_enabled
						(void);
gchar *		e_util_next_uri_from_uri_list	(guchar **uri_list,
						 gint *len,
						 gint *list_len);
void		e_util_resize_window_for_screen	(GtkWindow *window,
						 gint window_width,
						 gint window_height,
						 const GSList *children); /* GtkWidget * */
gboolean	e_util_query_ldap_root_dse_sync	(const gchar *host,
						 guint16 port,
						 ESourceLDAPSecurity security,
						 gchar ***out_root_dse,
						 GCancellable *cancellable,
						 GError **error);
gchar *		e_util_get_uri_tooltip		(const gchar *uri);
gchar *		e_util_get_language_name	(const gchar *language_tag);
gboolean	e_util_get_language_info	(const gchar *language_tag,
						 gchar **out_language_name,
						 gchar **out_country_name);
void		e_misc_util_free_global_memory	(void);
gboolean	e_util_can_preview_filename	(const gchar *filename);
void		e_util_markup_append_escaped	(GString *buffer,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (2, 3);
void		e_util_markup_append_escaped_text
						(GString *buffer,
						 const gchar *text);

typedef struct _ESupportedLocales {
	const gchar *code;	/* like 'en' */
	const gchar *locale;	/* like 'en_US' */
} ESupportedLocales;

void		e_util_enum_supported_locales	(void);
const ESupportedLocales *
		e_util_get_supported_locales	(void);

void		e_util_ensure_scrolled_window_height
						(GtkScrolledWindow *scrolled_window);
void		e_util_make_safe_filename	(gchar *filename);
gboolean	e_util_setup_toolbar_icon_size	(GtkToolbar *toolbar,
						 GtkIconSize default_size);
gboolean	e_util_get_use_header_bar	(void);
void		e_open_map_uri			(GtkWindow *parent,
						 const gchar *location);
gboolean	e_util_link_requires_reference	(const gchar *href,
						 const gchar *text);
void		e_util_call_malloc_trim_limited	(void);
void		e_util_connect_menu_detach_after_deactivate
						(GtkMenu *menu);
gboolean	e_util_ignore_accel_for_focused	(GtkWidget *focused);
gboolean	e_util_is_dark_theme		(GtkWidget *widget);

G_END_DECLS

#endif /* E_MISC_UTILS_H */
