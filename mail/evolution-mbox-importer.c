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

#include "evolution-mbox-importer.h"

#include <stdio.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mail-importer.h"

#include <camel/camel-exception.h>

typedef struct {
	MailImporter importer; /* Parent */

	char *filename;
	FILE *handle;
	off_t size;
} MboxImporter;


/* EvolutionImporter methods */

static void
process_item_fn (EvolutionImporter *eimporter,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	MboxImporter *mbi = (MboxImporter *) closure;
	MailImporter *importer = (MailImporter *) mbi;
	CamelException *ex;
	static char *line = NULL;

	if (line == NULL)
		line = g_new0 (char, 4096);

	if (line != NULL) { 
		/* We had a From line the last time
		   so just add it and start again */
		mail_importer_add_line (importer, line, FALSE);
	}

	if (fgets (line, 4096, mbi->handle) == NULL) {
		if (*line != '\0')
			mail_importer_add_line (importer, line, TRUE); 
		/* Must be the end */

		g_free (line);
		line = NULL;
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_OK,
							       FALSE, ev);
		
		ex = camel_exception_new ();
		camel_folder_thaw (importer->folder);
		importer->frozen = FALSE;
		camel_folder_sync (importer->folder, FALSE, ex);
		camel_exception_free (ex);
		fclose (mbi->handle);
		mbi->handle = NULL;
		return;
	}

	/* It's the From line, so add it and get a new line. */
	if (strncmp (line, "From ", 5) == 0) {
		mail_importer_add_line (importer, line, FALSE);
		
		if (fgets (line, 4096, mbi->handle) == NULL) {
			if (*line != '\0')
				mail_importer_add_line (importer, line, TRUE); 
			/* Must be the end */
			
			g_free (line);
			line = NULL;
			GNOME_Evolution_ImporterListener_notifyResult (listener,
								       GNOME_Evolution_ImporterListener_OK,
								       FALSE, ev);
			
			ex = camel_exception_new ();
			camel_folder_thaw (importer->folder);
			importer->frozen = FALSE;
			camel_folder_sync (importer->folder, FALSE, ex);
			camel_exception_free (ex);
			fclose (mbi->handle);
			mbi->handle = NULL;
			return;
		}
	}
	
	while (strncmp (line, "From ", 5) != 0) {
		mail_importer_add_line (importer, line, FALSE);

		if (fgets (line, 4096, mbi->handle) == NULL) {
			if (*line != '\0')
				mail_importer_add_line (importer, line, TRUE);

			g_free (line);
			line = NULL;
			GNOME_Evolution_ImporterListener_notifyResult (listener,
								       GNOME_Evolution_ImporterListener_OK,
								       FALSE, ev);
			ex = camel_exception_new ();
			camel_folder_thaw (importer->folder);
			importer->frozen = FALSE;
			camel_folder_sync (importer->folder, FALSE, ex);
			camel_exception_free (ex);
			fclose (mbi->handle);
			mbi->handle = NULL;
			return;
		}
	}	
	
	mail_importer_add_line (importer, "\0", TRUE);
	GNOME_Evolution_ImporterListener_notifyResult (listener, 
						       GNOME_Evolution_ImporterListener_OK,
						       TRUE, ev);
		
	return;
}

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	FILE *handle;
	char signature[5];
	
	handle = fopen (filename, "rb");
	if (handle == NULL)
		return FALSE; /* Can't open file: Can't support it :) */
	
	/* SIGNATURE */
	fread (&signature, 5, 1, handle); 
	if (strncmp (signature, "From ", 5) == 0) {
		fclose (handle);
		return TRUE;
	}

	fclose (handle);
	return FALSE; 
}

static void
importer_destroy_cb (GtkObject *object,
		     MboxImporter *mbi)
{
	MailImporter *importer;

	importer = (MailImporter *) mbi;
	if (importer->frozen) 
		camel_folder_thaw (importer->folder);

	if (importer->folder)
		camel_object_unref (CAMEL_OBJECT (importer->folder));

	g_free (mbi->filename);
	if (mbi->handle)
		fclose (mbi->handle);

	g_free (mbi);
}

static gboolean
load_file_fn (EvolutionImporter *eimporter,
	      const char *filename,
	      void *closure)
{
	MboxImporter *mbi;
	MailImporter *importer;
	struct stat buf;

	mbi = (MboxImporter *) closure;
	importer = (MailImporter *) mbi;

	mbi->filename = g_strdup (filename);

	mbi->handle = fopen (filename, "rb");
	if (mbi->handle == NULL) {
		g_warning ("Cannot open file");
		return FALSE;
	}

	/* Get size of file */
	if (stat (filename, &buf) == -1) {
		g_warning ("Cannot stat file");
		return FALSE;
	}
	
	mbi->size = buf.st_size;

	importer->mstream = NULL;
	importer->folder = mail_importer_get_folder ("Inbox", NULL);

	if (importer->folder == NULL){
		g_print ("Bad folder\n");
		return FALSE;
	}

	camel_folder_freeze (importer->folder);
	importer->frozen = TRUE;
	return TRUE;
}

BonoboObject *
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
	
	return BONOBO_OBJECT (importer);
}

