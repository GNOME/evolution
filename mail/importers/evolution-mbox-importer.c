/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* evolution-mbox-importer.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 Novell, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>

#include <bonobo/bonobo-control.h>
#include <libgnome/gnome-i18n.h>

#include <camel/camel-exception.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mail/em-folder-selection-button.h"

#include "mail/mail-component.h"
#include "mail/mail-mt.h"

#include "mail-importer.h"

/*  #define IMPORTER_DEBUG */
#ifdef IMPORTER_DEBUG
#define IN g_print ("=====> %s (%d)\n", G_GNUC_FUNCTION, __LINE__)
#define OUT g_print ("<==== %s (%d)\n", G_GNUC_FUNCTION, __LINE__)
#else
#define IN
#define OUT
#endif

typedef struct {
	EvolutionImporter *ii;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *cancel;	/* cancel/status port */

	GtkWidget *selector;
	GtkWidget *label;
	GtkWidget *progressbar;
	GtkWidget *dialog;

	char *uri;
} MboxImporter;

static void
process_item_fn(EvolutionImporter *eimporter, CORBA_Object listener, void *data, CORBA_Environment *ev)
{
	/*MboxImporter *importer = data;*/
	GNOME_Evolution_ImporterListener_ImporterResult result;

	/* This is essentially a NOOP, it merely returns ok/fail and is only called once */

#if 0
	if (camel_exception_is_set(importer->ex))
		result = GNOME_Evolution_ImporterListener_BAD_FILE;
	else
#endif
		result = GNOME_Evolution_ImporterListener_OK;

	GNOME_Evolution_ImporterListener_notifyResult(listener, result, FALSE, ev);
	bonobo_object_unref(BONOBO_OBJECT(eimporter));
}

static void
folder_selected(EMFolderSelectionButton *button, MboxImporter *importer)
{
	g_free(importer->uri);
	importer->uri = g_strdup(em_folder_selection_button_get_selection(button));
}

static void
create_control_fn(EvolutionImporter *importer, Bonobo_Control *control, void *data)
{
	GtkWidget *hbox, *w;
	
	hbox = gtk_hbox_new(FALSE, 0);

	w = gtk_label_new(_("Destination folder:"));
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, TRUE, 6);

	w = em_folder_selection_button_new(_("Select folder"), _("Select folder to import into"));
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)w,
						 mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_INBOX));
	g_signal_connect(w, "selected", G_CALLBACK(folder_selected), data);
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, TRUE, 6);

	gtk_widget_show_all(hbox);

	/* Another weird-arsed shell api */
	*control = BONOBO_OBJREF(bonobo_control_new(hbox));
}

static gboolean
support_format_fn(EvolutionImporter *importer, const char *filename, void *closure)
{
	char signature[6];
	gboolean ret = FALSE;
	int fd, n;

	fd = open(filename, O_RDONLY);
	if (fd != -1) {
		n = read(fd, signature, 5);
		ret = n == 5 && memcmp(signature, "From ", 5) == 0;
		close(fd);
	}

	return ret; 
}

static void
importer_destroy_cb(void *data, GObject *object)
{
	MboxImporter *importer = data;

	if (importer->status_timeout_id)
		g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);

	if (importer->dialog)
		gtk_widget_destroy(importer->dialog);

	g_free(importer);
}

static void
mbox_status(CamelOperation *op, const char *what, int pc, void *data)
{
	MboxImporter *importer = data;

	if (pc == CAMEL_OPERATION_START)
		pc = 0;
	else if (pc == CAMEL_OPERATION_END)
		pc = 100;

	g_mutex_lock(importer->status_lock);
	g_free(importer->status_what);
	importer->status_what = g_strdup(what);
	importer->status_pc = pc;
	g_mutex_unlock(importer->status_lock);
}

static gboolean
mbox_status_timeout(void *data)
{
	MboxImporter *importer = data;
	int pc;
	char *what;

	if (!importer->status_what)
		return TRUE;

	g_mutex_lock(importer->status_lock);
	what = importer->status_what;
	importer->status_what = NULL;
	pc = importer->status_pc;
	g_mutex_unlock(importer->status_lock);

	gtk_progress_bar_set_fraction((GtkProgressBar *)importer->progressbar, (gfloat)(pc/100.0));
	gtk_progress_bar_set_text((GtkProgressBar *)importer->progressbar, what);
	
	return TRUE;
}

static void
mbox_importer_response(GtkWidget *w, guint button, void *data)
{
	MboxImporter *importer = data;

	if (button == GTK_RESPONSE_CANCEL
	    && importer->cancel)
		camel_operation_cancel(importer->cancel);
}

static gboolean
load_file_fn(EvolutionImporter *eimporter, const char *filename, void *data)
{
	MboxImporter *importer = data;
	char *utf8_filename;
	
	utf8_filename = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	importer->dialog = gtk_message_dialog_new(NULL, 0/*GTK_DIALOG_NO_SEPARATOR*/,
						  GTK_MESSAGE_INFO, GTK_BUTTONS_CANCEL,
						  _("Importing `%s'"), utf8_filename);
	g_free (utf8_filename);
	gtk_window_set_title (GTK_WINDOW (importer->dialog), _("Importing..."));

	importer->label = gtk_label_new (_("Please wait"));
	importer->progressbar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (importer->dialog)->vbox), importer->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (importer->dialog)->vbox), importer->progressbar, FALSE, FALSE, 0);
	g_signal_connect(importer->dialog, "response", G_CALLBACK(mbox_importer_response), importer);
	gtk_widget_show_all(importer->dialog);

	importer->status_timeout_id = g_timeout_add(100, mbox_status_timeout, importer);
	importer->cancel = camel_operation_new(mbox_status, importer);

	mail_msg_wait(mail_importer_import_mbox(filename, importer->uri, importer->cancel));

	camel_operation_unref(importer->cancel);
	g_source_remove(importer->status_timeout_id);
	importer->status_timeout_id = 0;

	return TRUE;
}

BonoboObject *
mbox_importer_new(void)
{
	MboxImporter *mbox;
	
	mbox = g_new0 (MboxImporter, 1);
	mbox->status_lock = g_mutex_new();
	mbox->ii = evolution_importer_new(create_control_fn, support_format_fn, load_file_fn, process_item_fn, NULL, mbox);
	g_object_weak_ref(G_OBJECT(mbox->ii), importer_destroy_cb, mbox);
	
	return BONOBO_OBJECT (mbox->ii);
}
