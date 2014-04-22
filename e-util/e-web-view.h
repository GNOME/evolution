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
 * needs in Evolution.  Currently based on GtkHTML, the idea is to wrap
 * the GtkHTML API enough that we no longer have to make direct calls to
 * it.  This should help smooth the transition to WebKit/GTK+.
 *
 * This class handles basic tasks like mouse hovers over links, clicked
 * links, and servicing URI requests asynchronously via GIO. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_WEB_VIEW_H
#define E_WEB_VIEW_H

#include <webkit/webkit.h>
#include <e-util/e-activity.h>

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

struct _EWebView {
	WebKitWebView parent;
	EWebViewPrivate *priv;
};

struct _EWebViewClass {
	WebKitWebViewClass parent_class;

	/* Methods */
	GtkWidget *	(*create_plugin_widget)	(EWebView *web_view,
						 const gchar *mime_type,
						 const gchar *uri,
						 GHashTable *param);
	void		(*hovering_over_link)	(EWebView *web_view,
						 const gchar *title,
						 const gchar *uri);
	void		(*link_clicked)		(EWebView *web_view,
						 const gchar *uri);
	void		(*load_string)		(EWebView *web_view,
						 const gchar *load_string);
	void		(*load_uri)		(EWebView *web_view,
						 const gchar *load_uri);
	gchar *		(*redirect_uri)		(EWebView *web_view,
						 const gchar *uri);
	gchar *		(*suggest_filename)	(EWebView *web_view,
						 const gchar *uri);
	void		(*set_fonts)		(EWebView *web_view,
						 PangoFontDescription **monospace,
						 PangoFontDescription **variable_width);

	/* Signals */
	void		(*new_activity)		(EWebView *web_view,
						 EActivity *activity);
	gboolean	(*popup_event)		(EWebView *web_view,
						 const gchar *uri);
	void		(*status_message)	(EWebView *web_view,
						 const gchar *status_message);
	void		(*stop_loading)		(EWebView *web_view);
	void		(*update_actions)	(EWebView *web_view);
	gboolean	(*process_mailto)	(EWebView *web_view,
						 const gchar *mailto_uri);
};

GType		e_web_view_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_web_view_new			(void);
void		e_web_view_clear		(EWebView *web_view);
void		e_web_view_load_string		(EWebView *web_view,
						 const gchar *string);
void		e_web_view_load_uri		(EWebView *web_view,
						 const gchar *uri);
gchar *		e_web_view_redirect_uri		(EWebView *web_view,
						 const gchar *uri);
gchar *		e_web_view_suggest_filename	(EWebView *web_view,
						 const gchar *uri);
void		e_web_view_reload		(EWebView *web_view);
gchar *		e_web_view_get_html		(EWebView *web_view);
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
GtkAction *	e_web_view_get_open_proxy	(EWebView *web_view);
void		e_web_view_set_open_proxy	(EWebView *web_view,
						 GtkAction *open_proxy);
GtkTargetList *	e_web_view_get_paste_target_list
						(EWebView *web_view);
GtkAction *	e_web_view_get_print_proxy	(EWebView *web_view);
void		e_web_view_set_print_proxy	(EWebView *web_view,
						 GtkAction *print_proxy);
GtkAction *	e_web_view_get_save_as_proxy	(EWebView *web_view);
void		e_web_view_set_save_as_proxy	(EWebView *web_view,
						 GtkAction *save_as_proxy);
void		e_web_view_add_highlight	(EWebView *web_view,
						 const gchar *highlight);
void		e_web_view_clear_highlights	(EWebView *web_view);
void		e_web_view_update_highlights	(EWebView *web_view);
GtkAction *	e_web_view_get_action		(EWebView *web_view,
						 const gchar *action_name);
GtkActionGroup *e_web_view_get_action_group	(EWebView *web_view,
						 const gchar *group_name);
void		e_web_view_copy_clipboard	(EWebView *web_view);
void		e_web_view_cut_clipboard	(EWebView *web_view);
gboolean	e_web_view_is_selection_active	(EWebView *web_view);
void		e_web_view_paste_clipboard	(EWebView *web_view);
gboolean	e_web_view_scroll_forward	(EWebView *web_view);
gboolean	e_web_view_scroll_backward	(EWebView *web_view);
void		e_web_view_select_all		(EWebView *web_view);
void		e_web_view_unselect_all		(EWebView *web_view);
void		e_web_view_zoom_100		(EWebView *web_view);
void		e_web_view_zoom_in		(EWebView *web_view);
void		e_web_view_zoom_out		(EWebView *web_view);
GtkUIManager *	e_web_view_get_ui_manager	(EWebView *web_view);
GtkWidget *	e_web_view_get_popup_menu	(EWebView *web_view);
void		e_web_view_show_popup_menu	(EWebView *web_view);
EActivity *	e_web_view_new_activity		(EWebView *web_view);
void		e_web_view_status_message	(EWebView *web_view,
						 const gchar *status_message);
void		e_web_view_stop_loading		(EWebView *web_view);
void		e_web_view_update_actions	(EWebView *web_view);
gchar *		e_web_view_get_selection_html	(EWebView *web_view);
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
void		e_web_view_add_css_rule_into_style_sheet
						(EWebView *web_view,
						 const gchar *style_sheet_id,
						 const gchar *selector,
						 const gchar *style);
gboolean	element_has_id			(WebKitDOMElement *element,
						 const gchar* id);
gboolean	element_has_tag			(WebKitDOMElement *element,
						 const gchar* tag);
gboolean	element_has_class		(WebKitDOMElement *element,
						 const gchar* class);
void		element_add_class		(WebKitDOMElement *element,
						 const gchar* class);
void		element_remove_class		(WebKitDOMElement *element,
						 const gchar* class);
G_END_DECLS

#endif /* E_WEB_VIEW_H */
