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
		/* if it failed, need to say it failed ... */
		/* the create_folder callback needs to store the create result */
		/* here we need to pass FALSE for more items */
		printf("not ready\n");
		if (mbi->create_result == GNOME_Evolution_Storage_OK)
			GNOME_Evolution_ImporterListener_notifyResult (listener,
								       GNOME_Evolution_ImporterListener_NOT_READY,
								       TRUE, ev);
		else
			GNOME_Evolution_ImporterListener_notifyResult (listener,
								       GNOME_Evolution_ImporterListener_BAD_FILE,
								       FALSE, ev);
		return;
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

	printf("folder created cb, result = %d\n", result->result);
	((MboxImporter *)importer)->create_result = result->result;

	if (result->result != GNOME_Evolution_Storage_OK)
		return;

	fullpath = g_strconcat ("file://", result->path, NULL);

	ex = camel_exception_new ();
	importer->folder = mail_tool_uri_to_folder (fullpath, CAMEL_STORE_FOLDER_CREATE, ex);
	if (camel_exception_is_set (ex)) {
		g_warning ("Error opening %s", fullpath);
		camel_exception_free (ex);

		g_free (fullpath);
		((MboxImporter *)importer)->create_result = GNOME_Evolution_Storage_GENERIC_ERROR;

		return;
	}

	camel_folder_freeze (importer->folder);
	importer->frozen = TRUE;

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
	gboolean delayed = FALSE;
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
	if (folderpath == NULL || *folderpath == '\0') {
		importer->folder = mail_tool_get_local_inbox (NULL);
	} else {
		char *parent, *fullpath, *homedir;
		const char *name;
		BonoboListener *listener;
		CamelException *ex;
	
		homedir = g_strdup_printf("file://%s/evolution/local", g_get_home_dir());

		fullpath = e_path_to_physical (homedir, folderpath);
		ex = camel_exception_new ();
		importer->folder = mail_tool_uri_to_folder (fullpath, 0, ex);
		g_free (homedir);
	
		if (camel_exception_is_set (ex) || importer->folder == NULL) {
			/* Make a new directory */
			name = strrchr (folderpath, '/');
			if (name == NULL) {
				parent = g_strdup ("/");
				name = folderpath;
			} else {
				name += 1;
				parent = g_path_get_dirname (folderpath);
			}
			
			listener = bonobo_listener_new (NULL, NULL);
			g_signal_connect((listener), "event-notify",
					 G_CALLBACK (folder_created_cb),
					 importer);
			mbi->create_result = GNOME_Evolution_Storage_OK;
			mail_importer_create_folder (parent, name, NULL, listener);
			delayed = importer->folder == NULL;
			g_free (parent);
		}
		camel_exception_free (ex);
		g_free (fullpath);
	}

	if (importer->folder == NULL && delayed == FALSE){
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

