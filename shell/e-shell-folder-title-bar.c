/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-title-bar.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "widgets/misc/e-clipped-label.h"
#include "e-util/e-util.h"

#include "e-shell-constants.h"
#include "e-shell-folder-title-bar.h"


#define PARENT_TYPE GTK_TYPE_EVENT_BOX
static GtkEventBox *parent_class = NULL;

struct _EShellFolderTitleBarPrivate {
	GdkPixbuf *icon;
	GtkWidget *icon_widget;

	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *button_hbox;
	GtkWidget *button;
};

enum {
	TITLE_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Icon handling.  */

static unsigned int
rgb_from_gdk_color (GdkColor *color)
{
	return (((color->red >> 8) << 16) |
		((color->green >> 8) << 8) |
		((color->blue >> 8)));
}

static GdkPixmap *
make_icon_pixmap (EShellFolderTitleBar *folder_title_bar,
		  const GdkPixbuf *pixbuf)
{
	GdkPixmap *pixmap;
	GtkWidget *widget;
	unsigned int depth;
	unsigned int rgb;

	widget = GTK_WIDGET (folder_title_bar);

	rgb = rgb_from_gdk_color (&widget->style->bg[GTK_STATE_NORMAL]);

	depth = gtk_widget_get_visual (widget)->depth;

	pixmap = gdk_pixmap_new (GTK_WIDGET (folder_title_bar)->window,
				 E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
				 depth);

	if (pixbuf == NULL) {
		gdk_draw_rectangle (pixmap, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
				    0, 0, E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE);
	} else {
		GdkPixbuf *composited_pixbuf;

		composited_pixbuf = gdk_pixbuf_composite_color_simple
			(pixbuf, gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
			 GDK_INTERP_NEAREST, 255, 32, rgb, rgb);

		gdk_pixbuf_render_to_drawable (composited_pixbuf,
					       pixmap, widget->style->fg_gc[GTK_STATE_NORMAL],
					       0, 0, 0, 0,
					       E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
					       GDK_RGB_DITHER_MAX, 0, 0);

		gdk_pixbuf_unref (composited_pixbuf);
	}

	return pixmap;
}


/* Style handling.  */

static void
endarken_style (GtkWidget *widget)
{
	GtkStyle *style;
	GtkRcStyle *new_rc_style;
	int i;

	style = widget->style;

	new_rc_style = gtk_rc_style_new ();

	for (i = 0; i < 5; i++) {
		new_rc_style->bg[i].red      = style->bg[i].red * .8;
		new_rc_style->bg[i].green    = style->bg[i].green * .8;
		new_rc_style->bg[i].blue     = style->bg[i].blue * .8;
		new_rc_style->fg[i].red      = 0xffff;
		new_rc_style->fg[i].green    = 0xffff;
		new_rc_style->fg[i].blue     = 0xffff;
		new_rc_style->color_flags[i] = GTK_RC_BG | GTK_RC_FG;
	}

	gtk_widget_modify_style (widget, new_rc_style);

	gtk_rc_style_unref (new_rc_style);
}

static void
style_set_cb (GtkWidget *widget,
	      GtkStyle *previous_style,
	      void *data)
{
	/* This will cause a style_set signal to be emitted again, so we need to do this to prevent infinite recursion.  */
	gtk_signal_handler_block_by_func (GTK_OBJECT (widget), GTK_SIGNAL_FUNC (style_set_cb), data);

	endarken_style (widget);

	gtk_signal_handler_unblock_by_func (GTK_OBJECT (widget), GTK_SIGNAL_FUNC (style_set_cb), data);
}

static void
endarken_and_connect_style_set_signal (GtkWidget *widget)
{
	endarken_style (widget);
	gtk_signal_connect (GTK_OBJECT (widget), "style_set",
			    GTK_SIGNAL_FUNC (style_set_cb), NULL);
}

static void
setup_style (EShellFolderTitleBar *folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;

	priv = folder_title_bar->priv;

	/* endarken_and_connect_style_set_signal (priv->button); */
	endarken_and_connect_style_set_signal (priv->label);
	endarken_and_connect_style_set_signal (GTK_WIDGET (folder_title_bar));
}


/* Button signals.  */

static void
title_button_clicked_cb (GtkButton *button,
			 void *data)
{
	EShellFolderTitleBar *folder_title_bar;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (data);

	gtk_signal_emit (GTK_OBJECT (folder_title_bar), signals[TITLE_CLICKED]);
}


/* GTkWidget methods. */

static void
realize (GtkWidget *widget)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;
	GdkPixmap *pixmap;

	(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (widget);
	priv = folder_title_bar->priv;

	pixmap = make_icon_pixmap (folder_title_bar, priv->icon);
	priv->icon_widget = gtk_pixmap_new (pixmap, NULL);
	gdk_pixmap_unref (pixmap);
	gtk_widget_show (priv->icon_widget);

	gtk_misc_set_alignment (GTK_MISC (priv->icon_widget), 1.0, .5);
	gtk_misc_set_padding (GTK_MISC (priv->icon_widget), 5, 0);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->icon_widget, TRUE, TRUE, 2);
}

static void
unrealize (GtkWidget *widget)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (widget);
	priv = folder_title_bar->priv;

	gtk_widget_destroy (priv->icon_widget);
	priv->icon_widget = NULL;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShellFolderTitleBar *folder_title_bar;
	EShellFolderTitleBarPrivate *priv;

	folder_title_bar = E_SHELL_FOLDER_TITLE_BAR (object);
	priv = folder_title_bar->priv;

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EShellFolderTitleBarClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->realize   = realize;
	widget_class->unrealize = unrealize;

	parent_class = gtk_type_class (PARENT_TYPE);

	signals[TITLE_CLICKED] = gtk_signal_new ("title_clicked",
						 GTK_RUN_FIRST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (EShellFolderTitleBarClass, title_clicked),
						 gtk_marshal_NONE__NONE,
						 GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EShellFolderTitleBar *shell_folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;

	priv = g_new (EShellFolderTitleBarPrivate, 1);
	priv->icon        = NULL;
	priv->icon_widget = NULL;
	priv->hbox        = NULL;
	priv->label       = NULL;
	priv->button_hbox = NULL;
	priv->button      = NULL;

	shell_folder_title_bar->priv = priv;
}


void
e_shell_folder_title_bar_construct (EShellFolderTitleBar *folder_title_bar)
{
	EShellFolderTitleBarPrivate *priv;
	GtkWidget *widget;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;
	widget = GTK_WIDGET (folder_title_bar);

	priv->label = e_clipped_label_new ("");
	gtk_misc_set_padding (GTK_MISC (priv->label), 5, 0);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
	gtk_widget_show (priv->label);

#if 0
	priv->button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (priv->button), priv->label);
	GTK_WIDGET_UNSET_FLAGS (priv->button, GTK_CAN_FOCUS);
	gtk_widget_show (priv->button);
#endif

	priv->hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (priv->hbox), 2);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->label, TRUE, TRUE, 0);

	gtk_widget_show (priv->hbox);

#if 0
	gtk_signal_connect (GTK_OBJECT (priv->button), "clicked",
			    GTK_SIGNAL_FUNC (title_button_clicked_cb), folder_title_bar);
#endif

	gtk_container_add (GTK_CONTAINER (folder_title_bar), priv->hbox);

	setup_style (folder_title_bar);

	e_shell_folder_title_bar_set_title (folder_title_bar, NULL);
}

GtkWidget *
e_shell_folder_title_bar_new (void)
{
	EShellFolderTitleBar *new;

	new = gtk_type_new (e_shell_folder_title_bar_get_type ());

	e_shell_folder_title_bar_construct (new);

	return GTK_WIDGET (new);
}

void
e_shell_folder_title_bar_set_title (EShellFolderTitleBar *folder_title_bar,
				    const char *title)
{
	EShellFolderTitleBarPrivate *priv;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	if (title == NULL)
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->label), _("(Untitled)"));
	else
		e_clipped_label_set_text (E_CLIPPED_LABEL (priv->label), title);

	/* FIXME: There seems to be a bug in EClippedLabel, this is just a workaround.  */
	gtk_widget_queue_draw (GTK_WIDGET (folder_title_bar));
}

void
e_shell_folder_title_bar_set_icon (EShellFolderTitleBar *folder_title_bar,
				   const GdkPixbuf *icon)
{
	EShellFolderTitleBarPrivate *priv;
	GdkPixmap *pixmap;

	g_return_if_fail (folder_title_bar != NULL);
	g_return_if_fail (E_IS_SHELL_FOLDER_TITLE_BAR (folder_title_bar));

	priv = folder_title_bar->priv;

	pixmap = make_icon_pixmap (folder_title_bar, icon);

	gtk_pixmap_set (GTK_PIXMAP (priv->icon_widget), pixmap, NULL);
}


E_MAKE_TYPE (e_shell_folder_title_bar, "EShellFolderTitleBar", EShellFolderTitleBar, class_init, init, PARENT_TYPE)
