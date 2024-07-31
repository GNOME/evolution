/*
 * e-web-view.h
 *
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
 */

/* This is intended to serve as a common base class for all HTML viewing
 * needs in Evolution.
 *
 * This class handles basic tasks like mouse hovers over links, clicked
 * links, and servicing URI requests asynchronously via GIO. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_WEB_VIEW_H
#define E_WEB_VIEW_H

#include <webkit2/webkit2.h>
#include <e-util/e-activity.h>
#include <e-util/e-content-request.h>
#include <e-util/e-ui-action.h>
#include <e-util/e-ui-action-group.h>
#include <e-util/e-ui-manager.h>

/* Standard GObject macros */
#define E_TYPE_WEB_VIEW \
	(e_web_view_get_type ())
#define E_WEB_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_VIEW, EWebView))
#define E_WEB_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEB_VIEW, EWebViewClass))
#define E_IS_WEB_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEB_VIEW))
#define E_IS_WEB_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEB_VIEW))
#define E_WEB_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEB_VIEW, EWebViewClass))

G_BEGIN_DECLS

typedef struct _EWebView EWebView;
typedef struct _EWebViewClass EWebViewClass;
typedef struct _EWebViewPrivate EWebViewPrivate;

typedef enum {
	CID_URI_SCHEME,
	FILE_URI_SCHEME,
	MAIL_URI_SCHEME,
	EVO_HTTP_URI_SCHEME,
	EVO_HTTPS_URI_SCHEME,
	GTK_STOCK_URI_SCHEME
} EURIScheme;

typedef void (*EWebViewElementClickedFunc) (EWebView *web_view,
					    const gchar *iframe_id,
					    const gchar *element_id,
					    const gchar *element_class,
					    const gchar *element_value,
					    const GtkAllocation *element_position,
					    gpointer user_data);

struct _EWebView {
	WebKitWebView parent;
	EWebViewPrivate *priv;
};

struct _EWebViewClass {
	WebKitWebViewClass parent_class;

	/* Methods */
	void		(*hovering_over_link)	(EWebView *web_view,
						 const gchar *title,
						 const gchar *uri);
	void		(*link_clicked)		(EWebView *web_view,
						 const gchar *uri);
	void		(*load_string)		(EWebView *web_view,
						 const gchar *load_string);
	void		(*load_uri)		(EWebView *web_view,
						 const gchar *load_uri);
	gchar *		(*suggest_filename)	(EWebView *web_view,
						 const gchar *uri);
	void		(*set_fonts)		(EWebView *web_view,
						 PangoFontDescription **monospace,
						 PangoFontDescription **variable_width);

	/* Signals */
	void		(*new_activity)		(EWebView *web_view,
						 EActivity *activity);
	gboolean	(*popup_event)		(EWebView *web_view,
						 const gchar *uri,
						 GdkEvent *event);
	void		(*status_message)	(EWebView *web_view,
						 const gchar *status_message);
	void		(*stop_loading)		(EWebView *web_view);
	void		(*update_actions)	(EWebView *web_view);
	gboolean	(*process_mailto)	(EWebView *web_view,
						 const gchar *mailto_uri);
	void		(*uri_requested)	(EWebView *web_view,
						 const gchar *uri,
						 gchar **redirect_to_uri);
	void		(*content_loaded)	(EWebView *web_view,
						 const gchar *frame_id);
	void		(*before_popup_event)	(EWebView *web_view,
						 const gchar *uri);

	/* Padding for future expansion */
	gpointer reserved[15];
};

GType		e_web_view_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_web_view_new			(void);
WebKitSettings *
		e_web_view_get_default_webkit_settings
						(void);
void		e_web_view_utils_apply_minimum_font_size
						(WebKitSettings *wk_settings);
gint		e_web_view_get_minimum_font_size(EWebView *web_view);
void		e_web_view_set_minimum_font_size(EWebView *web_view,
						 gint pixels);
GCancellable *	e_web_view_get_cancellable	(EWebView *web_view);
void		e_web_view_register_content_request_for_scheme
						(EWebView *web_view,
						 const gchar *scheme,
						 EContentRequest *content_request);
void		e_web_view_update_fonts_settings
						(GSettings *font_settings,
						 PangoFontDescription *ms_font,
						 PangoFontDescription *vw_font,
						 GtkWidget *view_widget);
void		e_web_view_clear		(EWebView *web_view);
void		e_web_view_load_string		(EWebView *web_view,
						 const gchar *string);
void		e_web_view_load_uri		(EWebView *web_view,
						 const gchar *uri);
gchar *		e_web_view_suggest_filename	(EWebView *web_view,
						 const gchar *uri);
void		e_web_view_reload		(EWebView *web_view);
gboolean	e_web_view_get_caret_mode	(EWebView *web_view);
void		e_web_view_set_caret_mode	(EWebView *web_view,
						 gboolean caret_mode);
GtkTargetList *	e_web_view_get_copy_target_list	(EWebView *web_view);
gboolean	e_web_view_get_disable_printing	(EWebView *web_view);
void		e_web_view_set_disable_printing	(EWebView *web_view,
						 gboolean disable_printing);
gboolean	e_web_view_get_disable_save_to_disk
						(EWebView *web_view);
void		e_web_view_set_disable_save_to_disk
						(EWebView *web_view,
						 gboolean disable_save_to_disk);
gboolean	e_web_view_get_editable		(EWebView *web_view);
void		e_web_view_set_editable		(EWebView *web_view,
						 gboolean editable);
gboolean	e_web_view_get_need_input	(EWebView *web_view);
gboolean	e_web_view_get_inline_spelling	(EWebView *web_view);
void		e_web_view_set_inline_spelling	(EWebView *web_view,
						 gboolean inline_spelling);
gboolean	e_web_view_get_magic_links	(EWebView *web_view);
void		e_web_view_set_magic_links	(EWebView *web_view,
						 gboolean magic_links);
gboolean	e_web_view_get_magic_smileys	(EWebView *web_view);
void		e_web_view_set_magic_smileys	(EWebView *web_view,
						 gboolean magic_smileys);
const gchar *	e_web_view_get_selected_uri	(EWebView *web_view);
void		e_web_view_set_selected_uri	(EWebView *web_view,
						 const gchar *selected_uri);
const gchar *	e_web_view_get_cursor_image_src	(EWebView *web_view);
void		e_web_view_set_cursor_image_src	(EWebView *web_view,
						 const gchar *src_uri);
EUIAction *	e_web_view_get_open_proxy	(EWebView *web_view);
void		e_web_view_set_open_proxy	(EWebView *web_view,
						 EUIAction *open_proxy);
GtkTargetList *	e_web_view_get_paste_target_list
						(EWebView *web_view);
EUIAction *	e_web_view_get_print_proxy	(EWebView *web_view);
void		e_web_view_set_print_proxy	(EWebView *web_view,
						 EUIAction *print_proxy);
EUIAction *	e_web_view_get_save_as_proxy	(EWebView *web_view);
void		e_web_view_set_save_as_proxy	(EWebView *web_view,
						 EUIAction *save_as_proxy);
void		e_web_view_get_last_popup_place	(EWebView *web_view,
						 gchar **out_iframe_src,
						 gchar **out_iframe_id,
						 gchar **out_element_id,
						 gchar **out_link_uri);
void		e_web_view_add_highlight	(EWebView *web_view,
						 const gchar *highlight);
void		e_web_view_clear_highlights	(EWebView *web_view);
void		e_web_view_update_highlights	(EWebView *web_view);
void		e_web_view_disable_highlights	(EWebView *web_view);
EUIAction *	e_web_view_get_action		(EWebView *web_view,
						 const gchar *action_name);
EUIActionGroup *e_web_view_get_action_group	(EWebView *web_view,
						 const gchar *group_name);
void		e_web_view_copy_clipboard	(EWebView *web_view);
void		e_web_view_cut_clipboard	(EWebView *web_view);
gboolean	e_web_view_has_selection	(EWebView *web_view);
void		e_web_view_paste_clipboard	(EWebView *web_view);
gboolean	e_web_view_scroll_forward	(EWebView *web_view);
gboolean	e_web_view_scroll_backward	(EWebView *web_view);
void		e_web_view_select_all		(EWebView *web_view);
void		e_web_view_unselect_all		(EWebView *web_view);
void		e_web_view_zoom_100		(EWebView *web_view);
void		e_web_view_zoom_in		(EWebView *web_view);
void		e_web_view_zoom_out		(EWebView *web_view);
EUIManager *	e_web_view_get_ui_manager	(EWebView *web_view);
void		e_web_view_show_popup_menu	(EWebView *web_view,
						 GdkEvent *event);
EActivity *	e_web_view_new_activity		(EWebView *web_view);
void		e_web_view_status_message	(EWebView *web_view,
						 const gchar *status_message);
void		e_web_view_stop_loading		(EWebView *web_view);
void		e_web_view_update_actions	(EWebView *web_view);
void		e_web_view_update_fonts		(EWebView *web_view);
void		e_web_view_cursor_image_copy	(EWebView *web_view);
void		e_web_view_cursor_image_save	(EWebView *web_view);
void		e_web_view_request		(EWebView *web_view,
						 const gchar *uri,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GInputStream *	e_web_view_request_finish	(EWebView *web_view,
						 GAsyncResult *result,
						 GError **error);
void		e_web_view_install_request_handler
						(EWebView *web_view,
						 GType handler_type);
const gchar *	e_web_view_get_citation_color_for_level
						(gint level);
void		e_web_view_set_iframe_src	(EWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *new_src);
void		e_web_view_register_element_clicked
						(EWebView *web_view,
						 const gchar *element_class,
						 EWebViewElementClickedFunc callback,
						 gpointer user_data);
void		e_web_view_unregister_element_clicked
						(EWebView *web_view,
						 const gchar *element_class,
						 EWebViewElementClickedFunc callback,
						 gpointer user_data);
void		e_web_view_set_element_hidden	(EWebView *web_view,
						 const gchar *element_id,
						 gboolean hidden);
void		e_web_view_set_element_style_property
						(EWebView *web_view,
						 const gchar *element_id,
						 const gchar *property_name,
						 const gchar *value);
void		e_web_view_set_element_attribute
						(EWebView *web_view,
						 const gchar *element_id,
						 const gchar *namespace_uri,
						 const gchar *qualified_name,
						 const gchar *value);
G_END_DECLS

#endif /* E_WEB_VIEW_H */
