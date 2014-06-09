/*
 * e-html-editor-spell-check-dialog.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_SPELL_CHECK_DIALOG_H
#define E_HTML_EDITOR_SPELL_CHECK_DIALOG_H

#include <e-util/e-html-editor-dialog.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG \
	(e_html_editor_spell_check_dialog_get_type ())
#define E_HTML_EDITOR_SPELL_CHECK_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG, EHTMLEditorSpellCheckDialog))
#define E_HTML_EDITOR_SPELL_CHECK_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG, EHTMLEditorSpellCheckDialogClass))
#define E_IS_HTML_EDITOR_SPELL_CHECK_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG))
#define E_IS_HTML_EDITOR_SPELL_CHECK_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG))
#define E_HTML_EDITOR_SPELL_CHECK_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_SPELL_CHECK_DIALOG, EHTMLEditorSpellCheckDialogClass))

G_BEGIN_DECLS

typedef struct _EHTMLEditorSpellCheckDialog EHTMLEditorSpellCheckDialog;
typedef struct _EHTMLEditorSpellCheckDialogClass EHTMLEditorSpellCheckDialogClass;
typedef struct _EHTMLEditorSpellCheckDialogPrivate EHTMLEditorSpellCheckDialogPrivate;

struct _EHTMLEditorSpellCheckDialog {
	EHTMLEditorDialog parent;
	EHTMLEditorSpellCheckDialogPrivate *priv;
};

struct _EHTMLEditorSpellCheckDialogClass {
	EHTMLEditorDialogClass parent_class;
};

GType		e_html_editor_spell_check_dialog_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_html_editor_spell_check_dialog_new
					(EHTMLEditor *editor);
void		e_html_editor_spell_check_dialog_update_dictionaries
					(EHTMLEditorSpellCheckDialog *dialog);

G_END_DECLS

#endif /* E_HTML_EDITOR_SPELL_CHECK_DIALOG_H */
