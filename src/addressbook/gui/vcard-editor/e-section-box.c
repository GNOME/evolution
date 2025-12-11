/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-section-box.h"

#define SPACING 12
#define MAX_COLUMNS 32

struct _ESectionBox {
	GtkBox parent_object;

	GPtrArray *widgets; /* owned, GtkWidget * */
	GtkBox *columns_box;
	GtkBox *column_boxes[MAX_COLUMNS];

	gint last_allocated_width;
	gint last_n_columns;
	gint last_n_shown;
	guint idle_layout_id;
};

G_DEFINE_TYPE (ESectionBox, e_section_box, GTK_TYPE_BOX)

static void
e_section_box_layout (ESectionBox *self)
{
	gint max_width = 0, max_height = 0, total_height = 0, mid_height, cur_height;
	guint ii, n_shown = 0, picked_column;
	guint n_columns;

	if (!self->widgets)
		return;

	for (ii = 0; ii < self->widgets->len; ii++) {
		GtkWidget *widget = g_ptr_array_index (self->widgets, ii);
		GtkRequisition min_req = { 0, 0 };

		if (!gtk_widget_get_visible (widget))
			continue;

		gtk_widget_get_preferred_size (widget, &min_req, NULL);

		if (max_width < min_req.width)
			max_width = min_req.width;
		if (max_height < min_req.height)
			max_height = min_req.height;

		total_height += min_req.height;
		n_shown++;
	}

	if (n_shown > 1 && max_width > 0) {
		gint allocated_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));

		if (allocated_width > self->last_allocated_width)
			allocated_width = self->last_allocated_width;

		n_columns = allocated_width / max_width;
		if (n_columns != (allocated_width - (SPACING * (n_columns - 1))) / max_width)
			n_columns--;
		if (n_columns < 1)
			n_columns = 1;
		else if (n_columns > MAX_COLUMNS)
			n_columns = MAX_COLUMNS;
	} else {
		n_columns = 1;
	}

	if (self->last_n_columns == n_columns && self->last_n_shown == n_shown)
		return;

	if (n_shown > 1)
		total_height += (n_shown - 1) * SPACING;

	self->last_n_columns = n_columns;
	self->last_n_shown = n_shown;

	picked_column = 0;
	cur_height = 0;
	mid_height = total_height / n_columns;

	if (mid_height < max_height)
		mid_height = max_height;

	for (ii = 0; ii < self->widgets->len; ii++) {
		GtkWidget *widget = g_ptr_array_index (self->widgets, ii);
		GtkBox *column_box;

		if (gtk_widget_get_parent (widget))
			gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (widget)), widget);

		if (!gtk_widget_get_visible (widget))
			continue;

		if (picked_column + 1 < n_columns) {
			GtkRequisition min_req = { 0, 0 };

			gtk_widget_get_preferred_size (widget, &min_req, NULL);

			if (cur_height + min_req.height + (cur_height ? SPACING : 0) > mid_height) {
				picked_column++;
				cur_height = min_req.height;
			} else {
				cur_height += min_req.height + (cur_height ? SPACING : 0);
			}
		}

		column_box = self->column_boxes[picked_column];
		if (!column_box) {
			GtkWidget *box_widget;

			box_widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, SPACING);
			gtk_widget_set_visible (box_widget, TRUE);
			gtk_box_pack_start (self->columns_box, box_widget, FALSE, TRUE, 0);

			column_box = GTK_BOX (box_widget);

			self->column_boxes[picked_column] = column_box;
		} else {
			gtk_widget_set_visible (GTK_WIDGET (column_box), TRUE);
		}

		gtk_box_pack_start (column_box, widget, FALSE, TRUE, 0);
	}

	for (ii = n_columns; ii < MAX_COLUMNS && self->column_boxes[ii]; ii++) {
		gtk_widget_set_visible (GTK_WIDGET (self->column_boxes[ii]), FALSE);
	}
}

static void
e_section_box_notify_child_visible_cb (GObject *object,
				       GParamSpec *param,
				       gpointer user_data)
{
	ESectionBox *self = user_data;

	e_section_box_layout (self);
}

static gboolean
e_section_box_idle_layout_cb (gpointer user_data)
{
	ESectionBox *self = user_data;

	self->idle_layout_id = 0;

	e_section_box_layout (self);

	return G_SOURCE_REMOVE;
}

static void
e_section_box_container_size_allocated_cb (GtkWidget *container,
					   GdkRectangle *allocation,
					   gpointer user_data)
{
	ESectionBox *self = user_data;
	GtkWidget *scrollbar;
	gint allocated_width;

	if (!self->widgets)
		return;

	allocated_width = gtk_widget_get_allocated_width (container);
	scrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (container));

	if (scrollbar && gtk_widget_get_visible (scrollbar))
		allocated_width -= gtk_widget_get_allocated_width (scrollbar);

	if (self->last_allocated_width != allocated_width) {
		self->last_allocated_width = allocated_width;

		if (!self->idle_layout_id)
			self->idle_layout_id = g_idle_add (e_section_box_idle_layout_cb, self);
	}
}

static void
e_section_box_constructed (GObject *object)
{
	ESectionBox *self = E_SECTION_BOX (object);
	GtkWidget *widget;

	G_OBJECT_CLASS (e_section_box_parent_class)->constructed (object);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, SPACING);
	gtk_box_pack_start (GTK_BOX (self), widget, FALSE, TRUE, 0);

	self->columns_box = GTK_BOX (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, SPACING);
	gtk_box_pack_start (self->columns_box, widget, FALSE, TRUE, 0);
	self->column_boxes[0] = GTK_BOX (widget);
}

static void
e_section_box_dispose (GObject *object)
{
	ESectionBox *self = E_SECTION_BOX (object);
	guint ii;

	if (self->idle_layout_id) {
		g_source_remove (self->idle_layout_id);
		self->idle_layout_id = 0;
	}

	for (ii = 0; self->widgets && ii < self->widgets->len; ii++) {
		GtkWidget *child = g_ptr_array_index (self->widgets, ii);

		g_signal_handlers_disconnect_by_func (child, G_CALLBACK (e_section_box_notify_child_visible_cb), self);
	}

	g_clear_pointer (&self->widgets, g_ptr_array_unref);

	self->columns_box = NULL;

	for (ii = 0; ii < MAX_COLUMNS; ii++) {
		self->column_boxes[ii] = NULL;
	}

	G_OBJECT_CLASS (e_section_box_parent_class)->dispose (object);
}

static void
e_section_box_class_init (ESectionBoxClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_section_box_constructed;
	object_class->dispose = e_section_box_dispose;
}

static void
e_section_box_init (ESectionBox *self)
{
	self->widgets = g_ptr_array_new_with_free_func (g_object_unref);
}

GtkWidget *
e_section_box_new (void)
{
	return g_object_new (E_TYPE_SECTION_BOX,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"spacing", SPACING,
		NULL);
}

void
e_section_box_add (ESectionBox *self,
		   GtkWidget *widget)
{
	g_return_if_fail (E_IS_SECTION_BOX (self));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (self->widgets != NULL);

	g_ptr_array_add (self->widgets, g_object_ref_sink (widget));

	g_signal_connect (widget, "notify::visible",
		G_CALLBACK (e_section_box_notify_child_visible_cb), self);

	if (gtk_widget_get_visible (widget))
		e_section_box_layout (self);
}

void
e_section_box_connect_parent_container (ESectionBox *self,
					GtkWidget *container)
{
	g_return_if_fail (E_IS_SECTION_BOX (self));
	g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));

	g_signal_connect_object (container, "size-allocate",
		G_CALLBACK (e_section_box_container_size_allocated_cb), self, 0);
}
