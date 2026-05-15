/*
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_PAGE_DIALOG_H
#define E_HTML_EDITOR_PAGE_DIALOG_H

#include <e-util/e-html-editor-dialog.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_PAGE_DIALOG \
	(e_html_editor_page_dialog_get_type ())
#define E_HTML_EDITOR_PAGE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_PAGE_DIALOG, EHTMLEditorPageDialog))
#define E_HTML_EDITOR_PAGE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_PAGE_DIALOG, EHTMLEditorPageDialogClass))
#define E_IS_HTML_EDITOR_PAGE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_PAGE_DIALOG))
#define E_IS_HTML_EDITOR_PAGE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_PAGE_DIALOG))
#define E_HTML_EDITOR_PAGE_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_PAGE_DIALOG, EHTMLEditorPageDialogClass))

G_BEGIN_DECLS

typedef struct _EHTMLEditorPageDialog EHTMLEditorPageDialog;
typedef struct _EHTMLEditorPageDialogClass EHTMLEditorPageDialogClass;
typedef struct _EHTMLEditorPageDialogPrivate EHTMLEditorPageDialogPrivate;

struct _EHTMLEditorPageDialog {
	EHTMLEditorDialog parent;
	EHTMLEditorPageDialogPrivate *priv;
};

struct _EHTMLEditorPageDialogClass {
	EHTMLEditorDialogClass parent_class;
};

GType		e_html_editor_page_dialog_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_html_editor_page_dialog_new	(EHTMLEditor *editor);

G_END_DECLS

#endif /* E_HTML_EDITOR_PAGE_DIALOG_H */
