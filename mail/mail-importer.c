/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-importer.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
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

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <gmodule.h>
#include <libgnome/gnome-util.h>
#include <evolution-storage.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-exception.h>
#include <e-util/e-path.h>

#include "mail-importer.h"
#include "mail-local.h"
#include "mail.h"

static GList *importer_modules = NULL;
extern char *evolution_dir;
static GNOME_Evolution_Storage local_storage = NULL;

void mail_importer_uninit (void);

struct _create_data {
	GNOME_Evolution_Storage_Result create_result;
	int create_done:1;
};

static void
folder_created_cb(BonoboListener *listener, const char *event_name, const BonoboArg *event_data,
		  CORBA_Environment *ev, struct _create_data *data)
{
	GNOME_Evolution_Storage_FolderResult *result;

	data->create_done = TRUE;

	if (strcmp (event_name, "evolution-shell:folder_created") != 0) {
		return; /* Unknown event */
	}

	result = event_data->_value;
	data->create_result = result->result;
}

/**
 * mail_importer_make_local_folder:
 * @folderpath: 
 * 
 * Check a local folder exists at path @folderpath, and if not, create it.
 * 
 * Return value: The physical uri of the folder, or NULL if the folder did
 * not exist and could not be created.
 **/
char *
mail_importer_make_local_folder(const char *folderpath)
{
	CORBA_Environment ev;
	char *uri = NULL, *tmp;
	GNOME_Evolution_Folder *fi;
	BonoboListener *listener;

	CORBA_exception_init (&ev);

	/* first, check, does this folder exist, if so, use the right path */
	fi = GNOME_Evolution_Storage_getFolderAtPath(local_storage, folderpath, &ev);
	if (fi) {
		printf("folder %s exists @ %s\n", folderpath, fi->physicalUri);
		uri = g_strdup(fi->physicalUri);
		CORBA_free(fi);
	} else {
		struct _create_data data = { GNOME_Evolution_Storage_GENERIC_ERROR, FALSE };

		tmp = g_strdup_printf("file://%s/local", evolution_dir);
		uri = e_path_to_physical(tmp, folderpath);
		g_free(tmp);
		tmp = strrchr(uri, '/');
		tmp[0] = 0;

		printf("Creating folder %s, parent %s\n", folderpath, uri);

		listener = bonobo_listener_new (NULL, NULL);
		g_signal_connect(listener, "event-notify", G_CALLBACK (folder_created_cb), &data);

		GNOME_Evolution_Storage_asyncCreateFolder(local_storage, folderpath, "mail",  "", uri,
							  bonobo_object_corba_objref((BonoboObject *)listener), &ev);

		while (!data.create_done)
			g_main_context_iteration(NULL, TRUE);

		bonobo_object_unref((BonoboObject *)listener);

		if (data.create_result != GNOME_Evolution_Storage_OK) {
			g_free(uri);
			uri = NULL;
		} else {
			*tmp = '/';
		}
	}

	CORBA_exception_free (&ev);

	return uri;
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
	
	if (importer->mstream == NULL)
		importer->mstream = CAMEL_STREAM_MEM (camel_stream_mem_new ());

	camel_stream_write (CAMEL_STREAM (importer->mstream), str,  strlen (str));
	
	if (finished == FALSE)
		return;

	camel_stream_reset (CAMEL_STREAM (importer->mstream));
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_SEEN;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  CAMEL_STREAM (importer->mstream));
	
	camel_object_unref (importer->mstream);
	importer->mstream = NULL;

	ex = camel_exception_new ();
	camel_folder_append_message (importer->folder, msg, info, NULL, ex);
	camel_object_unref (msg);

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

		path = g_build_filename (MAIL_IMPORTERSDIR, d->d_name, NULL);
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

