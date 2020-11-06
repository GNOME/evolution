/*
 * e-web-view-preview.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is intended to serve as a common widget for previews before import.
 * It contains a GtkTreeView at the top and an EWebView at the bottom.
 * The tree view is not shown initially, it should be forced with
 * e_web_view_preview_show_tree_view().
 *
 * The internal default EWebView can be accessed by e_web_view_preview_get_preview()
 * and it should be updated for each change of the selected item in the tree
 * view, when it's shown.
 *
 * Updating an EWebView content through helper functions of an EWebViewPreview
 * begins with call of e_web_view_preview_begin_update(), which starts an empty
 * page construction, which is finished by e_web_view_preview_end_update(),
 * and the content of the EWebView is updated.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_WEB_VIEW_PREVIEW_H
#define E_WEB_VIEW_PREVIEW_H

#include <e-util/e-web-view.h>

/* Standard GObject macros */
#define E_TYPE_WEB_VIEW_PREVIEW \
	(e_web_view_preview_get_type ())
#define E_WEB_VIEW_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_VIEW_PREVIEW, EWebViewPreview))
#define E_WEB_VIEW_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEB_VIEW_PREVIEW, EWebViewPreviewClass))
#define E_IS_WEB_VIEW_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEB_VIEW_PREVIEW))
#define E_IS_WEB_VIEW_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEB_VIEW_PREVIEW))
#define E_WEB_VIEW_PREVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEB_VIEW_PREVIEW, EWebViewPreviewClass))

G_BEGIN_DECLS

typedef struct _EWebViewPreview EWebViewPreview;
typedef struct _EWebViewPreviewClass EWebViewPreviewClass;
typedef struct _EWebViewPreviewPrivate EWebViewPreviewPrivate;

struct _EWebViewPreview {
	GtkVPaned parent;
	EWebViewPreviewPrivate *priv;
};

struct _EWebViewPreviewClass {
	GtkVPanedClass parent_class;
};

GType		e_web_view_preview_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_web_view_preview_new		(void);
GtkTreeView *	e_web_view_preview_get_tree_view
						(EWebViewPreview *preview);
GtkWidget *	e_web_view_preview_get_preview	(EWebViewPreview *preview);
void		e_web_view_preview_set_preview	(EWebViewPreview *preview,
						 GtkWidget *preview_widget);
void		e_web_view_preview_show_tree_view
						(EWebViewPreview *preview);
void		e_web_view_preview_hide_tree_view
						(EWebViewPreview *preview);
gboolean	e_web_view_preview_get_escape_values
						(EWebViewPreview *preview);
void		e_web_view_preview_set_escape_values
						(EWebViewPreview *preview,
						 gboolean escape);
void		e_web_view_preview_begin_update	(EWebViewPreview *preview);
void		e_web_view_preview_end_update	(EWebViewPreview *preview);
void		e_web_view_preview_add_header	(EWebViewPreview *preview,
						 gint index,
						 const gchar *header);
void		e_web_view_preview_add_text	(EWebViewPreview *preview,
						 const gchar *text);
void		e_web_view_preview_add_raw_html	(EWebViewPreview *preview,
						 const gchar *raw_html);
void		e_web_view_preview_add_separator
						(EWebViewPreview *preview);
void		e_web_view_preview_add_empty_line
						(EWebViewPreview *preview);
void		e_web_view_preview_add_section	(EWebViewPreview *preview,
						 const gchar *section,
						 const gchar *value);
void		e_web_view_preview_add_section_html
						(EWebViewPreview *preview,
						 const gchar *section,
						 const gchar *html);

G_END_DECLS

#endif /* E_WEB_VIEW_PREVIEW_H */
