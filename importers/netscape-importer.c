/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* netscape-importer.c
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
#include <pwd.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>

#include <glib.h>
#include <gnome.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>

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
/*
  GtkWidget *addrs;
  gboolean do_addrs;
  GtkWidget *filters;
  gboolean do_filters;
*/
	GtkWidget *settings;
	gboolean do_settings;

	Bonobo_ConfigDatabase db;

	/* GUI */
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progressbar;
} NetscapeImporter;

static void import_next (NetscapeImporter *importer);

static GtkWidget *
create_importer_gui (NetscapeImporter *importer)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("Evolution is importing your old Netscape data"), GNOME_MESSAGE_BOX_INFO, NULL);
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
netscape_store_settings (NetscapeImporter *importer)
{
	bonobo_config_set_boolean (importer->db, "/Importer/Netscape/mail", 
				   importer->do_mail, NULL);
	bonobo_config_set_boolean (importer->db, "/Importer/Netscape/settings",
				   importer->do_settings, NULL);
}

static void
netscape_restore_settings (NetscapeImporter *importer)
{
	importer->do_mail = FALSE;
	importer->do_settings = FALSE;
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

	d(g_warning ("Found key: %s", key));
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

	d(g_warning ("Found value: %s", value));
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

		if (*line == 0) {
			continue;
		}

		if (*line == '/' && line[1] == '/') {
			continue;
		}

		key = netscape_get_key (line);
		value = netscape_get_value (line);

		if (key == NULL)
			continue;

		g_hash_table_insert (user_prefs, key, value);
	}

	return;
}

static char *
get_user_fullname (void)
{
	char *uname, *gecos, *special;
	struct passwd *pwd;

	uname = getenv ("USER");
	pwd = getpwnam (uname);

	if (strcmp (pwd->pw_gecos, "") == 0) {
		return g_strdup (uname);
	}

	special = strchr (pwd->pw_gecos, ',');
	if (special == NULL) {
		gecos = g_strdup (pwd->pw_gecos);
	} else {
		gecos = g_strndup (pwd->pw_gecos, special - pwd->pw_gecos);
	}

	special = strchr (gecos, '&');
	if (special == NULL) {
		return gecos;
	} else {
		char *capname, *expanded, *noamp;

		capname = g_strdup (uname);
		capname[0] = toupper ((int) capname[0]);
		noamp = g_strndup (gecos, special - gecos - 1);
		expanded = g_strconcat (noamp, capname, NULL);

		g_free (noamp);
		g_free (capname);
		g_free (gecos);

		return expanded;
	}
}

static void
netscape_import_accounts (NetscapeImporter *importer)
{
	char *username;
	const char *nstr;
	const char *imap;
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
	if (nstr != NULL) {
		username = g_strdup (nstr);
	} else {
		username = get_user_fullname ();
	}

	id.name = CORBA_string_dup (username);
	nstr = netscape_get_string ("mail.identity.useremail");
	id.address = CORBA_string_dup (nstr ? nstr : "");
	nstr = netscape_get_string ("mail.identity.organization");
	id.organization = CORBA_string_dup (nstr ? nstr : "");
	nstr = netscape_get_string ("mail.signature_file");
	/* FIXME rodo id.signature = CORBA_string_dup (nstr ? nstr : "");
	id.html_signature = CORBA_string_dup ("");
	id.has_html_signature = FALSE; */

	/* Create transport */
	nstr = netscape_get_string ("network.hosts.smtp_server");
	if (nstr != NULL) {
		char *url;
		const char *nstr2;

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
	} else {
		transport.url = CORBA_string_dup ("");
		transport.keep_on_server = FALSE;
		transport.auto_check = FALSE;
		transport.auto_check_time = 0;
		transport.save_passwd = FALSE;
		transport.enabled = FALSE;
	}

	/* Create account */
	account.name = CORBA_string_dup (username);
	account.id = id;
	account.transport = transport;

	account.drafts_folder_uri = CORBA_string_dup ("");
	account.sent_folder_uri = CORBA_string_dup ("");

	/* Create POP3 source */
	nstr = netscape_get_string ("network.hosts.pop_server");
	if (nstr != NULL && *nstr != 0) {
		char *url;
		gboolean bool;
		const char *nstr2;

		nstr2 = netscape_get_string ("mail.pop_name");
		if (nstr2) {
			url = g_strconcat ("pop://", nstr2, "@", nstr, NULL);
		} else {
			url = g_strconcat ("pop://", nstr, NULL);
		}
		source.url = CORBA_string_dup (url);
		bool = netscape_get_boolean ("mail.leave_on_server");
		g_warning ("mail.leave_on_server: %s", bool ? "true" : "false");
		source.keep_on_server = netscape_get_boolean ("mail.leave_on_server");
		source.auto_check = TRUE;
		source.auto_check_time = 10;
		bool = netscape_get_boolean ("mail.remember_password");
		g_warning ("mail.remember_password: %s", bool ? "true" : "false");
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
				char *serverstr, *name, *url;
				const char *username;

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
		} else {
			char *url, *path;

			/* Using Movemail */
			path = getenv ("MAIL");
			url = g_strconcat ("mbox://", path, NULL);
			source.url = CORBA_string_dup (url);
			g_free (url);

			source.keep_on_server = netscape_get_boolean ("mail.leave_on_server");
			source.auto_check = TRUE;
			source.auto_check_time = 10;
			source.save_passwd = netscape_get_boolean ("mail.remember_password");
			source.enabled = FALSE;
		}
	}
	account.source = source;

	GNOME_Evolution_MailConfig_addAccount (objref, &account, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error setting account: %s", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return;
	}
	
	g_free (username);
	CORBA_exception_free (&ev);
}

static gboolean
is_dir_empty (const char *path)
{
	DIR *base;
	struct stat buf;
	struct dirent *contents;

	base = opendir (path);
	if (base == NULL) {
		return TRUE; /* Can't open dir */
	}
	
	contents = readdir (base);
	while (contents != NULL) {
		char *fullpath;
		
		if (strcmp (contents->d_name, ".") == 0 ||
		    strcmp (contents->d_name, "..") == 0) {
			contents = readdir (base);
			continue;
		}

		fullpath = g_concat_dir_and_file (path, contents->d_name);
		stat (fullpath, &buf);
		if (S_ISDIR (buf.st_mode)) {
			gboolean sub;

			sub = is_dir_empty (fullpath);
			if (sub == FALSE) {
				g_free (fullpath);
				closedir (base);
				return FALSE;
			}
		} else {
			/* File */
			if (buf.st_size != 0) {
				g_free (fullpath);
				closedir (base);
				return FALSE;
			}
		}

		g_free (fullpath);
		contents = readdir (base);
	}

	closedir (base);
	return TRUE;
}

static gboolean
netscape_can_import (EvolutionIntelligentImporter *ii,
		     void *closure)
{
	NetscapeImporter *importer = closure;
	gboolean mail, settings;

	if (user_prefs == NULL) {
		netscape_init_prefs ();
	}

	if (user_prefs == NULL) {
		d(g_warning ("No netscape dir"));
		return FALSE;
	}

	nsmail_dir = g_hash_table_lookup (user_prefs, "mail.directory");
	if (nsmail_dir == NULL) {
		return FALSE;
	} else {
		return !is_dir_empty (nsmail_dir);
	}
}

static gboolean
importer_timeout_fn (gpointer data)
{
	NetscapeImporter *importer = (NetscapeImporter *) data;
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
	NetscapeImporter *importer = (NetscapeImporter *) data;
	CORBA_Object objref;
	CORBA_Environment ev;

	if (result == EVOLUTION_IMPORTER_NOT_READY ||
	    result == EVOLUTION_IMPORTER_BUSY) {
		gtk_timeout_add (5000, importer_timeout_fn, data);
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
	char *str;

	/* Do import */
	d(g_warning ("Importing %s as %s\n", path, folderpath));

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
		g_warning ("Exception here: %s", CORBA_exception_id (&ev));
		CORBA_Object_release (importer->importer, &ev);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	importer->listener = evolution_importer_listener_new (importer_cb, 
							      importer);
	objref = bonobo_object_corba_objref (BONOBO_OBJECT (importer->listener));
	d(g_print ("%s:Processing...\n", __FUNCTION__));
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
	char *str;

	nsmail = opendir (dirname);
	if (nsmail == NULL) {
		d(g_warning ("Could not open %s\nopendir returned: %s", 
			     dirname, g_strerror (errno)));
		return;
	}

	str = g_strdup_printf (_("Scanning %s"), dirname);
	gtk_label_set_text (GTK_LABEL (importer->label), str);
	g_free (str);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
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
	CORBA_Environment ev;
	NetscapeImporter *importer = closure;

	g_return_if_fail (nsmail_dir != NULL);

	/* Reference our object so when the shell release_unrefs us
	   we will still exist and not go byebye */
	bonobo_object_ref (BONOBO_OBJECT (ii));

	netscape_store_settings (importer);

	/* Create a dialog if we're going to be active */
	if (importer->do_settings == TRUE ||
	    importer->do_mail == TRUE) {
		importer->dialog = create_importer_gui (importer);
		gtk_widget_show_all (importer->dialog);
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
	}

	if (importer->do_settings == TRUE) {
		bonobo_config_set_boolean (importer->db, 
                "/Importer/Netscape/settings-imported", TRUE, NULL);
		netscape_import_accounts (importer);
	}

	if (importer->do_mail == TRUE) {
		bonobo_config_set_boolean (importer->db, 
                "/Importer/Netscape/mail-imported", TRUE, NULL);
		/* Scan the nsmail folder and find out what folders 
		   need to be imported */
		gtk_label_set_text (GTK_LABEL (importer->label), 
				    _("Scanning directory"));
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}

		scan_dir (importer, "/", nsmail_dir);
		
		/* Import them */
		gtk_label_set_text (GTK_LABEL (importer->label),
				    _("Starting import"));
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
		import_next (importer);
	}

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_sync (importer->db, &ev);
	CORBA_exception_free (&ev);

	if (importer->do_mail == FALSE) {
		/* Destroy it here if we weren't importing mail
		   otherwise the mail importer destroys itself
		   once the mail in imported */
		bonobo_object_unref (BONOBO_OBJECT (ii));
	}
}

static void
netscape_destroy_cb (GtkObject *object,
		     NetscapeImporter *importer)
{
	CORBA_Environment ev;

	netscape_store_settings (importer);

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
	GtkWidget *hbox;
	BonoboControl *control;

	hbox = gtk_hbox_new (FALSE, 2);

	importer->mail = gtk_check_button_new_with_label (_("Mail"));
	gtk_signal_connect (GTK_OBJECT (importer->mail), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_mail);

	importer->settings = gtk_check_button_new_with_label (_("Settings"));
	gtk_signal_connect (GTK_OBJECT (importer->settings), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_settings);

	gtk_box_pack_start (GTK_BOX (hbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), importer->settings, FALSE, FALSE, 0);

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
	NetscapeImporter *netscape;
	CORBA_Environment ev;
	char *message = N_("Evolution has found Netscape mail files.\n"
			   "Would you like them to be imported into Evolution?");
	
	netscape = g_new0 (NetscapeImporter, 1);

	CORBA_exception_init (&ev);

	netscape->db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", 
					  &ev);

	if (BONOBO_EX (&ev) || netscape->db == CORBA_OBJECT_NIL) {
		g_free (netscape);
		CORBA_exception_free (&ev);
		return NULL;
 	}

	netscape_restore_settings (netscape);

	netscape->importer = oaf_activate_from_id (MBOX_IMPORTER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not start MBox importer\n%s", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	importer = evolution_intelligent_importer_new (netscape_can_import,
						       netscape_create_structure,
						       "Netscape", 
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
	BonoboGenericFactory *factory;

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

	bindtextdomain(PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain(PACKAGE);
	
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
