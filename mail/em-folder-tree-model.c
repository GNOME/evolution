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

#include <camel/camel-file-utils.h>

#include "mail-config.h"

#include "em-marshal.h"
#include "em-folder-tree-model.h"


static GType col_types[] = {
	G_TYPE_STRING,   /* display name */
	G_TYPE_POINTER,  /* store object */
	G_TYPE_STRING,   /* path */
	G_TYPE_STRING,   /* uri */
	G_TYPE_UINT,     /* unread count */
	G_TYPE_BOOLEAN,  /* is a store node */
	G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
};


/* GObject virtual method overrides */
static void em_folder_tree_model_class_init (EMFolderTreeModelClass *klass);
static void em_folder_tree_model_init (EMFolderTreeModel *model);
static void em_folder_tree_model_finalize (GObject *obj);

/* interface init methods */
static void tree_model_iface_init (GtkTreeModelIface *iface);
static void tree_drag_dest_iface_init (GtkTreeDragDestIface *iface);
static void tree_drag_source_iface_init (GtkTreeDragSourceIface *iface);

/* drag & drop iface methods */
static gboolean model_drag_data_received (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data);
static gboolean model_row_drop_possible  (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data);
static gboolean model_row_draggable      (GtkTreeDragSource *drag_source,
					  GtkTreePath *src_path);
static gboolean model_drag_data_get      (GtkTreeDragSource *drag_source,
					  GtkTreePath *src_path,
					  GtkSelectionData *selection_data);
static gboolean model_drag_data_delete   (GtkTreeDragSource *drag_source,
					  GtkTreePath *src_path);


enum {
	DRAG_DATA_RECEIVED,
	ROW_DROP_POSSIBLE,
	ROW_DRAGGABLE,
	DRAG_DATA_GET,
	DRAG_DATA_DELETE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };


static GtkTreeStore *parent_class = NULL;


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
		static const GInterfaceInfo drag_dest_info = {
			(GInterfaceInitFunc) tree_drag_dest_iface_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo drag_source_info = {
			(GInterfaceInitFunc) tree_drag_source_iface_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GTK_TYPE_TREE_STORE, "EMFolderTreeModel", &info, 0);
		
		g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_DRAG_DEST,
					     &drag_dest_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_DRAG_SOURCE,
					     &drag_source_info);
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
	signals[DRAG_DATA_RECEIVED] =
		g_signal_new ("drag-data-received",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, drag_data_received),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[ROW_DROP_POSSIBLE] =
		g_signal_new ("row-drop-possible",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, row_drop_possible),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[ROW_DRAGGABLE] =
		g_signal_new ("row-draggable",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, row_draggable),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER,
			      G_TYPE_BOOLEAN, 1,
			      G_TYPE_POINTER);
	
	signals[DRAG_DATA_GET] =
		g_signal_new ("drag-data-get",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, drag_data_get),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[DRAG_DATA_DELETE] =
		g_signal_new ("drag-data-delete",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, drag_data_delete),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER,
			      G_TYPE_BOOLEAN, 1,
			      G_TYPE_POINTER);
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	model->store_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->uri_hash = g_hash_table_new (g_str_hash, g_str_equal);
	model->expanded = g_hash_table_new (g_str_hash, g_str_equal);
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

static gboolean
expanded_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	return TRUE;
}

static void
em_folder_tree_model_finalize (GObject *obj)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) obj;
	
	g_hash_table_foreach (model->store_hash, store_hash_free, NULL);
	g_hash_table_destroy (model->store_hash);
	
	g_hash_table_foreach (model->uri_hash, uri_hash_free, NULL);
	g_hash_table_destroy (model->uri_hash);
	
	g_hash_table_foreach (model->expanded, (GHFunc) expanded_free, NULL);
	g_hash_table_destroy (model->expanded);
	
	g_free (model->filename);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
	;
}

static void
tree_drag_dest_iface_init (GtkTreeDragDestIface *iface)
{
	iface->drag_data_received = model_drag_data_received;
	iface->row_drop_possible = model_row_drop_possible;
}

static void
tree_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = model_row_draggable;
	iface->drag_data_get = model_drag_data_get;
	iface->drag_data_delete = model_drag_data_delete;
}


static gboolean
model_drag_data_received (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_dest;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[DRAG_DATA_RECEIVED], 0, dest_path, selection_data, &retval);
	
	return retval;
}

static gboolean
model_row_drop_possible (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_dest;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[ROW_DROP_POSSIBLE], 0, dest_path, selection_data, &retval);
	
	return retval;
}

static gboolean
model_row_draggable (GtkTreeDragSource *drag_source, GtkTreePath *src_path)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_source;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[ROW_DRAGGABLE], 0, src_path, &retval);
	
	return retval;
}

static gboolean
model_drag_data_get (GtkTreeDragSource *drag_source, GtkTreePath *src_path, GtkSelectionData *selection_data)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_source;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[DRAG_DATA_GET], 0, src_path, selection_data, &retval);
	
	return retval;
}

static gboolean
model_drag_data_delete (GtkTreeDragSource *drag_source, GtkTreePath *src_path)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_source;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[DRAG_DATA_DELETE], 0, src_path, &retval);
	
	return retval;
}


static void
em_folder_tree_model_load_state (EMFolderTreeModel *model, const char *filename)
{
	char *node;
	FILE *fp;
	
	g_hash_table_foreach_remove (model->expanded, expanded_free, NULL);
	
	if ((fp = fopen (filename, "r")) == NULL)
		return;
	
	while (camel_file_util_decode_string (fp, &node) != -1)
		g_hash_table_insert (model->expanded, node, GINT_TO_POINTER (TRUE));
	
	fclose (fp);
}


EMFolderTreeModel *
em_folder_tree_model_new (const char *evolution_dir)
{
	EMFolderTreeModel *model;
	char *filename;
	
	model = g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
	gtk_tree_store_set_column_types ((GtkTreeStore *) model, NUM_COLUMNS, col_types);
	
	filename = g_build_filename (evolution_dir, "mail", "config", "folder-tree.state", NULL);
	em_folder_tree_model_load_state (model, filename);
	model->filename = filename;
	
	return model;
}


void
em_folder_tree_model_set_folder_info (EMFolderTreeModel *model, GtkTreeIter *iter,
				      struct _EMFolderTreeModelStoreInfo *si,
				      CamelFolderInfo *fi)
{
	GtkTreeRowReference *uri_row, *path_row;
	unsigned int unread;
	EAccount *account;
	GtkTreePath *path;
	GtkTreeIter sub;
	gboolean load;
	char *node;
	
	load = !fi->child && (fi->flags & CAMEL_FOLDER_CHILDREN) && !(fi->flags & CAMEL_FOLDER_NOINFERIORS);
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
	uri_row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	path_row = gtk_tree_row_reference_copy (uri_row);
	/*gtk_tree_path_free (path);*/
	
	g_hash_table_insert (model->uri_hash, g_strdup (fi->url), uri_row);
	g_hash_table_insert (si->path_hash, g_strdup (fi->path), path_row);
	
	unread = fi->unread_message_count == -1 ? 0 : fi->unread_message_count;
	
	gtk_tree_store_set ((GtkTreeStore *) model, iter,
			    COL_STRING_DISPLAY_NAME, fi->name,
			    COL_POINTER_CAMEL_STORE, si->store,
			    COL_STRING_FOLDER_PATH, fi->path,
			    COL_STRING_URI, fi->url,
			    COL_UINT_UNREAD, unread,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_BOOL_LOAD_SUBDIRS, load,
			    -1);
	
	node = fi->path;
	
	if (fi->child) {
		fi = fi->child;
		
		do {
			gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
			em_folder_tree_model_set_folder_info (model, &sub, si, fi);
			fi = fi->sibling;
		} while (fi);
	} else if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
		gtk_tree_store_set ((GtkTreeStore *) model, &sub,
				    COL_STRING_DISPLAY_NAME, _("Loading..."),
				    COL_POINTER_CAMEL_STORE, si->store,
				    COL_STRING_FOLDER_PATH, fi->path,
				    COL_BOOL_LOAD_SUBDIRS, TRUE,
				    COL_BOOL_IS_STORE, FALSE,
				    COL_STRING_URI, fi->url,
				    COL_UINT_UNREAD, 0,
				    -1);
	}
#if 0
	/* FIXME: need to somehow get access to the appropriate treeview widget... */
	if ((account = mail_config_get_account_by_name (si->display_name)))
		node = g_strdup_printf ("%s:%s", account->uid, node);
	else
		node = g_strdup_printf ("%s:%s", si->display_name, node);
	
	if (g_hash_table_lookup (priv->expanded, node)) {
		printf ("expanding node '%s'\n", node);
		gtk_tree_view_expand_to_path (priv->treeview, path);
	}
	
	gtk_tree_path_free (path);
	g_free (node);
#endif
}


static void
folder_subscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	CamelFolderInfo *fi = event_data;
	GtkTreeRowReference *row;
	GtkTreeIter parent, iter;
	GtkTreePath *path;
	gboolean load;
	char *dirname;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	/* make sure we don't already know about it? */
	if (g_hash_table_lookup (si->path_hash, fi->path))
		return;
	
	/* get our parent folder's path */
	if (!(dirname = g_path_get_dirname (fi->path)))
		return;
	
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
			return;
	}
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &parent, path))) {
		gtk_tree_path_free (path);
		return;
	}
	
	gtk_tree_path_free (path);
	
	/* make sure parent's subfolders have already been loaded */
	gtk_tree_model_get ((GtkTreeModel *) model, &parent, COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		return;
	
	/* append a new node */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &parent);
	
	em_folder_tree_model_set_folder_info (model, &iter, si, fi);
}

static void
folder_unsubscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	CamelFolderInfo *fi = event_data;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	if (!(row = g_hash_table_lookup (si->path_hash, fi->path)))
		return;
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		return;
	}
	
	em_folder_tree_model_remove_folders (model, si, &iter);
}

static void
folder_created_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	/* we only want created events to do more work if we don't support subscriptions */
	if (!camel_store_supports_subscriptions (store))
		folder_subscribed_cb (store, event_data, model);
}

static void
folder_deleted_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	/* we only want deleted events to do more work if we don't support subscriptions */
	if (!camel_store_supports_subscriptions (store))
		folder_unsubscribed_cb (store, event_data, model);
}

static void
folder_renamed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	CamelRenameInfo *info = event_data;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	char *parent, *p;
	
	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;
	
	parent = g_strdup_printf ("/%s", info->old_base);
	if (!(row = g_hash_table_lookup (si->path_hash, parent))) {
		g_free (parent);
		return;
	}
	g_free (parent);
	
	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		return;
	}
	
	em_folder_tree_model_remove_folders (model, si, &iter);
	
	parent = g_strdup (info->new->path);
	if ((p = strrchr (parent + 1, '/')))
		*p = '\0';
	
	if (!strcmp (parent, "/")) {
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
			return;
		}
		
		path = gtk_tree_row_reference_get_path (row);
	}
	
	g_free (parent);
	
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &root, path)) {
		gtk_tree_path_free (path);
		g_assert_not_reached ();
		return;
	}
	
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	em_folder_tree_model_set_folder_info (model, &iter, si, info->new);
}


void
em_folder_tree_model_add_store (EMFolderTreeModel *model, CamelStore *store, const char *display_name)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	EAccount *account;
	char *node, *uri;
	
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);
	
	if ((si = g_hash_table_lookup (model->store_hash, store))) {
		const char *name;
		
		path = gtk_tree_row_reference_get_path (si->row);
		gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
		gtk_tree_path_free (path);
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_STRING_DISPLAY_NAME, (char **) &name, -1);
		
		g_warning ("the store `%s' is already in the folder tree as `%s'",
			   display_name, name);
		
		return;
	}
	
	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	
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
	gtk_tree_path_free (path);
	
	si = g_new (struct _EMFolderTreeModelStoreInfo, 1);
	si->display_name = g_strdup (display_name);
	camel_object_ref (store);
	si->store = store;
	si->row = row;
	si->path_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (model->store_hash, store, si);
	
	/* each store has folders... but we don't load them until the user demands them */
	root = iter;
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, _("Loading..."),
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FOLDER_PATH, "/",
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_STRING_URI, uri,
			    COL_UINT_UNREAD, 0,
			    -1);
	
	g_free (uri);
	
#if 0
	/* FIXME: how to do this now that it is being done in the
	 * model instead of the tree widget code??? need to somehow
	 * get access to the appropriate treeview widget... */
	if ((account = mail_config_get_account_by_name (display_name)))
		node = g_strdup_printf ("%s:/", account->uid);
	else
		node = g_strdup_printf ("%s:/", display_name);
	
	if (g_hash_table_lookup (priv->expanded, node)) {
		path = gtk_tree_model_get_path ((GtkTreeModel *) model, &iter);
		gtk_tree_view_expand_to_path (priv->treeview, path);
		gtk_tree_path_free (path);
	}
	
	g_free (node);
#endif
	
	/* listen to store events */
#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
	si->created_id = camel_object_hook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), model);
	si->deleted_id = camel_object_hook_event (store, "folder_deleted", CAMEL_CALLBACK (folder_deleted_cb), model);
	si->renamed_id = camel_object_hook_event (store, "folder_renamed", CAMEL_CALLBACK (folder_renamed_cb), model);
	si->subscribed_id = camel_object_hook_event (store, "folder_subscribed", CAMEL_CALLBACK (folder_subscribed_cb), model);
	si->unsubscribed_id = camel_object_hook_event (store, "folder_unsubscribed", CAMEL_CALLBACK (folder_unsubscribed_cb), model);
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
	
	if ((row = g_hash_table_lookup (si->path_hash, folder_path))) {
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
	
	if (!(si = g_hash_table_lookup (model->store_hash, store))) {
		g_warning ("the store `%s' is not in the folder tree", si->display_name);
		
		return;
	}
	
	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
	gtk_tree_path_free (path);
	
	/* recursively remove subfolders and finally the toplevel store */
	em_folder_tree_model_remove_folders (model, si, &iter);
}


gboolean
em_folder_tree_model_get_expanded (EMFolderTreeModel *model, const char *key)
{
	if (g_hash_table_lookup (model->expanded, key))
		return TRUE;
	
	return FALSE;
}


void
em_folder_tree_model_set_expanded (EMFolderTreeModel *model, const char *key, gboolean expanded)
{
	gpointer okey, oval;
	
	if (g_hash_table_lookup_extended (model->expanded, key, &okey, &oval)) {
		g_hash_table_remove (model->expanded, okey);
		g_free (okey);
	}
	
	if (expanded)
		g_hash_table_insert (model->expanded, g_strdup (key), GINT_TO_POINTER (TRUE));
}


static void
expanded_save (gpointer key, gpointer value, FILE *fp)
{
	if (!GPOINTER_TO_INT (value))
		return;
	
	camel_file_util_encode_string (fp, key);
}

void
em_folder_tree_model_save_expanded (EMFolderTreeModel *model)
{
	char *dirname, *tmpname;
	FILE *fp;
	int fd;
	
	dirname = g_path_get_dirname (model->filename);
	if (camel_mkdir (dirname, 0777) == -1 && errno != EEXIST) {
		g_free (dirname);
		return;
	}
	
	g_free (dirname);
	tmpname = g_strdup_printf ("%s~", model->filename);
	
	if (!(fp = fopen (tmpname, "w+"))) {
		g_free (tmpname);
		return;
	}
	
	g_hash_table_foreach (model->expanded, (GHFunc) expanded_save, fp);
	
	if (fflush (fp) != 0)
		goto exception;
	
	if ((fd = fileno (fp)) == -1)
		goto exception;
	
	if (fsync (fd) == -1)
		goto exception;
	
	fclose (fp);
	fp = NULL;
	
	if (rename (tmpname, model->filename) == -1)
		goto exception;
	
	g_free (tmpname);
	
	return;
	
 exception:
	
	if (fp != NULL)
		fclose (fp);
	
	unlink (tmpname);
	g_free (tmpname);
}
