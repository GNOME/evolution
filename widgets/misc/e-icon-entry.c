/*
 *  e-icon-entry.c
 *
 *  Author: Johnny Jacob <jjohnny@novell.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Copyright (C) 2003, 2004, 2005  Christian Persch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *  Adapted and modified from gtk+ code:
 *
 *  Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *  Modified by the GTK+ Team and others 1997-2005.  See the AUTHORS
 *  file in the gtk+ distribution for a list of people on the GTK+ Team.
 *  See the ChangeLog in the gtk+ distribution files for a list of changes.
 *  These files are distributed with GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 *
 */

#include "config.h"

#include "e-icon-entry.h"

#define E_ICON_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), E_TYPE_ICON_ENTRY, EIconEntryPrivate))

struct _EIconEntryPrivate
{
	GtkWidget *hbox;
};

static GtkWidgetClass *parent_class = NULL;

/* private helper functions */

static gboolean
entry_focus_change_cb (GtkWidget *widget,
		       GdkEventFocus *event,
		       GtkWidget *entry)
{
	gtk_widget_queue_draw (entry);

	return FALSE;
}

static void
e_icon_entry_get_borders (GtkWidget *widget,
			     GtkWidget *entry,
			     gint *xborder,
			     gint *yborder)
{
	gint focus_width;
	gboolean interior_focus;

	g_return_if_fail (entry->style != NULL);

	gtk_widget_style_get (entry,
			      "focus-line-width", &focus_width,
			      "interior-focus", &interior_focus,
			      NULL);

	*xborder = entry->style->xthickness;
	*yborder = entry->style->ythickness;

	if (!interior_focus)
	{
		*xborder += focus_width;
		*yborder += focus_width;
	}
}

static void
e_icon_entry_paint (GtkWidget *widget,
		       GdkEventExpose *event)
{
	EIconEntry *entry = E_ICON_ENTRY (widget);
	GtkWidget *entry_widget = entry->entry;
	gint x = 0, y = 0, width, height, focus_width;
	gboolean interior_focus;

	gtk_widget_style_get (entry_widget,
			      "interior-focus", &interior_focus,
			      "focus-line-width", &focus_width,
			      NULL);

	gdk_drawable_get_size (widget->window, &width, &height);

	if (GTK_WIDGET_HAS_FOCUS (entry_widget) && !interior_focus)
	{
		x += focus_width;
		y += focus_width;
		width -= 2 * focus_width;
		height -= 2 * focus_width;
	}

	gtk_paint_flat_box (entry_widget->style, widget->window,
			    GTK_WIDGET_STATE (entry_widget), GTK_SHADOW_NONE,
			    NULL, entry_widget, "entry_bg",
			    /* FIXME: was 0, 0 in gtk_entry_expose, but I think this is correct: */
			    x, y, width, height);

	gtk_paint_shadow (entry_widget->style, widget->window,
			  GTK_STATE_NORMAL, GTK_SHADOW_IN,
			  NULL, entry_widget, "entry",
			  x, y, width, height);

	if (GTK_WIDGET_HAS_FOCUS (entry_widget) && !interior_focus)
	{
		x -= focus_width;
		y -= focus_width;
		width += 2 * focus_width;
		height += 2 * focus_width;

		gtk_paint_focus (entry_widget->style, widget->window,
				 GTK_WIDGET_STATE (entry_widget),
				 NULL, entry_widget, "entry",
				 /* FIXME: was 0, 0 in gtk_entry_draw_frame, but I think this is correct: */
				 x, y, width, height);
	}
}

/* Class implementation */

static void
e_icon_entry_init (EIconEntry *entry)
{
	EIconEntryPrivate *priv;
	GtkWidget *widget = (GtkWidget *) entry;

	priv = entry->priv = E_ICON_ENTRY_GET_PRIVATE (entry);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_NO_WINDOW);

	priv->hbox = gtk_hbox_new (FALSE, /* FIXME */ 0);
	gtk_container_add (GTK_CONTAINER (entry), priv->hbox);

	entry->entry = gtk_entry_new ();
	gtk_entry_set_has_frame (GTK_ENTRY (entry->entry), FALSE);
	gtk_box_pack_start (GTK_BOX (priv->hbox), entry->entry, TRUE, TRUE, /* FIXME */ 0);

	/* We need to queue a redraw when focus changes, to comply with themes
	 * (like Clearlooks) which draw focused and unfocused entries differently.
	 */
	g_signal_connect_after (entry->entry, "focus-in-event",
				G_CALLBACK (entry_focus_change_cb), entry);
	g_signal_connect_after (entry->entry, "focus-out-event",
				G_CALLBACK (entry_focus_change_cb), entry);
}

static void
e_icon_entry_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	gint border_width;

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	border_width = GTK_CONTAINER (widget)->border_width;

	attributes.x = widget->allocation.x + border_width;
	attributes.y = widget->allocation.y + border_width;
	attributes.width = widget->allocation.width - 2 * border_width;
	attributes.height = widget->allocation.height - 2 * border_width;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.event_mask = gtk_widget_get_events (widget)
				| GDK_EXPOSURE_MASK;

	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
e_icon_entry_size_request (GtkWidget *widget,
			      GtkRequisition *requisition)
{
	EIconEntry *entry = E_ICON_ENTRY (widget);
	GtkContainer *container = GTK_CONTAINER (widget);
	GtkBin *bin = GTK_BIN (widget);
	gint xborder, yborder;

	requisition->width = requisition->height = container->border_width * 2;

	gtk_widget_ensure_style (entry->entry);
	e_icon_entry_get_borders (widget, entry->entry, &xborder, &yborder);

	if (GTK_WIDGET_VISIBLE (bin->child))
	{
		GtkRequisition child_requisition;

		gtk_widget_size_request (bin->child, &child_requisition);
		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}

	requisition->width += 2 * xborder;
	requisition->height += 2 * yborder;
}

static void
e_icon_entry_size_allocate (GtkWidget *widget,
			       GtkAllocation *allocation)
{
	EIconEntry *entry = E_ICON_ENTRY (widget);
	GtkContainer *container = GTK_CONTAINER (widget);
	GtkBin *bin = GTK_BIN (widget);
	GtkAllocation child_allocation;
	gint xborder, yborder;

	widget->allocation = *allocation;

	e_icon_entry_get_borders (widget, entry->entry, &xborder, &yborder);

	if (GTK_WIDGET_REALIZED (widget))
	{
		child_allocation.x = container->border_width;
		child_allocation.y = container->border_width;
		child_allocation.width = MAX (allocation->width - container->border_width * 2, 0);
		child_allocation.height = MAX (allocation->height - container->border_width * 2, 0);

		gdk_window_move_resize (widget->window,
					allocation->x + child_allocation.x,
					allocation->y + child_allocation.y,
					child_allocation.width,
					child_allocation.height);
	}

	child_allocation.x = container->border_width + xborder;
	child_allocation.y = container->border_width + yborder;
	child_allocation.width = MAX (allocation->width - (container->border_width + xborder) * 2, 0);
	child_allocation.height = MAX (allocation->height - (container->border_width + yborder) * 2, 0);

	gtk_widget_size_allocate (bin->child, &child_allocation);
}

static gboolean
e_icon_entry_expose (GtkWidget *widget,
			GdkEventExpose *event)
{
	if (GTK_WIDGET_DRAWABLE (widget) &&
	    event->window == widget->window)
	{
		e_icon_entry_paint (widget, event);
	}

	return parent_class->expose_event (widget, event);
}

static void
e_icon_entry_class_init (EIconEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = GTK_WIDGET_CLASS (g_type_class_peek_parent (klass));

	widget_class->realize = e_icon_entry_realize;
	widget_class->size_request = e_icon_entry_size_request;
	widget_class->size_allocate = e_icon_entry_size_allocate;
	widget_class->expose_event = e_icon_entry_expose;

	g_type_class_add_private (object_class, sizeof (EIconEntryPrivate));
}

GType
e_icon_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EIconEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) e_icon_entry_class_init,
			NULL,
			NULL,
			sizeof (EIconEntry),
			0,
			(GInstanceInitFunc) e_icon_entry_init
		};

		type = g_type_register_static (GTK_TYPE_BIN,
					       "EIconEntry",
					       &our_info, 0);
	}

	return type;
}

/* public functions */

GtkWidget *
e_icon_entry_new (void)
{
	return GTK_WIDGET (g_object_new (E_TYPE_ICON_ENTRY, NULL));
}

void
e_icon_entry_pack_widget (EIconEntry *entry,
			     GtkWidget *widget,
			     gboolean start)
{
	EIconEntryPrivate *priv;

	g_return_if_fail (E_IS_ICON_ENTRY (entry));

	priv = entry->priv;

	if (start)
	{
		gtk_box_pack_start (GTK_BOX (priv->hbox), widget, FALSE, FALSE, /* FIXME */ 2);
		gtk_box_reorder_child (GTK_BOX (priv->hbox), widget, 0);
	}
	else
	{
		gtk_box_pack_end (GTK_BOX (priv->hbox), widget, FALSE, FALSE, /* FIXME */ 2);
	}
}

static void
set_cursor (GtkWidget *widget, GdkEventCrossing *event, gpointer dummy)
{

    if (event->type == GDK_ENTER_NOTIFY)
	gdk_window_set_cursor (widget->window, gdk_cursor_new (GDK_HAND1));
    else
	gdk_window_set_cursor (widget->window, gdk_cursor_new (GDK_LEFT_PTR));

}

GtkWidget *
e_icon_entry_create_button (const gchar *stock)
{
	GtkWidget *eventbox;
	GtkWidget *image;

	eventbox = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (eventbox), 2);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (eventbox), FALSE);

	image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (eventbox), image);

	g_signal_connect_after (eventbox, "enter-notify-event", (GCallback) set_cursor, NULL);
	g_signal_connect_after (eventbox, "leave-notify-event", (GCallback) set_cursor, NULL);

	return eventbox;
}

GtkWidget *
e_icon_entry_create_text (const gchar *text)
{
	GtkWidget *eventbox;
	GtkWidget *image;

	eventbox = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (eventbox), 2);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (eventbox), FALSE);

	image = gtk_label_new (text);
	gtk_container_add (GTK_CONTAINER (eventbox), image);
	g_object_set_data ((GObject *)eventbox, "lbl", image);
	g_signal_connect_after (eventbox, "enter-notify-event", (GCallback) set_cursor, NULL);
	g_signal_connect_after (eventbox, "leave-notify-event", (GCallback) set_cursor, NULL);

	return eventbox;
}

GtkWidget *
e_icon_entry_create_separator ()
{
	GtkWidget *eventbox;
	GtkWidget *image;

	eventbox = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (eventbox), 0);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (eventbox), FALSE);

	image = (GtkWidget *)gtk_separator_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (eventbox), image);

	return eventbox;
}

GtkWidget *
e_icon_entry_get_entry (EIconEntry *entry)
{
	g_return_val_if_fail (E_IS_ICON_ENTRY (entry), NULL);

	return entry->entry;
}
