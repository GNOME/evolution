/*
 * e-html-editor-link-dialog.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_LINK_DIALOG_H
#define E_HTML_EDITOR_LINK_DIALOG_H

#include <e-util/e-html-editor-dialog.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_LINK_DIALOG \
	(e_html_editor_link_dialog_get_type ())
#define E_HTML_EDITOR_LINK_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_LINK_DIALOG, EHTMLEditorLinkDialog))
#define E_HTML_EDITOR_LINK_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_LINK_DIALOG, EHTMLEditorLinkDialogClass))
#define E_IS_HTML_EDITOR_LINK_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_LINK_DIALOG))
#define E_IS_HTML_EDITOR_LINK_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_LINK_DIALOG))
#define E_HTML_EDITOR_LINK_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_LINK_DIALOG, EHTMLEditorLinkDialogClass))

G_BEGIN_DECLS

typedef struct _EHTMLEditorLinkDialog EHTMLEditorLinkDialog;
typedef struct _EHTMLEditorLinkDialogClass EHTMLEditorLinkDialogClass;
typedef struct _EHTMLEditorLinkDialogPrivate EHTMLEditorLinkDialogPrivate;

struct _EHTMLEditorLinkDialog {
	EHTMLEditorDialog parent;
	EHTMLEditorLinkDialogPrivate *priv;
};

struct _EHTMLEditorLinkDialogClass {
	EHTMLEditorDialogClass parent_class;
};

GType		e_html_editor_link_dialog_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_html_editor_link_dialog_new	(EHTMLEditor *editor);

G_END_DECLS

#endif /* E_HTML_EDITOR_LINK_DIALOG_H */
