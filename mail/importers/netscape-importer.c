/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* netscape-importer.c
 * 
 * Authors: 
 *    Iain Holmes  <iain@ximian.com>
 *
 * Copyright 2001 Ximian, Inc. (http://www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <glib.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-exception.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <evolution-storage.h>

#include "mail-importer.h"
#include "mail-tools.h"

static char *nsmail_dir = NULL;

extern char *evolution_dir;

#define NETSCAPE_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Netscape_Intelligent_Importer_Factory"
#define MBOX_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Mbox_ImporterFactory"
#define KEY "netscape-mail-imported"

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

typedef struct {
	MailImporter importer;
	GList *dir_list;

	int num;
	CamelMimeParser *mp;
	BonoboListener *listener;
} NetscapeImporter;

void mail_importer_module_init (void);
void g_module_unload (void);

static gboolean
netscape_import_mbox (CamelFolder *folder,
		      const char *filename)
{
	gboolean done = FALSE;
	CamelException *ex;
	CamelMimeParser *mp;
	int fd, n = 0;

	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("Cannot open %s", filename);
		return FALSE;
	}

	camel_object_ref (CAMEL_OBJECT (folder));
	camel_folder_freeze (folder);

	ex = camel_exception_new ();
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
		g_warning ("Unable to process file %s", filename);
		camel_object_unref (CAMEL_OBJECT (mp));
		camel_folder_thaw (folder);
		camel_object_unref (CAMEL_OBJECT (folder));
		return FALSE;
	}

	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		/* Import the next message */
		CamelMimeMessage *msg;
		CamelMessageInfo *info;

		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg),
							   mp) == -1) {
			g_warning ("Failed message %d", n);
			camel_object_unref (CAMEL_OBJECT (msg));
			done = TRUE;
		}

		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (folder, msg, info, ex);
		g_free (info);
		camel_object_unref (CAMEL_OBJECT (msg));
		if (camel_exception_is_set (ex)) {
			g_warning ("Failed message %d", n);
			done = TRUE;
		}

		if (!done) {
			n++;
			camel_mime_parser_step (mp, 0, 0);
		}
	}

	camel_folder_sync (folder, FALSE, ex);
	camel_folder_thaw (folder);
	camel_object_unref (CAMEL_OBJECT (folder));
	done = TRUE;

	camel_exception_free (ex);
	return done;
}

static void
netscape_clean_up (void)
{
	if (nsmail_dir == NULL)
		return;

	g_free (nsmail_dir);
	nsmail_dir = NULL;
}

static gboolean
netscape_can_import (EvolutionIntelligentImporter *ii,
		     void *closure)
{
	char *nsprefs;
	FILE *prefs_handle;
	char *key;

	/* Already imported */
	key = g_strdup_printf ("=%s/config/Mail=/importers/", evolution_dir);
	gnome_config_push_prefix (key);
	g_free (key);

	if (gnome_config_get_bool (KEY) == TRUE) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	nsprefs = gnome_util_prepend_user_home (".netscape/preferences.js");
	prefs_handle = fopen (nsprefs, "r");
	g_free (nsprefs);

	if (prefs_handle == NULL) {
		d(g_warning ("No .netscape/preferences.js"));
		return FALSE;
	}

	/* Find the user mail dir */
	while (1) {
		char line[4096];

		fgets (line, 4096, prefs_handle);
		if (line == NULL) {
			g_warning ("No mail.directory entry");
			fclose (prefs_handle);
			return FALSE;
		}

		if (strstr (line, "mail.directory") != NULL) {
			char *sep, *start, *end;
			/* Found the line */
			
			sep = strchr (line, ',');
			if (sep == NULL) {
				g_warning ("Bad line %s", line);
				fclose (prefs_handle);
				return FALSE;
			}

			start = strchr (sep, '\"') + 1;
			if (start == NULL) {
				g_warning ("Bad line %s", line);
				fclose (prefs_handle);
				return FALSE;
			}

			end = strrchr (sep, '\"');
			if (end == NULL) {
				g_warning ("Bad line %s", line);
				fclose (prefs_handle);
				return FALSE;
			}
			
			nsmail_dir = g_strndup (start, end - start);
			d(g_warning ("Got nsmail_dir: %s", nsmail_dir));
			fclose (prefs_handle);
			return TRUE;
		}
	}
}

static void
netscape_import_file (NetscapeImporter *importer,
		      const char *path,
		      const char *fullpath)
{
	char *protocol;
	CamelException *ex;
	CamelFolder *folder;

	/* Do import */
	d(g_warning ("Importing %s as %s\n", filename, fullpath));

	ex = camel_exception_new ();
	protocol = g_strconcat ("file://", fullpath, NULL);
	folder = mail_tool_uri_to_folder (protocol, ex);
	if (camel_exception_is_set (ex)) {
		g_free (protocol);
		camel_exception_free (ex);
		return;
	}

	g_free (protocol);

	if (folder == NULL) {
		g_warning ("Folder for %s is NULL", fullpath);
		camel_exception_free (ex);
		return;
	}

	camel_exception_free (ex);

	netscape_import_mbox (folder, path);
}

typedef struct {
	NetscapeImporter *importer;
	char *parent;
	char *path;
	char *foldername;
} NetscapeCreateDirectoryData;

static void
netscape_dir_created (BonoboListener *listener,
		      const char *event_name,
		      const BonoboArg *event_data,
		      CORBA_Environment *ev,
		      NetscapeImporter *importer)
{
	EvolutionStorageResult storage_result;
	NetscapeCreateDirectoryData *data;
	GList *l;
	GNOME_Evolution_Storage_FolderResult *result;
	char *fullpath;
	
	if (strcmp (event_name, "evolution-shell:folder_created") != 0) {
		return; /* Unknown event notification */
	}

	result = event_data->_value;
	storage_result = result->result;
	fullpath = result->path;

	g_warning ("path: %s\tresult: %d", fullpath, storage_result);

	l = importer->dir_list;
	importer->dir_list = g_list_remove_link (importer->dir_list, l);
	data = l->data;
	g_list_free_1 (l);

	/* Import the file */
	/* We got the folder, so try to import the file into it. */
	if (fullpath != NULL || *fullpath != '\0')
		netscape_import_file (data->importer, data->path, fullpath);

	g_free (data->parent);
	g_free (data->path);
	g_free (data->foldername);
	g_free (data);

	if (importer->dir_list) {
		/* Do the next in the list */
		data = importer->dir_list->data;
		mail_importer_create_folder (data->parent, data->foldername, 
					     NULL, importer->listener);
	}
}

static char *
maybe_replace_name (const char *original_name)
{
	if (strcmp (original_name, "Trash") == 0) {
		return g_strdup ("Netscape-Trash"); /* Trash is an invalid name */
	} else if (strcmp (original_name, "Unsent Messages") == 0) {
		return g_strdup ("Outbox");
	} 

	return g_strdup (original_name);
}

/* This function basically flattens the tree structure.
   It makes a list of all the directories that are to be imported. */
static void
scan_dir (NetscapeImporter *importer,
	  const char *orig_parent,
	  const char *dirname)
{
	DIR *nsmail;
	struct stat buf;
	struct dirent *current;

	nsmail = opendir (dirname);
	if (nsmail == NULL) {
		d(g_warning ("Could not open %s\nopendir returned: %s", 
			     dirname, g_strerror (errno)));
		return;
	}

	current = readdir (nsmail);
	while (current) {
		char *fullname, *foldername;

		/* Ignore things which start with . 
		   which should be ., .., and the summaries. */
		if (current->d_name[0] =='.') {
			current = readdir (nsmail);
			continue;
		}

		if (*orig_parent == '/') {
			foldername = maybe_replace_name (current->d_name);
		} else {
			foldername = g_strdup (current->d_name);
		}

		fullname = g_concat_dir_and_file (dirname, current->d_name);
		if (stat (fullname, &buf) == -1) {
			d(g_warning ("Could not stat %s\nstat returned:%s",
				     fullname, g_strerror (errno)));
			current = readdir (nsmail);
			g_free (fullname);
			continue;
		}

		if (S_ISREG (buf.st_mode)) {
			char *sbd, *parent;
			NetscapeCreateDirectoryData *data;

			d(g_print ("File: %s\n", fullname));

			data = g_new0 (NetscapeCreateDirectoryData, 1);
			data->importer = importer;
			data->parent = g_strdup (orig_parent);
			data->path = g_strdup (fullname);
			data->foldername = g_strdup (foldername);

			importer->dir_list = g_list_append (importer->dir_list,
							    data);

	
			parent = g_concat_dir_and_file (orig_parent, 
							data->foldername);
			
			/* Check if a .sbd folder exists */
			sbd = g_strconcat (fullname, ".sbd", NULL);
			if (g_file_exists (sbd)) {
				scan_dir (importer, parent, sbd);
			}
			
			g_free (parent);
			g_free (sbd);
		} 
		
		g_free (fullname);
		g_free (foldername);
		current = readdir (nsmail);
	}
}

static void
netscape_create_structure (EvolutionIntelligentImporter *ii,
			   void *closure)
{
	NetscapeImporter *importer = closure;
	NetscapeCreateDirectoryData *data;
	char *key;

	g_return_if_fail (nsmail_dir != NULL);

	scan_dir (importer, "/", nsmail_dir);

	importer->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (importer->listener), "event_notify",
			    GTK_SIGNAL_FUNC (netscape_dir_created), importer);
	data = importer->dir_list->data;
	mail_importer_create_folder (data->parent, data->foldername, NULL, 
				     importer->listener);

	key = g_strdup_printf ("=%s/config/Mail=/importers/", evolution_dir);
	gnome_config_push_prefix (key);
	g_free (key);

	gnome_config_set_bool (KEY, TRUE);
	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();
}

static BonoboObject *
netscape_factory_fn (BonoboGenericFactory *_factory,
		 void *closure)
{
	EvolutionIntelligentImporter *importer;
	NetscapeImporter *netscape;
	char *message = N_("Evolution has found Netscape mail files.\n"
			   "Would you like them to be imported into Evolution?");

	netscape = g_new0 (NetscapeImporter, 1);
	importer = evolution_intelligent_importer_new (netscape_can_import,
						       netscape_create_structure,
						       "Netscape mail", 
						       message, netscape);
	return BONOBO_OBJECT (importer);
}

/* Entry Point */
void
mail_importer_module_init (void)
{
	static gboolean initialised = FALSE;
	BonoboGenericFactory *factory;

	if (initialised == TRUE)
		return;

	factory = bonobo_generic_factory_new (NETSCAPE_INTELLIGENT_IMPORTER_IID,
					      netscape_factory_fn, NULL);
	if (factory == NULL)
		g_warning ("Could not initialise Netscape Intelligent Mail Importer.");
	initialised = TRUE;
}

/* GModule g_module_unload routine */
void 
g_module_unload (void)
{
	netscape_clean_up ();
}
