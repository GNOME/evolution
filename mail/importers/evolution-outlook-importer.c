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

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>

#include <stdio.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include <camel/camel-exception.h>

#include "e-util/e-memory.h"

#include "mail-importer.h"
#include "mail-tools.h"


#define OUTLOOK_FACTORY_IID "OAFIID:GNOME_Evolution_Mail_Outlook_ImporterFactory"

extern char *evolution_dir;
typedef struct {
	MailImporter importer;

	char *filename;
	gboolean oe4; /* Is file OE4 or not? */
	FILE *handle;
	long pos;
	off_t size;

	gboolean busy;
} OutlookImporter;

struct oe_msg_segmentheader {
	int self;
	int increase;
	int include;
	int next;
	int usenet;
};

typedef struct oe_msg_segmentheader oe_msg_segmentheader;

/* Prototype */

void mail_importer_module_init (void);


/* EvolutionImporter methods */

/* Based on code from liboe 0.92 (STABLE)
   Copyright (C) 2000 Stephan B. Nedregård (stephan@micropop.com)
   Modified 2001 Iain Holmes  <iain@ximian.com>
   Copyright (C) 2001 Ximian, Inc. */

static void
process_item_fn (EvolutionImporter *eimporter,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	OutlookImporter *oli = (OutlookImporter *) closure;
	MailImporter *importer = (MailImporter *) oli;
	oe_msg_segmentheader *header;
	gboolean more = TRUE;
	char *cb, *sfull, *s;
	long end_pos = 0;
	int i;

	if (oli->busy == TRUE) {
		GNOME_Evolution_ImporterListener_notifyResult (listener, 
							       GNOME_Evolution_ImporterListener_BUSY,
							       more, ev);
		return;
	}

	oli->busy = TRUE;
	header = g_new (oe_msg_segmentheader, 1);
	fread (header, 16, 1, oli->handle);

	/* Write a From line */
	mail_importer_add_line (importer, 
				"From evolution-outlook-importer", FALSE);
	end_pos = oli->pos + header->include;
	if (end_pos >= oli->size) {
		end_pos = oli->size;
		more = FALSE;
	}

	oli->pos += 4;

	cb = g_new (char, 4);
	sfull = g_new (char, 65536);
	s = sfull;

	while (oli->pos < end_pos) {
		fread (cb, 1, 4, oli->handle);
		for (i = 0; i < 4; i++, oli->pos++) {
			if (*(cb + i ) != 0x0d) {
				*s++ = *(cb + i);

				if (*(cb + i) == 0x0a) {
					*s = '\0';
					mail_importer_add_line (importer, 
								sfull, FALSE);
					s = sfull;
				}
			}
		}
	}

	if (s != sfull) {
		*s = '\0';
		mail_importer_add_line (importer, sfull, FALSE);
		s = sfull;
	}

	mail_importer_add_line (importer, "\n", TRUE);

	oli->pos = end_pos;
	fseek (oli->handle, oli->pos, SEEK_SET);

	g_free (header);
	g_free (sfull);
	g_free (cb);

	GNOME_Evolution_ImporterListener_notifyResult (listener, 
						       GNOME_Evolution_ImporterListener_OK,
						       more, ev);
	if (more == FALSE) {
		CamelException *ex;

		ex = camel_exception_new ();
		camel_folder_thaw (importer->folder);
		camel_folder_sync (importer->folder, FALSE, ex);
		camel_exception_free (ex);
		fclose (oli->handle);
		oli->handle = NULL;
	}

	oli->busy = FALSE;
	return;
}


/* EvolutionImporterFactory methods */

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	FILE *handle;
	int signature[4];

	/* Outlook Express sniffer.
	   Taken from liboe 0.92 (STABLE)
	   Copyright (C) 2000 Stephan B. Nedregård (stephan@micropop.com) */

	handle = fopen (filename, "rb");
	if (handle == NULL)
		return FALSE; /* Can't open file: Can't support it :) */

	  /* SIGNATURE */
	fread (&signature, 16, 1, handle); 
	if ((signature[0]!=0xFE12ADCF) || /* OE 5 & OE 5 BETA SIGNATURE */
	    (signature[1]!=0x6F74FDC5) ||
	    (signature[2]!=0x11D1E366) ||
	    (signature[3]!=0xC0004E9A)) {
		if ((signature[0]==0x36464D4A) &&
		    (signature[1]==0x00010003)) /* OE4 SIGNATURE */ {
			fclose (handle);
			return TRUE; /* OE 4 */
		}
		fclose (handle);
		return FALSE; /* Not Outlook 4 or 5 */
	}

	fclose (handle);
	return FALSE; /* Can't handle OE 5 yet */
}

static void
importer_destroy_cb (void *data, GObject *object)
{
	OutlookImporter *oli = data;
	MailImporter *importer = data;

	if (importer->folder)
		camel_object_unref (importer->folder);

	g_free (oli->filename);
	if (oli->handle)
		fclose (oli->handle);

	g_free (oli);
}

static gboolean
load_file_fn (EvolutionImporter *eimporter,
	      const char *filename,
	      const char *uri,
	      const char *folder_type,
	      void *closure)
{
	OutlookImporter *oli;
	MailImporter *importer;
	struct stat buf;
	long pos = 0x54;

	oli = (OutlookImporter *) closure;
	importer = (MailImporter *) oli;

	oli->filename = g_strdup (filename);
	/* Will return TRUE if oe4 format */
	oli->oe4 = support_format_fn (NULL, filename, NULL);
	if (oli->oe4 == FALSE) {
		g_warning ("Not OE4 format");
		return FALSE;
	}

	oli->handle = fopen (filename, "rb");
	if (oli->handle == NULL) {
		g_warning ("Cannot open the file");
		return FALSE;
	}

	/* Get size of file */
	if (stat (filename, &buf) == -1) {
		g_warning ("Cannot stat file");
		return FALSE;
	}
	
	oli->size = buf.st_size;

	/* Set the fposition to the begining */
	fseek (oli->handle, pos, SEEK_SET);
	oli->pos = pos;

	importer->mstream = NULL;

	if (uri == NULL || *uri == 0)
		importer->folder = mail_tool_get_local_inbox (NULL);
	else
		importer->folder = mail_tool_uri_to_folder (uri, 0, NULL);

	if (importer->folder == NULL){
		g_warning ("Bad folder");
		return FALSE;
	}

	camel_folder_freeze (importer->folder);
	oli->busy = FALSE;
	return TRUE;
}

static BonoboObject *
outlook_factory_fn (BonoboGenericFactory *_factory,
		    const char *cid,
		    void *closure)
{
	EvolutionImporter *importer;
	OutlookImporter *oli;

	oli = g_new0 (OutlookImporter, 1);

	importer = evolution_importer_new (support_format_fn, load_file_fn, 
					   process_item_fn, NULL, oli);
	g_object_weak_ref((GObject *)importer, importer_destroy_cb, oli);

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

	factory = bonobo_generic_factory_new (OUTLOOK_FACTORY_IID, 
					      outlook_factory_fn, NULL);

	if (factory == NULL)
		g_warning ("Could not initialise Outlook importer factory.");

	initialised = TRUE;
}


