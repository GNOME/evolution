/* 
 * e-summary-mail.c: Mail summary bit.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#include <liboaf/liboaf.h>

#include "Mail.h"
#include "e-summary.h"
#include "e-summary-mail.h"

#include <gtk/gtksignal.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>

#include <Evolution.h>
#include <evolution-storage-listener.h>

#define MAIL_IID "OAFIID:GNOME_Evolution_FolderInfo"

struct _ESummaryMail {
	GNOME_Evolution_FolderInfo folder_info;
	BonoboListener *listener;

	GHashTable *folders;
	ESummaryMailMode mode;

	char *html;
};

typedef struct _ESummaryMailFolder {
	char *name;
	char *path;

	int count;
	int unread;
} ESummaryMailFolder;

const char *
e_summary_mail_get_html (ESummary *summary)
{
	if (summary->mail == NULL) {
		return NULL;
	}

	return summary->mail->html;
}

/* Work out what to do with folder names */
static char *
make_pretty_foldername (const char *foldername)
{
	char *pretty;

	if ((pretty = strrchr (foldername, '/'))) {
		return g_strdup (pretty + 1);
	} else {
		return g_strdup (foldername);
	}
}

static void
folder_gen_html (gpointer key,
		 gpointer value,
		 gpointer user_data)
{
	GString *string = user_data;
	ESummaryMailFolder *folder = value;
	char *str, *pretty_name, *uri;
	
	pretty_name = make_pretty_foldername (folder->name);
	uri = g_strconcat ("evolution:", folder->name, NULL); 
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

	mail = summary->mail;
	string = g_string_new ("<dl><dt><img src=\"ico-mail.png\" "
			       "align=\"middle\" alt=\"\" width=\"48\" "
			       "height=\"48\"> <b><a href=\"evolution:/local/Inbox\">Mail summary</a>"
			       "</b></dt><dd><table numcols=\"2\" width=\"100%\">");
	
	g_hash_table_foreach (mail->folders, folder_gen_html, string);

	g_string_append (string, "</table></dd></dl>");
	mail->html = string->str;
	g_string_free (string, FALSE);
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

	/* Don't care about none mail */
	if (strcmp (folder->type, "mail") != 0 ||
	    strncmp (folder->physical_uri, "file://", 7) != 0) {
		return;
	}

	mail = summary->mail;

	mail_folder = g_new (ESummaryMailFolder, 1);
	mail_folder->path = g_strdup (folder->physical_uri);
	mail_folder->name = g_strdup (path);
	mail_folder->count = -1;
	mail_folder->unread = -1;

	g_hash_table_insert (mail->folders, mail_folder->path, mail_folder);
	e_summary_mail_get_info (mail, mail_folder->path, mail->listener);
}

static void
remove_folder_cb (EvolutionStorageListener *listener,
		  const char *path,
		  ESummary *summary)
{
	ESummaryMail *mail;
	ESummaryMailFolder *mail_folder;

	mail = summary->mail;
	mail_folder = g_hash_table_lookup (mail->folders, path);
	if (mail_folder == NULL) {
		return;
	}

	g_hash_table_remove (mail->folders, path);
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

	mail = summary->mail;

	count = arg->_value;
	folder = g_hash_table_lookup (mail->folders, count->path);

	if (folder == NULL) {
		return;
	}

	folder->count = count->count;
	folder->unread = count->unread;

	/* Regen HTML */
	e_summary_mail_generate_html (summary);
	e_summary_draw (summary);
}

static void
e_summary_mail_protocol (ESummary *summary,
			 const char *uri,
			 void *closure)
{
}

void
e_summary_mail_init (ESummary *summary,
		     GNOME_Evolution_Shell corba_shell)
{
	ESummaryMail *mail;
	CORBA_Environment ev;
	GNOME_Evolution_LocalStorage local_storage;
	EvolutionStorageListener *listener;
	GNOME_Evolution_StorageListener corba_listener;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	mail = g_new (ESummaryMail, 1);
	summary->mail = mail;

	CORBA_exception_init (&ev);
	mail->folder_info = oaf_activate_from_id (MAIL_IID, 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Exception creating FolderInfo: %s", 
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);

		g_free (mail);
		return;
	}

	/* Create a hash table for the folders */
	mail->folders = g_hash_table_new (g_str_hash, g_str_equal);
	
	/* Create a BonoboListener for all the notifies. */
	mail->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (mail->listener), "event-notify",
			    GTK_SIGNAL_FUNC (mail_change_notify), summary);

	local_storage = GNOME_Evolution_Shell_getLocalStorage (corba_shell, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Exception getting local storage: %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		
		g_free (mail);
		return;
	}

	listener = evolution_storage_listener_new ();
	gtk_signal_connect (GTK_OBJECT (listener), "new-folder",
			    GTK_SIGNAL_FUNC (new_folder_cb), summary);
	gtk_signal_connect (GTK_OBJECT (listener), "removed-folder",
			    GTK_SIGNAL_FUNC (remove_folder_cb), summary);
	corba_listener = evolution_storage_listener_corba_objref (listener);

	GNOME_Evolution_Storage_addListener (local_storage, corba_listener, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Exception adding listener: %s", 
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		
		g_free (mail);
		return;
	}

	CORBA_exception_free (&ev);

	e_summary_add_protocol_listener (summary, "mail", e_summary_mail_protocol, mail);
	return;
}
