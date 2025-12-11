/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-vcard-editor-item.h"
#include "e-vcard-editor-section.h"

struct _EVCardEditorSection {
	GtkExpander parent_object;

	EVCardEditorSectionReadFunc fill_section_func;
	EVCardEditorSectionWriteFunc fill_contact_func;
	GCompareFunc sort_func;
	GPtrArray *items; /* EVCardEditorItem * */
	GPtrArray *widgets; /* GtkWidget * */

	struct _EVCardEditor *editor; /* not owned */
	GtkBox *top_box; /* not owned */
	GtkGrid *grid; /* not owned */
	GtkWidget *add_button; /* owned */

	gboolean changed;
	gboolean single_line;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EVCardEditorSection, e_vcard_editor_section, GTK_TYPE_EXPANDER)

static void
eve_section_maybe_move_add_button (EVCardEditorSection *self)
{
	gint y_pos;

	if (!self->grid || !self->add_button || !self->items || !gtk_widget_get_visible (self->add_button))
		return;

	gtk_container_remove (GTK_CONTAINER (self->grid), self->add_button);

	y_pos = self->items->len > 0 ? self->items->len - 1 : 0;
	gtk_grid_attach (self->grid, self->add_button, 2, y_pos, 1, 1);
}

static void
eve_section_notify_expanded_cb (GObject *object,
				GParamSpec *param,
				gpointer user_data)
{
	EVCardEditorSection *self = E_VCARD_EDITOR_SECTION (object);

	if (self->top_box)
		gtk_widget_set_sensitive (GTK_WIDGET (self->top_box), gtk_expander_get_expanded (GTK_EXPANDER (self)));
}

static void
e_vcard_editor_section_show_all (GtkWidget *widget)
{
	EVCardEditorSection *self = E_VCARD_EDITOR_SECTION (widget);
	gboolean was_visible = self->add_button ? gtk_widget_get_visible (self->add_button) : FALSE;

	GTK_WIDGET_CLASS (e_vcard_editor_section_parent_class)->show_all (widget);

	if (self->add_button)
		gtk_widget_set_visible (self->add_button, was_visible);
}

static void
e_vcard_editor_section_dispose (GObject *object)
{
	EVCardEditorSection *self = E_VCARD_EDITOR_SECTION (object);

	self->editor = NULL;
	self->grid = NULL;
	self->top_box = NULL;
	g_clear_object (&self->add_button);
	g_clear_pointer (&self->items, g_ptr_array_unref);
	g_clear_pointer (&self->widgets, g_ptr_array_unref);

	G_OBJECT_CLASS (e_vcard_editor_section_parent_class)->dispose (object);
}

static void
e_vcard_editor_section_class_init (EVCardEditorSectionClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_vcard_editor_section_dispose;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show_all = e_vcard_editor_section_show_all;

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
e_vcard_editor_section_init (EVCardEditorSection *self)
{
	GtkWidget *widget;

	self->items = g_ptr_array_new_with_free_func (g_object_unref);
	self->single_line = TRUE;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	g_object_set (widget,
		"visible", TRUE,
		"margin-start", 12,
		NULL);
	gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "vertical");
	gtk_container_add (GTK_CONTAINER (self), widget);

	self->top_box = GTK_BOX (widget);

	widget = gtk_grid_new ();
	g_object_set (widget,
		"visible", TRUE,
		"margin-start", 0,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);
	gtk_box_pack_start (self->top_box, widget, FALSE, TRUE, 0);

	self->grid = GTK_GRID (widget);

	widget = gtk_button_new_from_icon_name ("list-add", GTK_ICON_SIZE_BUTTON);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "flat");
	g_object_set (widget,
		"visible", FALSE,
		"tooltip-text", _("Add"),
		"always-show-image", TRUE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_grid_attach (self->grid, widget, 2, 0, 1, 1);

	self->add_button = g_object_ref_sink (widget);

	g_signal_connect (self, "notify::expanded",
		G_CALLBACK (eve_section_notify_expanded_cb), NULL);
}

GtkWidget *
e_vcard_editor_section_new (struct _EVCardEditor *editor, /* transfer none */
			    const gchar *label,
			    EVCardEditorSectionReadFunc fill_section_func,
			    EVCardEditorSectionWriteFunc fill_contact_func)
{
	EVCardEditorSection *self;
	GtkWidget *widget;
	PangoAttrList *bold;

	self = g_object_new (E_TYPE_VCARD_EDITOR_SECTION,
		"can-focus", TRUE,
		"vexpand", FALSE,
		NULL);

	self->editor = editor;
	self->fill_section_func = fill_section_func;
	self->fill_contact_func = fill_contact_func;

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	widget = gtk_label_new (label);
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		"visible", TRUE,
		"attributes", bold,
		"margin-start", 4,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
	gtk_expander_set_label_widget (GTK_EXPANDER (self), widget);

	g_clear_pointer (&bold, pango_attr_list_unref);

	return GTK_WIDGET (self);
}

struct _EVCardEditor *
e_vcard_editor_section_get_editor (EVCardEditorSection *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), NULL);

	return self->editor;
}

GtkButton *
e_vcard_editor_section_get_add_button (EVCardEditorSection *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), NULL);

	if (!self->add_button)
		return NULL;

	return GTK_BUTTON (self->add_button);
}

void
e_vcard_editor_section_set_sort_function (EVCardEditorSection *self,
					  GCompareFunc sort_func)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	self->sort_func = sort_func;
}

static void
e_vcard_editor_section_item_changed_cb (EVCardEditorSection *self)
{
	self->changed = TRUE;

	g_signal_emit (self, signals[CHANGED], 0, NULL);
}

gboolean
e_vcard_editor_section_get_changed (EVCardEditorSection *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), FALSE);

	return self->changed;
}

void
e_vcard_editor_section_set_changed (EVCardEditorSection *self,
				    gboolean value)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	self->changed = value;
}

gboolean
e_vcard_editor_section_get_single_line (EVCardEditorSection *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), FALSE);

	return self->single_line;
}

void
e_vcard_editor_section_set_single_line (EVCardEditorSection *self,
					gboolean value)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	self->single_line = value;

	if (self->add_button)
		gtk_widget_set_valign (self->add_button, self->single_line ? GTK_ALIGN_CENTER : GTK_ALIGN_END);
}

void
e_vcard_editor_section_take_item (EVCardEditorSection *self,
				  EVCardEditorItem *item)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));
	g_return_if_fail (E_IS_VCARD_EDITOR_ITEM (item));

	if (self->items) {
		GtkWidget *label;
		GtkWidget *box;
		gint index = self->items->len;

		if (self->sort_func) {
			guint ii;

			for (ii = 0; ii < self->items->len; ii++) {
				EVCardEditorItem *existing_item = g_ptr_array_index (self->items, ii);

				if (self->sort_func (existing_item, item) > 0) {
					GtkContainer *container = GTK_CONTAINER (self->grid);
					gint top;

					index = ii;

					for (; ii < self->items->len; ii++) {
						existing_item = g_ptr_array_index (self->items, ii);
						label = e_vcard_editor_item_get_label_widget (existing_item);
						box = e_vcard_editor_item_get_data_box (existing_item);

						gtk_container_child_get (container, box, "top-attach", &top, NULL);
						top++;

						if (label)
							gtk_container_child_set (container, label, "top-attach", top, NULL);

						gtk_container_child_set (container, box, "top-attach", top, NULL);
					}
					break;
				}
			}
		}

		label = e_vcard_editor_item_get_label_widget (item);
		box = e_vcard_editor_item_get_data_box (item);

		if (label)
			gtk_grid_attach (self->grid, label, 0, index, 1, 1);

		gtk_grid_attach (self->grid, box, 1, index, 1, 1);

		g_ptr_array_insert (self->items, index, item);

		g_signal_connect_object (item, "changed", G_CALLBACK (e_vcard_editor_section_item_changed_cb), self, G_CONNECT_SWAPPED);
		gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
		gtk_expander_set_expanded (GTK_EXPANDER (self), TRUE);

		eve_section_maybe_move_add_button (self);
		self->changed = TRUE;

		g_signal_emit (self, signals[CHANGED], 0, NULL);
	} else {
		g_clear_object (&item);
	}
}

guint
e_vcard_editor_section_get_n_items (EVCardEditorSection *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), 0);

	return self->items ? self->items->len : 0;
}

EVCardEditorItem *
e_vcard_editor_section_get_item	(EVCardEditorSection *self,
				 guint index)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), NULL);

	if (!self->items || index >= self->items->len)
		return NULL;

	return g_ptr_array_index (self->items, index);
}

void
e_vcard_editor_section_remove_item (EVCardEditorSection *self,
				    guint index)
{
	EVCardEditorItem *item;

	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	item = e_vcard_editor_section_get_item (self, index);
	if (!item)
		return;

	gtk_grid_remove_row (self->grid, index);

	/* it also frees the 'item' */
	g_ptr_array_remove_index (self->items, index);
	gtk_widget_set_visible (GTK_WIDGET (self), self->items->len > 0);

	eve_section_maybe_move_add_button (self);
	self->changed = TRUE;

	g_signal_emit (self, signals[CHANGED], 0, NULL);
}

void
e_vcard_editor_section_remove_dynamic (EVCardEditorSection *self)
{
	guint ii;
	gboolean changed = FALSE;

	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	if (!self->items)
		return;

	for (ii = 0; ii < self->items->len; ii++) {
		guint index = self->items->len - ii - 1;
		EVCardEditorItem *item = g_ptr_array_index (self->items, index);

		if (!e_vcard_editor_item_get_permanent (item)) {
			gtk_grid_remove_row (self->grid, index);
			g_ptr_array_remove_index (self->items, index);
			ii--;
			changed = TRUE;
		}
	}

	gtk_widget_set_visible (GTK_WIDGET (self), self->items->len > 0);

	if (changed) {
		eve_section_maybe_move_add_button (self);
		self->changed = TRUE;

		g_signal_emit (self, signals[CHANGED], 0, NULL);
	}
}

void
e_vcard_editor_section_remove_all (EVCardEditorSection *self)
{
	guint ii;

	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	if (self->items && self->items->len > 0) {
		eve_section_maybe_move_add_button (self);
		self->changed = TRUE;
	}

	for (ii = 0; self->items && ii < self->items->len; ii++) {
		gtk_grid_remove_row (self->grid, self->items->len - ii - 1);
	}

	g_ptr_array_remove_range (self->items, 0, self->items->len);
	gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

	g_signal_emit (self, signals[CHANGED], 0, NULL);
}

void
e_vcard_editor_section_take_widget (EVCardEditorSection *self,
				    GtkWidget *widget, /* transfer full */
				    GtkPackType pack_type)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	widget = g_object_ref_sink (widget);

	/* really items, which indicates whether dispose() was called;
	   the 'widgets' is created on demand */
	if (!self->items) {
		g_object_unref (widget);
		return;
	}

	if (!self->widgets)
		self->widgets = g_ptr_array_new_with_free_func (g_object_unref);

	g_ptr_array_add (self->widgets, widget);

	if (pack_type == GTK_PACK_START)
		gtk_box_pack_start (self->top_box, widget, FALSE, TRUE, 0);
	else
		gtk_box_pack_end (self->top_box, widget, FALSE, TRUE, 0);
}

void
e_vcard_editor_section_remove_widget (EVCardEditorSection *self,
				      guint index)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));

	if (!self->widgets || index >= self->widgets->len)
		return;

	widget = g_ptr_array_index (self->widgets, index);
	gtk_container_remove (GTK_CONTAINER (self->top_box), widget);
	g_ptr_array_remove_index (self->widgets, index);
}

guint
e_vcard_editor_section_get_n_widgets (EVCardEditorSection *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), 0);

	return self->widgets ? self->widgets->len : 0;
}

GtkWidget * /* transfer none */
e_vcard_editor_section_get_widget (EVCardEditorSection *self,
				   guint index)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), NULL);

	if (!self->widgets || index >= self->widgets->len)
		return NULL;

	return g_ptr_array_index (self->widgets, index);
}

void
e_vcard_editor_section_fill_section (EVCardEditorSection *self,
				     EContact *contact)
{
	guint ii;

	g_return_if_fail (E_IS_VCARD_EDITOR_SECTION (self));
	g_return_if_fail (E_IS_CONTACT (contact));

	if (self->fill_section_func)
		self->fill_section_func (self, contact);

	for (ii = 0; self->items && ii < self->items->len; ii++) {
		EVCardEditorItem *item = g_ptr_array_index (self->items, ii);

		e_vcard_editor_item_fill_item (item, self->editor, contact);
	}
}

gboolean
e_vcard_editor_section_fill_contact (EVCardEditorSection *self,
				     EContact *contact,
				     gchar **out_error_message,
				     GtkWidget **out_error_widget)
{
	guint ii;

	g_return_val_if_fail (E_IS_VCARD_EDITOR_SECTION (self), FALSE);
	g_return_val_if_fail (E_IS_CONTACT (contact), FALSE);

	if (self->fill_contact_func) {
		if (!self->fill_contact_func (self, contact, out_error_message, out_error_widget))
			return FALSE;
	}

	for (ii = 0; self->items && ii < self->items->len; ii++) {
		EVCardEditorItem *item = g_ptr_array_index (self->items, ii);

		if (!e_vcard_editor_item_fill_contact (item, self->editor, contact, out_error_message, out_error_widget))
			return FALSE;
	}

	return TRUE;
}
