/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-title-bar.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkrc.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <libgnome/gnome-i18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "widgets/misc/e-clipped-label.h"
#include "e-shell-constants.h"
#include "e-shell-marshal.h"
#include "e-shell-folder-title-bar.h"

struct _EShellFolderTitleBarPrivate {
	GdkPixbuf *icon;

	/* We have an icon, a label and a button that contains an icon and a
           label.  When the button is enabled, the stand-alone label icon get
           hidden; when the button is disabled, the button gets hidden and the
           label and the icon get shown.  This is pretty ugly but it easier to
           manage the GTK layout this way.  */

	/* The stand-alone icon/label combo.  */
	GtkWidget *title_icon;
	GtkWidget *title_label;

	/* The button.  */
	GtkWidget *title_button;
	GtkWidget *title_button_icon;
	GtkWidget *title_button_label;
	GtkWidget *title_button_arrow;

	/* Holds extra information that is to be shown on the bar.  */
	GtkWidget *folder_bar_label;

	/* Navigation buttons.  */
	GtkWidget *back_button;
	GtkWidget *forward_button;

	gboolean title_clickable;
};

enum {
	TITLE_TOGGLED,
	BACK_CLICKED,
	FORWARD_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EShellFolderTitleBar, e_shell_folder_title_bar, GTK_TYPE_HBOX)


/* Utility functions for managing icons and icon widgets.  */

static GdkPixbuf *
new_empty_pixbuf (void)
{
	GdkPixbuf *empty_pixbuf;
	unsigned char *pixels;

	empty_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
	pixels = gdk_pixbuf_get_pixels (empty_pixbuf);

	memset (pixels, 0, 4);

	return empty_pixbuf;
}

static GtkWidget *
new_empty_image_widget (void)
{
	GtkWidget *image_widget;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *empty_pixbuf;

	empty_pixbuf = new_empty_pixbuf ();

	gdk_pixbuf_render_pixmap_and_mask (empty_pixbuf, &pixmap, &mask, 127);
	image_widget = gtk_image_new_from_pixmap (pixmap, mask);

	g_object_unref (empty_pixbuf);

	return image_widget;
}


/* Utility functions.  */

static int
get_max_clipped_label_width (EClippedLabel *clipped_label)
{
	int width;

	pango_layout_get_pixel_size (clipped_label->layout, &width, NULL);
	width += 2 * GTK_MISC (clipped_label)->xpad;

	return width;
}

static void
size_allocate_title_button (EShellFolderTitleBar *title_bar,
			    GtkAllocation *allocation,
			    int offset,
			    int *available_width_inout)
{
	EShellFolderTitleBarPrivate *priv;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	int border_width;

	priv = title_bar->priv;

	/* Keep a little distance from the navigation arrows.  */
	allocation->x += 2;

	border_width = GTK_CONTAINER (title_bar)->border_width;

	gtk_widget_get_child_requisition (priv->title_button, &child_requisition);
	child_allocation.x = allocation->x + border_width + offset;
	child_allocation.y = allocation->y + border_width;
	child_allocation.height = allocation->height - 2 * border_width;

	child_allocation.width = child_requisition.width;
	child_allocation.width += get_max_clipped_label_width (E_CLIPPED_LABEL (priv->title_button_label));

	child_allocation.width = MIN (child_allocation.width, *available_width_inout);

	gtk_widget_size_allocate (priv->title_button, & child_allocation);

	*available_width_inout -= child_allocation.width;
}

static int
size_allocate_navigation_buttons_and_title_icon (EShellFolderTitleBar *title_bar,
						 GtkAllocation *allocation)
{
	EShellFolderTitleBarPrivate *priv;
	GtkRequisition child_requisition;
	GtkAllocation child_allocation;
	int border_width;

	priv = title_bar->priv;

	border_width = GTK_CONTAINER (title_bar)->border_width;

	child_allocation.x = allocation->x + border_width;
	child_allocation.y = allocation->y + border_width;
	child_allocation.height = allocation->height - 2 * border_width;

	gtk_widget_size_request (priv->back_button, &child_requisition);
	child_allocation.width = child_requisition.width;
	gtk_widget_size_allocate (priv->back_button, &child_allocation);

	child_allocation.x += child_allocation.width;

	gtk_widget_size_request (priv->forward_button, &child_requisition);
	child_allocation.width = child_requisition.width;
	gtk_widget_size_allocate (priv->forward_button, &child_allocation);

	if (! priv->title_clickable) {
		/* Keep a little distance from the navigation arrows.  */
		child_allocation.x += child_allocation.width + 5;

		gtk_widget_size_request (priv->title_icon, &child_requisition);
		child_allocation.width = child_requisition.width;
		gtk_widget_size_allocate (priv->title_icon, &child_allocation);
	}

	return child_allocation.x + child_allocation.width;
}

static void
size_allocate_label (EShellFolderTitleBar *title_bar,
		     GtkAllocation *allocation,
		     int offset,
		     int *available_width_inout)
{
	EShellFolderTitleBarPrivate *priv;
	GtkAllocation child_allocation;
	int border_width;

	priv = title_bar->priv;

	border_width = GTK_CONTAINER (title_bar)->border_width;

	child_allocation.x = allocation->x + border_width + offset;
	child_allocation.y = allocation->y + border_width;
	child_allocation.height = allocation->height - 2 * border_width;

	child_allocation.width = MIN (get_max_clipped_label_width (E_CLIPPED_LABEL (priv->title_label)),
				      *available_width_inout);

	gtk_widget_size_allocate (priv->title_label, & child_allocation);

	*available_width_inout -= child_allocation.width;
}


/* The back/forward navigation buttons.  */

static void
back_button_clicked_callback (GtkButton *button,
			      void *data)
{
	EShellFolderTitleBar *folder_title_bar;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (data);

	g_signal_emit (folder_title_bar, signals[BACK_CLICKED], 0);
}

static void
forward_button_clicked_callback (GtkButton *button,
				 void *data)
{
	EShellFolderTitleBar *folder_title_bar;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (data);

	g_signal_emit (folder_title_bar, signals[FORWARD_CLICKED], 0);
}

static void
add_navigation_buttons (EShellFolderTitleBar *folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;
	GtkWidget *back_image;
	GtkWidget *forward_image;

	priv = folder_title_bar->priv;

	priv->back_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->back_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS (priv->back_button, GTK_CAN_FOCUS);

	back_image = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (priv->back_button), back_image);

	g_signal_connect (priv->back_button, "clicked",
			  G_CALLBACK (back_button_clicked_callback), folder_title_bar);

	priv->forward_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->forward_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS (priv->forward_button, GTK_CAN_FOCUS);

	forward_image = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (priv->forward_button), forward_image);

	g_signal_connect (priv->forward_button, "clicked",
			  G_CALLBACK (forward_button_clicked_callback), folder_title_bar);

	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->back_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->forward_button, FALSE, FALSE, 0);

	gtk_widget_show_all (priv->back_button);
	gtk_widget_show_all (priv->forward_button);
}


/* Popup button callback.  */

static void
title_button_toggled_cb (GtkToggleButton *title_button,
			 void *data)
{
	EShellFolderTitleBar *folder_title_bar;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (data);

	g_signal_emit (folder_title_bar, signals[TITLE_TOGGLED], 0,
		       gtk_toggle_button_get_active (title_button));
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (object);
	priv = folder_title_bar->priv;

	if (priv->icon != NULL) {
		g_object_unref (priv->icon);
		priv->icon = NULL;
	}

	(* G_OBJECT_CLASS (e_shell_folder_title_bar_parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (object);
	priv = folder_title_bar->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (e_shell_folder_title_bar_parent_class)->finalize) (object);
}


/* GTkWidget methods. */

static void
impl_size_allocate (GtkWidget *widget,
		    GtkAllocation *allocation)
{
	EShellFolderTitleBar *title_bar;
	EShellFolderTitleBarPrivate *priv;
	GtkAllocation label_allocation;
	int border_width;
	int available_width;
	int width_before_icon;
	int offset;

	title_bar = E_SHELL_FOLDER_TITLE_BAR (widget);
	priv = title_bar->priv;

	border_width = GTK_CONTAINER (widget)->border_width;
	available_width = allocation->width - 2 * border_width;

	offset = size_allocate_navigation_buttons_and_title_icon (title_bar, allocation);
	available_width -= offset;

	width_before_icon = available_width;

	if (priv->title_clickable)
		size_allocate_title_button (title_bar, allocation, offset, & available_width);
	else
		size_allocate_label (title_bar, allocation, offset, & available_width);

	label_allocation.x      = allocation->x + width_before_icon - available_width - border_width + offset;
	label_allocation.y      = allocation->y + border_width;
	label_allocation.width  = available_width - 2 * border_width;
	label_allocation.height = allocation->height - 2 * border_width;

	gtk_widget_size_allocate (priv->folder_bar_label, & label_allocation);

	widget->allocation = *allocation;
}


static int
impl_expose_event (GtkWidget *widget,
		   GdkEventExpose *event)
{
	gtk_paint_flat_box (widget->style, widget->window,
			    GTK_STATE_ACTIVE, GTK_SHADOW_OUT,
			    &event->area, widget, "EShellFolderTitleBar",
			    widget->allocation.x,
			    widget->allocation.y,
			    widget->allocation.width,
			    widget->allocation.height);

	(* GTK_WIDGET_CLASS (e_shell_folder_title_bar_parent_class)->expose_event) (widget, event);

	return FALSE;
}


static void
e_shell_folder_title_bar_class_init (EShellFolderTitleBarClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->size_allocate = impl_size_allocate;
	widget_class->expose_event = impl_expose_event;

	signals[TITLE_TOGGLED]
		= g_signal_new ("title_toggled",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EShellFolderTitleBarClass, title_toggled),
				NULL, NULL,
				e_shell_marshal_NONE__BOOL,
				G_TYPE_NONE, 1,
				G_TYPE_BOOLEAN);

	signals[BACK_CLICKED]
		= g_signal_new ("back_clicked",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EShellFolderTitleBarClass, back_clicked),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[FORWARD_CLICKED]
		= g_signal_new ("forward_clicked",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EShellFolderTitleBarClass, forward_clicked),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);
}

static void
e_shell_folder_title_bar_init (EShellFolderTitleBar *shell_folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;

	priv = g_new (EShellFolderTitleBarPrivate, 1);

	priv->icon               = NULL;

	priv->title_icon         = NULL;
	priv->title_label        = NULL;

	priv->title_button       = NULL;
	priv->title_button_icon  = NULL;
	priv->title_button_label = NULL;
	priv->title_button_arrow = NULL;

	priv->folder_bar_label   = NULL;

	priv->back_button        = NULL;
	priv->forward_button     = NULL;

	priv->title_clickable    = TRUE;

	shell_folder_title_bar->priv = priv;
}


/**
 * e_shell_folder_title_bar_construct:
 * @folder_title_bar: 
 * 
 * Construct the folder title bar widget.
 **/
void
e_shell_folder_title_bar_construct (EShellFolderTitleBar *folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;
	GtkWidget *title_button_hbox;
	GtkWidget *widget;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;
	widget = GTK_WIDGET (folder_title_bar);

	priv->title_icon = new_empty_image_widget ();
	gtk_misc_set_alignment (GTK_MISC (priv->title_icon), 1.0, .5);
	gtk_misc_set_padding (GTK_MISC (priv->title_icon), 2, 0);
	gtk_widget_show (priv->title_icon);

	priv->title_label = e_clipped_label_new ("", PANGO_WEIGHT_BOLD, 1.2);
	gtk_misc_set_padding (GTK_MISC (priv->title_label), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (priv->title_label), 0.0, 0.5);

	priv->title_button_label = e_clipped_label_new ("", PANGO_WEIGHT_BOLD, 1.2);
	gtk_misc_set_padding (GTK_MISC (priv->title_button_label), 2, 0);
	gtk_misc_set_alignment (GTK_MISC (priv->title_button_label), 0.0, 0.5);
	gtk_widget_show (priv->title_button_label);

	priv->folder_bar_label = e_clipped_label_new ("", PANGO_WEIGHT_NORMAL, 1.0);
	gtk_misc_set_alignment (GTK_MISC (priv->folder_bar_label), 1.0, 0.5);
	gtk_widget_show (priv->folder_bar_label);

	priv->title_button_icon = new_empty_image_widget ();
	gtk_widget_show (priv->title_button_icon);

	title_button_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_button_hbox), priv->title_button_icon,
			    FALSE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (title_button_hbox), priv->title_button_label,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (title_button_hbox), gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE),
			    FALSE, TRUE, 2);
	gtk_widget_show (title_button_hbox);

	priv->title_button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->title_button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (priv->title_button), title_button_hbox);
	GTK_WIDGET_UNSET_FLAGS (priv->title_button, GTK_CAN_FOCUS);
	gtk_widget_show (priv->title_button);

	gtk_container_set_border_width (GTK_CONTAINER (folder_title_bar), 2);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->title_icon, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->title_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->title_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->folder_bar_label, TRUE, TRUE, 0);

	/* Make the label have a border as large as the button's.
	   FIXME: This is really hackish.  The hardcoded numbers should be OK
	   as the padding is hardcoded in GtkButton too (see CHILD_SPACING in
	   gtkbutton.c).  */
	gtk_misc_set_padding (GTK_MISC (priv->title_label),
			      GTK_WIDGET (priv->title_button)->style->xthickness,
			      GTK_WIDGET (priv->title_button)->style->ythickness + 2);

	g_signal_connect (priv->title_button, "toggled",
			  G_CALLBACK (title_button_toggled_cb), folder_title_bar);

	add_navigation_buttons (folder_title_bar);

	e_shell_folder_title_bar_set_title (folder_title_bar, NULL);
}

/**
 * e_shell_folder_title_bar_new:
 * @void: 
 * 
 * Create a new title bar widget.
 * 
 * Return value: 
 **/
GtkWidget *
e_shell_folder_title_bar_new (void)
{
	EShellFolderTitleBar *new;

	new = g_object_new (e_shell_folder_title_bar_get_type (), NULL);

	e_shell_folder_title_bar_construct (new);

	return GTK_WIDGET (new);
}

/**
 * e_shell_folder_title_bar_set_title:
 * @folder_title_bar: 
 * @title: 
 * 
 * Set the title for the title bar.
 **/
void
e_shell_folder_title_bar_set_title (EShellFolderTitleBar *folder_title_bar,
				    const char *title)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	if (title == NULL) {
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->title_button_label), _("(Untitled)"));
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->title_label), _("(Untitled)"));
	} else {
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->title_button_label), title);
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->title_label), title);
	}

	/* FIXME: There seems to be a bug in EClippedLabel, this is just a workaround.  */
	gtk_widget_queue_resize (GTK_WIDGET (folder_title_bar));
}

/**
 * e_shell_folder_title_bar_set_folder_bar_label:
 * @folder_title_bar:
 * @text: Some text to show in the label.
 *
 * Sets the right-justified text label (to the left of the icon) for
 * the title bar.
 **/
void
e_shell_folder_title_bar_set_folder_bar_label (EShellFolderTitleBar *folder_title_bar,
					       const char *text)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	if (text == NULL)
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->folder_bar_label), "");
	else
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->folder_bar_label), text);

	/* FIXME: Might want to set the styles somewhere in here too,
           black text on grey background isn't the best combination */

	gtk_widget_queue_resize (GTK_WIDGET (folder_title_bar));
}

/**
 * e_shell_folder_title_bar_set_icon:
 * @folder_title_bar: 
 * @icon: 
 * 
 * Set the name of the icon for the title bar.
 **/
void
e_shell_folder_title_bar_set_icon (EShellFolderTitleBar *folder_title_bar,
				   GdkPixbuf *icon)
{
	EShellFolderTitleBarPrivate *priv;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	if (icon == NULL) {
		if (priv->icon != NULL)
			g_object_unref (priv->icon);
		priv->icon = new_empty_pixbuf ();
	} else {
		g_object_ref (icon);
		if (priv->icon != NULL)
			g_object_unref (priv->icon);
		priv->icon = icon;
	}

	gdk_pixbuf_render_pixmap_and_mask (priv->icon, &pixmap, &mask, 127);
	gtk_image_set_from_pixmap (GTK_IMAGE (priv->title_button_icon), pixmap, mask);

	gdk_pixbuf_render_pixmap_and_mask (priv->icon, &pixmap, &mask, 127);
	gtk_image_set_from_pixmap (GTK_IMAGE (priv->title_icon), pixmap, mask);
}


/**
 * e_shell_folder_title_bar_set_toggle_state:
 * @folder_title_bar: 
 * @state: 
 * 
 * Set whether the title bar's button is in pressed state (TRUE) or not (FALSE).
 **/
void
e_shell_folder_title_bar_set_toggle_state (EShellFolderTitleBar *folder_title_bar,
					   gboolean state)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->title_button), state);
}

/**
 * e_shell_folder_title_bar_set_clickable:
 * @folder_title_bar: 
 * @clickable: 
 * 
 * Specify whether the title in the @folder_title_bar is clickable.  If not,
 * the arrow pixmap is not shown.
 **/
void
e_shell_folder_title_bar_set_title_clickable (EShellFolderTitleBar *folder_title_bar,
					      gboolean title_clickable)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	if ((priv->title_clickable && title_clickable)
	    || (! priv->title_clickable && ! title_clickable))
		return;

	if (title_clickable) {
		gtk_widget_hide (priv->title_label);
		gtk_widget_hide (priv->title_icon);

		gtk_widget_show_all (priv->title_button);
	} else {
		gtk_widget_hide (priv->title_button);

		gtk_widget_show (priv->title_icon);
		gtk_widget_show (priv->title_label);
	}

	priv->title_clickable = !! title_clickable;
}


void
e_shell_folder_title_bar_update_navigation_buttons  (EShellFolderTitleBar *folder_title_bar,
						     gboolean can_go_back,
						     gboolean can_go_forward)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	gtk_widget_set_sensitive (priv->back_button, can_go_back);
	gtk_widget_set_sensitive (priv->forward_button, can_go_forward);
}

