/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-local.c: Local mailbox support. */

/* 
 * Authors: 
 *  Michael Zucchi <NotZed@helixcode.com>
 *  Peter Williams <peterw@helixcode.com>
 *  Ettore Perazzoli <ettore@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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

/*
  TODO:

  If we are going to have all this LocalStore stuff, then the LocalStore
  should have a reconfigure_folder method on it, as, in reality, it is
  the maintainer of this information.

*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo.h>
#include <libgnomeui/gnome-dialog.h>
#include <glade/glade.h>
#include <gnome-xml/xmlmemory.h>

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-shell-component.h"
#include "evolution-storage-listener.h"

#include "camel/camel.h"

#include "filter/vfolder-context.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-editor.h"

#include "mail.h"
#include "mail-local.h"
#include "mail-tools.h"
#include "mail-threads.h"
#include "folder-browser.h"
#include "mail-mt.h"

#define d(x)


/* Local folder metainfo */

struct _local_meta {
	char *path;		/* path of metainfo file */

	char *format;		/* format of mailbox */
	char *name;		/* name of mbox itself */
	int indexed;		/* do we index the body? */
};

static struct _local_meta *
load_metainfo(const char *path)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	struct _local_meta *meta;

	meta = g_malloc0(sizeof(*meta));
	meta->path = g_strdup(path);

	d(printf("Loading folder metainfo from : %s\n", meta->path));

	doc = xmlParseFile(meta->path);
	if (doc == NULL) {
		goto dodefault;
	}
	node = doc->root;
	if (strcmp(node->name, "folderinfo")) {
		goto dodefault;
	}
	node = node->childs;
	while (node) {
		if (!strcmp(node->name, "folder")) {
			char *index, *txt;

			txt = xmlGetProp(node, "type");
			meta->format = g_strdup (txt ? txt : "mbox");
			xmlFree (txt);

			txt = xmlGetProp(node, "name");
			meta->name = g_strdup (txt ? txt : "mbox");
			xmlFree (txt);

			index = xmlGetProp(node, "index");
			if (index) {
				meta->indexed = atoi(index);
				xmlFree(index);
			} else
				meta->indexed = TRUE;
			
		}
		node = node->next;
	}
	xmlFreeDoc(doc);
	return meta;

dodefault:
	meta->format = g_strdup("mbox"); /* defaults */
	meta->name = g_strdup("mbox");
	meta->indexed = TRUE;
	if (doc)
		xmlFreeDoc(doc);
	return meta;
}

static void
free_metainfo(struct _local_meta *meta)
{
	g_free(meta->path);
	g_free(meta->format);
	g_free(meta->name);
	g_free(meta);
}

static int
save_metainfo(struct _local_meta *meta)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;
	int ret;

	d(printf("Saving folder metainfo to : %s\n", meta->path));

	doc = xmlNewDoc("1.0");
	root = xmlNewDocNode(doc, NULL, "folderinfo", NULL);
	xmlDocSetRootElement(doc, root);

	node  = xmlNewChild(root, NULL, "folder", NULL);
	xmlSetProp(node, "type", meta->format);
	xmlSetProp(node, "name", meta->name);
	xmlSetProp(node, "index", meta->indexed?"1":"0");

	ret = xmlSaveFile(meta->path, doc);
	xmlFreeDoc(doc);
	return ret;
}


/* Local folder reconfiguration stuff */

/*
   open new
   copy old->new
   close old
   rename old oldsave
   rename new old
   open oldsave
   delete oldsave

   close old
   rename oldtmp
   open new
   open oldtmp
   copy oldtmp new
   close oldtmp
   close oldnew

*/

static void
update_progress(char *fmt, float percent)
{
	if (fmt)
		mail_status(fmt);
	/*mail_op_set_percentage (percent);*/
}

/* ******************** */

typedef struct reconfigure_folder_input_s {
	FolderBrowser *fb;
	gchar *newtype;
	GtkWidget *frame;
	GtkWidget *apply;
	GtkWidget *cancel;
	GtkOptionMenu *optionlist;
} reconfigure_folder_input_t;

static gchar *
describe_reconfigure_folder (gpointer in_data, gboolean gerund)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	if (gerund)
		return g_strdup_printf (_("Changing folder \"%s\" to \"%s\" format"),
					input->fb->uri,
					input->newtype);
	else
		return g_strdup_printf (_("Change folder \"%s\" to \"%s\" format"),
					input->fb->uri,
					input->newtype);
}

static void
setup_reconfigure_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	if (!IS_FOLDER_BROWSER (input->fb)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Input has a bad FolderBrowser in reconfigure_folder");
		return;
	}

	if (!input->newtype) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No new folder type in reconfigure_folder");
		return;
	}

	gtk_object_ref (GTK_OBJECT (input->fb));
}

static void
do_reconfigure_folder(gpointer in_data, gpointer op_data, CamelException *ex)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	CamelStore *fromstore = NULL, *tostore = NULL;
	char *fromurl = NULL, *tourl = NULL;
	CamelFolder *fromfolder = NULL, *tofolder = NULL;
	GPtrArray *uids;
	int i;
	char *metapath;
	char *tmpname;
	CamelURL *url = NULL;
	struct _local_meta *meta;
	guint32 flags;

	d(printf("reconfiguring folder: %s to type %s\n", input->fb->uri, input->newtype));

	mail_status_start(_("Reconfiguring folder"));

	/* NOTE: This var is cleared by the folder_browser via the set_uri method */
	input->fb->reconfigure = TRUE;

	/* get the actual location of the mailbox */
	url = camel_url_new(input->fb->uri, ex);
	if (camel_exception_is_set(ex)) {
		g_warning("%s is not a workable url!", input->fb->uri);
		goto cleanup;
	}

	metapath = g_strdup_printf("%s/local-metadata.xml", url->path);
	meta = load_metainfo(metapath);
	g_free(metapath);

	/* first, 'close' the old folder */
	if (input->fb->folder != NULL) {
		update_progress(_("Closing current folder"), 0.0);

		camel_folder_sync(input->fb->folder, FALSE, ex);
		camel_object_unref (CAMEL_OBJECT (input->fb->folder));
		input->fb->folder = NULL;
	}

	camel_url_set_protocol (url, meta->format);
	fromurl = camel_url_to_string (url, FALSE);
	camel_url_set_protocol (url, input->newtype);
	tourl = camel_url_to_string (url, FALSE);

	d(printf("opening stores %s and %s\n", fromurl, tourl));

	fromstore = camel_session_get_store(session, fromurl, ex);

	if (camel_exception_is_set(ex))
		goto cleanup;

	tostore = camel_session_get_store(session, tourl, ex);
	if (camel_exception_is_set(ex))
		goto cleanup;

	/* rename the old mbox and open it again, without indexing */
	tmpname = g_strdup_printf("%s_reconfig", meta->name);
	d(printf("renaming %s to %s, and opening it\n", meta->name, tmpname));
	update_progress(_("Renaming old folder and opening"), 0.0);

	camel_store_rename_folder(fromstore, meta->name, tmpname, ex);
	if (camel_exception_is_set(ex)) {
		goto cleanup;
	}
	
	/* we dont need to set the create flag ... or need an index if it has one */
	fromfolder = camel_store_get_folder(fromstore, tmpname, 0, ex);
	if (fromfolder == NULL || camel_exception_is_set(ex)) {
		/* try and recover ... */
		camel_exception_clear (ex);
		camel_store_rename_folder(fromstore, tmpname, meta->name, ex);
		goto cleanup;
	}

	/* create a new mbox */
	d(printf("Creating the destination mbox\n"));
	update_progress(_("Creating new folder"), 0.0);

	flags = CAMEL_STORE_FOLDER_CREATE;
	if (meta->indexed)
		flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
	tofolder = camel_store_get_folder(tostore, meta->name, flags, ex);
	if (tofolder == NULL || camel_exception_is_set(ex)) {
		d(printf("cannot open destination folder\n"));
		/* try and recover ... */
		camel_exception_clear (ex);
		camel_store_rename_folder(fromstore, tmpname, meta->name, ex);
		goto cleanup;
	}

	update_progress(_("Copying messages"), 0.0);
	uids = camel_folder_get_uids(fromfolder);
	for (i=0;i<uids->len;i++) {
		mail_statusf("Copying message %d of %d", i, uids->len);
		camel_folder_move_message_to(fromfolder, uids->pdata[i], tofolder, ex);
		if (camel_exception_is_set(ex)) {
			camel_folder_free_uids(fromfolder, uids);
			goto cleanup;
		}
	}
	camel_folder_free_uids(fromfolder, uids);
	camel_folder_expunge(fromfolder, ex);

	d(printf("delete old mbox ...\n"));
	camel_store_delete_folder(fromstore, tmpname, ex);

	/* switch format */
	g_free(meta->format);
	meta->format = g_strdup(input->newtype);
	if (save_metainfo(meta) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot save folder metainfo; "
					"you'll probably find you can't\n"
					"open this folder anymore: %s"),
				      tourl);
	}
	free_metainfo(meta);

	/* and unref our copy of the new folder ... */
 cleanup:
	if (tofolder)
		camel_object_unref (CAMEL_OBJECT (tofolder));
	if (fromfolder)
		camel_object_unref (CAMEL_OBJECT (fromfolder));
	if (fromstore)
		camel_object_unref (CAMEL_OBJECT (fromstore));
	if (tostore)
		camel_object_unref (CAMEL_OBJECT (tostore));
	g_free(fromurl);
	g_free(tourl);
	if (url)
		camel_url_free (url);
}

static void
cleanup_reconfigure_folder  (gpointer in_data, gpointer op_data, CamelException *ex)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;
	char *uri;

	if (camel_exception_is_set(ex)) {
		GtkWidget *win = gtk_widget_get_ancestor((GtkWidget *)input->frame, GTK_TYPE_WINDOW);
		gnome_error_dialog_parented (_("If you can no longer open this mailbox, then\n"
					       "you may need to repair it manually."), GTK_WINDOW (win));
	}

	/* force a reload of the newly formatted folder */
	d(printf("opening new source\n"));
	uri = g_strdup(input->fb->uri);
	folder_browser_set_uri(input->fb, uri);
	g_free(uri);

	mail_status_end();

	gtk_object_unref (GTK_OBJECT (input->fb));
	g_free (input->newtype);
}

static const mail_operation_spec op_reconfigure_folder =
{
	describe_reconfigure_folder,
	0,
	setup_reconfigure_folder,
	do_reconfigure_folder,
	cleanup_reconfigure_folder
};

static void
reconfigure_clicked(GnomeDialog *d, int button, reconfigure_folder_input_t *data)
{
	if (button == 0) {
		GtkMenu *menu;
		int type;
		char *types[] = { "mbox", "maildir", "mh" };

		menu = (GtkMenu *)gtk_option_menu_get_menu(data->optionlist);
		type = g_list_index(GTK_MENU_SHELL(menu)->children, gtk_menu_get_active(menu));
		if (type < 0 || type > 2)
			type = 0;

		gtk_widget_set_sensitive(data->frame, FALSE);
		gtk_widget_set_sensitive(data->apply, FALSE);
		gtk_widget_set_sensitive(data->cancel, FALSE);

		data->newtype = g_strdup (types[type]);
		mail_operation_queue (&op_reconfigure_folder, data, TRUE);
	}

	if (button != -1)
		gnome_dialog_close(d);
}

void
mail_local_reconfigure_folder(FolderBrowser *fb)
{
	CamelStore *store;
	GladeXML *gui;
	GnomeDialog *gd;
	reconfigure_folder_input_t *data;

	if (fb->folder == NULL) {
		g_warning("Trying to reconfigure nonexistant folder");
		return;
	}

	data = g_new (reconfigure_folder_input_t, 1);

	store = camel_folder_get_parent_store(fb->folder);

	gui = glade_xml_new(EVOLUTION_GLADEDIR "/local-config.glade", "dialog_format");
	gd = (GnomeDialog *)glade_xml_get_widget (gui, "dialog_format");

	data->frame = glade_xml_get_widget (gui, "frame_format");
	data->apply = glade_xml_get_widget (gui, "apply_format");
	data->cancel = glade_xml_get_widget (gui, "cancel_format");
	data->optionlist = (GtkOptionMenu *)glade_xml_get_widget (gui, "option_format");
	data->newtype = NULL;
	data->fb = fb;

	gtk_label_set_text((GtkLabel *)glade_xml_get_widget (gui, "label_format"),
			   ((CamelService *)store)->url->protocol);

	gtk_signal_connect((GtkObject *)gd, "clicked", reconfigure_clicked, data);
	gtk_object_unref((GtkObject *)gui);

	gnome_dialog_run_and_close (GNOME_DIALOG (gd));
}



/* MailLocalStore implementation */
#define MAIL_LOCAL_STORE_TYPE     (mail_local_store_get_type ())
#define MAIL_LOCAL_STORE(obj)     (CAMEL_CHECK_CAST((obj), MAIL_LOCAL_STORE_TYPE, MailLocalStore))
#define MAIL_LOCAL_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_LOCAL_STORE_TYPE, MailLocalStoreClass))
#define MAIL_IS_LOCAL_STORE(o)    (CAMEL_CHECK_TYPE((o), MAIL_LOCAL_STORE_TYPE))

typedef struct {
	CamelStore parent_object;	

	GNOME_Evolution_LocalStorage corba_local_storage;
	EvolutionStorageListener *local_storage_listener;

	char *local_path;
	int local_pathlen;
	GHashTable *folders, /* points to MailLocalFolder */
		*unread;
} MailLocalStore;

typedef struct {
	CamelStoreClass parent_class;
} MailLocalStoreClass;

typedef struct {
	CamelFolder *folder;
	MailLocalStore *local_store;
	char *path, *name;
	int last_unread;
} MailLocalFolder;

static void local_folder_changed_proxy (CamelObject *folder, gpointer event_data, gpointer user_data);

CamelType mail_local_store_get_type (void);

static char *get_name(CamelService *service, gboolean brief);
static CamelFolder *get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static void delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);
static void rename_folder(CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
static char *get_folder_name(CamelStore *store, const char *folder_name, CamelException *ex);
static CamelFolder *lookup_folder(CamelStore *store, const char *folder_name);

static CamelStoreClass *local_parent_class;

static void
mail_local_store_class_init (MailLocalStoreClass *mail_local_store_class)
{
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (mail_local_store_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (mail_local_store_class);

	/* virtual method overload */
	camel_service_class->get_name = get_name;

	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->get_folder_name = get_folder_name;
	camel_store_class->lookup_folder = lookup_folder;

	local_parent_class = (CamelStoreClass *)camel_type_get_global_classfuncs(camel_store_get_type ());
}

static void
mail_local_store_init (gpointer object, gpointer klass)
{
	MailLocalStore *local_store = MAIL_LOCAL_STORE (object);

	local_store->corba_local_storage = CORBA_OBJECT_NIL;
}

static void
free_local_folder(MailLocalFolder *lf)
{
	if (lf->folder) {
		camel_object_unhook_event((CamelObject *)lf->folder,
					  "folder_changed", local_folder_changed_proxy,
					  lf);
		camel_object_unhook_event((CamelObject *)lf->folder,
					  "message_changed", local_folder_changed_proxy,
					  lf);
		camel_object_unref((CamelObject *)lf->folder);
	}
	g_free(lf->path);
	g_free(lf->name);
	camel_object_unref((CamelObject *)lf->local_store);
}

static void
free_folder (gpointer key, gpointer data, gpointer user_data)
{
	MailLocalFolder *lf = data;

	g_free(key);
	free_local_folder(lf);
}

static void
mail_local_store_finalize (gpointer object)
{
	MailLocalStore *local_store = MAIL_LOCAL_STORE (object);
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	if (!CORBA_Object_is_nil (local_store->corba_local_storage, &ev)) {
		Bonobo_Unknown_unref (local_store->corba_local_storage, &ev);
		CORBA_Object_release (local_store->corba_local_storage, &ev);
	}
	CORBA_exception_free (&ev);

	if (local_store->local_storage_listener)
		gtk_object_unref (GTK_OBJECT (local_store->local_storage_listener));

	g_hash_table_foreach (local_store->folders, free_folder, NULL);
	g_hash_table_destroy (local_store->folders);

	g_free (local_store->local_path);
}

CamelType
mail_local_store_get_type (void)
{
	static CamelType mail_local_store_type = CAMEL_INVALID_TYPE;

	if (mail_local_store_type == CAMEL_INVALID_TYPE) {
		mail_local_store_type = camel_type_register (
			CAMEL_STORE_TYPE, "MailLocalStore",
			sizeof (MailLocalStore),
			sizeof (MailLocalStoreClass),
			(CamelObjectClassInitFunc) mail_local_store_class_init,
			NULL,
			(CamelObjectInitFunc) mail_local_store_init,
			(CamelObjectFinalizeFunc) mail_local_store_finalize);
	}

	return mail_local_store_type;
}

/* sigh,
   because of all this LocalStore nonsense, we have to snoop cache hits to find out
   if our local folder type has changed under us (sort of the whole point of most
   of this file, is the storage type of the folder), and then reload the new folder
   to match.

   The only other way would be to poke it even more directly, which seems worse.

   Not sure if the ref stuff is 100%, but its probably no worse than it was.
*/
static CamelFolder *
lookup_folder (CamelStore *store, const char *folder_name)
{
	char *name, *type;
	struct _local_meta *meta;
	MailLocalFolder *local_folder;
	CamelStore *newstore;
	MailLocalStore *local_store = (MailLocalStore *)store;
	CamelFolder *folder;
	CamelException *ex;

	folder = local_parent_class->lookup_folder(store, folder_name);

	d(printf("looking up local folder: %s = %p\n", folder_name, folder));

	if (folder != NULL) {
		type = ((CamelService *)folder->parent_store)->url->protocol;
		name = g_strdup_printf("/%s/local-metadata.xml", folder_name);
		meta = load_metainfo(name);
		g_free(name);
		d(printf("found folder, checking type '%s' against meta '%s'\n", type, meta->format));
		if (strcmp(meta->format, type) != 0) {
			d(printf("ok, mismatch, checking ...\n"));
			local_parent_class->uncache_folder(store, folder);
			local_folder = g_hash_table_lookup(local_store->folders, folder_name);
			if (local_folder) {
				d(printf("we have to update the old folder ...\n"));
				camel_object_unhook_event(CAMEL_OBJECT (local_folder->folder),
							  "folder_changed", local_folder_changed_proxy,
							  local_folder);
				camel_object_unhook_event(CAMEL_OBJECT (local_folder->folder),
							  "message_changed", local_folder_changed_proxy,
							  local_folder);
				camel_object_unref((CamelObject *)local_folder->folder);
				folder = local_folder->folder = NULL;

				ex = camel_exception_new();
				name = g_strdup_printf ("%s:/%s", meta->format, folder_name);
				newstore = camel_session_get_store (session, name, ex);
				d(printf("getting new store %s = %p\n", name, newstore));
				g_free (name);
				if (newstore) {
					guint32 flags = CAMEL_STORE_FOLDER_CREATE;
					if (meta->indexed)
						flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
					folder = local_folder->folder =
						camel_store_get_folder(newstore, meta->name, flags, ex);
					camel_object_unref((CamelObject *)newstore);

					d(printf("we got the new folder: %s : %p\n", folder_name, folder));
					camel_object_hook_event (CAMEL_OBJECT (local_folder->folder),
								 "folder_changed", local_folder_changed_proxy,
								 local_folder);
					camel_object_hook_event (CAMEL_OBJECT (local_folder->folder),
								 "message_changed", local_folder_changed_proxy,
								 local_folder);
				}
				if (folder)
					local_parent_class->cache_folder(store, folder_name, folder);

				camel_exception_free(ex);
			}
		}
		free_metainfo(meta);
	}

	if (folder)
		camel_object_ref((CamelObject *)folder);

	return folder;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name,
	    guint32 flags, CamelException *ex)
{
	MailLocalStore *local_store = (MailLocalStore *)store;
	CamelFolder *folder;
	MailLocalFolder *local_folder;

	local_folder = g_hash_table_lookup (local_store->folders, folder_name);
	if (local_folder) {
		folder = local_folder->folder;
		camel_object_ref (CAMEL_OBJECT (folder));
	} else {
		folder = NULL;
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER, "No such folder %s", folder_name);
	}
	return folder;
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	/* No-op. The shell local storage deals with this. */
}

static void
rename_folder (CamelStore *store, const char *old, const char *new,
	       CamelException *ex)
{
	/* Probable no-op... */
}

static char *
get_folder_name (CamelStore *store, const char *folder_name,
		 CamelException *ex)
{
	return g_strdup (folder_name);
}

static char *
get_name (CamelService *service, gboolean brief)
{
	return g_strdup ("Local mail folders");
}


/* Callbacks for the EvolutionStorageListner signals.  */

static void
local_storage_destroyed_cb (EvolutionStorageListener *storage_listener,
			    void *data)
{
	/* FIXME: Dunno how to handle this yet.  */
	g_warning ("%s -- The LocalStorage has gone?!", __FILE__);
}


static void
local_folder_changed (CamelObject *object, gpointer event_data,
		      gpointer user_data)
{
	MailLocalFolder *local_folder = user_data;
	int unread = GPOINTER_TO_INT (event_data);
	char *display;

	if (unread != local_folder->last_unread) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		if (unread > 0) {
			display = g_strdup_printf ("%s (%d)", local_folder->name, unread);
			GNOME_Evolution_LocalStorage_updateFolder (
				local_folder->local_store->corba_local_storage,
				local_folder->path, display, TRUE, &ev);
			g_free (display);
		} else {
			GNOME_Evolution_LocalStorage_updateFolder (
				local_folder->local_store->corba_local_storage,
				local_folder->path, local_folder->name,
				FALSE, &ev);
		}
		CORBA_exception_free (&ev);

		local_folder->last_unread = unread;
	}
}

static void
local_folder_changed_proxy (CamelObject *folder, gpointer event_data, gpointer user_data)
{
	int unread;

	unread = camel_folder_get_unread_message_count (CAMEL_FOLDER (folder));
	mail_proxy_event (local_folder_changed, folder,
			  GINT_TO_POINTER (unread), user_data);
}

static char *
describe_register_folder (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup (_("Registering local folder"));
	else
		return g_strdup (_("Register local folder"));
}

static void
do_register_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	MailLocalFolder *local_folder = in_data;
	char *name;
	struct _local_meta *meta;
	CamelStore *store;
	guint32 flags;

	name = g_strdup_printf ("/%s/local-metadata.xml", local_folder->name);
	meta = load_metainfo (name);
	g_free (name);

	name = g_strdup_printf ("%s:/%s", meta->format, local_folder->name);
	store = camel_session_get_store (session, name, ex);
	g_free (name);
	if (!store) {
		free_metainfo (meta);
		return;
	}

	flags = CAMEL_STORE_FOLDER_CREATE;
	if (meta->indexed)
		flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
	local_folder->folder = camel_store_get_folder (store, meta->name, flags, ex);
	local_folder->last_unread = camel_folder_get_unread_message_count(local_folder->folder);
	camel_object_unref (CAMEL_OBJECT (store));
	free_metainfo (meta);
}

static void
cleanup_register_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	MailLocalFolder *local_folder = in_data;
	int unread;

	if (!local_folder->folder) {
		free_local_folder(local_folder);
		return;
	}

	g_hash_table_insert (local_folder->local_store->folders, local_folder->name, local_folder);
	local_folder->name = strrchr (local_folder->path, '/') + 1;

	camel_object_hook_event (CAMEL_OBJECT (local_folder->folder),
				 "folder_changed", local_folder_changed_proxy,
				 local_folder);
	camel_object_hook_event (CAMEL_OBJECT (local_folder->folder),
				 "message_changed", local_folder_changed_proxy,
				 local_folder);
	unread = local_folder->last_unread;
	local_folder->last_unread = 0;
	local_folder_changed (CAMEL_OBJECT (local_folder->folder), GINT_TO_POINTER (unread), local_folder);
}

static const mail_operation_spec op_register_folder =
{
	describe_register_folder,
	0,
	NULL,
	do_register_folder,
	cleanup_register_folder
};

static void
local_storage_new_folder_cb (EvolutionStorageListener *storage_listener,
			     const char *path,
			     const GNOME_Evolution_Folder *folder,
			     void *data)
{
	MailLocalStore *local_store = data;
	MailLocalFolder *local_folder;

	if (strcmp (folder->type, "mail") != 0 ||
	    strncmp (folder->physical_uri, "file://", 7) != 0 ||
	    strncmp (folder->physical_uri + 7, local_store->local_path,
		     local_store->local_pathlen) != 0)
		return;

	local_folder = g_new0 (MailLocalFolder, 1);
	local_folder->name = g_strdup (folder->physical_uri + 8);
	local_folder->path = g_strdup (path);
	local_folder->local_store = local_store;
	camel_object_ref((CamelObject *)local_store);

	/* Note: This needs to be synchronous, as that is what the shell
	   expects.  Doesn't that suck. */
	/* This used to be made 'synchronous' by having us wait for
	   outstanding requests, which was BAD */

	/*mail_operation_queue (&op_register_folder, local_folder, FALSE);*/
	{
		CamelException *ex = camel_exception_new();

		do_register_folder(local_folder, NULL, ex);
		cleanup_register_folder(local_folder, NULL, ex);

#if 0
		/* yay, so we can't do this, because we've probably got the bloody
		   splash screen up */
		if (camel_exception_is_set(ex)) {
			char *msg = g_strdup_printf(_("Unable to register folder '%s':\n%s"),
						    path, camel_exception_get_description(ex));
			GnomeDialog *gd = (GnomeDialog *)gnome_error_dialog(msg);
			gnome_dialog_run_and_close(gd);
			g_free(msg);
		}
#endif
		camel_exception_free(ex);
	}
}

static void
local_storage_removed_folder_cb (EvolutionStorageListener *storage_listener,
				 const char *path,
				 void *data)
{
	MailLocalStore *local_store = data;
	MailLocalFolder *local_folder;

	if (strncmp (path, "file://", 7) != 0 ||
	    strncmp (path + 7, local_store->local_path,
		     local_store->local_pathlen) != 0)
		return;

	path += 7 + local_store->local_pathlen;

	local_folder = g_hash_table_lookup (local_store->folders, path);
	if (local_folder) {
		g_hash_table_remove (local_store->folders, path);
		free_local_folder(local_folder);
	}
}

static CamelProvider local_provider = {
	"file", "Local mail", NULL, "mail",
	CAMEL_PROVIDER_IS_STORAGE, CAMEL_URL_NEED_PATH,
	{ 0, 0 }, NULL
};

/* There's only one "file:" store. */
static guint
non_hash (gconstpointer key)
{
	return 0;
}

static gint
non_equal (gconstpointer a, gconstpointer b)
{
	return TRUE;
}

CamelFolder *
mail_local_lookup_folder (const char *name,
			  CamelException *ev)
{
	MailLocalStore *local_store;

	local_store = (MailLocalStore *)camel_session_get_service (session,
								   "file:/",
								   CAMEL_PROVIDER_STORE, NULL);

	return get_folder (CAMEL_STORE(local_store), name, 0, ev);
}

void
mail_local_storage_startup (EvolutionShellClient *shellclient,
			    const char *evolution_path)
{
	MailLocalStore *local_store;
	GNOME_Evolution_StorageListener corba_local_storage_listener;
	CORBA_Environment ev;

	/* Register with Camel to handle file: URLs */
	local_provider.object_types[CAMEL_PROVIDER_STORE] =
		mail_local_store_get_type();

	local_provider.service_cache = g_hash_table_new (non_hash, non_equal);
	camel_session_register_provider (session, &local_provider);


	/* Now build the storage. */
	local_store = (MailLocalStore *)camel_session_get_service (
		session, "file:/", CAMEL_PROVIDER_STORE, NULL);
	if (!local_store) {
		g_warning ("No local store!");
		return;
	}
	local_store->corba_local_storage =
		evolution_shell_client_get_local_storage (shellclient);
	if (local_store->corba_local_storage == CORBA_OBJECT_NIL) {
		g_warning ("No local storage!");
		camel_object_unref (CAMEL_OBJECT (local_store));
		return;
	}

	local_store->local_storage_listener =
		evolution_storage_listener_new ();
	corba_local_storage_listener =
		evolution_storage_listener_corba_objref (
			local_store->local_storage_listener);

	gtk_signal_connect (GTK_OBJECT (local_store->local_storage_listener),
			    "destroyed",
			    GTK_SIGNAL_FUNC (local_storage_destroyed_cb),
			    local_store);
	gtk_signal_connect (GTK_OBJECT (local_store->local_storage_listener),
			    "new_folder",
			    GTK_SIGNAL_FUNC (local_storage_new_folder_cb),
			    local_store);
	gtk_signal_connect (GTK_OBJECT (local_store->local_storage_listener),
			    "removed_folder",
			    GTK_SIGNAL_FUNC (local_storage_removed_folder_cb),
			    local_store);

	local_store->local_path = g_strdup_printf ("%s/local",
						   evolution_path);
	local_store->local_pathlen = strlen (local_store->local_path);

	local_store->folders = g_hash_table_new (g_str_hash, g_str_equal);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_addListener (local_store->corba_local_storage,
					corba_local_storage_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot add a listener to the Local Storage.");
		camel_object_unref (CAMEL_OBJECT (local_store));
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);
}
