/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-mail.c
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <liboaf/liboaf.h>
#include <gal/widgets/e-unicode.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h> /* gnome_util_prepend_user_home */
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>

#include <Evolution.h>
#include <evolution-storage-listener.h>

#include "Mail.h"
#include "e-summary.h"
#include "e-summary-mail.h"
#include "e-summary-table.h"

#include "e-util/e-path.h"

#define MAIL_IID "OAFIID:GNOME_Evolution_FolderInfo"

struct _ESummaryMail {
	GNOME_Evolution_FolderInfo folder_info;
	BonoboListener *listener;
	EvolutionStorageListener *storage_listener;

	GHashTable *folders;
	GList *shown;
	ESummaryMailMode mode;

	char *html;
};

typedef struct _ESummaryMailFolder {
	char *name;
	char *path;

	int count;
	int unread;

	gboolean init; /* Has this folder been initialised? */
} ESummaryMailFolder;

/* Work out what to do with folder names */
static char *
make_pretty_foldername (ESummary *summary,
			const char *foldername)
{
	char *pretty;

	if (summary->preferences->show_full_path == FALSE) {
		if ((pretty = strrchr (foldername, '/'))) {
			return g_strdup (pretty + 1);
		} else {
			return g_strdup (foldername);
		}
	} else {
		return g_strdup (foldername);
	}
}

static void
folder_gen_html (ESummary *summary,
		 ESummaryMailFolder *folder,
		 GString *string)
{
	char *str, *pretty_name, *uri;
	
	pretty_name = make_pretty_foldername (summary, folder->name);
	uri = g_strconcat ("evolution:/local", folder->name, NULL); 
	str = g_strdup_printf ("<tr><td><a href=\"%s\"><pre>%s</pre></a></td><td align=\"Left\"><pre>%d/%d</pre></td></tr>", 
			       uri, pretty_name, folder->unread, folder->count);
	g_free (uri);
	g_string_append (string, str);
	g_free (pretty_name);
	g_free (str);
}

static void
e_summary_mail_generate_html (ESummary *summary)
{
	ESummaryMail *mail;
	GString *string;
	GList *p;
	char *s, *old;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	mail = summary->mail;
	string = g_string_new ("<dl><dt><img src=\"myevo-mail-summary.png\" "
	                       "align=\"middle\" alt=\"\" width=\"48\" "
	                       "height=\"48\"> <b><a href=\"evolution:/local/Inbox\">");
	s = e_utf8_from_locale_string (_("Mail summary"));
	g_string_append (string, s);
	g_free (s);
	g_string_append (string, "</a></b></dt><dd><table numcols=\"2\" width=\"100%\">");
	
	for (p = mail->shown; p; p = p->next) {
		folder_gen_html (summary, p->data, string);
	}

	g_string_append (string, "</table></dd></dl>");

	old = mail->html;
	mail->html = string->str;

	g_free (old);

	g_string_free (string, FALSE);
}

const char *
e_summary_mail_get_html (ESummary *summary)
{
	/* Only regenerate HTML when it's needed */
	e_summary_mail_generate_html (summary);
	
	if (summary->mail == NULL) {
		return NULL;
	}

	return summary->mail->html;
}

static void
e_summary_mail_get_info (ESummaryMail *mail,
			 const char *uri,
			 BonoboListener *listener)
{
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;

	g_return_if_fail (mail != NULL);
	g_return_if_fail (mail->folder_info != CORBA_OBJECT_NIL);

	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));
	CORBA_exception_init (&ev);
	GNOME_Evolution_FolderInfo_getInfo (mail->folder_info, uri ? uri : "",
					    corba_listener, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error getting info for %s:\n%s", uri,
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
	return;
}

static void
new_folder_cb (EvolutionStorageListener *listener,
	       const char *path,
	       const GNOME_Evolution_Folder *folder,
	       ESummary *summary)
{
	ESummaryMail *mail;
	ESummaryMailFolder *mail_folder;
	GList *p;

	/* Don't care about non mail */
	if (strcmp (folder->type, "mail") != 0 ||
	    strncmp (folder->physicalUri, "file://", 7) != 0) {
		return;
	}

	mail = summary->mail;

	mail_folder = g_new (ESummaryMailFolder, 1);
	mail_folder->path = g_strdup (folder->physicalUri);
	mail_folder->name = g_strdup (path);
	mail_folder->count = -1;
	mail_folder->unread = -1;
	mail_folder->init = FALSE;

	g_hash_table_insert (mail->folders, mail_folder->path, mail_folder);

	for (p = summary->preferences->display_folders; p; p = p->next) {
		char *uri;

		uri = g_strconcat ("file://", p->data, NULL);
		if (strcmp (uri, folder->physicalUri) == 0) {
			mail->shown = g_list_append (mail->shown, mail_folder);
			e_summary_mail_get_info (mail, mail_folder->path, 
						 mail->listener);
		}
		g_free (uri);
	}
}

static void
update_folder_cb (EvolutionStorageListener *listener,
		  const char *path,
		  int unread_count,
		  ESummary *summary)
{
	char *evolution_dir;
	static char *proto = NULL;
	char *uri;

	/* Make this static, saves having to recompute it each time */
	if (proto == NULL) {
		evolution_dir = gnome_util_prepend_user_home ("evolution/local");

		proto = g_strconcat ("file://", evolution_dir, NULL);
		g_free (evolution_dir);
	}
	uri = e_path_to_physical (proto, path);

	e_summary_mail_get_info (summary->mail, uri, summary->mail->listener);

	g_free (uri);
}

static void
remove_folder_cb (EvolutionStorageListener *listener,
		  const char *path,
		  ESummary *summary)
{
	ESummaryMail *mail;
	ESummaryMailFolder *mail_folder;
	GList *p;

	mail = summary->mail;
	mail_folder = g_hash_table_lookup (mail->folders, path);
	if (mail_folder == NULL) {
		return;
	}

	/* Check if we're displaying it, because we can't display it if it
	   doesn't exist :) */
	for (p = mail->shown; p; p = p->next) {
		if (p->data == mail_folder) {
			mail->shown = g_list_remove_link (mail->shown, p);
			g_list_free (p);
		}
	}

	g_hash_table_remove (mail->folders, path);
	g_free (mail_folder->name);
	g_free (mail_folder->path);
	g_free (mail_folder);
}

static void
mail_change_notify (BonoboListener *listener,
		    const char *name,
		    const BonoboArg *arg,
		    CORBA_Environment *ev,
		    ESummary *summary)
{
	GNOME_Evolution_FolderInfo_MessageCount *count;
	ESummaryMail *mail;
	ESummaryMailFolder *folder;
	GList *p;

	mail = summary->mail;

	g_return_if_fail (mail != NULL);

	count = arg->_value;
	folder = g_hash_table_lookup (mail->folders, count->path);

	if (folder == NULL) {
		return;
	}

	folder->count = count->count;
	folder->unread = count->unread;
	folder->init = TRUE;

	/* Are we displaying this folder? */
	for (p = summary->preferences->display_folders; p; p = p->next) {
		char *uri;
		
		uri = g_strconcat ("file://", p->data, NULL);
		if (strcmp (uri, folder->path) == 0) {
			e_summary_draw (summary);

			g_free (uri);
			return;
		}

		g_free (uri);
	}
}

static void
e_summary_mail_protocol (ESummary *summary,
			 const char *uri,
			 void *closure)
{
}

static gboolean
e_summary_mail_register_storage (ESummary *summary,
				 GNOME_Evolution_Storage corba_storage)
{
	ESummaryMail *mail;
	EvolutionStorageListener *listener;
	GNOME_Evolution_StorageListener corba_listener;
	CORBA_Environment ev;

	mail = summary->mail;

	if (mail->storage_listener == NULL) {
		mail->storage_listener = evolution_storage_listener_new ();

		gtk_signal_connect (GTK_OBJECT (mail->storage_listener), "new-folder",
				    GTK_SIGNAL_FUNC (new_folder_cb), summary);
		gtk_signal_connect (GTK_OBJECT (mail->storage_listener), "removed-folder",
				    GTK_SIGNAL_FUNC (remove_folder_cb), summary);
		gtk_signal_connect (GTK_OBJECT (mail->storage_listener), "update_folder",
				    GTK_SIGNAL_FUNC (update_folder_cb), summary);
	}
	listener = mail->storage_listener;
	
	corba_listener = evolution_storage_listener_corba_objref (listener);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_addListener (corba_storage, corba_listener, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Exception adding listener: %s", 
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		
		g_free (mail);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

static gboolean
e_summary_mail_register_storages (ESummary *summary,
				  GNOME_Evolution_Shell corba_shell)
{
	GNOME_Evolution_Storage local_storage;
	CORBA_Environment ev;

	g_return_val_if_fail (summary != NULL, FALSE);
	g_return_val_if_fail (IS_E_SUMMARY (summary), FALSE);

	CORBA_exception_init (&ev);
	local_storage = GNOME_Evolution_Shell_getLocalStorage (corba_shell, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Exception getting local storage: %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		
		return FALSE;
	}
	CORBA_exception_free (&ev);

	if (e_summary_mail_register_storage (summary, local_storage))
		return TRUE;
	else
		return FALSE;
}

void
e_summary_mail_init (ESummary *summary,
		     GNOME_Evolution_Shell corba_shell)
{
	ESummaryMail *mail;
	CORBA_Environment ev;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	mail = g_new0 (ESummaryMail, 1);
	summary->mail = mail;

	mail->html = NULL;
	CORBA_exception_init (&ev);
	mail->folder_info = oaf_activate_from_id (MAIL_IID, 0, NULL, &ev);
	if (BONOBO_EX (&ev) || mail->folder_info == NULL) {
		g_warning ("Exception creating FolderInfo: %s", 
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);

		return;
	}

	/* Create a BonoboListener for all the notifies. */
	mail->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (mail->listener), "event-notify",
			    GTK_SIGNAL_FUNC (mail_change_notify), summary);

	/* Create a hash table for the folders */
	mail->folders = g_hash_table_new (g_str_hash, g_str_equal);
	mail->shown = NULL;

	e_summary_mail_register_storages (summary, corba_shell);	
	e_summary_add_protocol_listener (summary, "mail", e_summary_mail_protocol, mail);
	return;
}

void
e_summary_mail_reconfigure (ESummary *summary)
{
	ESummaryMail *mail;
	GList *old, *p;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));
	
	mail = summary->mail;
	old = mail->shown;
	mail->shown = NULL;

	for (p = g_list_last (summary->preferences->display_folders); p; p = p->prev) {
		ESummaryMailFolder *folder;
		char *uri;

		if (strncmp (p->data, "file://", 7) == 0) {
			uri = g_strdup (p->data);
		} else {
			uri = g_strconcat ("file://", p->data, NULL);
		}
		folder = g_hash_table_lookup (mail->folders, uri);
		if (folder != NULL) {
			if (folder->init == FALSE) {
				e_summary_mail_get_info (mail, folder->path, 
							 mail->listener);
			}
			mail->shown = g_list_append (mail->shown, folder);
		}

		g_free (uri);
	}

	/* Free the old list */
	g_list_free (old);

	e_summary_mail_generate_html (summary);
	e_summary_draw (summary);
}

static void
free_row_data (gpointer data)
{
	ESummaryMailRowData *rd = data;

	g_free (rd->name);
	g_free (rd->uri);
	g_free (rd);
}

static void
hash_to_list (gpointer key,
	      gpointer value,
	      gpointer data)
{
	ESummaryMailRowData *rd;
	ESummaryMailFolder *folder;
	GList **p;

	p = (GList **) data;
	folder = (ESummaryMailFolder *) value;

	rd = g_new (ESummaryMailRowData, 1);
	rd->name = g_strdup (folder->name);
	rd->uri = g_strdup (key);

	*p = g_list_prepend (*p, rd);
}

static int
str_compare (gconstpointer a,
	     gconstpointer b)
{
	ESummaryMailRowData *rda, *rdb;

	rda = (ESummaryMailRowData *) a;
	rdb = (ESummaryMailRowData *) b;
	return strcmp (rda->name, rdb->name);
}

static char *
get_parent_path (const char *path)
{
	char *last;

	last = strrchr (path, '/');
	return g_strndup (path, last - path);
}

static gboolean
is_folder_shown (ESummaryMail *mail,
		 const char *path)
{
	GList *p;

	for (p = mail->shown; p; p = p->next) {
		ESummaryMailFolder *folder = p->data;
		if (strcmp (folder->path, path) == 0) {
			return TRUE;
		}
	}
	
	return FALSE;
}

static ETreePath
insert_path_recur (ESummaryTable *est,
		   GHashTable *hash_table,
		   const char *path,
		   ESummaryMail *mail)
{
	char *parent_path, *name;
	ETreePath parent_node, node;
	ESummaryTableModelEntry *entry;
	static char *toplevel = NULL;
	
	parent_path = get_parent_path (path);

	if (toplevel == NULL) {
		char *tmp;
		
		tmp = gnome_util_prepend_user_home ("evolution/local");
		toplevel = g_strconcat ("file://", tmp, NULL);
		g_free (tmp);
	}

	parent_node = g_hash_table_lookup (hash_table, parent_path);
	if (parent_node == NULL) {
		if (strcmp (toplevel, path) == 0) {
			/* Insert root */
			return NULL;
		} else {
			parent_node = insert_path_recur (est, hash_table, parent_path, mail);
		}
	}

	g_free (parent_path);
	name = strrchr (path, '/');

	/* Leave out folders called "subfolder" */
	if (strcmp (name + 1, "subfolders") == 0) {
		return parent_node;
	}
	
	node = e_summary_table_add_node (est, parent_node, 0, NULL);
	entry = g_new (ESummaryTableModelEntry, 1);
	entry->path = node;
	entry->location = g_strdup (path);
	entry->name = g_strdup (name + 1);
	entry->editable = TRUE;
	entry->removable = FALSE;

	/* Check if shown */
	entry->shown = is_folder_shown (mail, path);
	g_hash_table_insert (est->model, entry->path, entry);
	g_hash_table_insert (hash_table, g_strdup (path), node);

	return node;
}

static void
free_path_hash (gpointer key,
		gpointer value,
		gpointer data)
{
	g_free (key);
}

void
e_summary_mail_fill_list (ESummaryTable *est,
			  ESummary *summary)
{
	ESummaryMail *mail;
	GList *names, *p;
	GHashTable *path_hash;

	g_return_if_fail (IS_E_SUMMARY_TABLE (est));
	g_return_if_fail (IS_E_SUMMARY (summary));
	
	mail = summary->mail;
	if (mail == NULL) {
		return;
	}

	names = NULL;
	g_hash_table_foreach (mail->folders, hash_to_list, &names);

	path_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	names = g_list_sort (names, str_compare);
	for (p = names; p; p = p->next) {
		ESummaryMailRowData *rd = p->data;
		insert_path_recur (est, path_hash, rd->uri, mail);
		free_row_data (rd);
	}

	/* Free everything */
	g_list_free (names);
	g_hash_table_foreach (path_hash, free_path_hash, NULL);
	g_hash_table_destroy (path_hash);
}

const char *
e_summary_mail_uri_to_name (ESummary *summary,
			    const char *uri)
{
	ESummaryMailFolder *folder;

	folder = g_hash_table_lookup (summary->mail->folders, uri);
	if (folder == NULL) {
		return NULL;
	} else {
		return folder->name;
	}
}
	
static void
free_folder (gpointer key,
	     gpointer value,
	     gpointer data)
{
	ESummaryMailFolder *folder = value;
	
	g_free (folder->name);
	g_free (folder->path);
	g_free (folder);
}

void
e_summary_mail_free (ESummary *summary)
{
	ESummaryMail *mail;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	mail = summary->mail;
	bonobo_object_release_unref (mail->folder_info, NULL);
	mail->folder_info = CORBA_OBJECT_NIL;

	gtk_signal_disconnect_by_func (GTK_OBJECT (mail->listener),
				       GTK_SIGNAL_FUNC (mail_change_notify), summary);
	bonobo_object_unref (BONOBO_OBJECT (mail->listener));

	g_hash_table_foreach (mail->folders, free_folder, NULL);
	g_hash_table_destroy (mail->folders);

	g_free (mail->html);

	gtk_signal_disconnect_by_func (GTK_OBJECT (mail->storage_listener),
				       GTK_SIGNAL_FUNC (new_folder_cb), summary);
	gtk_signal_disconnect_by_func (GTK_OBJECT (mail->storage_listener),
				       GTK_SIGNAL_FUNC (remove_folder_cb), summary);
	gtk_signal_disconnect_by_func (GTK_OBJECT (mail->storage_listener),
				       GTK_SIGNAL_FUNC (update_folder_cb), summary);

	g_free (mail);
	summary->mail = NULL;
}
