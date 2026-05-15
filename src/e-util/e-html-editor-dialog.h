/*
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_DIALOG_H
#define E_HTML_EDITOR_DIALOG_H

#include <gtk/gtk.h>
#include <e-util/e-html-editor.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_DIALOG \
	(e_html_editor_dialog_get_type ())
#define E_HTML_EDITOR_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_DIALOG, EHTMLEditorDialog))
#define E_HTML_EDITOR_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_DIALOG, EHTMLEditorDialogClass))
#define E_IS_HTML_EDITOR_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_DIALOG))
#define E_IS_HTML_EDITOR_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_DIALOG))
#define E_HTML_EDITOR_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_DIALOG, EHTMLEditorDialogClass))

G_BEGIN_DECLS

typedef struct _EHTMLEditorDialog EHTMLEditorDialog;
typedef struct _EHTMLEditorDialogClass EHTMLEditorDialogClass;
typedef struct _EHTMLEditorDialogPrivate EHTMLEditorDialogPrivate;

struct _EHTMLEditorDialog {
	GtkWindow parent;
	EHTMLEditorDialogPrivate *priv;
};

struct _EHTMLEditorDialogClass {
	GtkWindowClass parent_class;
};

GType		e_html_editor_dialog_get_type	(void) G_GNUC_CONST;
EHTMLEditor *	e_html_editor_dialog_get_editor	(EHTMLEditorDialog *dialog);
GtkBox *	e_html_editor_dialog_get_button_box
						(EHTMLEditorDialog *dialog);
GtkGrid *	e_html_editor_dialog_get_container
						(EHTMLEditorDialog *dialog);

G_END_DECLS

#endif /* E_HTML_EDITOR_DIALOG_H */
