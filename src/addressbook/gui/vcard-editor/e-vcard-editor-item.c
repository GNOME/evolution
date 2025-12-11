/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-vcard-editor-item.h"
/* it's after, because of predefined type for the section in the header */
#include "e-vcard-editor-section.h"

struct _EVCardEditorItem {
	GObject parent_object;

	GtkWidget *label_widget; /* owned */
	GtkWidget *data_box; /* owned */
	GPtrArray *data_widgets; /* GtkWidget *, owned */
	EVCardEditorItemReadFunc fill_item_func;
	EVCardEditorItemWriteFunc fill_contact_func;
	gpointer user_data;
	GDestroyNotify user_data_free;
	EContactField field_id;
	gboolean permanent;
	gboolean single_line;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EVCardEditorItem, e_vcard_editor_item, G_TYPE_OBJECT)

static void
e_vcard_editor_item_dispose (GObject *object)
{
	EVCardEditorItem *self = E_VCARD_EDITOR_ITEM (object);

	g_clear_object (&self->label_widget);
	g_clear_object (&self->data_box);
	g_clear_pointer (&self->data_widgets, g_ptr_array_unref);

	if (self->user_data_free) {
		g_clear_pointer (&self->user_data, self->user_data_free);
		self->user_data_free = NULL;
	}

	G_OBJECT_CLASS (e_vcard_editor_item_parent_class)->dispose (object);
}

static void
e_vcard_editor_item_class_init (EVCardEditorItemClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_vcard_editor_item_dispose;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_vcard_editor_item_init (EVCardEditorItem *self)
{
	self->data_box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4));
	self->data_widgets = g_ptr_array_new_with_free_func (g_object_unref);
	self->field_id = E_CONTACT_FIELD_LAST;
	self->single_line = TRUE;
}

EVCardEditorItem *
e_vcard_editor_item_new (GtkWidget *label_widget, /* nullable, transfer full */
			 GtkWidget *data_widget, /* transfer full */
			 gboolean expand_data_widget,
			 EVCardEditorItemReadFunc fill_item_func, /* nullable */
			 EVCardEditorItemWriteFunc fill_contact_func)
{
	EVCardEditorItem *self;

	g_return_val_if_fail (GTK_IS_WIDGET (data_widget), NULL);

	self = g_object_new (E_TYPE_VCARD_EDITOR_ITEM, NULL);

	self->label_widget = label_widget ? g_object_ref_sink (label_widget) : NULL;
	g_ptr_array_add (self->data_widgets, g_object_ref_sink (data_widget));
	self->fill_item_func = fill_item_func;
	self->fill_contact_func = fill_contact_func;

	if (self->label_widget && GTK_IS_LABEL (self->label_widget))
		gtk_label_set_mnemonic_widget (GTK_LABEL (self->label_widget), data_widget);

	if (self->label_widget)
		gtk_widget_set_visible (self->label_widget, TRUE);

	gtk_box_pack_start (GTK_BOX (self->data_box), data_widget, expand_data_widget, expand_data_widget, 0);

	if (expand_data_widget) {
		g_object_set (self->data_box,
			"hexpand", TRUE,
			NULL);
	}

	return self;
}

GtkWidget *
e_vcard_editor_item_get_label_widget (EVCardEditorItem *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), NULL);

	return self->label_widget;
}

GtkWidget *
e_vcard_editor_item_get_data_box (EVCardEditorItem *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), NULL);

	return self->data_box;
}

void
e_vcard_editor_item_add_data_widget (EVCardEditorItem *self,
				     GtkWidget *data_widget,
				     gboolean expand)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));
	g_return_if_fail (GTK_IS_WIDGET (data_widget));

	g_ptr_array_add (self->data_widgets, g_object_ref_sink (data_widget));
	gtk_box_pack_start (GTK_BOX (self->data_box), data_widget, expand, expand, 0);
}

guint
e_vcard_editor_item_get_n_data_widgets (EVCardEditorItem *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), 0);

	return self->data_widgets->len;
}

GtkWidget *
e_vcard_editor_item_get_data_widget (EVCardEditorItem *self,
				     guint index)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), NULL);
	g_return_val_if_fail (index < self->data_widgets->len, NULL);

	return g_ptr_array_index (self->data_widgets, index);
}

#define ITEM_KEY "e-vcard-editor-item"

static void
eve_item_remove_button_clicked_cb (GtkButton *button,
				   gpointer user_data)
{
	EVCardEditorSection *section = user_data;
	EVCardEditorItem *item = g_object_get_data (G_OBJECT (button), ITEM_KEY);
	guint n_items, ii;

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *existing = e_vcard_editor_section_get_item (section, ii);

		if (existing == item) {
			e_vcard_editor_section_remove_item (section, ii);
			break;
		}
	}
}

void
e_vcard_editor_item_add_remove_button (EVCardEditorItem *self,
				       struct _EVCardEditorSection *section)
{
	GtkWidget *trash;

	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));

	trash = gtk_button_new_from_icon_name ("user-trash", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_object_set (trash,
		"halign", GTK_ALIGN_START,
		"valign", e_vcard_editor_section_get_single_line (section) ? GTK_ALIGN_CENTER : GTK_ALIGN_END,
		"tooltip-text", _("Remove"),
		NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (trash), "flat");

	g_object_set_data (G_OBJECT (trash), ITEM_KEY, self);

	e_vcard_editor_item_add_data_widget (self, trash, FALSE);

	g_signal_connect_object (trash, "clicked",
		G_CALLBACK (eve_item_remove_button_clicked_cb), section, 0);
}

void
e_vcard_editor_item_set_user_data (EVCardEditorItem *self,
				   gpointer user_data,
				   GDestroyNotify free_func)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));

	if (self->user_data_free)
		self->user_data_free (self->user_data);

	self->user_data = user_data;
	self->user_data_free = free_func;
}

gpointer
e_vcard_editor_item_get_user_data (EVCardEditorItem *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), NULL);

	return self->user_data;
}

void
e_vcard_editor_item_set_field_id (EVCardEditorItem *self,
				  EContactField value)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));

	self->field_id = value;
}

EContactField
e_vcard_editor_item_get_field_id (EVCardEditorItem *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), E_CONTACT_FIELD_LAST);

	return self->field_id;
}

void
e_vcard_editor_item_set_permanent (EVCardEditorItem *self,
				   gboolean value)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));

	self->permanent = value;
}

gboolean
e_vcard_editor_item_get_permanent (EVCardEditorItem *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), FALSE);

	return self->permanent;
}

void
e_vcard_editor_item_fill_item (EVCardEditorItem *self,
			       struct _EVCardEditor *editor,
			       EContact *contact)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));

	if (self->fill_item_func)
		self->fill_item_func (self, editor, contact);
}

gboolean
e_vcard_editor_item_fill_contact (EVCardEditorItem *self,
				  struct _EVCardEditor *editor,
				  EContact *contact,
				  gchar **out_error_message,
				  GtkWidget **out_error_widget)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_ITEM (self), FALSE);

	if (self->fill_contact_func)
		return self->fill_contact_func (self, editor, contact, out_error_message, out_error_widget);

	return TRUE;
}

void
e_vcard_editor_item_emit_changed (EVCardEditorItem *self)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (self));

	g_signal_emit (self, signals[CHANGED], 0, NULL);
}
