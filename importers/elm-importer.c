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
#include <bonobo/bonobo-context.h>
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

	GtkWidget *ask;
	gboolean ask_again;
} ElmImporter;

typedef struct {
	char *parent;
	char *foldername;
	char *path;
} ElmFolder;

static GHashTable *elm_prefs = NULL;

static void import_next (ElmImporter *importer);

static void
elm_store_settings (ElmImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Elm-Importer=/settings/",
			       evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	gnome_config_set_bool ("mail", importer->do_mail);
	gnome_config_set_bool ("alias", importer->do_alias);

	gnome_config_set_bool ("ask-again", importer->ask_again);
	gnome_config_pop_prefix ();
}

static void
elm_restore_settings (ElmImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Elm-Importer=/settings/",
			       evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	importer->do_mail = gnome_config_get_bool ("mail=True");
	importer->do_alias = gnome_config_get_bool ("alias=True");

	importer->ask_again = gnome_config_get_bool ("ask-again=False");
	gnome_config_pop_prefix ();
}

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

static void
parse_elm_rc (const char *elmrc)
{
	static gboolean parsed = FALSE;
	char line[4096];
	FILE *handle;
	gboolean exists;

	if (parsed == TRUE)
		return;

	elm_prefs = g_hash_table_new (g_str_hash, g_str_equal);

	exists = g_file_exists (elmrc);
	if (exists == FALSE) {
		parsed = TRUE;
		return;
	}

	handle = fopen (elmrc, "r");
	if (handle == NULL) {
		parsed = TRUE;
		return;
	}

	while (fgets (line, 4096, handle) != NULL) {
		char *linestart, *end;
		char *key, *value;
		if (*line == '#' &&
		    (line[1] != '#' && line[2] != '#')) {
			continue;
		} else if (*line == '\n') {
			continue;
		} else if (*line == '#' && line[1] == '#' && line[2] == '#') {
			linestart = line + 4;
		} else {
			linestart = line;
		}

		end = strstr (linestart, " = ");
		if (end == NULL) {
			g_warning ("Broken line");
			continue;
		}

		*end = 0;
		key = g_strdup (linestart);

		linestart = end + 3;
		end = strchr (linestart, '\n');
		if (end == NULL) {
			g_warning ("Broken line");
			g_free (key);
			continue;
		}

		*end = 0;
		value = g_strdup (linestart);

		g_hash_table_insert (elm_prefs, key, value);
	}

	parsed = TRUE;
	fclose (handle);
}

static char *
elm_get_rc_value (const char *value)
{
	if (elm_prefs == NULL)
		return NULL;

	return g_hash_table_lookup (elm_prefs, value);
}

static gboolean
elm_can_import (EvolutionIntelligentImporter *ii,
		void *closure)
{
	ElmImporter *importer = closure;
	char *key, *elmdir, *maildir, *evolution_dir, *aliasfile;
	char *elmrc;
	gboolean exists, mailexists, aliasexists;
	gboolean mail, alias;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	/* Already imported */
	key = g_strdup_printf ("=%s/config/Importers=/elm-importers/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	mail = gnome_config_get_bool ("mail-imported");
	alias = gnome_config_get_bool ("alias-importer");

	if (alias && mail) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	importer->do_mail = !mail;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail),
				      importer->do_mail);
	importer->do_alias = !alias;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->alias),
				      importer->do_alias);

	if (importer->ask_again == TRUE) {
		return FALSE;
	}

	elmdir = gnome_util_prepend_user_home (".elm");
	exists = g_file_exists (elmdir);

	g_free (elmdir);
	if (exists == FALSE)
		return FALSE;

	elmrc = gnome_util_prepend_user_home (".elm/elmrc");
	parse_elm_rc (elmrc);

	maildir = elm_get_rc_value ("maildir");
	if (maildir == NULL) {
		maildir = g_strdup ("Mail");
	} else {
		maildir = g_strdup (maildir);
	}

	if (!g_path_is_absolute (maildir)) {
		elmdir = gnome_util_prepend_user_home (maildir);
	} else {
		elmdir = g_strdup (maildir);
	}

	g_free (maildir);

	mailexists = g_file_exists (elmdir);
	g_free (elmdir);

	aliasfile = gnome_util_prepend_user_home (".elm/aliases");
	aliasexists = g_file_exists (aliasfile);
	g_free (aliasfile);

	exists = (aliasexists || mailexists);

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
	  const char *orig_parent,
	  const char *dirname)
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
			scan_dir (importer, subdir, fullname);
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

	/* Reference our object so when the shell release_unrefs us
	   we will still exist and not go byebye */
	bonobo_object_ref (BONOBO_OBJECT (ii));

	elm_store_settings (importer);
	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Importers=/elm-importers/", evolution_dir);
	g_free (evolution_dir);
	gnome_config_push_prefix (key);
	g_free (key);

	if (importer->do_alias == TRUE) {
		/* Do the aliases */
	}

	if (importer->do_mail == TRUE) {
		char *elmdir;
		gnome_config_set_bool ("mail-importer", TRUE);

		maildir = elm_get_rc_value ("maildir");
		if (maildir == NULL) {
			maildir = g_strdup ("Mail");
		} else {
			maildir = g_strdup (maildir);
		}
		
		if (!g_path_is_absolute (maildir)) {
			elmdir = gnome_util_prepend_user_home (maildir);
		} else {
			elmdir = g_strdup (maildir);
		}
		
		g_free (maildir);

		scan_dir (importer, "/", maildir);
		g_free (maildir);
		
		/* Import them */
		import_next (importer);
	}

	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();

	if (importer->do_mail == FALSE) {
		bonobo_object_unref (BONOBO_OBJECT (ii));
	}
}

static void
elm_destroy_cb (GtkObject *object,
		ElmImporter *importer)
{
	g_print ("\n----------Settings-------\n");
	g_print ("Mail - %s\n", importer->do_mail ? "Yes" : "No");
	g_print ("Alias - %s\n", importer->do_alias ? "Yes" : "No");

	elm_store_settings (importer);
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
	GtkWidget *container, *vbox, *sep;
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

	sep = gtk_hseparator_new ();

	importer->ask = gtk_check_button_new_with_label (_("Don't ask me again"));
	gtk_signal_connect (GTK_OBJECT (importer->ask), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->ask_again);

	gtk_box_pack_start (GTK_BOX (vbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->alias, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->ask, FALSE, FALSE, 0);

	/* Disable the things that can't be done */
	gtk_widget_set_sensitive (importer->alias, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail), 
				      importer->do_mail);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->alias),
				      importer->do_alias);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->ask), 
				      importer->ask_again);

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
	char *message = N_("Evolution has found Elm mail files\n"
			   "Would you like to import them into Evolution?");

	elm = g_new0 (ElmImporter, 1);
	elm_restore_settings (elm);

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
