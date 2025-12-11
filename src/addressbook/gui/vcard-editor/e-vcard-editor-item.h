/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VCARD_EDITOR_ITEM_H
#define E_VCARD_EDITOR_ITEM_H

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

G_BEGIN_DECLS

struct _EVCardEditor;
struct _EVCardEditorSection;

#define E_TYPE_VCARD_EDITOR_ITEM (e_vcard_editor_item_get_type ())
G_DECLARE_FINAL_TYPE (EVCardEditorItem, e_vcard_editor_item, E, VCARD_EDITOR_ITEM, GObject)

/* reads data from the contact to the item */
typedef void	(* EVCardEditorItemReadFunc)	(EVCardEditorItem *item,
						 struct _EVCardEditor *editor,
						 EContact *contact);
/* writes data from the item to the contact and returns whether succeeded (validates the data too);
   sets the out arguments on error, with widget not referenced */
typedef gboolean(* EVCardEditorItemWriteFunc)	(EVCardEditorItem *item,
						 struct _EVCardEditor *editor,
						 EContact *contact,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);

EVCardEditorItem *
		e_vcard_editor_item_new		(GtkWidget *label_widget, /* nullable, transfer full */
						 GtkWidget *data_widget, /* transfer full */
						 gboolean expand_data_widget,
						 EVCardEditorItemReadFunc fill_item_func, /* nullable */
						 EVCardEditorItemWriteFunc fill_contact_func); /* nullable */
GtkWidget *	e_vcard_editor_item_get_label_widget
						(EVCardEditorItem *self);
GtkWidget *	e_vcard_editor_item_get_data_box(EVCardEditorItem *self);
void		e_vcard_editor_item_add_data_widget
						(EVCardEditorItem *self,
						 GtkWidget *data_widget, /* transfer full */
						 gboolean expand);
guint		e_vcard_editor_item_get_n_data_widgets
						(EVCardEditorItem *self);
GtkWidget *	e_vcard_editor_item_get_data_widget
						(EVCardEditorItem *self,
						 guint index);
void		e_vcard_editor_item_add_remove_button
						(EVCardEditorItem *self,
						 struct _EVCardEditorSection *section);
void		e_vcard_editor_item_set_user_data
						(EVCardEditorItem *self,
						 gpointer user_data,
						 GDestroyNotify free_func); /* nullable */
gpointer	e_vcard_editor_item_get_user_data
						(EVCardEditorItem *self);
void		e_vcard_editor_item_set_field_id(EVCardEditorItem *self,
						 EContactField value);
EContactField	e_vcard_editor_item_get_field_id(EVCardEditorItem *self);
void		e_vcard_editor_item_set_permanent
						(EVCardEditorItem *self,
						 gboolean value);
gboolean	e_vcard_editor_item_get_permanent
						(EVCardEditorItem *self);
void		e_vcard_editor_item_set_single_line
						(EVCardEditorItem *self,
						 gboolean value);
gboolean	e_vcard_editor_item_get_single_line
						(EVCardEditorItem *self);
void		e_vcard_editor_item_fill_item	(EVCardEditorItem *self,
						 struct _EVCardEditor *editor,
						 EContact *contact); /* nullable */
gboolean	e_vcard_editor_item_fill_contact(EVCardEditorItem *self,
						 struct _EVCardEditor *editor,
						 EContact *contact,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);
void		e_vcard_editor_item_emit_changed(EVCardEditorItem *self);

G_END_DECLS

#endif /* E_VCARD_EDITOR_ITEM_H */
