/*
 * e-preview-pane.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PREVIEW_PANE_H
#define E_PREVIEW_PANE_H

#include <gtk/gtk.h>

#include <e-util/e-search-bar.h>
#include <e-util/e-web-view.h>

/* Standard GObject macros */
#define E_TYPE_PREVIEW_PANE \
	(e_preview_pane_get_type ())
#define E_PREVIEW_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PREVIEW_PANE, EPreviewPane))
#define E_PREVIEW_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PREVIEW_PANE, EPreviewPaneClass))
#define E_IS_PREVIEW_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PREVIEW_PANE))
#define E_IS_PREVIEW_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PREVIEW_PANE))
#define E_PREVIEW_PANE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PREVIEW_PANE, EPreviewPaneClass))

G_BEGIN_DECLS

typedef struct _EPreviewPane EPreviewPane;
typedef struct _EPreviewPaneClass EPreviewPaneClass;
typedef struct _EPreviewPanePrivate EPreviewPanePrivate;

struct _EPreviewPane {
	GtkBox parent;
	EPreviewPanePrivate *priv;
};

struct _EPreviewPaneClass {
	GtkBoxClass parent_class;

	/* Signals */
	void		(*show_search_bar)	(EPreviewPane *preview_pane);
};

GType		e_preview_pane_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_preview_pane_new		(EWebView *web_view);
EWebView *	e_preview_pane_get_web_view	(EPreviewPane *preview_pane);
ESearchBar *	e_preview_pane_get_search_bar	(EPreviewPane *preview_pane);
void		e_preview_pane_clear_alerts	(EPreviewPane *preview_pane);
void		e_preview_pane_show_search_bar	(EPreviewPane *preview_pane);

G_END_DECLS

#endif /* E_PREVIEW_PANE_H */
