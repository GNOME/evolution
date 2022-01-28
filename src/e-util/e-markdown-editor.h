/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MARKDOWN_EDITOR_H
#define E_MARKDOWN_EDITOR_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MARKDOWN_EDITOR \
	(e_markdown_editor_get_type ())
#define E_MARKDOWN_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MARKDOWN_EDITOR, EMarkdownEditor))
#define E_MARKDOWN_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MARKDOWN_EDITOR, EMarkdownEditorClass))
#define E_IS_MARKDOWN_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MARKDOWN_EDITOR))
#define E_IS_MARKDOWN_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MARKDOWN_EDITOR))
#define E_MARKDOWN_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MARKDOWN_EDITOR, EMarkdownEditorClass))

G_BEGIN_DECLS

typedef struct _EMarkdownEditor EMarkdownEditor;
typedef struct _EMarkdownEditorClass EMarkdownEditorClass;
typedef struct _EMarkdownEditorPrivate EMarkdownEditorPrivate;

struct _EMarkdownEditor {
	GtkBox parent;
	EMarkdownEditorPrivate *priv;
};

struct _EMarkdownEditorClass {
	GtkBoxClass parent_class;
};

GType		e_markdown_editor_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_markdown_editor_new			(void);
gchar *		e_markdown_editor_dup_text		(EMarkdownEditor *self);
gchar *		e_markdown_editor_dup_html		(EMarkdownEditor *self);

gchar *		e_markdown_util_text_to_html		(const gchar *plain_text,
							 gssize length);

G_END_DECLS

#endif /* E_MARKDOWN_EDITOR_H */
