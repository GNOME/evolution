/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-request.c
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

#include "e-request.h"

#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>


/**
 * e_request_string:
 * @parent: parent window, or %NULL
 * @title: the dialog title (in the locale character set)
 * @prompt: the prompt (in the locale character set)
 * @default: default value (in UTF8)
 * 
 * Request a string from the user.
 * 
 * Return value: %NULL if the user cancelled the dialog, the inserted
 * string (in UTF8) otherwise. The string must be freed by the caller.
 **/
char *
e_request_string (GtkWindow *parent,
		  const char *title,
		  const char *prompt,
		  const char *default_string)
{
	GtkWidget *prompt_label;
	char *text;
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *vbox;
	
	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (prompt != NULL, NULL);
	
	dialog = gtk_dialog_new_with_buttons (title, parent,
					      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 275, -1);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6); 

	vbox = GTK_DIALOG (dialog)->vbox;
	
	prompt_label = gtk_label_new (prompt);
	gtk_box_pack_start (GTK_BOX (vbox), prompt_label, TRUE, TRUE, 6);
	gtk_box_set_spacing (GTK_BOX (vbox), 6); 

	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), default_string);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 3);

	atk_object_set_description (gtk_widget_get_accessible (entry), prompt);	

	gtk_widget_grab_focus (entry);
	
	gtk_widget_show (prompt_label);
	gtk_widget_show (entry);
	gtk_widget_show (dialog);
	
	switch (gtk_dialog_run (GTK_DIALOG (dialog))) {
	case GTK_RESPONSE_OK:
		text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		break;
	default:
		text = NULL;
		break;
	}

	gtk_widget_destroy (dialog);
	
	return text;
}
