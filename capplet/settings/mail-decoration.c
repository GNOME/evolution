/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Novell, Inc. (www.novell.com)
 *
 */

/* Many thanks to Aaron Bockover & Cubano. This is just a C version Cubano's self decoration */

#include "mail-decoration.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>

#define MAIL_DECORATION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MAIL_DECORATION_TYPE, MailDecorationPrivate))

struct _MailDecorationPrivate
{
	GdkCursor *cursors[8];
	gboolean default_cursor;
	gboolean resizing;
	GdkWindowEdge last_edge;
	gint resize_width;
	gint top_height;
	gboolean check_window;
	gboolean can_resize;
	gboolean full_screen;

	gint window_width;
	gint window_height;
};

static GObjectClass *parent_class = NULL;

static void mail_decoration_class_init(MailDecorationClass *klass);
static void mail_decoration_init(MailDecoration *facet);

GType
mail_decoration_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo mail_decoration_info =
		{
			sizeof (MailDecorationClass),
			NULL,
			NULL,
			(GClassInitFunc) mail_decoration_class_init,
			NULL,
			NULL,
			sizeof (MailDecoration),
			0,
			(GInstanceInitFunc) mail_decoration_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "MailDecoration",
					       &mail_decoration_info, 0);
	}

	return type;
}

static void
md_translate_position (GdkWindow *w, double ex, double ey, gint *x, gint *y, GtkWidget *window)
{
	*x = (gint)ex;
	*y = (gint)ey;

	while (w && w != gtk_widget_get_window (window)) {
		gint cx, cy, cw, ch, cd;
		gdk_window_get_geometry (w, &cx, &cy, &cw, &ch, &cd);
                *x += cx;
                *y += cy;
                w = gdk_window_get_parent (w);
	}
}

static gboolean
in_top (MailDecoration *md, double y)
{
	return y <= md->priv->resize_width;
}

static gboolean
in_left (MailDecoration *md, double x)
{
	return x <= md->priv->resize_width;
}

static gboolean
in_bottom (MailDecoration *md, double y)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (md->window), &allocation);

	return y >= allocation.height - md->priv->resize_width;
}

static gboolean
in_right (MailDecoration *md, double x)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (md->window), &allocation);

	return x >= allocation.width - md->priv->resize_width;
}

static void
set_cursor (MailDecoration *md, GdkWindowEdge edge)
{
	gdk_window_set_cursor (
		gtk_widget_get_window (GTK_WIDGET (md->window)),
		md->priv->cursors[edge]);
	md->priv->default_cursor = FALSE;
}

static void
reset_cursor (MailDecoration *md)
{
	if (!md->priv->default_cursor) {
		md->priv->default_cursor = TRUE;
		gdk_window_set_cursor (
			gtk_widget_get_window (GTK_WIDGET (md->window)),
			NULL);
	}

}

static void
update_cursor (MailDecoration *md, double x, double y, gboolean update)
{
	if (update)
		md->priv->resizing = TRUE;

	if (in_top(md, y) && in_left (md, x)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_NORTH_WEST;
		set_cursor (md, GDK_WINDOW_EDGE_NORTH_WEST);
	} else if (in_top (md, y) && in_right (md, x)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_NORTH_EAST;
		set_cursor (md, GDK_WINDOW_EDGE_NORTH_EAST);
	} else if (in_bottom (md, y) && in_left (md, x)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_SOUTH_WEST;
		set_cursor (md, GDK_WINDOW_EDGE_SOUTH_WEST);
	} else if (in_bottom (md, y) && in_right (md, x)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_SOUTH_EAST;
		set_cursor (md, GDK_WINDOW_EDGE_SOUTH_EAST);
	} else if (in_top (md, y)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_NORTH;
		set_cursor (md, GDK_WINDOW_EDGE_NORTH);
	} else if (in_bottom (md, y)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_SOUTH;
		set_cursor (md, GDK_WINDOW_EDGE_SOUTH);
	} else if (in_left (md, x)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_WEST;
		set_cursor (md, GDK_WINDOW_EDGE_WEST);
	} else if (in_right (md, x)) {
		md->priv->last_edge = GDK_WINDOW_EDGE_EAST;
		set_cursor (md, GDK_WINDOW_EDGE_EAST);
	} else {
		if (update)
			md->priv->resizing = FALSE;
		reset_cursor (md);
	}
}

static gboolean
md_motion_event (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	gint x, y;
	MailDecoration *md = (MailDecoration *)user_data;

	md_translate_position (event->window, event->x, event->y, &x, &y, (GtkWidget *)md->window);

	if (md->priv->can_resize) {
		update_cursor  (md, x, y, FALSE);
	}

	return FALSE;
}

static gboolean
md_enter_event (GtkWidget *widget , GdkEventCrossing *event, gpointer user_data)
{
	MailDecoration *md = (MailDecoration *)user_data;
	gint x, y;

	md_translate_position (event->window, event->x, event->y, &x, &y, (GtkWidget *)md->window);

	if (md->priv->can_resize) {
		update_cursor (md, x, y, FALSE);
	}

	return FALSE;
}

static gboolean
md_leave_event (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
	MailDecoration *md = (MailDecoration *)user_data;

	if (md->priv->can_resize)
		reset_cursor (md);

	return FALSE;
}

static void
md_size_allocate_event (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
	gint width=1024, height=500;
	MailDecoration *md = (MailDecoration *)user_data;

	gtk_widget_queue_draw (widget);
	gtk_window_get_size ((GtkWindow *)widget, &width, &height);
	if (width != md->priv->window_width || height != md->priv->window_height) {
		GConfClient *client = gconf_client_get_default ();

		md->priv->window_height = height;
		md->priv->window_width = width;
		gconf_client_set_int (client, "/apps/anjal/window_width", width, NULL);
		gconf_client_set_int (client, "/apps/anjal/window_height", height, NULL);
		g_object_unref(client);
	}

}

static gboolean
md_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	MailDecoration *md = (MailDecoration *)user_data;
	gint x_root = (gint) event->x_root;
	gint y_root = (gint) event->y_root;
	gint x, y;

        if (!md->priv->can_resize) {
                return FALSE;
        }

	md_translate_position (event->window, event->x, event->y, &x, &y, (GtkWidget *)md->window);
        update_cursor (md, x, y, TRUE);
        if (md->priv->resizing && event->button == 1 && event->type != GDK_2BUTTON_PRESS) {
		gtk_window_begin_resize_drag ((GtkWindow *)widget, md->priv->last_edge, 1, x_root, y_root, event->time);

        } else if ((md->priv->resizing && event->button == 2 && event->type != GDK_2BUTTON_PRESS) ||
                (event->button == 1 && y <= md->priv->top_height && event->type != GDK_2BUTTON_PRESS)) {
                gtk_window_begin_move_drag ((GtkWindow *)widget, event->button, x_root, y_root, event->time);
	} else if (y <= md->priv->top_height && event->type == GDK_2BUTTON_PRESS) {
		if (md->priv->full_screen)
			gtk_window_unfullscreen (md->window);
		else
			gtk_window_fullscreen (md->window);

		md->priv->full_screen = md->priv->full_screen != TRUE;
	} else
		return FALSE;

	return TRUE;
}

static gboolean
md_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	gint x, y;
	MailDecoration *md = (MailDecoration *)user_data;

	md_translate_position (event->window, event->x, event->y, &x, &y, (GtkWidget *)md->window);
	if (md->priv->resizing) {
		update_cursor (md, x, y, TRUE);
	}

	return FALSE;
}

MailDecoration* mail_decoration_new(GtkWindow *window)
{
	MailDecoration *md = g_object_new(mail_decoration_get_type(), NULL);
	GConfClient *client = gconf_client_get_default ();
	gint width, height;

	md->priv->window_width = width = gconf_client_get_int (client, "/apps/anjal/window_width", NULL);
	if (!width)
		md->priv->window_width = width = 1024;
	md->priv->window_height = height = gconf_client_get_int (client, "/apps/anjal/window_height", NULL);
	if (!height)
		md->priv->window_height = height = 500;
	g_object_unref (client);

	md->window = window;
	gtk_window_set_decorated (window, FALSE);
	gtk_widget_add_events ((GtkWidget *)window, GDK_BUTTON_PRESS_MASK |
					GDK_POINTER_MOTION_MASK |
					GDK_ENTER_NOTIFY_MASK |
					GDK_LEAVE_NOTIFY_MASK |
					GDK_BUTTON_RELEASE_MASK);

	g_signal_connect (window, "motion-notify-event", G_CALLBACK(md_motion_event), md);
	g_signal_connect (window, "enter-notify-event", G_CALLBACK(md_enter_event), md);
	g_signal_connect (window, "leave-notify-event", G_CALLBACK(md_leave_event), md);
	g_signal_connect (window, "button-press-event", G_CALLBACK(md_button_press_event), md);
	g_signal_connect (window, "button-release-event", G_CALLBACK(md_button_release_event), md);
	g_signal_connect (window, "size-allocate", G_CALLBACK(md_size_allocate_event), md);
	gtk_window_set_default_size ((GtkWindow *)window , width, height);/* We officiall should support 800x600 */

	return md;
}

static void
mail_decoration_class_init(MailDecorationClass *klass)
{

	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof(MailDecorationPrivate));
}

static void
mail_decoration_init(MailDecoration *md)
{
	MailDecorationPrivate *priv;

	priv = MAIL_DECORATION_GET_PRIVATE (md);
	md->priv = priv;

	priv->cursors[0]= gdk_cursor_new (GDK_TOP_LEFT_CORNER);
	priv->cursors[1]= gdk_cursor_new (GDK_TOP_SIDE);
	priv->cursors[2]= gdk_cursor_new (GDK_TOP_RIGHT_CORNER);
	priv->cursors[3]= gdk_cursor_new (GDK_LEFT_SIDE);
	priv->cursors[4]= gdk_cursor_new (GDK_RIGHT_SIDE);
	priv->cursors[5]= gdk_cursor_new (GDK_BOTTOM_LEFT_CORNER);
	priv->cursors[6]= gdk_cursor_new (GDK_BOTTOM_SIDE);
	priv->cursors[7]= gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);

	priv->default_cursor = TRUE;
	priv->resizing = FALSE;
	priv->resize_width = 4;
	priv->top_height = 54;
	priv->check_window = TRUE;
	priv->can_resize = TRUE;
	priv->full_screen = TRUE;
}
