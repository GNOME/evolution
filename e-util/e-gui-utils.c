/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * GUI utility functions
 *
 * Authors:
 *   Miguel de Icaza (miguel@ximian.com)
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 1999 Miguel de Icaza
 * Copyright (C) 2000-2003 Ximian, Inc.
 */
#include <config.h>

#include "e-gui-utils.h"

#include <glib.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>

GtkWidget *e_create_image_widget(gchar *name,
				 gchar *string1, gchar *string2,
				 gint int1, gint int2)
{
	char *filename;
	GtkWidget *alignment = NULL;
	if (string1) {
		GtkWidget *w;

		if (*string1 == '/')
			filename = g_strdup(string1);
		else
			filename = g_build_filename (EVOLUTION_IMAGES, string1, NULL);

		w = gtk_image_new_from_file (filename);

		alignment = gtk_widget_new(gtk_alignment_get_type(),
					   "child", w,
					   "xalign", (double) 0,
					   "yalign", (double) 0,
					   "xscale", (double) 0,
					   "yscale", (double) 0,
					   NULL);

		gtk_widget_show_all (alignment);
		g_free (filename);
	}

	return alignment;
}

GtkWidget *
e_button_new_with_stock_icon (const char *label_str, const char *stockid)
{
	GtkWidget *button, *hbox, *label, *align, *image;

	button = gtk_button_new ();

	label = gtk_label_new_with_mnemonic (label_str);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);

	image = gtk_image_new_from_stock (stockid, GTK_ICON_SIZE_BUTTON);
	hbox = gtk_hbox_new (FALSE, 2);

	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      
	gtk_container_add (GTK_CONTAINER (button), align);
	gtk_container_add (GTK_CONTAINER (align), hbox);
	gtk_widget_show_all (align);

	return button;
}

