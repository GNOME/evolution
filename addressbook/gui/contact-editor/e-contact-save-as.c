/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include "e-contact-save-as.h"

#include <unistd.h>
#include <fcntl.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-dialog.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <libgnome/gnome-i18n.h>
#include <errno.h>
#include <string.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>

static int file_exists(GtkFileSelection *filesel, const char *filename);

typedef struct {
	GtkFileSelection *filesel;
	char *vcard;
} SaveAsInfo;

static void
save_it(GtkWidget *widget, SaveAsInfo *info)
{
	gint error = 0;
	gint response = 0;
	
	const char *filename = gtk_file_selection_get_filename (info->filesel);

	error = e_write_file (filename, info->vcard, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC);

	if (error == EEXIST) {
		response = file_exists(info->filesel, filename);
		switch (response) {
			case 0 : /* Overwrite */
				e_write_file(filename, info->vcard, O_WRONLY | O_CREAT | O_TRUNC);
				break;
			case 1 : /* cancel */
				return;
		}
	} else if (error != 0) {
		GtkWidget *dialog;
		char *str;

		str = g_strdup_printf ("Error saving %s: %s", filename, strerror(errno));
		dialog = gnome_message_box_new (str, GNOME_MESSAGE_BOX_ERROR, GNOME_STOCK_BUTTON_OK, NULL);
		g_free (str);

		gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (info->filesel));

		gtk_widget_show (dialog);
		
		return;
	}
	
	gtk_widget_destroy(GTK_WIDGET(info->filesel));
}

static void
close_it(GtkWidget *widget, SaveAsInfo *info)
{
	gtk_widget_destroy (GTK_WIDGET (info->filesel));
}

static void
destroy_it(GtkWidget *widget, SaveAsInfo *info)
{
	g_free (info->vcard);
	g_free (info);
}

static char *
make_safe_filename (const char *prefix, char *name)
{
	char *safe, *p;

	if (!name) {
		/* This is a filename. Translators take note. */
		name = _("card.vcf");
	}

	p = strrchr (name, '/');
	if (p)
		safe = g_strdup_printf ("%s%s%s", prefix, p, ".vcf");
	else
		safe = g_strdup_printf ("%s/%s%s", prefix, name, ".vcf");
	
	p = strrchr (safe, '/') + 1;
	if (p)
		e_filename_make_safe (p);
	
	return safe;
}

void
e_contact_save_as(char *title, ECard *card, GtkWindow *parent_window)
{
	GtkFileSelection *filesel;
	char *file;
	char *name;
	char *locale_name;
	SaveAsInfo *info = g_new(SaveAsInfo, 1);

	filesel = GTK_FILE_SELECTION(gtk_file_selection_new(title));

	gtk_object_get (GTK_OBJECT (card),
			"file_as", &name,
			NULL);
	locale_name = e_utf8_to_locale_string (name);
	file = make_safe_filename (g_get_home_dir(), locale_name);
	gtk_file_selection_set_filename (filesel, file);
	g_free (file);
	g_free (locale_name);

	info->filesel = filesel;
	info->vcard = e_card_get_vcard(card);

	gtk_signal_connect(GTK_OBJECT(filesel->ok_button), "clicked",
			   save_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel->cancel_button), "clicked",
			   close_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel), "destroy",
			   destroy_it, info);

	if (parent_window) {
		gtk_window_set_transient_for (GTK_WINDOW (filesel),
					      parent_window);
		gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
	}

	gtk_widget_show(GTK_WIDGET(filesel));
}

void
e_contact_list_save_as(char *title, GList *list, GtkWindow *parent_window)
{
	GtkFileSelection *filesel;
	SaveAsInfo *info = g_new(SaveAsInfo, 1);

	filesel = GTK_FILE_SELECTION(gtk_file_selection_new(title));

	/* This is a filename. Translators take note. */
	if (list && list->data && list->next == NULL) {
		char *name, *locale_name, *file;
		gtk_object_get (GTK_OBJECT (list->data),
				"file_as", &name,
				NULL);
		locale_name = e_utf8_to_locale_string (name);
		file = make_safe_filename (g_get_home_dir(), locale_name);
		gtk_file_selection_set_filename (filesel, file);
		g_free (file);
		g_free (locale_name);
	} else {
		char *file;
		file = make_safe_filename (g_get_home_dir(), _("list"));
		gtk_file_selection_set_filename (filesel, file);
		g_free (file);
	}

	info->filesel = filesel;
	info->vcard = e_card_list_get_vcard (list);
	
	gtk_signal_connect(GTK_OBJECT(filesel->ok_button), "clicked",
			   save_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel->cancel_button), "clicked",
			   close_it, info);
	gtk_signal_connect(GTK_OBJECT(filesel), "destroy",
			   destroy_it, info);

	if (parent_window) {
		gtk_window_set_transient_for (GTK_WINDOW (filesel),
					      parent_window);
		gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
	}

	gtk_widget_show(GTK_WIDGET(filesel));
}

static int
file_exists(GtkFileSelection *filesel, const char *filename)
{
	GnomeDialog *dialog = NULL;
	GtkWidget *label;
	GladeXML *gui = NULL;
	int result = 0;
	char *string;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/file-exists.glade", NULL);
	dialog = GNOME_DIALOG(glade_xml_get_widget(gui, "dialog-exists"));
	
	label = glade_xml_get_widget (gui, "label-exists");
	if (GTK_IS_LABEL (label)) {
		string = g_strdup_printf (_("%s already exists\nDo you want to overwrite it?"), filename);
		gtk_label_set_text (GTK_LABEL (label), string);
		g_free (string);
	}

	gnome_dialog_set_parent(dialog, GTK_WINDOW(filesel));

	gtk_widget_show (GTK_WIDGET (dialog));
	result = gnome_dialog_run_and_close(dialog);

	g_free(gui);

	return result;
}
