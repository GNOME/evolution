/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <camel/camel.h>
#include <camel/camel-session.h>
#include <camel/camel-file-utils.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <gal/util/e-xml-utils.h>

#include "em-migrate.h"


#define EM_MIGRATE_SESSION_TYPE     (em_migrate_session_get_type ())
#define EM_MIGRATE_SESSION(obj)     (CAMEL_CHECK_CAST((obj), EM_MIGRATE_SESSION_TYPE, EMMigrateSession))
#define EM_MIGRATE_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_MIGRATE_SESSION_TYPE, EMMigrateSessionClass))
#define EM_MIGRATE_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), EM_MIGRATE_SESSION_TYPE))

typedef struct _EMMigrateSession {
	CamelSession parent_object;
	
	CamelStore *store;   /* new folder tree store */
	char *srcdir;        /* old folder tree path */
} EMMigrateSession;

typedef struct _EMMigrateSessionClass {
	CamelSessionClass parent_class;
	
} EMMigrateSessionClass;

static CamelType em_migrate_session_get_type (void);
static CamelSession *em_migrate_session_new (const char *path);

static void
class_init (EMMigrateSessionClass *klass)
{
	;
}

static CamelType
em_migrate_session_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_session_get_type (),
			"EMMigrateSession",
			sizeof (EMMigrateSession),
			sizeof (EMMigrateSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			NULL,
			NULL);
	}
	
	return type;
}

static CamelSession *
em_migrate_session_new (const char *path)
{
	CamelSession *session;
	
	session = CAMEL_SESSION (camel_object_new (EM_MIGRATE_SESSION_TYPE));
	
	camel_session_construct (session, path);
	
	return session;
}


static GtkWidget *window;
static GtkLabel *label;
static GtkProgressBar *progress;

static void
em_migrate_setup_progress_dialog (void)
{
	GtkWidget *vbox, *hbox, *w;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *) window, _("Migrating..."));
	gtk_window_set_modal ((GtkWindow *) window, TRUE);
	gtk_container_set_border_width ((GtkContainer *) window, 6);
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add ((GtkContainer *) window, vbox);
	
	w = gtk_label_new (_("The location and hierarchy of the Evolution mailbox "
			     "folders has changed since Evolution 1.x.\n\nPlease be "
			     "patient while Evolution migrates your folders..."));
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_widget_show (w);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, w);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start_defaults ((GtkBox *) vbox, hbox);
	
	label = (GtkLabel *) gtk_label_new ("");
	gtk_widget_show ((GtkWidget *) label);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) label);
	
	progress = (GtkProgressBar *) gtk_progress_bar_new ();
	gtk_widget_show ((GtkWidget *) progress);
	gtk_box_pack_start_defaults ((GtkBox *) hbox, (GtkWidget *) progress);
	
	gtk_widget_show (window);
}

static void
em_migrate_close_progress_dialog (void)
{
	gtk_widget_destroy ((GtkWidget *) window);
}

static void
em_migrate_set_folder_name (const char *folder_name)
{
	char *text;
	
	text = g_strdup_printf (_("Migrating `%s':"), folder_name);
	gtk_label_set_text (label, text);
	g_free (text);
	
	gtk_progress_bar_set_fraction (progress, 0.0);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
em_migrate_set_progress (double percent)
{
	char text[5];
	
	snprintf (text, sizeof (text), "%d%%", (int) (percent * 100.0f));
	
	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}


static gboolean
is_mail_folder (const char *metadata)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	char *type;
	
	if (!(doc = xmlParseFile (metadata))) {
		g_warning ("Cannot parse `%s'", metadata);
		return FALSE;
	}
	
	if (!(node = xmlDocGetRootElement (doc))) {
		g_warning ("`%s' corrupt: document contains no root node", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	if (!node->name || strcmp (node->name, "efolder") != 0) {
		g_warning ("`%s' corrupt: root node is not 'efolder'", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "type")) {
			type = xmlNodeGetContent (node);
			if (!strcmp (type, "mail")) {
				xmlFreeDoc (doc);
				xmlFree (type);
				
				return TRUE;
			}
			
			xmlFree (type);
			
			break;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	return FALSE;
}

static CamelStore *
get_local_store (CamelSession *session, const char *dirname, const char *metadata, char **namep, int *index, CamelException *ex)
{
	char *protocol, *name, *buf;
	CamelStore *store;
	struct stat st;
	xmlNodePtr node;
	xmlDocPtr doc;
	
	if (stat (metadata, &st) == -1 || !S_ISREG (st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' is not a regular file", metadata);
		return NULL;
	}
	
	if (!(doc = xmlParseFile (metadata))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "cannot parse `%s'", metadata);
		return NULL;
	}
	
	if (!(node = xmlDocGetRootElement (doc)) || strcmp (node->name, "folderinfo") != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' is malformed", metadata);
		xmlFreeDoc (doc);
		return NULL;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "folder")) {
			protocol = xmlGetProp (node, "type");
			name = xmlGetProp (node, "name");
			buf = xmlGetProp (node, "index");
			if (buf != NULL) {
				*index = atoi (buf);
				xmlFree (buf);
			} else {
				*index = 0;
			}
			
			xmlFreeDoc (doc);
			
			buf = g_strdup_printf ("%s:%s", protocol, dirname);
			xmlFree (protocol);
			
			if ((store = camel_session_get_store (session, buf, ex)))
				*namep = g_strdup (name);
			else
				*namep = NULL;
			
			xmlFree (name);
			
			return store;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, "`%s' does not contain needed info", metadata);
	
	return NULL;
}

static void
em_migrate_dir (EMMigrateSession *session, const char *dirname, const char *full_name)
{
	guint32 flags = CAMEL_STORE_FOLDER_CREATE;
	CamelFolder *old_folder, *new_folder;
	CamelStore *local_store;
	struct dirent *dent;
	CamelException ex;
	char *path, *name;
	GPtrArray *uids;
	struct stat st;
	int index, i;
	DIR *dir;
	
	path = g_strdup_printf ("%s/folder-metadata.xml", dirname);
	if (stat (path, &st) == -1 || !S_ISREG (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!is_mail_folder (path)) {
		g_free (path);
		
		goto try_subdirs;
	}
	
	g_free (path);
	
	camel_exception_init (&ex);
	
	/* get old store & folder */
	path = g_strdup_printf ("%s/local-metadata.xml", dirname);
	if (!(local_store = get_local_store ((CamelSession *) session, dirname, path, &name, &index, &ex))) {
		g_warning ("error opening old store for `%s': %s", full_name, ex.desc);
		camel_exception_clear (&ex);
		g_free (path);
		
		/* try subfolders anyway? */
		goto try_subdirs;
	}
	
	g_free (path);
	
	if (!(old_folder = camel_store_get_folder (local_store, name, 0, &ex))) {
		g_warning ("error opening old folder `%s': %s", full_name, ex.desc);
		camel_object_unref (local_store);
		camel_exception_clear (&ex);
		g_free (name);
		
		/* try subfolders anyway? */
		goto try_subdirs;
	}
	
	g_free (name);
	
	flags |= (index ? CAMEL_STORE_FOLDER_BODY_INDEX : 0);
	if (!(new_folder = camel_store_get_folder (session->store, full_name, flags, &ex))) {
		g_warning ("error creating new mbox folder `%s': %s", full_name, ex.desc);
		camel_object_unref (local_store);
		camel_object_unref (old_folder);
		camel_exception_clear (&ex);
		
		/* try subfolders anyway? */
		goto try_subdirs;
	}
	
	em_migrate_set_folder_name (full_name);
	
	uids = camel_folder_get_uids (old_folder);
	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *message;
		CamelMessageInfo *info;
		
		if (!(info = camel_folder_get_message_info (old_folder, uids->pdata[i])))
			continue;
		
		if (!(message = camel_folder_get_message (old_folder, uids->pdata[i], &ex))) {
			camel_folder_free_message_info (old_folder, info);
			break;
		}
		
		camel_folder_append_message (new_folder, message, info, NULL, &ex);
		camel_folder_free_message_info (old_folder, info);
		camel_object_unref (message);
		
		if (camel_exception_is_set (&ex))
			break;
		
		em_migrate_set_progress (((double) i + 1) / ((double) uids->len));
	}
	camel_folder_free_uids (old_folder, uids);
	
	if (camel_exception_is_set (&ex)) {
		g_warning ("error migrating folder `%s': %s", full_name, ex.desc);
		camel_object_unref (local_store);
		camel_object_unref (old_folder);
		camel_object_unref (new_folder);
		camel_exception_clear (&ex);
		
		/* try subfolders anyway? */
		goto try_subdirs;
	}
	
	/*camel_object_unref (local_store);*/
	camel_object_unref (old_folder);
	camel_object_unref (new_folder);
	
 try_subdirs:
	
	path = g_strdup_printf ("%s/subfolders", dirname);
	if (stat (path, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!(dir = opendir (path))) {
		g_warning ("cannot open `%s': %s", path, strerror (errno));
		g_free (path);
		return;
	}
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", path, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		name = g_strdup_printf ("%s/%s", full_name, dent->d_name);
		em_migrate_dir (session, full_path, name);
		g_free (full_path);
		g_free (name);
	}
	
	closedir (dir);
	
	g_free (path);
}

static void
em_migrate_local_folders (EMMigrateSession *session)
{
	struct dirent *dent;
	struct stat st;
	DIR *dir;
	
	if (!(dir = opendir (session->srcdir))) {
		g_warning ("cannot open `%s': %s", session->srcdir, strerror (errno));
		return;
	}
	
	em_migrate_setup_progress_dialog ();
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", session->srcdir, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		em_migrate_dir (session, full_path, dent->d_name);
		g_free (full_path);
	}
	
	closedir (dir);
	
	em_migrate_close_progress_dialog ();
}


static xmlNodePtr
xml_find_node (xmlNodePtr parent, const char *name)
{
	xmlNodePtr node;
	
	node = parent->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, name))
			return node;
		
		node = node->next;
	}
	
	return NULL;
}

static char *
em_migrate_uri (const char *uri)
{
	char *path, *prefix, *p;
	CamelURL *url;
	
	if (!strncmp (uri, "file:", 5)) {
		url = camel_url_new (uri, NULL);
		camel_url_set_protocol (url, "email");
		camel_url_set_user (url, "local");
		camel_url_set_host (url, "local");
		
		prefix = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
		g_assert (strncmp (url->path, prefix, strlen (prefix)) == 0);
		path = g_strdup (url->path + strlen (prefix));
		g_free (prefix);
		
		/* modify the path in-place */
		p = path + strlen (path) - 12;
		while (p > path) {
			if (!strncmp (p, "/subfolders/", 12))
				memmove (p, p + 11, strlen (p + 11) + 1);
			
			p--;
		}
		
		camel_url_set_path (url, path);
		g_free (path);
		
		path = camel_url_to_string (url, 0);
		camel_url_free (url);
		
		return path;
	} else {
		return em_uri_from_camel (uri);
	}
}

static int
em_migrate_filter_file (const char *evolution_dir, const char *filename, CamelException *ex)
{
	char *path, *uri, *new;
	xmlNodePtr node;
	xmlDocPtr doc;
	int retval;
	
	path = g_strdup_printf ("%s/evolution/%s", g_get_home_dir (), filename);
	
	if (!(doc = xmlParseFile (path))) {
		/* can't parse - this means nothing to upgrade */
		g_free (path);
		return 0;
	}
	
	g_free (path);
	
	if (!(node = xmlDocGetRootElement (doc))) {
		/* document contains no root node - nothing to upgrade */
		xmlFreeDoc (doc);
		return 0;
	}
	
	if (!node->name || strcmp (node->name, "filteroptions") != 0) {
		/* root node is not <filteroptions>, nothing to upgrade */
		xmlFreeDoc (doc);
		return 0;
	}
	
	if (!(node = xml_find_node (node, "ruleset"))) {
		/* no ruleset node, nothing to upgrade */
		xmlFreeDoc (doc);
		return 0;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "rule")) {
			xmlNodePtr actionset, part, val, n;
			
			if ((actionset = xml_find_node (node, "actionset"))) {
				/* filters.xml */
				part = actionset->children;
				while (part != NULL) {
					if (part->name && !strcmp (part->name, "part")) {
						val = part->children;
						while (val != NULL) {
							if (val->name && !strcmp (val->name, "value")) {
								char *type;
								
								type = xmlGetProp (val, "type");
								if (type && !strcmp (type, "folder")) {
									if ((n = xml_find_node (val, "folder"))) {
										uri = xmlGetProp (n, "uri");
										new = em_migrate_uri (uri);
										xmlFree (uri);
										
										xmlSetProp (n, "uri", new);
										g_free (new);
									}
								}
								
								xmlFree (type);
							}
							
							val = val->next;
						}
					}
					
					part = part->next;
				}
			} else if ((actionset = xml_find_node (node, "sources"))) {
				/* vfolders.xml */
				n = actionset->children;
				while (n != NULL) {
					if (n->name && !strcmp (n->name, "folder")) {
						uri = xmlGetProp (n, "uri");
						new = em_uri_from_camel (uri);
						xmlFree (uri);
						
						xmlSetProp (n, "uri", new);
						g_free (new);
					}
					
					n = n->next;
				}
			}
		}
		
		node = node->next;
	}
	
	path = g_strdup_printf ("%s/mail/%s", evolution_dir, filename);
	if ((retval = e_xml_save_file (path, doc)) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to migrate `%s': %s"),
				      filename, g_strerror (errno));
	}
	
	g_free (path);
	
	xmlFreeDoc (doc);
	
	return retval;
}


int
em_migrate (MailComponent *component, CamelException *ex)
{
	const char *evolution_dir;
	EMMigrateSession *session;
	CamelException lex;
	struct stat st;
	char *path;
	
	evolution_dir = mail_component_peek_base_directory (component);
	path = g_strdup_printf ("%s/mail", evolution_dir);
	if (stat (path, &st) == -1) {
		if (errno != ENOENT || camel_mkdir (path, 0777) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM, 
					      _("Failed to create directory `%s': %s"),
					      path, g_strerror (errno));
			g_free (path);
			return -1;
		}
	}
	
	camel_init (path, TRUE);
	session = (EMMigrateSession *) em_migrate_session_new (path);
	g_free (path);
	
	session->srcdir = g_strdup_printf ("%s/evolution/local", g_get_home_dir ());
	
	path = g_strdup_printf ("mbox:%s/.evolution/mail/local", g_get_home_dir ());
	if (stat (path + 5, &st) == -1) {
		if (errno != ENOENT || camel_mkdir (path + 5, 0777) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to create directory `%s': %s"),
					      path + 5, g_strerror (errno));
			g_free (session->srcdir);
			camel_object_unref (session);
			g_free (path);
			return -1;
		}
	}
	
	camel_exception_init (&lex);
	if (!(session->store = camel_session_get_store ((CamelSession *) session, path, &lex))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to open store for `%s': %s"),
				      path, lex.desc);
		g_free (session->srcdir);
		camel_object_unref (session);
		camel_exception_clear (&lex);
		g_free (path);
		return -1;
	}
	g_free (path);
	
	em_migrate_local_folders (session);
	
	camel_object_unref (session->store);
	g_free (session->srcdir);
	
	camel_object_unref (session);
	
	if (em_migrate_filter_file (evolution_dir, "filters.xml", ex) == -1)
		return -1;
	
	if (em_migrate_filter_file (evolution_dir, "vfolders.xml", ex) == -1)
		return -1;
	
	return 0;
}
