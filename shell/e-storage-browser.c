/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-browser.c
 *
 * Copyright (C) 2003  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

/* TODO:

  - Currently it assumes that the starting path always exists, and you
    can't remove it.  It might be a limitation, but it makes the logic
    very simple and robust.

   - Doesn't save expansion state for nodes.

   - Context menu handling?

*/

#include <config.h>

#include "e-storage-browser.h"

#include "e-shell-marshal.h"
#include "e-storage-set-view.h"

#include <gal/util/e-util.h>

#include <gtk/gtknotebook.h>
#include <string.h>


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;


struct _EStorageBrowserPrivate {
	char *starting_path;
	char *current_path;

	GtkWidget *view_notebook;
	GtkWidget *storage_set_view;

	GHashTable *path_to_view; /* (char *, GtkWidget *) */

	EStorageBrowserCreateViewCallback create_view_callback;
	void *create_view_callback_data;
};


enum {
	WIDGETS_GONE,
	PAGE_SWITCHED,
	NUM_SIGNALS
};

static unsigned int signals[NUM_SIGNALS] = { 0 };


/* Callbacks.  */

static void
storage_set_view_folder_selected_callback (EStorageSetView *storage_set_view,
					   const char *path,
					   EStorageBrowser *browser)
{
	if (! e_storage_browser_show_path (browser, path)) {
		/* Make the selection go back to where it was.  */
		e_storage_browser_show_path (browser, browser->priv->current_path);
	}
}

static void
storage_set_removed_folder_callback (EStorageSet *storage_set,
				     const char *path,
				     EStorageBrowser *browser)
{
	if (g_hash_table_lookup (browser->priv->path_to_view, path) != NULL)
		e_storage_browser_remove_view_for_path (browser, path);
}

static void
view_notebook_weak_notify (EStorageBrowser *browser)
{
	browser->priv->view_notebook = NULL;

	if (browser->priv->storage_set_view == NULL)
		g_signal_emit (browser, signals[WIDGETS_GONE], 0);
}

static void
storage_set_view_weak_notify (EStorageBrowser *browser)
{
	browser->priv->storage_set_view = NULL;

	if (browser->priv->view_notebook == NULL)
		g_signal_emit (browser, signals[WIDGETS_GONE], 0);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EStorageBrowserPrivate *priv = E_STORAGE_BROWSER (object)->priv;

	if (priv->view_notebook != NULL) {
		g_object_weak_unref (G_OBJECT (priv->view_notebook),
				     (GWeakNotify) view_notebook_weak_notify,
				     object);
		priv->view_notebook = NULL;
	}

	if (priv->storage_set_view != NULL) {
		g_object_weak_unref (G_OBJECT (priv->storage_set_view),
				     (GWeakNotify) storage_set_view_weak_notify,
				     object);
		priv->storage_set_view = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EStorageBrowserPrivate *priv = E_STORAGE_BROWSER (object)->priv;

	g_free (priv->starting_path);
	g_free (priv->current_path);

	g_hash_table_destroy (priv->path_to_view);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (EStorageBrowserClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);

	signals[WIDGETS_GONE]
		= g_signal_new ("widgets_gone",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EStorageBrowserClass, widgets_gone),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[PAGE_SWITCHED]
		= g_signal_new ("page_switched",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (EStorageBrowserClass, page_switched),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_POINTER,
				G_TYPE_NONE, 2,
				G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
init (EStorageBrowser *browser)
{
	EStorageBrowserPrivate *priv;

	priv = g_new0 (EStorageBrowserPrivate, 1);

	priv->path_to_view = g_hash_table_new_full (g_str_hash, g_str_equal,
						    (GDestroyNotify) g_free,
						    (GDestroyNotify) g_object_unref);

	priv->view_notebook = gtk_notebook_new ();
	g_object_weak_ref (G_OBJECT (priv->view_notebook), (GWeakNotify) view_notebook_weak_notify, browser);

	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->view_notebook), FALSE);

	browser->priv = priv;
}


EStorageBrowser *
e_storage_browser_new  (EStorageSet *storage_set,
			const char *starting_path,
			EStorageBrowserCreateViewCallback create_view_callback,
			void *callback_data)
{
	EStorageBrowser *new;

	g_return_val_if_fail (create_view_callback != NULL, NULL);

	new = g_object_new (e_storage_browser_get_type (), NULL);

	new->priv->create_view_callback      = create_view_callback;
	new->priv->create_view_callback_data = callback_data;
	new->priv->starting_path             = g_strdup (starting_path);
	new->priv->storage_set_view          = e_storage_set_create_new_view (storage_set, NULL);

	g_object_weak_ref (G_OBJECT (new->priv->storage_set_view), (GWeakNotify) storage_set_view_weak_notify, new);

	g_signal_connect_object (new->priv->storage_set_view,
				 "folder_selected", G_CALLBACK (storage_set_view_folder_selected_callback),
				 G_OBJECT (new), 0);
	g_signal_connect_object (e_storage_set_view_get_storage_set (E_STORAGE_SET_VIEW (new->priv->storage_set_view)),
				 "removed_folder", G_CALLBACK (storage_set_removed_folder_callback),
				 G_OBJECT (new), 0);

	if (! e_storage_browser_show_path (new, starting_path)) {
		g_object_unref (new);
		return NULL;
	}

	return new;
}


GtkWidget *
e_storage_browser_peek_tree_widget (EStorageBrowser *browser)
{
	return browser->priv->storage_set_view;
}

GtkWidget *
e_storage_browser_peek_view_widget (EStorageBrowser *browser)
{
	return browser->priv->view_notebook;
}

EStorageSet *
e_storage_browser_peek_storage_set (EStorageBrowser *browser)
{
	return e_storage_set_view_get_storage_set (E_STORAGE_SET_VIEW (browser->priv->storage_set_view));
}

gboolean
e_storage_browser_show_path  (EStorageBrowser *browser,
			      const char *path)
{
	EStorageBrowserPrivate *priv = browser->priv;
	GtkWidget *current_view;
	GtkWidget *existing_view;
	GtkWidget *new_view;
	GtkNotebook *notebook;

	notebook = GTK_NOTEBOOK (priv->view_notebook);

	current_view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->view_notebook),
						  gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->view_notebook)));

	existing_view = g_hash_table_lookup (priv->path_to_view, path);
	if (existing_view != NULL) {
		gtk_notebook_set_current_page (notebook, gtk_notebook_page_num (notebook, existing_view));
		g_print ("page switched\n");
		g_signal_emit (browser, signals[PAGE_SWITCHED], 0, current_view, existing_view);
		return TRUE;
	}

	new_view = (* priv->create_view_callback) (browser, path, priv->create_view_callback_data);
	if (new_view == NULL)
		return FALSE;

	gtk_widget_show (new_view);
	gtk_notebook_append_page (notebook, new_view, NULL);
	gtk_notebook_set_current_page (notebook, gtk_notebook_page_num (notebook, new_view));

	g_print ("page switched\n");
	g_signal_emit (browser, signals[PAGE_SWITCHED], 0, current_view, new_view);

	g_hash_table_insert (priv->path_to_view, g_strdup (path), new_view);

	g_free (priv->current_path);
	priv->current_path = g_strdup (path);

	e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view), path);

	return TRUE;
}

void
e_storage_browser_remove_view_for_path (EStorageBrowser *browser,
					const char *path)
{
	GtkWidget *view;

	if (strcmp (path, browser->priv->starting_path) == 0) {
		g_warning (G_GNUC_FUNCTION ": cannot remove starting view");
		return;
	}

	view = g_hash_table_lookup (browser->priv->path_to_view, path);
	if (view == NULL) {
		g_warning (G_GNUC_FUNCTION ": no view for %s", path);
		return;
	}

	g_hash_table_remove (browser->priv->path_to_view, path);
	gtk_widget_destroy (view);

	e_storage_browser_show_path (browser, browser->priv->starting_path);
}


E_MAKE_TYPE (e_storage_browser, "EStorageBrowser", EStorageBrowser, class_init, init, PARENT_TYPE)
