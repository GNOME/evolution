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

/* This doens't do what it's supposed to do ...
   I think etree changed so it just fills out the whole tree always anyway */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gal/util/e-util.h>

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
 * appropriate when only a single folder is selected. We need a
 * mechanism to learn when the selected folder's subscription status
 * changes, so when the user double-clicks it (eg) the buttons can
 * (de)sensitize appropriately. See Ximian bug #7673.
 */

/*#define NEED_TOGGLE_SELECTION*/

typedef struct _FolderETree              FolderETree;
typedef struct _FolderETreeClass         FolderETreeClass;

typedef void (*FolderETreeActivityCallback) (int level, gpointer user_data);

struct _FolderETree {
	ETreeMemory parent;
	ETreePath root;

	GHashTable *scan_ops;
	GHashTable *subscribe_ops;

	GHashTable *node_full_name;

	CamelStore *store;
	EvolutionStorage *e_storage;
	char *service_name;
	
	FolderETreeActivityCallback activity_cb;
	gpointer activity_data;
	int activity_level;
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

	evolution_storage_new_folder (storage, path, name, "mail", url, name, NULL, FALSE, TRUE, 0);
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

	d(printf("%d: getted folderinfo '%s'\n", mm->seq, m->prefix));
}

static void
get_short_folderinfo_got (struct _mail_msg *mm)
{
	struct _get_short_folderinfo_msg *m = (struct _get_short_folderinfo_msg *) mm;

	d(printf("%d: got folderinfo '%s'\n", mm->seq, m->prefix));

	if (camel_exception_is_set (&mm->ex) && camel_exception_get_id(&mm->ex) != CAMEL_EXCEPTION_USER_CANCEL) {
		g_warning ("Error getting folder info from store at %s: %s",
			   camel_service_get_url (CAMEL_SERVICE (m->ftree->store)),
			   camel_exception_get_description (&mm->ex));
	}

	if (m->func)
		m->func (m->ftree->store, m->prefix, m->info, m->user_data);
}

static void
get_short_folderinfo_free (struct _mail_msg *mm)
{
	struct _get_short_folderinfo_msg *m = (struct _get_short_folderinfo_msg *) mm;

	camel_store_free_folder_info (m->ftree->store, m->info);
	g_object_unref((m->ftree));

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
	g_object_ref((ftree));
	m->prefix = g_strdup (prefix);
	m->func = func;
	m->user_data = user_data;

	d(printf("%d: get folderinfo '%s'\n", m->msg.seq, m->prefix));

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
	
	camel_object_unref (m->store);
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

	camel_object_ref(store);

	id = m->msg.seq;
	e_thread_put (mail_thread_queued, (EMsg *)m);
	return id;
}

/* ** FolderETree Extras *************************************************** */

static void
fete_finalise (GObject *object)
{
	FolderETreeExtras *extras = (FolderETreeExtras *) object;

	g_object_unref (extras->toggles[0]);
	g_object_unref (extras->toggles[1]);

	((GObjectClass *)ftree_extras_parent_class)->finalize (object);
}

static void
fete_class_init (GObjectClass *object_class)
{
	object_class->finalize = fete_finalise;

	ftree_extras_parent_class = g_type_class_ref (E_TABLE_EXTRAS_TYPE);
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
	g_object_set (G_OBJECT (cell),
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
global_extras_destroyed (void *user_data, GObject *obj)
{
	global_extras = NULL;
}

static ETableExtras *
subscribe_get_global_extras (void)
{
	if (global_extras == NULL) {
		global_extras = g_object_new (fete_get_type(), NULL);
		/*g_object_ref(global_extras);
		  gtk_object_sink((GtkObject *)global_extras);*/
		g_object_weak_ref(G_OBJECT(global_extras), global_extras_destroyed, NULL);
	} else {
		g_object_ref(global_extras);
	}

	return E_TABLE_EXTRAS (global_extras);
}

/* ** Folder Tree Node ***************************************************** */

typedef struct _ftree_node ftree_node;

struct _ftree_node {
	guint8 flags;
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
ftree_node_new_root (void)
{
	ftree_node *node;
	
	node = g_malloc (sizeof (ftree_node));
	node->flags = FTREE_NODE_ROOT;
	node->uri_offset = 0;
	node->full_name_offset = 0;
	node->data[0] = '\0';
	
	return node;
}

static ftree_node *
ftree_node_new (CamelStore *store, CamelFolderInfo *fi)
{
	ftree_node *node;
	int         uri_offset, full_name_offset;
	size_t      size;
	
	uri_offset       = strlen (fi->name) + 1;
	full_name_offset = uri_offset + strlen (fi->url) + 1;
	size             = full_name_offset + strlen (fi->full_name);
  
	/* - 1 for sizeof(node.data) but +1 for terminating \0 */
	node = g_malloc (sizeof (*node) + size);

	node->cache = NULL;
	
	node->flags = FTREE_NODE_SUBSCRIBABLE;
	
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

	/* if in utf8 locale ? */
	return strcasecmp (ftree_node_get_name (n_left), ftree_node_get_name (n_right));
}


static void fe_check_for_children (FolderETree *ftree, ETreePath path);

/* scanning */
static void
fe_got_children (CamelStore *store, char *prefix, CamelFolderInfo *info, gpointer data)
{
	ftree_op_data *closure = (ftree_op_data *) data;

	if (!info) /* cancelled */
		goto done;

	/* also cancelled, but camel returned data, might leak */
	if (closure->handle == -1)
		goto done;

	if (!prefix)
		prefix = "";

	for ( ; info; info = info->sibling) {
		ETreePath child_path;
		ftree_node *node;
		
		if (g_hash_table_lookup(closure->ftree->node_full_name, info->full_name))
			continue;
		
		node = ftree_node_new (store, info);
		child_path = e_tree_memory_node_insert (E_TREE_MEMORY (closure->ftree),
							closure->path,
							0,
							node);
		g_hash_table_insert(closure->ftree->node_full_name, ftree_node_get_full_name(node), child_path);
		
		if (!(info->flags & CAMEL_FOLDER_NOINFERIORS))
			fe_check_for_children (closure->ftree, child_path);
	}

	/* FIXME: this needs to be added back to sort the tree */
	e_tree_memory_sort_node (E_TREE_MEMORY (closure->ftree), 
				 closure->path,
				 fe_sort_folder,
				 NULL);

	if (closure->data)
		closure->data->flags |= FTREE_NODE_GOT_CHILDREN;

	g_hash_table_remove (closure->ftree->scan_ops, closure->path);

done:
	/* finish off the activity of this task */
	/* hack, we know activity_data is an object */
	closure->ftree->activity_level--;
	(closure->ftree->activity_cb) (closure->ftree->activity_level, closure->ftree->activity_data);
	g_object_unref(closure->ftree->activity_data);

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
		prefix = "";
	else
		prefix = ftree_node_get_full_name (node);

	closure = g_new (ftree_op_data, 1);
	closure->ftree = ftree;
	closure->path = path;
	closure->data = node;
	closure->handle = -1;

	g_hash_table_insert (ftree->scan_ops, path, closure);

	/* hack, we know this is an object ... infact the subscribe dialog */
	g_object_ref(ftree->activity_data);
	ftree->activity_level++;
	(ftree->activity_cb) (ftree->activity_level, ftree->activity_data);

	/* FIXME. Tiny race possiblity I guess. */
	closure->handle = subscribe_get_short_folderinfo (ftree, prefix, fe_got_children, closure);
}

static void
fe_create_root_node (FolderETree *ftree)
{
	ftree_node *node;

	node = ftree_node_new_root ();
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
static void
fe_done_subscribing (const char *full_name, const char *name, gboolean subscribe, gboolean success, gpointer user_data)
{
	ftree_op_data *closure = (ftree_op_data *) user_data;

	if (success && closure->handle != -1) {
		char *path;
		
		path = g_strdup_printf ("/%s", full_name);
		
		if (subscribe) {
			closure->data->flags |= FTREE_NODE_SUBSCRIBED;
			recursive_add_folder (closure->ftree->e_storage,
					      path, name,
					      ftree_node_get_uri (closure->data));
		} else {
			closure->data->flags &= ~FTREE_NODE_SUBSCRIBED;

			/* FIXME: recursively remove folder as well? Possible? */
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

	if (closure->handle != -1) {
		d(printf("%d: cancel get messageinfo\n", closure->handle));
		mail_msg_cancel (closure->handle);
	}

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
fe_finalise (GObject *obj)
{
	FolderETree *ftree = (FolderETree *) (obj);

	d(printf("fe finalise!?\n"));

	fe_kill_current_tree (ftree);

	g_hash_table_foreach_remove (ftree->subscribe_ops, fe_cancel_op_foreach, ftree);
	
	g_hash_table_destroy (ftree->scan_ops);
	g_hash_table_destroy (ftree->subscribe_ops);
	g_hash_table_destroy(ftree->node_full_name);

	camel_object_unref (ftree->store);
	bonobo_object_unref (BONOBO_OBJECT (ftree->e_storage));
	
	g_free (ftree->service_name);

	((GObjectClass *)folder_etree_parent_class)->finalize(obj);
}

typedef gboolean (*bool_func_1) (ETreeModel *, ETreePath, int);
typedef gboolean (*bool_func_2) (ETreeModel *);

static void
folder_etree_class_init (GObjectClass *klass)
{
	ETreeModelClass  *etree_model_class = E_TREE_MODEL_CLASS (klass);

	folder_etree_parent_class = g_type_class_ref (E_TREE_MEMORY_TYPE);

	klass->finalize                         = fe_finalise;

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
	
	ftree->activity_level = 0;
	ftree->node_full_name = g_hash_table_new(g_str_hash, g_str_equal);
}

static FolderETree *
folder_etree_construct (FolderETree *ftree,
			CamelStore  *store,
			FolderETreeActivityCallback activity_cb,
			gpointer                    activity_data)
{
	e_tree_memory_construct (E_TREE_MEMORY (ftree));
	
	ftree->store = store;
	camel_object_ref (store);
	
	ftree->service_name = camel_service_get_name (CAMEL_SERVICE (store), FALSE);
	
	ftree->e_storage = mail_lookup_storage (store); /* this gives us a ref */

	ftree->activity_cb = activity_cb;
	ftree->activity_data = activity_data;

	fe_create_root_node (ftree);

	return ftree;
}

static
E_MAKE_TYPE (folder_etree, "FolderETree", FolderETree, folder_etree_class_init, folder_etree_init, E_TREE_MEMORY_TYPE);

/* public */

static FolderETree *
folder_etree_new (CamelStore *store,
		  FolderETreeActivityCallback activity_cb,
		  gpointer                    activity_data)
{
	FolderETree *ftree;

	ftree = g_object_new (folder_etree_get_type(), NULL);
	ftree = folder_etree_construct (ftree, store, activity_cb, activity_data);
	return ftree;
}

static void
folder_etree_clear_tree (FolderETree *ftree)
{
	e_tree_memory_freeze (E_TREE_MEMORY (ftree));
	e_tree_memory_node_remove (E_TREE_MEMORY (ftree), ftree->root);
	fe_create_root_node (ftree);
	g_hash_table_destroy(ftree->node_full_name);
	ftree->node_full_name = g_hash_table_new(g_str_hash, g_str_equal);
	e_tree_memory_thaw (E_TREE_MEMORY (ftree));
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

static void
folder_etree_cancel_all(FolderETree *ftree)
{
	g_hash_table_foreach_remove (ftree->scan_ops, fe_cancel_op_foreach, ftree);
	g_hash_table_foreach_remove (ftree->subscribe_ops, fe_cancel_op_foreach, ftree);
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
	d(printf("store data free?\n"));

	if (sd->request_id)
		mail_msg_cancel (sd->request_id);

	if (sd->ftree) {
		folder_etree_cancel_all(sd->ftree);
		g_object_unref(sd->ftree);
	}
	
	if (sd->store)
		camel_object_unref (sd->store);
	
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
		camel_object_ref (sd->store);
	
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
	sd->request_id = mail_get_store (sd->uri, NULL, sd_got_store, sd);
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
store_data_get_widget (StoreData *sd, 
		       FolderETreeActivityCallback activity_cb,
		       gpointer                    activity_data)
{
	GtkWidget *tree;

	if (!sd->store) {
		d(printf ("store data can't get widget before getting store.\n"));
		return NULL;
	}

	if (sd->widget)
		return sd->widget;

	sd->ftree = folder_etree_new (sd->store, activity_cb, activity_data);

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
	g_signal_connect(e_tree_scrolled_get_tree(E_TREE_SCROLLED (tree)),
			 "double_click", G_CALLBACK (sd_toggle_cb), sd);

	g_object_unref(global_extras);

	sd->widget = tree;

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
	GtkWidget *sub_button, *unsub_button, *refresh_button, *close_button;
	GtkWidget *progress;

	int cancel;		/* have we been cancelled? */
	guint activity_timeout_id;
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
sc_close_pressed (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	/* order important here */
	gtk_object_destroy (GTK_OBJECT (sc));
	gtk_widget_destroy (GTK_WIDGET (sc->app));
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
kill_default_view (SubscribeDialog *sc)
{
	gtk_widget_hide (sc->priv->none_item);
	
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

static gboolean
sc_activity_timeout (SubscribeDialog *sc)
{
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(sc->priv->progress));

	return TRUE;
}

static void
sc_activity_cb (int level, SubscribeDialog *sc)
{
	g_assert (pthread_self() == mail_gui_thread);

	if (sc->priv->cancel)
		return;

	if (level) {
		if (sc->priv->activity_timeout_id)
			return;

		sc->priv->activity_timeout_id = g_timeout_add(50, (GSourceFunc)sc_activity_timeout, sc);
		gtk_widget_show(sc->priv->progress);
	} else {
		if (sc->priv->activity_timeout_id) {
			g_source_remove (sc->priv->activity_timeout_id);
			sc->priv->activity_timeout_id = 0;
		}

		gtk_widget_hide(sc->priv->progress);
	}
}

static void
menu_item_selected (GtkMenuItem *item, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	StoreData       *sd = g_object_get_data (G_OBJECT (item), STORE_DATA_KEY);

	g_return_if_fail (sd);

	if (sd->widget == NULL) {
		GtkWidget *widget;
		ESelectionModel *esm;
		ETree *tree;

		widget = store_data_get_widget (sd, (FolderETreeActivityCallback) sc_activity_cb, sc);
		gtk_box_pack_start (GTK_BOX (sc->priv->hbox), widget, TRUE, TRUE, 0);

		tree = e_tree_scrolled_get_tree (E_TREE_SCROLLED (widget));
		esm = e_tree_get_selection_model (tree);
		g_signal_connect(esm, "selection_changed", G_CALLBACK(sc_selection_changed), sc);
		sc_selection_changed ((GtkObject *)esm, sc);
	}

	if (sc->priv->current_widget == sc->priv->default_widget)
		kill_default_view (sc);

	gtk_widget_hide (sc->priv->current_widget);
	gtk_widget_show (sd->widget);
	sc->priv->current_widget = sd->widget;
	sc->priv->current_store  = sd;
}

static void
dummy_item_selected (GtkMenuItem *item, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	gtk_widget_hide (sc->priv->current_widget);
	gtk_widget_show (sc->priv->default_widget);
	sc->priv->current_widget = sc->priv->default_widget;
	sc->priv->current_store  = NULL;

	gtk_entry_set_text (GTK_ENTRY (sc->priv->search_entry), "");
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
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	GtkWidget *menu;
	GtkWidget *omenu;
	GList *l;
	
	accounts = mail_config_get_accounts ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		StoreData *sd;
		
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->enabled && account->source->url) {
			sd = store_data_new (account->source->url);
			sc->priv->store_list = g_list_prepend (sc->priv->store_list, sd);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	menu = gtk_menu_new ();
	
	for (l = sc->priv->store_list; l; l = l->next) {
		GtkWidget *item;
		CamelURL *url;
		char *string;
		
		url = camel_url_new (((StoreData *) l->data)->uri, NULL);
		string = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		item = gtk_menu_item_new_with_label (string);
		store_data_async_get_store (l->data, got_sd_store, item);
		g_object_set_data (G_OBJECT (item), STORE_DATA_KEY, l->data);
		g_signal_connect (item, "activate", G_CALLBACK (menu_item_selected), sc);
		g_free (string);
		
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	}
	
	sc->priv->none_item = gtk_menu_item_new_with_label (_("No server has been selected"));
	g_signal_connect (sc->priv->none_item, "activate", G_CALLBACK (dummy_item_selected), sc);
	gtk_widget_show (sc->priv->none_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), sc->priv->none_item);
	
	gtk_widget_show (menu);
	
	omenu = glade_xml_get_widget (sc->priv->xml, "store_menu");
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
}

static void
subscribe_dialog_finalise (GObject *object)
{
	SubscribeDialog *sc;
	GList *iter;

	sc = SUBSCRIBE_DIALOG (object);

	if (sc->priv->store_list) {
		for (iter = sc->priv->store_list; iter; iter = iter->next) {
			StoreData *data = iter->data;
			store_data_unref (data);
		}
		
		g_list_free (sc->priv->store_list);
		sc->priv->store_list = NULL;
	}

	g_free (sc->priv);
	sc->priv = NULL;

	((GObjectClass *)subscribe_dialog_parent_class)->finalize (object);
}

static void
subscribe_dialog_destroy (GtkObject *object)
{
	SubscribeDialog *sc;
	GList *iter;

	sc = SUBSCRIBE_DIALOG (object);

	d(printf("subscribe_dialog_destroy\n"));

	if (!sc->priv->cancel) {
		sc->priv->cancel = 1;

		if (sc->priv->activity_timeout_id) {
			g_source_remove (sc->priv->activity_timeout_id);
			sc->priv->activity_timeout_id = 0;
		}

		if (sc->priv->store_list) {
			for (iter = sc->priv->store_list; iter; iter = iter->next) {
				StoreData *data = iter->data;
		
				if (store_data_mid_request (data))
					store_data_cancel_get_store (data);

				if (data->ftree)
					folder_etree_cancel_all(data->ftree);
				
				data->store_func = NULL;
			}
		}

		if (sc->priv->xml) {
			g_object_unref(sc->priv->xml);
			sc->priv->xml = NULL;
		}
	}

	subscribe_dialog_parent_class->destroy (object);
}

static void
subscribe_dialog_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = subscribe_dialog_destroy;
	((GObjectClass *)object_class)->finalize = subscribe_dialog_finalise;

	subscribe_dialog_parent_class = g_type_class_ref (PARENT_TYPE);
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
	/* "app2" */
	sc->priv->xml            = glade_xml_new (EVOLUTION_GLADEDIR "/subscribe-dialog.glade", "subscribe_dialog", NULL);

	sc->app                  = glade_xml_get_widget (sc->priv->xml, "subscribe_dialog");
	sc->priv->hbox           = glade_xml_get_widget (sc->priv->xml, "tree_box");
	sc->priv->close_button   = glade_xml_get_widget (sc->priv->xml, "close_button");
	sc->priv->sub_button     = glade_xml_get_widget (sc->priv->xml, "subscribe_button");
	sc->priv->unsub_button   = glade_xml_get_widget (sc->priv->xml, "unsubscribe_button");
	sc->priv->refresh_button = glade_xml_get_widget (sc->priv->xml, "refresh_button");
	sc->priv->progress       = glade_xml_get_widget(sc->priv->xml, "progress_bar");

	/* create default view */
	sc->priv->default_widget = sc_create_default_widget();
	sc->priv->current_widget = sc->priv->default_widget;
	gtk_box_pack_start (GTK_BOX (sc->priv->hbox), sc->priv->default_widget, TRUE, TRUE, 0);
	gtk_widget_show (sc->priv->default_widget);
	
	gtk_widget_set_sensitive (sc->priv->sub_button, FALSE);
	gtk_widget_set_sensitive (sc->priv->unsub_button, FALSE);
	gtk_widget_set_sensitive (sc->priv->refresh_button, FALSE);
	
	/* hook up some signals */
	g_signal_connect(sc->priv->close_button, "clicked", G_CALLBACK(sc_close_pressed), sc);
	g_signal_connect(sc->priv->sub_button, "clicked", G_CALLBACK(sc_subscribe_pressed), sc);
	g_signal_connect(sc->priv->unsub_button, "clicked", G_CALLBACK(sc_unsubscribe_pressed), sc);
	g_signal_connect(sc->priv->refresh_button, "clicked", G_CALLBACK(sc_refresh_pressed), sc);
	
	/* progress */
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(sc->priv->progress), 0.1);
	gtk_widget_hide(sc->priv->progress);

	/* reasonable starting point */
	gtk_window_set_default_size((GtkWindow *)sc->app, 350, 400);

	/* Get the list of stores */
	populate_store_list (sc);
}

GtkObject *
subscribe_dialog_new (void)
{
	SubscribeDialog *subscribe_dialog;
	
	subscribe_dialog = g_object_new (SUBSCRIBE_DIALOG_TYPE, NULL);
	subscribe_dialog_construct (GTK_OBJECT (subscribe_dialog));
	
	return GTK_OBJECT (subscribe_dialog);
}

E_MAKE_TYPE (subscribe_dialog, "SubscribeDialog", SubscribeDialog, subscribe_dialog_class_init, subscribe_dialog_init, PARENT_TYPE);
