/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* pine-importer.c
 * 
 * Authors: 
 *    Iain Holmes  <iain@ximian.com>
 *
 * Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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
#include <ctype.h>

#include <glib.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/evolution-importer-client.h>
#include <importer/GNOME_Evolution_Importer.h>

#include <ebook/e-book.h>
#include <ebook/e-card-simple.h>

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
	GtkWidget *address;
	gboolean do_address;

	EBook *book;

	Bonobo_ConfigDatabase db;

	/* GUI */
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progressbar;
} PineImporter;

typedef struct {
	char *parent;
	char *foldername;
	char *path;
	gboolean folder;
} PineFolder;

static void import_next (PineImporter *importer);

static GtkWidget *
create_importer_gui (PineImporter *importer)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("Evolution is importing your old Pine data"), GNOME_MESSAGE_BOX_INFO, NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Importing..."));

	importer->label = gtk_label_new (_("Please wait"));
	importer->progressbar = gtk_progress_bar_new ();
	gtk_progress_set_activity_mode (GTK_PROGRESS (importer->progressbar), TRUE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    importer->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    importer->progressbar, FALSE, FALSE, 0);
	return dialog;
}

static void
pine_store_settings (PineImporter *importer)
{
	bonobo_config_set_boolean (importer->db, "/Importer/Pine/mail", 
				   importer->do_mail, NULL);
	bonobo_config_set_boolean (importer->db, "/Importer/Pine/address", 
				   importer->do_address, NULL);
}

static void
pine_restore_settings (PineImporter *importer)
{
	importer->do_mail = FALSE;
	importer->do_address = FALSE;

}

/* A very basic address spliter.
   Returns the first email address
   denoted by <address> */
static char *
parse_address (const char *address)
{
	char *addr_dup, *result, *start, *end;

	if (address == NULL) {
		return NULL;
	}

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
add_card_cb (EBook *book, 
	     EBookStatus status,
	     const char *id,
	     gpointer closure)
{
	gtk_object_unref (GTK_OBJECT (closure));
}

static void
parse_line (EBook *book,
	    char *line)
{
	char **strings;
	ECardName *name;
	ECard *card;
	EList *list;

	card = e_card_new ("");
	strings = g_strsplit (line, "\t", 3);
	if (strings[0] && strings[1] && strings[2]) {
		name = e_card_name_from_string (strings[1]);
		gtk_object_set (GTK_OBJECT (card),
				"nickname", strings[0],
				"full_name", strings[1],
				"name", name, NULL);
		gtk_object_get (GTK_OBJECT (card),
				"email", &list,
				NULL);
		e_list_append (list, strings[2]);
		g_strfreev (strings);
		e_book_add_card (book, card, add_card_cb, card);
	}
}

static void
import_addressfile (EBook *book,
		    EBookStatus status,
		    gpointer user_data)
{
	char *addressbook;
	FILE *handle;
	char line[2 * 1024];
	int which = 0;
	char *lastline = NULL;
	PineImporter *importer = user_data;

	addressbook = gnome_util_prepend_user_home (".addressbook");
	handle = fopen (addressbook, "r");
	g_free (addressbook);

	if (handle == NULL) {
		g_warning ("Cannot open .addressbook");
		return;
	}

	while (fgets (line + which * 1024, 1024, handle)) {
		int length;
		char *thisline = line + which * 1024;
		
		length = strlen (thisline);
		if (thisline[length - 1] == '\n') {
			line[--length] = 0;
		}

		if (lastline && *thisline && isspace ((int) *thisline)) {
			char *temp;

			while (*thisline && isspace ((int) *thisline)) {
				thisline++;
			}
			temp = lastline;
			lastline = g_strdup_printf ("%s%s", lastline, thisline);
			g_free (temp);
			continue;
		}

		if (lastline) {
			parse_line (book, lastline);
			g_free (lastline);
		}

		lastline = g_strdup (thisline);
	}

	if (lastline) {
		parse_line (book, lastline);
		g_free (lastline);
	}

	fclose (handle);
	if (importer->do_mail == FALSE) {
		if (importer->db != CORBA_OBJECT_NIL) {
			bonobo_object_release_unref (importer->db, NULL);
		}
		gtk_main_quit ();
	}
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

static gboolean
importer_timeout_fn (gpointer data)
{
	PineImporter *importer = (PineImporter *) data;
	CORBA_Object objref;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	objref = bonobo_object_corba_objref (BONOBO_OBJECT (importer->listener));
	GNOME_Evolution_Importer_processItem (importer->importer, objref, &ev);
	CORBA_exception_free (&ev);

	return FALSE;
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

	if (result == EVOLUTION_IMPORTER_NOT_READY ||
	    result == EVOLUTION_IMPORTER_BUSY) {
		gtk_timeout_add (1000, importer_timeout_fn, data);
		return;
	}

	if (more_items) {
		GtkAdjustment *adj;
		float newval;

		adj = GTK_PROGRESS (importer->progressbar)->adjustment;
		newval = adj->value + 1;
		if (newval > adj->upper) {
			newval = adj->lower;
		}

		gtk_progress_set_value (GTK_PROGRESS (importer->progressbar), newval);
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
		bonobo_object_release_unref (importer->importer, NULL);
		if (importer->db != CORBA_OBJECT_NIL) {
			bonobo_object_release_unref (importer->db, NULL);
		}
		gtk_main_quit ();
	}
}

static gboolean
pine_import_file (PineImporter *importer,
		  const char *path,
		  const char *folderpath,
		  gboolean folder)
{
	CORBA_boolean result;
	CORBA_Environment ev;
	CORBA_Object objref;
	char *str;

	CORBA_exception_init (&ev);

	str = g_strdup_printf (_("Importing %s as %s"), path, folderpath);
	gtk_label_set_text (GTK_LABEL (importer->label), str);
	g_free (str);
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
	result = GNOME_Evolution_Importer_loadFile (importer->importer, path,
						    folderpath, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || result == FALSE) {
		g_warning ("Exception here: %s\n%s, %s", CORBA_exception_id (&ev), path, folderpath);
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
	char *maildir, *addrfile;
	gboolean mail;
	gboolean md_exists, addr_exists;

	maildir = gnome_util_prepend_user_home ("mail");
	md_exists = g_file_exists (maildir);
	importer->do_mail = md_exists;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail),
				      importer->do_mail);
	
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
		pine_import_file (importer, data->path, folder, data->folder);
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
	char *str;
	
	maildir = opendir (dirname);
	if (maildir == NULL) {
		g_warning ("Could not open %s\nopendir returned: %s",
			   dirname, g_strerror (errno));
		return;
	}
	
	str = g_strdup_printf (_("Scanning %s"), dirname);
	gtk_label_set_text (GTK_LABEL (importer->label), str);
	g_free (str);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
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
			pf->folder = FALSE;
			importer->dir_list = g_list_append (importer->dir_list, pf);
		} else if (S_ISDIR (buf.st_mode)) {
			char *subdir;

			pf = g_new (PineFolder, 1);
			pf->path = g_strdup (fullname);
			pf->parent = g_strdup (orig_parent);
			pf->foldername = g_strdup (foldername);
			pf->folder = TRUE;
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
	CORBA_Environment ev;
	PineImporter *importer = closure;
	char *maildir;

	bonobo_object_ref (BONOBO_OBJECT (ii));
	pine_store_settings (importer);

	/* Create a dialog */
	if (importer->do_address == TRUE ||
	    importer->do_mail == TRUE) {
		importer->dialog = create_importer_gui (importer);
		gtk_widget_show_all (importer->dialog);
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
	}

	if (importer->do_address == TRUE) {
		bonobo_config_set_boolean (importer->db, 
                "/Importer/Pine/address-imported", TRUE, NULL);

		import_addressbook (importer);
	}

	if (importer->do_mail == TRUE) {
		bonobo_config_set_boolean (importer->db, 
                "/Importer/Pine/mail-imported", TRUE, NULL);

		maildir = gnome_util_prepend_user_home ("mail");
		gtk_label_set_text (GTK_LABEL (importer->label),
				    _("Scanning directory"));
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}

		scan_dir (importer, maildir, "/");
		g_free (maildir);

		/* Import them */
		import_next (importer);
	}

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (importer->db, &ev);
	CORBA_exception_free (&ev);

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
	CORBA_Environment ev;

	pine_store_settings (importer);

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (importer->db, &ev);
	CORBA_exception_free (&ev);

	if (importer->db != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (importer->db, NULL);
	}
	importer->db = CORBA_OBJECT_NIL;

	if (importer->importer != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (importer->importer, NULL);
	}
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
	GtkWidget *hbox;
	BonoboControl *control;

	hbox = gtk_hbox_new (FALSE, 2);

	importer->mail = gtk_check_button_new_with_label (_("Mail"));
	gtk_signal_connect (GTK_OBJECT (importer->mail), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_mail);

	importer->address = gtk_check_button_new_with_label (_("Addressbook"));
	gtk_signal_connect (GTK_OBJECT (importer->address), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_address);

	gtk_box_pack_start (GTK_BOX (hbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), importer->address, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	control = bonobo_control_new (hbox);
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

	CORBA_exception_init (&ev);

	pine->db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);

	if (BONOBO_EX (&ev) || pine->db == CORBA_OBJECT_NIL) {
		g_free (pine);
		CORBA_exception_free (&ev);
		return NULL;
 	}

	pine_restore_settings (pine);

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
						       _("Pine"),
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

	bindtextdomain(PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain(PACKAGE);
	
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
