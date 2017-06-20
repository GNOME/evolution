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

const gchar *	e_get_accels_filename		(void);
void		e_show_uri			(GtkWindow *parent,
						 const gchar *uri);
void		e_display_help			(GtkWindow *parent,
						 const gchar *link_id);
void		e_restore_window		(GtkWindow *window,
						 const gchar *settings_path,
						 ERestoreWindowFlags flags);
GtkAction *	e_lookup_action			(GtkUIManager *ui_manager,
						 const gchar *action_name);
GtkActionGroup *e_lookup_action_group		(GtkUIManager *ui_manager,
						 const gchar *group_name);
gint		e_action_compare_by_label	(GtkAction *action1,
						 GtkAction *action2);
void		e_action_group_remove_all_actions
						(GtkActionGroup *action_group);
GtkRadioAction *e_radio_action_get_current_action
						(GtkRadioAction *radio_action);
void		e_action_group_add_actions_localized
						(GtkActionGroup *action_group,
						 const gchar *translation_domain,
						 const GtkActionEntry *entries,
						 guint n_entries,
						 gpointer user_data);
GtkWidget *	e_builder_get_widget		(GtkBuilder *builder,
						 const gchar *widget_name);
void		e_load_ui_builder_definition	(GtkBuilder *builder,
						 const gchar *basename);
guint		e_load_ui_manager_definition	(GtkUIManager *ui_manager,
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
gint		e_str_compare			(gconstpointer x,
						 gconstpointer y);
gint		e_str_case_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_collate_compare		(gconstpointer x,
						 gconstpointer y);
gint		e_int_compare                   (gconstpointer x,
						 gconstpointer y);
guint32		e_color_to_value		(const GdkColor *color);

guint32		e_rgba_to_value			(const GdkRGBA *rgba);

void		e_rgba_to_color			(const GdkRGBA *rgba,
						 GdkColor *color);

void		e_utils_get_theme_color		(GtkWidget *widget,
						 const gchar *color_names,
						 const gchar *fallback_color_ident,
						 GdkRGBA *rgba);

void		e_utils_get_theme_color_color	(GtkWidget *widget,
						 const gchar *color_names,
						 const gchar *fallback_color_ident,
						 GdkColor *color);

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
GDateWeekday	e_weekday_subtract_days		(GDateWeekday weekday,
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
gboolean	e_binding_transform_color_to_string
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer not_used);
gboolean	e_binding_transform_string_to_color
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer not_used);
gboolean	e_binding_transform_source_to_uid
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 ESourceRegistry *registry);
gboolean	e_binding_transform_uid_to_source
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 ESourceRegistry *registry);

gboolean	e_binding_transform_text_non_null
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer user_data);

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
gboolean	e_util_prompt_user		(GtkWindow *parent,
						 const gchar *settings_schema,
						 const gchar *promptkey,
						 const gchar *tag,
						 ...);
void		e_util_run_simple_async_result_in_thread
						(GSimpleAsyncResult *simple,
						 GSimpleAsyncThreadFunc func,
						 GCancellable *cancellable);
gboolean	e_util_is_running_gnome		(void);
void		e_util_set_entry_issue_hint	(GtkWidget *entry,
						 const gchar *hint);

guint		e_util_normalize_font_size	(GtkWidget *widget,
						 gdouble font_size);
void		e_util_init_main_thread		(GThread *thread);
gboolean	e_util_is_main_thread		(GThread *thread);
gchar *		e_util_save_image_from_clipboard
						(GtkClipboard *clipboard);
gboolean	e_util_check_gtk_bindings_in_key_press_event_cb
						(GtkWidget *widget,
						 GdkEvent *event);
void		e_util_claim_dbus_proxy_call_error
						(GDBusProxy *dbus_proxy,
						 const gchar *method_name,
						 const GError *in_error);
void		e_util_invoke_g_dbus_proxy_call_with_error_check
						(GDBusProxy *dbus_proxy,
						 const gchar *method_name,
						 GVariant *parameters,
						 GCancellable *cancellable);
void		e_util_invoke_g_dbus_proxy_call_with_error_check_full
						(GDBusProxy *dbus_proxy,
						 const gchar *method_name,
						 GVariant *parameters,
						 GDBusCallFlags flags,
						 gint timeout_msec,
						 GCancellable *cancellable);
GVariant *	e_util_invoke_g_dbus_proxy_call_sync_wrapper_with_error_check
						(GDBusProxy *dbus_proxy,
						 const gchar *method_name,
						 GVariant *parameters,
						 GCancellable *cancellable);
GVariant *	e_util_invoke_g_dbus_proxy_call_sync_wrapper_full
						(GDBusProxy *dbus_proxy,
						 const gchar *method_name,
						 GVariant *parameters,
						 GDBusCallFlags flags,
						 gint timeout_msec,
						 GCancellable *cancellable,
						 GError **error);
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

G_END_DECLS

#endif /* E_MISC_UTILS_H */
