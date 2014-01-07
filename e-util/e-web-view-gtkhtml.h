/*
 * e-web-view-gtkhtml.h
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

#ifndef E_WEB_VIEW_GTKHTML_H
#define E_WEB_VIEW_GTKHTML_H

#include <gtkhtml/gtkhtml.h>

/* Standard GObject macros */
#define E_TYPE_WEB_VIEW_GTKHTML \
	(e_web_view_gtkhtml_get_type ())
#define E_WEB_VIEW_GTKHTML(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_VIEW_GTKHTML, EWebViewGtkHTML))
#define E_WEB_VIEW_GTKHTML_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEB_VIEW_GTKHTML, EWebViewGtkHTMLClass))
#define E_IS_WEB_VIEW_GTKHTML(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEB_VIEW_GTKHTML))
#define E_IS_WEB_VIEW_GTKHTML_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEB_VIEW_GTKHTML))
#define E_WEB_VIEW_GTKHTML_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEB_VIEW_GTKHTML, EWebViewGtkHTMLClass))

G_BEGIN_DECLS

typedef struct _EWebViewGtkHTML EWebViewGtkHTML;
typedef struct _EWebViewGtkHTMLClass EWebViewGtkHTMLClass;
typedef struct _EWebViewGtkHTMLPrivate EWebViewGtkHTMLPrivate;

struct _EWebViewGtkHTML {
	GtkHTML parent;
	EWebViewGtkHTMLPrivate *priv;
};

struct _EWebViewGtkHTMLClass {
	GtkHTMLClass parent_class;

	/* Methods */
	gchar *		(*extract_uri)		(EWebViewGtkHTML *web_view,
						 GdkEventButton *event,
						 GtkHTML *frame);
	void		(*hovering_over_link)	(EWebViewGtkHTML *web_view,
						 const gchar *title,
						 const gchar *uri);
	void		(*link_clicked)		(EWebViewGtkHTML *web_view,
						 const gchar *uri);
	void		(*load_string)		(EWebViewGtkHTML *web_view,
						 const gchar *load_string);

	/* Signals */
	void		(*copy_clipboard)	(EWebViewGtkHTML *web_view);
	void		(*cut_clipboard)	(EWebViewGtkHTML *web_view);
	void		(*paste_clipboard)	(EWebViewGtkHTML *web_view);
	gboolean	(*popup_event)		(EWebViewGtkHTML *web_view,
						 GdkEventButton *event,
						 const gchar *uri);
	void		(*status_message)	(EWebViewGtkHTML *web_view,
						 const gchar *status_message);
	void		(*stop_loading)		(EWebViewGtkHTML *web_view);
	void		(*update_actions)	(EWebViewGtkHTML *web_view);
	gboolean	(*process_mailto)	(EWebViewGtkHTML *web_view,
						 const gchar *mailto_uri);
};

GType		e_web_view_gtkhtml_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_web_view_gtkhtml_new		(void);
void		e_web_view_gtkhtml_clear	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_load_string	(EWebViewGtkHTML *web_view,
						 const gchar *string);
gboolean	e_web_view_gtkhtml_get_animate	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_animate	(EWebViewGtkHTML *web_view,
						 gboolean animate);
gboolean	e_web_view_gtkhtml_get_caret_mode
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_caret_mode
						(EWebViewGtkHTML *web_view,
						 gboolean caret_mode);
GtkTargetList *	e_web_view_gtkhtml_get_copy_target_list
						(EWebViewGtkHTML *web_view);
gboolean	e_web_view_gtkhtml_get_disable_printing
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_disable_printing
						(EWebViewGtkHTML *web_view,
						 gboolean disable_printing);
gboolean	e_web_view_gtkhtml_get_disable_save_to_disk
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_disable_save_to_disk
						(EWebViewGtkHTML *web_view,
						 gboolean disable_save_to_disk);
gboolean	e_web_view_gtkhtml_get_editable	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_editable	(EWebViewGtkHTML *web_view,
						 gboolean editable);
gboolean	e_web_view_gtkhtml_get_inline_spelling
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_inline_spelling
						(EWebViewGtkHTML *web_view,
						 gboolean inline_spelling);
gboolean	e_web_view_gtkhtml_get_magic_links
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_magic_links
						(EWebViewGtkHTML *web_view,
						 gboolean magic_links);
gboolean	e_web_view_gtkhtml_get_magic_smileys
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_magic_smileys
						(EWebViewGtkHTML *web_view,
						 gboolean magic_smileys);
const gchar *	e_web_view_gtkhtml_get_selected_uri
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_selected_uri
						(EWebViewGtkHTML *web_view,
						 const gchar *selected_uri);
GdkPixbufAnimation *
		e_web_view_gtkhtml_get_cursor_image
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_cursor_image
						(EWebViewGtkHTML *web_view,
						 GdkPixbufAnimation *animation);
GtkAction *	e_web_view_gtkhtml_get_open_proxy
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_open_proxy
						(EWebViewGtkHTML *web_view,
						 GtkAction *open_proxy);
GtkTargetList *	e_web_view_gtkhtml_get_paste_target_list
						(EWebViewGtkHTML *web_view);
GtkAction *	e_web_view_gtkhtml_get_print_proxy
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_print_proxy
						(EWebViewGtkHTML *web_view,
						 GtkAction *print_proxy);
GtkAction *	e_web_view_gtkhtml_get_save_as_proxy
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_set_save_as_proxy
						(EWebViewGtkHTML *web_view,
						 GtkAction *save_as_proxy);
GtkAction *	e_web_view_gtkhtml_get_action	(EWebViewGtkHTML *web_view,
						 const gchar *action_name);
GtkActionGroup *e_web_view_gtkhtml_get_action_group
						(EWebViewGtkHTML *web_view,
						 const gchar *group_name);
gchar *		e_web_view_gtkhtml_extract_uri	(EWebViewGtkHTML *web_view,
						 GdkEventButton *event,
						 GtkHTML *frame);
void		e_web_view_gtkhtml_copy_clipboard
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_cut_clipboard
						(EWebViewGtkHTML *web_view);
gboolean	e_web_view_gtkhtml_is_selection_active
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_paste_clipboard
						(EWebViewGtkHTML *web_view);
gboolean	e_web_view_gtkhtml_scroll_forward
						(EWebViewGtkHTML *web_view);
gboolean	e_web_view_gtkhtml_scroll_backward
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_select_all	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_unselect_all	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_zoom_100	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_zoom_in	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_zoom_out	(EWebViewGtkHTML *web_view);
GtkUIManager *	e_web_view_gtkhtml_get_ui_manager
						(EWebViewGtkHTML *web_view);
GtkWidget *	e_web_view_gtkhtml_get_popup_menu
						(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_show_popup_menu
						(EWebViewGtkHTML *web_view,
						 GdkEventButton *event,
						 GtkMenuPositionFunc func,
						 gpointer user_data);
void		e_web_view_gtkhtml_status_message
						(EWebViewGtkHTML *web_view,
						const gchar *status_message);
void		e_web_view_gtkhtml_stop_loading	(EWebViewGtkHTML *web_view);
void		e_web_view_gtkhtml_update_actions
						(EWebViewGtkHTML *web_view);

G_END_DECLS

#endif /* E_WEB_VIEW_GTKHTML_H */
