/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VCARD_EDITOR_SECTION_H
#define E_VCARD_EDITOR_SECTION_H

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

#include "e-vcard-editor-item.h"

G_BEGIN_DECLS

struct _EVCardEditor;

#define E_TYPE_VCARD_EDITOR_SECTION (e_vcard_editor_section_get_type ())
G_DECLARE_FINAL_TYPE (EVCardEditorSection, e_vcard_editor_section, E, VCARD_EDITOR_SECTION, GtkExpander)

/* reads data from the contact to the section */
typedef void	(* EVCardEditorSectionReadFunc)	(EVCardEditorSection *section,
						 EContact *contact);
/* writes data from the section to the contact and returns whether succeeded (validates the data too);
   sets the out arguments on error, with widget not referenced */
typedef gboolean(* EVCardEditorSectionWriteFunc)(EVCardEditorSection *section,
						 EContact *contact,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);

GtkWidget *	e_vcard_editor_section_new	(struct _EVCardEditor *editor,
						 const gchar *label,
						 EVCardEditorSectionReadFunc fill_section_func, /* nullable */
						 EVCardEditorSectionWriteFunc fill_contact_func); /* nullable */
struct _EVCardEditor *
		e_vcard_editor_section_get_editor
						(EVCardEditorSection *self);
GtkButton *	e_vcard_editor_section_get_add_button
						(EVCardEditorSection *self);
void		e_vcard_editor_section_set_sort_function
						(EVCardEditorSection *self,
						 GCompareFunc sort_func);
gboolean	e_vcard_editor_section_get_changed
						(EVCardEditorSection *self);
void		e_vcard_editor_section_set_changed
						(EVCardEditorSection *self,
						 gboolean value);
gboolean	e_vcard_editor_section_get_single_line
						(EVCardEditorSection *self);
void		e_vcard_editor_section_set_single_line
						(EVCardEditorSection *self,
						 gboolean value);
void		e_vcard_editor_section_take_item(EVCardEditorSection *self,
						 EVCardEditorItem *item); /* transfer full */
guint		e_vcard_editor_section_get_n_items
						(EVCardEditorSection *self);
EVCardEditorItem * /* transfer none */
		e_vcard_editor_section_get_item	(EVCardEditorSection *self,
						 guint index);
void		e_vcard_editor_section_remove_item
						(EVCardEditorSection *self,
						 guint index);
void		e_vcard_editor_section_remove_dynamic
						(EVCardEditorSection *self);
void		e_vcard_editor_section_remove_all
						(EVCardEditorSection *self);
void		e_vcard_editor_section_take_widget
						(EVCardEditorSection *self,
						 GtkWidget *widget, /* transfer full */
						 GtkPackType pack_type);
void		e_vcard_editor_section_remove_widget
						(EVCardEditorSection *self,
						 guint index);
guint		e_vcard_editor_section_get_n_widgets
						(EVCardEditorSection *self);
GtkWidget * /* transfer none */
		e_vcard_editor_section_get_widget
						(EVCardEditorSection *self,
						 guint index);
void		e_vcard_editor_section_fill_section
						(EVCardEditorSection *self,
						 EContact *contact);
gboolean	e_vcard_editor_section_fill_contact
						(EVCardEditorSection *self,
						 EContact *contact,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);

G_END_DECLS

#endif /* E_VCARD_EDITOR_SECTION_H */
