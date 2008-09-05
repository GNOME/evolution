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

#include "e-icon-entry.h"

#define E_ICON_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ICON_ENTRY, EIconEntryPrivate))

struct _EIconEntryPrivate {
	GtkWidget *entry;
	GtkWidget *hbox;
};

static gpointer parent_class;

static void
icon_entry_proxy_set_cursor (GtkWidget *widget,
                             GdkEventCrossing *event)
{
	if (event->type == GDK_ENTER_NOTIFY) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_HAND1);
		gdk_window_set_cursor (widget->window, cursor);
		gdk_cursor_unref (cursor);
	} else
		gdk_window_set_cursor (widget->window, NULL);
}

static GtkWidget *
icon_entry_create_proxy (GtkAction *action)
{
	GtkWidget *proxy;
	GtkWidget *widget;

	proxy = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (proxy), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (proxy), 2);
	gtk_action_connect_proxy (action, proxy);
	gtk_widget_show (widget);

	widget = gtk_action_create_icon (action, GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (proxy), widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		proxy, "button-press-event",
		G_CALLBACK (gtk_action_activate), action);
	g_signal_connect_after (
		proxy, "enter-notify-event",
		G_CALLBACK (icon_entry_proxy_set_cursor), NULL);
	g_signal_connect_after (
		proxy, "leave-notify-event",
		G_CALLBACK (icon_entry_proxy_set_cursor), NULL);

	return proxy;
}
static gboolean
icon_entry_focus_change_cb (GtkWidget *widget,
		            GdkEventFocus *event,
		            GtkWidget *entry)
{
	gtk_widget_queue_draw (entry);

	return FALSE;
}

static void
icon_entry_get_borders (GtkWidget *widget,
			GtkWidget *entry,
			gint *xborder,
			gint *yborder)
{
	gint focus_width;
	gboolean interior_focus;

	g_return_if_fail (entry->style != NULL);

	gtk_widget_style_get (
		entry, "focus-line-width", &focus_width,
		"interior-focus", &interior_focus, NULL);

	*xborder = entry->style->xthickness;
	*yborder = entry->style->ythickness;

	if (!interior_focus) {
		*xborder += focus_width;
		*yborder += focus_width;
	}
}

static void
icon_entry_paint (GtkWidget *widget,
		       GdkEventExpose *event)
{
	EIconEntry *entry = E_ICON_ENTRY (widget);
	GtkWidget *entry_widget = entry->priv->entry;
	int x = 0, y = 0, width, height, focus_width;
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

static void
icon_entry_dispose (GObject *object)
{
	EIconEntryPrivate *priv;

	priv = E_ICON_ENTRY_GET_PRIVATE (object);

	if (priv->entry != NULL) {
		g_object_unref (priv->entry);
		priv->entry = NULL;
	}

	if (priv->hbox != NULL) {
		g_object_unref (priv->hbox);
		priv->hbox = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
icon_entry_realize (GtkWidget *widget)
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

	widget->window = gdk_window_new (
		gtk_widget_get_parent_window (widget),
		&attributes, attributes_mask);

	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gtk_style_set_background (
		widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
icon_entry_size_request (GtkWidget *widget,
                         GtkRequisition *requisition)
{
	EIconEntryPrivate *priv;
	GtkContainer *container;
	GtkWidget *child;
	gint xborder, yborder;

	priv = E_ICON_ENTRY_GET_PRIVATE (widget);
	container = GTK_CONTAINER (widget);

	requisition->width = container->border_width * 2;
	requisition->height = container->border_width * 2;

	gtk_widget_ensure_style (priv->entry);
	icon_entry_get_borders (widget, priv->entry, &xborder, &yborder);

	child = GTK_BIN (widget)->child;
	if (GTK_WIDGET_VISIBLE (child)) {
		GtkRequisition child_requisition;

		gtk_widget_size_request (child, &child_requisition);
		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}

	requisition->width += 2 * xborder;
	requisition->height += 2 * yborder;
}

static void
icon_entry_size_allocate (GtkWidget *widget,
                          GtkAllocation *allocation)
{
	EIconEntryPrivate *priv;
	GtkContainer *container;
	GtkAllocation child_allocation;
	gint xborder, yborder;
	gint width, height;

	priv = E_ICON_ENTRY_GET_PRIVATE (widget);
	container = GTK_CONTAINER (widget);

	widget->allocation = *allocation;

	icon_entry_get_borders (widget, priv->entry, &xborder, &yborder);

	if (GTK_WIDGET_REALIZED (widget)) {
		width = allocation->width - container->border_width * 2;
		height = allocation->height - container->border_width * 2;

		child_allocation.x = container->border_width;
		child_allocation.y = container->border_width;
		child_allocation.width = MAX (width, 0);
		child_allocation.height = MAX (height, 0);

		gdk_window_move_resize (
			widget->window,
			allocation->x + child_allocation.x,
			allocation->y + child_allocation.y,
			child_allocation.width,
			child_allocation.height);
	}

	width = allocation->width - (container->border_width + xborder) * 2;
	height = allocation->height - (container->border_width + yborder) * 2;

	child_allocation.x = container->border_width + xborder;
	child_allocation.y = container->border_width + yborder;
	child_allocation.width = MAX (width, 0);
	child_allocation.height = MAX (height, 0);

	gtk_widget_size_allocate (GTK_BIN (widget)->child, &child_allocation);
}

static gboolean
icon_entry_expose (GtkWidget *widget,
                   GdkEventExpose *event)
{
	if (GTK_WIDGET_DRAWABLE (widget) && event->window == widget->window)
		icon_entry_paint (widget, event);

	/* Chain up to parent's expose() method. */
	return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}

static void
icon_entry_class_init (EIconEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EIconEntryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = icon_entry_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = icon_entry_realize;
	widget_class->size_request = icon_entry_size_request;
	widget_class->size_allocate = icon_entry_size_allocate;
	widget_class->expose_event = icon_entry_expose;
}

static void
icon_entry_init (EIconEntry *icon_entry)
{
	GtkWidget *widget;
	GtkWidget *container;

	icon_entry->priv = E_ICON_ENTRY_GET_PRIVATE (icon_entry);

	GTK_WIDGET_UNSET_FLAGS (icon_entry, GTK_NO_WINDOW);

	widget = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (icon_entry), widget);
	icon_entry->priv->hbox = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_has_frame (GTK_ENTRY (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	icon_entry->priv->entry = g_object_ref (widget);
	gtk_widget_show (widget);

	/* We need to queue a redraw when focus changes, to comply with
	 * themes (like Clearlooks) which draw focused and unfocused
	 * entries differently. */
	g_signal_connect_after (
		widget, "focus-in-event",
		G_CALLBACK (icon_entry_focus_change_cb), icon_entry);
	g_signal_connect_after (
		widget, "focus-out-event",
		G_CALLBACK (icon_entry_focus_change_cb), icon_entry);
}

GType
e_icon_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo type_info = {
			sizeof (EIconEntryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) icon_entry_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EIconEntry),
			0,     /* n_preallocs */
			(GInstanceInitFunc) icon_entry_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BIN, "EIconEntry", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_icon_entry_new (void)
{
	return GTK_WIDGET (g_object_new (E_TYPE_ICON_ENTRY, NULL));
}

GtkWidget *
e_icon_entry_get_entry (EIconEntry *icon_entry)
{
	g_return_val_if_fail (E_IS_ICON_ENTRY (icon_entry), NULL);

	return icon_entry->priv->entry;
}

void
e_icon_entry_add_action_start (EIconEntry *icon_entry,
                               GtkAction *action)
{
	GtkWidget *proxy;
	GtkBox *box;

	g_return_if_fail (E_IS_ICON_ENTRY (icon_entry));
	g_return_if_fail (GTK_IS_ACTION (action));

	box = GTK_BOX (icon_entry->priv->hbox);
	proxy = icon_entry_create_proxy (action);
	gtk_box_pack_start (box, proxy, FALSE, FALSE, 2);
	gtk_box_reorder_child (box, proxy, 0);
}

void
e_icon_entry_add_action_end (EIconEntry *icon_entry,
                             GtkAction *action)
{
	GtkWidget *proxy;
	GtkBox *box;

	g_return_if_fail (E_IS_ICON_ENTRY (icon_entry));
	g_return_if_fail (GTK_IS_ACTION (action));

	box = GTK_BOX (icon_entry->priv->hbox);
	proxy = icon_entry_create_proxy (action);
	gtk_box_pack_end (box, proxy, FALSE, FALSE, 2);
}
