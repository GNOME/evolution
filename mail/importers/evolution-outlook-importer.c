/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* evolution-outlook-importer.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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
#include <camel/camel-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-store.h>
#include <camel/camel-mime-message.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mail/em-folder-selection-button.h"

#include "mail/mail-component.h"
#include "mail/mail-mt.h"
#include "mail/mail-tools.h"

#include "mail-importer.h"

static int mail_importer_import_outlook(const char *path, const char *folderuri, CamelOperation *cancel);

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
} OutlookImporter;

struct oe_msg_segmentheader {
	gint32 self;
	gint32 increase;
	gint32 include;
	gint32 next;
	gint32 usenet;
};

typedef struct oe_msg_segmentheader oe_msg_segmentheader;


/* EvolutionImporter methods */

/* Based on code from liboe 0.92 (STABLE)
   Copyright (C) 2000 Stephan B. Nedregård (stephan@micropop.com)
   Modified 2001 Iain Holmes  <iain@ximian.com>
   Copyright (C) 2001 Ximian, Inc. */

static void
process_item_fn(EvolutionImporter *eimporter, CORBA_Object listener, void *data, CORBA_Environment *ev)
{
	GNOME_Evolution_ImporterListener_ImporterResult result;
#if 0
	if (camel_exception_is_set(importer->ex))
		result = GNOME_Evolution_ImporterListener_BAD_FILE;
	else
#endif
		result = GNOME_Evolution_ImporterListener_OK;

	GNOME_Evolution_ImporterListener_notifyResult(listener, result, FALSE, ev);
	bonobo_object_unref(BONOBO_OBJECT(eimporter));
}


/* EvolutionImporterFactory methods */

static void
folder_selected(EMFolderSelectionButton *button, OutlookImporter *importer)
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
support_format_fn(EvolutionImporter *importer, const char *filename, void *data)
{
	FILE *handle;
	guint32 signature[4];
	int ok;

	/* Outlook Express sniffer.
	   Taken from liboe 0.92 (STABLE)
	   Copyright (C) 2000 Stephan B. Nedregård (stephan@micropop.com) */

	handle = fopen (filename, "rb");
	if (handle == NULL)
		return FALSE; /* Can't open file: Can't support it :) */

	  /* SIGNATURE */
	fread (&signature, 16, 1, handle);
	/* This needs testing */
#if G_BYTE_ORDER == G_BIG_ENDIAN
	signature[0] = GUINT32_TO_BE(signature[0]);
	signature[1] = GUINT32_TO_BE(signature[1]);
	signature[2] = GUINT32_TO_BE(signature[2]);
	signature[3] = GUINT32_TO_BE(signature[3]);
#endif
	ok = ((signature[0]!=0xFE12ADCF /* OE 5 & OE 5 BETA SIGNATURE */
	       || signature[1]!=0x6F74FDC5
	       || signature[2]!=0x11D1E366
	       || signature[3]!=0xC0004E9A)
	      && (signature[0]==0x36464D4A /* OE4 SIGNATURE */
		  && signature[1]==0x00010003));

	fclose (handle);
	return ok; /* Can't handle OE 5 yet */
}

/* Note the similarity of most of this code to evolution-mbox-importer.
   Yes it should be subclassed, or something ... */
static void
importer_destroy_cb(void *data, GObject *object)
{
	OutlookImporter *importer = data;

	if (importer->status_timeout_id)
		g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);

	if (importer->dialog)
		gtk_widget_destroy(importer->dialog);

	g_free(importer);
}

static void
outlook_status(CamelOperation *op, const char *what, int pc, void *data)
{
	OutlookImporter *importer = data;

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
outlook_status_timeout(void *data)
{
	OutlookImporter *importer = data;
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
outlook_importer_response(GtkWidget *w, guint button, void *data)
{
	OutlookImporter *importer = data;

	if (button == GTK_RESPONSE_CANCEL
	    && importer->cancel)
		camel_operation_cancel(importer->cancel);
}

static gboolean
load_file_fn(EvolutionImporter *eimporter, const char *filename, void *data)
{
	OutlookImporter *importer = data;
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
	g_signal_connect(importer->dialog, "response", G_CALLBACK(outlook_importer_response), importer);
	gtk_widget_show_all(importer->dialog);

	importer->status_timeout_id = g_timeout_add(100, outlook_status_timeout, importer);
	importer->cancel = camel_operation_new(outlook_status, importer);

	mail_msg_wait(mail_importer_import_outlook(filename, importer->uri, importer->cancel));

	camel_operation_unref(importer->cancel);
	g_source_remove(importer->status_timeout_id);
	importer->status_timeout_id = 0;

	return TRUE;
}

BonoboObject *
outlook_importer_new(void)
{
	EvolutionImporter *importer;
	OutlookImporter *oli;

	oli = g_new0 (OutlookImporter, 1);
	oli->status_lock = g_mutex_new();
	importer = evolution_importer_new (create_control_fn, support_format_fn, load_file_fn, process_item_fn, NULL, oli);
	g_object_weak_ref((GObject *)importer, importer_destroy_cb, oli);

	return BONOBO_OBJECT (importer);
}

struct _import_outlook_msg {
	struct _mail_msg msg;
	
	char *path;
	char *uri;
	CamelOperation *cancel;
};

static char *
import_outlook_describe(struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Importing mailbox"));
}

static void
import_outlook_import(struct _mail_msg *mm)
{
	struct _import_outlook_msg *m = (struct _import_outlook_msg *) mm;
	struct stat st;
	CamelFolder *folder;

	if (stat(m->path, &st) == -1) {
		g_warning("cannot find source file to import '%s': %s", m->path, g_strerror(errno));
		return;
	}

	if (m->uri == NULL || m->uri[0] == 0)
		folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_INBOX);
	else
		folder = mail_tool_uri_to_folder(m->uri, CAMEL_STORE_FOLDER_CREATE, &mm->ex);

	if (folder == NULL)
		return;

	if (S_ISREG(st.st_mode)) {
		CamelOperation *oldcancel = NULL;
		CamelMessageInfo *info;
		GByteArray *buffer;
		int fd;
		off_t pos;

		fd = open(m->path, O_RDONLY);
		if (fd == -1) {
			g_warning("cannot find source file to import '%s': %s", m->path, g_strerror(errno));
			goto fail;
		}

		if (lseek(fd, 0x54, SEEK_SET) == -1)
			goto fail;

		if (m->cancel)
			oldcancel = camel_operation_register(m->cancel);

		camel_folder_freeze(folder);

		buffer = g_byte_array_new();
		pos = 0x54;
		do {
			oe_msg_segmentheader header;
			int pc;
			size_t len;
			CamelStream *mem;
			CamelMimeMessage *msg;

			if (st.st_size > 0)
				pc = (int)(100.0 * ((double)pos / (double)st.st_size));
			camel_operation_progress(NULL, pc);

			if (read(fd, &header, sizeof(header)) != sizeof(header))
				goto fail2;

			pos += sizeof(header);

#if G_BYTE_ORDER == G_BIG_ENDIAN
			header.include = GUINT32_TO_BE(header.include);
#endif
			/* the -4 is some magical value */
			len = header.include - sizeof(header) - 4;
			/* sanity check */
			if (len > (pos + st.st_size))
				goto fail2;
			g_byte_array_set_size(buffer, len);
			if (read(fd, buffer->data, len) != len)
				goto fail2;

			pos += len;

			mem = camel_stream_mem_new();
			camel_stream_mem_set_byte_array((CamelStreamMem *)mem, buffer);

			msg = camel_mime_message_new();
			if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg, mem) == -1) {
				camel_object_unref(msg);
				camel_object_unref(mem);
				goto fail2;
			}

			info = camel_message_info_new(NULL);
			/* any headers to read? */

			camel_folder_append_message(folder, msg, info, NULL, &mm->ex);

			camel_message_info_free(info);
			camel_object_unref(msg);
			camel_object_unref(mem);
		} while (!camel_exception_is_set(&mm->ex) && pos < st.st_size);

		camel_folder_sync(folder, FALSE, NULL);
		camel_folder_thaw(folder);
		camel_operation_end(NULL);
	fail2:
		/* TODO: these api's are a bit weird, registering the old is the same as deregistering */
		if (m->cancel)
			camel_operation_register(oldcancel);
		g_byte_array_free(buffer, TRUE);
	}
fail:
	camel_object_unref(folder);
}

static void
import_outlook_done(struct _mail_msg *mm)
{
}

static void
import_outlook_free (struct _mail_msg *mm)
{
	struct _import_outlook_msg *m = (struct _import_outlook_msg *)mm;
	
	if (m->cancel)
		camel_operation_unref(m->cancel);
	g_free(m->uri);
	g_free(m->path);
}

static struct _mail_msg_op import_outlook_op = {
	import_outlook_describe,
	import_outlook_import,
	import_outlook_done,
	import_outlook_free,
};

static int
mail_importer_import_outlook(const char *path, const char *folderuri, CamelOperation *cancel)
{
	struct _import_outlook_msg *m;
	int id;

	m = mail_msg_new(&import_outlook_op, NULL, sizeof (*m));
	m->path = g_strdup(path);
	m->uri = g_strdup(folderuri);
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}

	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}
