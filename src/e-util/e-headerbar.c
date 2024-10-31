/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-misc-utils.h"
#include "e-headerbar-button.h"
#include "e-headerbar.h"

#define MIN_TITLE_WIDTH 100
#define BUTTON_SPACING 5

typedef struct _PriorityBasket {
	EHeaderBar *headerbar; /* (not owned) */
	GSList *widgets; /* (not owned) GtkWidget * */
	guint priority;
} PriorityBasket;

struct _EHeaderBarPrivate {
	GtkWidget *start_buttons;
	GtkWidget *end_buttons;

	GPtrArray *priorities; /* (nullable) PriorityBasket * */

	gint allocated_width;
	guint update_buttons_id;
	guint queue_resize_id;
	gboolean force_icon_only_buttons;
};

G_DEFINE_TYPE_WITH_CODE (EHeaderBar, e_header_bar, GTK_TYPE_HEADER_BAR,
	G_ADD_PRIVATE (EHeaderBar))

static void header_bar_widget_destroyed (gpointer user_data, GObject *widget);

static PriorityBasket *
priority_basket_new (EHeaderBar *headerbar,
		     guint priority)
{
	PriorityBasket *pb;

	pb = g_new0 (PriorityBasket, 1);
	pb->headerbar = headerbar;
	pb->priority = priority;

	return pb;
}

static void
priority_basket_free (gpointer ptr)
{
	PriorityBasket *pb = ptr;

	if (pb) {
		GSList *link;

		for (link = pb->widgets; link; link = g_slist_next (link)) {
			GObject *widget = link->data;

			g_object_weak_unref (widget, header_bar_widget_destroyed, pb->headerbar);
		}

		g_slist_free (pb->widgets);
		g_free (pb);
	}
}

static gint
priority_basket_compare (gconstpointer ptr1,
			 gconstpointer ptr2)
{
	const PriorityBasket *pb1 = * ((PriorityBasket **) ptr1);
	const PriorityBasket *pb2 = * ((PriorityBasket **) ptr2);

	return pb1->priority < pb2->priority ? -1 :
	       pb1->priority > pb2->priority ? 1 : 0;
}

static gboolean
header_bar_queue_resize_cb (gpointer user_data)
{
	EHeaderBar *self = user_data;

	g_return_val_if_fail (E_IS_HEADER_BAR (self), FALSE);

	self->priv->queue_resize_id = 0;

	gtk_widget_queue_resize (GTK_WIDGET (self));

	return FALSE;
}

static void
header_bar_update_buttons (EHeaderBar *self,
			   gint for_width)
{
	GtkAllocation allocation_start, allocation_end;
	GSList *labeled_groups = NULL; /* GSList { GtkWidget * }; labeled widgets, divided by groups */
	GSList *icon_only_buttons = NULL; /* GtkWidget * */
	guint ii;
	gint available_width;
	gboolean changed = FALSE;

	if (self->priv->update_buttons_id) {
		g_source_remove (self->priv->update_buttons_id);
		self->priv->update_buttons_id = 0;
	}

	/* dispose() had been called */
	if (!self->priv->priorities)
		return;

	if (self->priv->force_icon_only_buttons) {
		for (ii = 0; ii < self->priv->priorities->len; ii++) {
			PriorityBasket *bt = g_ptr_array_index (self->priv->priorities, ii);
			GSList *link;

			for (link = bt->widgets; link; link = g_slist_next (link)) {
				GtkWidget *widget = link->data;

				if (gtk_widget_is_visible (widget) && E_IS_HEADER_BAR_BUTTON (widget)) {
					EHeaderBarButton *button = E_HEADER_BAR_BUTTON (widget);
					if (!e_header_bar_button_get_show_icon_only (button)) {
						e_header_bar_button_set_show_icon_only (button, TRUE);
						changed = TRUE;
					}
				}
			}
		}
	} else {
		/* space between the start_buttons and the end_buttons is the available width,
		   including the window title */

		gtk_widget_get_allocation (self->priv->start_buttons, &allocation_start);
		gtk_widget_get_allocation (self->priv->end_buttons, &allocation_end);

		available_width = MAX (allocation_end.x + allocation_end.width - allocation_start.x
			- MIN_TITLE_WIDTH - (2 * BUTTON_SPACING), 0);

		/* when making window smaller, calculate with the new size, to have buttons of the proper size */
		if (for_width > 0) {
			gint current_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));
			if (current_width > for_width)
				available_width = MAX (0, available_width - (current_width - for_width));
		}

		for (ii = 0; ii < self->priv->priorities->len && available_width > 0; ii++) {
			PriorityBasket *bt = g_ptr_array_index (self->priv->priorities, ii);
			GSList *link;

			for (link = bt->widgets; link && available_width > 0; link = g_slist_next (link)) {
				GtkWidget *widget = link->data;

				if (gtk_widget_is_visible (widget)) {
					if (!E_IS_HEADER_BAR_BUTTON (widget)) {
						gint minimum_width = 0;

						gtk_widget_get_preferred_width (widget, &minimum_width, NULL);

						if (minimum_width > 0)
							available_width = MAX (available_width - minimum_width, 0);
					}

					available_width = MAX (available_width - BUTTON_SPACING, 0);
				}
			}
		}

		for (ii = 0; ii < self->priv->priorities->len; ii++) {
			PriorityBasket *bt = g_ptr_array_index (self->priv->priorities, ii);
			GSList *link, *labeled_widgets = NULL;

			for (link = bt->widgets; link; link = g_slist_next (link)) {
				GtkWidget *widget = link->data;

				if (gtk_widget_is_visible (widget) && E_IS_HEADER_BAR_BUTTON (widget)) {
					EHeaderBarButton *button = E_HEADER_BAR_BUTTON (widget);
					gint labeled_width = -1, icon_only_width = -1;

					e_header_bar_button_get_widths (button, &labeled_width, &icon_only_width);

					if (labeled_width > 0 && icon_only_width > 0) {
						if (available_width >= labeled_width) {
							available_width -= labeled_width;
							labeled_widgets = g_slist_prepend (labeled_widgets, button);
						} else {
							available_width -= icon_only_width;
							if (!e_header_bar_button_get_show_icon_only (button))
								icon_only_buttons = g_slist_prepend (icon_only_buttons, button);
						}
					}
				}
			}

			if (labeled_widgets)
				labeled_groups = g_slist_prepend (labeled_groups, g_slist_reverse (labeled_widgets));
		}

		if (available_width < 0 && labeled_groups) {
			GSList *lglink;

			for (lglink = labeled_groups; lglink && available_width < 0; lglink = g_slist_next (lglink)) {
				GSList *labeled_widgets = lglink->data, *lwlink;

				for (lwlink = labeled_widgets; lwlink && available_width < 0; lwlink = g_slist_next (lwlink)) {
					EHeaderBarButton *button = lwlink->data;
					gint labeled_width = -1, icon_only_width = -1;

					e_header_bar_button_get_widths (button, &labeled_width, &icon_only_width);
					icon_only_buttons = g_slist_prepend (icon_only_buttons, button);
					lwlink->data = NULL;
					available_width += labeled_width - icon_only_width;
				}
			}
		}

		if (icon_only_buttons) {
			GSList *link;

			changed = TRUE;

			for (link = icon_only_buttons; link; link = g_slist_next (link)) {
				EHeaderBarButton *button = link->data;
				e_header_bar_button_set_show_icon_only (button, TRUE);
			}

			g_slist_free (icon_only_buttons);
		}

		if (labeled_groups) {
			GSList *lglink;

			for (lglink = labeled_groups; lglink; lglink = g_slist_next (lglink)) {
				GSList *labeled_widgets = lglink->data, *lwlink;

				for (lwlink = labeled_widgets; lwlink; lwlink = g_slist_next (lwlink)) {
					EHeaderBarButton *button = lwlink->data;

					if (button && e_header_bar_button_get_show_icon_only (button)) {
						e_header_bar_button_set_show_icon_only (button, FALSE);
						changed = TRUE;
					}
				}
			}

			g_slist_free_full (labeled_groups, (GDestroyNotify) g_slist_free);
		}
	}

	if (changed && !self->priv->queue_resize_id)
		self->priv->queue_resize_id = g_idle_add (header_bar_queue_resize_cb, self);
}

static gboolean
header_bar_update_buttons_idle_cb (gpointer user_data)
{
	EHeaderBar *self = user_data;

	g_return_val_if_fail (E_IS_HEADER_BAR (self), FALSE);

	self->priv->update_buttons_id = 0;

	header_bar_update_buttons (self, -1);

	return FALSE;
}

static void
header_bar_schedule_update_buttons (EHeaderBar *self)
{
	if (self->priv->update_buttons_id ||
	    !gtk_widget_get_realized (GTK_WIDGET (self)))
		return;

	self->priv->update_buttons_id = g_idle_add (header_bar_update_buttons_idle_cb, self);
}

static void
header_bar_icon_only_buttons_setting_changed_cb (GSettings *settings,
						 const gchar *key,
						 gpointer user_data)
{
	EHeaderBar *self = user_data;
	gboolean new_value;

	new_value = g_settings_get_boolean (settings, "icon-only-buttons-in-header-bar");

	if ((new_value ? 1 : 0) != (self->priv->force_icon_only_buttons ? 1 : 0)) {
		self->priv->force_icon_only_buttons = new_value;
		header_bar_schedule_update_buttons (self);
	}
}

static void
header_bar_widget_destroyed (gpointer user_data,
			     GObject *widget)
{
	EHeaderBar *self = user_data;

	if (self->priv->priorities) {
		guint ii;

		for (ii = 0; ii < self->priv->priorities->len; ii++) {
			PriorityBasket *pb = g_ptr_array_index (self->priv->priorities, ii);

			if (g_slist_find (pb->widgets, widget)) {
				pb->widgets = g_slist_remove (pb->widgets, widget);

				if (!pb->widgets)
					g_ptr_array_remove_index (self->priv->priorities, ii);

				break;
			}
		}

		if (ii < self->priv->priorities->len)
			header_bar_schedule_update_buttons (self);
	}
}

static void
header_bar_set_label_priority (EHeaderBar *self,
			       GtkWidget *widget,
			       guint priority)
{
	PriorityBasket *pb = NULL;
	guint ii;

	/* dispose() had been called */
	if (!self->priv->priorities)
		return;

	for (ii = 0; ii < self->priv->priorities->len; ii++) {
		pb = g_ptr_array_index (self->priv->priorities, ii);
		if (pb->priority == priority)
			break;
		pb = NULL;
	}

	if (!pb) {
		pb = priority_basket_new (self, priority);
		g_ptr_array_add (self->priv->priorities, pb);
		g_ptr_array_sort (self->priv->priorities, priority_basket_compare);
	}

	g_object_weak_ref (G_OBJECT (widget), header_bar_widget_destroyed, self);

	pb->widgets = g_slist_append (pb->widgets, widget);

	header_bar_schedule_update_buttons (self);
}

static void
header_bar_size_allocate (GtkWidget *widget,
			  GtkAllocation *allocation)
{
	EHeaderBar *self = E_HEADER_BAR (widget);

	/* prepare buttons for after-allocation size */
	if (self->priv->allocated_width != allocation->width)
		header_bar_update_buttons (self, allocation->width);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_header_bar_parent_class)->size_allocate (widget, allocation);

	/* apply the new size */
	if (self->priv->allocated_width != allocation->width) {
		self->priv->allocated_width = allocation->width;
		header_bar_update_buttons (self, -1);
	}
}

static void
header_bar_realize (GtkWidget *widget)
{
	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_header_bar_parent_class)->realize (widget);

	header_bar_update_buttons (E_HEADER_BAR (widget), -1);
}

static void
header_bar_map (GtkWidget *widget)
{
	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_header_bar_parent_class)->map (widget);

	header_bar_update_buttons (E_HEADER_BAR (widget), -1);
}

static void
header_bar_get_preferred_width (GtkWidget *in_widget,
				gint *minimum_width,
				gint *natural_width)
{
	EHeaderBar *self = E_HEADER_BAR (in_widget);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_header_bar_parent_class)->get_preferred_width (in_widget, minimum_width, natural_width);

	if (!self->priv->force_icon_only_buttons) {
		gint decrement_minimum = 0;
		guint ii;

		for (ii = 0; ii < self->priv->priorities->len; ii++) {
			PriorityBasket *bt = g_ptr_array_index (self->priv->priorities, ii);
			GSList *link;

			for (link = bt->widgets; link; link = g_slist_next (link)) {
				GtkWidget *widget = link->data;

				if (gtk_widget_is_visible (widget) && E_IS_HEADER_BAR_BUTTON (widget)) {
					EHeaderBarButton *button = E_HEADER_BAR_BUTTON (widget);
					if (!e_header_bar_button_get_show_icon_only (button)) {
						gint labeled_width = -1, icon_only_width = -1;

						e_header_bar_button_get_widths (button, &labeled_width, &icon_only_width);

						/* it's showing the labeled button now, and when it's switched
						   to the icon-only button, it'll use this less space, which can
						   be subtracted from the current minimum width */
						if (icon_only_width > 0 && labeled_width > icon_only_width)
							decrement_minimum += labeled_width - icon_only_width;
					}
				}
			}
		}

		if (decrement_minimum > 0 && *minimum_width > decrement_minimum)
			*minimum_width -= decrement_minimum;
	}
}

static void
header_bar_constructed (GObject *object)
{
	EHeaderBar *self = E_HEADER_BAR (object);
	GSettings *settings;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_header_bar_parent_class)->constructed (object);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	self->priv->force_icon_only_buttons = g_settings_get_boolean (settings, "icon-only-buttons-in-header-bar");
	g_signal_connect_object (settings, "changed::icon-only-buttons-in-header-bar",
		G_CALLBACK (header_bar_icon_only_buttons_setting_changed_cb), self, 0);
	g_clear_object (&settings);

	self->priv->start_buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BUTTON_SPACING);
	gtk_header_bar_pack_start (GTK_HEADER_BAR (self), self->priv->start_buttons);
	gtk_widget_show (self->priv->start_buttons);

	self->priv->end_buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BUTTON_SPACING);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (self), self->priv->end_buttons);
	gtk_widget_show (self->priv->end_buttons);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "titlebar");
}

static void
header_bar_dispose (GObject *object)
{
	EHeaderBar *self = E_HEADER_BAR (object);

	if (self->priv->update_buttons_id) {
		g_source_remove (self->priv->update_buttons_id);
		self->priv->update_buttons_id = 0;
	}

	if (self->priv->queue_resize_id) {
		g_source_remove (self->priv->queue_resize_id);
		self->priv->queue_resize_id = 0;
	}

	g_clear_pointer (&self->priv->priorities, g_ptr_array_unref);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_header_bar_parent_class)->dispose (object);
}

static void
e_header_bar_class_init (EHeaderBarClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->size_allocate = header_bar_size_allocate;
	widget_class->realize = header_bar_realize;
	widget_class->map = header_bar_map;
	widget_class->get_preferred_width = header_bar_get_preferred_width;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = header_bar_constructed;
	object_class->dispose = header_bar_dispose;
}

static void
e_header_bar_init (EHeaderBar *self)
{
	self->priv = e_header_bar_get_instance_private (self);
	self->priv->priorities = g_ptr_array_new_with_free_func (priority_basket_free);

	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self), TRUE);
}

/**
 * e_header_bar_new:
 *
 * Creates a new #EHeaderBar
 *
 * Returns: (transfer full): a new #EHeaderBar
 *
 * Since: 3.48
 **/
GtkWidget *
e_header_bar_new (void)
{
	return g_object_new (E_TYPE_HEADER_BAR,
		"has-subtitle", FALSE,
		NULL);
}

/**
 * e_header_bar_pack_start:
 * @self: an #EHeaderBar
 * @widget: widget to pack
 * @label_priority: priority to show a label
 *
 * Adds child to bar, packed with reference to the start of the bar.
 *
 * The @label_priority is to set for the @widget, if it's an #EHeaderBarButton,
 * priority to show a label on the button, when such is set. The lower number
 * the priority is, the sooner the label will be shown.
 *
 * Since: 3.48
 **/
void
e_header_bar_pack_start (EHeaderBar *self,
			 GtkWidget *widget,
			 guint label_priority)
{
	g_return_if_fail (E_IS_HEADER_BAR (self));

	gtk_box_pack_start (GTK_BOX (self->priv->start_buttons), widget, FALSE, FALSE, 0);

	header_bar_set_label_priority (self, widget, label_priority);
}

/**
 * e_header_bar_pack_end:
 * @self: an #EHeaderBar
 * @widget: widget to pack
 * @label_priority: priority to show a label
 *
 * Adds child to bar, packed with reference to the end of the bar.
 *
 * The @label_priority is to set for the @widget, if it's an #EHeaderBarButton,
 * priority to show a label on the button, when such is set. The lower number
 * the priority is, the sooner the label will be shown.
 *
 * Since: 3.48
 **/
void
e_header_bar_pack_end (EHeaderBar *self,
		       GtkWidget *widget,
		       guint label_priority)
{
	g_return_if_fail (E_IS_HEADER_BAR (self));

	gtk_box_pack_end (GTK_BOX (self->priv->end_buttons), widget, FALSE, FALSE, 0);

	header_bar_set_label_priority (self, widget, label_priority);
}

/**
 * e_header_bar_remove_all:
 * @self: an #EHeaderBar
 *
 * Removes all children of the @self added by e_header_bar_pack_start()
 * and e_header_bar_pack_end().
 *
 * Since: 3.56
 **/
void
e_header_bar_remove_all (EHeaderBar *self)
{
	GtkContainer *container;
	GList *children, *link;

	g_return_if_fail (E_IS_HEADER_BAR (self));

	container = GTK_CONTAINER (self->priv->start_buttons);
	children = gtk_container_get_children (container);
	for (link = children; link; link = g_list_next (link)) {
		gtk_container_remove (container, link->data);
	}
	g_list_free (children);

	container = GTK_CONTAINER (self->priv->end_buttons);
	children = gtk_container_get_children (container);
	for (link = children; link; link = g_list_next (link)) {
		gtk_container_remove (container, link->data);
	}
	g_list_free (children);
}

/**
 * e_header_bar_get_start_widgets:
 * @self: an #EHeaderBar
 *
 * Returns widgets packed at the start. Free the list with g_list_free(),
 * when no longer needed.
 *
 * Returns: (transfer container): widgets packed at the start
 *
 * Since: 3.48
 **/
GList *
e_header_bar_get_start_widgets (EHeaderBar *self)
{
	g_return_val_if_fail (E_IS_HEADER_BAR (self), NULL);

	return gtk_container_get_children (GTK_CONTAINER (self->priv->start_buttons));
}

/**
 * e_header_bar_get_end_widgets:
 * @self: an #EHeaderBar
 *
 * Returns widgets packed at the end. Free the list with g_list_free(),
 * when no longer needed.
 *
 * Returns: (transfer container): widgets packed at the end
 *
 * Since: 3.48
 **/
GList *
e_header_bar_get_end_widgets (EHeaderBar *self)
{
	g_return_val_if_fail (E_IS_HEADER_BAR (self), NULL);

	return gtk_container_get_children (GTK_CONTAINER (self->priv->end_buttons));
}
