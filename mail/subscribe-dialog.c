/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* subscribe-dialog.c: Subscribe dialog */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *           Peter Williams <peterw@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>

#include <gal/e-table/e-tree-scrolled.h>
#include <gal/e-table/e-tree-memory-callbacks.h>
#include <gal/e-table/e-tree.h>

#include <pthread.h>

#include "evolution-shell-component-utils.h"
#include "mail.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "camel/camel-exception.h"
#include "camel/camel-store.h"
#include "camel/camel-session.h"
#include "subscribe-dialog.h"

#include "art/empty.xpm"
#include "art/mark.xpm"

#define d(x) 

/* Things to test.
 * - Feature
 *   + How to check that it works.
 *
 * - Proper stores displayed
 *   + Skip stores that don't support subscriptions
 *   + Skip disabled stores
 * - Changing subscription status
 *   + Select single folder, double-click row -> toggled
 *   + Select multiple folders, press subscribe -> all selected folders end up subscribed
 * - (un)Subscribing from/to already (un)subscribed folder
 *   + Check that no IMAP command is sent
 * - Switching views between stores
 *   + Proper tree shown
 * - No crashes when buttons are pressed with "No store" screen
 *   + obvious
 * - Restoring filter settings when view switched
 *   + Enter search, change view, change back -> filter checked and search entry set
 *   + Clear search, change view, change back -> "all" checked
 * - Cancelling in middle of get_store 
 *   + Enter invalid hostname, open dialog, click Close
 * - Cancelling in middle if listing 
 *   + Open large directory, click Close
 * - Cancelling in middle of subscription op
 *   + How to test?
 * - Test with both IMAP and NNTP
 *   + obvious
 * - Verify that refresh view works
 *   + obvious
 * - No unnecessary tree rebuilds
 *   + Show All folders, change filter with empty search -> no tree rebuild
 *   + Converse
 * - No out of date tree
 *   + Show All Folders, change to filter with a search -> tree rebuild
 * - Tree construction logic (mostly IMAP-specific terminology)
 *   + Tree is created progressively
 *   + No wasted LIST responses
 *   + No extraneous LIST commands
 *   + Specifying "folder names begin with" works
 *   + Always show folders below IMAP namespace (no escaping the namespace)
 *   + Don't allow subscription to NoSelect folders
 *   + IMAP accounts always show INBOX
 * - Shell interactions
 *   + Folders are properly created / delete from folder tree when subscribed / unsubscribed
 *   + Folders with spaces in names / 8bit chars
 *   + Toplevel as well as subfolders
 *   + Mail Folder Cache doesn't complain
 * - No ETable wackiness
 *   + Verify columns cannot be DnD'd
 *   + Alphabetical order always
 * - UI cleanliness
 *   + Keybindings work
 *   + Some widget has focus by default
 *   + Escape / enter work
 *   + Close button works
 */

/* FIXME: we should disable/enable the subscribe/unsubscribe buttons as
 * appropriate when only a single message is selected. We need a
 * mechanism to learn when the selected folder's subscription status
 * changes, so when the user double-clicks it (eg) the buttons can
 * (de)sensitize appropriately. See Ximian bug #7673.
 */

/*#define NEED_TOGGLE_SELECTION*/

typedef struct _FolderETree              FolderETree;
typedef struct _FolderETreeClass         FolderETreeClass;

struct _FolderETree {
	ETreeMemory parent;
	ETreePath root;

	GHashTable *scan_ops;
	GHashTable *subscribe_ops;

	CamelStore *store;
	EvolutionStorage *e_storage;
	char *service_name;
	char *search;
};

struct _FolderETreeClass {
	ETreeMemoryClass parent;
};

static GtkObjectClass *folder_etree_parent_class = NULL;

typedef struct _FolderETreeExtras      FolderETreeExtras;
typedef struct _FolderETreeExtrasClass FolderETreeExtrasClass;

enum {
	FOLDER_COL_SUBSCRIBED,
	FOLDER_COL_NAME,
	FOLDER_COL_LAST
};

struct _FolderETreeExtras {
	ETableExtras parent;
	GdkPixbuf *toggles[2];
};

struct _FolderETreeExtrasClass {
	ETableExtrasClass parent;
};

static GtkObjectClass *ftree_extras_parent_class = NULL;

/* util */

static void
recursive_add_folder (EvolutionStorage *storage, const char *path, const char *name, const char *url)
{
	char *parent, *pname, *p;

	p = strrchr (path, '/');
	if (p && p != path) {
		parent = g_strndup (path, p - path);
		if (!evolution_storage_folder_exists (storage, parent)) {
			p = strrchr (parent, '/');
			if (p)
				pname = g_strdup (p + 1);
			else
				pname = g_strdup ("");
			recursive_add_folder (storage, parent, pname, "");
			g_free (pname);
		}
		g_free (parent);
	}

	evolution_storage_new_folder (storage, path, name, "mail", url, name, FALSE, TRUE);
}

/* ** Get one level of folderinfo ****************************************** */

typedef void (*SubscribeShortFolderinfoFunc) (CamelStore *store, char *prefix, CamelFolderInfo *info, gpointer data);

int subscribe_get_short_folderinfo (FolderETree *ftree, const char *prefix,
				    SubscribeShortFolderinfoFunc func, gpointer user_data);

struct _get_short_folderinfo_msg {
	struct _mail_msg msg;

	char *prefix;

	FolderETree *ftree;
	CamelFolderInfo *info;

	SubscribeShortFolderinfoFunc func;
	gpointer user_data;
};

static char *
get_short_folderinfo_desc (struct _mail_msg *mm, int done)
{
	struct _get_short_folderinfo_msg *m = (struct _get_short_folderinfo_msg *) mm;
	char *ret, *name;

	name = camel_service_get_name (CAMEL_SERVICE (m->ftree->store), TRUE);

	if (m->prefix)
		ret = g_strdup_printf (_("Scanning folders under %s on \"%s\""), m->prefix, name);
	else
		ret = g_strdup_printf (_("Scanning root-level folders on \"%s\""), name);

	g_free (name);
	return ret;
}

static void
get_short_folderinfo_get (struct _mail_msg *mm)
{
	struct _get_short_folderinfo_msg *m = (struct _get_short_folderinfo_msg *) mm;

	m->info = camel_store_get_folder_info (m->ftree->store, m->prefix, CAMEL_STORE_FOLDER_INFO_FAST, &mm->ex);
}

static void
get_short_folderinfo_got (struct _mail_msg *mm)
{
	struct _get_short_folderinfo_msg *m = (struct _get_short_folderinfo_msg *) mm;

	if (camel_exception_is_set (&mm->ex))
		g_warning ("Error getting folder info from store at %s: %s",
			   camel_service_get_url (CAMEL_SERVICE (m->ftree->store)),
			   camel_exception_get_description (&mm->ex));

	/* 'done' is probably guaranteed to fail, but... */

	if (m->func)
		m->func (m->ftree->store, m->prefix, m->info, m->user_data);
}

static void
get_short_folderinfo_free (struct _mail_msg *mm)
{
	struct _get_short_folderinfo_msg *m = (struct _get_short_folderinfo_msg *) mm;

	camel_store_free_folder_info (m->ftree->store, m->info);
	gtk_object_unref (GTK_OBJECT (m->ftree));

	g_free (m->prefix); /* may be NULL but that's ok */
}

static struct _mail_msg_op get_short_folderinfo_op = {
	get_short_folderinfo_desc,
	get_short_folderinfo_get,
	get_short_folderinfo_got,
	get_short_folderinfo_free,
};

int
subscribe_get_short_folderinfo (FolderETree *ftree, 
				const char *prefix,
				SubscribeShortFolderinfoFunc func, 
				gpointer user_data)
{
	struct _get_short_folderinfo_msg *m;
	int id;

	m = mail_msg_new (&get_short_folderinfo_op, NULL, sizeof(*m));

	m->ftree = ftree;
	gtk_object_ref (GTK_OBJECT (ftree));
	
	if (prefix)
		m->prefix = g_strdup (prefix);
	else
		m->prefix = NULL;

	m->func = func;
	m->user_data = user_data;

	id = m->msg.seq;
	e_thread_put (mail_thread_queued, (EMsg *)m);
	return id;
}

/* ** Subscribe folder operation **************************************** */

typedef void (*SubscribeFolderCallback) (const char *, const char *, gboolean, gboolean, gpointer);

struct _subscribe_msg {
	struct _mail_msg         msg;

	CamelStore              *store;
	gboolean                 subscribe;
	char                    *full_name;
	char                    *name;

	SubscribeFolderCallback  cb;
	gpointer                 cb_data;
};

static char *
subscribe_folder_desc (struct _mail_msg *mm, int done)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *) mm;

	if (m->subscribe)
		return g_strdup_printf (_("Subscribing to folder \"%s\""), m->name);
	else
		return g_strdup_printf (_("Unsubscribing to folder \"%s\""), m->name);
}

static void 
subscribe_folder_subscribe (struct _mail_msg *mm)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *) mm;
	
	if (m->subscribe)
		camel_store_subscribe_folder (m->store, m->full_name, &mm->ex);
	else
		camel_store_unsubscribe_folder (m->store, m->full_name, &mm->ex);
}

static void 
subscribe_folder_subscribed (struct _mail_msg *mm)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *) mm;
	
	if (m->cb)
		(m->cb) (m->full_name, m->name, m->subscribe, 
			 !camel_exception_is_set (&mm->ex), m->cb_data);
}

static void 
subscribe_folder_free (struct _mail_msg *mm)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *) mm;

	g_free (m->name);
	g_free (m->full_name);
	
	camel_object_unref (CAMEL_OBJECT (m->store));
}

static struct _mail_msg_op subscribe_folder_op = {
	subscribe_folder_desc,
	subscribe_folder_subscribe,
	subscribe_folder_subscribed,
	subscribe_folder_free,
};

static int
subscribe_do_subscribe_folder (CamelStore *store, const char *full_name, const char *name,
			       gboolean subscribe, SubscribeFolderCallback cb, gpointer cb_data)
{
	struct _subscribe_msg *m;
	int id;

	g_return_val_if_fail (CAMEL_IS_STORE (store), 0);
	g_return_val_if_fail (full_name, 0);

	m            = mail_msg_new (&subscribe_folder_op, NULL, sizeof(*m));
	m->store     = store;
	m->subscribe = subscribe;
	m->name      = g_strdup (name);
	m->full_name = g_strdup (full_name);
	m->cb        = cb;
	m->cb_data   = cb_data;

	camel_object_ref (CAMEL_OBJECT (store));

	id = m->msg.seq;
	e_thread_put (mail_thread_queued, (EMsg *)m);
	return id;
}

/* ** FolderETree Extras *************************************************** */

static void
fete_destroy (GtkObject *object)
{
	FolderETreeExtras *extras = (FolderETreeExtras *) object;

	gdk_pixbuf_unref (extras->toggles[0]);
	gdk_pixbuf_unref (extras->toggles[1]);

	ftree_extras_parent_class->destroy (object);
}

static void
fete_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = fete_destroy;

	ftree_extras_parent_class = gtk_type_class (E_TABLE_EXTRAS_TYPE);
}

static void
fete_init (GtkObject *object)
{
	FolderETreeExtras *extras = (FolderETreeExtras *) object;
	ECell             *cell;
	ECell             *text_cell;

	/* text column */

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	text_cell = cell;
	gtk_object_set (GTK_OBJECT (cell),
			"bold_column", FOLDER_COL_SUBSCRIBED,
			NULL);
	e_table_extras_add_cell (E_TABLE_EXTRAS (extras), "cell_text", cell);

	/* toggle column */

	extras->toggles[0] = gdk_pixbuf_new_from_xpm_data ((const char **)empty_xpm);
	extras->toggles[1] = gdk_pixbuf_new_from_xpm_data ((const char **)mark_xpm);
	cell = e_cell_toggle_new (0, 2, extras->toggles);
	e_table_extras_add_cell (E_TABLE_EXTRAS (extras), "cell_toggle", cell);

	/* tree cell */

	cell = e_cell_tree_new (NULL, NULL, TRUE, text_cell);
	e_table_extras_add_cell (E_TABLE_EXTRAS (extras), "cell_tree", cell);

	/* misc */

	e_table_extras_add_pixbuf (E_TABLE_EXTRAS (extras), "subscribed-image", extras->toggles[1]);
}

/* naughty! */
static
E_MAKE_TYPE (fete, "FolderETreeExtras", FolderETreeExtras, fete_class_init, fete_init, E_TABLE_EXTRAS_TYPE);

/* ** Global Extras ******************************************************** */

static FolderETreeExtras *global_extras = NULL;

static void
global_extras_destroyed (GtkObject *obj, gpointer user_data)
{
	global_extras = NULL;
}

static ETableExtras *
subscribe_get_global_extras (void)
{
	if (global_extras == NULL) {
		global_extras = gtk_type_new (fete_get_type());
		gtk_object_ref (GTK_OBJECT (global_extras));
		gtk_object_sink (GTK_OBJECT (global_extras));
		gtk_signal_connect (GTK_OBJECT (global_extras), "destroy", 
				    global_extras_destroyed, NULL);
	}

	gtk_object_ref (GTK_OBJECT (global_extras));
	return E_TABLE_EXTRAS (global_extras);
}

/* ** Folder Tree Node ***************************************************** */

typedef struct _ftree_node ftree_node;

struct _ftree_node {
	guint8    flags;
	char *cache;
	int uri_offset;
	int full_name_offset;

	/* format: {name}{\0}{uri}{\0}{full_name}{\0}
	 * (No braces). */
	char data[1];
};

#define FTREE_NODE_GOT_CHILDREN (1 << 0)
#define FTREE_NODE_SUBSCRIBABLE (1 << 1)
#define FTREE_NODE_SUBSCRIBED   (1 << 2)
#define FTREE_NODE_ROOT         (1 << 3)

static ftree_node *
ftree_node_new_root (const char *prefix)
{
	ftree_node *node;
	size_t      size;

	if (prefix == NULL)
		prefix = "";

	size = sizeof (ftree_node) + strlen (prefix) + 1;

	node = g_malloc (size);
	node->flags = FTREE_NODE_ROOT;
	node->uri_offset = 0;
	node->full_name_offset = 1;
	node->data[0] = '\0';
	strcpy (node->data + 1, prefix);
	
	return node;
}

static ftree_node *
ftree_node_new (CamelStore *store, CamelFolderInfo *fi)
{
	ftree_node *node;
	int         uri_offset, full_name_offset;
	size_t      size;
	CamelURL   *url;

	uri_offset       = strlen (fi->name) + 1;
	full_name_offset = uri_offset + strlen (fi->url) + 1;
	size             = full_name_offset + strlen (fi->full_name);
  
	/* - 1 for sizeof(node.data) but +1 for terminating \0 */
	node = g_malloc (sizeof (*node) + size);

	node->cache = NULL;
	
	/* Noselect? */

	url = camel_url_new (fi->url, NULL);
	if (camel_url_get_param (url, "noselect"))
		node->flags = 0;
	else
		node->flags = FTREE_NODE_SUBSCRIBABLE;
	camel_url_free (url);

	/* subscribed? */

	if (camel_store_folder_subscribed (store, fi->full_name))
		node->flags |= FTREE_NODE_SUBSCRIBED;

	/* Copy strings */

	node->uri_offset       = uri_offset;
	node->full_name_offset = full_name_offset;

	strcpy (node->data, fi->name);
	strcpy (node->data + uri_offset, fi->url);
	strcpy (node->data + full_name_offset, fi->full_name);

	/* Done */

	return node;
}

#define ftree_node_subscribable(node)  ( ((ftree_node *) (node))->flags & FTREE_NODE_SUBSCRIBABLE )
#define ftree_node_subscribed(node)    ( ((ftree_node *) (node))->flags & FTREE_NODE_SUBSCRIBED )
#define ftree_node_get_name(node)      ( ((ftree_node *) (node))->data )
#define ftree_node_get_full_name(node) ( ((ftree_node *) (node))->data + ((ftree_node *) (node))->full_name_offset )
#define ftree_node_get_uri(node)       ( ((ftree_node *) (node))->data + ((ftree_node *) (node))->uri_offset )

/* ** Folder Tree Model **************************************************** */

/* A subscribe or scan operation */

typedef struct _ftree_op_data ftree_op_data;

struct _ftree_op_data {
	FolderETree *ftree;
	ETreePath path;
	ftree_node *data;
	int handle;
};


/* ETreeModel functions */

static int
fe_column_count (ETreeModel *etm)
{
	return FOLDER_COL_LAST;
}

static void *
fe_duplicate_value (ETreeModel *etm, int col, const void *val)
{
	return g_strdup (val);
}

static void
fe_free_value (ETreeModel *etm, int col, void *val)
{
	g_free (val);
}

static void*
fe_init_value (ETreeModel *etm, int col)
{
	return g_strdup ("");
}

static gboolean
fe_value_is_empty (ETreeModel *etm, int col, const void *val)
{
	return !(val && *(char *)val);
}

static char *
fe_value_to_string (ETreeModel *etm, int col, const void *val)
{
	return g_strdup (val);
}

static GdkPixbuf *
fe_icon_at (ETreeModel *etree, ETreePath path)
{
	return NULL; /* XXX no icons for now */
}

static gpointer
fe_root_value_at (FolderETree *ftree, int col)
{
	switch (col) {
	case FOLDER_COL_NAME:
		return ftree->service_name;
	case FOLDER_COL_SUBSCRIBED:
		return GINT_TO_POINTER (0);
	default:
		printf ("Oh no, unimplemented column %d in subscribe dialog\n", col);
	}

	return NULL;
}

static gpointer
fe_real_value_at (FolderETree *ftree, int col, gpointer data)
{
	switch (col) {
	case FOLDER_COL_NAME:
		return ftree_node_get_name (data);
	case FOLDER_COL_SUBSCRIBED:
		if (ftree_node_subscribed (data))
			return GINT_TO_POINTER (1);
		return GINT_TO_POINTER (0);
	default:
		printf ("Oh no, unimplemented column %d in subscribe dialog\n", col);
	}
	
	return NULL;
}

static void *
fe_value_at (ETreeModel *etree, ETreePath path, int col)
{
	FolderETree *ftree = (FolderETree *) etree;
	gpointer node_data;
	
	if (path == ftree->root)
		return fe_root_value_at (ftree, col);

	node_data = e_tree_memory_node_get_data (E_TREE_MEMORY (etree), path);
	return fe_real_value_at (ftree, col, node_data);
}

static void
fe_set_value_at (ETreeModel *etree, ETreePath path, int col, const void *val)
{
	/* nothing */
}

static gboolean
fe_return_false (void)
{
	return FALSE;
}

static gint
fe_sort_folder (ETreeMemory *etmm, ETreePath left, ETreePath right, gpointer user_data)
{
	ftree_node *n_left, *n_right;

	n_left = e_tree_memory_node_get_data (etmm, left);
	n_right = e_tree_memory_node_get_data (etmm, right);

	return g_strcasecmp (ftree_node_get_name (n_left), ftree_node_get_name (n_right));
}

/* scanning */

static void
fe_got_children (CamelStore *store, char *prefix, CamelFolderInfo *info, gpointer data)
{
	ftree_op_data *closure = (ftree_op_data *) data;

	if (!info) /* cancelled */
		return;

	if (!prefix)
		prefix = "";

	for ( ; info; info = info->sibling) {
		ETreePath   child_path;
		ftree_node *node;

		if (strcmp (info->full_name, prefix) == 0)
			continue;

		node = ftree_node_new (store, info);
		child_path = e_tree_memory_node_insert (E_TREE_MEMORY (closure->ftree),
							closure->path,
							0,
							node);
		e_tree_memory_sort_node (E_TREE_MEMORY (closure->ftree), 
					 closure->path,
					 fe_sort_folder,
					 NULL);
	}

	if (closure->data)
		closure->data->flags |= FTREE_NODE_GOT_CHILDREN;

	g_hash_table_remove (closure->ftree->scan_ops, closure->path);
	g_free (closure);
}

static void
fe_check_for_children (FolderETree *ftree, ETreePath path)
{
	ftree_op_data *closure;
	ftree_node *node;
	char *prefix;

	node = e_tree_memory_node_get_data (E_TREE_MEMORY (ftree), path);

	/* have we already gotten these children? */
	if (node->flags & FTREE_NODE_GOT_CHILDREN)
		return;

	/* or we're loading them right now? */
	if (g_hash_table_lookup (ftree->scan_ops, path))
		return;

	/* figure out our search prefix */
	if (path == ftree->root)
		prefix = ftree->search;
	else
		prefix = ftree_node_get_full_name (node);

	closure = g_new (ftree_op_data, 1);
	closure->ftree = ftree;
	closure->path = path;
	closure->data = node;
	closure->handle = -1;

	g_hash_table_insert (ftree->scan_ops, path, closure);

	/* FIXME. Tiny race possiblity I guess. */

	closure->handle = subscribe_get_short_folderinfo (ftree, prefix, fe_got_children, closure);
}

static void
fe_create_root_node (FolderETree *ftree)
{
	ftree_node *node;

	node = ftree_node_new_root (ftree->search);
	ftree->root = e_tree_memory_node_insert (E_TREE_MEMORY(ftree), NULL, 0, node);
	fe_check_for_children (ftree, ftree->root);
}

static ETreePath
fe_get_first_child (ETreeModel *model, ETreePath path)
{
	ETreePath child_path;

	child_path = E_TREE_MODEL_CLASS (folder_etree_parent_class)->get_first_child (model, path);
	if (child_path)
		fe_check_for_children ((FolderETree *) model, child_path);
	else
		fe_check_for_children ((FolderETree *) model, path);
	return child_path;
}

/* subscribing */

static char *
fe_node_to_shell_path (ftree_node *node)
{
	char *path = NULL;
	int name_len, full_name_len;

	name_len = strlen (ftree_node_get_name (node));
	full_name_len = strlen (ftree_node_get_full_name (node));
	
	if (name_len != full_name_len) {
		char *full_name;
		char *iter;
		char sep;
	
		/* so, we don't know the heirarchy separator. But
		 * full_name = blahXblahXname, where X = separator
		 * and name = .... name. So we can determine it.
		 * (imap_store->dir_sep isn't really private, I guess,
		 * so we could use that if we had the store. But also
		 * we don't "know" that it is an IMAP store anyway.)
		 */

		full_name = ftree_node_get_full_name (node);
		sep = full_name[full_name_len - (name_len + 1)];

		if (sep != '/') {
			path = g_malloc (full_name_len + 2);
			path[0] = '/';
			strcpy (path + 1, full_name);
			while ((iter = strchr (path, sep)) != NULL)
				*iter = '/';
		}
	}

	if (!path)
		path = g_strdup_printf ("/%s", ftree_node_get_full_name (node));

	return path;
}

static void
fe_done_subscribing (const char *full_name, const char *name, gboolean subscribe, gboolean success, gpointer user_data)
{
	ftree_op_data *closure = (ftree_op_data *) user_data;

	if (success && closure->handle != -1) {
		char *path;

		path = fe_node_to_shell_path (closure->data);

		if (subscribe) {
			closure->data->flags |= FTREE_NODE_SUBSCRIBED;
			recursive_add_folder (closure->ftree->e_storage,
					      path, name,
					      ftree_node_get_uri (closure->data));
		} else {
			closure->data->flags &= ~FTREE_NODE_SUBSCRIBED;

			/* FIXME: recursively remove folder as well? Possible? */
			evolution_storage_removed_folder (closure->ftree->e_storage, path);
		}

		g_free (path);
		e_tree_model_node_data_changed (E_TREE_MODEL (closure->ftree), closure->path);
	}

	if (closure->handle != -1)
		g_hash_table_remove (closure->ftree->subscribe_ops, closure->path);

	g_free (closure);
}

/* cleanup */

static gboolean
fe_cancel_op_foreach (gpointer key, gpointer value, gpointer user_data)
{
	/*FolderETree   *ftree = (FolderETree *) user_data;*/
	ftree_op_data *closure = (ftree_op_data *) value;

	if (closure->handle != -1)
		mail_msg_cancel (closure->handle);

	closure->handle = -1;

	return TRUE;
}

static void
fe_kill_current_tree (FolderETree *ftree)
{
	g_hash_table_foreach_remove (ftree->scan_ops, fe_cancel_op_foreach, ftree);
	g_assert (g_hash_table_size (ftree->scan_ops) == 0);
}

static void
fe_destroy (GtkObject *obj)
{
	FolderETree *ftree = (FolderETree *) (obj);

	fe_kill_current_tree (ftree);

	g_hash_table_foreach_remove (ftree->subscribe_ops, fe_cancel_op_foreach, ftree);
	
	g_hash_table_destroy (ftree->scan_ops);
	g_hash_table_destroy (ftree->subscribe_ops);

	camel_object_unref (CAMEL_OBJECT (ftree->store));
	bonobo_object_unref (BONOBO_OBJECT (ftree->e_storage));

	g_free (ftree->search);
	g_free (ftree->service_name);
}

typedef gboolean (*bool_func_1) (ETreeModel *, ETreePath, int);
typedef gboolean (*bool_func_2) (ETreeModel *);

static void
folder_etree_class_init (GtkObjectClass *klass)
{
	ETreeModelClass  *etree_model_class = E_TREE_MODEL_CLASS (klass);

	folder_etree_parent_class = gtk_type_class (E_TREE_MEMORY_TYPE);

	klass->destroy                          = fe_destroy;

	etree_model_class->value_at             = fe_value_at;
	etree_model_class->set_value_at         = fe_set_value_at;
	etree_model_class->column_count         = fe_column_count;
	etree_model_class->duplicate_value      = fe_duplicate_value;
	etree_model_class->free_value           = fe_free_value;
	etree_model_class->initialize_value     = fe_init_value;
	etree_model_class->value_is_empty       = fe_value_is_empty;
	etree_model_class->value_to_string      = fe_value_to_string;
	etree_model_class->icon_at              = fe_icon_at;
	etree_model_class->is_editable          = (bool_func_1) fe_return_false;
	etree_model_class->has_save_id          = (bool_func_2) fe_return_false;
	etree_model_class->has_get_node_by_id   = (bool_func_2) fe_return_false;
	etree_model_class->get_first_child      = fe_get_first_child;
}

static void
folder_etree_init (GtkObject *object)
{
	FolderETree *ftree = (FolderETree *) object;

	e_tree_memory_set_node_destroy_func (E_TREE_MEMORY (ftree), (GFunc) g_free, ftree);

	ftree->scan_ops = g_hash_table_new (g_direct_hash, g_direct_equal);
	ftree->subscribe_ops = g_hash_table_new (g_direct_hash, g_direct_equal);

	ftree->search = g_strdup ("");
}

static FolderETree *
folder_etree_construct (FolderETree *ftree,
			CamelStore  *store)
{
	e_tree_memory_construct (E_TREE_MEMORY (ftree));
	
	ftree->store = store;
	camel_object_ref (CAMEL_OBJECT (store));
	
	ftree->service_name = camel_service_get_name (CAMEL_SERVICE (store), FALSE);
	
	ftree->e_storage = mail_lookup_storage (store); /* this gives us a ref */

	fe_create_root_node (ftree);

	return ftree;
}

static
E_MAKE_TYPE (folder_etree, "FolderETree", FolderETree, folder_etree_class_init, folder_etree_init, E_TREE_MEMORY_TYPE);

/* public */

static FolderETree *
folder_etree_new (CamelStore *store)
{
	FolderETree *ftree;

	ftree = gtk_type_new (folder_etree_get_type());
	ftree = folder_etree_construct (ftree, store);
	return ftree;
}

static void
folder_etree_clear_tree (FolderETree *ftree)
{
	e_tree_memory_freeze (E_TREE_MEMORY (ftree));
	e_tree_memory_node_remove (E_TREE_MEMORY (ftree), ftree->root);
	fe_create_root_node (ftree);
	e_tree_memory_thaw (E_TREE_MEMORY (ftree));
}

static void
folder_etree_set_search (FolderETree *ftree, const char *search)
{
	if (!strcmp (search, ftree->search))
		return;

	g_free (ftree->search);
	ftree->search = g_strdup (search);
	
	folder_etree_clear_tree (ftree);
}


static int
folder_etree_path_set_subscription (FolderETree *ftree, ETreePath path, gboolean subscribe)
{
	ftree_op_data *closure;
	ftree_node    *node;

	/* already in progress? */

	if (g_hash_table_lookup (ftree->subscribe_ops, path))
		return 0;

	/* noselect? */

	node = e_tree_memory_node_get_data (E_TREE_MEMORY (ftree), path);

	if (!ftree_node_subscribable (node))
		return -1;

	/* noop? */

	/* uh, this should be a not XOR or something */
	if ((ftree_node_subscribed (node) && subscribe) ||
	    (!ftree_node_subscribed (node) && !subscribe))
		return 0;

	closure         = g_new (ftree_op_data, 1);
	closure->ftree  = ftree;
	closure->path   = path;
	closure->data   = node;
	closure->handle = -1;

	g_hash_table_insert (ftree->subscribe_ops, path, closure);

	closure->handle = subscribe_do_subscribe_folder (ftree->store,
							 ftree_node_get_full_name (node),
							 ftree_node_get_name (node),
							 subscribe,
							 fe_done_subscribing,
							 closure);
	return 0;
}

static int
folder_etree_path_toggle_subscription (FolderETree *ftree, ETreePath path)
{
	ftree_node *node = e_tree_memory_node_get_data (E_TREE_MEMORY (ftree), path);

	if (ftree_node_subscribed (node))
		return folder_etree_path_set_subscription (ftree, path, FALSE);
	else
		return folder_etree_path_set_subscription (ftree, path, TRUE);
}

/* ** StoreData ************************************************************ */

typedef struct _StoreData StoreData;

typedef void (*StoreDataStoreFunc) (StoreData *, CamelStore *, gpointer);

struct _StoreData {
	int refcount;
	char *uri;
	
	FolderETree *ftree;
	CamelStore *store;
	
	int request_id;
	
	GtkWidget *widget;
	StoreDataStoreFunc store_func;
	gpointer store_data;
};

static StoreData *
store_data_new (const char *uri)
{
	StoreData *sd;
	
	sd = g_new0 (StoreData, 1);
	sd->refcount = 1;
	sd->uri = g_strdup (uri);
	
	return sd;
}

static void
store_data_free (StoreData *sd)
{
	if (sd->request_id)
		mail_msg_cancel (sd->request_id);
	
	if (sd->widget)
		gtk_object_unref (GTK_OBJECT (sd->widget));
	
	if (sd->ftree)
		gtk_object_unref (GTK_OBJECT (sd->ftree));
	
	if (sd->store)
		camel_object_unref (CAMEL_OBJECT (sd->store));
	
	g_free (sd->uri);
	g_free (sd);
}

static void
store_data_ref (StoreData *sd)
{
	sd->refcount++;
}

static void
store_data_unref (StoreData *sd)
{
	if (sd->refcount <= 1) {
		store_data_free (sd);
	} else {
		sd->refcount--;
	}
}

static void
sd_got_store (char *uri, CamelStore *store, gpointer user_data)
{
	StoreData *sd = (StoreData *) user_data;
	
	sd->store = store;
	
	if (store) /* we can have exceptions getting the store... server is down, eg */
		camel_object_ref (CAMEL_OBJECT (sd->store));
	
	/* uh, so we might have a problem if this operation is cancelled. Unsure. */
	sd->request_id = 0;
	
	if (sd->store_func)
		(sd->store_func) (sd, sd->store, sd->store_data);
	
	store_data_unref (sd);
}

static void
store_data_async_get_store (StoreData *sd, StoreDataStoreFunc func, gpointer user_data)
{
	if (sd->request_id) {
		d(printf ("Already loading store, nooping\n"));
		return;
	}
	
	if (sd->store) {
		/* um, is this the best behavior? */
		func (sd, sd->store, user_data);
		return;
	}
	
	sd->store_func = func;
	sd->store_data = user_data;
	store_data_ref (sd);
	sd->request_id = mail_get_store (sd->uri, sd_got_store, sd);
}

static void
store_data_cancel_get_store (StoreData *sd)
{
	g_return_if_fail (sd->request_id);

	mail_msg_cancel (sd->request_id);
	sd->request_id = 0;
}

static void
sd_toggle_cb (ETree *tree, int row, ETreePath path, int col, GdkEvent *event, gpointer user_data)
{
	StoreData *sd = (StoreData *) user_data;

	folder_etree_path_toggle_subscription (sd->ftree, path);
}

static GtkWidget *
store_data_get_widget (StoreData *sd)
{
	GtkWidget *tree;

	if (!sd->store) {
		d(printf ("store data can't get widget before getting store.\n"));
		return NULL;
	}

	if (sd->widget)
		return sd->widget;

	sd->ftree = folder_etree_new (sd->store);

	/* You annoy me, etree! */
	tree = gtk_widget_new (E_TREE_SCROLLED_TYPE,
			       "hadjustment", NULL,
			       "vadjustment", NULL,
			       NULL);

	tree = (GtkWidget *) e_tree_scrolled_construct_from_spec_file (E_TREE_SCROLLED (tree),
								       E_TREE_MODEL (sd->ftree),
								       subscribe_get_global_extras (),
								       EVOLUTION_ETSPECDIR "/subscribe-dialog.etspec",
								       NULL);
	e_tree_root_node_set_visible (e_tree_scrolled_get_tree(E_TREE_SCROLLED(tree)), TRUE);
	gtk_signal_connect (GTK_OBJECT (e_tree_scrolled_get_tree(E_TREE_SCROLLED (tree))),
			    "double_click", GTK_SIGNAL_FUNC (sd_toggle_cb), sd);

	gtk_object_unref (GTK_OBJECT (global_extras));

	sd->widget = tree;
	gtk_object_ref (GTK_OBJECT (sd->widget));

	return sd->widget;
}

typedef struct _selection_closure {
	StoreData *sd;
	enum { SET, CLEAR, TOGGLE } mode;
} selection_closure;

static void
sd_subscribe_folder_foreach (int model_row, gpointer closure)
{
	selection_closure *sc   = (selection_closure *) closure;
	StoreData         *sd   = sc->sd;
	ETree             *tree = e_tree_scrolled_get_tree(E_TREE_SCROLLED(sd->widget));
	ETreePath          path = e_tree_node_at_row (tree, model_row);

	/* ignore results */
	switch (sc->mode) {
	case SET:
		folder_etree_path_set_subscription (sd->ftree, path, TRUE);
		break;
	case CLEAR:
		folder_etree_path_set_subscription (sd->ftree, path, FALSE);
		break;
	case TOGGLE:
		folder_etree_path_toggle_subscription (sd->ftree, path);
		break;
	}
}

static void
store_data_selection_set_subscription (StoreData *sd, gboolean subscribe)
{
	selection_closure sc;
	ETree *tree;
	
	sc.sd = sd;
	if (subscribe)
		sc.mode = SET;
	else
		sc.mode = CLEAR;

	tree = e_tree_scrolled_get_tree (E_TREE_SCROLLED (sd->widget));
	e_tree_selected_row_foreach (tree, sd_subscribe_folder_foreach, &sc);
}

#ifdef NEED_TOGGLE_SELECTION
static void
store_data_selection_toggle_subscription (StoreData *sd)
{
	selection_closure  sc;
	ETree             *tree;
	
	sc.sd   = sd;
	sc.mode = TOGGLE;

	tree = e_tree_scrolled_get_tree (E_TREE_SCROLLED (sd->widget));
	e_tree_selected_row_foreach (tree, sd_subscribe_folder_foreach, &sc);
}
#endif

static gboolean
store_data_mid_request (StoreData *sd)
{
	return (gboolean) sd->request_id;
}

/* ** yaay, SubscribeDialog ******************************************************* */

#define PARENT_TYPE (gtk_object_get_type ())

#ifdef JUST_FOR_TRANSLATORS
static char *str = N_("Folder");
#endif

#define STORE_DATA_KEY   "store-data"

struct _SubscribeDialogPrivate {
	GladeXML  *xml;
	GList     *store_list;

	StoreData *current_store;
	GtkWidget *current_widget;

	GtkWidget *default_widget;
	GtkWidget *none_item;
	GtkWidget *search_entry;
	GtkWidget *hbox;
	GtkWidget *filter_radio, *all_radio;
	GtkWidget *sub_button, *unsub_button, *refresh_button;
};

static GtkObjectClass *subscribe_dialog_parent_class;

static void
sc_refresh_pressed (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	if (sc->priv->current_store)
		folder_etree_clear_tree (sc->priv->current_store->ftree);
}

static void
sc_search_activated (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData       *store = sc->priv->current_store;
	char            *search;

	if (!store)
		return;

	search = e_utf8_gtk_entry_get_text (GTK_ENTRY (widget));
	folder_etree_set_search (store->ftree, search);
}

static void
sc_subscribe_pressed (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData       *store = sc->priv->current_store;

	if (!store)
		return;

	store_data_selection_set_subscription (store, TRUE);
}

static void
sc_unsubscribe_pressed (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData *store = sc->priv->current_store;

	if (!store)
		return;

	store_data_selection_set_subscription (store, FALSE);
}

static void
sc_all_toggled (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData       *store = sc->priv->current_store;

	if (!store)
		return;

	if (GTK_TOGGLE_BUTTON (widget)->active) {
		gtk_widget_set_sensitive (sc->priv->search_entry, FALSE);
		folder_etree_set_search (store->ftree, "");
	}
}

static void
sc_filter_toggled (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData       *store = sc->priv->current_store;

	if (!store)
		return;

	if (GTK_TOGGLE_BUTTON (widget)->active) {
		gtk_widget_set_sensitive (sc->priv->search_entry, TRUE);
		sc_search_activated (sc->priv->search_entry, sc);
	}
}

static void
populate_store_foreach (MailConfigService *service, SubscribeDialog *sc)
{
	StoreData            *sd;

	if (service->url == NULL || service->enabled == FALSE)
		return;

	sd = store_data_new (service->url);
	sc->priv->store_list = g_list_prepend (sc->priv->store_list, sd);
}

static void
kill_default_view (SubscribeDialog *sc)
{
	gtk_widget_hide (sc->priv->none_item);

	/* the entry will be set sensitive when one of the
	 * radio buttons is activated, if necessary. */

	gtk_widget_set_sensitive (sc->priv->all_radio, TRUE);
	gtk_widget_set_sensitive (sc->priv->filter_radio, TRUE);
	gtk_widget_set_sensitive (sc->priv->sub_button, TRUE);
	gtk_widget_set_sensitive (sc->priv->unsub_button, TRUE);
	gtk_widget_set_sensitive (sc->priv->refresh_button, TRUE);
}

static void
sc_selection_changed (GtkObject *obj, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	gboolean sensitive;

	if (e_selection_model_selected_count (E_SELECTION_MODEL (obj)))
		sensitive = TRUE;
	else
		sensitive = FALSE;

	gtk_widget_set_sensitive (sc->priv->sub_button, sensitive);
	gtk_widget_set_sensitive (sc->priv->unsub_button, sensitive);
}

static void
menu_item_selected (GtkMenuItem *item, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData       *sd = gtk_object_get_data (GTK_OBJECT (item), STORE_DATA_KEY);

	g_return_if_fail (sd);

	if (sd->widget == NULL) {
		GtkWidget *widget;
		ESelectionModel *esm;
		ETree *tree;

		widget = store_data_get_widget (sd);
		gtk_box_pack_start (GTK_BOX (sc->priv->hbox), widget, TRUE, TRUE, 0);

		tree = e_tree_scrolled_get_tree (E_TREE_SCROLLED (widget));
		esm = e_tree_get_selection_model (tree);
		gtk_signal_connect (GTK_OBJECT (esm), "selection_changed", sc_selection_changed, sc);
		sc_selection_changed ((GtkObject *)esm, sc);
	}

	if (sc->priv->current_widget == sc->priv->default_widget)
		kill_default_view (sc);

	gtk_widget_hide (sc->priv->current_widget);
	gtk_widget_show (sd->widget);
	sc->priv->current_widget = sd->widget;
	sc->priv->current_store  = sd;

	if (*sd->ftree->search) {
		e_utf8_gtk_entry_set_text (GTK_ENTRY (sc->priv->search_entry), sd->ftree->search);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sc->priv->filter_radio), TRUE);
	} else {
		e_utf8_gtk_entry_set_text (GTK_ENTRY (sc->priv->search_entry), "");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sc->priv->all_radio), TRUE);
	}
}

static void
dummy_item_selected (GtkMenuItem *item, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	gtk_widget_hide (sc->priv->current_widget);
	gtk_widget_show (sc->priv->default_widget);
	sc->priv->current_widget = sc->priv->default_widget;
	sc->priv->current_store  = NULL;

	e_utf8_gtk_entry_set_text (GTK_ENTRY (sc->priv->search_entry), "");
}

/* wonderful */

static void
got_sd_store (StoreData *sd, CamelStore *store, gpointer data)
{
	if (store && camel_store_supports_subscriptions (store))
		gtk_widget_show (GTK_WIDGET (data));
}

/* FIXME: if there aren't any stores that are subscribable, the option
 * menu will only have the "No server selected" item and the user will
 * be confused. */

static void
populate_store_list (SubscribeDialog *sc)
{
	const GSList *news;
	GSList       *sources;
	GList        *iter;
	GtkWidget    *menu;
	GtkWidget    *omenu;

	sources = mail_config_get_sources ();
	g_slist_foreach (sources, (GFunc) populate_store_foreach, sc);
	g_slist_free (sources);
	
	news = mail_config_get_news ();
	g_slist_foreach ((GSList *) news, (GFunc) populate_store_foreach, sc);

	menu = gtk_menu_new ();

	for (iter = sc->priv->store_list; iter; iter = iter->next) {
		GtkWidget *item;
		CamelURL *url;
		char *string;

		url = camel_url_new (((StoreData *) iter->data)->uri, NULL);
		string = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		item = gtk_menu_item_new_with_label (string);
		store_data_async_get_store (iter->data, got_sd_store, item);
		gtk_object_set_data (GTK_OBJECT (item), STORE_DATA_KEY, iter->data);
		gtk_signal_connect (GTK_OBJECT (item), "activate", menu_item_selected, sc);
		g_free (string);

		gtk_menu_prepend (GTK_MENU (menu), item);
	}

	sc->priv->none_item = gtk_menu_item_new_with_label (_("No server has been selected"));
	gtk_signal_connect (GTK_OBJECT (sc->priv->none_item), "activate", dummy_item_selected, sc);
	gtk_widget_show (sc->priv->none_item);
	gtk_menu_prepend (GTK_MENU (menu), sc->priv->none_item);

	gtk_widget_show (menu);

	omenu = glade_xml_get_widget (sc->priv->xml, "store_menu");
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
}

static void
subscribe_dialog_destroy (GtkObject *object)
{
	SubscribeDialog *sc;
	GList *iter;
	
	sc = SUBSCRIBE_DIALOG (object);
	
	for (iter = sc->priv->store_list; iter; iter = iter->next) {
		StoreData *data = iter->data;
		
		if (store_data_mid_request (data))
			store_data_cancel_get_store (data);
		
		data->store_func = NULL;
		
		store_data_unref (data);
	}
	
	g_list_free (sc->priv->store_list);
	
	gtk_object_unref (GTK_OBJECT (sc->priv->xml));

	g_free (sc->priv);

	subscribe_dialog_parent_class->destroy (object);
}

static void
subscribe_dialog_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = subscribe_dialog_destroy;

	subscribe_dialog_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
subscribe_dialog_init (GtkObject *object)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (object);

	sc->priv = g_new0 (SubscribeDialogPrivate, 1);
}

static GtkWidget *
sc_create_default_widget (void)
{
	GtkWidget *label;
	GtkWidget *viewport;

	label = gtk_label_new (_("Please select a server."));
	gtk_widget_show (label);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (viewport), label);

	return viewport;
}

static void
subscribe_dialog_construct (GtkObject *object)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (object);
	
	/* Load the XML */
	sc->priv->xml            = glade_xml_new (EVOLUTION_GLADEDIR "/subscribe-dialog.glade", NULL);
	
	sc->app                  = glade_xml_get_widget (sc->priv->xml, "Manage Subscriptions");
	sc->priv->hbox           = glade_xml_get_widget (sc->priv->xml, "tree_box");
	sc->priv->search_entry   = glade_xml_get_widget (sc->priv->xml, "search_entry");
	sc->priv->filter_radio   = glade_xml_get_widget (sc->priv->xml, "filter_radio");
	sc->priv->all_radio      = glade_xml_get_widget (sc->priv->xml, "all_radio");
	sc->priv->sub_button     = glade_xml_get_widget (sc->priv->xml, "subscribe_button");
	sc->priv->unsub_button   = glade_xml_get_widget (sc->priv->xml, "unsubscribe_button");
	sc->priv->refresh_button = glade_xml_get_widget (sc->priv->xml, "refresh_button");
	
	/* create default view */
	sc->priv->default_widget = sc_create_default_widget();
	sc->priv->current_widget = sc->priv->default_widget;
	gtk_box_pack_start (GTK_BOX (sc->priv->hbox), sc->priv->default_widget, TRUE, TRUE, 0);
	gtk_widget_show (sc->priv->default_widget);
	
	gtk_widget_set_sensitive (sc->priv->all_radio, FALSE);
	gtk_widget_set_sensitive (sc->priv->filter_radio, FALSE);
	gtk_widget_set_sensitive (sc->priv->search_entry, FALSE);
	gtk_widget_set_sensitive (sc->priv->sub_button, FALSE);
	gtk_widget_set_sensitive (sc->priv->unsub_button, FALSE);
	gtk_widget_set_sensitive (sc->priv->refresh_button, FALSE);
	
	/* hook up some signals */
	gtk_signal_connect (GTK_OBJECT (sc->priv->search_entry), "activate", sc_search_activated, sc);
	gtk_signal_connect (GTK_OBJECT (sc->priv->sub_button), "clicked", sc_subscribe_pressed, sc);
	gtk_signal_connect (GTK_OBJECT (sc->priv->unsub_button), "clicked", sc_unsubscribe_pressed, sc);
	gtk_signal_connect (GTK_OBJECT (sc->priv->refresh_button), "clicked", sc_refresh_pressed, sc);
	gtk_signal_connect (GTK_OBJECT (sc->priv->all_radio), "toggled", sc_all_toggled, sc);
	gtk_signal_connect (GTK_OBJECT (sc->priv->filter_radio), "toggled", sc_filter_toggled, sc);
	
	/* Get the list of stores */
	populate_store_list (sc);
}

GtkObject *
subscribe_dialog_new (void)
{
	SubscribeDialog *subscribe_dialog;
	
	subscribe_dialog = gtk_type_new (SUBSCRIBE_DIALOG_TYPE);
	subscribe_dialog_construct (GTK_OBJECT (subscribe_dialog));
	
	return GTK_OBJECT (subscribe_dialog);
}

E_MAKE_TYPE (subscribe_dialog, "SubscribeDialog", SubscribeDialog, subscribe_dialog_class_init, subscribe_dialog_init, PARENT_TYPE);
