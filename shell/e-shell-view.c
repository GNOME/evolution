/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.c
 *
 * Copyright (C) 2000, 2001, 2002 Ximian, Inc.
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
 * Authors:
 *   Ettore Perazzoli <ettore@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
 *   Matt Loper <matt@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-view.h"

#include "evolution-shell-view.h"

#include "e-shell-marshal.h"

#include "e-history.h"
#include "e-icon-factory.h"
#include "e-shell-constants.h"
#include "e-shell-folder-title-bar.h"
#include "e-shell-utils.h"
#include "e-shell-view-menu.h"
#include "e-shell.h"
#include "e-shortcuts-view.h"
#include "e-storage-set-view.h"
#include "e-title-bar.h"

#include "e-util/e-gtk-utils.h"

#include "widgets/misc/e-clipped-label.h"

#include <gtk/gtkwidget.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

#include <glib.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-window.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomeui/gnome-app.h>

#include <gtk/gtkscrolledwindow.h>
#include <gconf/gconf-client.h>

#include <bonobo/bonobo-socket.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-engine.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-window.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

static BonoboWindowClass *parent_class = NULL;

struct _View {
	char *uri;
	GtkWidget *control;
	EFolder *folder;
};
typedef struct _View View;

struct _EShellViewPrivate {
	/* The shell.  */
	EShell *shell;

	/* EvolutionShellView Bonobo object for implementing the
           Evolution::ShellView interface.  */
	EvolutionShellView *corba_interface;

	/* The UI handler & container.  */
	BonoboUIComponent *ui_component;
	BonoboUIContainer *ui_container;

	/* History of visited (evolution:) URIs. */
	EHistory *history;

	/* Currently displayed URI.  */
	char *uri;

	/* Delayed selection, used when a path doesn't exist in an EStorage.
           Cleared when we're signaled with "folder_selected".  */
	char *delayed_selection;

	/* uri to go to at timeout */
	unsigned int set_folder_timeout;
	char        *set_folder_uri;

	/* Tooltips.  */
	GtkTooltips *tooltips;

	/* The widgetry.  */
	GtkWidget *appbar;
	GtkWidget *hpaned;
	GtkWidget *view_vbox;
	GtkWidget *folder_title_bar;
	GtkWidget *view_hpaned;
	GtkWidget *contents;
	GtkWidget *notebook;
	GtkWidget *shortcut_frame;
	GtkWidget *shortcut_bar;
	GtkWidget *storage_set_title_bar;
	GtkWidget *storage_set_view;
	GtkWidget *storage_set_view_box;

	/* The status bar widgetry.  */
	GtkWidget *status_bar;
	GtkWidget *offline_toggle;
	GtkWidget *offline_toggle_image;
	GtkWidget *menu_hint_label;
	GtkWidget *task_bar;

	/* The pop-up window for the folder-tree (i.e. the one we create when
	   the user clicks on the folder title.  */
	GtkWidget *folder_bar_popup;

	/* The views we have already open.  */
	GHashTable *uri_to_view;

	/* Position of the handles in the paneds, to be restored when we show elements
           after hiding them.  */
	unsigned int hpaned_position;
	unsigned int view_hpaned_position;

	/* Whether the shortcut and folder bars are visible or not.  */
	unsigned int shortcut_bar_shown : 1;
	unsigned int folder_bar_shown : 1;

	/* List of sockets we created.  */
	GList *sockets;
};

enum {
	SHORTCUT_BAR_VISIBILITY_CHANGED,
	FOLDER_BAR_VISIBILITY_CHANGED,
	VIEW_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_SHORTCUT_BAR_WIDTH 100

#define DEFAULT_TREE_WIDTH         130
#define MIN_POPUP_TREE_WIDTH       130

#define DEFAULT_WIDTH 705
#define DEFAULT_HEIGHT 550

#define SET_FOLDER_DELAY 250

/* URI to display when the currently displayed folder is removed from the
   storage.  */
#define FALLBACK_URI E_SUMMARY_URI


/* The icons for the offline/online status.  */

static GdkPixmap *offline_pixmap = NULL;
static GdkBitmap *offline_mask = NULL;

static GdkPixmap *online_pixmap = NULL;
static GdkBitmap *online_mask = NULL;


static void        update_for_current_uri         (EShellView *shell_view);
static void        update_offline_toggle_status   (EShellView *shell_view);
static const char *get_storage_set_path_from_uri  (const char *uri);


/* Boo.  */
static void new_folder_cb (EStorageSet *storage_set, const char *path, void *data);
static gboolean display_uri (EShellView *shell_view, const char *uri,
			     gboolean add_to_history, gboolean queue);


/* View handling.  */

static View *
view_new (const char *uri,
	  GtkWidget *control)
{
	View *new;

	new = g_new (View, 1);
	new->uri     = g_strdup (uri);
	new->control = control;

	return new;
}

static void
view_destroy (View *view)
{
	g_free (view->uri);
	g_free (view);
}


/* Utility functions.  */

static void
update_other_users_folder_items_sensitivity (EShellView *shell_view)
{
	EShellViewPrivate *priv = shell_view->priv;
	gboolean a_storage_supports_shared_folders;
	GList *storage_list, *p;

	storage_list = e_storage_set_get_storage_list (e_shell_get_storage_set (priv->shell));
	a_storage_supports_shared_folders = FALSE;
	for (p = storage_list; p != NULL; p = p->next) {
		if (e_storage_supports_shared_folders (E_STORAGE (p->data)))
			a_storage_supports_shared_folders = TRUE;
	}

	if (a_storage_supports_shared_folders) {
		bonobo_ui_component_set_prop (priv->ui_component, "/commands/FileOpenOtherUsersFolder",
					      "sensitive", "1", NULL);
		bonobo_ui_component_set_prop (priv->ui_component, "/commands/FileRemoveOtherUsersFolder",
					      "sensitive", "1", NULL);
	} else {
		bonobo_ui_component_set_prop (priv->ui_component, "/commands/FileOpenOtherUsersFolder",
					      "sensitive", "0", NULL);
		bonobo_ui_component_set_prop (priv->ui_component, "/commands/FileRemoveOtherUsersFolder",
					      "sensitive", "0", NULL);
	}

	g_list_foreach (storage_list, (GFunc) g_object_unref, NULL);
	g_list_free (storage_list);
}

static GtkWidget *
create_label_for_empty_page (void)
{
	GtkWidget *label;

	label = e_clipped_label_new (_("(No folder displayed)"), PANGO_WEIGHT_NORMAL, 1.0);
	gtk_widget_show (label);

	return label;
}

/* Initialize the icons.  */
static void
load_images (void)
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_file (EVOLUTION_IMAGES "/offline.png", NULL);
	if (pixbuf == NULL) {
		g_warning ("Cannot load `%s'", EVOLUTION_IMAGES "/offline.png");
	} else {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &offline_pixmap, &offline_mask, 128);
		g_object_unref (pixbuf);
	}

	pixbuf = gdk_pixbuf_new_from_file (EVOLUTION_IMAGES "/online.png", NULL);
	if (pixbuf == NULL) {
		g_warning ("Cannot load `%s'", EVOLUTION_IMAGES "/online.png");
	} else {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &online_pixmap, &online_mask, 128);
		g_object_unref (pixbuf);
	}
}

static void
cleanup_delayed_selection (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	if (priv->delayed_selection != NULL) {
		g_free (priv->delayed_selection);
		priv->delayed_selection = NULL;
		g_signal_handlers_disconnect_by_func (e_shell_get_storage_set (priv->shell),
						      G_CALLBACK (new_folder_cb), shell_view);
	}
}

static GtkWidget *
find_socket (GtkContainer *container)
{
	GList *children, *tmp;

	children = gtk_container_get_children(container);
	while (children) {
		if (BONOBO_IS_SOCKET (children->data))
			return children->data;
		else if (GTK_IS_CONTAINER (children->data)) {
			GtkWidget *socket = find_socket (children->data);
			if (socket)
				return socket;
		}
		tmp = children->next;
		g_list_free_1 (children);
		children = tmp;
	}
	return NULL;
}

static void
setup_verb_sensitivity_for_folder (EShellView *shell_view,
				   const char *path)
{
	EShellViewPrivate *priv;
	BonoboUIComponent *ui_component;
	EFolder *folder;
	const char *prop;

	priv = shell_view->priv;

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);

	if (path == NULL)
		folder = NULL;
	else
		folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell), path);

	/* Adjust sensitivity for menu options depending on whether the folder
	   selected can actually be manipulated or not.  */

	if (folder != NULL
	    && ! e_folder_get_is_stock (folder)
	    && e_folder_get_physical_uri (folder) != NULL)
		prop = "1";
	else
		prop = "0";
	bonobo_ui_component_set_prop (ui_component, "/commands/MoveFolder", "sensitive", prop, NULL);
	bonobo_ui_component_set_prop (ui_component, "/commands/CopyFolder", "sensitive", prop, NULL);
	bonobo_ui_component_set_prop (ui_component, "/commands/DeleteFolder", "sensitive", prop, NULL);
	bonobo_ui_component_set_prop (ui_component, "/commands/RenameFolder", "sensitive", prop, NULL);

	/* Adjust sensitivity for menu options depending on whether the user
	   right-clicked a folder whose contents can be viewed.  */

	if (folder != NULL
	    && e_folder_type_registry_get_handler_for_type (e_shell_get_folder_type_registry (e_shell_view_get_shell (shell_view)),
							    e_folder_get_type_string (folder)) != NULL)
		prop = "1";
	else
		prop = "0";
	bonobo_ui_component_set_prop (ui_component, "/commands/ActivateView", "sensitive", prop, NULL);
	bonobo_ui_component_set_prop (ui_component, "/commands/OpenFolderInNewWindow", "sensitive", prop, NULL);
	bonobo_ui_component_set_prop (ui_component, "/commands/AddFolderToShortcutBar", "sensitive", prop, NULL);
}


static void
update_navigation_buttons (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	e_shell_folder_title_bar_update_navigation_buttons (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar),
							    e_history_has_prev (priv->history),
							    e_history_has_next (priv->history));
}

static int
history_uri_matching_func (const void *a,
			   const void *b)
{
	const char *s1, *s2;

	s1 = (const char *) a;
	s2 = (const char *) b;

	return strcmp (s1, s2);
}

static void
remove_uri_from_history (EShellView *shell_view,
			 const char *uri)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;
	
	e_history_remove_matching (priv->history, uri, history_uri_matching_func);
}


static void
setup_defaults (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	EShortcutBar *shortcut_bar;
	GConfClient *client;
	char *file_name;
	int shortcut_group;
	int width;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = shell_view->priv;
	shortcut_bar = E_SHORTCUT_BAR (priv->shortcut_bar);

	client = gconf_client_get_default ();

	gtk_window_set_default_size (GTK_WINDOW (shell_view),
				     gconf_client_get_int (client, "/apps/evolution/shell/view_defaults/width", NULL),
				     gconf_client_get_int (client, "/apps/evolution/shell/view_defaults/height", NULL));

	shortcut_group = gconf_client_get_int (client, "/apps/evolution/shell/view_defaults/selected_shortcut_group", NULL);
	e_shell_view_set_current_shortcuts_group_num (shell_view, shortcut_group);

	e_shell_view_show_folder_bar (shell_view,
				      gconf_client_get_bool (client, "/apps/evolution/shell/view_defaults/show_folder_bar", NULL));
	e_shell_view_show_shortcut_bar (shell_view,
					gconf_client_get_bool (client, "/apps/evolution/shell/view_defaults/show_shortcut_bar", NULL));

	width = gconf_client_get_int (client, "/apps/evolution/shell/view_defaults/shortcut_bar/width", NULL);
	if (priv->shortcut_bar_shown)
		gtk_paned_set_position (GTK_PANED (priv->hpaned), width);
	priv->hpaned_position = width;

	width = gconf_client_get_int (client, "/apps/evolution/shell/view_defaults/folder_bar/width", NULL);
	if (priv->folder_bar_shown)
		gtk_paned_set_position (GTK_PANED (priv->view_hpaned), width);
	priv->view_hpaned_position = width;

	/* Load the expanded state for the ShellView's StorageSetView */
	file_name = g_strdup_printf ("%s/config/storage-set-view-expanded:default",
				     e_shell_get_local_directory (priv->shell));
	e_tree_load_expanded_state (E_TREE (priv->storage_set_view),
				    file_name);
	g_free (file_name);
}


/* This implements the behavior for when the folder which is currently displayed
   gets deleted.  */

/* Find the path for an Inbox in the specified storage.  This is not really
   100% correct, but should work for most cases.  */
static char *
find_inbox_in_storage (EShellView *shell_view,
		       const char *storage_name)
{
	EShellViewPrivate *priv;
	EStorageSet *storage_set;
	EStorage *storage;
	GList *subfolder_paths;
	GList *p;

	priv = shell_view->priv;
	storage_set = e_shell_get_storage_set (priv->shell);
	storage = e_storage_set_get_storage (storage_set, storage_name);

	subfolder_paths = e_storage_get_subfolder_paths (storage, "/");
	for (p = subfolder_paths; p != NULL; p = p->next) {
		const char *path;
		char *casefold_i18n_inbox_name;
		char *casefold_path;

		path = (const char *) p->data;

		casefold_i18n_inbox_name = g_utf8_casefold (_("Inbox"), -1);
		casefold_path = g_utf8_casefold (path + 1, -1);

		if (g_utf8_collate (casefold_path, "/inbox") == 0
		    || g_utf8_collate (casefold_path + 1, _("Inbox")) == 0) {
			char *return_path;

			return_path = g_strconcat ("/", storage_name, "/", path,
						   NULL);
			e_free_string_list (subfolder_paths);

			g_free (casefold_i18n_inbox_name);
			g_free (casefold_path);
			return return_path;
		}

		g_free (casefold_i18n_inbox_name);
		g_free (casefold_path);
	}

	e_free_string_list (subfolder_paths);

	return NULL;
}

static void
handle_current_folder_removed (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	const char *current_path;
	const char *p;
	char *new_path;

	/* Note: we assume that priv->uri is an evolution: URI.  */

	priv = shell_view->priv;

	current_path = priv->uri + E_SHELL_URI_PREFIX_LEN;

	g_assert (*current_path == E_PATH_SEPARATOR);

	new_path = NULL;

	/* If we have a parent folder (not a parent storage), try to display
	   that one.  */

	p = strrchr (current_path + 1, E_PATH_SEPARATOR);
	if (p != NULL && p[1] != '\0' && strchr (current_path + 1, E_PATH_SEPARATOR) != p) {
		new_path = g_strndup (current_path, p - current_path);
	} else {
		/* We don't have a parent folder, so try to see if there is an
		   Inbox folder in the same storage.  */

		/* Extract the storage name.  */
		p = strchr (current_path + 1, E_PATH_SEPARATOR);
		if (p == NULL) {
			/* The URL points itself to a storage, so just redirect
			   to the default case.  */
			new_path = NULL;
		} else {
			char *storage_name;

			storage_name = g_strndup (current_path + 1, p - current_path - 1);

			new_path = find_inbox_in_storage (shell_view, storage_name);
			if (new_path == NULL) {
				char *storage_uri;

				/* No Inbox in this storage -- fallback to the storage.  */
				storage_uri = g_strconcat (E_SHELL_URI_PREFIX, storage_name, NULL);
				e_shell_view_display_uri (shell_view, storage_uri, TRUE);

				g_free (storage_uri);
				g_free (storage_name);
				return;
			}

			g_free (storage_name);
		}
	}

	if (new_path == NULL) {
		e_shell_view_display_uri (shell_view, FALLBACK_URI, TRUE);
	} else {
		EFolder *folder;

		/* Check that the folder we have chosen exists; if it doesn't,
		   we just use the fallback URI.  */

		folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell), new_path);
		if (folder == NULL) {
			e_shell_view_display_uri (shell_view, FALLBACK_URI, TRUE);
		} else {
			char *new_uri;

			new_uri = g_strconcat (E_SHELL_URI_PREFIX, new_path, NULL);
			e_shell_view_display_uri (shell_view, new_uri, TRUE);
			g_free (new_uri);
		}

		g_free (new_path);
	}
}


/* Callbacks for the EStorageSet.  */

static void
storage_set_new_storage_callback (EStorageSet *storage_set,
				  EStorage *storage,
				  void *data)
{
	if (e_storage_supports_shared_folders (storage))
		update_other_users_folder_items_sensitivity (E_SHELL_VIEW (data));
}

static void
storage_set_removed_storage_callback (EStorageSet *storage_set,
				      EStorage *storage,
				      void *data)
{
	EShellView *shell_view = E_SHELL_VIEW (data);

	if (! e_storage_supports_shared_folders (storage))
		return;

	update_other_users_folder_items_sensitivity (shell_view);
}

static void
storage_set_removed_folder_callback (EStorageSet *storage_set,
				     const char *path,
				     void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	GtkWidget *socket;
	View *view;
	int destroy_connection_id;
	int page_num;
	char *uri;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);

	remove_uri_from_history (shell_view, uri);
	update_navigation_buttons (shell_view);

	/* (Note that at this point the current URI in the history might have
	   been changed and not match the current view.  But we catch this case
	   when checking if this was the current view, below.)  */

	view = g_hash_table_lookup (priv->uri_to_view, uri);

	g_free (uri);

	if (view == NULL)
		return;

	socket = find_socket (GTK_CONTAINER (view->control));
	priv->sockets = g_list_remove (priv->sockets, socket);

	destroy_connection_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (socket), "e_shell_view_destroy_connection_id"));
	g_signal_handler_disconnect((socket), destroy_connection_id);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), view->control);

	/* Check if it's the URI that we are currently displaying.  */
	if (strncmp (priv->uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0
	    && strcmp (priv->uri + E_SHELL_URI_PREFIX_LEN, path) == 0) {
		handle_current_folder_removed (shell_view);
	}

	bonobo_control_frame_control_deactivate (BONOBO_CONTROL_FRAME (bonobo_widget_get_control_frame (BONOBO_WIDGET (view->control))));
	gtk_widget_destroy (view->control);

	g_hash_table_remove (priv->uri_to_view, view->uri);
	view_destroy (view);

	gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), page_num);
}


/* Folder bar pop-up handling.  */

static void
reparent (GtkWidget *widget,
	  GtkContainer *new_container)
{
	gtk_widget_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_container_add (GTK_CONTAINER (new_container), widget);
	gtk_widget_unref (widget);
}

static void
reparent_storage_set_view_box_and_destroy_popup (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	if (priv->folder_bar_popup == NULL)
		return;

	gtk_widget_ref (priv->storage_set_view_box);
	gtk_container_remove (GTK_CONTAINER (priv->folder_bar_popup), priv->storage_set_view_box);
	gtk_paned_pack1 (GTK_PANED (priv->view_hpaned), priv->storage_set_view_box, FALSE, FALSE);
	gtk_widget_unref (priv->storage_set_view_box);

	gtk_widget_destroy (priv->folder_bar_popup);
	priv->folder_bar_popup = NULL;

	/* Re-enable DnD on the StorageSetView (it got disabled when displaying
	   the pop-up).  */
	e_storage_set_view_set_allow_dnd (E_STORAGE_SET_VIEW (priv->storage_set_view), TRUE);
}

static void
popdown_transient_folder_bar (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	gtk_grab_remove (priv->storage_set_view_box);

	reparent_storage_set_view_box_and_destroy_popup (shell_view);
	gtk_widget_hide (priv->storage_set_view_box);

	e_shell_folder_title_bar_set_toggle_state (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar), FALSE);

	/* Re-enable DnD on the StorageSetView (it got disabled when displaying
	   the pop-up).  */
	e_storage_set_view_set_allow_dnd (E_STORAGE_SET_VIEW (priv->storage_set_view), TRUE);
}

static int
storage_set_view_box_button_release_event_cb (GtkWidget *widget,
					      GdkEventButton *button_event,
					      void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	if (button_event->window == GTK_PANED (priv->view_hpaned)->handle
	    || button_event->button != 1)
		return FALSE;

	popdown_transient_folder_bar (shell_view);
	return TRUE;
}

static void
storage_set_view_folder_opened_cb (EStorageSetView *storage_set_view,
				   const char *path,
				   void *data)
{
	/* Pop down for top level nodes, see #31303.  */
	if (strchr (path + 1, E_PATH_SEPARATOR) == NULL)
		popdown_transient_folder_bar (E_SHELL_VIEW (data));
}

static void
popup_storage_set_view_button_clicked (ETitleBar *title_bar,
				       void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	g_assert (priv->folder_bar_popup != NULL);

	reparent_storage_set_view_box_and_destroy_popup (shell_view);

	e_shell_view_show_folder_bar (shell_view, TRUE);
	e_shell_folder_title_bar_set_toggle_state (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar), FALSE);
}

static void
folder_bar_popup_map_callback (GtkWidget *widget,
			       void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	guint32 current_time;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	current_time = GDK_CURRENT_TIME;

	if (gdk_pointer_grab (widget->window, TRUE,
			      (GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_POINTER_MOTION_MASK),
			      NULL, NULL, current_time) != 0) {
		g_warning ("e-shell-view.c:folder_bar_popup_map_callback() -- pointer grab failed.");
		return;
	}

	if (gdk_keyboard_grab (widget->window, TRUE, 0) != 0) {
		g_warning ("e-shell-view.c:folder_bar_popup_map_callback() -- keyboard grab failed.");
		gdk_pointer_ungrab (current_time);
		return;
	}

	gtk_grab_add (widget);

	e_signal_connect_while_alive (widget, "button_release_event",
				      G_CALLBACK (storage_set_view_box_button_release_event_cb),
				      shell_view, priv->folder_bar_popup);
	e_signal_connect_while_alive (priv->storage_set_view, "folder_opened",
				      G_CALLBACK (storage_set_view_folder_opened_cb),
				      shell_view, priv->folder_bar_popup);
	e_signal_connect_while_alive (priv->storage_set_view, "button_release_event",
				      G_CALLBACK (storage_set_view_box_button_release_event_cb),
				      shell_view, priv->folder_bar_popup);
	e_signal_connect_while_alive (priv->storage_set_title_bar, "button_clicked",
				      G_CALLBACK (popup_storage_set_view_button_clicked),
				      shell_view, priv->folder_bar_popup);
}

static void
pop_up_folder_bar (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	int x, y;
	int orig_x, orig_y;

	priv = shell_view->priv;

	g_assert (! priv->folder_bar_shown);

	e_title_bar_set_button_mode (E_TITLE_BAR (priv->storage_set_title_bar),
				     E_TITLE_BAR_BUTTON_MODE_PIN);

	priv->folder_bar_popup = gtk_window_new (GTK_WINDOW_POPUP);

	/* We need to show the storage set view box and do a pointer grab to catch the
           mouse clicks.  But until the box is shown, we cannot grab.  So we connect to
           the "map" signal; `storage_set_view_box_map_cb()' will do the grab.  */
	g_signal_connect (priv->folder_bar_popup, "map",
			  G_CALLBACK (folder_bar_popup_map_callback), shell_view);

	x = priv->folder_title_bar->allocation.x;
	y = priv->folder_title_bar->allocation.y + priv->folder_title_bar->allocation.height;

	gdk_window_get_origin (priv->folder_title_bar->window, &orig_x, &orig_y);
	x += orig_x;
	y += orig_y + 2;

	priv->view_hpaned_position = MAX (priv->view_hpaned_position, MIN_POPUP_TREE_WIDTH);

	gtk_window_set_default_size (GTK_WINDOW (priv->folder_bar_popup),
				     priv->view_hpaned_position,
				     priv->view_hpaned->allocation.height);

	reparent (priv->storage_set_view_box, GTK_CONTAINER (priv->folder_bar_popup));

	gtk_widget_show (priv->storage_set_view_box);

	gtk_window_move (GTK_WINDOW (priv->folder_bar_popup), x, y);
	gtk_widget_show (priv->folder_bar_popup);

	/* Disable DnD or "interesting" things will happen.  */
	e_storage_set_view_set_allow_dnd (E_STORAGE_SET_VIEW (priv->storage_set_view), FALSE);
}



/* Switching views on a tree view click.  */

static int
set_folder_timeout (gpointer data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	/* Set to 0 so we don't remove it in _display_uri().  */
	priv->set_folder_timeout = 0;
	e_shell_view_display_uri (shell_view, priv->set_folder_uri, TRUE);

	return FALSE;
}

static int
popdown_transient_folder_bar_idle (void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	popdown_transient_folder_bar (shell_view);

	g_object_unref (shell_view);

	return FALSE;
}

static void
switch_on_folder_tree_click (EShellView *shell_view,
			     const char *path)
{
	EShellViewPrivate *priv;
	char *uri;

	priv = shell_view->priv;

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	gtk_grab_remove (priv->storage_set_view_box);

	uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);
	if (priv->uri != NULL && !strcmp (uri, priv->uri)) {
		g_free (uri);
		return;
	}

	if (priv->set_folder_timeout != 0)
		gtk_timeout_remove (priv->set_folder_timeout);

	g_free (priv->set_folder_uri);
	priv->set_folder_uri = NULL;

	cleanup_delayed_selection (shell_view);

	if (priv->folder_bar_popup != NULL) {
		e_shell_view_display_uri (shell_view, uri, TRUE);
		g_free (uri);

		g_object_ref (shell_view);
		gtk_idle_add (popdown_transient_folder_bar_idle, shell_view);
		return;
	}

	priv->set_folder_uri = uri;

	priv->set_folder_timeout = gtk_timeout_add (SET_FOLDER_DELAY, set_folder_timeout, shell_view);
}


/* Callbacks.  */

/* Callback when a new folder is added.  Removed when we clear the
   delayed_selection.  */
static void
new_folder_cb (EStorageSet *storage_set,
	       const char *path,
	       void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	char *delayed_path;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	delayed_path = strchr (priv->delayed_selection, ':');
	if (delayed_path) {
		delayed_path ++;
		if (!strcmp(path, delayed_path)) {
			char *uri;

			uri = g_strdup (priv->delayed_selection);
			cleanup_delayed_selection (shell_view);
			e_shell_view_display_uri (shell_view, uri, FALSE);
			g_free (uri);
		}
	}
}

/* Callback called when an icon on the shortcut bar gets clicked.  */
static void
activate_shortcut_cb (EShortcutsView *shortcut_view,
		      EShortcuts *shortcuts,
		      const char *uri,
		      gboolean in_new_window,
		      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	if (in_new_window) {
		EShellView *new_view;

		new_view = e_shell_create_view (e_shell_view_get_shell (shell_view), uri, shell_view);
		e_shell_view_show_shortcut_bar (new_view, FALSE);
		e_shell_view_show_folder_bar (new_view, FALSE);
	} else {
		e_shell_view_display_uri (shell_view, uri, TRUE);
	}
}

/* Callback when user chooses "Hide shortcut bar" via a right click */
static void
hide_requested_cb (EShortcutsView *shortcut_view,
		   void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	e_shell_view_show_shortcut_bar (shell_view, FALSE);
}

/* Callback called when a folder on the tree view gets clicked.  */
static void
folder_selected_cb (EStorageSetView *storage_set_view,
		    const char *path,
		    void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	setup_verb_sensitivity_for_folder (shell_view, path);
	switch_on_folder_tree_click (shell_view, path);
}

/* Callbacks for the folder context menu in the folder bar.  */

static void
folder_context_menu_popping_up_cb (EStorageSetView *storage_set_view,
				   const char *path,
				   void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	setup_verb_sensitivity_for_folder (shell_view, path);
}

static void
folder_context_menu_popped_down_cb (EStorageSetView *storage_set_view,
				    void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	setup_verb_sensitivity_for_folder (shell_view, e_shell_view_get_current_path (shell_view));

	if (shell_view->priv->folder_bar_popup != NULL)
		popdown_transient_folder_bar (shell_view);
}

/* Callback called when the button on the tree's title bar is clicked.  */
static void
storage_set_view_button_clicked_cb (ETitleBar *title_bar,
				    void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	if (shell_view->priv->folder_bar_popup == NULL)
		e_shell_view_show_folder_bar (shell_view, FALSE);
}

/* Callback called when the title bar button is clicked.  */
static void
title_bar_toggled_cb (EShellFolderTitleBar *title_bar,
		      gboolean state,
		      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	if (! state)
		return;

	if (shell_view->priv->folder_bar_popup == NULL)
		pop_up_folder_bar (shell_view);
}

/* Callback called when the offline toggle button is clicked.  */
static void
offline_toggle_clicked_cb (GtkButton *button,
			   void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	switch (e_shell_get_line_status (priv->shell)) {
	case E_SHELL_LINE_STATUS_ONLINE:
		e_shell_go_offline (priv->shell, shell_view);
		break;
	case E_SHELL_LINE_STATUS_OFFLINE:
		e_shell_go_online (priv->shell, shell_view);
		break;
	default:
		g_assert_not_reached ();
	}
}


/* Navigation button callbacks.  */

static void
back_clicked_callback (EShellFolderTitleBar *title_bar,
		       void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	const char *new_uri;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	if (! e_history_has_prev (priv->history))
		return;

	new_uri = (const char *) e_history_prev (priv->history);

	display_uri (shell_view, new_uri, FALSE, TRUE);
}

static void
forward_clicked_callback (EShellFolderTitleBar *title_bar,
			  void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	const char *new_uri;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	if (! e_history_has_next (priv->history))
		return;

	new_uri = (const char *) e_history_next (priv->history);

	display_uri (shell_view, new_uri, FALSE, TRUE);
}


/* Widget setup.  */

static void
setup_storage_set_subwindow (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	GtkWidget *storage_set_view;
	GtkWidget *vbox;
	GtkWidget *scrolled_window;

	priv = shell_view->priv;

	storage_set_view = e_storage_set_create_new_view (e_shell_get_storage_set (priv->shell),
							  priv->ui_container);
	g_signal_connect (storage_set_view, "folder_selected",
			  G_CALLBACK (folder_selected_cb), shell_view);
	g_signal_connect (storage_set_view, "folder_context_menu_popping_up",
			  G_CALLBACK (folder_context_menu_popping_up_cb), shell_view);
	g_signal_connect (storage_set_view, "folder_context_menu_popped_down",
			  G_CALLBACK (folder_context_menu_popped_down_cb), shell_view);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);

	gtk_container_add (GTK_CONTAINER (scrolled_window), storage_set_view);

	vbox = gtk_vbox_new (FALSE, 0);

	priv->storage_set_title_bar = e_title_bar_new (_("Folders"));

	gtk_box_pack_start (GTK_BOX (vbox), priv->storage_set_title_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

	g_signal_connect (priv->storage_set_title_bar, "button_clicked",
			  G_CALLBACK (storage_set_view_button_clicked_cb), shell_view);

	gtk_widget_show (storage_set_view);
	gtk_widget_show (priv->storage_set_title_bar);
	gtk_widget_show (scrolled_window);

	priv->storage_set_view_box = vbox;
	priv->storage_set_view = storage_set_view;

	/* Notice we don't show the vbox here yet.  By default it's hidden.  */
}

static void
setup_offline_toggle (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	GtkWidget *toggle;
	GtkWidget *image;

	priv = shell_view->priv;

	toggle = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (toggle, GTK_CAN_FOCUS);
	gtk_button_set_relief (GTK_BUTTON (toggle), GTK_RELIEF_NONE);

	g_signal_connect (toggle, "clicked",
			  G_CALLBACK (offline_toggle_clicked_cb), shell_view);

	image = gtk_image_new_from_pixmap (offline_pixmap, offline_mask);

	gtk_container_add (GTK_CONTAINER (toggle), image);

	gtk_widget_show (toggle);
	gtk_widget_show (image);

	priv->offline_toggle       = toggle;
	priv->offline_toggle_image = image;

	update_offline_toggle_status (shell_view);

	g_assert (priv->status_bar != NULL);

	gtk_box_pack_start (GTK_BOX (priv->status_bar), priv->offline_toggle, FALSE, TRUE, 0);
}

static void
setup_menu_hint_label (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	priv->menu_hint_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->menu_hint_label), 0.0, 0.5);

	gtk_box_pack_start (GTK_BOX (priv->status_bar), priv->menu_hint_label, TRUE, TRUE, 0);
}

static void
setup_task_bar (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	priv->task_bar = e_task_bar_new ();

	g_assert (priv->status_bar != NULL);

	gtk_box_pack_start (GTK_BOX (priv->status_bar), priv->task_bar, TRUE, TRUE, 0);
	gtk_widget_show (priv->task_bar);
}

static void
create_status_bar (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	priv->status_bar = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (priv->status_bar);

	setup_offline_toggle (shell_view);
	setup_menu_hint_label (shell_view);
	setup_task_bar (shell_view);
}


/* Menu hints for the status bar.  */

static void
ui_engine_add_hint_callback (BonoboUIEngine *engine,
			     const char *hint,
			     void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	gtk_label_set_text (GTK_LABEL (priv->menu_hint_label), hint);
	gtk_widget_show (priv->menu_hint_label);
	gtk_widget_hide (priv->task_bar);
}

static void
ui_engine_remove_hint_callback (BonoboUIEngine *engine,
				void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	gtk_widget_hide (priv->menu_hint_label);
	gtk_widget_show (priv->task_bar);
}

static void
setup_statusbar_hints (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	BonoboUIEngine *ui_engine;

	priv = shell_view->priv;

	g_assert (priv->status_bar != NULL);

	ui_engine = bonobo_window_get_ui_engine (BONOBO_WINDOW (shell_view));
 
	g_signal_connect (ui_engine, "add_hint",
			  G_CALLBACK (ui_engine_add_hint_callback), shell_view);
	g_signal_connect (ui_engine, "remove_hint",
			  G_CALLBACK (ui_engine_remove_hint_callback), shell_view);
}


static void
setup_widgets (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	GtkWidget *contents_vbox;
	GtkWidget *gray_bar;

	priv = shell_view->priv;

	/* The shortcut bar.  */

	priv->shortcut_bar = e_shortcuts_new_view (e_shell_get_shortcuts (priv->shell));
	g_signal_connect (priv->shortcut_bar, "activate_shortcut",
			  G_CALLBACK (activate_shortcut_cb), shell_view);

	g_signal_connect (priv->shortcut_bar, "hide_requested",
			  G_CALLBACK (hide_requested_cb), shell_view);

	priv->shortcut_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->shortcut_frame), GTK_SHADOW_IN);

	/* The storage set view.  */

	setup_storage_set_subwindow (shell_view);

	/* The tabless notebook which we used to contain the views.  */

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);

	/* Page for "No URL displayed" message.  */

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), create_label_for_empty_page (), NULL);

	/* Put things into a paned and the paned into the GnomeApp.  */

	priv->view_vbox = gtk_vbox_new (FALSE, 0);

	priv->folder_title_bar = e_shell_folder_title_bar_new ();
	g_signal_connect (priv->folder_title_bar, "title_toggled",
			  G_CALLBACK (title_bar_toggled_cb), shell_view);
	g_signal_connect (priv->folder_title_bar, "back_clicked",
			  G_CALLBACK (back_clicked_callback), shell_view);
	g_signal_connect (priv->folder_title_bar, "forward_clicked",
			  G_CALLBACK (forward_clicked_callback), shell_view);

	priv->view_hpaned = gtk_hpaned_new ();
	gtk_paned_pack1 (GTK_PANED (priv->view_hpaned), priv->storage_set_view_box, TRUE, FALSE);
	gtk_paned_pack2 (GTK_PANED (priv->view_hpaned), priv->notebook, TRUE, FALSE);

	gray_bar = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (gray_bar), priv->folder_title_bar);
	gtk_box_pack_start (GTK_BOX (priv->view_vbox), gray_bar, FALSE, FALSE, 2);

	gtk_box_pack_start (GTK_BOX (priv->view_vbox), priv->view_hpaned, TRUE, TRUE, 0);

	priv->hpaned = gtk_hpaned_new ();
	gtk_container_add (GTK_CONTAINER (priv->shortcut_frame), priv->shortcut_bar);
	gtk_paned_pack1 (GTK_PANED (priv->hpaned), priv->shortcut_frame, TRUE, FALSE);
	gtk_paned_pack2 (GTK_PANED (priv->hpaned), priv->view_vbox, TRUE, FALSE);
	gtk_paned_set_position (GTK_PANED (priv->hpaned), DEFAULT_SHORTCUT_BAR_WIDTH);

	/* The status bar.  */

	create_status_bar (shell_view);
	setup_statusbar_hints (shell_view);

	/* The contents.  */

	contents_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (contents_vbox), priv->hpaned, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (contents_vbox), priv->status_bar, FALSE, TRUE, 0);
	gtk_widget_show (contents_vbox);

	bonobo_window_set_contents (BONOBO_WINDOW (shell_view), contents_vbox);

	/* Show stuff.  */

	gtk_widget_show (priv->shortcut_frame);
	gtk_widget_show (priv->shortcut_bar);
	gtk_widget_show (priv->storage_set_view);
	gtk_widget_show (priv->notebook);
	gtk_widget_show (priv->hpaned);
	gtk_widget_show (priv->view_hpaned);
	gtk_widget_show (priv->view_vbox);
	gtk_widget_show (priv->folder_title_bar);
	gtk_widget_show (priv->status_bar);

	gtk_widget_show (gray_bar);

	priv->shortcut_bar_shown = TRUE;
	priv->folder_bar_shown   = FALSE;

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (GTK_WINDOW (shell_view), DEFAULT_WIDTH, DEFAULT_HEIGHT);
}


/* GObject methods.  */

static void
hash_foreach_destroy_view (void *name,
			   void *value,
			   void *data)
{
	View *view;

	view = (View *) value;

	gtk_widget_destroy (view->control);

	view_destroy (view);
}

static void
impl_dispose (GObject *object)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	GList *p;

	shell_view = E_SHELL_VIEW (object);
	priv = shell_view->priv;

	/* This is necessary to remove the signal handler for folder_new on the
	   storage set used for the delayed selection mechanism.  */
	cleanup_delayed_selection (shell_view);

	if (priv->tooltips != NULL) {
		g_object_unref (priv->tooltips);
		priv->tooltips = NULL;
	}

	if (priv->history != NULL) {
		g_object_unref (priv->history);
		priv->history = NULL;
	}

	if (priv->shell != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->shell));
		priv->shell = NULL;
	}

	if (priv->corba_interface != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->corba_interface));
		priv->corba_interface = NULL;
	}

	if (priv->folder_bar_popup != NULL) {
		gtk_widget_destroy (priv->folder_bar_popup);
		priv->folder_bar_popup = NULL;
	}

	for (p = priv->sockets; p != NULL; p = p->next) {
		GtkWidget *socket_widget;
		int destroy_connection_id;

		socket_widget = GTK_WIDGET (p->data);
		destroy_connection_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (socket_widget),
									    "e_shell_view_destroy_connection_id"));
		g_signal_handler_disconnect((socket_widget), destroy_connection_id);
	}
	g_list_free (priv->sockets);
	priv->sockets = NULL;

	if (priv->uri_to_view != NULL) {
		g_hash_table_foreach (priv->uri_to_view, hash_foreach_destroy_view, NULL);
		g_hash_table_destroy (priv->uri_to_view);
		priv->uri_to_view = NULL;
	}

	if (priv->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->ui_component));
		priv->ui_component = NULL;
	}

	if (priv->set_folder_timeout != 0) {
		gtk_timeout_remove (priv->set_folder_timeout);
		priv->set_folder_timeout = 0;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (object);
	priv = shell_view->priv;

	g_free (priv->uri);
	g_free (priv->set_folder_uri);
	g_free (priv->delayed_selection);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (EShellViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_ref(BONOBO_TYPE_WINDOW);

	signals[SHORTCUT_BAR_VISIBILITY_CHANGED]
		= g_signal_new ("shortcut_bar_visibility_changed",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EShellViewClass, shortcut_bar_visibility_changed),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);

	signals[FOLDER_BAR_VISIBILITY_CHANGED]
		= g_signal_new ("folder_bar_visibility_changed",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EShellViewClass, folder_bar_visibility_changed),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);

	signals[VIEW_CHANGED]
		= g_signal_new ("view_changed",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EShellViewClass, view_changed),
				NULL, NULL,
				e_shell_marshal_NONE__STRING_STRING_STRING_STRING,
				G_TYPE_NONE, 4,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING);

	load_images ();
}

static void
init (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = g_new (EShellViewPrivate, 1);

	priv->shell                   = NULL;
	priv->corba_interface         = NULL;
	priv->ui_component            = NULL;
	priv->history                 = e_history_new ((EHistoryItemFreeFunc) g_free);
	priv->uri                     = NULL;
	priv->delayed_selection       = NULL;

	priv->tooltips                = gtk_tooltips_new ();
	g_object_ref (priv->tooltips);
	gtk_object_sink (GTK_OBJECT (priv->tooltips));

	priv->appbar                  = NULL;
	priv->hpaned                  = NULL;
	priv->view_hpaned             = NULL;
	priv->contents                = NULL;
	priv->notebook                = NULL;

	priv->storage_set_title_bar   = NULL;
	priv->storage_set_view        = NULL;
	priv->storage_set_view_box    = NULL;
	priv->shortcut_bar            = NULL;

	priv->status_bar              = NULL;
	priv->offline_toggle          = NULL;
	priv->offline_toggle_image    = NULL;
	priv->menu_hint_label         = NULL;
	priv->task_bar                = NULL;

	priv->folder_bar_popup        = NULL;

	priv->shortcut_bar_shown      = FALSE;
	priv->folder_bar_shown        = FALSE;

	priv->hpaned_position         = 0;
	priv->view_hpaned_position    = 0;

	priv->uri_to_view             = g_hash_table_new (g_str_hash, g_str_equal);

	priv->sockets		      = NULL;

	priv->set_folder_timeout      = 0;
	priv->set_folder_uri          = NULL;

	shell_view->priv = priv;
}


/* EvolutionShellView interface callbacks.  */

static void
corba_interface_set_message_cb (EvolutionShellView *shell_view,
				const char *message,
				gboolean busy,
				void *data)
{
	/* Don't do anything here anymore.  The interface is going to be
	   deprecated soon.  */
}

static void
corba_interface_unset_message_cb (EvolutionShellView *shell_view,
				  void *data)
{
	/* Don't do anything here anymore.  The interface is going to be
	   deprecated soon.  */
}

static void
corba_interface_change_current_view_cb (EvolutionShellView *shell_view,
					const char *uri,
					void *data)
{
	EShellView *view;

	view = E_SHELL_VIEW (data);

	g_return_if_fail (view != NULL);

	e_shell_view_display_uri (view, uri, TRUE);
}

static void
corba_interface_set_title (EvolutionShellView *shell_view,
			   const char *title,
			   void *data)
{
	EShellView *view;

	view = E_SHELL_VIEW (data);
	
	g_return_if_fail (view != NULL);

	gtk_window_set_title (GTK_WINDOW (view), title);
}

static void
corba_interface_set_folder_bar_label (EvolutionShellView *evolution_shell_view,
				      const char *text,
				      void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	g_return_if_fail (data != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (data));

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	e_shell_folder_title_bar_set_folder_bar_label (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar),
						       text);
}

static void
corba_interface_show_settings (EvolutionShellView *evolution_shell_view,
			       void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	g_return_if_fail (data != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (data));

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	e_shell_view_show_settings (shell_view);
}

static void
unmerge_on_error (BonoboObject *object,
		  CORBA_Object  cobject,
		  CORBA_Environment *ev)
{
#if 0
	BonoboWindow *window;
	BonoboUIEngine *ui_engine;

	/* FIXME changes.txt says we should be able to do this but bonobo_ui_engine_get_view()
	   is marked as internal in bonoob-ui-engine.h!  */
	ui_engine = bonobo_ui_container_get_engine (BONOBO_UI_CONTAINER (object));
	window = BONOBO_WINDOW (bonobo_ui_engine_get_view (ui_engine));

	if (window != NULL)
		bonobo_ui_engine_deregister_component_by_ref (ui_engine, cobject);
#endif
}

static void
updated_folder_cb (EStorageSet *storage_set,
		   const char *path,
		   void *data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	const char *view_path;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	view_path = get_storage_set_path_from_uri (priv->uri);
	if (view_path && strcmp (path, view_path) != 0)
		return;

	/* Update the folder title bar and the window title bar */
	update_for_current_uri (shell_view);
}


/* Shell callbacks.  */

static void
shell_line_status_changed_cb (EShell *shell,
			      EShellLineStatus new_status,
			      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);
	update_offline_toggle_status (shell_view);

	if (new_status == E_SHELL_LINE_STATUS_OFFLINE)
		bonobo_ui_component_set_prop (shell_view->priv->ui_component,
					      "/commands/SendReceive",
					      "sensitive", "0", NULL);
	else
		bonobo_ui_component_set_prop (shell_view->priv->ui_component,
					      "/commands/SendReceive",
					      "sensitive", "1", NULL);
}

static int
delete_event_cb (GtkWidget *widget,
		 GdkEventAny *ev,
		 void *data)
{
	return FALSE;
}


EShellView *
e_shell_view_construct (EShellView *shell_view,
			EShell     *shell,
			const char *uri)
{
	EShellViewPrivate *priv;
	EStorageSet *storage_set;
	EShellView *view;
	char *uri_to_load;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	priv = shell_view->priv;

	view = E_SHELL_VIEW (bonobo_window_construct (BONOBO_WINDOW (shell_view),
						      bonobo_ui_container_new (),
						      "evolution", "Ximian Evolution"));

	if (!view) {
		g_object_unref (shell_view);
		return NULL;
	}

	priv->shell = shell;
	bonobo_object_ref (BONOBO_OBJECT (priv->shell));

	g_signal_connect (view, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	priv->ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (view));
	g_signal_connect (priv->ui_container, "system_exception",
			  G_CALLBACK (unmerge_on_error), NULL);

	priv->ui_component = bonobo_ui_component_new ("evolution");
	bonobo_ui_component_set_container (priv->ui_component,
					   bonobo_object_corba_objref (BONOBO_OBJECT (priv->ui_container)),
					   NULL);

	bonobo_ui_component_freeze (priv->ui_component, NULL);

	bonobo_ui_util_set_ui (priv->ui_component, EVOLUTION_DATADIR,
			       EVOLUTION_UIDIR "/evolution.xml",
			       "evolution-1.4", NULL);

	setup_widgets (shell_view);

	bonobo_ui_engine_config_set_path (bonobo_window_get_ui_engine (BONOBO_WINDOW (shell_view)),
					  "/evolution/UIConf/kvps");
	e_shell_view_menu_setup (shell_view);

	e_shell_view_show_folder_bar (shell_view, FALSE);

	bonobo_ui_component_thaw (priv->ui_component, NULL);

	g_signal_connect_object (shell, "line_status_changed",
				 G_CALLBACK (shell_line_status_changed_cb), shell_view, 0);

	storage_set = e_shell_get_storage_set (shell);
	e_signal_connect_while_alive (storage_set, "updated_folder",
				      G_CALLBACK (updated_folder_cb), shell_view, shell_view);
	g_signal_connect_object (storage_set, "new_storage",
				 G_CALLBACK (storage_set_new_storage_callback), shell_view, 0);
	g_signal_connect_object (storage_set, "removed_storage",
				 G_CALLBACK (storage_set_removed_storage_callback), shell_view, 0);
	g_signal_connect_object (storage_set, "removed_folder",
				 G_CALLBACK (storage_set_removed_folder_callback), shell_view, 0);

	e_shell_user_creatable_items_handler_attach_menus (e_shell_get_user_creatable_items_handler (priv->shell),
							   shell_view);

	setup_defaults (view);
	update_other_users_folder_items_sensitivity (view);

	if (uri != NULL) {
		uri_to_load = g_strdup (uri);
	} else {
		GConfClient *client = gconf_client_get_default ();
		char *path = gconf_client_get_string (client, "/apps/evolution/shell/view_defaults/folder_path", NULL);

		uri_to_load = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);
		g_free (path);
		g_object_unref (client);
	}

	if (! e_shell_view_display_uri (shell_view, uri_to_load, FALSE)) {
		e_shell_view_display_uri (shell_view, E_SHELL_VIEW_DEFAULT_URI, FALSE);
		e_shell_view_display_uri (shell_view, uri_to_load, TRUE);
	}

	g_free (uri_to_load);

	return view;
}

/* WARNING: Don't use `e_shell_view_new()' to create new views for the shell
   unless you know what you are doing; this is just the standard GTK+
   constructor thing and it won't allow the shell to do the required
   bookkeeping for the created views.  Instead, the right way to create a new
   view is calling `e_shell_create_view()'.  */
EShellView *
e_shell_view_new (EShell *shell,
		  const char *uri)
{
	GtkWidget *new;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	new = g_object_new (e_shell_view_get_type (), NULL);

	return e_shell_view_construct (E_SHELL_VIEW (new), shell, uri);
}

const GNOME_Evolution_ShellView
e_shell_view_get_corba_interface (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	g_return_val_if_fail (shell_view != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), CORBA_OBJECT_NIL);

	priv = shell_view->priv;

	return bonobo_object_corba_objref (BONOBO_OBJECT (priv->corba_interface));
}


static const char *
get_storage_set_path_from_uri (const char *uri)
{
	const char *colon;

	if (uri == NULL)
		return NULL;

	if (g_path_is_absolute (uri))
		return NULL;

	colon = strchr (uri, ':');
	if (colon == NULL || colon == uri || colon[1] == '\0')
		return NULL;

	if (! g_path_is_absolute (colon + 1))
		return NULL;

	if (g_ascii_strncasecmp (uri, E_SHELL_URI_PREFIX, colon - uri) != 0)
		return NULL;

	return colon + 1;
}

static void
update_window_icon (EShellView *shell_view,
		    const char *type)
{
	EShellViewPrivate *priv;
	const char *icon_name;
	char *icon_path;

	priv = shell_view->priv;

	if (type == NULL) {
		icon_path = NULL;
	} else {
		EFolderTypeRegistry *folder_type_registry;

		folder_type_registry = e_shell_get_folder_type_registry (priv->shell);
		icon_name = e_folder_type_registry_get_icon_name_for_type (folder_type_registry, type);
		if (icon_name == NULL)
			icon_path = NULL;
		else
			icon_path = e_shell_get_icon_path (icon_name, FALSE);
	}

	if (icon_path == NULL) {
		gnome_window_icon_set_from_default (GTK_WINDOW (shell_view));
	} else {
		gnome_window_icon_set_from_file (GTK_WINDOW (shell_view), icon_path);
		g_free (icon_path);
	}
}

static void
update_folder_title_bar (EShellView *shell_view,
			 const char *title,
			 EFolder *folder)
{
	EShellViewPrivate *priv;
	GdkPixbuf *folder_icon;

	priv = shell_view->priv;

	if (folder == NULL) {
		folder_icon = NULL;
	} else {
		const char *icon_name;

		icon_name = e_folder_get_custom_icon_name (folder);
		if (icon_name != NULL) {
			folder_icon = e_icon_factory_get_icon (icon_name, TRUE);
		} else {
			EFolderTypeRegistry *folder_type_registry;

			folder_type_registry = e_shell_get_folder_type_registry (priv->shell);
			folder_icon = e_folder_type_registry_get_icon_for_type (folder_type_registry,
										e_folder_get_type_string (folder),
										TRUE);
			g_object_ref (folder_icon);
		}
	}

	e_shell_folder_title_bar_set_icon (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar),
					   folder_icon);

	if (folder_icon != NULL)
		g_object_unref (folder_icon);

	if (title != NULL)
		e_shell_folder_title_bar_set_title (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar), title);
}

static void
update_for_current_uri (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	EFolder *folder;
	const char *path;
	const char *curr_path;
	const char *type;
	const char *folder_name;
	char *title;
	char *window_title;
	int unread_count;

	priv = shell_view->priv;

	/* If we update when there is a timeout set, the selection will jump
	   around against the user's wishes.  So we just return.  */     
	if (priv->set_folder_timeout != 0)
		return;

	path = get_storage_set_path_from_uri (priv->uri);

	folder = NULL;
	folder_name = NULL;
	type = NULL;
	unread_count = 0;

	if (path != NULL) {
		folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell), path);

		if (folder != NULL) {
			folder_name = e_folder_get_name (folder);
			type = e_folder_get_type_string (folder);
			unread_count = e_folder_get_unread_count (folder);
		}
	}

	if (unread_count > 0)
		title = g_strdup_printf (_("%s (%d)"), folder_name, unread_count);
	else if (folder_name == NULL)
		title = g_strdup (_("(None)"));
	else
		title = g_strdup (folder_name);

	if (SUB_VERSION[0] == '\0')
		window_title = g_strdup_printf ("%s - Ximian Evolution %s", title, VERSION);
	else
		window_title = g_strdup_printf ("%s - Ximian Evolution %s [%s]", title, VERSION, SUB_VERSION);

	gtk_window_set_title (GTK_WINDOW (shell_view), window_title);

	update_folder_title_bar (shell_view, title, folder);
	update_window_icon (shell_view, type);

	g_free (window_title);
	g_free (title);

	g_signal_handlers_block_by_func (priv->storage_set_view,
					 G_CALLBACK (folder_selected_cb), shell_view);

	curr_path = e_storage_set_view_get_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view));
	if (path != NULL && (curr_path == NULL || strcmp(path, curr_path)))
		e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view), path);

	g_signal_handlers_unblock_by_func (priv->storage_set_view,
					   G_CALLBACK (folder_selected_cb), shell_view);
}

static void
update_offline_toggle_status (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	GdkPixmap *icon_pixmap;
	GdkBitmap *icon_mask;
	const char *tooltip;
	gboolean sensitive;

	priv = shell_view->priv;

	switch (e_shell_get_line_status (priv->shell)) {
	case E_SHELL_LINE_STATUS_ONLINE:
		icon_pixmap = online_pixmap;
		icon_mask   = online_mask;
		sensitive   = TRUE;
		tooltip     = _("Ximian Evolution is currently online.  "
				"Click on this button to work offline.");
		break;
	case E_SHELL_LINE_STATUS_GOING_OFFLINE:
		icon_pixmap = online_pixmap;
		icon_mask   = online_mask;
		sensitive   = FALSE;
		tooltip     = _("Ximian Evolution is in the process of going offline.");
		break;
	case E_SHELL_LINE_STATUS_OFFLINE:
		icon_pixmap = offline_pixmap;
		icon_mask   = offline_mask;
		sensitive   = TRUE;
		tooltip     = _("Ximian Evolution is currently offline.  "
				"Click on this button to work online.");
		break;
	default:
		g_assert_not_reached ();
		return;
	}

	gtk_image_set_from_pixmap (GTK_IMAGE (priv->offline_toggle_image), icon_pixmap, icon_mask);
	gtk_widget_set_sensitive (priv->offline_toggle, sensitive);
	gtk_tooltips_set_tip (priv->tooltips, priv->offline_toggle, tooltip, NULL);
}

/* This displays the specified page, doing the appropriate Bonobo activation/deactivation
   magic to make sure things work nicely.  FIXME: Crappy way to solve the issue.  */
static void
set_current_notebook_page (EShellView *shell_view,
			   int page_num)
{
	EShellViewPrivate *priv;
	GtkNotebook *notebook;
	GtkWidget *current;
	BonoboControlFrame *old_control_frame = NULL;
	BonoboControlFrame *new_control_frame;
	int current_page;

	priv = shell_view->priv;
	notebook = GTK_NOTEBOOK (priv->notebook);

	current_page = gtk_notebook_get_current_page (notebook);
	if (current_page == page_num)
		return;

	if (current_page != -1 && current_page != 0) {
		current = gtk_notebook_get_nth_page (notebook, current_page);
		old_control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (current));
		bonobo_control_frame_set_autoactivate (old_control_frame, FALSE);
	}

	e_shell_folder_title_bar_set_folder_bar_label  (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar), "");
	gtk_notebook_set_current_page (notebook, page_num);

	if (page_num == -1 || page_num == 0)
		return;

	current = gtk_notebook_get_nth_page (notebook, page_num);
	new_control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (current));
	bonobo_control_frame_set_autoactivate (new_control_frame, FALSE);
 
	bonobo_control_frame_control_activate (new_control_frame);
	if (old_control_frame)
		bonobo_control_frame_control_deactivate (old_control_frame);
}

static void
setup_corba_interface (EShellView *shell_view,
		       GtkWidget *control)
{
	EShellViewPrivate *priv;
	BonoboControlFrame *control_frame;
	EvolutionShellView *corba_interface;

	g_return_if_fail (control != NULL);

	priv = shell_view->priv;

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (control));
	corba_interface = evolution_shell_view_new ();

	g_signal_connect_object (corba_interface, "set_message",
				 G_CALLBACK (corba_interface_set_message_cb), shell_view, 0);
	g_signal_connect_object (corba_interface, "unset_message",
				 G_CALLBACK (corba_interface_unset_message_cb), shell_view, 0);
	g_signal_connect_object (corba_interface, "change_current_view",
				 G_CALLBACK (corba_interface_change_current_view_cb), shell_view, 0);
	g_signal_connect_object (corba_interface, "set_title",
				 G_CALLBACK (corba_interface_set_title), shell_view, 0);
	g_signal_connect_object (corba_interface, "set_folder_bar_label",
				 G_CALLBACK (corba_interface_set_folder_bar_label), shell_view, 0);
	g_signal_connect_object (corba_interface, "show_settings",
				 G_CALLBACK (corba_interface_show_settings), shell_view, 0);

	bonobo_object_add_interface (BONOBO_OBJECT (control_frame),
				     BONOBO_OBJECT (corba_interface));

	bonobo_object_ref (BONOBO_OBJECT (corba_interface));
	priv->corba_interface = corba_interface;
}


/* Socket destruction handling.  */

static void
socket_destroy_cb (GtkWidget *socket_widget, gpointer data)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;
	EFolder *folder;
	View *view;
	const char *uri;
	gboolean viewing_closed_uri;
	const char *current_uri;
	const char *path;
	const char *folder_type;

	shell_view = E_SHELL_VIEW (data);
	priv = shell_view->priv;

	uri = (const char *) g_object_get_data (G_OBJECT (socket_widget), "e_shell_view_folder_uri");

	view = g_hash_table_lookup (priv->uri_to_view, uri);
	if (view == NULL) {
		g_warning ("What?! Destroyed socket for non-existing URI?  -- %s", uri);
		return;
	}

	priv->sockets = g_list_remove (priv->sockets, socket_widget);

	gtk_widget_destroy (view->control);

	g_hash_table_remove (priv->uri_to_view, view->uri);
	view_destroy (view);

	path = get_storage_set_path_from_uri (uri);
	folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell), path);

	if (folder != NULL)
		folder_type = e_folder_get_type_string (folder);
	else
		folder_type = NULL;

	/* See if we were actively viewing the uri for the socket that's being closed */
	current_uri = e_shell_view_get_current_uri (shell_view);
	if (current_uri == NULL) {
		viewing_closed_uri = FALSE;
	} else {
		if (strcmp (uri, current_uri) == 0)
			viewing_closed_uri = TRUE;
		else
			viewing_closed_uri = FALSE;
	}

	if (viewing_closed_uri)
		e_shell_view_display_uri (shell_view, NULL, TRUE);

	e_shell_component_maybe_crashed (priv->shell, uri, folder_type, shell_view);

	/* We were actively viewing the component that just crashed, so flip to the default URI */
	if (viewing_closed_uri)
		e_shell_view_display_uri (shell_view, E_SHELL_VIEW_DEFAULT_URI, TRUE);
}


static const char *
get_type_for_folder (EShellView *shell_view,
		     const char *path,
		     const char **physical_uri_return)
{
	EShellViewPrivate *priv;
	EStorageSet *storage_set;
	EFolder *folder;

	priv = shell_view->priv;

	storage_set = e_shell_get_storage_set (priv->shell);
	folder = e_storage_set_get_folder (storage_set, path);
	if (!folder)
		return NULL;

	if (physical_uri_return != NULL)
		*physical_uri_return = e_folder_get_physical_uri (folder);

	return e_folder_get_type_string (folder);
}

/* Create a new view for @uri with @control.  It assumes a view for @uri does not exist yet.  */
static View *
get_view_for_uri (EShellView *shell_view,
		  const char *uri,
		  const char *view_info)
{
	EShellViewPrivate *priv;
	CORBA_Environment ev;
	EvolutionShellComponentClient *handler_client;
	EFolderTypeRegistry *folder_type_registry;
	GNOME_Evolution_ShellComponent handler;
	Bonobo_UIContainer container;
	GtkWidget *control;
	GtkWidget *socket;
	Bonobo_Control corba_control;
	const char *path;
	const char *physical_uri;
	const char *folder_type;
	int destroy_connection_id;

	priv = shell_view->priv;

	path = strchr (uri, ':');
	if (path == NULL)
		return NULL;

	path++;
	if (*path == '\0')
		return NULL;

	folder_type = get_type_for_folder (shell_view, path, &physical_uri);
	if (folder_type == NULL || physical_uri == NULL)
		return NULL;

	folder_type_registry = e_shell_get_folder_type_registry (e_shell_view_get_shell (shell_view));

	handler_client = e_folder_type_registry_get_handler_for_type (folder_type_registry, folder_type);
	if (handler_client == CORBA_OBJECT_NIL)
		return NULL;

	handler = evolution_shell_component_client_corba_objref (handler_client);

	CORBA_exception_init (&ev);

	corba_control = GNOME_Evolution_ShellComponent_createView (handler, physical_uri, folder_type, view_info, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	if (corba_control == CORBA_OBJECT_NIL)
		return NULL;

	container = bonobo_ui_component_get_container (priv->ui_component);
	control = bonobo_widget_new_control_from_objref (corba_control, container);

	socket = find_socket (GTK_CONTAINER (control));
	destroy_connection_id = g_signal_connect (socket, "destroy",
						  G_CALLBACK (socket_destroy_cb),
						  shell_view);
	g_object_set_data (G_OBJECT (socket), "e_shell_view_destroy_connection_id", GINT_TO_POINTER (destroy_connection_id));
	g_object_set_data_full (G_OBJECT (socket), "e_shell_view_folder_uri", g_strdup (uri), g_free);

	priv->sockets = g_list_prepend (priv->sockets, socket);

	setup_corba_interface (shell_view, control);

	return view_new (uri, control);
}

static gboolean
show_existing_view (EShellView *shell_view,
		    const char *uri,
		    View *view)
{
	EShellViewPrivate *priv;
	int notebook_page;

	priv = shell_view->priv;

	notebook_page = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), view->control);
	g_assert (notebook_page != -1);

	g_free (priv->uri);
	priv->uri = g_strdup (uri);

	set_current_notebook_page (shell_view, notebook_page);

	return TRUE;
}

static gboolean
create_new_view_for_uri (EShellView *shell_view,
			 const char *uri,
			 const char *view_info)
{
	View *view;
	EShellViewPrivate *priv;
	int page_num;

	priv = shell_view->priv;

	view = get_view_for_uri (shell_view, uri, view_info);
	if (view == NULL)
		return FALSE;

	g_free (priv->uri);
	priv->uri = g_strdup (uri);

	gtk_widget_show (view->control);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), view->control, NULL);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), view->control);
	g_assert (page_num != -1);
	set_current_notebook_page (shell_view, page_num);

	g_hash_table_insert (priv->uri_to_view, view->uri, view);

	return TRUE;
}

static char *
evolution_uri_for_default_uri (EShell *shell,
			       const char *default_uri)
{
	char *uri;
	char *path;
	char *extra;

	if (! e_shell_parse_uri (shell, default_uri, &path, &extra))
		return NULL;

	uri = g_strconcat (E_SHELL_URI_PREFIX, path, "#", extra, NULL);

	g_free (path);
	g_free (extra);

	return uri;
}

static gboolean
display_uri (EShellView *shell_view,
	     const char *uri,
	     gboolean add_to_history,
	     gboolean queue)
{
	EShellViewPrivate *priv;
	View *view;
	gboolean retval;
	const char *view_info;
	char *real_uri;
	char *allocated_uri = NULL;

	priv = shell_view->priv;

	if (uri == NULL && priv->uri == NULL)
		return TRUE;

	if (priv->uri != NULL && uri != NULL && strcmp (priv->uri, uri) == 0)
		return TRUE;

	bonobo_ui_engine_freeze (bonobo_window_get_ui_engine (BONOBO_WINDOW (shell_view)));

	if (uri == NULL) {
		gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), 0);
		gtk_notebook_prepend_page (GTK_NOTEBOOK (priv->notebook), create_label_for_empty_page (), NULL);

		set_current_notebook_page (shell_view, 0);

		g_free (priv->uri);
		priv->uri = real_uri = NULL;

		retval = TRUE;
		goto end;
	}

	if (strncmp (uri, E_SHELL_DEFAULTURI_PREFIX, E_SHELL_DEFAULTURI_PREFIX_LEN) == 0) {
		allocated_uri = evolution_uri_for_default_uri (e_shell_view_get_shell (shell_view), uri);
		uri = allocated_uri;
	}

	view_info = strchr (uri, '#');
	if (view_info) {
		real_uri = g_strndup (uri, view_info - uri);
		view_info++;
	} else {
		view_info = "";
		real_uri = g_strdup (uri);
	}

	if (strncmp (uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) != 0) {
		retval = FALSE;
		goto end;
	}

	view = g_hash_table_lookup (priv->uri_to_view, real_uri);
	if (view != NULL) {
		show_existing_view (shell_view, real_uri, view);
	} else if (! create_new_view_for_uri (shell_view, real_uri, view_info)) {
		if (! queue) {
			retval = FALSE;
			goto end;
		}

		cleanup_delayed_selection (shell_view);
		priv->delayed_selection = g_strdup (real_uri);
		g_signal_connect_after (e_shell_get_storage_set (priv->shell),
					"new_folder", G_CALLBACK (new_folder_cb), shell_view);
		retval = TRUE;
		goto end;
	}

	retval = TRUE;

 end:
	g_free (real_uri);

	if (add_to_history && retval == TRUE && priv->uri != NULL)
		e_history_add (priv->history, g_strdup (priv->uri));

	update_navigation_buttons (shell_view);

	g_free (priv->set_folder_uri);
	priv->set_folder_uri = NULL;

	if (priv->set_folder_timeout != 0) {
		gtk_timeout_remove (priv->set_folder_timeout);
		priv->set_folder_timeout = 0;
	}

	update_for_current_uri (shell_view);

	bonobo_ui_engine_thaw (bonobo_window_get_ui_engine (BONOBO_WINDOW (shell_view)));

	g_signal_emit (shell_view, signals[VIEW_CHANGED], 0,
		       e_shell_view_get_current_path (shell_view),
		       e_shell_view_get_current_uri (shell_view),
		       e_shell_view_get_current_folder_type (shell_view),
		       e_shell_view_get_current_component_id (shell_view));

	g_free (allocated_uri);

	return retval;
}

gboolean
e_shell_view_display_uri (EShellView *shell_view,
			  const char *uri,
			  gboolean queue)
{
	g_return_val_if_fail (shell_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return display_uri (shell_view, uri, TRUE, queue);
}


void
e_shell_view_show_shortcut_bar (EShellView *shell_view,
				gboolean show)
{
	EShellViewPrivate *priv;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = shell_view->priv;

	if (!! show == priv->shortcut_bar_shown)
		return;

	if (show) {
		if (! GTK_WIDGET_VISIBLE (priv->shortcut_frame)) {
			gtk_widget_show (priv->shortcut_frame);
			gtk_paned_set_position (GTK_PANED (priv->hpaned), priv->hpaned_position);
		}
	} else {
		if (GTK_WIDGET_VISIBLE (priv->shortcut_frame)) {
			/* FIXME this is a private field!  */
			priv->hpaned_position = GTK_PANED (priv->hpaned)->child1_size;

			gtk_widget_hide (priv->shortcut_frame);
		}
		gtk_paned_set_position (GTK_PANED (priv->hpaned), 0);
	}

	priv->shortcut_bar_shown = !! show;

	g_signal_emit (shell_view, signals[SHORTCUT_BAR_VISIBILITY_CHANGED], 0,
		       priv->shortcut_bar_shown);
}

void
e_shell_view_show_folder_bar (EShellView *shell_view,
			      gboolean show)
{
	EShellViewPrivate *priv;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = shell_view->priv;

	if (!! show == priv->folder_bar_shown)
		return;

	if (show) {
		gtk_widget_show (priv->storage_set_view_box);
		gtk_paned_set_position (GTK_PANED (priv->view_hpaned), priv->view_hpaned_position);

		e_title_bar_set_button_mode (E_TITLE_BAR (priv->storage_set_title_bar),
					     E_TITLE_BAR_BUTTON_MODE_CLOSE);

		e_shell_folder_title_bar_set_title_clickable (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar),
							      FALSE);
	} else {
		if (GTK_WIDGET_VISIBLE (priv->storage_set_view_box)) {
			/* FIXME this is a private field!  */
			priv->view_hpaned_position = GTK_PANED (priv->view_hpaned)->child1_size;
			gtk_widget_hide (priv->storage_set_view_box);
		}

		gtk_paned_set_position (GTK_PANED (priv->view_hpaned), 0);

		e_title_bar_set_button_mode (E_TITLE_BAR (priv->storage_set_title_bar),
					     E_TITLE_BAR_BUTTON_MODE_PIN);

		e_shell_folder_title_bar_set_title_clickable (E_SHELL_FOLDER_TITLE_BAR (priv->folder_title_bar),
							      TRUE);
	}

        priv->folder_bar_shown = !! show;

	g_signal_emit (shell_view, signals[FOLDER_BAR_VISIBILITY_CHANGED], 0,
		       priv->folder_bar_shown);
}

void
e_shell_view_show_settings (EShellView *shell_view)
{
	EShellViewPrivate *priv;	
	const char *type;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	
	priv = shell_view->priv;
	
	type = e_shell_view_get_current_folder_type (shell_view);
	e_shell_show_settings (priv->shell, type, shell_view);
}

gboolean
e_shell_view_shortcut_bar_shown (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->shortcut_bar_shown;
}

gboolean
e_shell_view_folder_bar_shown (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->folder_bar_shown;
}


ETaskBar *
e_shell_view_get_task_bar (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_TASK_BAR (shell_view->priv->task_bar);
}

EShell *
e_shell_view_get_shell (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->shell;
}

BonoboUIComponent *
e_shell_view_get_bonobo_ui_component (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->ui_component;
}

BonoboUIContainer *
e_shell_view_get_bonobo_ui_container (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->ui_container;
}

GtkWidget *
e_shell_view_get_appbar (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->appbar;
}

/**
 * e_shell_view_get_current_uri:
 * @shell_view: A pointer to an EShellView object
 * 
 * Get the URI currently displayed by this shell view.
 * 
 * Return value: 
 **/
const char *
e_shell_view_get_current_uri (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->uri;
}

/**
 * e_shell_view_get_current_path:
 * @shell_view: A pointer to an EShellView object
 * 
 * Get the path of the current displayed folder.
 * 
 * Return value: 
 **/
const char *
e_shell_view_get_current_path (EShellView *shell_view)
{
	const char *current_uri;
	const char *current_path;

	current_uri = e_shell_view_get_current_uri (shell_view);
	if (current_uri == NULL)
		return NULL;

	if (strncmp (current_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0)
		current_path = current_uri + E_SHELL_URI_PREFIX_LEN;
	else
		current_path = NULL;

	return current_path;
}

const char *
e_shell_view_get_current_physical_uri (EShellView *shell_view)
{
	const char *current_path;
	const char *physical_uri;

	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	current_path = e_shell_view_get_current_path (shell_view);
	if (current_path == NULL)
		return NULL;

	if (get_type_for_folder (shell_view, current_path, &physical_uri) == NULL)
		return NULL;
	else
		return physical_uri;
}

const char *
e_shell_view_get_current_folder_type (EShellView *shell_view)
{
	const char *current_path;

	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	current_path = e_shell_view_get_current_path (shell_view);
	if (current_path == NULL)
		return NULL;

	return get_type_for_folder (shell_view, current_path, NULL);
}

const char *
e_shell_view_get_current_component_id (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	EFolderTypeRegistry *type_registry;
	EvolutionShellComponentClient *component_client;
	const char *current_folder_type;

	priv = shell_view->priv;

	type_registry = e_shell_get_folder_type_registry (priv->shell);

	current_folder_type = e_shell_view_get_current_folder_type (shell_view);
	if (current_folder_type == NULL)
		return NULL;

	component_client = e_folder_type_registry_get_handler_for_type (type_registry, current_folder_type);
	if (component_client == NULL)
		return NULL;

	return evolution_shell_component_client_get_id (component_client);
}


/**
 * e_shell_view_save_defaults:
 * @shell_view: 
 **/
void
e_shell_view_save_defaults (EShellView *shell_view)
{
	GConfClient *client;
	EShellViewPrivate *priv;
	EShortcutBar *shortcut_bar;
	const char *uri;
	char *file_name;
	struct stat temp;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = shell_view->priv;
	shortcut_bar = E_SHORTCUT_BAR (priv->shortcut_bar);

	client = gconf_client_get_default ();

	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/width",
			      GTK_WIDGET (shell_view)->allocation.width, NULL);
	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/height",
			      GTK_WIDGET (shell_view)->allocation.height, NULL);

	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/selected_shortcut_group",
			      e_shell_view_get_current_shortcuts_group_num (shell_view), NULL);

	gconf_client_set_bool (client, "/apps/evolution/shell/view_defaults/show_folder_bar",
			       e_shell_view_folder_bar_shown (shell_view), NULL);
	gconf_client_set_bool (client, "/apps/evolution/shell/view_defaults/show_shortcut_bar",
			       e_shell_view_shortcut_bar_shown (shell_view), NULL);

	if (priv->shortcut_bar_shown)
		gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/shortcut_bar/width",
				      GTK_PANED (priv->hpaned)->child1_size, NULL); 
	else
		gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/shortcut_bar/width",
				      priv->hpaned_position, NULL);

	if (priv->folder_bar_shown)
		gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/folder_bar/width",
				      GTK_PANED (priv->view_hpaned)->child1_size, NULL);
	else
		gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/folder_bar/width",
				      priv->view_hpaned_position, NULL);

	uri = e_shell_view_get_current_uri (shell_view);
	if (uri != NULL)
		gconf_client_set_string (client, "/apps/evolution/shell/view_defaults/folder_path",
					 uri + E_SHELL_URI_PREFIX_LEN, NULL);
	else
		gconf_client_unset (client, "/apps/evolution/shell/view_defaults/folder_path", NULL);

	/* If ~/evolution/config/ doesn't exist yet, make it */
	file_name = g_strdup_printf ("%s/config/", e_shell_get_local_directory (priv->shell));
	if (stat (file_name, &temp) != 0)
		mkdir (file_name, S_IRWXU);
	g_free (file_name);

	/* Save the expanded state for the StorageSetView.  */

	file_name = g_strdup_printf ("%s/config/storage-set-view-expanded:default",
				     e_shell_get_local_directory (priv->shell));
	e_tree_save_expanded_state (E_TREE (priv->storage_set_view),
				    file_name);
	g_free (file_name);

	g_object_unref (client);
}


/* FIXME: This function could become static */
void
e_shell_view_set_current_shortcuts_group_num (EShellView *shell_view, int group_num)
{
	EShellViewPrivate *priv;
	EShortcutsView *shortcuts_view;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = shell_view->priv;

	shortcuts_view = E_SHORTCUTS_VIEW (priv->shortcut_bar);

	e_group_bar_set_current_group_num (E_GROUP_BAR (E_SHORTCUT_BAR (shortcuts_view)), group_num, FALSE);
}

int
e_shell_view_get_current_shortcuts_group_num (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	EShortcutsView *shortcuts_view;
	int group;

	g_return_val_if_fail (shell_view != NULL, -1);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), -1);

	priv = shell_view->priv;

	shortcuts_view = E_SHORTCUTS_VIEW (priv->shortcut_bar);

	group = e_group_bar_get_current_group_num (E_GROUP_BAR (E_SHORTCUT_BAR (shortcuts_view)));

	return group;
}


const char *
e_shell_view_get_folder_bar_right_click_path (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	priv = shell_view->priv;

	return e_storage_set_view_get_right_click_path (E_STORAGE_SET_VIEW (priv->storage_set_view));
}


E_MAKE_TYPE (e_shell_view, "EShellView", EShellView, class_init, init, BONOBO_TYPE_WINDOW)
