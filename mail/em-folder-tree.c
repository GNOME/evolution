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

#include <string.h>

#include <gtk/gtk.h>

#include "em-folder-tree.h"


enum {
	COL_STRING_DISPLAY_NAME,  /* string that appears in the tree */
	COL_POINTER_CAMEL_STORE,  /* CamelStore object */
	COL_STRING_FOLDER_PATH,   /* if node is a folder, the full path of the folder */
	COL_STRING_URI,           /* the uri to get the store or
				   * folder object */
	COL_UINT_UNREAD,          /* unread count */
	
	COL_BOOL_IS_STORE,        /* toplevel store node? */
	COL_BOOL_LOAD_SUBDIRS,    /* %TRUE only if the store/folder
				   * has subfolders which have not yet
				   * been added to the tree */
	COL_LAST
};

static GType col_types[] = {
	G_TYPE_STRING,   /* display name */
	G_TYPE_POINTER,  /* store object */
	G_TYPE_STRING,   /* full_name */
	G_TYPE_STRING,   /* uri */
	G_TYPE_UINT,     /* unread count */
	G_TYPE_BOOLEAN,  /* is a store node */
	G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
};

struct _EMFolderTreePrivate {
	GtkTreeView *treeview;
	
	GHashTable *store_hash;  /* maps CamelStore's to GtkTreePath's */
	
	char *selected_uri;
};

static void em_folder_tree_class_init (EMFolderTreeClass *klass);
static void em_folder_tree_init (EMFolderTree *tree);
static void em_folder_tree_destroy (GtkObject *obj);
static void em_folder_tree_finalize (GObject *obj);

static void tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *ftree);
static gboolean tree_button_press (GtkWidget *treeview, GdkEventButton *event, EMFolderTree *ftree);
static void tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *ftree);


static GtkVBoxClass *parent_class = NULL;


GType
em_folder_tree_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTree),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_init,
		};
		
		type = g_type_register_static (GTK_TYPE_VBOX, "EMFolderTree", &info, 0);
	}
	
	return type;
}

static void
em_folder_tree_class_init (EMFolderTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GTK_TYPE_VBOX);
	
	object_class->finalize = em_folder_tree_finalize;
	gtk_object_class->destroy = em_folder_tree_destroy;
	
	/* FIXME: init signals */
}


static gboolean
subdirs_contain_unread (GtkTreeModel *model, GtkTreeIter *root)
{
	unsigned int unread;
	GtkTreeIter iter;
	
	if (!gtk_tree_model_iter_children (model, &iter, root))
		return FALSE;
	
	do {
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);
		if (unread)
			return TRUE;
		
		if (gtk_tree_model_iter_has_child (model, &iter))
			if (subdirs_contain_unread (model, &iter))
				return TRUE;
	} while (gtk_tree_model_iter_next (model, &iter));
	
	return FALSE;
}


enum {
	FOLDER_ICON_NORMAL,
	FOLDER_ICON_INBOX,
	FOLDER_ICON_OUTBOX,
	FOLDER_ICON_TRASH
};

static GdkPixbuf *folder_icons[4];

static void
render_pixbuf (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
	       GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	static gboolean initialised = FALSE;
	GdkPixbuf *pixbuf = NULL;
	gboolean is_store;
	char *path;
	
	if (!initialised) {
		folder_icons[0] = gdk_pixbuf_load_from_file (EVOLUTION_ICONSDIR "/folder-mini.png");
		folder_icons[1] = gdk_pixbuf_load_from_file (EVOLUTION_ICONSDIR "/inbox-mini.png");
		folder_icons[2] = gdk_pixbuf_load_from_file (EVOLUTION_ICONSDIR "/outbox-mini.png");
		folder_icons[3] = gdk_pixbuf_load_from_file (EVOLUTION_ICONSDIR "/evolution-trash-mini.png");
		initialised = TRUE;
	}
	
	gtk_tree_model_get (model, iter, COL_STRING_FOLDER_PATH, &path,
			    COL_BOO_IS_STORE, &is_store, -1);
	
	if (!is_store) {
		if (!strcasecmp (name, "/Inbox"))
			pixbuf = folder_icons[FOLDER_ICON_INBOX];
		else if (!strcasecmp (name, "/Outbox"))
			pixbuf = folder_icons[FOLDER_ICON_OUTBOX];
		else if (!strcasecmp (name, "/Trash"))
			pixbuf = folder_icons[FOLDER_ICON_TRASH];
		else
			pixbuf = folder_icons[FOLDER_ICON_NORMAL];
	}
	
	g_object_set (renderer, "pixbuf", pixbuf, -1);
}

static void
render_display_name (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gboolean is_store, bold;
	unsigned int unread;
	char *name;
	
	gtk_tree_model_get (model, iter, COL_STRING_DISPLAY_NAME, &name,
			    COL_BOO_IS_STORE, &is_store,
			    COL_UINT_UNREAD, &unread, -1);
	
	if (!(bold = is_store || unread)) {
		if (gtk_tree_model_iter_has_child (model, iter))
			bold = subdirs_contain_unread (model, iter);
	}
	
	g_object_set (renderer, "text", name,
		      "weight", bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      "foreground_set", unread ? TRUE : FALSE,
		      "foreground", unread ? "#0000ff" : "#000000", NULL);
}

static GtkTreeView *
folder_tree_new (void)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeStore *model;
	GtkWidget *tree;
	
	model = gtk_tree_store_newv (COL_LAST, col_types);
	tree = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
	
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column ((GtkTreeView *) tree, column);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_pixbuf, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_display_name, NULL, NULL);
	/*gtk_tree_view_insert_column_with_attributes ((GtkTreeView *) tree, -1, "",
	  renderer, "text", 0, NULL);*/
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) tree);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	gtk_tree_view_set_headers_visible ((GtkTreeView *) tree, FALSE);
	
	return (GtkTreeView *) tree;
}

static void
em_folder_tree_init (EMFolderTree *tree)
{
	struct _EMFolderTreePrivate *priv;
	GtkTreeSelection *selection;
	GtkWidget *scrolled;
	
	priv = g_new (struct _EMFolderTreePrivate, 1);
	priv->store_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	priv->uri_hash = g_hash_table_new (g_str_hash, g_str_equal);
	priv->selected_uri = NULL;
	
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
	
	priv->treeview = folder_tree_new ();
	gtk_widget_show ((GtkWidget *) priv->treeview);
	
	g_signal_connect (priv->treeview, "row-expanded", G_CALLBACK (tree_row_expanded), tree);
	g_signal_connect (priv->treeview, "button-press-event", G_CALLBACK (tree_button_press), tree);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->treeview);
	g_signal_connect (selection, "changed", G_CALLBACK (tree_selection_changed), tree);
	
	gtk_container_add ((GtkContainer *) scrolled, (GtkWidget *) priv->treeview);
	gtk_widget_show (scrolled);
	
	gtk_box_pack_start ((GtkBox *) tree, scrolled, TRUE, TRUE, 0);
}

static void
store_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	gtk_tree_path_free (value);
	camel_object_unref (key);
}

static void
em_folder_tree_finalize (GObject *obj)
{
	EMFolderTree *tree = (EMFolderTree *) obj;
	
	g_hash_table_foreach (tree->priv->store_hash, store_hash_free, NULL);
	g_hash_table_destroy (tree->priv->store_hash);
	
	g_free (tree->priv->selected_uri);
	g_free (tree->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_folder_tree_destroy (GtkObject *obj)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}


GtkWidget *
em_folder_tree_new (void)
{
	return g_object_new (EM_TYPE_FOLDER_TREE, NULL);
}


static void
tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *ftree)
{
	CamelFolderInfo *fi, *child;
	CamelStore *store;
	GtkTreeStore *model;
	GtkTreeIter iter;
	char *full_name;
	gboolean load;
	
	model = (GtkTreeStore *) gtk_tree_view_get_model (treeview);
	
	gtk_tree_model_get ((GtkTreeModel *) model, root,
			    COL_STRING_FOLDER_NAME, &full_name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_LOAD_SUBDIRS, &load,
			    -1);
	if (!load)
		return;
	
	/* get the first child (which will be a dummy if we haven't loaded the child folders yet) */
	gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, root);
	
	/* FIXME: are there any flags we want to pass when getting folder-info's? */
	camel_exception_init (&ex);
	if (!(fi = camel_store_get_folder_info (store, full_name, 0, &ex))) {
		/* FIXME: report error to user? or simply re-collapse node? or both? */
		camel_exception_clear (&ex);
		return;
	}
	
	if (!(child = fi->child)) {
		/* no children afterall... remove the "Loading..." placeholder node */
		gtk_tree_store_remove (model, &iter);
	} else {
		do {
			load = (child->flags & CAMEL_FOLDER_CHILDREN) && !(child->flags & CAMEL_FOLDER_NOINFERIORS);
			
			gtk_tree_store_set (model, &iter,
					    COL_STRING_DISPLAY_NAME, child->name,
					    COL_POINTER_CAMEL_STORE, store,
					    COL_STRING_FOLDER_PATH, child->path,
					    COL_STRING_URI, child->url,
					    COL_UINT_UNREAD, child->unread_message_count,
					    COL_BOOL_IS_STORE, FALSE,
					    COL_BOOL_LOAD_SUBDIRS, load,
					    -1);
			
			if ((child = child->sibling) != NULL)
				gtk_tree_store_append (model, &iter, root);
		} while (child != NULL);
	}
	
	gtk_tree_store_set (model, root, COL_BOOL_LOAD_SUBDIRS, FALSE);
	
	camel_store_free_folder_info (store, fi);
}


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
	d(printf ("folder created %s\n", result == E_STORAGE_OK ? "ok" : "failed"));
}

static void
emc_popup_new_folder_response (EMFolderSelector *emfs, guint response, MailComponent *mc)
{
	/* FIXME: port this too :\ */
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
		
		camel_exception_free (ex);
		
		g_free (full);
		g_free (path);
		
		/* Blah, this should just use camel, we get better error reporting if we do too */
		/*e_storage_set_async_create_folder(mc->priv->storage_set, path, "mail", "", emc_popup_new_folder_create, mc);*/
	}
	
	gtk_widget_destroy ((GtkWidget *) emfs);
}

static void
emc_popup_new_folder (GtkWidget *w, EMFolderTree *folder_tree)
{
	GtkWidget *dialog;
	
	/* FIXME: ugh, need to port this (and em_folder_selector*) to use EMFolderTree I guess */
	dialog = em_folder_selector_create_new (mc->priv->storage_set, 0, _("Create folder"), _("Specify where to create the folder:"));
	em_folder_selector_set_selected ((EMFolderSelector *) dialog, mc->priv->context_path);
	g_signal_connect (dialog, "response", G_CALLBACK (emc_popup_new_folder_response), mc);
	gtk_widget_show (dialog);
}

static void
em_delete_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;
		
		if (fi->child)
			em_delete_rec (store, fi->child, ex);
		if (camel_exception_is_set (ex))
			return;
		
		d(printf ("deleting folder '%s'\n", fi->full_name));
		
		/* shouldn't camel do this itself? */
		if (camel_store_supports_subscriptions (store))
			camel_store_unsubscribe_folder (store, fi->full_name, NULL);
		
		folder = camel_store_get_folder (store, fi->full_name, 0, NULL);
		if (folder) {
			GPtrArray *uids = camel_folder_get_uids (folder);
			int i;
			
			camel_folder_freeze (folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message (folder, uids->pdata[i]);
			camel_folder_sync (folder, TRUE, NULL);
			camel_folder_thaw (folder);
			camel_folder_free_uids (folder, uids);
		}
		
		camel_store_delete_folder (store, fi->full_name, ex);
		if (camel_exception_is_set (ex))
			return;
		
		fi = fi->sibling;
	}
}

static void
em_delete_folders (CamelStore *store, const char *base, CamelException *ex)
{
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	CamelFolderInfo *fi;
	
	if (camel_store_supports_subscriptions (store))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	fi = camel_store_get_folder_info (store, base, flags, ex);
	if (camel_exception_is_set (ex))
		return;
	
	em_delete_rec (store, fi, ex);
	camel_store_free_folder_info (store, fi);
}

static void
emc_popup_delete_response (GtkWidget *dialog, guint response, EMFolderTree *folder_tree)
{
	struct _EMFolderTreePrivate *priv = folder_tree->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_OK)
		return;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store, -1);
	
	/* FIXME: need to hook onto store changed event and delete view as well, somewhere else tho */
	camel_exception_init (&ex);
	em_delete_folders (store, path, &ex);
	if (camel_exception_is_set (&ex)) {
		e_notice (NULL, GTK_MESSAGE_ERROR, _("Could not delete folder: %s"), ex.desc);
		camel_exception_clear (&ex);
	}
}

static void
emc_popup_delete_folder (GtkWidget *item, EMFolderTree *folder_tree)
{
	struct _EMFolderTreePrivate *priv = folder_tree->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	char *title, *path;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path, -1);
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Really delete folder \"%s\" and all of its subfolders?"),
					 path);
	
	gtk_dialog_add_button ((GtkDialog *) dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button ((GtkDialog *) dialog, GTK_STOCK_DELETE, GTK_RESPONSE_OK);
	
	gtk_dialog_set_default_response ((GtkDialog *) dialog, GTK_RESPONSE_OK);
	gtk_container_set_border_width ((GtkContainer *) dialog, 6); 
	gtk_box_set_spacing ((GtkBox *) ((GtkDialog *) dialog)->vbox, 6);
	
	title = g_strdup_printf (_("Delete \"%s\""), path);
	gtk_window_set_title ((GtkWindow *) dialog, title);
	g_free (title);
	
	g_signal_connect (dialog, "response", G_CALLBACK (emc_popup_delete_response), folder_tree);
	gtk_widget_show (dialog);
}

static void
emc_popup_rename_folder (GtkWidget *item, EMFolderTree *folder_tree)
{
	struct _EMFolderTreePrivate *priv = folder_tree->priv;
	char *prompt, *folder_path, *name, *new_name, *uri;
	GtkTreeSelection *selection;
	gboolean done = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelStore *store;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &folder_path,
			    COL_STRING_DISPLAY_NAME, &name,
			    COL_POINTER_STORE, &store,
			    COL_STRING_URI, &uri, -1);
	
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), name);
	while (!done) {
		const char *why;
		
		new = e_request_string (NULL, _("Rename Folder"), prompt, name);
		if (new_name == NULL || !strcmp (name, new_name)) {
			/* old name == new name */
			done = TRUE;
		} else {
			CamelFolderInfo *fi;
			CamelException ex;
			char *base, *path;
			
			/* FIXME: we can't use the os independent path crap here, since we want to control the format */
			base = g_path_get_dirname (folder_path);
			path = g_build_filename (base, new_name, NULL);
			
			camel_exception_init (&ex);
			if ((fi = camel_store_get_folder_info (store, path, CAMEL_STORE_FOLDER_INFO_FAST, &ex)) != NULL) {
				camel_store_free_folder_info (store, fi);
				
				e_notice (NULL, GTK_MESSAGE_ERROR,
					  _("A folder named \"%s\" already exists. Please use a different name."),
					  new_name);
			} else {
				const char *oldpath, *newpath;
				
				oldpath = folder_path + 1;
				newpath = path + 1;
				
				printf ("renaming %s to %s\n", oldpath, newpath);
				
				camel_exception_clear (&ex);
				camel_store_rename_folder (store, oldpath, newpath, &ex);
				if (camel_exception_is_set (&ex)) {
					e_notice (NULL, GTK_MESSAGE_ERROR, _("Could not rename folder: %s"), ex.desc);
					camel_exception_clear (&ex);
				}
				
				done = TRUE;
			}
			
			g_free (path);
			g_free (base);
		}
		
		g_free (new_name);
	}
}

struct _prop_data {
	void *object;
	CamelArgV *argv;
	GtkWidget **widgets;
};

static void
emc_popup_properties_response (GtkWidget *dialog, int response, struct _prop_data *prop_data)
{
	CamelArgV *argv = prop_data->argv;
	int i;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}
	
	for (i = 0; i < argv->argc; i++) {
		CamelArg *arg = &argv->argv[i];
		
		switch (arg->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			arg->ca_int = gtk_toggle_button_get_active ((GtkToggleButton *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_STR:
			g_free (arg->ca_str);
			arg->ca_str = gtk_entry_get_text ((GtkEntry *) prop_data->widgets[i]);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
	
	camel_object_setv (prop_data->object, NULL, argv);
	gtk_widget_destroy (dialog);
}

static void
emc_popup_properties_free (void *data)
{
	struct _prop_data *prop_data = data;
	int i;
	
	for (i = 0; i < prop_data->argv->argc; i++) {
		if ((prop_data->argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			g_free (prop_data->argv->argv[i].ca_str);
	}
	
	camel_object_unref (prop_data->object);
	g_free (prop_data->argv);
	g_free (prop_data);
}

static void
emc_popup_properties_got_folder (char *uri, CamelFolder *folder, void *data)
{
	GtkWidget *dialog, *w, *table, *label;
	struct _prop_data *prop_data;
	CamelArgGetV *arggetv;
	CamelArgV *argv;
	GSList *list, *l;
	gint32 count, i;
	char *name;
	int row = 1;
	
	if (folder == NULL)
		return;
	
	camel_object_get (folder, NULL, CAMEL_FOLDER_PROPERTIES, &list, CAMEL_FOLDER_NAME, &name, NULL);
	
	dialog = gtk_dialog_new_with_buttons (_("Folder properties"), NULL,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	
	/* TODO: maybe we want some basic properties here, like message counts/approximate size/etc */
	w = gtk_frame_new (_("Properties"));
	gtk_widget_show (w);
	gtk_box_pack_start ((GtkBox *) ((GtkDialog *) dialog)->vbox, w, TRUE, TRUE, 6);
	
	table = gtk_table_new (g_slist_length (list) + 1, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add ((GtkContainer *) w, table);
	
	label = gtk_label_new (_("Folder Name"));
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 3, 0);
	
	label = gtk_label_new (name);
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 3, 0);
	
	/* build an arggetv/argv to retrieve/store the results */
	count = g_slist_length (list);
	arggetv = g_malloc0 (sizeof (*arggetv) + (count - CAMEL_ARGV_MAX) * sizeof (arggetv->argv[0]));
	arggetv->argc = count;
	argv = g_malloc0 (sizeof (*argv) + (count - CAMEL_ARGV_MAX) * sizeof (argv->argv[0]));
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
	
	camel_object_getv (folder, NULL, arggetv);
	g_free (arggetv);
	
	prop_data = g_malloc0 (sizeof (*prop_data));
	prop_data->widgets = g_malloc0 (sizeof (prop_data->widgets[0]) * count);
	prop_data->argv = argv;
	
	/* setup the ui with the values retrieved */
	l = list;
	i = 0;
	while (l) {
		CamelProperty *prop = l->data;
		
		switch (prop->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			w = gtk_check_button_new_with_label (prop->description);
			gtk_toggle_button_set_active ((GtkToggleButton *) w, argv->argv[i].ca_int != 0);
			gtk_widget_show (w);
			gtk_table_attach ((GtkTable *) table, w, 0, 2, row, row + 1, 0, 0, 3, 3);
			prop_data->widgets[i] = w;
			break;
		case CAMEL_ARG_STR:
			label = gtk_label_new (prop->description);
			gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
			gtk_widget_show (label);
			gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 3, 3);
			
			w = gtk_entry_new ();
			gtk_widget_show (w);
			if (argv->argv[i].ca_str) {
				gtk_entry_set_text ((GtkEntry *) w, argv->argv[i].ca_str);
				camel_object_free (folder, argv->argv[i].tag, argv->argv[i].ca_str);
				argv->argv[i].ca_str = NULL;
			}
			gtk_table_attach ((GtkTable *) table, w, 1, 2, row, row + 1, GTK_FILL, 0, 3, 3);
			prop_data->widgets[i] = w;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		
		row++;
		l = l->next;
	}
	
	prop_data->object = folder;
	camel_object_ref (folder);
	
	camel_object_free (folder, CAMEL_FOLDER_PROPERTIES, list);
	camel_object_free (folder, CAMEL_FOLDER_NAME, name);
	
	/* we do 'apply on ok' ... since instant apply may apply some very long running tasks */
	
	g_signal_connect (dialog, "response", G_CALLBACK (emc_popup_properties_response), prop_data);
	g_object_set_data_full ((GObject *) dialog, "e-prop-data", prop_data, emc_popup_properties_free);
	gtk_widget_show (dialog);
}

static void
emc_popup_properties (GtkWidget *item, EMFolderTree *folder_tree)
{
	struct _EMFolderTreePrivate *priv = folder_tree->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);
	
	mail_get_folder (uri, 0, emc_popup_properties_got_folder, folder_tree, mail_thread_new);
}

static EMPopupItem emc_popup_menu[] = {
#if 0
	{ EM_POPUP_ITEM, "00.emc.00", N_("_View"), G_CALLBACK (emc_popup_view), NULL, NULL, 0 },
	{ EM_POPUP_ITEM, "00.emc.01", N_("Open in _New Window"), G_CALLBACK (emc_popup_open_new), NULL, NULL, 0 },

	{ EM_POPUP_BAR, "10.emc" },
#endif
	{ EM_POPUP_ITEM, "10.emc.00", N_("_Copy"), G_CALLBACK (emc_popup_copy), NULL, "folder-copy-16.png", 0 },
	{ EM_POPUP_ITEM, "10.emc.01", N_("_Move"), G_CALLBACK (emc_popup_move), NULL, "folder-move-16.png", 0 },

	{ EM_POPUP_BAR, "20.emc" },
	{ EM_POPUP_ITEM, "20.emc.00", N_("_New Folder..."), G_CALLBACK (emc_popup_new_folder), NULL, "folder-mini.png", 0 },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Delete"), G_CALLBACK (emc_popup_delete_folder), NULL, "evolution-trash-mini.png", 0 },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Rename"), G_CALLBACK (emc_popup_rename_folder), NULL, NULL, 0 },

	{ EM_POPUP_BAR, "80.emc" },
	{ EM_POPUP_ITEM, "80.emc.00", N_("_Properties..."), G_CALLBACK (emc_popup_properties), NULL, "configure_16_folder.xpm", 0 },
};

static gboolean
tree_button_press (GtkWidget *treeview, GdkEventButton *event, EMFolderTree *ftree)
{
	GSList *menus = NULL;
	GtkMenu *menu;
	EMPopup *emp;
	int i;
	
	if (event->button != 3)
		return FALSE;
	
	/* handle right-click by opening a context menu */
	emp = em_popup_new ("com.ximian.mail.storageset.popup.select");
	
	for (i = 0; i < sizeof (emc_popup_menu) / sizeof (emc_popup_menu[0]); i++) {
		EMPopupItem *item = &emc_popup_menu[i];
		
		item->activate_data = ftree;
		menus = g_slist_prepend (menus, item);
	}
	
	em_popup_add_items (emp, menus, (GDestroyNotify) g_slist_free);
	
	menu = em_popup_create_menu_once (emp, NULL, 0, 0);
	
	if (event == NULL || event->type == GDK_KEY_PRESS) {
		/* FIXME: menu pos function */
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, event->key.time);
	} else {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button.button, event->button.time);
	}
	
	return TRUE;
}


static void
tree_selection_changed (GtkTreeSelection *selection, EMFolderTRee *ftree)
{
	/* new folder has been selected */
}


void
em_folder_tree_add_store (EMFolderTree *tree, CamelStore *store, const char *display_name)
{
	struct _EMFolderTreePrivate *priv;
	GtkTreeIter root, iter;
	GtkTreeStore *model;
	GtkTreePath *path;
	char *uri;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (tree));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);
	
	priv = tree->priv;
	model = (GtkTreeStore *) gtk_tree_view_get_model (priv->treeview);
	
	if ((path = g_hash_table_lookup (priv->store_hash, store))) {
		const char *name;
		
		gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_STRING_DISPLAY_NAME, (char **) &name, -1);
		
		g_warning ("the store `%s' is already in the folder tree as `%s'",
			   display_name, name);
		
		return;
	}
	
	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	
	/* add the store to the tree */
	gtk_tree_store_append (model, &iter, NULL);
	gtk_tree_store_set (model, &iter,
			    COL_STRING_DISPLAY_NAME, display_name,
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FOLDER_PATH, "/",
			    COL_BOOL_IS_STORE, TRUE,
			    COL_STRING_URI, uri, -1);
	
	camel_object_ref (store);
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, &iter);
	g_hash_table_insert (priv->store_hash, store, path);
	g_free (uri);
	
	/* each store has folders... but we don't load them until the user demands them */
	root = iter;
	gtk_tree_store_append (model, &iter, &root);
	gtk_tree_store_set (model, &iter,
			    COL_STRING_DISPLAY_NAME, _("Loading..."),
			    COL_POINTER_CAMEL_STORE, store,
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_STRING_URI, uri,
			    COL_UINT_UNREAD, 0,
			    -1);
}


void
em_folder_tree_remove_store (EMFolderTree *tree, CamelStore *store)
{
	struct _EMFolderTreePrivate *priv;
	GtkTreeStore *model;
	GtkTreeIter iter;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (tree));
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	priv = tree->priv;
	model = (GtkTreeStore *) gtk_tree_view_get_model (priv->treeview);
	
	if (!(path = g_hash_table_lookup (priv->store_hash, store))) {
		g_warning ("the store `%s' is not in the folder tree", display_name);
		
		return;
	}
	
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
	
	gtk_tree_store_remove (model, &iter);
	g_hash_table_remove (priv->store_hash, store);
	
	camel_object_unref (store);
	gtk_tree_path_free (path);
}
