/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-importer.c
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

#include <bonobo.h>
#include "mail-importer.h"
#include "mail-local.h"

#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-exception.h>

#include "evolution-outlook-importer.h"
#include "evolution-mbox-importer.h"

static gboolean factory_initialised = FALSE;

extern char *evolution_dir;
/**
 * mail_importer_add_line:
 * importer: A MailImporter structure.
 * str: Next line of the mbox.
 * finished: TRUE if @str is the last line of the message.
 *
 * Adds lines to the message until it is finished, and then adds
 * the complete message to the folder.
 */
void
mail_importer_add_line (MailImporter *importer,
			const char *str,
			gboolean finished)
{
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	CamelException *ex;
	
	if (importer->mstream == NULL) {
		importer->mstream = camel_stream_mem_new ();
	}

	camel_stream_write (CAMEL_STREAM (importer->mstream), str, 
			    strlen (str));
	
	if (finished == FALSE)
		return;

	camel_stream_reset (CAMEL_STREAM (importer->mstream));
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_SEEN;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  CAMEL_STREAM (importer->mstream));
	
	camel_object_unref (CAMEL_OBJECT (importer->mstream));
	importer->mstream = NULL;

	ex = camel_exception_new ();
	camel_folder_append_message (importer->folder, msg, info, ex);
	camel_object_unref (CAMEL_OBJECT (msg));

	camel_exception_free (ex);
	g_free (info);
}

/**
 * mail_importer_get_folder:
 * @name: The folder name.
 * *opt_ex: A #CamelException, or NULL if you don't care about errors.
 *
 * Gets the local folder called @name.
 *
 * Returns: A CamelFolder (which needs to be unrefed when you are done with it)
 * on success, or NULL on fail. A more detailed error is given in @opt_ex, if
 * @opt_ex is not NULL.
 */
CamelFolder *
mail_importer_get_folder (const char *name,
			  CamelException *opt_ex)
{
	CamelFolder *folder;
	CamelException *real_ex;
	char *path, *tmp;

	if (opt_ex != NULL)
		real_ex = opt_ex;
	else
		real_ex = camel_exception_new ();

	g_print ("Evolution_dir: %s\n", evolution_dir);
	tmp = g_concat_dir_and_file (evolution_dir, "local");
	path = g_concat_dir_and_file (tmp, name);
	g_free (tmp);

	folder = mail_local_lookup_folder (path + 1, real_ex);
	if (opt_ex == NULL)
		camel_exception_free (real_ex);

	return folder;
}
	
/**
 * mail_importer_init:
 *
 * Initialises all the importers
 */
void
mail_importer_init (void)
{
	BonoboGenericFactory *factory;
	if (factory_initialised == TRUE)
		return;

	/* FIXME: Need plugins */
	factory = bonobo_generic_factory_new (OUTLOOK_FACTORY_IID,
					      outlook_factory_fn, 
					      NULL);
	if (factory == NULL) {
		g_error ("Unable to create outlook factory.");
	}

	factory = bonobo_generic_factory_new (MBOX_FACTORY_IID,
					      mbox_factory_fn, NULL);
	if (factory == NULL) {
		g_error ("Unable to create mbox factory.");
	}

	factory_initialised = TRUE;
}

