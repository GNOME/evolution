/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* evolution-mbox-importer.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>

#include <camel/camel-exception.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-mime-part.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mail/mail-importer.h"
#include "mail-tools.h"

#define IMPORTER_DEBUG
#ifdef IMPORTER_DEBUG
#define IN g_print ("=====> %s (%d)\n", __FUNCTION__, __LINE__)
#define OUT g_print ("<==== %s (%d)\n", __FUNCTION__, __LINE__)
#else
#define IN
#define OUT
#endif

#define MBOX_FACTORY_IID "OAFIID:GNOME_Evolution_Mail_Mbox_ImporterFactory"

typedef struct {
	MailImporter importer; /* Parent */

	char *filename;
	int num;
	CamelMimeParser *mp;
} MboxImporter;

void mail_importer_module_init (void);

/* EvolutionImporter methods */

static void
process_item_fn (EvolutionImporter *eimporter,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	MboxImporter *mbi = (MboxImporter *) closure;
	MailImporter *importer = (MailImporter *) mbi;
	gboolean done = FALSE;
	CamelException *ex;

	ex = camel_exception_new ();
	if (camel_mime_parser_step (mbi->mp, 0, 0) == HSCAN_FROM) {
		/* Import the next message */
		CamelMimeMessage *msg;
		CamelMessageInfo *info;

		IN;
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg),
							   mbi->mp) == -1) {
			g_warning ("Failed message %d", mbi->num);
			camel_object_unref (CAMEL_OBJECT (msg));
			done = TRUE;
		}

		/* write the mesg */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (importer->folder, msg, info, ex);
		g_free (info);
		camel_object_unref (CAMEL_OBJECT (msg));
		if (camel_exception_is_set (ex)) {
			g_warning ("Failed message %d", mbi->num);
			done = TRUE;
		}
		OUT;
	} else {
		IN;
		/* all messages have now been imported */
		camel_folder_sync (importer->folder, FALSE, ex);
		camel_folder_thaw (importer->folder);
		importer->frozen = FALSE;
		done = TRUE;
		OUT;
	}

	if (!done) {
		camel_mime_parser_step (mbi->mp, 0, 0);
	}

	camel_exception_free (ex);
	g_print ("Notifying...\n");
	GNOME_Evolution_ImporterListener_notifyResult (listener,
						       GNOME_Evolution_ImporterListener_OK,
						       !done, ev);
	return;
}

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	char signature[6];
	gboolean ret = FALSE;
	int fd, n;

	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return FALSE;

	n = read (fd, signature, 5);
	if (n > 0) {
		signature[n] = '\0';
		if (!g_strncasecmp (signature, "From ", 5))
			ret = TRUE;
	}

	close (fd);

	return ret; 
}

static void
importer_destroy_cb (GtkObject *object,
		     MboxImporter *mbi)
{
	MailImporter *importer;

	importer = (MailImporter *) mbi;
	if (importer->frozen) {
		camel_folder_sync (importer->folder, FALSE, NULL);
		camel_folder_thaw (importer->folder);
	}

	if (importer->folder)
		camel_object_unref (CAMEL_OBJECT (importer->folder));

	g_free (mbi->filename);
	if (mbi->mp)
		camel_object_unref (CAMEL_OBJECT (mbi->mp));

	g_free (mbi);
}

static void
folder_created_cb (BonoboListener *listener,
		   const char *event_name,
		   const BonoboArg *event_data,
		   CORBA_Environment *ev,
		   MailImporter *importer)
{
	char *fullpath;
	GNOME_Evolution_Storage_FolderResult *result;
	CamelException *ex;

	if (strcmp (event_name, "evolution-shell:folder_created") != 0) {
		return; /* Unknown event */
	}

	result = event_data->_value;
	fullpath = g_strconcat ("file://", result->path, NULL);

	ex = camel_exception_new ();
	importer->folder = mail_tool_uri_to_folder (fullpath, ex);
	
	if (camel_exception_is_set (ex)) {
		g_warning ("Error opening %s", fullpath);
		camel_exception_free (ex);

		g_free (fullpath);
		return;
	}

	g_warning ("%s created", fullpath);
	g_free (fullpath);
	bonobo_object_unref (BONOBO_OBJECT (listener));
}

static gboolean
load_file_fn (EvolutionImporter *eimporter,
	      const char *filename,
	      const char *folderpath,
	      void *closure)
{
	MboxImporter *mbi;
	MailImporter *importer;
	int fd;

	g_warning ("%s", __FUNCTION__);
	mbi = (MboxImporter *) closure;
	importer = (MailImporter *) mbi;

	mbi->filename = g_strdup (filename);

	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("Cannot open file");
		return FALSE;
	}

	mbi->mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mbi->mp, TRUE);
	if (camel_mime_parser_init_with_fd (mbi->mp, fd) == -1) {
		g_warning ("Unable to process spool folder");
		goto fail;
	}

	importer->mstream = NULL;
	if (folderpath == NULL || *folderpath == '\0')
		importer->folder = mail_tool_get_local_inbox (NULL);
	else {
		char *parent;
		const char *name;
		BonoboListener *listener;

		/* Make a new directory */
		name = strrchr (folderpath, '/');
		if (name == NULL) {
			parent = g_strdup ("/");
			name = folderpath;
		} else {
			name += 1;
			parent = g_dirname (folderpath);
		}

		listener = bonobo_listener_new (NULL, NULL);
		gtk_signal_connect (GTK_OBJECT (listener), "event-notify",
				    GTK_SIGNAL_FUNC (folder_created_cb),
				    importer);

		mail_importer_create_folder (parent, name, NULL, listener);
		g_free (parent);
	}

	if (importer->folder == NULL){
		g_print ("Bad folder\n");
		goto fail;
	}

	camel_folder_freeze (importer->folder);
	importer->frozen = TRUE;

	g_warning ("Okay, so everything is now ready to import that mbox file!");
	return TRUE;

 fail:
	camel_object_unref (CAMEL_OBJECT (mbi->mp));
	mbi->mp = NULL;

	return FALSE;
}

static BonoboObject *
mbox_factory_fn (BonoboGenericFactory *_factory,
		 void *closure)
{
	EvolutionImporter *importer;
	MboxImporter *mbox;
	
	mbox = g_new0 (MboxImporter, 1);
	importer = evolution_importer_new (support_format_fn, load_file_fn,
					   process_item_fn, NULL, mbox);
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (importer_destroy_cb), mbox);

	g_warning ("Returning");
	return BONOBO_OBJECT (importer);
}

/* Entry point */
void
mail_importer_module_init (void)
{
	static gboolean initialised = FALSE;
	BonoboGenericFactory *factory;
	
	if (initialised == TRUE)
		return;

	factory = bonobo_generic_factory_new (MBOX_FACTORY_IID, 
					      mbox_factory_fn, NULL);

	if (factory == NULL)
		g_warning ("Could not initialise Outlook importer factory.");

	initialised = TRUE;
}

