/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-sidebar.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include "e-sidebar.h"

#include "e-shell-marshal.h"

#include <gal/util/e-util.h>

#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktogglebutton.h>


#define PARENT_TYPE gtk_container_get_type ()
static GtkContainerClass *parent_class = NULL;


typedef struct {
	GtkWidget *button_widget;
	GtkWidget *label_widget;
	GtkWidget *hbox;
	int id;
} Button;

struct _ESidebarPrivate {
	ESidebarMode mode;
	
	GtkWidget *selection_widget;
	GSList *buttons;

	gboolean in_toggle;
};


enum {
	BUTTON_SELECTED,
	NUM_SIGNALS
};

static unsigned int signals[NUM_SIGNALS] = { 0 };


#define H_PADDING 6
#define V_PADDING 6


/* Utility functions.  */

static Button *
button_new (GtkWidget *button_widget, GtkWidget *label_widget, GtkWidget *hbox, int id)
{
	Button *button = g_new (Button, 1);

	button->button_widget = button_widget;
	button->label_widget = label_widget;
	button->hbox = hbox;
	button->id = id;

	g_object_ref (button_widget);
	g_object_ref (label_widget);
	g_object_ref (hbox);

	return button;
}

static void
button_free (Button *button)
{
	g_object_unref (button->button_widget);
	g_object_unref (button->label_widget);
	g_object_unref (button->hbox);
	g_free (button);
}

static void
update_buttons (ESidebar *sidebar, int new_selected_id)
{
	GSList *p;

	sidebar->priv->in_toggle = TRUE;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (button->id == new_selected_id)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), TRUE);
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), FALSE);
	}

	sidebar->priv->in_toggle = FALSE;
}


/* Callbacks.  */

static void
button_toggled_callback (GtkToggleButton *toggle_button,
			 ESidebar *sidebar)
{
	int id = 0;
	GSList *p;

	if (sidebar->priv->in_toggle)
		return;

	sidebar->priv->in_toggle = TRUE;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (button->button_widget != GTK_WIDGET (toggle_button)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), FALSE);
		} else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), TRUE);
			id = button->id;
		}
	}

	sidebar->priv->in_toggle = FALSE;

	g_signal_emit (sidebar, signals[BUTTON_SELECTED], 0, id);
}


/* Layout.  */

static int
do_layout_text_buttons (ESidebar *sidebar)
{
	GtkAllocation *allocation = & GTK_WIDGET (sidebar)->allocation;
	GSList *rows [g_slist_length (sidebar->priv->buttons)];
	GSList *p;
	int row_number;
	int row_width;
	int max_btn_width = 0, max_btn_height = 0;
	int btns_required;
	int row_last;
	int x, y;
	int i;

	/* (Yes, this code calls gtk_widget_size_request() an ungodly number of times, but it's not
	   like we care about performance here, and this makes the code simpler.)  */


	/* 1. Figure out the max width and height */
	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;
		GtkRequisition requisition;

		gtk_widget_size_request (GTK_WIDGET (button->button_widget), &requisition);

		max_btn_height = MAX (max_btn_height, requisition.height);	
		max_btn_width = MAX (max_btn_width, requisition.width);	

	}

	/* 2. Split the buttons into rows, depending on the max_btn_width.  */

	row_number = 0;
	rows [0] = NULL;
	row_width = H_PADDING;
	btns_required = allocation->width / (max_btn_width + H_PADDING);
	for (; btns_required > 2; btns_required--) {
		if (g_slist_length (sidebar->priv->buttons) % btns_required == 0)
			break;

	}
	
	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (g_slist_length (rows[row_number]) % btns_required == 0
		    && rows [row_number] != NULL) {
			row_number ++;
			rows [row_number] = NULL;
			row_width = H_PADDING;
		}

		row_width += max_btn_width + H_PADDING;
		rows [row_number] = g_slist_append (rows [row_number], button->button_widget);
	}

	row_last = row_number;

	/* 3. Layout the buttons. */

	y = allocation->y + allocation->height - V_PADDING - 1;
	for (i = row_last; i >= 0; i --) {
		int len, extra_width;
		
		y -= max_btn_height;
		x = H_PADDING + allocation->x;
		len = g_slist_length (rows[i]);
		extra_width = (allocation->width - (len * max_btn_width ) - (len * H_PADDING)) / len;
		for (p = rows [i]; p != NULL; p = p->next) {
			GtkAllocation child_allocation;

			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = max_btn_width + extra_width;
			child_allocation.height = max_btn_height;

			gtk_widget_size_allocate (GTK_WIDGET (p->data), &child_allocation);

			x += child_allocation.width + H_PADDING;
		}

		y -= V_PADDING;
	}

	/* 4. Free stuff */

	for (i = 0; i <= row_last; i ++)
		g_slist_free (rows [i]);

	return y;
}

static int
do_layout_icon_buttons (ESidebar *sidebar)
{
	GtkAllocation *allocation = & GTK_WIDGET (sidebar)->allocation;
	GSList *rows [g_slist_length (sidebar->priv->buttons)];
	GSList *p;
	int row_number;
	int row_width;
	int row_last;
	int x, y;
	int i;

	/* (Yes, this code calls gtk_widget_size_request() an ungodly number of times, but it's not
	   like we care about performance here, and this makes the code simpler.)  */

	/* 1. Split the buttons into rows, depending on their width.  */

	row_number = 0;
	rows [0] = NULL;
	row_width = H_PADDING;
	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;
		GtkRequisition requisition;

		gtk_widget_size_request (GTK_WIDGET (button->button_widget), &requisition);

		if (row_width + requisition.width + H_PADDING >= allocation->width
		    && rows [row_number] != NULL) {
			row_number ++;
			rows [row_number] = NULL;
			row_width = H_PADDING;
		}

		row_width += requisition.width + H_PADDING;
		rows [row_number] = g_slist_append (rows [row_number], button->button_widget);
	}

	row_last = row_number;

	/* 2. Layout the buttons. */

	y = allocation->y + allocation->height - V_PADDING - 1;
	for (i = row_last; i >= 0; i --) {
		int row_height = 0;

		for (p = rows [i]; p != NULL; p = p->next) {
			GtkRequisition requisition;

			gtk_widget_size_request (GTK_WIDGET (p->data), &requisition);
			row_height = MAX (row_height, requisition.height);
		}

		y -= row_height;
		x = H_PADDING;
		for (p = rows [i]; p != NULL; p = p->next) {
			GtkRequisition requisition;
			GtkAllocation child_allocation;

			gtk_widget_size_request (GTK_WIDGET (p->data), &requisition);

			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = requisition.width;
			child_allocation.height = requisition.height;

			gtk_widget_size_allocate (GTK_WIDGET (p->data), &child_allocation);

			x += requisition.width + H_PADDING;
		}

		y -= V_PADDING;
	}

	/* 3. Free stuff */

	for (i = 0; i <= row_last; i ++)
		g_slist_free (rows [i]);

	return y;
}

static void
do_layout (ESidebar *sidebar)
{
	GtkAllocation *allocation = & GTK_WIDGET (sidebar)->allocation;
	GtkAllocation child_allocation;
	int y;

	switch (sidebar->priv->mode) {
	case E_SIDEBAR_MODE_TEXT:
		y = do_layout_text_buttons (sidebar);
		break;
	case E_SIDEBAR_MODE_ICON:
		y = do_layout_icon_buttons (sidebar);
		break;
	default:
		g_assert_not_reached ();
		return;
	}
	
	/* Place the selection widget.  */
	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;
	child_allocation.height = y - allocation->y;
	
	gtk_widget_size_allocate (sidebar->priv->selection_widget, & child_allocation);
}


/* GtkContainer methods.  */

static void
impl_forall (GtkContainer *container,
	     gboolean include_internals,
	     GtkCallback callback,
	     void *callback_data)
{
	ESidebar *sidebar = E_SIDEBAR (container);
	GSList *p;

	if (sidebar->priv->selection_widget != NULL)
		(* callback) (sidebar->priv->selection_widget, callback_data);

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		GtkWidget *widget = ((Button *) p->data)->button_widget;
		(* callback) (widget, callback_data);
	}
}

static void
impl_remove (GtkContainer *container,
	     GtkWidget *widget)
{
	ESidebar *sidebar = E_SIDEBAR (container);
	GSList *p;

	if (widget == sidebar->priv->selection_widget) {
		e_sidebar_set_selection_widget (sidebar, NULL);
		return;
	}

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		GtkWidget *button_widget = ((Button *) p->data)->button_widget;

		if (button_widget == widget) {
			gtk_widget_unparent (button_widget);
			sidebar->priv->buttons = g_slist_remove_link (sidebar->priv->buttons, p);
			gtk_widget_queue_resize (GTK_WIDGET (sidebar));
			break;
		}
	}
}


/* GtkWidget methods.  */

static void
impl_size_request (GtkWidget *widget,
		   GtkRequisition *requisition)
{
	ESidebar *sidebar = E_SIDEBAR (widget);
	GSList *p;

	if (sidebar->priv->selection_widget == NULL) {
		requisition->width = 2 * H_PADDING;
		requisition->height = 2 * V_PADDING;
	} else {
		gtk_widget_size_request (sidebar->priv->selection_widget, requisition);
	}

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;
		GtkRequisition button_requisition;

		gtk_widget_size_request (button->button_widget, &button_requisition);

		requisition->width = MAX (requisition->width, button_requisition.width + 2 * H_PADDING);
		requisition->height += button_requisition.height + V_PADDING;
	}
}

static void
impl_size_allocate (GtkWidget *widget,
		    GtkAllocation *allocation)
{
	widget->allocation = *allocation;

	do_layout (E_SIDEBAR (widget));
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ESidebarPrivate *priv = E_SIDEBAR (object)->priv;

	g_slist_foreach (priv->buttons, (GFunc) button_free, NULL);
	g_slist_free (priv->buttons);
	priv->buttons = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ESidebarPrivate *priv = E_SIDEBAR (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (ESidebarClass *class)
{
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	container_class->forall = impl_forall;
	container_class->remove = impl_remove;

	widget_class->size_request = impl_size_request;
	widget_class->size_allocate = impl_size_allocate;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);

	
	signals[BUTTON_SELECTED]
		= g_signal_new ("button_selected",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (ESidebarClass, button_selected),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
}

static void
init (ESidebar *sidebar)
{
	ESidebarPrivate *priv;

	GTK_WIDGET_SET_FLAGS (sidebar, GTK_NO_WINDOW);
  
	priv = g_new0 (ESidebarPrivate, 1);
	sidebar->priv = priv;

	priv->mode = E_SIDEBAR_MODE_TEXT;
}


GtkWidget *
e_sidebar_new (void)
{
	ESidebar *sidebar = g_object_new (e_sidebar_get_type (), NULL);

	return GTK_WIDGET (sidebar);
}


void
e_sidebar_set_selection_widget (ESidebar *sidebar, GtkWidget *widget)
{
	if (sidebar->priv->selection_widget != NULL)
		gtk_widget_unparent (sidebar->priv->selection_widget);

	sidebar->priv->selection_widget = widget;

	if (widget != NULL)
		gtk_widget_set_parent (widget, GTK_WIDGET (sidebar));

	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}


void
e_sidebar_add_button (ESidebar *sidebar,
		      const char *label,
		      GdkPixbuf *icon,
		      int id)
{
	GtkWidget *button_widget;
	GtkWidget *hbox;
	GtkWidget *icon_widget;
	GtkWidget *label_widget;

	button_widget = gtk_toggle_button_new ();
	g_signal_connect (button_widget, "toggled", G_CALLBACK (button_toggled_callback), sidebar);

	hbox = gtk_hbox_new (FALSE, 3);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
	icon_widget = gtk_image_new_from_pixbuf (icon);
	label_widget = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), icon_widget, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_widget, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (button_widget), hbox);

	sidebar->priv->buttons = g_slist_append (sidebar->priv->buttons, button_new (button_widget, label_widget, hbox, id));
	gtk_widget_set_parent (button_widget, GTK_WIDGET (sidebar));

	if (sidebar->priv->mode == E_SIDEBAR_MODE_ICON)
		gtk_container_remove (GTK_CONTAINER (hbox), label_widget);
	
	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}


void
e_sidebar_select_button (ESidebar *sidebar, int id)
{
	update_buttons (sidebar, id);

	g_signal_emit (sidebar, signals[BUTTON_SELECTED], 0, id);
}

ESidebarMode
e_sidebar_get_mode (ESidebar *sidebar)
{
	return sidebar->priv->mode;
}

void
e_sidebar_set_mode (ESidebar *sidebar, ESidebarMode mode)
{
	GSList *p;
	
	if (sidebar->priv->mode == mode)
		return;
	
	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		switch (mode) {
		case E_SIDEBAR_MODE_TEXT:
			gtk_box_pack_start (GTK_BOX (button->hbox), button->label_widget, TRUE, TRUE, 0);
			break;
		case E_SIDEBAR_MODE_ICON:
			gtk_container_remove (GTK_CONTAINER (button->hbox), button->label_widget);
			break;
		default:
			g_assert_not_reached ();
			return;
		}
	}

	sidebar->priv->mode = mode;

	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}


E_MAKE_TYPE (e_sidebar, "ESidebar", ESidebar, class_init, init, PARENT_TYPE)
