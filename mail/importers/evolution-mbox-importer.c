/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* evolution-mbox-importer.c
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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>

#include <camel/camel-exception.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-mime-part.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mozilla-status-headers.h"

#include "mail/mail-importer.h"
#include "mail-tools.h"

#include "e-util/e-path.h"

/*  #define IMPORTER_DEBUG */
#ifdef IMPORTER_DEBUG
#define IN g_print ("=====> %s (%d)\n", G_GNUC_FUNCTION, __LINE__)
#define OUT g_print ("<==== %s (%d)\n", G_GNUC_FUNCTION, __LINE__)
#else
#define IN
#define OUT
#endif

#define MBOX_FACTORY_IID "OAFIID:GNOME_Evolution_Mail_Mbox_ImporterFactory"

typedef struct {
	MailImporter importer; /* Parent */

	char *filename;
	int num;
	GNOME_Evolution_Storage_Result create_result;

	CamelMimeParser *mp;
	gboolean is_folder;
} MboxImporter;

void mail_importer_module_init (void);

/* EvolutionImporter methods */


static CamelMessageInfo *
get_info_from_mozilla (const char *mozilla_status,
		       gboolean *deleted)
{
	unsigned int status;
	CamelMessageInfo *info;
	
	*deleted = FALSE;
	
	status = strtoul (mozilla_status, NULL, 16);
	if (status == 0) {
		return camel_message_info_new ();
	}
	
	if (status & MSG_FLAG_EXPUNGED) {
		*deleted = TRUE;
		
		return NULL;
	}
	
	info = camel_message_info_new ();
	
	if (status & MSG_FLAG_READ)
		info->flags |= CAMEL_MESSAGE_SEEN;
	
	if (status & MSG_FLAG_MARKED)
		info->flags |= CAMEL_MESSAGE_FLAGGED;
	
	if (status & MSG_FLAG_REPLIED)
		info->flags |= CAMEL_MESSAGE_ANSWERED;
	
	return info;
}

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
	const char *mozilla_status;

	if (importer->folder == NULL) {
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_BAD_FILE,
							       FALSE, ev);
	}

	if (mbi->is_folder == TRUE) {
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_OK,
							       FALSE, ev);
		return;
	}
		
	ex = camel_exception_new ();
	if (camel_mime_parser_step (mbi->mp, 0, 0) == HSCAN_FROM) {
		/* Import the next message */
		CamelMimeMessage *msg;
		CamelMessageInfo *info;
		gboolean deleted;
		
		IN;
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mbi->mp) == -1) {
			g_warning ("Failed message %d", mbi->num);
			camel_object_unref (CAMEL_OBJECT (msg));
			done = TRUE;
		} else {
			mozilla_status = camel_medium_get_header (CAMEL_MEDIUM (msg), "X-Mozilla-Status");
			if (mozilla_status != NULL) {
				info = get_info_from_mozilla (mozilla_status, &deleted);
			} else {
				deleted = FALSE;
				info = camel_message_info_new ();
			}
			
			if (deleted == FALSE) {
				/* write the mesg */
				camel_folder_append_message (importer->folder, msg, info, NULL, ex);
			}
			
			if (info)
				camel_message_info_free (info);
			
			camel_object_unref (msg);
			if (camel_exception_is_set (ex)) {
				g_warning ("Failed message %d", mbi->num);
				done = TRUE;
			}
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
	
	if (!done)
		camel_mime_parser_step (mbi->mp, 0, 0);
	
	camel_exception_free (ex);

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
importer_destroy_cb (void *data, GObject *object)
{
	MboxImporter *mbi = data;
	MailImporter *importer = data;

	if (importer->folder) {
		if (importer->frozen) {
			camel_folder_sync (importer->folder, FALSE, NULL);
			camel_folder_thaw (importer->folder);
		}

		camel_object_unref (importer->folder);
	}

	g_free (mbi->filename);
	if (mbi->mp)
		camel_object_unref (mbi->mp);

	g_free (mbi);
}

static gboolean
load_file_fn (EvolutionImporter *eimporter,
	      const char *filename,
	      const char *uri,
	      const char *folder_type,
	      void *closure)
{
	MboxImporter *mbi;
	MailImporter *importer;
	struct stat buf;
	int fd;

	mbi = (MboxImporter *) closure;
	importer = (MailImporter *) mbi;

	mbi->filename = g_strdup (filename);

	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("Cannot open file");
		return FALSE;
	}

	fstat (fd, &buf);
	if (S_ISREG (buf.st_mode)) {
		mbi->mp = camel_mime_parser_new ();
		camel_mime_parser_scan_from (mbi->mp, TRUE);
		if (camel_mime_parser_init_with_fd (mbi->mp, fd) == -1) {
			g_warning ("Unable to process spool folder");
			goto fail;
		}
		mbi->is_folder = FALSE;
	} else {
		mbi->is_folder = TRUE;
	}

	importer->mstream = NULL;
	if (uri == NULL || *uri == '\0')
		importer->folder = mail_tool_get_local_inbox (NULL);
	else
		importer->folder = mail_tool_uri_to_folder(uri, 0, NULL);

	if (importer->folder == NULL) {
		g_warning ("Bad folder\n");
		goto fail;
	}

	if (importer->folder != NULL) { 
		camel_folder_freeze (importer->folder);
		importer->frozen = TRUE;
	}

	return TRUE;

 fail:
	camel_object_unref (mbi->mp);
	mbi->mp = NULL;

	return FALSE;
}

static BonoboObject *
mbox_factory_fn (BonoboGenericFactory *_factory,
		 const char *cid,
		 void *closure)
{
	EvolutionImporter *importer;
	MboxImporter *mbox;
	
	mbox = g_new0 (MboxImporter, 1);
	importer = evolution_importer_new (support_format_fn, load_file_fn,
					   process_item_fn, NULL, mbox);
	g_object_weak_ref(G_OBJECT(importer), importer_destroy_cb, mbox);
	
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
		g_warning ("Could not initialise mbox importer factory.");

	initialised = TRUE;
}

