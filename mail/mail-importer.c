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

#include <dirent.h>
#include <gmodule.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <evolution-storage.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-exception.h>

#include "mail-importer.h"
#include "mail-local.h"
#include "mail.h"


static GList *importer_modules = NULL;

extern char *evolution_dir;

static GNOME_Evolution_LocalStorage local_storage = NULL;

/* Prototype */

void mail_importer_uninit (void);

/**
 * mail_importer_create_folder:
 * parent_path: The path of the parent folder.
 * name: The name of the folder to be created.
 * description: A description of the folder.
 * listener: A BonoboListener for notification.
 *
 * Attempts to create the folder @parent_path/@name. When the folder has been
 * created, or there is an error, the "evolution-shell:folder-created" event is
 * emitted on @listener. The BonoboArg that is sent to @listener is a 
 * GNOME_Evolution_Storage_FolderResult which has two elements: result and path.
 * Result contains the error code, or success, and path contains the complete
 * physical path to the newly created folder.
 */
void
mail_importer_create_folder (const char *parent_path,
			     const char *name,
			     const char *description,
			     const BonoboListener *listener)
{
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	char *path, *physical;
	char *real_description;

	g_return_if_fail (local_storage != NULL);
	g_return_if_fail (listener != NULL);
	g_return_if_fail (BONOBO_IS_LISTENER (listener));

	path = g_concat_dir_and_file (parent_path, name);
	physical = g_strdup_printf ("file://%s/local/%s", evolution_dir,
				    parent_path);

	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));
	/* Darn CORBA wanting non-NULL values for strings */
	real_description = CORBA_string_dup (description ? description : "");
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncCreateFolder (local_storage, 
						   path, "mail", 
						   real_description, physical,
						   corba_listener, &ev);
	CORBA_exception_free (&ev);
	g_free (path);
	g_free (physical);
}

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
		importer->mstream = CAMEL_STREAM_MEM (camel_stream_mem_new ());
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

/* module management */
static GList *
get_importer_list (void)
{
	DIR *dir;
	struct dirent *d;
	GList *importers_ret = NULL;

	dir = opendir (MAIL_IMPORTERSDIR);
	if (!dir) {
		g_warning ("No importers dir: %s", MAIL_IMPORTERSDIR);
		return NULL;
	}

	while ((d = readdir (dir))) {
		char *path, *ext;

		ext = strchr (d->d_name, '.');
		if (!ext || strcmp (ext, ".so") != 0)
			continue;

		path = g_concat_dir_and_file (MAIL_IMPORTERSDIR, d->d_name);
		importers_ret = g_list_prepend (importers_ret, path);
	}

	closedir (dir);
	return importers_ret;
}

static void
free_importer_list (GList *list)
{
	for (; list; list = list->next) {
		g_free (list->data);
	}

	g_list_free (list);
}

/**
 * mail_importer_init:
 *
 * Initialises all the importers
 */
void
mail_importer_init (EvolutionShellClient *client)
{
	GList *importers, *l;

	if (importer_modules != NULL) {
		return;
	}

	local_storage = evolution_shell_client_get_local_storage (client);

	if (!g_module_supported ()) {
		g_warning ("Could not initialise the importers as module loading"
			   " is not supported on this system");
		return;
	}

	importers = get_importer_list ();
	if (importers == NULL)
		return;

	for (l = importers; l; l = l->next) {
		GModule *module;

		module = g_module_open (l->data, 0);
		if (!module) {
			g_warning ("Could not load: %s: %s", (char *) l->data,
				   g_module_error ());
		} else {
			void *(*mail_importer_module_init) ();

			if (!g_module_symbol (module, "mail_importer_module_init",
					      (gpointer *)&mail_importer_module_init)) {
				g_warning ("Could not load %s: No initialisation",
					   (char *) l->data);
				g_module_close (module);
			}

			mail_importer_module_init ();
			importer_modules = g_list_prepend (importer_modules, module);
		}
	}

	free_importer_list (importers);
}

/**
 * mail_importer_uninit:
 *
 * Unloads all the modules.
 */
void
mail_importer_uninit (void)
{
	CORBA_Environment ev;
	GList *l;

	for (l = importer_modules; l; l = l->next) {
		g_module_close (l->data);
	}

	g_list_free (importer_modules);
	importer_modules = NULL;
	
	CORBA_exception_init (&ev);
	bonobo_object_release_unref (local_storage, &ev);
	local_storage = NULL;
	CORBA_exception_free (&ev);
}

