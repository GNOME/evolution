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
#include <gnome.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <importer/evolution-importer-client.h>

#include "Mail.h"

static char *nsmail_dir = NULL;
static GHashTable *user_prefs = NULL;

#define FACTORY_IID "OAFIID:GNOME_Evolution_Netscape_Intelligent_Importer_Factory"
#define MBOX_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Mbox_Importer"
#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig"

#define KEY "netscape-mail-imported"

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

	/* Checkboxes */
	GtkWidget *mail;
	gboolean do_mail;
	GtkWidget *addrs;
	gboolean do_addrs;
	GtkWidget *filters;
	gboolean do_filters;
	GtkWidget *settings;
	gboolean do_settings;

	GtkWidget *ask;
	gboolean ask_again;
} NetscapeImporter;

static void import_next (NetscapeImporter *importer);

static void
netscape_store_settings (NetscapeImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Netscape-Importer=/settings/", 
			       evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	gnome_config_set_bool ("mail", importer->do_mail);
	gnome_config_set_bool ("address", importer->do_addrs);
	gnome_config_set_bool ("filters", importer->do_filters);
	gnome_config_set_bool ("settings", importer->do_settings);

	gnome_config_set_bool ("ask-again", importer->ask_again);
	gnome_config_pop_prefix ();
}

static void
netscape_restore_settings (NetscapeImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Netscape-Importer=/settings/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	importer->do_mail = gnome_config_get_bool ("mail=True");
	importer->do_addrs = gnome_config_get_bool ("address=True");
	importer->do_filters = gnome_config_get_bool ("filters=True");
	importer->do_settings = gnome_config_get_bool ("setting=True");

	importer->ask_again = gnome_config_get_bool ("ask-again=False");
	gnome_config_pop_prefix ();
}

static const char *
netscape_get_string (const char *strname)
{
	return g_hash_table_lookup (user_prefs, strname);
}

static int
netscape_get_integer (const char *strname)
{
	char *intstr;

	intstr = g_hash_table_lookup (user_prefs, strname);
	g_print ("Request: %s %s.\n", strname, intstr);
	if (intstr == NULL) {
		return 0;
	} else {
		return atoi (intstr);
	}
}

static gboolean
netscape_get_boolean (const char *strname)
{
	char *boolstr;

	boolstr = g_hash_table_lookup (user_prefs, strname);

	g_print ("Request: %s %s.\n", strname, boolstr);
	if (boolstr == NULL) {
		return FALSE;
	} else {
		if (strcasecmp (boolstr, "false") == 0) {
			return FALSE;
		} else if (strcasecmp (boolstr, "true") == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static char *
netscape_get_key (const char *line)
{
	char *line_dup;
	char *start, *end;
	char *key;
	
	line_dup = g_strdup (line);
	start = strchr (line_dup, '\"');
	if (start == NULL)
		goto die;
	start++;
	if (*start == '\0')
		goto die;

	end = strchr (start, '\"');
	if (end == NULL)
		goto die;
	*end = '\0';

	key = g_strdup (start);
	g_free (line_dup);

	g_warning ("Found key: %s", key);
	return key;

 die:
	g_free (line_dup);
	g_warning ("Broken line: %s", line);
	return NULL;
}

static char *
netscape_get_value (const char *line)
{
	char *line_dup;
	char *start, *end;
	char *value;

	line_dup = g_strdup (line);
	start = strchr (line_dup, ',');
	if (start == NULL)
		goto die;
	start++;
	if (*start == '\0')
		goto die;

	if (*start == ' ')
		start++;
	if (*start == '\0')
		goto die;

	if (*start == '\"')
		start++;
	if (*start == '\0')
		goto die;

	/* Start should now be the start of the value */
	end = strrchr (start, ')');
	if (end == NULL)
		goto die;
	*end = '\0';
	if (*(end - 1) == '\"')
		*(end - 1) = '\0';

	if (start == (end - 1)) {
		g_free (line_dup);
		return NULL;
	}

	value = g_strdup (start);
	g_free (line_dup);

	g_warning ("Found value: %s", value);
	return value;

 die:
	g_free (line_dup);
	g_warning ("Broken line: %s", line);
	return NULL;
}

static void
netscape_init_prefs (void)
{
	FILE *prefs_handle;
	char *nsprefs;
	char line[4096];

	user_prefs = g_hash_table_new (g_str_hash, g_str_equal);

	nsprefs = gnome_util_prepend_user_home (".netscape/preferences.js");
	prefs_handle = fopen (nsprefs, "r");
	g_free (nsprefs);

	if (prefs_handle == NULL) {
		d(g_warning ("No .netscape/preferences.js"));
		g_hash_table_destroy (user_prefs);
		user_prefs = NULL;
		return;
	}

	/* Find the user mail dir */
	while (fgets (line, 4096, prefs_handle)) {
		char *key, *value;

		key = netscape_get_key (line);
		value = netscape_get_value (line);

		if (key == NULL)
			continue;

		g_hash_table_insert (user_prefs, key, value);
	}

	return;
}

static void
netscape_import_accounts (NetscapeImporter *importer)
{
	char *nstr;
	char *imap;
	GNOME_Evolution_MailConfig_Account account;
	GNOME_Evolution_MailConfig_Service source, transport;
	GNOME_Evolution_MailConfig_Identity id;
	CORBA_Object objref;
	CORBA_Environment ev;

	if (user_prefs == NULL) {
		netscape_init_prefs ();
		if (user_prefs == NULL)
			return;
	}

	CORBA_exception_init (&ev);
	objref = oaf_activate_from_id (MAIL_CONFIG_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error starting mail config");
		CORBA_exception_free (&ev);
		return;
	}

	if (objref == CORBA_OBJECT_NIL) {
		g_warning ("Error activating mail config");
		return;
	}

	/* Create identify structure */
	nstr = netscape_get_string ("mail.identity.username");
	id.name = CORBA_string_dup (nstr ? nstr : "John Doe");
	nstr = netscape_get_string ("mail.identity.useremail");
	id.address = CORBA_string_dup (nstr ? nstr : "");
	nstr = netscape_get_string ("mail.identity.organization");
	id.organization = CORBA_string_dup (nstr ? nstr : "");
	nstr = netscape_get_string ("mail.signature_file");
	id.signature = CORBA_string_dup (nstr ? nstr : "");

	/* Create transport */
	nstr = netscape_get_string ("network.hosts.smtp_server");
	if (nstr != NULL) {
		char *url, *nstr2;

		nstr2 = netscape_get_string ("mail.smtp_name");
		if (nstr2) {
			url = g_strconcat ("smtp://", nstr2, "@", nstr, NULL);
		} else {
			url = g_strconcat ("smtp://", nstr, NULL);
		}
		transport.url = CORBA_string_dup (url);
		transport.keep_on_server = FALSE;
		transport.auto_check = FALSE;
		transport.auto_check_time = 10;
		transport.save_passwd = FALSE;
		transport.enabled = TRUE;
		g_free (url);
	}

	/* Create account */
	nstr = netscape_get_string ("mail.identity.username");
	account.name = CORBA_string_dup (nstr ? nstr : "");
	account.default_account = FALSE;
	account.id = id;
	account.source = source;
	account.transport = transport;

	account.drafts_folder_name = CORBA_string_dup ("");
	account.drafts_folder_uri = CORBA_string_dup ("");
	account.sent_folder_name = CORBA_string_dup ("");
	account.sent_folder_uri = CORBA_string_dup ("");

	/* Create POP3 source */
	nstr = netscape_get_string ("network.hosts.pop_server");
	if (nstr != NULL && *nstr != 0) {
		char *url, *nstr2;

		nstr2 = netscape_get_string ("mail.pop_name");
		if (nstr2) {
			url = g_strconcat ("pop://", nstr2, "@", nstr, NULL);
		} else {
			url = g_strconcat ("pop://", nstr, NULL);
		}
		source.url = CORBA_string_dup (url);
		source.keep_on_server = netscape_get_boolean ("mail.leave_on_server");
		source.auto_check = TRUE;
		source.auto_check_time = 10;
		source.save_passwd = netscape_get_boolean ("mail.remember_password");
		source.enabled = TRUE;
		g_free (url);
	} else {
		/* Are there IMAP accounts? */
		imap = netscape_get_string ("network.hosts.imap_servers");
		if (imap != NULL) {
			char **servers;
			int i;

			servers = g_strsplit (imap, ",", 1024);
			for (i = 0; servers[i] != NULL; i++) {
				GNOME_Evolution_MailConfig_Service imapsource;
				char *serverstr, *name, *url, *username;

				g_warning ("i: %d", i);
				/* Create a server for each of these */
				serverstr = g_strdup_printf ("mail.imap.server.%s.", servers[i]);
				name = g_strconcat (serverstr, "userName", NULL);
				username = netscape_get_string (name);
				g_free (name);

				if (username)
					url = g_strconcat ("imap://", username,
							   "@", servers[i], NULL);
				else
					url = g_strconcat ("imap://", servers[i], NULL);

				g_warning ("URL: %s", url);
				imapsource.url = CORBA_string_dup (url);
				
				imapsource.keep_on_server = netscape_get_boolean ("mail.leave_on_server");
				
				name = g_strconcat (serverstr, "check_new_mail", NULL);
				imapsource.auto_check = netscape_get_boolean (name);
				g_free (name);

				name = g_strconcat (serverstr, "check_time", NULL);
				imapsource.auto_check_time = netscape_get_integer (name);
				g_free (name);

				name = g_strconcat (serverstr, "remember_password", NULL);
				imapsource.save_passwd = netscape_get_boolean (name);
				g_free (name);
				imapsource.enabled = TRUE;

				account.source = imapsource;

				GNOME_Evolution_MailConfig_addAccount (objref, &account, &ev);
				if (ev._major != CORBA_NO_EXCEPTION) {
					g_warning ("Error setting account: %s", CORBA_exception_id (&ev));
					CORBA_exception_free (&ev);
					return;
				}
				
				g_free (url);
				g_free (serverstr);
			}

			CORBA_exception_free (&ev);			
			g_strfreev (servers);
			return;
		}
	}

	GNOME_Evolution_MailConfig_addAccount (objref, &account, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error setting account: %s", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
}
	
static gboolean
netscape_can_import (EvolutionIntelligentImporter *ii,
		     void *closure)
{
	NetscapeImporter *importer = closure;
	gboolean mail, settings;
	char *evolution_dir;
	char *key;

	/* Probably shouldn't hard code this, but there's no way yet to change
	   the home dir. FIXME */
	evolution_dir = gnome_util_prepend_user_home ("evolution");
	/* Already imported */
	key = g_strdup_printf ("=%s/config/Importers=/netscape-importers/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	mail = gnome_config_get_bool ("mail-imported");
	settings = gnome_config_get_bool ("settings-imported");

	if (settings && mail) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	importer->do_mail = !mail;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail), 
				      importer->do_mail);
	importer->do_settings = !settings;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->settings),
				      importer->do_settings);

	if (importer->ask_again == TRUE) {
		return FALSE;
	}

	if (user_prefs == NULL) {
		netscape_init_prefs ();
	}

	nsmail_dir = g_hash_table_lookup (user_prefs, "mail.directory");
	if (nsmail_dir == NULL)
		return FALSE;
	else
		return TRUE;
}

static void
importer_cb (EvolutionImporterListener *listener,
	     EvolutionImporterResult result,
	     gboolean more_items,
	     void *data)
{
	NetscapeImporter *importer = (NetscapeImporter *) data;
	CORBA_Object objref;
	CORBA_Environment ev;

	g_print ("Processed....\n");
	if (more_items) {
		g_print ("Processing...\n");
		
		CORBA_exception_init (&ev);
		objref = bonobo_object_corba_objref (BONOBO_OBJECT (importer->listener));
		GNOME_Evolution_Importer_processItem (importer->importer,
						      objref, &ev);
		if (ev._major != CORBA_OBJECT_NIL) {
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
netscape_import_file (NetscapeImporter *importer,
		      const char *path,
		      const char *folderpath)
{
	CORBA_boolean result;
	CORBA_Environment ev;
	CORBA_Object objref;

	/* Do import */
	g_warning ("Importing %s as %s\n", path, folderpath);

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
	g_print ("%s:Processing...\n", __FUNCTION__);
	CORBA_exception_init (&ev);
	GNOME_Evolution_Importer_processItem (importer->importer, 
					      objref, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception: %s", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}
	CORBA_exception_free (&ev);

	return TRUE;
}

typedef struct {
	NetscapeImporter *importer;
	char *parent;
	char *path;
	char *foldername;
} NetscapeCreateDirectoryData;

static void
import_next (NetscapeImporter *importer)
{
	NetscapeCreateDirectoryData *data;

	if (importer->dir_list) {
		char *folder;

		/* Do the next in the list */
		data = importer->dir_list->data;

		folder = g_concat_dir_and_file (data->parent, data->foldername);
		netscape_import_file (importer, data->path, folder);
		g_free (folder);
		g_free (data->parent);
		g_free (data->path);
		g_free (data->foldername);
		g_free (data);
		importer->dir_list = importer->dir_list->next;
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
	char *key, *evolution_dir;

	g_return_if_fail (nsmail_dir != NULL);

	/* Reference our object so when the shell release_unrefs us
	   we will still exist and not go byebye */
	bonobo_object_ref (ii);

	netscape_store_settings (importer);
	evolution_dir = gnome_util_prepend_user_home ("evolution");
	/* Set that we've imported the folders so we won't import them again */
	key = g_strdup_printf ("=%s/config/Importers=/netscape-importers/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	if (importer->do_settings == TRUE) {
		gnome_config_set_bool ("settings-imported", TRUE);
		netscape_import_accounts (importer);
	}

	if (importer->do_mail == TRUE) {
		gnome_config_set_bool ("mail-imported", TRUE);
		/* Scan the nsmail folder and find out what folders 
		   need to be imported */
		scan_dir (importer, "/", nsmail_dir);
		
		/* Import them */
		import_next (importer);
	}

	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();

	if (importer->do_mail == FALSE) {
		/* Destroy it here if we weren't importing mail
		   otherwise the mail importer destroys itself
		   once the mail in imported */
		bonobo_object_unref (ii);
	}
}

static void
netscape_destroy_cb (GtkObject *object,
		     NetscapeImporter *importer)
{
	/* Save the state of the checkboxes */
	g_print ("\n-------Settings-------\n");
	g_print ("Mail - %s\n", importer->do_mail ? "Yes" : "No");
	g_print ("Addressbook - %s\n", importer->do_addrs ? "Yes" : "No");
	g_print ("Filters - %s\n", importer->do_filters ? "Yes" : "No");
	g_print ("Settings - %s\n", importer->do_settings ? "Yes" : "No");

	netscape_store_settings (importer);
	gtk_main_quit ();
}

/* Fun initialisation stuff */

/* Fun with aggregation */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active (tb);
}

static BonoboControl *
create_checkboxes_control (NetscapeImporter *importer)
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

	importer->filters = gtk_check_button_new_with_label (_("Filters"));
	gtk_signal_connect (GTK_OBJECT (importer->filters), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_filters);

	importer->addrs = gtk_check_button_new_with_label (_("Addressbooks"));
	gtk_signal_connect (GTK_OBJECT (importer->addrs), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_addrs);

	sep = gtk_hseparator_new ();

	importer->ask = gtk_check_button_new_with_label (_("Don't ask me again"));
	gtk_signal_connect (GTK_OBJECT (importer->ask), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->ask_again);

	gtk_box_pack_start (GTK_BOX (vbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->settings, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->filters, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->addrs, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->ask, FALSE, FALSE, 0);

	/* Disable the things that can't be done yet :) */
	gtk_widget_set_sensitive (importer->filters, FALSE);
	gtk_widget_set_sensitive (importer->addrs, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail), 
				      importer->do_mail);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->settings),
				      importer->do_settings);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->filters),
				      importer->do_filters);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->addrs),
				      importer->do_addrs);
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
	NetscapeImporter *netscape;
	CORBA_Environment ev;
	char *message = N_("Evolution has found Netscape mail files.\n"
			   "Would you like them to be imported into Evolution?");
	
	netscape = g_new0 (NetscapeImporter, 1);
	netscape_restore_settings (netscape);

	CORBA_exception_init (&ev);
	netscape->importer = oaf_activate_from_id (MBOX_IMPORTER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not start MBox importer\n%s", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	importer = evolution_intelligent_importer_new (netscape_can_import,
						       netscape_create_structure,
						       "Netscape mail", 
						       _(message), netscape);
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (netscape_destroy_cb), netscape);

	control = create_checkboxes_control (netscape);
	bonobo_object_add_interface (BONOBO_OBJECT (importer),
				     BONOBO_OBJECT (control));
	return BONOBO_OBJECT (importer);
}

static void
importer_init (void)
{
	BonoboObject *factory;

	g_print ("Hi");
	factory = bonobo_generic_factory_new (FACTORY_IID, factory_fn, NULL);
	if (factory == NULL) {
		g_error ("Unable to create factory");
		exit (0);
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}

int 
main (int argc,
      char **argv) 
{
	CORBA_ORB orb;

	gnome_init_with_popt_table ("Evolution-Netscape-Importer",
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
