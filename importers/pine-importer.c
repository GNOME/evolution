/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* pine-importer.c
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
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-main.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/evolution-importer-client.h>
#include <importer/GNOME_Evolution_Importer.h>

#include <e-book.h>
#include <e-card-simple.h>

#define PINE_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Pine_Intelligent_Importer_Factory"
#define MBOX_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Mbox_Importer"
#define KEY "pine-mail-imported"

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

typedef struct {
	GList *dir_list;

	GNOME_Evolution_Importer importer;
	EvolutionImporterListener *listener;

	GtkWidget *mail;
	gboolean do_mail;
	GtkWidget *settings;
	gboolean do_settings;
	GtkWidget *address;
	gboolean do_address;

	GtkWidget *ask;
	gboolean ask_again;

	EBook *book;
} PineImporter;

typedef struct {
	char *parent;
	char *foldername;
	char *path;
} PineFolder;

static void import_next (PineImporter *importer);

static void
pine_store_settings (PineImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Pine-Importer=/settings/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	gnome_config_set_bool ("mail", importer->do_mail);
	gnome_config_set_bool ("settings", importer->do_settings);
	gnome_config_set_bool ("address", importer->do_address);

	gnome_config_set_bool ("ask-again", importer->ask_again);
	gnome_config_pop_prefix ();
}

static void
pine_restore_settings (PineImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Pine-Importer=/settings/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);
	
	importer->do_mail = gnome_config_get_bool ("mail=True");
	importer->do_settings = gnome_config_get_bool ("settings=True");
	importer->do_address = gnome_config_get_bool ("address=True");

	importer->ask_again = gnome_config_get_bool ("ask-again=False");
	gnome_config_pop_prefix ();
}

/* Pass in handle so we can get the next line if we need to */
static char *
get_field (char **start,
	   FILE *handle)
{
	char line[4096];
	char *end, *field;
	int length;

	/* if this was the last line, just return */
	if (*start == NULL) {
		return NULL;
	}

	/* if start is just \n then we need the next line */
	if (**start == '\n') {
		g_warning ("Need next line");
		if (fgets (line, 4096, handle) == NULL) {
			g_warning ("Hmmm, no next line");
			return NULL;
		}

		if (line[0] != ' ' || line[1] != ' ' || line[2] != ' ') {
			g_warning ("Next line was not a continuation line\n"
				   "Oppps: %s", line);
			return NULL;
		}

		*start = line + 3;
	}

	if (**start == '\t') {
		/* Blank field */

		*start += 1;
		return NULL;
	}

	end = strchr (*start, '\t');
	if (end == NULL) {
		/* the line was the last comment, so just return the line */
		length = strlen (line);
		line[length - 1] = 0;

		field = g_strdup (*start);
		
		*start = NULL;
	} else {
		/* Null the end */
		*end = 0;

		field = g_strdup (*start);
		*start = end + 1;
	}

	g_warning ("Found %s", field);
	return field;
}

/* A very basic address spliter.
   Returns the first email address
   denoted by <address> */
static char *
parse_address (const char *address)
{
	char *addr_dup, *result, *start, *end;

	addr_dup = g_strdup (address);
	start = strchr (addr_dup, '<');
	if (start == NULL) {
		/* Whole line is an address */
		return addr_dup;
	}

	start += 1;
	end = strchr (start, '>');
	if (end == NULL) {
		result = g_strdup (start);
		g_free (addr_dup);

		return result;
	}

	*end = 0;
	result = strdup (start);
	g_free (addr_dup);

	return result;
}

static void
import_addressfile (EBook *book,
		    EBookStatus status,
		    gpointer user_data)
{
	char *addressbook;
	FILE *handle;
	char line[4096];

	PineImporter *importer = user_data;

	addressbook = gnome_util_prepend_user_home (".addressbook");
	handle = fopen (addressbook, "r");
	g_free (addressbook);

	if (handle == NULL) {
		g_warning ("Cannot open .addressbook");
		return;
	}

	while (fgets (line, 4096, handle) != NULL) {
		char *nick, *fullname, *address, *comment, *fcc, *email = NULL;
		char *start;
		gboolean distrib = FALSE;

		start = line;
		nick = get_field (&start, handle);
		g_print ("Nick: %s\n", nick);

		fullname = get_field (&start, handle);
		g_print ("Fullname: %s\n", fullname);

		address = get_field (&start, handle);
		g_print ("Address: %s\n", address);

		if (*address == '(') {
			char nextline[4096];
			/* Fun, it's a distribution list */
			distrib = TRUE;
			
			/* if the last char of address == ) then this is the
			   full list */
			while (fgets (nextline, 4096, handle)) {
				char *bracket;
				if (nextline[0] != ' ' ||
				    nextline[1] != ' ' ||
				    nextline[2] != ' ') {
					/* Not a continuation line */
					start = nextline;
					break;
				}

				bracket = strchr (nextline, ')');
				if (bracket != NULL &&
				    *(bracket + 1) == '\t') {

					*(bracket + 1) = 0;
					g_print ("More addresses: %s\n", nextline);
#if 0
					e_list_append (emaillist, g_strdup (nextline));
#endif
					start = bracket + 2;
					break;
				} else {
					g_print ("More addresses: %s\n", nextline);
#if 0
					e_list_append (emaillist, g_strdup (nextline));
#endif
				}
			}
		} else {
			email = parse_address (address);

			g_print ("Real address: %s", email);
		}

		fcc = get_field (&start, handle);
		g_print ("FCC: %s\n", fcc);

		comment = get_field (&start, handle);
		g_print ("Comment: %s\n", comment);

		if (distrib == FALSE) {
			/* This was not a distribution list...
			   Evolution doesn't handle that yet (070501)
			   Fixme when it does */
			ECard *card = e_card_new ("");
			ECardSimple *simple = e_card_simple_new (card);

			if (fullname != NULL)
				e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_FULL_NAME,
						   fullname);
			else
				e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_FILE_AS,
						   nick);

			e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_EMAIL,
					   email);
			e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_NOTE,
					   comment);
			e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_NICKNAME,
					   nick);
			e_book_add_card (importer->book, simple->card, NULL, NULL);
		}
		
	}
	
	fclose (handle);
}

static void
import_addressbook (PineImporter *importer)
{
	char *path, *uri;

	importer->book = e_book_new ();
	if (importer->book == NULL) {
		g_warning ("Could not create EBook.");
		return;
	}

	path = g_concat_dir_and_file (g_get_home_dir (),
				      "evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf ("file://%s", path);
	g_free (path);

	if (!e_book_load_uri (importer->book, uri, import_addressfile, importer)) {
		g_warning ("Error calling load_uri");
	}
	g_free (uri);
}

static void
importer_cb (EvolutionImporterListener *listener,
	     EvolutionImporterResult result,
	     gboolean more_items,
	     void *data)
{
	PineImporter *importer = (PineImporter *) data;
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

	if (importer->dir_list) {
		import_next (importer);
	} else {
		gtk_main_quit ();
	}
}

static gboolean
pine_import_file (PineImporter *importer,
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
	}
	CORBA_exception_free (&ev);

	return TRUE;
}

static gboolean
pine_can_import (EvolutionIntelligentImporter *ii,
		 void *closure)
{
	PineImporter *importer = closure;
	char *key, *maildir, *evolution_dir, *addrfile;
	gboolean mail;
	gboolean md_exists, addr_exists;

	/* Already imported */
	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Importers=/pine-importer/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	mail = gnome_config_get_bool ("mail-imported");

	if (mail) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	importer->do_mail = !mail;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail),
				      importer->do_mail);
	
	if (importer->ask_again == TRUE)
		return FALSE;

	maildir = gnome_util_prepend_user_home ("mail");
	md_exists = g_file_exists (maildir);
	gtk_widget_set_sensitive (importer->mail, md_exists);
	g_free (maildir);

	addrfile = gnome_util_prepend_user_home (".addressbook");
	addr_exists = g_file_exists (addrfile);
	g_free (addrfile);
	gtk_widget_set_sensitive (importer->address, addr_exists);

	return md_exists || addr_exists;
}

static void
import_next (PineImporter *importer)
{
	PineFolder *data;

	if (importer->dir_list) {
		char *folder;

		data = importer->dir_list->data;

		folder = g_concat_dir_and_file (data->parent, data->foldername);
		pine_import_file (importer, data->path, folder);
		g_free (folder);
		g_free (data->parent);
		g_free (data->path);
		g_free (data->foldername);
		g_free (data);
		importer->dir_list = importer->dir_list->next;
	}

}

/* Pine uses sent-mail and saved-mail whereas Evolution uses Sent and Drafts */
static char *
maybe_replace_name (const char *original_name)
{
	if (strcmp (original_name, "sent-mail") == 0) {
		return g_strdup ("Sent");
	} else if (strcmp (original_name, "saved-messages") == 0) {
		return g_strdup ("Drafts");
	} 

	return g_strdup (original_name);
}

static void
scan_dir (PineImporter *importer,
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
		PineFolder *pf;
		char *fullname, *foldername;
		
		/* Ignore . and .. */
		if (current->d_name[0] == '.') {
			if (current->d_name[1] == '\0' ||
			    (current->d_name[1] == '.' && current->d_name[2] == '\0')) {
				current = readdir (maildir);
				continue;
			}
		}

		if (*orig_parent == '/') {
			foldername = maybe_replace_name (current->d_name);
		} else {
			foldername = g_strdup (current->d_name);
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
			pf = g_new (PineFolder, 1);
			pf->path = g_strdup (fullname);
			pf->parent = g_strdup (orig_parent);
			pf->foldername = g_strdup (foldername);
			importer->dir_list = g_list_append (importer->dir_list, pf);
		} else if (S_ISDIR (buf.st_mode)) {
			char *subdir;

			pf = g_new (PineFolder, 1);
			pf->path = NULL;
			pf->parent = g_strdup (orig_parent);
			pf->foldername = g_strdup (foldername);
			importer->dir_list = g_list_append (importer->dir_list, pf);

			subdir = g_concat_dir_and_file (orig_parent, foldername);
			scan_dir (importer, fullname, subdir);
			g_free (subdir);
		}
		
		g_free (fullname);
		g_free (foldername);
		current = readdir (maildir);
	}
}

static void
pine_create_structure (EvolutionIntelligentImporter *ii,
		       void *closure)
{
	PineImporter *importer = closure;
	char *maildir, *key, *evolution_dir;

	bonobo_object_ref (BONOBO_OBJECT (ii));
	pine_store_settings (importer);

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Importers=/pine-importer/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	if (importer->do_address == TRUE) {
		gnome_config_set_bool ("address-imported", TRUE);

		import_addressbook (importer);
	}

	if (importer->do_mail == TRUE) {
		gnome_config_set_bool ("mail-imported", TRUE);

		maildir = gnome_util_prepend_user_home ("mail");
		scan_dir (importer, maildir, "/");
		g_free (maildir);

		/* Import them */
		import_next (importer);
	}

	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();

	if (importer->do_mail == FALSE && importer->do_address == FALSE) {
		/* Destroy it here if we weren't importing mail
		   otherwise the mail importer destroys itself
		   once the mail is imported */

		/* Hmmm, this needs fixed badly */
		bonobo_object_unref (BONOBO_OBJECT (ii));
	}
}

static void
pine_destroy_cb (GtkObject *object,
		 PineImporter *importer)
{
	g_print ("\n-------Settings-------\n");
	g_print ("Mail - %s\n", importer->do_mail ? "Yes" : "No");
	g_print ("Settings - %s\n", importer->do_settings ? "Yes" : "No");
	g_print ("Address - %s\n", importer->do_address ? "Yes" : "No");

	pine_store_settings (importer);
	gtk_main_quit ();
}

/* Fun inity stuff */

/* Fun control stuff */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active (tb);
}

static BonoboControl *
create_checkboxes_control (PineImporter *importer)
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

	importer->settings = gtk_check_button_new_with_label (_("Settings"));
	gtk_signal_connect (GTK_OBJECT (importer->settings), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_settings);

	importer->address = gtk_check_button_new_with_label (_("Addressbook"));
	gtk_signal_connect (GTK_OBJECT (importer->address), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_address);

	sep = gtk_hseparator_new ();

	importer->ask = gtk_check_button_new_with_label (_("Don't ask me again"));
	gtk_signal_connect (GTK_OBJECT (importer->ask), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->ask_again);

	gtk_box_pack_start (GTK_BOX (vbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->settings, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->address, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->ask, FALSE, FALSE, 0);

	/* Disable the don't do anythings */
	gtk_widget_set_sensitive (importer->settings, FALSE);
	gtk_widget_set_sensitive (importer->address, FALSE);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail), 
				      importer->do_mail);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->settings),
				      importer->do_settings);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->address),
				      importer->do_address);
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
	PineImporter *pine;
	CORBA_Environment ev;
	char *message = N_("Evolution has found Pine mail files.\n"
			   "Would you like to import them into Evolution?");

	pine = g_new0 (PineImporter, 1);
	pine_restore_settings (pine);

	CORBA_exception_init (&ev);
	pine->importer = oaf_activate_from_id (MBOX_IMPORTER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not start MBox importer\n%s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	importer = evolution_intelligent_importer_new (pine_can_import,
						       pine_create_structure,
						       _("Pine mail"),
						       _(message), pine);
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (pine_destroy_cb), pine);

	control = create_checkboxes_control (pine);
	bonobo_object_add_interface (BONOBO_OBJECT (importer),
				     BONOBO_OBJECT (control));
	return BONOBO_OBJECT (importer);
}

static void
importer_init (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (PINE_INTELLIGENT_IMPORTER_IID,
					      factory_fn, NULL);
	if (factory == NULL) {
		g_warning ("Could not initialise Pine Intelligent Mail Importer.");
		exit (0);
	}
	
	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}

int
main (int argc,
      char **argv)
{
	CORBA_ORB orb;

	gnome_init_with_popt_table ("Evolution-Pine-Importer",
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
