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
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>

#include <glib/gi18n.h>

#include <camel/camel-exception.h>

#include "mail/em-folder-selection-button.h"

#include "mail/mail-component.h"
#include "mail/mail-mt.h"

#include "mail-importer.h"

#include "e-util/e-import.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *cancel;	/* cancel/status port */

	char *uri;
} MboxImporter;

static void
folder_selected(EMFolderSelectionButton *button, EImportTargetURI *target)
{
	g_free(target->uri_dest);
	target->uri_dest = g_strdup(em_folder_selection_button_get_selection(button));
}

static GtkWidget *
mbox_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *hbox, *w;
	
	hbox = gtk_hbox_new(FALSE, 0);

	w = gtk_label_new(_("Destination folder:"));
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, TRUE, 6);

	w = em_folder_selection_button_new(_("Select folder"), _("Select folder to import into"));
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)w,
						 mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_INBOX));
	g_signal_connect(w, "selected", G_CALLBACK(folder_selected), target);
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, TRUE, 6);

	w = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start((GtkBox *)w, hbox, FALSE, FALSE, 0);
	gtk_widget_show_all(w);

	return w;
}

static gboolean
mbox_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	char signature[6];
	gboolean ret = FALSE;
	int fd, n;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", strlen("file:///")) != 0)
		return FALSE;

	fd = open(s->uri_src + strlen("file://"), O_RDONLY);
	if (fd != -1) {
		n = read(fd, signature, 5);
		ret = n == 5 && memcmp(signature, "From ", 5) == 0;
		close(fd);
	}

	return ret; 
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

	if (importer->status_what) {
		g_mutex_lock(importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock(importer->status_lock);

		e_import_status(importer->import, (EImportTarget *)importer->target, what, pc);
	}

	return TRUE;
}

static void
mbox_import_done(void *data, CamelException *ex)
{
	MboxImporter *importer = data;

	g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);
	camel_operation_unref(importer->cancel);

	e_import_complete(importer->import, importer->target);
	g_free(importer);
}

static void
mbox_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	MboxImporter *importer;

	/* TODO: do we validate target? */

	importer = g_malloc0(sizeof(*importer));
	g_datalist_set_data(&target->data, "mbox-data", importer);
	importer->import = ei;
	importer->target = target;
	importer->status_lock = g_mutex_new();
	importer->status_timeout_id = g_timeout_add(100, mbox_status_timeout, importer);
	importer->cancel = camel_operation_new(mbox_status, importer);

	mail_importer_import_mbox(((EImportTargetURI *)target)->uri_src+strlen("file://"), ((EImportTargetURI *)target)->uri_dest, importer->cancel, mbox_import_done, importer);
}

static void
mbox_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	MboxImporter *importer = g_datalist_get_data(&target->data, "mbox-data");

	if (importer)
		camel_operation_cancel(importer->cancel);
}

static EImportImporter mbox_importer = {
	E_IMPORT_TARGET_URI,
	0,
	mbox_supported,
	mbox_getwidget,
	mbox_import,
	mbox_cancel,
};

EImportImporter *
mbox_importer_peek(void)
{
	mbox_importer.name = _("Berkeley Mailbox (mbox)");
	mbox_importer.description = _("Importer Berkeley Mailbox format folders");

	return &mbox_importer;
}
