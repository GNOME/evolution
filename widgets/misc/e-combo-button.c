/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-combo-button.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-combo-button.h"

#include <gtk/gtkvseparator.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>

#include <gal/util/e-util.h>


enum {
	ACTIVATE_DEFAULT,
	LAST_SIGNAL
};


struct _EComboButtonPrivate {
	GtkWidget *arrow_pixmap;
	GtkWidget *hbox;
	GtkWidget *separator;
	GtkWidget *label_hbox;

	GtkMenu *menu;

	gboolean menu_popped_up;
};


#define SPACING 2

static char *arrow_xpm[] = {
	"11 5  2 1",
	" 	c none",
	".	c #000000000000",
	" ......... ",
	"  .......  ",
	"   .....   ",
	"    ...    ",
	"     .     ",
};


#define PARENT_TYPE gtk_button_get_type ()
static GtkButtonClass *parent_class = NULL;
static guint combo_button_signals[LAST_SIGNAL] = { 0 };


/* Callbacks for the associated menu.  */

static void
menu_detacher (GtkWidget *widget,
	       GtkMenu *menu)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	gtk_signal_disconnect_by_data (GTK_OBJECT (menu), combo_button);
	priv->menu = NULL;
}

static void
menu_deactivate_callback (GtkMenuShell *menu_shell,
			  void *data)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (data);
	priv = combo_button->priv;

	priv->menu_popped_up = FALSE;

	GTK_BUTTON (combo_button)->button_down = FALSE;
	gtk_button_leave (GTK_BUTTON (combo_button));
	gtk_button_clicked (GTK_BUTTON (combo_button));
}

static void
menu_position_func (GtkMenu *menu,
		    gint *x_return,
		    gint *y_return,
		    void *data)
{
	EComboButton *combo_button;
	GtkAllocation *allocation;

	combo_button = E_COMBO_BUTTON (data);
	allocation = & (GTK_WIDGET (combo_button)->allocation);

	gdk_window_get_origin (GTK_WIDGET (combo_button)->window, x_return, y_return);

	*y_return += allocation->height;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (object);
	priv = combo_button->priv;

	if (priv->arrow_pixmap != NULL) {
		gtk_widget_destroy (priv->arrow_pixmap);
		priv->arrow_pixmap = NULL;
	}

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static void
impl_realize (GtkWidget *widget)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;
	GdkPixmap *arrow_gdk_pixmap;
	GdkBitmap *arrow_gdk_mask;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	g_assert (priv->arrow_pixmap == NULL);

	arrow_gdk_pixmap = gdk_pixmap_create_from_xpm_d (widget->window, &arrow_gdk_mask, NULL, arrow_xpm);
	priv->arrow_pixmap = gtk_pixmap_new (arrow_gdk_pixmap, arrow_gdk_mask);
	gtk_widget_show (priv->arrow_pixmap);

	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->arrow_pixmap, FALSE, TRUE, SPACING);

	gdk_pixmap_unref (arrow_gdk_pixmap);
	gdk_bitmap_unref (arrow_gdk_mask);
}

static void
impl_unrealize (GtkWidget *widget)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

	if (priv->arrow_pixmap != NULL) {
		gtk_widget_destroy (priv->arrow_pixmap);
		priv->arrow_pixmap = NULL;
	}
}

static int
impl_button_press_event (GtkWidget *widget,
			 GdkEventButton *event)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
		GTK_BUTTON (widget)->button_down = TRUE;

		if (event->x >= priv->separator->allocation.x) {
			/* User clicked on the right side: pop up the menu.  */
			gtk_button_pressed (GTK_BUTTON (widget));

			priv->menu_popped_up = TRUE;
			gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL,
					menu_position_func, combo_button,
					event->button, event->time);
		} else {
			/* User clicked on the left side: just behave like a
			   normal button (i.e. not a toggle).  */
			gtk_grab_add (widget);
			gtk_button_pressed (GTK_BUTTON (widget));
		}
	}

	return TRUE;
}

static int
impl_button_release_event (GtkWidget *widget,
			   GdkEventButton *event)
{
	if (event->button == 1) {
		gtk_grab_remove (widget);
		gtk_button_released (GTK_BUTTON (widget));
	}

	return TRUE;
}

static int
impl_leave_notify_event (GtkWidget *widget,
			 GdkEventCrossing *event)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	/* This is to override the standard behavior of deactivating the button
	   when the pointer gets out of the widget, in the case in which we
	   have just popped up the menu.  Otherwise, the button would look as
	   inactive when the menu is popped up.  */
	if (! priv->menu_popped_up)
		return (* GTK_WIDGET_CLASS (parent_class)->leave_notify_event) (widget, event);

	return FALSE;
}


static void
class_init (GtkObjectClass *object_class)
{
	GtkWidgetClass *widget_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = impl_destroy;

	widget_class = GTK_WIDGET_CLASS (object_class);
	widget_class->realize              = impl_realize;
	widget_class->unrealize            = impl_unrealize;
	widget_class->button_press_event   = impl_button_press_event;
	widget_class->button_release_event = impl_button_release_event;
	widget_class->leave_notify_event   = impl_leave_notify_event;

	combo_button_signals[ACTIVATE_DEFAULT] = 
		gtk_signal_new ("activate_default",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EComboButtonClass, activate_default),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, combo_button_signals, LAST_SIGNAL);
}

static void
init (EComboButton *combo_button)
{
	EComboButtonPrivate *priv;
	GtkWidget *label;

	priv = g_new (EComboButtonPrivate, 1);
	combo_button->priv = priv;

	priv->hbox = gtk_hbox_new (FALSE, SPACING);
	gtk_container_add (GTK_CONTAINER (combo_button), priv->hbox);
	gtk_widget_show (priv->hbox);

	priv->label_hbox = gtk_hbox_new (FALSE, SPACING);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->label_hbox, TRUE, TRUE, 0);
	gtk_widget_show (priv->label_hbox);

	priv->separator = gtk_vseparator_new ();
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->separator, FALSE, TRUE, SPACING);
	gtk_widget_show (priv->separator);

	label = gtk_label_new ("TEST!!!");
	gtk_container_add (GTK_CONTAINER (priv->label_hbox), label);
	gtk_widget_show (label);

	priv->arrow_pixmap   = NULL;
	priv->menu           = NULL;
	priv->menu_popped_up = FALSE;
}


void
e_combo_button_construct (EComboButton *combo_button,
			  GtkMenu *menu)
{
	EComboButtonPrivate *priv;

	g_return_if_fail (combo_button != NULL);
	g_return_if_fail (E_IS_COMBO_BUTTON (combo_button));
	g_return_if_fail (menu != NULL);
	g_return_if_fail (GTK_IS_MENU (menu));

	priv = combo_button->priv;
	g_return_if_fail (priv->menu == NULL);

	priv->menu = menu;
	gtk_menu_attach_to_widget (menu, GTK_WIDGET (combo_button), menu_detacher);

	gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate_callback),
			    combo_button);

	GTK_WIDGET_UNSET_FLAGS (combo_button, GTK_CAN_FOCUS);
}

GtkWidget *
e_combo_button_new (GtkMenu *menu)
{
	GtkWidget *new;

	new = gtk_type_new (e_combo_button_get_type ());

	e_combo_button_construct (E_COMBO_BUTTON (new), menu);

	return new;
}


E_MAKE_TYPE (e_combo_button, "EComboButton", EComboButton, class_init, init, PARENT_TYPE)
