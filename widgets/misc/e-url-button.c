/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-url-button.c
 *
 * Copyright (C) 2002  JP Rosevear
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
 * Author: JP Rosevear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libgnome/gnome-url.h>
#include "art/connect_to_url-16.xpm"
#include "e-url-button.h"

struct _EUrlButtonPrivate {
	GtkWidget *entry;
};

static void class_init (EUrlButtonClass *klass);
static void init (EUrlButton *url_button);
static void destroy (GtkObject *obj);

static void button_clicked_cb (GtkWidget *widget, gpointer data);

static GtkButtonClass *parent_class = NULL;


GtkType
e_url_button_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info =
		{
			"EUrlButton",
			sizeof (EUrlButton),
			sizeof (EUrlButtonClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (gtk_button_get_type (), &info);
	}

	return type;
}

static void
class_init (EUrlButtonClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_button_get_type ());

	object_class->destroy = destroy;
}


static void
init (EUrlButton *url_button)
{
	EUrlButtonPrivate *priv;
	GdkColormap *colormap;
	GdkPixmap *url_icon;
	GdkBitmap *url_mask;
	GtkWidget *pixmap;

	priv = g_new0 (EUrlButtonPrivate, 1);

	url_button->priv = priv;

	priv->entry = NULL;
	
	colormap = gtk_widget_get_colormap (GTK_WIDGET (url_button));
	url_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, 
							  &url_mask, NULL,
							  connect_to_url_16_xpm);
	
	pixmap = gtk_pixmap_new (url_icon, url_mask);
	gtk_container_add (GTK_CONTAINER (url_button), pixmap);
	gtk_widget_show (pixmap);

	gtk_signal_connect (GTK_OBJECT (url_button), "clicked",
			    GTK_SIGNAL_FUNC (button_clicked_cb), url_button);
}

static void
destroy (GtkObject *obj)
{
	EUrlButton *url_button;
	EUrlButtonPrivate *priv;
	
	url_button = E_URL_BUTTON (obj);
	priv = url_button->priv;
	
	if (priv->entry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->entry));
	
	g_free (priv);
}



GtkWidget *
e_url_button_new (void)
{
	return gtk_type_new (E_TYPE_URL_BUTTON);
}

void
e_url_button_set_entry (EUrlButton *url_button, GtkWidget *entry)
{
	EUrlButtonPrivate *priv;
	
	g_return_if_fail (url_button != NULL);
	g_return_if_fail (E_IS_URL_BUTTON (url_button));
	g_return_if_fail (entry != NULL);
	g_return_if_fail (GTK_IS_ENTRY (entry));

	priv = url_button->priv;

	if (priv->entry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->entry));

	gtk_object_ref (GTK_OBJECT (entry));
	priv->entry = entry;
}

static void
button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EUrlButton *url_button;
	EUrlButtonPrivate *priv;
	char *url;
	
	url_button = E_URL_BUTTON (data);
	priv = url_button->priv;
	
	url = gtk_editable_get_chars (GTK_EDITABLE (priv->entry), 0, -1);
	gnome_url_show (url);
	g_free (url);
}


