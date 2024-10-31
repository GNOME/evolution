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

#include <e-util/e-focus-tracker.h>

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

	void	(* changed)		(EMarkdownEditor *self);
	void	(* format_bold)		(EMarkdownEditor *self);
	void	(* format_italic)	(EMarkdownEditor *self);
	void	(* format_quote)	(EMarkdownEditor *self);
	void	(* format_code)		(EMarkdownEditor *self);
	void	(* format_bullet_list)	(EMarkdownEditor *self);
	void	(* format_numbered_list)(EMarkdownEditor *self);
	void	(* format_header)	(EMarkdownEditor *self);
	void	(* insert_link)		(EMarkdownEditor *self);
	void	(* insert_emoji)	(EMarkdownEditor *self);

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_markdown_editor_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_markdown_editor_new			(void);
void		e_markdown_editor_connect_focus_tracker	(EMarkdownEditor *self,
							 EFocusTracker *focus_tracker);
GtkTextView *	e_markdown_editor_get_text_view		(EMarkdownEditor *self);
GtkToolbar *	e_markdown_editor_get_action_toolbar	(EMarkdownEditor *self);
void		e_markdown_editor_set_text		(EMarkdownEditor *self,
							 const gchar *text);
gchar *		e_markdown_editor_dup_text		(EMarkdownEditor *self);
gchar *		e_markdown_editor_dup_html		(EMarkdownEditor *self);
gboolean	e_markdown_editor_get_preview_mode	(EMarkdownEditor *self);
void		e_markdown_editor_set_preview_mode	(EMarkdownEditor *self,
							 gboolean preview_mode);

G_END_DECLS

#endif /* E_MARKDOWN_EDITOR_H */
