/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <unistd.h>
#include <fcntl.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtksignal.h>
#include <gal/util/e-util.h>
#include "e-contact-save-as.h"

typedef struct {
	GtkFileSelection *filesel;
	char *vcard;
} SaveAsInfo;

static void
save_it(GtkWidget *widget, SaveAsInfo *info)
{
	const char *filename = gtk_file_selection_get_filename (info->filesel);

	e_write_file (filename, info->vcard, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC);

	g_free (info->vcard);
	gtk_widget_destroy(GTK_WIDGET(info->filesel));
	g_free(info);
}

static void
close_it(GtkWidget *widget, SaveAsInfo *info)
{
	g_free (info->vcard);
	gtk_widget_destroy (GTK_WIDGET (info->filesel));
	g_free (info);
}

static void
delete_it(GtkWidget *widget, SaveAsInfo *info)
{
	g_free (info->vcard);
	g_free (info);
}

void
e_contact_save_as(char *title, ECard *card)
{
	GtkFileSelection *filesel;
	SaveAsInfo *info = g_new(SaveAsInfo, 1);
	
	filesel = GTK_FILE_SELECTION(gtk_file_selection_new(title));

	info->filesel = filesel;
	info->vcard = e_card_get_vcard(card);
	
	gtk_signal_connect(GTK_OBJECT(filesel->ok_button), "clicked",
			   save_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel->cancel_button), "clicked",
			   close_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel), "delete_event",
			   delete_it, info);
	gtk_widget_show(GTK_WIDGET(filesel));
}

void
e_contact_list_save_as(char *title, GList *list)
{
	GtkFileSelection *filesel;
	SaveAsInfo *info = g_new(SaveAsInfo, 1);
	
	filesel = GTK_FILE_SELECTION(gtk_file_selection_new(title));

	info->filesel = filesel;
	info->vcard = e_card_list_get_vcard (list);
	
	gtk_signal_connect(GTK_OBJECT(filesel->ok_button), "clicked",
			   save_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel->cancel_button), "clicked",
			   close_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel), "delete_event",
			   delete_it, info);
	gtk_widget_show(GTK_WIDGET(filesel));
}
