/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VCARD_EDITOR_NOTES_H
#define E_VCARD_EDITOR_NOTES_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_VCARD_EDITOR_NOTES (e_vcard_editor_notes_get_type ())
G_DECLARE_FINAL_TYPE (EVCardEditorNotes, e_vcard_editor_notes, E, VCARD_EDITOR_NOTES, GtkScrolledWindow)

GtkWidget *	e_vcard_editor_notes_new	(void);
void		e_vcard_editor_notes_set_text	(EVCardEditorNotes *self,
						 const gchar *text);
gchar *		e_vcard_editor_notes_get_text	(EVCardEditorNotes *self);

G_END_DECLS

#endif /* E_VCARD_EDITOR_NOTES_H */
