/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-component.c
 *
 * Copyright (C) 2003  Ximian Inc.
 *
 * This  program is free  software; you  can redistribute  it and/or
 * modify it under the terms of version 2  of the GNU General Public
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "e-storage.h"
#include "e-storage-set.h"
#include "e-storage-browser.h"
#include "e-storage-set-view.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"

#include "folder-browser-factory.h"
#include "mail-config.h"
#include "mail-component.h"
#include "mail-folder-cache.h"
#include "mail-vfolder.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-send-recv.h"
#include "mail-session.h"

#include "em-popup.h"
#include "em-utils.h"

#include <gtk/gtklabel.h>

#include <e-util/e-mktemp.h>

#include <gal/e-table/e-tree.h>
#include <gal/e-table/e-tree-memory.h>

#include <camel/camel.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>


#define MESSAGE_RFC822_TYPE   "message/rfc822"
#define TEXT_URI_LIST_TYPE    "text/uri-list"
#define UID_LIST_TYPE         "x-uid-list"
#define FOLDER_TYPE           "x-folder"

/* Drag & Drop types */
enum DndDragType {
	DND_DRAG_TYPE_FOLDER,          /* drag an evo folder */
	DND_DRAG_TYPE_TEXT_URI_LIST,   /* drag to an mbox file */
};

enum DndDropType {
	DND_DROP_TYPE_UID_LIST,        /* drop a list of message uids */
	DND_DROP_TYPE_FOLDER,          /* drop an evo folder */
	DND_DROP_TYPE_MESSAGE_RFC822,  /* drop a message/rfc822 stream */
	DND_DROP_TYPE_TEXT_URI_LIST,   /* drop an mbox file */
};

static GtkTargetEntry drag_types[] = {
	{ UID_LIST_TYPE,       0, DND_DRAG_TYPE_FOLDER         },
	{ TEXT_URI_LIST_TYPE,  0, DND_DRAG_TYPE_TEXT_URI_LIST  },
};

static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static GtkTargetEntry drop_types[] = {
	{ UID_LIST_TYPE,       0, DND_DROP_TYPE_UID_LIST       },
	{ FOLDER_TYPE,         0, DND_DROP_TYPE_FOLDER         },
	{ MESSAGE_RFC822_TYPE, 0, DND_DROP_TYPE_MESSAGE_RFC822 },
	{ TEXT_URI_LIST_TYPE,  0, DND_DROP_TYPE_TEXT_URI_LIST  },
};

static const int num_drop_types = sizeof (drop_types) / sizeof (drop_types[0]);


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _MailComponentPrivate {
	char *base_directory;

	MailAsyncEvent *async_event;
	GHashTable *storages_hash; /* storage by store */

	EFolderTypeRegistry *folder_type_registry;
	EStorageSet *storage_set;

	RuleContext *search_context;

	char *context_path;	/* current path for right-click menu */

	CamelStore *local_store;
};

static int emc_tree_right_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, MailComponent *component);

/* Utility functions.  */

/* EPFIXME: Eeek, this totally sucks.  See comment in e-storage.h,
   async_open_folder() should NOT be a signal.  */

struct _StorageConnectedData {
	EStorage *storage;
	char *path;
	EStorageDiscoveryCallback callback;
	void *callback_data;
};
typedef struct _StorageConnectedData StorageConnectedData;

static void
storage_connected_callback (CamelStore *store,
			    CamelFolderInfo *info,
			    StorageConnectedData *data)
{
	EStorageResult result;

	if (info != NULL)
		result = E_STORAGE_OK;
	else
		result = E_STORAGE_GENERICERROR;

	(* data->callback) (data->storage, result, data->path, data->callback_data);

	g_object_unref (data->storage);
	g_free (data->path);
	g_free (data);
}

static void
storage_async_open_folder_callback (EStorage *storage,
				    const char *path,
				    EStorageDiscoveryCallback callback,
				    void *callback_data,
				    CamelStore *store)
{
	StorageConnectedData *storage_connected_data = g_new0 (StorageConnectedData, 1);

	g_object_ref (storage);

	storage_connected_data->storage = storage;
	storage_connected_data->path = g_strdup (path);
	storage_connected_data->callback = callback;
	storage_connected_data->callback_data = callback_data;

	mail_note_store (store, NULL, storage,
			 (void *) storage_connected_callback, storage_connected_data);
}

static void
add_storage (MailComponent *component,
	     const char *name,
	     CamelService *store,
	     CamelException *ex)
{
	EStorage *storage;
	EFolder *root_folder;

	root_folder = e_folder_new (name, "noselect", "");
	storage = e_storage_new (name, root_folder);
	e_storage_declare_has_subfolders(storage, "/", _("Connecting..."));

	camel_object_ref(store);

	g_object_set_data((GObject *)storage, "em-store", store);
	g_hash_table_insert (component->priv->storages_hash, store, storage);

	g_signal_connect(storage, "async_open_folder",
			 G_CALLBACK (storage_async_open_folder_callback), store);

#if 0	/* Some private test code - zed */
	{
		static void *model;

		if (model == NULL) {
			model = em_store_model_new();
			em_store_model_view_new(model);
		}

		em_store_model_add_store(model, store);
	}
#endif

#if 0
	/* EPFIXME these are not needed anymore.  */
	g_signal_connect(storage, "create_folder", G_CALLBACK(storage_create_folder), store);
	g_signal_connect(storage, "remove_folder", G_CALLBACK(storage_remove_folder), store);
	g_signal_connect(storage, "xfer_folder", G_CALLBACK(storage_xfer_folder), store);
#endif

	e_storage_set_add_storage (component->priv->storage_set, storage);

	mail_note_store ((CamelStore *) store, NULL, storage, NULL, NULL);

	g_object_unref (storage);
}

static void
load_accounts(MailComponent *component, EAccountList *accounts)
{
	EIterator *iter;
	
	/* Load each service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */
	
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccountService *service;
		EAccount *account;
		const char *name;
		
		account = (EAccount *) e_iterator_get (iter);
		service = account->source;
		name = account->name;

		if (account->enabled && service->url != NULL)
			mail_component_load_storage_by_uri (component, service->url, name);
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
}

static inline gboolean
type_is_mail (const char *type)
{
	return !strcmp (type, "mail") || !strcmp (type, "mail/public");
}

static inline gboolean
type_is_vtrash (const char *type)
{
	return !strcmp (type, "vtrash");
}

static void
storage_go_online (gpointer key, gpointer value, gpointer data)
{
	CamelStore *store = key;
	CamelService *service = CAMEL_SERVICE (store);

	if (! (service->provider->flags & CAMEL_PROVIDER_IS_REMOTE)
	    || (service->provider->flags & CAMEL_PROVIDER_IS_EXTERNAL))
		return;

	if ((CAMEL_IS_DISCO_STORE (service)
	     && camel_disco_store_status (CAMEL_DISCO_STORE (service)) == CAMEL_DISCO_STORE_OFFLINE)
	    || service->status != CAMEL_SERVICE_DISCONNECTED) {
		mail_store_set_offline (store, FALSE, NULL, NULL);
		mail_note_store (store, NULL, NULL, NULL, NULL);
	}
}

static void
go_online (MailComponent *component)
{
	camel_session_set_online(session, TRUE);
	mail_session_set_interactive(TRUE);
	mail_component_storages_foreach(component, storage_go_online, NULL);
}

static void
setup_search_context (MailComponent *component)
{
	MailComponentPrivate *priv = component->priv;
	char *user = g_strdup_printf ("%s/evolution/searches.xml", g_get_home_dir ()); /* EPFIXME should be somewhere else. */
	char *system = g_strdup (EVOLUTION_PRIVDATADIR "/vfoldertypes.xml");
	
	priv->search_context = rule_context_new ();
	g_object_set_data_full (G_OBJECT (priv->search_context), "user", user, g_free);
	g_object_set_data_full (G_OBJECT (priv->search_context), "system", system, g_free);
	
	rule_context_add_part_set (priv->search_context, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
	
	rule_context_add_rule_set (priv->search_context, "ruleset", filter_rule_get_type (),
				   rule_context_add_rule, rule_context_next_rule);
	
	rule_context_load (priv->search_context, system, user);
}

/* Local store setup.  */
char *default_drafts_folder_uri;
CamelFolder *drafts_folder = NULL;
char *default_sent_folder_uri;
CamelFolder *sent_folder = NULL;
char *default_outbox_folder_uri;
CamelFolder *outbox_folder = NULL;
char *default_inbox_folder_uri;
CamelFolder *inbox_folder = NULL;

static struct {
	char *base;
	char **uri;
	CamelFolder **folder;
} default_folders[] = {
	{ "Inbox", &default_inbox_folder_uri, &inbox_folder },
	{ "Drafts", &default_drafts_folder_uri, &drafts_folder },
	{ "Outbox", &default_outbox_folder_uri, &outbox_folder },
	{ "Sent", &default_sent_folder_uri, &sent_folder },
};

static void
setup_local_store(MailComponent *component)
{
	MailComponentPrivate *p = component->priv;
	CamelException ex;
	char *store_uri;
	int i;

	g_assert(p->local_store == NULL);

	/* EPFIXME It should use base_directory once we have moved it.  */
	store_uri = g_strconcat("mbox:", g_get_home_dir(), "/.evolution/mail/local", NULL);
	p->local_store = mail_component_load_storage_by_uri(component, store_uri, _("On this Computer"));
	camel_object_ref(p->local_store);
	
	camel_exception_init (&ex);
	for (i=0;i<sizeof(default_folders)/sizeof(default_folders[0]);i++) {
		/* FIXME: should this uri be account relative? */
		*default_folders[i].uri = g_strdup_printf("%s#%s", store_uri, default_folders[i].base);
		*default_folders[i].folder = camel_store_get_folder(p->local_store, default_folders[i].base,
								    CAMEL_STORE_FOLDER_CREATE, &ex);
		camel_exception_clear(&ex);
	}

	g_free(store_uri);
}

/* EStorageBrowser callbacks.  */

static BonoboControl *
create_noselect_control (void)
{
	GtkWidget *label;

	label = gtk_label_new (_("This folder cannot contain messages."));
	gtk_widget_show (label);
	return bonobo_control_new (label);
}

static GtkWidget *
create_view_callback (EStorageBrowser *browser,
		      const char *path,
		      void *unused_data)
{
	BonoboControl *control;
	EFolder *folder;
	const char *folder_type;
	const char *physical_uri;

	folder = e_storage_set_get_folder (e_storage_browser_peek_storage_set (browser), path);
	if (folder == NULL) {
		g_warning ("No folder at %s", path);
		return gtk_label_new ("(You should not be seeing this label)");
	}

	folder_type  = e_folder_get_type_string (folder);
	physical_uri = e_folder_get_physical_uri (folder);

	if (type_is_mail (folder_type)) {
		const char *noselect;
		CamelURL *url;
		
		url = camel_url_new (physical_uri, NULL);
		noselect = url ? camel_url_get_param (url, "noselect") : NULL;
		if (noselect && !strcasecmp (noselect, "yes"))
			control = create_noselect_control ();
		else
			control = folder_browser_factory_new_control (physical_uri);
		camel_url_free (url);
	} else if (type_is_vtrash (folder_type)) {
		if (!strncasecmp (physical_uri, "file:", 5))
			control = folder_browser_factory_new_control ("vtrash:file:/");
		else
			control = folder_browser_factory_new_control (physical_uri);
	} else
		return NULL;
	
	if (!control)
		return NULL;

	/* EPFIXME: This leaks the control.  */
	return bonobo_widget_new_control_from_objref (BONOBO_OBJREF (control), CORBA_OBJECT_NIL);
}

static void
browser_page_switched_callback (EStorageBrowser *browser,
				GtkWidget *old_page,
				GtkWidget *new_page,
				BonoboControl *parent_control)
{
	if (BONOBO_IS_WIDGET (old_page)) {
		BonoboControlFrame *control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (old_page));

		bonobo_control_frame_control_deactivate (control_frame);
	}

	if (BONOBO_IS_WIDGET (new_page)) {
		BonoboControlFrame *control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (new_page));
		Bonobo_UIContainer ui_container = bonobo_control_get_remote_ui_container (parent_control, NULL);

		/* This is necessary because we are not embedding the folder browser control
		   directly; we are putting the folder browser control into a notebook which
		   is then exported to the shell as a control.  So we need to forward the
		   notebook's UIContainer to the folder browser.  */
		bonobo_control_frame_set_ui_container (control_frame, ui_container, NULL);

		bonobo_control_frame_control_activate (control_frame);
	}
}

static CamelFolder *
foo_get_folder (EStorageSetView *view, const char *path, CamelException *ex)
{
	/* <NotZed> either do
          mail_tool_uri_to_folder(ess_get_folder(path).physicaluri),
          or split the path into 'path' and 'storage name' and do get
          ess_get_storage() -> store -> open_folder
	*/
	CamelFolder *folder;
	EStorageSet *set;
	EFolder *efolder;
	const char *uri;
	
	set = e_storage_set_view_get_storage_set (view);
	efolder = e_storage_set_get_folder (set, path);
	uri = e_folder_get_physical_uri (efolder);
	
	folder = mail_tool_uri_to_folder (uri, 0, ex);
	
	return folder;
}

static void
drag_text_uri_list (EStorageSetView *view, const char *path, GtkSelectionData *selection, gpointer user_data)
{
	CamelFolder *src, *dest;
	const char *tmpdir;
	CamelStore *store;
	CamelException ex;
	GtkWidget *dialog;
	GPtrArray *uids;
	char *uri;
	
	camel_exception_init (&ex);
	
	if (!(src = foo_get_folder (view, path, &ex))) {
		dialog = gtk_message_dialog_new ((GtkWindow *) view, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 _("Could not open source folder: %s"),
						 camel_exception_get_description (&ex));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		camel_exception_clear (&ex);
		
		return;
	}
	
	if (!(tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX"))) {
		dialog = gtk_message_dialog_new ((GtkWindow *) view, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 _("Could not create temporary directory: %s"),
						 g_strerror (errno));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		camel_object_unref (src);
		
		return;
	}
	
	uri = g_strdup_printf ("mbox:%s", tmpdir);
	if (!(store = camel_session_get_store (session, uri, &ex))) {
		dialog = gtk_message_dialog_new ((GtkWindow *) view, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 _("Could not create temporary mbox store: %s"),
						 camel_exception_get_description (&ex));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		camel_exception_clear (&ex);
		camel_object_unref (src);
		g_free (uri);
		
		return;
	}
	
	if (!(dest = camel_store_get_folder (store, "mbox", CAMEL_STORE_FOLDER_CREATE, &ex))) {
		dialog = gtk_message_dialog_new ((GtkWindow *) view, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 _("Could not create temporary mbox folder: %s"),
						 camel_exception_get_description (&ex));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		camel_exception_clear (&ex);
		camel_object_unref (store);
		camel_object_unref (src);
		g_free (uri);
		
		return;
	}
	
	camel_object_unref (store);
	uids = camel_folder_get_uids (src);
	
	camel_folder_transfer_messages_to (src, uids, dest, NULL, FALSE, &ex);
	if (camel_exception_is_set (&ex)) {
		dialog = gtk_message_dialog_new ((GtkWindow *) view, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 _("Could not copy messages to temporary mbox folder: %s"),
						 camel_exception_get_description (&ex));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		camel_folder_free_uids (src, uids);
		camel_exception_clear (&ex);
		camel_object_unref (dest);
		camel_object_unref (src);
		g_free (uri);
		
		return;
	}
	
	camel_folder_free_uids (src, uids);
	camel_object_unref (dest);
	camel_object_unref (src);
	
	memcpy (uri, "file", 4);
	
	gtk_selection_data_set (selection, selection->target, 8,
				uri, strlen (uri));
	
	g_free (uri);
}

static void
folder_dragged_cb (EStorageSetView *view, const char *path, GdkDragContext *context,
		   GtkSelectionData *selection, guint info, guint time, gpointer user_data)
{
	printf ("dragging folder `%s'\n", path);
	
	switch (info) {
	case DND_DRAG_TYPE_FOLDER:
		/* dragging @path to a new location in the folder tree */
		gtk_selection_data_set (selection, selection->target, 8, path, strlen (path) + 1);
		break;
	case DND_DRAG_TYPE_TEXT_URI_LIST:
		/* dragging @path to some place external to evolution */
		drag_text_uri_list (view, path, selection, user_data);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
drop_uid_list (EStorageSetView *view, const char *path, gboolean move, GtkSelectionData *selection, gpointer user_data)
{
	CamelFolder *src, *dest;
	CamelException ex;
	GPtrArray *uids;
	char *src_uri;
	
	em_utils_selection_get_uidlist (selection, &src_uri, &uids);
	
	camel_exception_init (&ex);
	
	if (!(src = mail_tool_uri_to_folder (src_uri, 0, &ex))) {
		/* FIXME: report error to user? */
		camel_exception_clear (&ex);
		em_utils_uids_free (uids);
		g_free (src_uri);
		return;
	}
	
	g_free (src_uri);
	
	if (!(dest = foo_get_folder (view, path, &ex))) {
		/* FIXME: report error to user? */
		camel_exception_clear (&ex);
		em_utils_uids_free (uids);
		camel_object_unref (src);
		return;
	}
	
	camel_folder_transfer_messages_to (src, uids, dest, NULL, move, &ex);
	if (camel_exception_is_set (&ex)) {
		/* FIXME: report error to user? */
		camel_exception_clear (&ex);
		em_utils_uids_free (uids);
		camel_object_unref (dest);
		camel_object_unref (src);
		return;
	}
	
	em_utils_uids_free (uids);
	camel_object_unref (dest);
	camel_object_unref (src);
}

static void
drop_folder (EStorageSetView *view, const char *path, gboolean move, GtkSelectionData *selection, gpointer user_data)
{
	CamelFolder *src, *dest;
	CamelFolder *store;
	CamelException ex;
	
	camel_exception_init (&ex);
	
	/* get the destination folder (where the user dropped). this
	 * will become the parent folder of the folder that got
	 * dragged */
	if (!(dest = foo_get_folder (view, path, &ex))) {
		/* FIXME: report error to user? */
		camel_exception_clear (&ex);
		return;
	}
	
	/* get the folder being dragged */
	if (!(src = foo_get_folder (view, selection->data, &ex))) {
		/* FIXME: report error to user? */
		camel_exception_clear (&ex);
		camel_object_unref (dest);
		return;
	}
	
	if (src->parent_store == dest->parent_store && move) {
		/* simple rename() action */
		char *old_name, *new_name;
		
		old_name = g_strdup (src->full_name);
		new_name = g_strdup_printf ("%s/%s", dest->full_name, src->name);
		camel_object_unref (src);
		
		camel_store_rename_folder (dest->parent_store, old_name, new_name, &ex);
		if (camel_exception_is_set (&ex)) {
			/* FIXME: report error to user? */
			camel_exception_clear (&ex);
			camel_object_unref (dest);
			g_free (old_name);
			g_free (new_name);
			return;
		}
		
		camel_object_unref (dest);
		g_free (old_name);
		g_free (new_name);
	} else {
		/* copy the folder */
		camel_object_unref (dest);
		camel_object_unref (src);
	}
}

static gboolean
import_message_rfc822 (CamelFolder *dest, CamelStream *stream, gboolean scan_from, CamelException *ex)
{
	CamelMimeParser *mp;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, scan_from);
	camel_mime_parser_init_with_stream (mp, stream);
	
	while (camel_mime_parser_step (mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_object_unref (msg);
			camel_object_unref (mp);
			return FALSE;
		}
		
		/* append the message to the folder... */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (dest, msg, info, NULL, ex);
		camel_object_unref (msg);
		
		if (camel_exception_is_set (ex)) {
			camel_object_unref (mp);
			return FALSE;
		}
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (mp);
	
	return TRUE;
}

static void
drop_message_rfc822 (EStorageSetView *view, const char *path, GtkSelectionData *selection, gpointer user_data)
{
	CamelFolder *folder;
	CamelStream *stream;
	CamelException ex;
	gboolean scan_from;
	
	camel_exception_init (&ex);
	
	if (!(folder = foo_get_folder (view, path, &ex))) {
		/* FIXME: report error to user? */
		camel_exception_clear (&ex);
		return;
	}
	
	scan_from = selection->length > 5 && !strncmp (selection->data, "From ", 5);
	stream = camel_stream_mem_new_with_buffer (selection->data, selection->length);
	
	if (!import_message_rfc822 (folder, stream, scan_from, &ex)) {
		/* FIXME: report to user? */
	}
	
	camel_exception_clear (&ex);
	
	camel_object_unref (stream);
	camel_object_unref (folder);
}

static void
drop_text_uri_list (EStorageSetView *view, const char *path, GtkSelectionData *selection, gpointer user_data)
{
	CamelFolder *folder;
	CamelStream *stream;
	CamelException ex;
	char **urls, *tmp;
	int i;
	
	camel_exception_init (&ex);
	
	if (!(folder = foo_get_folder (view, path, &ex))) {
		/* FIXME: report to user? */
		camel_exception_clear (&ex);
		return;
	}
	
	tmp = g_strndup (selection->data, selection->length);
	urls = g_strsplit (tmp, "\n", 0);
	g_free (tmp);
	
	for (i = 0; urls[i] != NULL; i++) {
		CamelURL *uri;
		char *url;
		int fd;
		
		/* get the path component */
		url = g_strstrip (urls[i]);
		uri = camel_url_new (url, NULL);
		g_free (url);
		
		if (!uri || strcmp (uri->protocol, "file") != 0) {
			camel_url_free (uri);
			continue;
		}
		
		url = uri->path;
		uri->path = NULL;
		camel_url_free (uri);
		
		if ((fd = open (url, O_RDONLY)) == -1) {
			g_free (url);
			continue;
		}
		
		stream = camel_stream_fs_new_with_fd (fd);
		if (!import_message_rfc822 (folder, stream, TRUE, &ex)) {
			/* FIXME: report to user? */
		}
		
		camel_exception_clear (&ex);
		camel_object_unref (stream);
		g_free (url);
	}
	
	camel_object_unref (folder);
	g_free (urls);
}

static void
folder_receive_drop_cb (EStorageSetView *view, const char *path, GdkDragContext *context,
			GtkSelectionData *selection, guint info, guint time, gpointer user_data)
{
	gboolean move = context->action == GDK_ACTION_MOVE;
	
	/* this means we are receiving no data */
	if (!selection->data || selection->length == -1)
		return;
	
	switch (info) {
	case DND_DROP_TYPE_UID_LIST:
		/* import a list of uids from another evo folder */
		drop_uid_list (view, path, move, selection, user_data);
		printf ("* dropped a x-uid-list\n");
		break;
	case DND_DROP_TYPE_FOLDER:
		/* rename a folder */
		drop_folder (view, path, move, selection, user_data);
		printf ("* dropped a x-folder\n");
		break;
	case DND_DROP_TYPE_MESSAGE_RFC822:
		/* import a message/rfc822 stream */
		drop_message_rfc822 (view, path, selection, user_data);
		printf ("* dropped a message/rfc822\n");
		break;
	case DND_DROP_TYPE_TEXT_URI_LIST:
		/* import an mbox, maildir, or mh folder? */
		drop_text_uri_list (view, path, selection, user_data);
		printf ("* dropped a text/uri-list\n");
		break;
	default:
		g_assert_not_reached ();
	}
	
	gtk_drag_finish (context, TRUE, TRUE, time);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	MailComponentPrivate *priv = MAIL_COMPONENT (object)->priv;

	if (priv->storage_set != NULL) {
		g_object_unref (priv->storage_set);
		priv->storage_set = NULL;
	}

	if (priv->folder_type_registry != NULL) {
		g_object_unref (priv->folder_type_registry);
		priv->folder_type_registry = NULL;
	}

	if (priv->search_context != NULL) {
		g_object_unref (priv->search_context);
		priv->search_context = NULL;
	}

	if (priv->local_store != NULL) {
		camel_object_unref (CAMEL_OBJECT (priv->local_store));
		priv->local_store = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	MailComponentPrivate *priv = MAIL_COMPONENT (object)->priv;

	g_free (priv->base_directory);

	mail_async_event_destroy (priv->async_event);

	g_hash_table_destroy (priv->storages_hash); /* EPFIXME free the data within? */

	if (mail_async_event_destroy (priv->async_event) == -1) {
		g_warning("Cannot destroy async event: would deadlock");
		g_warning(" system may be unstable at exit");
	}

	g_free(priv->context_path);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Evolution::Component CORBA methods.  */

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     CORBA_Environment *ev)
{
	MailComponent *mail_component = MAIL_COMPONENT (bonobo_object_from_servant (servant));
	MailComponentPrivate *priv = mail_component->priv;
	EStorageBrowser *browser;
	GtkWidget *tree_widget;
	GtkWidget *tree_widget_scrolled;
	GtkWidget *view_widget;
	BonoboControl *sidebar_control;
	BonoboControl *view_control;

	browser = e_storage_browser_new (priv->storage_set, "/", create_view_callback, NULL);

	tree_widget = e_storage_browser_peek_tree_widget (browser);
	tree_widget_scrolled = e_storage_browser_peek_tree_widget_scrolled (browser);
	view_widget = e_storage_browser_peek_view_widget (browser);
	
	e_storage_set_view_set_drag_types ((EStorageSetView *) tree_widget, drag_types, num_drag_types);
	e_storage_set_view_set_drop_types ((EStorageSetView *) tree_widget, drop_types, num_drop_types);
	e_storage_set_view_set_allow_dnd ((EStorageSetView *) tree_widget, TRUE);
	
	g_signal_connect (tree_widget, "folder_dragged", G_CALLBACK (folder_dragged_cb), browser);
	g_signal_connect (tree_widget, "folder_receive_drop", G_CALLBACK (folder_receive_drop_cb), browser);
	
	gtk_widget_show (tree_widget_scrolled);
	gtk_widget_show (view_widget);

	sidebar_control = bonobo_control_new (tree_widget_scrolled);
	view_control = bonobo_control_new (view_widget);

	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (view_control), ev);

	g_signal_connect_object (browser, "page_switched",
				 G_CALLBACK (browser_page_switched_callback), view_control, 0);

	g_signal_connect(tree_widget, "right_click", G_CALLBACK(emc_tree_right_click), mail_component);
}


/* Initialization.  */

static void
mail_component_class_init (MailComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv->createControls = impl_createControls;
}

static void
mail_component_init (MailComponent *component)
{
	MailComponentPrivate *priv;
	EAccountList *accounts;

	priv = g_new0 (MailComponentPrivate, 1);
	component->priv = priv;

	/* EPFIXME: Move to a private directory.  */
	/* EPFIXME: Create the directory.  */
	priv->base_directory = g_build_filename (g_get_home_dir (), "evolution", NULL);

	/* EPFIXME: Turn into an object?  */
	mail_session_init (priv->base_directory);

	priv->async_event = mail_async_event_new();
	priv->storages_hash = g_hash_table_new (NULL, NULL);

	priv->folder_type_registry = e_folder_type_registry_new ();
	priv->storage_set = e_storage_set_new (priv->folder_type_registry);

#if 0				/* EPFIXME TODO somehow */
	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++)
		*standard_folders[i].uri = g_strdup_printf ("file://%s/local/%s", evolution_dir, standard_folders[i].name);
#endif
	setup_local_store (component);

	accounts = mail_config_get_accounts ();
	load_accounts(component, accounts);

#if 0
	/* EPFIXME?  */
	mail_local_storage_startup (shell_client, evolution_dir);
	mail_importer_init (shell_client);

	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++) {
		mail_msg_wait (mail_get_folder (*standard_folders[i].uri, CAMEL_STORE_FOLDER_CREATE,
						got_folder, standard_folders[i].folder, mail_thread_new));
	}
#endif
	
	/* mail_autoreceive_setup (); EPFIXME keep it off for testing */

	setup_search_context (component);

#if 0
	/* EPFIXME this shouldn't be here.  */
	if (mail_config_is_corrupt ()) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
						 _("Some of your mail settings seem corrupt, "
						   "please check that everything is in order."));
		g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
		gtk_widget_show (dialog);
	}
#endif

#if 0
	/* EPFIXME if we nuke the summary this is not necessary anymore.  */

	/* Everything should be ready now */
	evolution_folder_info_notify_ready ();
#endif

	/* EPFIXME not sure about this.  */
	go_online (component);
}


/* Public API.  */

MailComponent *
mail_component_peek (void)
{
	static MailComponent *component = NULL;

	if (component == NULL) {
		component = g_object_new (mail_component_get_type (), NULL);

		/* FIXME: this should all be initialised in a starutp routine, not from the peek function,
		   this covers much of the ::init method's content too */
		vfolder_load_storage();
	}

	return component;
}


const char *
mail_component_peek_base_directory (MailComponent *component)
{
	return component->priv->base_directory;
}

RuleContext *
mail_component_peek_search_context (MailComponent *component)
{
	return component->priv->search_context;
}


void
mail_component_add_store (MailComponent *component,
			  CamelStore *store,
			  const char *name)
{
	CamelException ex;

	camel_exception_init (&ex);
	
	if (name == NULL) {
		char *service_name;
		
		service_name = camel_service_get_name ((CamelService *) store, TRUE);
		add_storage (component, service_name, (CamelService *) store, &ex);
		g_free (service_name);
	} else {
		add_storage (component, name, (CamelService *) store, &ex);
	}
	
	camel_exception_clear (&ex);
}


/**
 * mail_component_load_storage_by_uri:
 * @component: 
 * @uri: 
 * @name: 
 * 
 * 
 * 
 * Return value: Pointer to the newly added CamelStore.  The caller is supposed
 * to ref the object if it wants to store it.
 **/
CamelStore *
mail_component_load_storage_by_uri (MailComponent *component,
				    const char *uri,
				    const char *name)
{
	CamelException ex;
	CamelService *store;
	CamelProvider *prov;
	
	camel_exception_init (&ex);
	
	/* Load the service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */
	
	prov = camel_session_get_provider (session, uri, &ex);
	if (prov == NULL) {
		/* EPFIXME: real error dialog */
		g_warning ("couldn't get service %s: %s\n", uri,
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return NULL;
	}
	
	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE) ||
	    (prov->flags & CAMEL_PROVIDER_IS_EXTERNAL))
		return NULL;
	
	store = camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex);
	if (store == NULL) {
		/* EPFIXME: real error dialog */
		g_warning ("couldn't get service %s: %s\n", uri,
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return NULL;
	}
	
	if (name != NULL) {
		add_storage (component, name, store, &ex);
	} else {
		char *service_name;
		
		service_name = camel_service_get_name (store, TRUE);
		add_storage (component, service_name, store, &ex);
		g_free (service_name);
	}
	
	if (camel_exception_is_set (&ex)) {
		/* EPFIXME: real error dialog */
		g_warning ("Cannot load storage: %s",
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
	}
	
	camel_object_unref (CAMEL_OBJECT (store));
	return CAMEL_STORE (store);		/* (Still has one ref in the hash.)  */
}


static void
store_disconnect (CamelStore *store,
		  void *event_data,
		  void *data)
{
	camel_service_disconnect (CAMEL_SERVICE (store), TRUE, NULL);
	camel_object_unref (CAMEL_OBJECT (store));
}

void
mail_component_remove_storage (MailComponent *component,
			       CamelStore *store)
{
	MailComponentPrivate *priv = component->priv;
	EStorage *storage;
	
	/* Because the storages_hash holds a reference to each store
	 * used as a key in it, none of them will ever be gc'ed, meaning
	 * any call to camel_session_get_{service,store} with the same
	 * URL will always return the same object. So this works.
	 */
	
	storage = g_hash_table_lookup (priv->storages_hash, store);
	if (!storage)
		return;
	
	g_hash_table_remove (priv->storages_hash, store);
	
	/* so i guess potentially we could have a race, add a store while one
	   being removed.  ?? */
	mail_note_store_remove (store);

	e_storage_set_remove_storage (priv->storage_set, storage);
	
	mail_async_event_emit(priv->async_event, MAIL_ASYNC_THREAD, (MailAsyncFunc) store_disconnect, store, NULL, NULL);
}


void
mail_component_remove_storage_by_uri (MailComponent *component,
				      const char *uri)
{
	CamelProvider *prov;
	CamelService *store;

	prov = camel_session_get_provider (session, uri, NULL);
	if (!prov)
		return;
	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE) ||
	    (prov->flags & CAMEL_PROVIDER_IS_EXTERNAL))
		return;

	store = camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store != NULL) {
		mail_component_remove_storage (component, CAMEL_STORE (store));
		camel_object_unref (CAMEL_OBJECT (store));
	}
}


EStorage *
mail_component_lookup_storage (MailComponent *component,
			       CamelStore *store)
{
	EStorage *storage;
	
	/* Because the storages_hash holds a reference to each store
	 * used as a key in it, none of them will ever be gc'ed, meaning
	 * any call to camel_session_get_{service,store} with the same
	 * URL will always return the same object. So this works.
	 */
	
	storage = g_hash_table_lookup (component->priv->storages_hash, store);
	if (storage)
		g_object_ref (storage);
	
	return storage;
}


int
mail_component_get_storage_count (MailComponent *component)
{
	return g_hash_table_size (component->priv->storages_hash);
}


EStorageSet *
mail_component_peek_storage_set (MailComponent *component)
{
	return component->priv->storage_set;
}


void
mail_component_storages_foreach (MailComponent *component,
				 GHFunc func,
				 void *data)
{
	g_hash_table_foreach (component->priv->storages_hash, func, data);
}

extern struct _CamelSession *session;

char *em_uri_from_camel(const char *curi)
{
	CamelURL *curl;
	EAccount *account;
	const char *uid, *path;
	char *euri;
	CamelProvider *provider;

	provider = camel_session_get_provider(session, curi, NULL);
	if (provider == NULL)
		return g_strdup(curi);

	curl = camel_url_new(curi, NULL);
	if (curl == NULL)
		return g_strdup(curi);

	account = mail_config_get_account_by_source_url(curi);
	uid = (account == NULL)?"local@local":account->uid;
	path = (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)?curl->fragment:curl->path;
	if (path[0] == '/')
		path++;
	euri = g_strdup_printf("email://%s/%s", uid, path);
	printf("em uri from camel '%s' -> '%s'\n", curi, euri);

	return euri;
}

char *em_uri_to_camel(const char *euri)
{
	EAccountList *accounts;
	const EAccount *account;
	EAccountService *service;
	CamelProvider *provider;
	CamelURL *eurl, *curl;
	char *uid, *curi;

	eurl = camel_url_new(euri, NULL);
	if (eurl == NULL)
		return g_strdup(euri);

	if (strcmp(eurl->protocol, "email") != 0) {
		camel_url_free(eurl);
		return g_strdup(euri);
	}

	g_assert(eurl->user != NULL);
	g_assert(eurl->host != NULL);

	if (strcmp(eurl->user, "local") == 0 && strcmp(eurl->host, "local") == 0) {
		/* FIXME: needs to track real local store location */
		curi = g_strdup_printf("mbox:%s/.evolution/mail/local#%s", g_get_home_dir(), eurl->path);
		camel_url_free(eurl);
		return curi;
	}

	uid = g_strdup_printf("%s@%s", eurl->user, eurl->host);

	accounts = mail_config_get_accounts();
	account = e_account_list_find(accounts, E_ACCOUNT_FIND_UID, uid);
	g_free(uid);

	if (account == NULL) {
		camel_url_free(eurl);
		return g_strdup(euri);
	}

	service = account->source;
	provider = camel_session_get_provider(session, service->url, NULL);

	curl = camel_url_new(service->url, NULL);
	if (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		camel_url_set_fragment(curl, eurl->path);
	else
		camel_url_set_path(curl, eurl->path);

	curi = camel_url_to_string(curl, 0);

	camel_url_free(eurl);
	camel_url_free(curl);

	printf("em uri to camel '%s' -> '%s'\n", euri, curi);

	return curi;
}


CamelFolder *
mail_component_get_folder_from_evomail_uri (MailComponent *component,
					    guint32 flags,
					    const char *evomail_uri,
					    CamelException *ex)
{
	CamelException local_ex;
	EAccountList *accounts;
	EIterator *iter;
	const char *p;
	const char *q;
	const char *folder_name;
	char *uid;

	camel_exception_init (&local_ex);

	if (strncmp (evomail_uri, "evomail:", 8) != 0)
		return NULL;

	p = evomail_uri + 8;
	while (*p == '/')
		p ++;

	q = strchr (p, '/');
	if (q == NULL)
		return NULL;

	uid = g_strndup (p, q - p);
	folder_name = q + 1;

	/* since we have no explicit account for 'local' folders, make one up */
	if (strcmp(uid, "local") == 0) {
		g_free(uid);
		return camel_store_get_folder(component->priv->local_store, folder_name, flags, ex);
	}

	accounts = mail_config_get_accounts ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);
		EAccountService *service = account->source;
		CamelProvider *provider;
		CamelStore *store;

		if (strcmp (account->uid, uid) != 0)
			continue;

		provider = camel_session_get_provider (session, service->url, &local_ex);
		if (provider == NULL)
			goto fail;

		store = (CamelStore *) camel_session_get_service (session, service->url, CAMEL_PROVIDER_STORE, &local_ex);
		if (store == NULL)
			goto fail;

		g_free (uid);
		return camel_store_get_folder (store, folder_name, flags, ex);
	}

 fail:
	camel_exception_clear (&local_ex);
	g_free (uid);
	return NULL;
}


char *
mail_component_evomail_uri_from_folder (MailComponent *component,
					CamelFolder *folder)
{
	CamelStore *store = camel_folder_get_parent_store (folder);
	EAccount *account;
	char *service_url;
	char *evomail_uri;
	const char *uid;

	if (store == NULL)
		return NULL;

	service_url = camel_service_get_url (CAMEL_SERVICE (store));
	account = mail_config_get_account_by_source_url (service_url);

	if (account == NULL) {
		/* since we have no explicit account for 'local' folders, make one up */
		/* TODO: check the folder is really a local one, folder->parent_store == local_store? */
		uid = "local";
		/*g_free (service_url);
		return NULL;*/
	} else {
		uid = account->uid;
	}

	evomail_uri = g_strconcat ("evomail:///", uid, "/", camel_folder_get_full_name (folder), NULL);
	g_free (service_url);

	return evomail_uri;
}


BONOBO_TYPE_FUNC_FULL (MailComponent, GNOME_Evolution_Component, PARENT_TYPE, mail_component)


/* ********************************************************************** */
#if 0
static void
emc_popup_view(GtkWidget *w, MailComponent *mc)
{

}

static void
emc_popup_open_new(GtkWidget *w, MailComponent *mc)
{
}
#endif

/* FIXME: This must be done in another thread */
static void
em_copy_folders(CamelStore *tostore, const char *tobase, CamelStore *fromstore, const char *frombase, int delete)
{
	GString *toname, *fromname;
	CamelFolderInfo *fi;
	GList *pending = NULL, *deleting = NULL, *l;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	CamelException *ex = camel_exception_new();
	int fromlen;
	const char *tmp;

	if (camel_store_supports_subscriptions(fromstore))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	fi = camel_store_get_folder_info(fromstore, frombase, flags, ex);
	if (camel_exception_is_set(ex))
		goto done;

	pending = g_list_append(pending, fi);

	toname = g_string_new("");
	fromname = g_string_new("");

	tmp = strrchr(frombase, '/');
	if (tmp == NULL)
		fromlen = 0;
	else
		fromlen = tmp-frombase+1;

	printf("top name is '%s'\n", fi->full_name);

	while (pending) {
		CamelFolderInfo *info = pending->data;

		pending = g_list_remove_link(pending, pending);
		while (info) {
			CamelFolder *fromfolder, *tofolder;
			GPtrArray *uids;

			if (info->child)
				pending = g_list_append(pending, info->child);
			if (tobase[0])
				g_string_printf(toname, "%s/%s", tobase, info->full_name + fromlen);
			else
				g_string_printf(toname, "%s", info->full_name + fromlen);

			printf("Copying from '%s' to '%s'\n", info->full_name, toname->str);

			/* This makes sure we create the same tree, e.g. from a nonselectable source */
			/* Not sure if this is really the 'right thing', e.g. for spool stores, but it makes the ui work */
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				printf("this folder is selectable\n");
				fromfolder = camel_store_get_folder(fromstore, info->full_name, 0, ex);
				if (fromfolder == NULL)
					goto exception;

				tofolder = camel_store_get_folder(tostore, toname->str, CAMEL_STORE_FOLDER_CREATE, ex);
				if (tofolder == NULL) {
					camel_object_unref(fromfolder);
					goto exception;
				}

				if (camel_store_supports_subscriptions(tostore)
				    && !camel_store_folder_subscribed(tostore, toname->str))
					camel_store_subscribe_folder(tostore, toname->str, NULL);

				uids = camel_folder_get_uids(fromfolder);
				camel_folder_transfer_messages_to(fromfolder, uids, tofolder, NULL, delete, ex);
				camel_folder_free_uids(fromfolder, uids);

				camel_object_unref(fromfolder);
				camel_object_unref(tofolder);
			}

			if (camel_exception_is_set(ex))
				goto exception;
			else if (delete)
				deleting = g_list_prepend(deleting, info);

			info = info->sibling;
		}
	}

	/* delete the folders in reverse order from how we copyied them, if we are deleting any */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;

		printf("deleting folder '%s'\n", info->full_name);

		if (camel_store_supports_subscriptions(fromstore))
			camel_store_unsubscribe_folder(fromstore, info->full_name, NULL);

		camel_store_delete_folder(fromstore, info->full_name, NULL);
		l = l->next;
	}

exception:
	camel_store_free_folder_info(fromstore, fi);
	g_list_free(deleting);

	g_string_free(toname, TRUE);
	g_string_free(fromname, TRUE);
done:
	printf("exception: %s\n", ex->desc?ex->desc:"<none>");
	camel_exception_free(ex);
}

struct _copy_folder_data {
	MailComponent *mc;
	int delete;
};

static void
emc_popup_copy_folder_selected(const char *uri, void *data)
{
	struct _copy_folder_data *d = data;

	if (uri == NULL) {
		g_free(d);
		return;
	}

	if (uri) {
		EFolder *folder = e_storage_set_get_folder(d->mc->priv->storage_set, d->mc->priv->context_path);
		CamelException *ex = camel_exception_new();
		CamelStore *fromstore, *tostore;
		char *tobase, *frombase;
		CamelURL *url;

		printf("copying folder '%s' to '%s'\n", d->mc->priv->context_path, uri);

		fromstore = camel_session_get_store(session, e_folder_get_physical_uri(folder), ex);
		frombase = strchr(d->mc->priv->context_path+1, '/')+1;

		tostore = camel_session_get_store(session, uri, ex);
		url = camel_url_new(uri, NULL);
		if (url->fragment)
			tobase = url->fragment;
		else if (url->path && url->path[0])
			tobase = url->path+1;
		else
			tobase = "";

		em_copy_folders(tostore, tobase, fromstore, frombase, d->delete);

		camel_url_free(url);
		camel_exception_free(ex);
	}
	g_free(d);
}

static void
emc_popup_copy(GtkWidget *w, MailComponent *mc)
{
	struct _copy_folder_data *d;

	d = g_malloc(sizeof(*d));
	d->mc = mc;
	d->delete = 0;
	em_select_folder(NULL, _("Select folder"), _("Select destination to copy folder into"), NULL, emc_popup_copy_folder_selected, d);
}

static void
emc_popup_move(GtkWidget *w, MailComponent *mc)
{
	struct _copy_folder_data *d;

	d = g_malloc(sizeof(*d));
	d->mc = mc;
	d->delete = 1;
	em_select_folder(NULL, _("Select folder"), _("Select destination to move folder into"), NULL, emc_popup_copy_folder_selected, d);
}
static void
emc_popup_new_folder_create(EStorageSet *ess, EStorageResult result, void *data)
{
	printf("folder created %s\n", result == E_STORAGE_OK?"ok":"failed");
}

static void
emc_popup_new_folder_response(EMFolderSelector *emfs, guint response, MailComponent *mc)
{
	if (response == GTK_RESPONSE_OK) {
		char *path, *tmp, *name, *full;
		EStorage *storage;
		CamelStore *store;
		CamelException *ex;

		printf("Creating folder: %s (%s)\n", em_folder_selector_get_selected(emfs),
		       em_folder_selector_get_selected_uri(emfs));

		path = g_strdup(em_folder_selector_get_selected(emfs));
		tmp = strchr(path+1, '/');
		*tmp++ = 0;
		/* FIXME: camel_store_create_folder should just take full path names */
		full = g_strdup(tmp);
		name = strrchr(tmp, '/');
		if (name == NULL) {
			name = tmp;
			tmp = "";
		} else
			*name++  = 0;

		storage = e_storage_set_get_storage(mc->priv->storage_set, path+1);
		store = g_object_get_data((GObject *)storage, "em-store");

		printf("creating folder '%s' / '%s' on '%s'\n", tmp, name, path+1);

		ex = camel_exception_new();
		camel_store_create_folder(store, tmp, name, ex);
		if (camel_exception_is_set(ex)) {
			printf("Create failed: %s\n", ex->desc);
		} else if (camel_store_supports_subscriptions(store)) {
			camel_store_subscribe_folder(store, full, ex);
			if (camel_exception_is_set(ex)) {
				printf("Subscribe failed: %s\n", ex->desc);
			}
		}

		camel_exception_free(ex);

		g_free(full);
		g_free(path);

		/* Blah, this should just use camel, we get better error reporting if we do too */
		/*e_storage_set_async_create_folder(mc->priv->storage_set, path, "mail", "", emc_popup_new_folder_create, mc);*/
	}
	gtk_widget_destroy((GtkWidget *)emfs);
}

static void
emc_popup_new_folder (GtkWidget *w, MailComponent *mc)
{
	GtkWidget *dialog;

	dialog = em_folder_selector_create_new(mc->priv->storage_set, 0, _("Create folder"), _("Specify where to create the folder:"));
	em_folder_selector_set_selected((EMFolderSelector *)dialog, mc->priv->context_path);
	g_signal_connect(dialog, "response", G_CALLBACK(emc_popup_new_folder_response), mc);
	gtk_widget_show(dialog);
}

static void
em_delete_rec(CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;

		if (fi->child)
			em_delete_rec(store, fi->child, ex);
		if (camel_exception_is_set(ex))
			return;

		printf("deleting folder '%s'\n", fi->full_name);

		/* shouldn't camel do this itself? */
		if (camel_store_supports_subscriptions(store))
			camel_store_unsubscribe_folder(store, fi->full_name, NULL);

		folder = camel_store_get_folder(store, fi->full_name, 0, NULL);
		if (folder) {
			GPtrArray *uids = camel_folder_get_uids(folder);
			int i;

			camel_folder_freeze(folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message(folder, uids->pdata[i]);
			camel_folder_sync(folder, TRUE, NULL);
			camel_folder_thaw(folder);
			camel_folder_free_uids(folder, uids);
		}

		camel_store_delete_folder(store, fi->full_name, ex);
		if (camel_exception_is_set(ex))
			return;
		fi = fi->sibling;
	}
}

static void
em_delete_folders(CamelStore *store, const char *base, CamelException *ex)
{
	CamelFolderInfo *fi;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
 
	if (camel_store_supports_subscriptions(store))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	fi = camel_store_get_folder_info(store, base, flags, ex);
	if (camel_exception_is_set(ex))
		return;

	em_delete_rec(store, fi, ex);
	camel_store_free_folder_info(store, fi);
}

static void
emc_popup_delete_response(GtkWidget *w, guint response, MailComponent *mc)
{
	gtk_widget_destroy(w);

	if (response == GTK_RESPONSE_OK) {
		const char *path = strchr(mc->priv->context_path+1, '/')+1;
		EFolder *folder = e_storage_set_get_folder(mc->priv->storage_set, mc->priv->context_path);
		CamelException *ex = camel_exception_new();
		CamelStore *store;

		/* FIXME: need to hook onto store changed event and delete view as well, somewhere else tho */
		store = camel_session_get_store(session, e_folder_get_physical_uri(folder), ex);
		if (camel_exception_is_set(ex))
			goto exception;

		em_delete_folders(store, path, ex);
		if (!camel_exception_is_set(ex))
			goto noexception;
	exception:
		e_notice(NULL, GTK_MESSAGE_ERROR,
			 _("Could not delete folder: %s"), ex->desc);
	noexception:
		camel_exception_free(ex);
		if (store)
			camel_object_unref(store);
	}
}

static void
emc_popup_delete_folder(GtkWidget *w, MailComponent *mc)
{
	GtkWidget *dialog;
	char *title;
	const char *path = strchr(mc->priv->context_path+1, '/')+1;
	EFolder *folder;

	folder = e_storage_set_get_folder(mc->priv->storage_set, mc->priv->context_path);
	if (folder == NULL)
		return;

	dialog = gtk_message_dialog_new(NULL,
					GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					_("Really delete folder \"%s\" and all of its subfolders?"), path);

	gtk_dialog_add_button((GtkDialog *)dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button((GtkDialog *)dialog, GTK_STOCK_DELETE, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response((GtkDialog *)dialog, GTK_RESPONSE_OK);
	gtk_container_set_border_width((GtkContainer *)dialog, 6); 
	gtk_box_set_spacing((GtkBox *)((GtkDialog *)dialog)->vbox, 6);

	title = g_strdup_printf(_("Delete \"%s\""), path);
	gtk_window_set_title((GtkWindow *)dialog, title);
	g_free(title);

	g_signal_connect(dialog, "response", G_CALLBACK(emc_popup_delete_response), mc);
	gtk_widget_show(dialog);
}

static void
emc_popup_rename_folder(GtkWidget *w, MailComponent *mc)
{
	char *prompt, *new;
	EFolder *folder;
	const char *old, *why;
	int done = 0;

	folder = e_storage_set_get_folder(mc->priv->storage_set, mc->priv->context_path);
	if (folder == NULL)
		return;

	old = e_folder_get_name(folder);
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), e_folder_get_name(folder));
	while (!done) {
		new = e_request_string(NULL, _("Rename Folder"), prompt, old);
		if (new == NULL || strcmp(old, new) == 0)
			done = 1;
#if 0
		else if (!e_shell_folder_name_is_valid(new, &why))
			e_notice(NULL, GTK_MESSAGE_ERROR, _("The specified folder name is not valid: %s"), why);
#endif
		else {
			char *base, *path;

			/* FIXME: we can't use the os independent path crap here, since we want to control the format */
			base = g_path_get_dirname(mc->priv->context_path);
			path = g_build_filename(base, new, NULL);

			if (e_storage_set_get_folder(mc->priv->storage_set, path) != NULL) {
				e_notice(NULL, GTK_MESSAGE_ERROR,
					 _("A folder named \"%s\" already exists.  Please use a different name."), new);
			} else {
				CamelStore *store;	
				CamelException *ex = camel_exception_new();
				const char *oldpath, *newpath;

				oldpath = strchr(mc->priv->context_path+1, '/');
				g_assert(oldpath);
				newpath = strchr(path+1, '/');
				g_assert(newpath);
				oldpath++;
				newpath++;

				printf("renaming %s to %s\n", oldpath, newpath);

				store = camel_session_get_store(session, e_folder_get_physical_uri(folder), ex);
				if (camel_exception_is_set(ex))
					goto exception;

				camel_store_rename_folder(store, oldpath, newpath, ex);
				if (!camel_exception_is_set(ex))
					goto noexception;

			exception:
				e_notice(NULL, GTK_MESSAGE_ERROR,
					 _("Could not rename folder: %s"), ex->desc);
			noexception:
				if (store)
					camel_object_unref(store);
				camel_exception_free(ex);

				done = 1;
			}
			g_free(path);
			g_free(base);
		}
		g_free(new);
	}
}

struct _prop_data {
	void *object;
	CamelArgV *argv;
	GtkWidget **widgets;
};

static void
emc_popup_properties_response(GtkWidget *dialog, int response, struct _prop_data *prop_data)
{
	int i;
	CamelArgV *argv = prop_data->argv;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy(dialog);
		return;
	}

	for (i=0;i<argv->argc;i++) {
		CamelArg *arg = &argv->argv[i];

		switch (arg->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			arg->ca_int = gtk_toggle_button_get_active ((GtkToggleButton *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_STR:
			g_free(arg->ca_str);
			arg->ca_str = gtk_entry_get_text ((GtkEntry *) prop_data->widgets[i]);
			break;
		default:
			printf("unknown property type set\n");
		}
	}

	camel_object_setv(prop_data->object, NULL, argv);
	gtk_widget_destroy(dialog);
}

static void
emc_popup_properties_free(void *data)
{
	struct _prop_data *prop_data = data;
	int i;

	for (i=0; i<prop_data->argv->argc; i++) {
		if ((prop_data->argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			g_free(prop_data->argv->argv[i].ca_str);
	}
	camel_object_unref(prop_data->object);
	g_free(prop_data->argv);
	g_free(prop_data);
}

static void
emc_popup_properties_got_folder (char *uri, CamelFolder *folder, void *data)
{
	if (folder) {
		GtkWidget *dialog, *w, *table, *label;
		GSList *list, *l;
		char *name;
		int row = 1;
		gint32 count, i;
		struct _prop_data *prop_data;
		CamelArgV *argv;
		CamelArgGetV *arggetv;

		camel_object_get(folder, NULL, CAMEL_FOLDER_PROPERTIES, &list, CAMEL_FOLDER_NAME, &name, NULL);

		dialog = gtk_dialog_new_with_buttons(_("Folder properties"),
						     NULL,
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_OK,
						     GTK_RESPONSE_OK,
						     NULL);

		/* TODO: maybe we want some basic properties here, like message counts/approximate size/etc */
		w = gtk_frame_new(_("Properties"));
		gtk_box_pack_start ((GtkBox *) ((GtkDialog *)dialog)->vbox, w, TRUE, TRUE, 6);
		table = gtk_table_new(g_slist_length(list)+1, 2, FALSE);
		gtk_container_add((GtkContainer *)w, table);
		label = gtk_label_new(_("Folder Name"));
		gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
		gtk_table_attach ((GtkTable *) table, label, 0, 1, 0, 1, GTK_FILL|GTK_EXPAND, 0, 3, 0);
		label = gtk_label_new(name);
		gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
		gtk_table_attach ((GtkTable *) table, label, 1, 2, 0, 1, GTK_FILL|GTK_EXPAND, 0, 3, 0);

		/* build an arggetv/argv to retrieve/store the results */
		count = g_slist_length(list);
		arggetv = g_malloc0(sizeof(*arggetv) + (count - CAMEL_ARGV_MAX) * sizeof(arggetv->argv[0]));
		arggetv->argc = count;
		argv = g_malloc0(sizeof(*argv) + (count - CAMEL_ARGV_MAX) * sizeof(argv->argv[0]));
		argv->argc = count;
		i = 0;
		l = list;
		while (l) {
			CamelProperty *prop = l->data;

			argv->argv[i].tag = prop->tag;
			arggetv->argv[i].tag = prop->tag;
			arggetv->argv[i].ca_ptr = &argv->argv[i].ca_ptr;

			l = l->next;
			i++;
		}
		camel_object_getv(folder, NULL, arggetv);
		g_free(arggetv);

		prop_data = g_malloc0(sizeof(*prop_data));
		prop_data->widgets = g_malloc0(sizeof(prop_data->widgets[0]) * count);
		prop_data->argv = argv;

		/* setup the ui with the values retrieved */
		l = list;
		i = 0;
		while (l) {
			CamelProperty *prop = l->data;

			switch (prop->tag & CAMEL_ARG_TYPE) {
			case CAMEL_ARG_BOO:
				w = gtk_check_button_new_with_label(prop->description);
				gtk_toggle_button_set_active((GtkToggleButton *)w, argv->argv[i].ca_int != 0);
				gtk_table_attach ((GtkTable *) table, w, 0, 2, row, row+1, 0, 0, 3, 3);
				prop_data->widgets[i] = w;
				break;
			case CAMEL_ARG_STR:
				label = gtk_label_new(prop->description);
				gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
				gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row+1, GTK_FILL|GTK_EXPAND, 0, 3, 3);

				w = gtk_entry_new();
				if (argv->argv[i].ca_str) {
					gtk_entry_set_text((GtkEntry *)w, argv->argv[i].ca_str);
					camel_object_free(folder, argv->argv[i].tag, argv->argv[i].ca_str);
					argv->argv[i].ca_str = NULL;
				}
				gtk_table_attach ((GtkTable *) table, w, 1, 2, row, row+1, GTK_FILL, 0, 3, 3);
				prop_data->widgets[i] = w;
				break;
			default:
				w = gtk_label_new("CamelFolder error: unsupported propery type");
				gtk_table_attach ((GtkTable *) table, w, 0, 2, row, row+1, 0, 0, 3, 3);
				break;
			}

			row++;
			l = l->next;
		}

		prop_data->object = folder;
		camel_object_ref(folder);

		camel_object_free(folder, CAMEL_FOLDER_PROPERTIES, list);
		camel_object_free(folder, CAMEL_FOLDER_NAME, name);

		/* we do 'apply on ok' ... since instant apply may apply some very long running tasks */

		g_signal_connect(dialog, "response", G_CALLBACK(emc_popup_properties_response), prop_data);
		g_object_set_data_full((GObject *)dialog, "e-prop-data", prop_data, emc_popup_properties_free);
		gtk_widget_show_all(dialog);
	}
}

static void
emc_popup_properties(GtkWidget *w, MailComponent *mc)
{
	EFolder *efolder;

	/* TODO: Make sure we only have one dialog open for any given folder */

	efolder = e_storage_set_get_folder(mc->priv->storage_set, mc->priv->context_path);
	if (efolder == NULL)
		return;

	mail_get_folder(e_folder_get_physical_uri(efolder), 0, emc_popup_properties_got_folder, mc, mail_thread_new);
}

static EMPopupItem emc_popup_menu[] = {
#if 0
	{ EM_POPUP_ITEM, "00.emc.00", N_("_View"), G_CALLBACK(emc_popup_view), NULL, NULL, 0 },
	{ EM_POPUP_ITEM, "00.emc.01", N_("Open in _New Window"), G_CALLBACK(emc_popup_open_new), NULL, NULL, 0 },

	{ EM_POPUP_BAR, "10.emc" },
#endif
	{ EM_POPUP_ITEM, "10.emc.00", N_("_Copy"), G_CALLBACK(emc_popup_copy), NULL, "folder-copy-16.png", 0 },
	{ EM_POPUP_ITEM, "10.emc.01", N_("_Move"), G_CALLBACK(emc_popup_move), NULL, "folder-move-16.png", 0 },

	{ EM_POPUP_BAR, "20.emc" },
	{ EM_POPUP_ITEM, "20.emc.00", N_("_New Folder..."), G_CALLBACK(emc_popup_new_folder), NULL, "folder-mini.png", 0 },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Delete"), G_CALLBACK(emc_popup_delete_folder), NULL, "evolution-trash-mini.png", 0 },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Rename"), G_CALLBACK(emc_popup_rename_folder), NULL, NULL, 0 },

	{ EM_POPUP_BAR, "80.emc" },
	{ EM_POPUP_ITEM, "80.emc.00", N_("_Properties..."), G_CALLBACK(emc_popup_properties), NULL, "configure_16_folder.xpm", 0 },
};


static int
emc_tree_right_click(ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, MailComponent *component)
{
	char *name;
	ETreeModel *model = e_tree_get_model(tree);
	EMPopup *emp;
	int i;
	GSList *menus = NULL;
	struct _GtkMenu *menu;

	name = e_tree_memory_node_get_data((ETreeMemory *)model, path);
	g_free(component->priv->context_path);
	component->priv->context_path = g_strdup(name);
	printf("right click, path = '%s'\n", name);

	emp = em_popup_new("com.ximian.mail.storageset.popup.select");

	for (i=0;i<sizeof(emc_popup_menu)/sizeof(emc_popup_menu[0]);i++) {
		EMPopupItem *item = &emc_popup_menu[i];

		item->activate_data = component;
		menus = g_slist_prepend(menus, item);
	}

	em_popup_add_items(emp, menus, (GDestroyNotify)g_slist_free);

	menu = em_popup_create_menu_once(emp, NULL, 0, 0);

	if (event == NULL || event->type == GDK_KEY_PRESS) {
		/* FIXME: menu pos function */
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, event->key.time);
	} else {
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button.button, event->button.time);
	}

	return TRUE;
}
