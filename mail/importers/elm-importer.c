/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* elm-importer.c
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
#include <gnome.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-exception.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include "mail-importer.h"
#include "mail-tools.h"

extern char *evolution_dir;

#define ELM_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Elm_Intelligent_Importer_Factory"
#define KEY "elm-mail-imported"

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
} ElmImporter;

typedef struct {
	char *parent;
	char *foldername;
	char *path;
} ElmFolder;

static gboolean
elm_is_mbox (const char *filename)
{
	char sig[5];
	int fd;
	
	fd = open (filename, O_RDONLY);
	if (read (fd, sig, 5) != 5) {
		close (fd);
		return FALSE;
	}

	close (fd);
	if (strncmp (sig, "From ", 5) != 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
elm_import_mbox (CamelFolder *folder,
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
elm_import_file (ElmImporter *importer,
		 const char *path,
		 const char *fullpath)
{
	char *protocol;
	CamelException *ex;
	CamelFolder *folder;

	g_warning ("Importing %s into %s", path, fullpath);
	protocol = g_strconcat ("file://", fullpath, NULL);
	ex = camel_exception_new ();
	folder = mail_tool_uri_to_folder (protocol, ex);
	g_free (protocol);

	if (camel_exception_is_set (ex)) {
		camel_exception_free (ex);
		return;
	}
	camel_exception_free (ex);

	if (folder == NULL) {
		return;
	}

	elm_import_mbox (folder, path);
}

static gboolean
is_kmail (const char *maildir)
{
	char *names[5] = 
	{
		"inbox",
		"outbox",
		"sent-mail",
		"trash",
		"drafts"
	};
	int i;

	for (i = 0; i < 5; i++) {
		char *file, *index, *tmp;
		
		file = g_concat_dir_and_file (maildir, names[i]);
		tmp = g_strdup_printf (".%s.index", names[i]);
		index = g_concat_dir_and_file (maildir, tmp);
		g_free (tmp);

		if (!g_file_exists (file) ||
		    !g_file_exists (index)) {
			g_free (index);
			g_free (file);
			return FALSE;
		}

		g_free (index);
		g_free (file);
	}

	return TRUE;
}

static gboolean
elm_can_import (EvolutionIntelligentImporter *ii,
		void *closure)
{
	ElmImporter *importer = closure;
	FILE *prefs_handle;
	char *key, *maildir;
	gboolean exists;

	/* Already imported */
	key = g_strdup_printf ("=%s/config/Mail=/importers/", evolution_dir);
	gnome_config_push_prefix (key);
	g_free (key);

	if (gnome_config_get_bool (KEY) == TRUE) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	/* Elm uses ~/Mail
	   Alas so does MH and KMail. */
	maildir = gnome_util_prepend_user_home ("Mail");
	exists = g_file_exists (maildir);

	if (exists) {
		char *mh, *mhdir;

		/* Check for some other files to work out what it is. */

		/* MH? */
		mh = g_concat_dir_and_file (maildir, "context");
		mhdir = g_concat_dir_and_file (maildir, "inbox");
		if (g_file_exists (mh) &&
		    g_file_test (mhdir, G_FILE_TEST_ISDIR)) {
			exists = FALSE; /* Probably MH */
		}
		
		g_free (mh);
		g_free (mhdir);
	}

	if (exists) {
		/* Check for KMail stuff */
		exists = !is_kmail (maildir);
	}

	g_free (maildir);

	return exists;
}

static void
scan_dir (ElmImporter *importer,
	  const char *dirname,
	  const char *orig_parent)
{
	DIR *maildir;
	struct stat buf;
	struct dirent *current;
	
	maildir = opendir (dirname);
	if (maildir == NULL) {
		g_warning ("Could not open %s\nopendir returned: %s",
			   dirname, g_strerror (errno));
		return;
	}
	
	current = readdir (maildir);
	while (current) {
		ElmFolder *pf;
		char *fullname;
		
		/* Ignore . and .. */
		if (current->d_name[0] == '.') {
			if (current->d_name[1] == '\0' ||
			    (current->d_name[1] == '.' && current->d_name[2] == '\0')) {
				current = readdir (maildir);
				continue;
			}
		}
		
		fullname = g_concat_dir_and_file (dirname, current->d_name);
		if (stat (fullname, &buf) == -1) {
			g_warning ("Could not stat %s\nstat returned: %s",
				   fullname, g_strerror (errno));
			current = readdir (maildir);
			g_free (fullname);
			continue;
		}
		
		if (S_ISREG (buf.st_mode)) {
			pf = g_new (ElmFolder, 1);
			pf->path = g_strdup (fullname);
			pf->parent = g_strdup (orig_parent);
			pf->foldername = g_strdup (current->d_name);
			importer->dir_list = g_list_append (importer->dir_list, pf);
		} else if (S_ISDIR (buf.st_mode)) {
			char *subdir;

			pf = g_new (ElmFolder, 1);
			pf->path = NULL;
			pf->parent = g_strdup (orig_parent);
			pf->foldername = g_strdup (current->d_name);
			importer->dir_list = g_list_append (importer->dir_list, pf);

			subdir = g_concat_dir_and_file (orig_parent, current->d_name);
			scan_dir (importer, fullname, subdir);
			g_free (subdir);
		}
		
		g_free (fullname);
		current = readdir (maildir);
	}
}

static void
folder_created_cb (BonoboListener *listener,
		   const char *event_name,
		   const BonoboArg *event_data,
		   CORBA_Environment *ev,
		   ElmImporter *importer)
{
	ElmFolder *folder;
	GList *l;
	GNOME_Evolution_Storage_FolderResult *result;
	char *fullpath;

	if (strcmp (event_name, "evolution-shell:folder_created") != 0) {
		return; /* Unknown event notification */
	}

	result = event_data->_value;
	fullpath = result->path;

	l = importer->dir_list;
	importer->dir_list = g_list_remove_link (importer->dir_list, l);
	folder = l->data;
	g_list_free_1 (l);

	/* We got the folder, so try to import the file into it. */
	if (folder->path != NULL && elm_is_mbox (folder->path)) {
		elm_import_file (importer, folder->path, fullpath);
	}

	g_free (folder->path);
	g_free (folder->parent);
	g_free (folder->foldername);
	g_free (folder);

	if (importer->dir_list) {
		/* Do the next in the list */
		folder = importer->dir_list->data;
		mail_importer_create_folder (folder->parent, folder->foldername,
					     NULL, importer->listener);
	}
}

static void
elm_create_structure (EvolutionIntelligentImporter *ii,
		      void *closure)
{
	ElmFolder *folder;
	ElmImporter *importer = closure;
	char *maildir, *key;

	maildir = gnome_util_prepend_user_home ("Mail");
	scan_dir (importer, maildir, "/");
	g_free (maildir);

	if (importer->dir_list == NULL)
		return;

	folder = importer->dir_list->data;
	mail_importer_create_folder (folder->parent, folder->foldername, NULL,
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
elm_factory_fn (BonoboGenericFactory *_factory,
		void *closure)
{
	EvolutionIntelligentImporter *importer;
	ElmImporter *elm;
	char *message = N_("Evolution has found Elm mail files.\n"
			   "Would you like to import them into Evolution?");

	elm = g_new0 (ElmImporter, 1);
	elm->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (elm->listener), "event_notify",
			    GTK_SIGNAL_FUNC (folder_created_cb), elm);

	importer = evolution_intelligent_importer_new (elm_can_import,
						       elm_create_structure,
						       _("Elm mail"),
						       _(message), elm);

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

	factory = bonobo_generic_factory_new (ELM_INTELLIGENT_IMPORTER_IID,
					      elm_factory_fn, NULL);
	if (factory == NULL)
		g_warning ("Could not initialise Elm Intelligent Mail Importer.");
	initialised = TRUE;
}
