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
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/evolution-importer-client.h>
#include <importer/GNOME_Evolution_Importer.h>

#define ELM_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Elm_Intelligent_Importer_Factory"
#define MBOX_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Mbox_Importer"
#define KEY "elm-mail-imported"

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

typedef struct {
	GList *dir_list;

	int num;
	
	GNOME_Evolution_Importer importer;
	EvolutionImporterListener *listener;

	GtkWidget *mail;
	gboolean do_mail;
	GtkWidget *alias;
	gboolean do_alias;
} ElmImporter;

typedef struct {
	char *parent;
	char *foldername;
	char *path;
} ElmFolder;

static void import_next (ElmImporter *importer);

static void
importer_cb (EvolutionImporterListener *listener,
	     EvolutionImporterResult result,
	     gboolean more_items,
	     void *data)
{
	ElmImporter *importer = (ElmImporter *) data;
	CORBA_Object objref;
	CORBA_Environment ev;

	g_print ("Processed.....\n");
	if (more_items) {
		g_print ("Processing.....\n");

		CORBA_exception_init (&ev);
		objref = bonobo_object_corba_objref (BONOBO_OBJECT (importer->listener));
		GNOME_Evolution_Importer_processItem (importer->importer,
						      objref, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Exception: %s", CORBA_exception_id (&ev));
			CORBA_exception_free (&ev);
			return;
		}
		CORBA_exception_free (&ev);
		return;
	}

	import_next (importer);
}

static gboolean
elm_import_file (ElmImporter *importer,
		 const char *path,
		 const char *folderpath)
{
	CORBA_boolean result;
	CORBA_Environment ev;
	CORBA_Object objref;

	g_warning ("Importing %s as %s", path, folderpath);
	
	CORBA_exception_init (&ev);
	result = GNOME_Evolution_Importer_loadFile (importer->importer, path,
						    folderpath, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || result == FALSE) {
		g_warning ("Exception here: %s", CORBA_exception_id (&ev));
		CORBA_Object_release (importer->importer, &ev);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	importer->listener = evolution_importer_listener_new (importer_cb,
							      importer);
	objref = bonobo_object_corba_objref (BONOBO_OBJECT (importer->listener));
	GNOME_Evolution_Importer_processItem (importer->importer, objref, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception: %s", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}
	CORBA_exception_free (&ev);

	return TRUE;
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
	char *key, *maildir, *evolution_dir;
	gboolean exists;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	/* Already imported */
	key = g_strdup_printf ("=%s/config/Importers=/importers/", evolution_dir);
	g_free (evolution_dir);

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
import_next (ElmImporter *importer)
{
	ElmFolder *data;

	if (importer->dir_list) {
		char *folder;

		data = importer->dir_list->data;

		folder = g_concat_dir_and_file (data->parent, data->foldername);
		elm_import_file (importer, data->path, folder);
		g_free (folder);
		g_free (data->parent);
		g_free (data->path);
		g_free (data->foldername);
		g_free (data);
		importer->dir_list = importer->dir_list->next;
	}
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
elm_create_structure (EvolutionIntelligentImporter *ii,
		      void *closure)
{
	ElmImporter *importer = closure;
	char *maildir, *key, *evolution_dir;

	maildir = gnome_util_prepend_user_home ("Mail");
	scan_dir (importer, maildir, "/");
	g_free (maildir);

	/* Import them */
	import_next (importer);

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Importers=/importers/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);
	
	gnome_config_set_bool (KEY, TRUE);
	gnome_config_pop_prefix ();
	
	gnome_config_sync ();
	gnome_config_drop_all ();
}

static void
elm_destroy_cb (GtkObject *object,
		ElmImporter *importer)
{
	g_print ("\n----------Settings-------\n");
	g_print ("Mail - %s\n", importer->do_mail ? "Yes" : "No");
	g_print ("Alias - %s\n", importer->do_alias ? "Yes" : "No");

	gtk_main_quit ();
}

/* Fun initialisation stuff */
/* Fun control stuff */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active (tb);
}

static BonoboControl *
create_checkboxes_control (ElmImporter *importer)
{
	GtkWidget *container, *vbox;
	BonoboControl *control;

	container = gtk_frame_new (_("Import"));
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (container), 2);
	gtk_container_add (GTK_CONTAINER (container), vbox);

	importer->mail = gtk_check_button_new_with_label (_("Mail"));
	gtk_signal_connect (GTK_OBJECT (importer->mail), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_mail);
	importer->alias = gtk_check_button_new_with_label (_("Elm Aliases"));
	gtk_signal_connect (GTK_OBJECT (importer->alias), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_alias);

	gtk_box_pack_start (GTK_BOX (vbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->alias, FALSE, FALSE, 0);

	/* Disable the things that can't be done */
	gtk_widget_set_sensitive (importer->alias, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail), TRUE);
	importer->do_mail = TRUE;
	importer->do_alias = FALSE;

	gtk_widget_show_all (container);
	control = bonobo_control_new (container);
	return control;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	EvolutionIntelligentImporter *importer;
	BonoboControl *control;
	ElmImporter *elm;
	CORBA_Environment ev;
	char *message = N_("Evolution has found Elm mail files in ~/Mail.\n"
			   "Would you like to import them into Evolution?");

	elm = g_new0 (ElmImporter, 1);
	CORBA_exception_init (&ev);
	elm->importer = oaf_activate_from_id (MBOX_IMPORTER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not start MBox importer\n%s", 
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	importer = evolution_intelligent_importer_new (elm_can_import,
						       elm_create_structure,
						       _("Elm mail"),
						       _(message), elm);
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (elm_destroy_cb), elm);

	control = create_checkboxes_control (elm);
	bonobo_object_add_interface (BONOBO_OBJECT (importer),
				     BONOBO_OBJECT (control));
	return BONOBO_OBJECT (importer);
}

/* Entry point */
static void
importer_init (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (ELM_INTELLIGENT_IMPORTER_IID,
					      factory_fn, NULL);
	if (factory == NULL) {
		g_error ("Could not initialise Elm Intelligent Mail Importer.");
		exit (0);
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}

int
main (int argc,
      char **argv)
{
	CORBA_ORB orb;

	gnome_init_with_popt_table ("Evolution-Elm-Importer",
				    VERSION, argc, argv, oaf_popt_options, 0,
				    NULL);
	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialise Bonobo");
		exit (0);
	}

	importer_init ();
	bonobo_main ();

	return 0;
}
