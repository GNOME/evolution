/*
 * e-editor-spell-check-dialog.h
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

#ifndef E_EDITOR_SPELL_CHECK_DIALOG_H
#define E_EDITOR_SPELL_CHECK_DIALOG_H

#include <e-util/e-editor-dialog.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR_SPELL_CHECK_DIALOG \
	(e_editor_spell_check_dialog_get_type ())
#define E_EDITOR_SPELL_CHECK_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_SPELL_CHECK_DIALOG, EEditorSpellCheckDialog))
#define E_EDITOR_SPELL_CHECK_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_SPELL_CHECK_DIALOG, EEditorSpellCheckDialogClass))
#define E_IS_EDITOR_SPELL_CHECK_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_SPELL_CHECK_DIALOG))
#define E_IS_EDITOR_SPELL_CHECK_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_SPELL_CHECK_DIALOG))
#define E_EDITOR_SPELL_CHECK_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_SPELL_CHECK_DIALOG, EEditorSpellCheckDialogClass))

G_BEGIN_DECLS

typedef struct _EEditorSpellCheckDialog EEditorSpellCheckDialog;
typedef struct _EEditorSpellCheckDialogClass EEditorSpellCheckDialogClass;
typedef struct _EEditorSpellCheckDialogPrivate EEditorSpellCheckDialogPrivate;

struct _EEditorSpellCheckDialog {
	EEditorDialog parent;
	EEditorSpellCheckDialogPrivate *priv;
};

struct _EEditorSpellCheckDialogClass {
	EEditorDialogClass parent_class;
};

GType		e_editor_spell_check_dialog_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_editor_spell_check_dialog_new
					(EEditor *editor);
void		e_editor_spell_check_dialog_update_dictionaries
					(EEditorSpellCheckDialog *dialog);

G_END_DECLS

#endif /* E_EDITOR_SPELL_CHECK_DIALOG_H */
