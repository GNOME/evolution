/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 *   Ettore Perazzoli <ettore@helixcode.com>
 *   Miguel de Icaza <miguel@helixcode.com>
 *   Matt Loper <matt@helixcode.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <bonobo.h>
#include <libgnomeui/gnome-window-icon.h>

#include "widgets/misc/e-clipped-label.h"
#include "e-util/e-util.h"

#include "e-shell-constants.h"
#include "e-shell-folder-title-bar.h"
#include "e-shell-utils.h"
#include "e-shell.h"
#include "e-shortcuts-view.h"
#include "e-storage-set-view.h"
#include "e-title-bar.h"

#include "e-shell-view.h"
#include "e-shell-view-menu.h"

#include <widgets/e-paned/e-hpaned.h>


#define PARENT_TYPE gnome_app_get_type () /* Losing GnomeApp does not define GNOME_TYPE_APP.  */
static GnomeAppClass *parent_class = NULL;

struct _EShellViewPrivate {
	/* The shell.  */
	EShell *shell;

	/* The UI handler.  */
	BonoboUIHandler *uih;

	/* Currently displayed URI.  */
	char *uri;

	/* The widgetry.  */
	GtkWidget *appbar;
	GtkWidget *hpaned;
	GtkWidget *view_vbox;
	GtkWidget *view_title_bar;
	GtkWidget *view_hpaned;
	GtkWidget *contents;
	GtkWidget *notebook;
	GtkWidget *shortcut_bar;
	GtkWidget *storage_set_view;
	GtkWidget *storage_set_view_box;

	/* The view we have already open.  */
	GHashTable *uri_to_control;

	/* Position of the handles in the paneds, to be restored when we show elements
           after hiding them.  */
	unsigned int hpaned_position;
	unsigned int view_hpaned_position;

	/* Status of the shortcut and folder bars.  */
	EShellViewSubwindowMode shortcut_bar_mode;
	EShellViewSubwindowMode folder_bar_mode;
};

enum {
	SHORTCUT_BAR_MODE_CHANGED,
	FOLDER_BAR_MODE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_SHORTCUT_BAR_WIDTH 100
#define DEFAULT_TREE_WIDTH         130

#define DEFAULT_WIDTH 705
#define DEFAULT_HEIGHT 550


/* Utility functions.  */

static GtkWidget *
create_label_for_empty_page (void)
{
	GtkWidget *label;

	label = e_clipped_label_new (_("(No folder displayed)"));
	gtk_widget_show (label);

	return label;
}

/* FIXME this is broken.  */
static gboolean
bonobo_widget_is_dead (BonoboWidget *bonobo_widget)
{
	BonoboControlFrame *control_frame;
	CORBA_Object corba_object;
	CORBA_Environment ev;
	gboolean is_dead;

	control_frame = bonobo_widget_get_control_frame (bonobo_widget);
	corba_object = bonobo_control_frame_get_control (control_frame);

	CORBA_exception_init (&ev);
	is_dead = CORBA_Object_non_existent (corba_object, &ev);
	CORBA_exception_free (&ev);

	return is_dead;
}


/* Callbacks.  */

/* Callback called when an icon on the shortcut bar gets clicked.  */
static void
activate_shortcut_cb (EShortcutsView *shortcut_view,
		      EShortcuts *shortcuts,
		      const char *uri,
		      void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	e_shell_view_display_uri (shell_view, uri);
}

/* Callback called when a folder on the tree view gets clicked.  */
static void
folder_selected_cb (EStorageSetView *storage_set_view,
		    const char *path,
		    void *data)
{
	EShellView *shell_view;
	char *uri;

	shell_view = E_SHELL_VIEW (data);

	uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);
	e_shell_view_display_uri (shell_view, uri);
	g_free (uri);
}

/* Callback called when the close button on the tree's title bar is clicked.  */
static void
storage_set_view_close_button_clicked_cb (ETitleBar *title_bar,
					  void *data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	e_shell_view_set_folder_bar_mode (shell_view, E_SHELL_VIEW_SUBWINDOW_HIDDEN);
}


/* Widget setup.  */

static void
setup_storage_set_subwindow (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	GtkWidget *storage_set_view;
	GtkWidget *title_bar;
	GtkWidget *vbox;
	GtkWidget *scrolled_window;

	priv = shell_view->priv;

	storage_set_view = e_storage_set_view_new (e_shell_get_storage_set (priv->shell));
	gtk_signal_connect (GTK_OBJECT (storage_set_view), "folder_selected",
			    GTK_SIGNAL_FUNC (folder_selected_cb), shell_view);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scrolled_window), storage_set_view);

	vbox = gtk_vbox_new (FALSE, 0);
	title_bar = e_title_bar_new (_("Folders"));

	gtk_box_pack_start (GTK_BOX (vbox), title_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

	gtk_signal_connect (GTK_OBJECT (title_bar), "close_button_clicked",
			    GTK_SIGNAL_FUNC (storage_set_view_close_button_clicked_cb), shell_view);

	gtk_widget_show (vbox);
	gtk_widget_show (storage_set_view);
	gtk_widget_show (title_bar);
	gtk_widget_show (scrolled_window);

	priv->storage_set_view_box = vbox;
	priv->storage_set_view = storage_set_view;
}

static void
setup_widgets (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	/* The application bar.  */

	priv->appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
	gnome_app_set_statusbar (GNOME_APP (shell_view), priv->appbar);

	/* The shortcut bar.  */

	priv->shortcut_bar = e_shortcuts_new_view (e_shell_get_shortcuts (priv->shell));
	gtk_signal_connect (GTK_OBJECT (priv->shortcut_bar), "activate_shortcut",
			    GTK_SIGNAL_FUNC (activate_shortcut_cb), shell_view);

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
	gtk_container_set_border_width (GTK_CONTAINER (priv->view_vbox), 2);

	priv->view_title_bar = e_shell_folder_title_bar_new ();

	priv->view_hpaned = e_hpaned_new ();
	e_paned_add1 (E_PANED (priv->view_hpaned), priv->storage_set_view_box);
	e_paned_add2 (E_PANED (priv->view_hpaned), priv->notebook);
	e_paned_set_position (E_PANED (priv->view_hpaned), DEFAULT_TREE_WIDTH);

	gtk_box_pack_start (GTK_BOX (priv->view_vbox), priv->view_title_bar,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (priv->view_vbox), priv->view_hpaned,
			    TRUE, TRUE, 2);

	priv->hpaned = e_hpaned_new ();
	e_paned_add1 (E_PANED (priv->hpaned), priv->shortcut_bar);
	e_paned_add2 (E_PANED (priv->hpaned), priv->view_vbox);
	e_paned_set_position (E_PANED (priv->hpaned), DEFAULT_SHORTCUT_BAR_WIDTH);

	gnome_app_set_contents (GNOME_APP (shell_view), priv->hpaned);

	/* Show stuff.  */

	gtk_widget_show (priv->shortcut_bar);
	gtk_widget_show (priv->storage_set_view);
	gtk_widget_show (priv->storage_set_view_box);
	gtk_widget_show (priv->notebook);
	gtk_widget_show (priv->hpaned);
	gtk_widget_show (priv->view_hpaned);
	gtk_widget_show (priv->view_vbox);
	gtk_widget_show (priv->view_title_bar);

	/* By default, both the folder bar and shortcut bar are visible.  */
	priv->shortcut_bar_mode = E_SHELL_VIEW_SUBWINDOW_STICKY;
	priv->folder_bar_mode   = E_SHELL_VIEW_SUBWINDOW_STICKY;

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (GTK_WINDOW (shell_view), DEFAULT_WIDTH, DEFAULT_HEIGHT);
}


/* BonoboUIHandler setup.  */

static void
setup_bonobo_ui_handler (EShellView *shell_view)
{
	BonoboUIHandler *uih;
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	uih = bonobo_ui_handler_new ();

	bonobo_ui_handler_set_app (uih, GNOME_APP (shell_view));
	bonobo_ui_handler_create_menubar (uih);
	bonobo_ui_handler_set_statusbar (uih, priv->appbar);

	priv->uih = uih;
}


/* GtkObject methods.  */

static void
hash_forall_destroy_control (void *name,
			     void *value,
			     void *data)
{
	BonoboWidget *bonobo_widget;

	bonobo_widget = BONOBO_WIDGET (value);
	gtk_widget_destroy (GTK_WIDGET (bonobo_widget));

	g_free (name);
}

static void
destroy (GtkObject *object)
{
	EShellView *shell_view;
	EShellViewPrivate *priv;

	shell_view = E_SHELL_VIEW (object);
	priv = shell_view->priv;

	g_hash_table_foreach (priv->uri_to_control, hash_forall_destroy_control, NULL);
	g_hash_table_destroy (priv->uri_to_control);
	
	g_free (priv->uri);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Initialization.  */

static void
class_init (EShellViewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gnome_app_get_type ());

	signals[SHORTCUT_BAR_MODE_CHANGED]
		= gtk_signal_new ("shortcut_bar_mode_changed",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShellViewClass, shortcut_bar_mode_changed),
				  gtk_marshal_NONE__INT,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_INT);

	signals[FOLDER_BAR_MODE_CHANGED]
		= gtk_signal_new ("folder_bar_mode_changed",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShellViewClass, folder_bar_mode_changed),
				  gtk_marshal_NONE__INT,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = g_new (EShellViewPrivate, 1);

	priv->shell                = NULL;
	priv->uih                  = NULL;
	priv->uri                  = NULL;

	priv->appbar               = NULL;
	priv->hpaned               = NULL;
	priv->view_hpaned          = NULL;
	priv->contents             = NULL;
	priv->notebook             = NULL;
	priv->storage_set_view     = NULL;
	priv->storage_set_view_box = NULL;
	priv->shortcut_bar         = NULL;

	priv->shortcut_bar_mode    = E_SHELL_VIEW_SUBWINDOW_HIDDEN;
	priv->folder_bar_mode      = E_SHELL_VIEW_SUBWINDOW_HIDDEN;

	priv->hpaned_position      = 0;
	priv->view_hpaned_position = 0;

	priv->uri_to_control = g_hash_table_new (g_str_hash, g_str_equal);

	shell_view->priv = priv;
}


void
e_shell_view_construct (EShellView *shell_view,
			EShell *shell)
{
	EShellViewPrivate *priv;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	priv = shell_view->priv;

	gnome_app_construct (GNOME_APP (shell_view), "evolution", "Evolution");

	gtk_object_ref (GTK_OBJECT (shell));
	priv->shell = shell;

	setup_widgets (shell_view);
	setup_bonobo_ui_handler (shell_view);

	e_shell_view_menu_setup (shell_view);

	e_shell_view_set_folder_bar_mode (shell_view, E_SHELL_VIEW_SUBWINDOW_HIDDEN);
}

GtkWidget *
e_shell_view_new (EShell *shell)
{
	GtkWidget *new;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	new = gtk_type_new (e_shell_view_get_type ());
	e_shell_view_construct (E_SHELL_VIEW (new), shell);

	return new;
}


static const char *
get_storage_set_path_from_uri (const char *uri)
{
	const char *colon;

	if (g_path_is_absolute (uri))
		return NULL;

	colon = strchr (uri, ':');
	if (colon == NULL || colon == uri || colon[1] == '\0')
		return NULL;

	if (! g_path_is_absolute (colon + 1))
		return NULL;

	if (g_strncasecmp (uri, E_SHELL_URI_PREFIX, colon - uri) != 0)
		return NULL;

	return colon + 1;
}

static void
update_window_icon (EShellView *shell_view,
		    EFolder *folder)
{
	EShellViewPrivate *priv;
	const char *type;
	const char *icon_name;
	char *icon_path;

	priv = shell_view->priv;

	if (folder == NULL)
		type = NULL;
	else
		type = e_folder_get_type_string (folder);

	if (type == NULL) {
		icon_path = NULL;
	} else {
		EFolderTypeRegistry *folder_type_registry;

		folder_type_registry = e_shell_get_folder_type_registry (priv->shell);
		icon_name = e_folder_type_registry_get_icon_name_for_type (folder_type_registry, type);
		if (icon_name == NULL)
			icon_path = NULL;
		else
			icon_path = e_shell_get_icon_path (icon_name, TRUE);
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
			 EFolder *folder)
{
	EShellViewPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;
	GdkPixbuf *folder_icon;
	const char *folder_name;
	const char *folder_type_name;

	priv = shell_view->priv;

	if (folder == NULL)
		folder_type_name = NULL;
	else
		folder_type_name = e_folder_get_type_string (folder);

	if (folder_type_name == NULL) {
		folder_name = NULL;
		folder_icon = NULL;
	} else {
		folder_type_registry = e_shell_get_folder_type_registry (priv->shell);
		folder_icon = e_folder_type_registry_get_icon_for_type (folder_type_registry,
									folder_type_name,
									TRUE);
		folder_name = e_folder_get_name (folder);
	}

	e_shell_folder_title_bar_set_icon (E_SHELL_FOLDER_TITLE_BAR (priv->view_title_bar), folder_icon);
	e_shell_folder_title_bar_set_title (E_SHELL_FOLDER_TITLE_BAR (priv->view_title_bar), folder_name);
}

static void
update_for_current_uri (EShellView *shell_view)
{
	EShellViewPrivate *priv;
	EFolder *folder;
	const char *folder_name;
	const char *path;
	char *window_title;

	priv = shell_view->priv;

	path = get_storage_set_path_from_uri (priv->uri);

	if (path == NULL)
		folder = NULL;
	else
		folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell),
						   path);

	if (folder == NULL)
		folder_name = _("None");
	else
		folder_name = e_folder_get_name (folder);

	window_title = g_strdup_printf (_("Evolution - %s"), folder_name);
	gtk_window_set_title (GTK_WINDOW (shell_view), window_title);
	g_free (window_title);

	update_folder_title_bar (shell_view, folder);

	update_window_icon (shell_view, folder);

	gtk_signal_handler_block_by_func (GTK_OBJECT (priv->storage_set_view),
					  GTK_SIGNAL_FUNC (folder_selected_cb),
					  shell_view);
	e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (priv->storage_set_view),
					       path);
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (priv->storage_set_view),
					    GTK_SIGNAL_FUNC (folder_selected_cb),
					    shell_view);
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
	BonoboControlFrame *control_frame;
	int current_page;

	priv = shell_view->priv;
	notebook = GTK_NOTEBOOK (priv->notebook);

	current_page = gtk_notebook_get_current_page (notebook);
	if (current_page == page_num)
		return;

	if (current_page != -1 && current_page != 0) {
		current = gtk_notebook_get_nth_page (notebook, current_page);
		control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (current));

		bonobo_control_frame_set_autoactivate (control_frame, FALSE);
		bonobo_control_frame_control_deactivate (control_frame);
	}

	gtk_notebook_set_page (notebook, page_num);

	if (page_num == -1 || page_num == 0)
		return;

	current = gtk_notebook_get_nth_page (notebook, page_num);
	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (current));

	bonobo_control_frame_set_autoactivate (control_frame, FALSE);
	bonobo_control_frame_control_activate (control_frame);
}

static void
show_error (EShellView *shell_view,
	    const char *uri)
{
	EShellViewPrivate *priv;
	GtkWidget *label;
	GtkNotebook *notebook;
	char *s;

	priv = shell_view->priv;

	s = g_strdup_printf (_("Cannot open location: %s"), uri);
	label = e_clipped_label_new (s);
	g_free (s);

	gtk_widget_show (label);

	notebook = GTK_NOTEBOOK (priv->notebook);

	gtk_notebook_remove_page (notebook, 0);
	gtk_notebook_prepend_page (notebook, label, NULL);

	set_current_notebook_page (shell_view, 0);
}

/* Create a new view for @uri with @control.  It assumes a view for @uri does not exist yet.  */
static GtkWidget *
get_control_for_uri (EShellView *shell_view,
		     const char *uri)
{
	EShellViewPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;
	EStorageSet *storage_set;
	EFolder *folder;
	Bonobo_UIHandler corba_uih;
	BonoboObjectClient *handler_client;
	Bonobo_Control corba_control;
	Evolution_ShellComponent handler;
	const char *path;
	const char *folder_type;
	GtkWidget *control;
	CORBA_Environment ev;

	priv = shell_view->priv;

	path = strchr (uri, ':');
	if (path == NULL)
		return NULL;

	path++;
	if (*path == '\0')
		return NULL;

	storage_set = e_shell_get_storage_set (priv->shell);
	folder_type_registry = e_shell_get_folder_type_registry (priv->shell);

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder == NULL)
		return NULL;

	folder_type = e_folder_get_type_string (folder);
	if (folder_type == NULL)
		return NULL;

	handler_client = e_folder_type_registry_get_handler_for_type (folder_type_registry, folder_type);
	if (handler_client == NULL)
		return NULL;

	handler = bonobo_object_corba_objref (BONOBO_OBJECT (handler_client));
	if (handler_client == CORBA_OBJECT_NIL)
		return NULL;

	CORBA_exception_init (&ev);

	corba_control = Evolution_ShellComponent_create_view (handler, e_folder_get_physical_uri (folder), &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	corba_uih = bonobo_object_corba_objref (BONOBO_OBJECT (priv->uih));
	control = bonobo_widget_new_control_from_objref (corba_control, corba_uih);

	return control;
}

static gboolean
show_existing_view (EShellView *shell_view,
		    const char *uri,
		    GtkWidget *control)
{
	EShellViewPrivate *priv;
	int notebook_page;

	g_print ("Already have view for %s\n", uri);

	priv = shell_view->priv;

	notebook_page = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), control);
	g_assert (notebook_page != -1);

	/* A BonoboWidget can be a "zombie" in the sense that its actual
	   control is dead; if it's zombie, we have to recreate it.  */
	if (bonobo_widget_is_dead (BONOBO_WIDGET (control))) {
		GtkWidget *parent;
		Bonobo_UIHandler uih;

		parent = control->parent;
		uih = bonobo_object_corba_objref (BONOBO_OBJECT (priv->uih));			

		/* Out with the old.  */
		gtk_container_remove (GTK_CONTAINER (parent), control);
		g_hash_table_remove (priv->uri_to_control, uri);

		/* In with the new.  */
		control = get_control_for_uri (shell_view, uri);
		if (control == NULL)
			return FALSE;

		gtk_container_add (GTK_CONTAINER (parent), control);
		g_hash_table_insert (priv->uri_to_control, g_strdup (uri), control);

		/* Show.  */
		gtk_widget_show (control);
	}

	set_current_notebook_page (shell_view, notebook_page);

	return TRUE;
}

static gboolean
create_new_view_for_uri (EShellView *shell_view,
			 const char *uri)
{
	GtkWidget *control;
	EShellViewPrivate *priv;
	int page_num;

	priv = shell_view->priv;

	control = get_control_for_uri (shell_view, uri);
	if (control == NULL) {
		show_error (shell_view, uri);
		return FALSE;
	}

	gtk_widget_show (control);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), control, NULL);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), control);
	g_assert (page_num != -1);
	set_current_notebook_page (shell_view, page_num);

	g_hash_table_insert (priv->uri_to_control, g_strdup (uri), control);

	return TRUE;
}

gboolean
e_shell_view_display_uri (EShellView *shell_view,
			  const char *uri)
{
	EShellViewPrivate *priv;
	GtkWidget *control;
	gboolean retval;

	g_return_val_if_fail (shell_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	priv = shell_view->priv;

	if (uri == NULL) {
		gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), 0);
		gtk_notebook_prepend_page (GTK_NOTEBOOK (priv->notebook),
					   create_label_for_empty_page (), NULL);

		set_current_notebook_page (shell_view, 0);

		if (priv->uri != NULL) {
			g_free (priv->uri);
			priv->uri = NULL;
		}

		retval = TRUE;
		goto end;
	}

	g_free (priv->uri);
	priv->uri = g_strdup (uri);

	if (strncmp (uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) != 0) {
		show_error (shell_view, uri);
		return FALSE;
	}

	control = g_hash_table_lookup (priv->uri_to_control, uri);
	if (control != NULL) {
		g_assert (GTK_IS_WIDGET (control));
		show_existing_view (shell_view, uri, control);
		retval = TRUE;
		goto end;
	}

	if (! create_new_view_for_uri (shell_view, uri)) {
		show_error (shell_view, uri);
		retval = FALSE;
		goto end;
	}

	retval = TRUE;

 end:
	update_for_current_uri (shell_view);
	return retval;
}


void
e_shell_view_set_shortcut_bar_mode (EShellView *shell_view,
				    EShellViewSubwindowMode mode)
{
	EShellViewPrivate *priv;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (mode == E_SHELL_VIEW_SUBWINDOW_STICKY
			  || mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN);

	priv = shell_view->priv;

	if (priv->shortcut_bar_mode == mode)
		return;

	if (mode == E_SHELL_VIEW_SUBWINDOW_STICKY) {
		if (! GTK_WIDGET_VISIBLE (priv->shortcut_bar)) {
			gtk_widget_show (priv->shortcut_bar);
			e_paned_set_position (E_PANED (priv->hpaned), priv->hpaned_position);
		}
	} else {
		if (GTK_WIDGET_VISIBLE (priv->shortcut_bar)) {
			gtk_widget_hide (priv->shortcut_bar);
			/* FIXME this is a private field!  */
			priv->hpaned_position = E_PANED (priv->hpaned)->child1_size;
			e_paned_set_position (E_PANED (priv->hpaned), 0);
		}
	}

	priv->shortcut_bar_mode = mode;

	gtk_signal_emit (GTK_OBJECT (shell_view), signals[SHORTCUT_BAR_MODE_CHANGED], mode);
}

void
e_shell_view_set_folder_bar_mode (EShellView *shell_view,
				  EShellViewSubwindowMode mode)
{
	EShellViewPrivate *priv;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (mode == E_SHELL_VIEW_SUBWINDOW_STICKY
			  || mode == E_SHELL_VIEW_SUBWINDOW_HIDDEN);

	priv = shell_view->priv;

	if (priv->folder_bar_mode == mode)
		return;

	if (mode == E_SHELL_VIEW_SUBWINDOW_STICKY) {
		if (! GTK_WIDGET_VISIBLE (priv->storage_set_view_box)) {
			gtk_widget_show (priv->storage_set_view_box);
			e_paned_set_position (E_PANED (priv->view_hpaned), priv->view_hpaned_position);
		}
	} else {
		if (GTK_WIDGET_VISIBLE (priv->storage_set_view_box)) {
			gtk_widget_hide (priv->storage_set_view_box);
			/* FIXME this is a private field!  */
			priv->view_hpaned_position = E_PANED (priv->view_hpaned)->child1_size;
			e_paned_set_position (E_PANED (priv->view_hpaned), 0);
		}
	}

        priv->folder_bar_mode = mode;

	gtk_signal_emit (GTK_OBJECT (shell_view), signals[FOLDER_BAR_MODE_CHANGED], mode);
}

EShellViewSubwindowMode
e_shell_view_get_shortcut_bar_mode (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, E_SHELL_VIEW_SUBWINDOW_HIDDEN);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), E_SHELL_VIEW_SUBWINDOW_HIDDEN);

	return shell_view->priv->shortcut_bar_mode;
}

EShellViewSubwindowMode
e_shell_view_get_folder_bar_mode (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, E_SHELL_VIEW_SUBWINDOW_HIDDEN);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), E_SHELL_VIEW_SUBWINDOW_HIDDEN);

	return shell_view->priv->folder_bar_mode;
}


EShell *
e_shell_view_get_shell (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->shell;
}

BonoboUIHandler *
e_shell_view_get_bonobo_ui_handler (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->uih;
}

GtkWidget *
e_shell_view_get_appbar (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->appbar;
}


E_MAKE_TYPE (e_shell_view, "EShellView", EShellView, class_init, init, PARENT_TYPE)
