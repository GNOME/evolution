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

#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkrc.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-font.h>

#include "widgets/misc/e-clipped-label.h"
#include "e-shell-constants.h"
#include "e-shell-folder-title-bar.h"


#define PARENT_TYPE GTK_TYPE_HBOX
static GtkHBox *parent_class = NULL;

struct _EShellFolderTitleBarPrivate {
	GdkPixbuf *icon;
	GtkWidget *icon_widget;

	/* We have a label and a button.  When the button is enabled,
           the label is hidden; when the button is disable, only the
           label is visible.  */

	/* The label.  */
	GtkWidget *title_label;

	/* Holds extra information that is to be shown to the left of the icon */
	GtkWidget *folder_bar_label;

	/* Navigation buttons.  */
	GtkWidget *back_button;
	GtkWidget *forward_button;

	/* The button.  */
	GtkWidget *title_button;
	GtkWidget *title_button_label;
	GtkWidget *title_button_arrow;

	gboolean title_clickable;
};

enum {
	TITLE_TOGGLED,
	BACK_CLICKED,
	FORWARD_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static const char *down_arrow_xpm[] = {
	"11 5  2 1",
	" 	c none",
	".	c #ffffffffffff",
	" ......... ",
	"  .......  ",
	"   .....   ",
	"    ...    ",
	"     .     ",
};

static const char *left_arrow_xpm[] = {
	"11 7  2 1",
	" 	c none",
	".	c #ffffffffffff",
	"    .      ",
	"   ..      ",
	"  ........ ",
	" ......... ",
	"  ........ ",
	"   ..      ",
	"    .      ",
};

static const char *right_arrow_xpm[] = {
	"11 7  2 1",
	" 	c none",
	".	c #ffffffffffff",
	"      .    ",
	"      ..   ",
	" ........  ",
	" ......... ",
	" ........  ",
	"      ..   ",
	"      .    ",
};


/* Icon pixmap.  */

static GtkWidget *
create_pixmap_widget_from_xpm (const char **xpm) 
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *widget;

	pixbuf = gdk_pixbuf_new_from_xpm_data (xpm);

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 127);

	widget = gtk_pixmap_new (pixmap, mask);
	gtk_widget_show (widget);

	return widget;
}


#if 0				/* This code is kinda broken in some subtle way
				   I haven't been able to figure out.  */

static void
label_realize_callback (GtkWidget *widget,
			void *data)
{
	GtkStyle *style;
	EFont *e_font;
	GdkFont *bolded_font;

	g_assert (widget->style->font != NULL);

	style = gtk_style_copy (widget->style);
	gtk_style_unref (widget->style);
	widget->style = style;

	e_font = e_font_from_gdk_font (style->font);
	bolded_font = e_font_to_gdk_font (e_font, E_FONT_BOLD);
	e_font_unref (e_font);

	if (bolded_font != NULL) {
		gdk_font_unref (style->font);
		style->font = bolded_font;
	}

	gtk_style_attach (style, widget->window);

	if (E_IS_CLIPPED_LABEL (widget)) {
		char *text;

		text = g_strdup (e_clipped_label_get_text (E_CLIPPED_LABEL (widget)));
		e_clipped_label_set_text (E_CLIPPED_LABEL (widget), text);
		g_free (text);
	}
}

static void
make_bold (GtkWidget *widget)
{
	gtk_signal_connect (GTK_OBJECT (widget), "realize",
			    GTK_SIGNAL_FUNC (label_realize_callback), NULL);
}

#endif

static void
set_title_bar_label_style (GtkWidget *widget)
{
	GtkRcStyle *rc_style;

	rc_style = gtk_rc_style_new();

	rc_style->color_flags[GTK_STATE_NORMAL] |= GTK_RC_FG;
	rc_style->fg[GTK_STATE_NORMAL].red = 0xffff;
	rc_style->fg[GTK_STATE_NORMAL].green = 0xffff;
	rc_style->fg[GTK_STATE_NORMAL].blue = 0xffff;

	gtk_widget_modify_style (widget, rc_style);
	gtk_rc_style_unref (rc_style);
}


/* Utility functions.  */

static int
get_max_clipped_label_width (EClippedLabel *clipped_label)
{
	GdkFont *font;
	int width;

	font = GTK_WIDGET (clipped_label)->style->font;

	width = gdk_string_width (font, clipped_label->label);
	width += 2 * GTK_MISC (clipped_label)->xpad;

	return width;
}

static void
size_allocate_icon (EShellFolderTitleBar *title_bar,
		    GtkAllocation *allocation,
		    int *available_width_inout)
{
	EShellFolderTitleBarPrivate *priv;
	GtkRequisition icon_requisition;
	GtkAllocation icon_allocation;
	int border_width;

	priv = title_bar->priv;

	if (priv->icon_widget == NULL)
		return;

	border_width = GTK_CONTAINER (title_bar)->border_width;

	gtk_widget_get_child_requisition (priv->icon_widget, &icon_requisition);

	icon_allocation.x      = allocation->x + allocation->width - border_width - icon_requisition.width;
	icon_allocation.y      = allocation->y + border_width;
	icon_allocation.width  = icon_requisition.width;
	icon_allocation.height = allocation->height - 2 * border_width;

	gtk_widget_size_allocate (priv->icon_widget, &icon_allocation);

	*available_width_inout -= icon_allocation.width;
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
size_allocate_navigation_buttons (EShellFolderTitleBar *title_bar,
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

static void
add_icon_widget (EShellFolderTitleBar *folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	priv = folder_title_bar->priv;

	g_assert (priv->icon != NULL);

	gdk_pixbuf_render_pixmap_and_mask (priv->icon, &pixmap, &mask, 128);

	if (priv->icon_widget != NULL)
		gtk_widget_destroy (priv->icon_widget);

	priv->icon_widget = gtk_pixmap_new (pixmap, mask);

	gdk_pixmap_unref (pixmap);
	gdk_pixmap_unref (mask);

	gtk_misc_set_alignment (GTK_MISC (priv->icon_widget), 1.0, .5);
	gtk_misc_set_padding (GTK_MISC (priv->icon_widget), 0, 0);

	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->icon_widget, FALSE, TRUE, 2);
	gtk_widget_show (priv->icon_widget);
}


/* The back/forward navigation buttons.  */

static void
back_button_clicked_callback (GtkButton *button,
			      void *data)
{
	EShellFolderTitleBar *folder_title_bar;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (data);

	gtk_signal_emit (GTK_OBJECT (folder_title_bar), signals[BACK_CLICKED]);
}

static void
forward_button_clicked_callback (GtkButton *button,
				 void *data)
{
	EShellFolderTitleBar *folder_title_bar;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (data);

	gtk_signal_emit (GTK_OBJECT (folder_title_bar), signals[FORWARD_CLICKED]);
}

static void
add_navigation_buttons (EShellFolderTitleBar *folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;
	GtkWidget *back_label, *back_pixmap, *back_box;
	GtkWidget *forward_pixmap;

	priv = folder_title_bar->priv;

	priv->back_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->back_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS (priv->back_button, GTK_CAN_FOCUS);

	back_label = gtk_label_new (_("Back"));
	set_title_bar_label_style (back_label);
	back_pixmap = create_pixmap_widget_from_xpm (left_arrow_xpm);

	back_box = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (back_box), back_pixmap, FALSE, TRUE, 0); 
	gtk_box_pack_start (GTK_BOX (back_box), back_label, FALSE, TRUE, 0); 

	gtk_container_add (GTK_CONTAINER (priv->back_button), back_box);

	gtk_signal_connect (GTK_OBJECT (priv->back_button), "clicked",
			    GTK_SIGNAL_FUNC (back_button_clicked_callback), folder_title_bar);

	priv->forward_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->forward_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS (priv->forward_button, GTK_CAN_FOCUS);

	forward_pixmap = create_pixmap_widget_from_xpm (right_arrow_xpm);
	gtk_container_add (GTK_CONTAINER (priv->forward_button), forward_pixmap);

	gtk_signal_connect (GTK_OBJECT (priv->forward_button), "clicked",
			    GTK_SIGNAL_FUNC (forward_button_clicked_callback), folder_title_bar);

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
	gtk_signal_emit (GTK_OBJECT (folder_title_bar),
			 signals[TITLE_TOGGLED],
			 gtk_toggle_button_get_active (title_button));
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (object);
	priv = folder_title_bar->priv;

	if (priv->icon != NULL)
		gdk_pixbuf_unref (priv->icon);
	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GTkWidget methods. */

static void
realize (GtkWidget *widget)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (widget);
	priv = folder_title_bar->priv;

	if (priv->icon != NULL)
		add_icon_widget (E_SHELL_FOLDER_TITLE_BAR (widget));
}

static void
unrealize (GtkWidget *widget)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (widget);
	priv = folder_title_bar->priv;

	if (priv->icon_widget != NULL) {
		gtk_widget_destroy (priv->icon_widget);
		priv->icon_widget = NULL;
	}
}

static void
size_allocate (GtkWidget *widget,
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

	offset = size_allocate_navigation_buttons (title_bar, allocation);
	available_width -= offset;

	size_allocate_icon (title_bar, allocation, & available_width);
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


static void
class_init (EShellFolderTitleBarClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->realize       = realize;
	widget_class->unrealize     = unrealize;
	widget_class->size_allocate = size_allocate;

	parent_class = gtk_type_class (PARENT_TYPE);

	signals[TITLE_TOGGLED] = gtk_signal_new ("title_toggled",
						 GTK_RUN_FIRST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (EShellFolderTitleBarClass, title_toggled),
						 gtk_marshal_NONE__BOOL,
						 GTK_TYPE_NONE, 1,
						 GTK_TYPE_BOOL);

	signals[BACK_CLICKED] = gtk_signal_new ("back_clicked",
						GTK_RUN_FIRST,
						object_class->type,
						GTK_SIGNAL_OFFSET (EShellFolderTitleBarClass, back_clicked),
						gtk_marshal_NONE__NONE,
						GTK_TYPE_NONE, 0);

	signals[FORWARD_CLICKED] = gtk_signal_new ("forward_clicked",
						   GTK_RUN_FIRST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (EShellFolderTitleBarClass, forward_clicked),
						   gtk_marshal_NONE__NONE,
						   GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EShellFolderTitleBar *shell_folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;

	priv = g_new (EShellFolderTitleBarPrivate, 1);

	priv->icon               = NULL;
	priv->icon_widget        = NULL;
	priv->title_label        = NULL;
	priv->folder_bar_label   = NULL;
	priv->title_button_label = NULL;
	priv->title_button       = NULL;
	priv->title_button_arrow = NULL;
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

	priv->title_label = e_clipped_label_new ("");
	gtk_misc_set_padding (GTK_MISC (priv->title_label), 5, 0);
	gtk_misc_set_alignment (GTK_MISC (priv->title_label), 0.0, 0.5);
	set_title_bar_label_style (priv->title_label);
	/* make_bold (priv->title_label); */

	priv->title_button_label = e_clipped_label_new ("");
	gtk_misc_set_padding (GTK_MISC (priv->title_button_label), 2, 0);
	gtk_misc_set_alignment (GTK_MISC (priv->title_button_label), 0.0, 0.5);
	gtk_widget_show (priv->title_button_label);
	set_title_bar_label_style (priv->title_button_label);
	/* make_bold (priv->title_label); */

	priv->folder_bar_label = e_clipped_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->folder_bar_label), 1.0, 0.5);
	gtk_widget_show (priv->folder_bar_label);
	set_title_bar_label_style (priv->folder_bar_label);

	title_button_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_button_hbox), priv->title_button_label,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (title_button_hbox), create_pixmap_widget_from_xpm (down_arrow_xpm),
			    FALSE, TRUE, 2);
	gtk_widget_show (title_button_hbox);

	priv->title_button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->title_button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (priv->title_button), title_button_hbox);
	GTK_WIDGET_UNSET_FLAGS (priv->title_button, GTK_CAN_FOCUS);
	gtk_widget_show (priv->title_button);

	gtk_container_set_border_width (GTK_CONTAINER (folder_title_bar), 2);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->title_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->title_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (folder_title_bar), priv->folder_bar_label, TRUE, TRUE, 0);

	/* Make the label have a border as large as the button's.
	   FIXME: This is really hackish.  The hardcoded numbers should be OK
	   as the padding is hardcoded in GtkButton too (see CHILD_SPACING in
	   gtkbutton.c).  */
	gtk_misc_set_padding (GTK_MISC (priv->title_label),
			      GTK_WIDGET (priv->title_button)->style->klass->xthickness + 3,
			      GTK_WIDGET (priv->title_button)->style->klass->ythickness + 1);

	gtk_signal_connect (GTK_OBJECT (priv->title_button), "toggled",
			    GTK_SIGNAL_FUNC (title_button_toggled_cb), folder_title_bar);

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

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	new = gtk_type_new (e_shell_folder_title_bar_get_type ());

	e_shell_folder_title_bar_construct (new);
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

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
				   const GdkPixbuf *icon)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (icon != NULL);

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	gdk_pixbuf_ref ((GdkPixbuf *) icon);
	if (priv->icon != NULL)
		gdk_pixbuf_unref (priv->icon);
	priv->icon = (GdkPixbuf *) icon;

	if (priv->icon != NULL)
		add_icon_widget (folder_title_bar);
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
		gtk_widget_show (priv->title_button);
	} else {
		gtk_widget_hide (priv->title_button);
		gtk_widget_show (priv->title_label);
	}

	priv->title_clickable = !! title_clickable;
}


E_MAKE_TYPE (e_shell_folder_title_bar, "EShellFolderTitleBar", EShellFolderTitleBar, class_init, init, PARENT_TYPE)
