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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <libxml/parser.h>

#include <e-util/e-mktemp.h>

#include <gal/util/e-xml-utils.h>

#include <camel/camel-file-utils.h>

#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-mt.h"

/* sigh, these 2 only needed for outbox total count checking - a mess */
#include "mail-component.h"
#include "mail-folder-cache.h"

#include "em-utils.h"

#include <camel/camel-folder.h>

#include "em-marshal.h"
#include "em-folder-tree-model.h"

#define u(x) x			/* unread count debug */
#define d(x) x

static GType col_types[] = {
	G_TYPE_STRING,   /* display name */
	G_TYPE_POINTER,  /* store object */
	G_TYPE_STRING,   /* path */
	G_TYPE_STRING,   /* uri */
	G_TYPE_UINT,     /* unread count */
	G_TYPE_UINT,     /* flags */
	G_TYPE_BOOLEAN,  /* is a store node */
	G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
};

/* GObject virtual method overrides */
static void em_folder_tree_model_class_init (EMFolderTreeModelClass *klass);
static void em_folder_tree_model_init (EMFolderTreeModel *model);
static void em_folder_tree_model_finalize (GObject *obj);

/* interface init methods */
static void tree_model_iface_init (GtkTreeModelIface *iface);
static void tree_sortable_iface_init (GtkTreeSortableIface *iface);

static void account_changed (EAccountList *accounts, EAccount *account, gpointer user_data);
static void account_removed (EAccountList *accounts, EAccount *account, gpointer user_data);

enum {
	LOADING_ROW,
	LOADED_ROW,
	FOLDER_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GtkTreeStoreClass *parent_class = NULL;

GType
em_folder_tree_model_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeModelClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_model_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTreeModel),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_model_init,
		};
		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) tree_model_iface_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo sortable_info = {
			(GInterfaceInitFunc) tree_sortable_iface_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GTK_TYPE_TREE_STORE, "EMFolderTreeModel", &info, 0);
		
		g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_SORTABLE,
					     &sortable_info);
	}
	
	return type;
}


static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GTK_TYPE_TREE_STORE);
	
	object_class->finalize = em_folder_tree_model_finalize;
	
	/* signals */
	signals[LOADING_ROW] =
		g_signal_new ("loading-row",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, loading_row),
			      NULL, NULL,
			      em_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[LOADED_ROW] =
		g_signal_new ("loaded-row",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, loaded_row),
			      NULL, NULL,
			      em_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[FOLDER_ADDED] =
		g_signal_new ("folder-added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, folder_added),
			      NULL, NULL,
			      em_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);
}

static int
sort_cb (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	extern CamelStore *vfolder_store;
	char *aname, *bname;
	CamelStore *store;
	gboolean is_store;
	
	gtk_tree_model_get (model, a, COL_BOOL_IS_STORE, &is_store,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_DISPLAY_NAME, &aname, -1);
	gtk_tree_model_get (model, b, COL_STRING_DISPLAY_NAME, &bname, -1);
	
	if (is_store) {
		/* On This Computer is always first and VFolders is always last */
		if (!strcmp (aname, _("On This Computer")))
			return -1;
		if (!strcmp (bname, _("On This Computer")))
			return 1;
		if (!strcmp (aname, _("VFolders")))
			return 1;
		if (!strcmp (bname, _("VFolders")))
			return -1;
	} else if (store == vfolder_store) {
		/* UNMATCHED is always last */
		if (aname && !strcmp (aname, _("UNMATCHED")))
			return 1;
		if (bname && !strcmp (bname, _("UNMATCHED")))
			return -1;
	} else {
		/* Inbox is always first */
		if (aname && (!strcmp (aname, "INBOX") || !strcmp (aname, _("Inbox"))))
			return -1;
		if (bname && (!strcmp (bname, "INBOX") || !strcmp (bname, _("Inbox"))))
			return 1;
	}
	
	if (aname == NULL) {
		if (bname == NULL)
			return 0;
	} else if (bname == NULL)
		return 1;
	
	return g_utf8_collate (aname, bname);
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	model->store_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->uri_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	gtk_tree_sortable_set_default_sort_func ((GtkTreeSortable *) model, sort_cb, NULL, NULL);
	
	model->accounts = mail_config_get_accounts ();
	model->account_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->account_changed_id = g_signal_connect (model->accounts, "account-changed", G_CALLBACK (account_changed), model);
	model->account_removed_id = g_signal_connect (model->accounts, "account-removed", G_CALLBACK (account_removed), model);
}

static void
path_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
}

static void
store_info_free (struct _EMFolderTreeModelStoreInfo *si)
{
	camel_object_remove_event (si->store, si->created_id);
	camel_object_remove_event (si->store, si->deleted_id);
	camel_object_remove_event (si->store, si->renamed_id);
	camel_object_remove_event (si->store, si->subscribed_id);
	camel_object_remove_event (si->store, si->unsubscribed_id);
	
	g_free (si->display_name);
	camel_object_unref (si->store);
	gtk_tree_row_reference_free (si->row);
	g_hash_table_foreach (si->path_hash, path_hash_free, NULL);
	g_free (si);
}

static void
store_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	struct _EMFolderTreeModelStoreInfo *si = value;
	
	store_info_free (si);
}

static void
uri_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
}

static void
em_folder_tree_model_finalize (GObject *obj)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) obj;
	
	g_free (model->filename);
	if (model->expanded)
		xmlFreeDoc (model->expanded);
	
	g_hash_table_foreach (model->store_hash, store_hash_free, NULL);
	g_hash_table_destroy (model->store_hash);
	
	g_hash_table_foreach (model->uri_hash, uri_hash_free, NULL);
	g_hash_table_destroy (model->uri_hash);
	
	g_hash_table_destroy (model->account_hash);
	g_signal_handler_disconnect (model->accounts, model->account_changed_id);
	g_signal_handler_disconnect (model->accounts, model->account_removed_id);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
	;
}

static void
tree_sortable_iface_init (GtkTreeSortableIface *iface)
{
	;
}


static void
em_folder_tree_model_load_state (EMFolderTreeModel *model, const char *filename)
{
	xmlNodePtr root, node;
	struct stat st;
	
	if (model->expanded)
		xmlFreeDoc (model->expanded);
	
	if (stat (filename, &st) == 0 && (model->expanded = xmlParseFile (filename)))
		return;
	
	/* setup some defaults - expand "Local Folders" and "VFolders" */
	model->expanded = xmlNewDoc ("1.0");
	root = xmlNewDocNode (model->expanded, NULL, "tree-state", NULL);
	xmlDocSetRootElement (model->expanded, root);
	
	node = xmlNewChild (root, NULL, "node", NULL);
	xmlSetProp (node, "name", "local");
	xmlSetProp (node, "expand", "true");
	
	node = xmlNewChild (root, NULL, "node", NULL);
	xmlSetProp (node, "name", "vfolder");
	xmlSetProp (node, "expand", "true");
}


EMFolderTreeModel *
em_folder_tree_model_new (const char *evolution_dir)
{
	EMFolderTreeModel *model;
	char *filename;
	
	model = g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
	gtk_tree_store_set_column_types ((GtkTreeStore *) model, NUM_COLUMNS, col_types);
	gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *) model,
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
	
	filename = g_build_filename (evolution_dir, "mail", "config", "folder-tree-expand-state.xml", NULL);
	em_folder_tree_model_load_state (model, filename);
	model->filename = filename;
	
	return model;
}


static void
account_changed (EAccountList *accounts, EAccount *account, gpointer user_data)
{
	EMFolderTreeModel *model = user_data;
	struct _EMFolderTreeModelStoreInfo *si;
	CamelProvider *provider;
	CamelStore *store;
	CamelException ex;
	char *uri;
	
	if (!(si = g_hash_table_lookup (model->account_hash, account)))
		return;
	
	em_folder_tree_model_remove_store (model, si->store);
	
	if (!(uri = account->source->url))
		return;
	
	camel_exception_init (&ex);
	if (!(provider = camel_provider_get(uri, &ex))) {
		camel_exception_clear (&ex);
		return;
	}
	
	/* make sure the new store belongs in the tree */
	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;
	
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}
	
	em_folder_tree_model_add_store (model, store, account->name);
	camel_object_unref (store);
}

static void
account_removed (EAccountList *accounts, EAccount *account, gpointer user_data)
{
	EMFolderTreeModel *model = user_data;
	struct _EMFolderTreeModelStoreInfo *si;
	
	if (!(si = g_hash_table_lookup (model->account_hash, account)))
		return;
	
	em_folder_tree_model_remove_store (model, si->store);
}


void
em_folder_tree_model_set_folder_info (EMFolderTreeModel *model, GtkTreeIter *iter,
				      struct _EMFolderTreeModelStoreInfo *si,
				      CamelFolderInfo *fi)
{
	GtkTreeRowReference *uri_row, *path_row;
	unsigned int unread;
	GtkTreePath *path;
	GtkTreeIter sub;
	gboolean load;
	struct _CamelFolder *folder;
	gboolean emitted = FALSE;
	
	load = fi->child == NULL && !(fi->flags & (CAMEL_FOLDER_NOCHILDREN | CAMEL_FOLDER_NOINFERIORS));
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
	uri_row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	path_row = gtk_tree_row_reference_copy (uri_row);
	gtk_tree_path_free (path);
	
	g_hash_table_insert (model->uri_hash, g_strdup (fi->uri), uri_row);
	g_hash_table_insert (si->path_hash, g_strdup (fi->path), path_row);
	
	/* HACK: if we have the folder, and its the outbox folder, we need the total count, not unread */
	/* This is duplicated in mail-folder-cache too, should perhaps be functionised */
	unread = fi->unread == -1 ? 0 : fi->unread;
	if (mail_note_get_folder_from_uri(fi->uri, &folder) && folder) {
		if (folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX)) {
			int total;
			
			if ((total = camel_folder_get_message_count (folder)) > 0) {
				int deleted = camel_folder_get_deleted_message_count (folder);
				
				if (deleted != -1)
					total -= deleted;
			}
			
			unread = total > 0 ? total : 0;
		}
		camel_object_unref(folder);
	}
		
	gtk_tree_store_set ((GtkTreeStore *) model, iter,
			    COL_STRING_DISPLAY_NAME, fi->name,
			    COL_POINTER_CAMEL_STORE, si->store,
			    COL_STRING_FOLDER_PATH, fi->path,
			    COL_STRING_URI, fi->uri,
			    COL_UINT_UNREAD, unread,
			    COL_UINT_FLAGS, fi->flags,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_BOOL_LOAD_SUBDIRS, load,
			    -1);
	
	if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
		gtk_tree_store_set ((GtkTreeStore *) model, &sub,
				    COL_STRING_DISPLAY_NAME, _("Loading..."),
				    COL_POINTER_CAMEL_STORE, NULL,
				    COL_STRING_FOLDER_PATH, NULL,
				    COL_BOOL_LOAD_SUBDIRS, FALSE,
				    COL_BOOL_IS_STORE, FALSE,
				    COL_STRING_URI, NULL,
				    COL_UINT_UNREAD, 0,
				    -1);
		
		path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
		g_signal_emit (model, signals[LOADING_ROW], 0, path, iter);
		gtk_tree_path_free (path);
		return;
	}
	
	if (fi->child) {
		fi = fi->child;
		
		do {
			gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
			
			if (!emitted) {
				path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
				g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
				gtk_tree_path_free (path);
				emitted = TRUE;
			}
			
			em_folder_tree_model_set_folder_info (model, &sub, si, fi);
			fi = fi->next;
		} while (fi);
	}
	
	if (!emitted) {
		path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
		g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
		gtk_tree_path_free (path);
	}
}


static void
folder_subscribed (CamelStore *store, CamelFolderInfo *fi, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter parent, iter;
	GtkTreePath *path;
	gboolean load;
	char *dirname;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;
	
	/* make sure we don't already know about it? */
	if (g_hash_table_lookup (si->path_hash, fi->path))
		goto done;
	
	/* get our parent folder's path */
	if (!(dirname = g_path_get_dirname (fi->path)))
		goto done;
	
	if (!strcmp (dirname, "/")) {
		/* user subscribed to a toplevel folder */
		row = si->row;
		g_free (dirname);
	} else {
		row = g_hash_table_lookup (si->path_hash, dirname);
		g_free (dirname);
		
		/* if row is NULL, don't bother adding to the tree,
		 * when the user expands enough nodes - it will be
		 * added auto-magically */
		if (row == NULL)
			goto done;
	}
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &parent, path))) {
		gtk_tree_path_free (path);
		goto done;
	}
	
	gtk_tree_path_free (path);
	
	/* make sure parent's subfolders have already been loaded */
	gtk_tree_model_get ((GtkTreeModel *) model, &parent, COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		goto done;
	
	/* append a new node */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &parent);
	
	em_folder_tree_model_set_folder_info (model, &iter, si, fi);
	
	g_signal_emit (model, signals[FOLDER_ADDED], 0, fi->path, fi->uri);
	
 done:
	
	camel_object_unref (store);
	camel_folder_info_free (fi);
}

static void
folder_subscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_subscribed, store, fi, model);
}

static void
folder_unsubscribed (CamelStore *store, CamelFolderInfo *fi, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;
	
	if (!(row = g_hash_table_lookup (si->path_hash, fi->path)))
		goto done;
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		goto done;
	}
	
	em_folder_tree_model_remove_folders (model, si, &iter);
	
 done:
	
	camel_object_unref (store);
	camel_folder_info_free (fi);
}

static void
folder_unsubscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_unsubscribed, store, fi, model);
}

static void
folder_created_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	/* we only want created events to do more work if we don't support subscriptions */
	if (camel_store_supports_subscriptions (store))
		return;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_subscribed_cb, store, fi, model);
}

static void
folder_deleted_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;
	
	/* we only want deleted events to do more work if we don't support subscriptions */
	if (camel_store_supports_subscriptions (store))
		return;
	
	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_unsubscribed_cb, store, fi, model);
}

static void
folder_renamed (CamelStore *store, CamelRenameInfo *info, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	char *parent, *p;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;
	
	parent = g_strdup_printf ("/%s", info->old_base);
	if (!(row = g_hash_table_lookup (si->path_hash, parent))) {
		g_free (parent);
		goto done;
	}
	g_free (parent);
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		goto done;
	}
	
	em_folder_tree_model_remove_folders (model, si, &iter);
	
	parent = g_strdup (info->new->path);
	p = strrchr(parent, '/');
	g_assert(p);
	*p = 0;
	if (parent == p) {
		/* renamed to a toplevel folder on the store */
		path = gtk_tree_row_reference_get_path (si->row);
	} else {
		if (!(row = g_hash_table_lookup (si->path_hash, parent))) {
			/* NOTE: this should never happen, but I
			 * suppose if it does in reality, we can add
			 * code here to add the missing nodes to the
			 * tree */
			g_assert_not_reached ();
			g_free (parent);
			goto done;
		}
		
		path = gtk_tree_row_reference_get_path (row);
	}
	
	g_free (parent);
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &root, path)) {
		gtk_tree_path_free (path);
		g_assert_not_reached ();
		goto done;
	}
	
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	em_folder_tree_model_set_folder_info (model, &iter, si, info->new);
	
 done:
	
	camel_object_unref (store);
	
	g_free (info->old_base);
	camel_folder_info_free (info->new);
	g_free (info);
}

static void
folder_renamed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelRenameInfo *rinfo, *info = event_data;
	
	camel_object_ref (store);
	
	rinfo = g_new0 (CamelRenameInfo, 1);
	rinfo->old_base = g_strdup (info->old_base);
	rinfo->new = camel_folder_info_clone (info->new);
	
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_renamed, store, rinfo, model);
}

void
em_folder_tree_model_add_store (EMFolderTreeModel *model, CamelStore *store, const char *display_name)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	EAccount *account;
	char *uri;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);
	
	if ((si = g_hash_table_lookup (model->store_hash, store)))
		em_folder_tree_model_remove_store (model, store);
	
	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	
	account = mail_config_get_account_by_source_url (uri);
	
	/* add the store to the tree */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, NULL);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, display_name,
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FOLDER_PATH, "/",
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, TRUE,
			    COL_STRING_URI, uri, -1);
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, &iter);
	row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	
	si = g_new (struct _EMFolderTreeModelStoreInfo, 1);
	si->display_name = g_strdup (display_name);
	camel_object_ref (store);
	si->store = store;
	si->account = account;
	si->row = row;
	si->path_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (model->store_hash, store, si);
	g_hash_table_insert (model->account_hash, account, si);
	
	/* each store has folders... but we don't load them until the user demands them */
	root = iter;
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, _("Loading..."),
			    COL_POINTER_CAMEL_STORE, NULL,
			    COL_STRING_FOLDER_PATH, NULL,
			    COL_BOOL_LOAD_SUBDIRS, FALSE,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_STRING_URI, NULL,
			    COL_UINT_UNREAD, 0,
			    -1);
	
	g_free (uri);
	
	/* listen to store events */
#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
	si->created_id = camel_object_hook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), model);
	si->deleted_id = camel_object_hook_event (store, "folder_deleted", CAMEL_CALLBACK (folder_deleted_cb), model);
	si->renamed_id = camel_object_hook_event (store, "folder_renamed", CAMEL_CALLBACK (folder_renamed_cb), model);
	si->subscribed_id = camel_object_hook_event (store, "folder_subscribed", CAMEL_CALLBACK (folder_subscribed_cb), model);
	si->unsubscribed_id = camel_object_hook_event (store, "folder_unsubscribed", CAMEL_CALLBACK (folder_unsubscribed_cb), model);
	
	g_signal_emit (model, signals[LOADED_ROW], 0, path, &root);
	gtk_tree_path_free (path);
}


static void
em_folder_tree_model_remove_uri (EMFolderTreeModel *model, const char *uri)
{
	GtkTreeRowReference *row;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (uri != NULL);
	
	if ((row = g_hash_table_lookup (model->uri_hash, uri))) {
		g_hash_table_remove (model->uri_hash, uri);
		gtk_tree_row_reference_free (row);
	}
}


static void
em_folder_tree_model_remove_store_info (EMFolderTreeModel *model, CamelStore *store)
{
	struct _EMFolderTreeModelStoreInfo *si;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	g_hash_table_remove (model->store_hash, si->store);
	g_hash_table_remove (model->account_hash, si->account);
	store_info_free (si);
}


void
em_folder_tree_model_remove_folders (EMFolderTreeModel *model, struct _EMFolderTreeModelStoreInfo *si, GtkTreeIter *toplevel)
{
	GtkTreeRowReference *row;
	char *uri, *folder_path;
	gboolean is_store, go;
	GtkTreeIter iter;
	
	if (gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, toplevel)) {
		do {
			GtkTreeIter next = iter;
			
			go = gtk_tree_model_iter_next ((GtkTreeModel *) model, &next);
			em_folder_tree_model_remove_folders (model, si, &iter);
			iter = next;
		} while (go);
	}
	
	gtk_tree_model_get ((GtkTreeModel *) model, toplevel, COL_STRING_URI, &uri,
			    COL_STRING_FOLDER_PATH, &folder_path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if (folder_path && (row = g_hash_table_lookup (si->path_hash, folder_path))) {
		g_hash_table_remove (si->path_hash, folder_path);
		gtk_tree_row_reference_free (row);
	}
	
	em_folder_tree_model_remove_uri (model, uri);
	
	gtk_tree_store_remove ((GtkTreeStore *) model, toplevel);
	
	if (is_store)
		em_folder_tree_model_remove_store_info (model, si->store);
}


void
em_folder_tree_model_remove_store (EMFolderTreeModel *model, CamelStore *store)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
	gtk_tree_path_free (path);
	
	/* recursively remove subfolders and finally the toplevel store */
	em_folder_tree_model_remove_folders (model, si, &iter);
}


static xmlNodePtr
find_xml_node (xmlNodePtr root, const char *name)
{
	xmlNodePtr node;
	char *nname;
	
	node = root->children;
	while (node != NULL) {
		if (!strcmp (node->name, "node")) {
			nname = xmlGetProp (node, "name");
			if (nname && !strcmp (nname, name)) {
				xmlFree (nname);
				return node;
			}
			
			xmlFree (nname);
		}
		
		node = node->next;
	}
	
	return node;
}

gboolean
em_folder_tree_model_get_expanded (EMFolderTreeModel *model, const char *key)
{
	xmlNodePtr node;
	const char *name;
	char *buf, *p;
	
	node = model->expanded ? model->expanded->children : NULL;
	if (!node || strcmp (node->name, "tree-state") != 0)
		return FALSE;
	
	name = buf = g_alloca (strlen (key) + 1);
	p = g_stpcpy (buf, key);
	if (p[-1] == '/')
		p[-1] = '\0';
	p = NULL;
	
	do {
		if ((p = strchr (name, '/')))
			*p = '\0';
		
		if ((node = find_xml_node (node, name))) {
			gboolean expanded;
			
			buf = xmlGetProp (node, "expand");
			expanded = buf && !strcmp (buf, "true");
			xmlFree (buf);
			
			if (!expanded || p == NULL)
				return expanded;
		}
		
		name = p ? p + 1 : NULL;
	} while (name && node);
	
	return FALSE;
}


void
em_folder_tree_model_set_expanded (EMFolderTreeModel *model, const char *key, gboolean expanded)
{
	xmlNodePtr node, parent;
	const char *name;
	char *buf, *p;
	
	if (model->expanded == NULL)
		model->expanded = xmlNewDoc ("1.0");
	
	if (!model->expanded->children) {
		node = xmlNewDocNode (model->expanded, NULL, "tree-state", NULL);
		xmlDocSetRootElement (model->expanded, node);
	} else {
		node = model->expanded->children;
	}
	
	name = buf = g_alloca (strlen (key) + 1);
	p = g_stpcpy (buf, key);
	if (p[-1] == '/')
		p[-1] = '\0';
	p = NULL;
	
	do {
		parent = node;
		if ((p = strchr (name, '/')))
			*p = '\0';
		
		if (!(node = find_xml_node (node, name))) {
			if (!expanded) {
				/* node doesn't exist, so we don't need to set expanded to FALSE */
				return;
			}
			
			/* node (or parent node) doesn't exist, need to add it */
			node = xmlNewChild (parent, NULL, "node", NULL);
			xmlSetProp (node, "name", name);
		}
		
		xmlSetProp (node, "expand", expanded || p ? "true" : "false");
		
		name = p ? p + 1 : NULL;
	} while (name);
}

void
em_folder_tree_model_save_expanded (EMFolderTreeModel *model)
{
	char *dirname;
	
	if (model->expanded == NULL)
		return;
	
	dirname = g_path_get_dirname (model->filename);
	if (camel_mkdir (dirname, 0777) == -1 && errno != EEXIST) {
		g_free (dirname);
		return;
	}
	
	g_free (dirname);
	
	e_xml_save_file (model->filename, model->expanded);
}


static void
expand_foreach_r (EMFolderTreeModel *model, xmlNodePtr parent, const char *dirname, EMFTModelExpandFunc func, void *user_data)
{
	xmlNodePtr node = parent->children;
	char *path, *name, *expand;
	
	while (node != NULL) {
		if (!strcmp (node->name, "node")) {
			name = xmlGetProp (node, "name");
			expand = xmlGetProp (node, "expand");
			
			if (expand && name && !strcmp (expand, "true")) {
				if (dirname)
					path = g_strdup_printf ("%s/%s", dirname, name);
				else
					path = g_strdup (name);
				
				func (model, path, user_data);
				if (node->children)
					expand_foreach_r (model, node, path, func, user_data);
				g_free (path);
			}
			
			xmlFree (expand);
			xmlFree (name);
		}
		
		node = node->next;
	}
}

void
em_folder_tree_model_expand_foreach (EMFolderTreeModel *model, EMFTModelExpandFunc func, void *user_data)
{
	xmlNodePtr root;
	
	root = model->expanded ? model->expanded->children : NULL;
	if (!root || !root->children || strcmp (root->name, "tree-state") != 0)
		return;
	
	expand_foreach_r (model, root, NULL, func, user_data);
}

void
em_folder_tree_model_set_unread_count (EMFolderTreeModel *model, CamelStore *store, const char *path, int unread)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *tree_path;
	GtkTreeIter iter;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (path != NULL);

	u(printf("set unread count %p '%s' %d\n", store, path, unread));

	if (unread < 0)
		unread = 0;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store))) {
		u(printf("  can't find store\n"));
		return;
	}
	
	if (!(row = g_hash_table_lookup (si->path_hash, path))) {
		u(printf("  can't find row\n"));
		return;
	}
	
	tree_path = gtk_tree_row_reference_get_path (row);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, tree_path)) {
		gtk_tree_path_free (tree_path);
		return;
	}
	
	gtk_tree_path_free (tree_path);
	
	gtk_tree_store_set ((GtkTreeStore *) model, &iter, COL_UINT_UNREAD, unread, -1);
}
