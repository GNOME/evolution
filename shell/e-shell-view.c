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

#include "e-shell.h"
#include "e-shortcuts-view.h"
#include "e-util/e-util.h"

#include "e-shell-view.h"
#include "e-shell-view-menu.h"



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
	GtkWidget *hpaned;
	GtkWidget *shortcut_bar;
	GtkWidget *contents;
	GtkWidget *notebook;

	/* The view we have already open.  */
	GHashTable *uri_to_control;
};

#define DEFAULT_SHORTCUT_BAR_WIDTH 100

#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 600


static GtkWidget *
create_label_for_empty_page (void)
{
	GtkWidget *label;

	label = gtk_label_new (_("(No folder displayed)"));
	gtk_widget_show (label);

	return label;
}

static void
setup_menus (EShellView *shell_view)
{
	BonoboUIHandlerMenuItem *list;
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	priv->uih = bonobo_ui_handler_new ();
	bonobo_ui_handler_set_app (priv->uih, GNOME_APP (shell_view));
	bonobo_ui_handler_create_menubar (priv->uih);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (e_shell_view_menu, shell_view);
	bonobo_ui_handler_menu_add_list (priv->uih, "/", list);
	bonobo_ui_handler_menu_free_list (list);
}

static gboolean
bonobo_widget_is_dead (BonoboWidget *bw)
{
	BonoboObject *boc = BONOBO_OBJECT (bonobo_widget_get_server (bw));
	CORBA_Object obj = bonobo_object_corba_objref (boc);

	CORBA_Environment ev;
	
	gboolean is_dead = FALSE;

	CORBA_exception_init (&ev);
	if (CORBA_Object_non_existent(obj, &ev))
		is_dead = TRUE;
	CORBA_exception_free (&ev);

	return is_dead;
}


static void
activate_shortcut_cb (EShortcutsView *shortcut_view,
		      EShortcuts *shortcuts,
		      const char *uri,
		      gpointer data)
{
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (data);

	e_shell_view_display_uri (shell_view, uri);
}

static void
setup_widgets (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = shell_view->priv;

	priv->hpaned = gtk_hpaned_new ();
	gnome_app_set_contents (GNOME_APP (shell_view), priv->hpaned);

	/* The shortcut bar.  */

	priv->shortcut_bar = e_shortcuts_new_view (e_shell_get_shortcuts (priv->shell));
	gtk_paned_add1 (GTK_PANED (priv->hpaned), priv->shortcut_bar);
	gtk_paned_set_position (GTK_PANED (priv->hpaned), DEFAULT_SHORTCUT_BAR_WIDTH);
	gtk_signal_connect (GTK_OBJECT (priv->shortcut_bar), "activate_shortcut",
			    GTK_SIGNAL_FUNC (activate_shortcut_cb), shell_view);

	/* The tabless notebook which we used to contain the views.  */

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_paned_add2 (GTK_PANED (priv->hpaned), priv->notebook);

	/* Page for "No URL displayed" message.  */

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), create_label_for_empty_page (), NULL);

	/* Show stuff.  */

	gtk_widget_show (priv->shortcut_bar);
	gtk_widget_show (priv->notebook);

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (GTK_WINDOW (shell_view), DEFAULT_WIDTH, DEFAULT_HEIGHT);
}


/* GtkObject methods.  */

static void
hash_forall_destroy_control (gpointer name,
			     gpointer value,
			     gpointer data)
{
	CORBA_Object corba_control;
	CORBA_Environment ev;
	BonoboObject *bonobo_object;
	BonoboWidget *bonobo_widget;

	bonobo_widget = BONOBO_WIDGET (value);
	bonobo_object = BONOBO_OBJECT (bonobo_widget_get_server (bonobo_widget));
	corba_control = bonobo_object_corba_objref (bonobo_object);

	g_return_if_fail (corba_control != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (corba_control, &ev);
	CORBA_exception_free (&ev);

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
}

static void
init (EShellView *shell_view)
{
	EShellViewPrivate *priv;

	priv = g_new (EShellViewPrivate, 1);

	priv->shell        = NULL;
	priv->uih          = NULL;
	priv->uri          = NULL;
	priv->hpaned       = NULL;
	priv->shortcut_bar = NULL;
	priv->contents     = NULL;
	priv->notebook     = NULL;

	priv->uri_to_control = g_hash_table_new (g_str_hash, g_str_equal);

	shell_view->priv = priv;
}


void
e_shell_view_construct (EShellView *shell_view,
			EShell *shell,
			const char *uri)
{
	EShellViewPrivate *priv;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (uri == NULL || ! g_path_is_absolute (uri));

	gnome_app_construct (GNOME_APP (shell_view), "evolution", "Evolution");

	priv = shell_view->priv;

	gtk_object_ref (GTK_OBJECT (shell));
	priv->shell = shell;

	priv->uri = g_strdup (uri);

	setup_widgets (shell_view);
	setup_menus (shell_view);
}

GtkWidget *
e_shell_view_new (EShell *shell,
		  const char *uri)
{
	GtkWidget *new;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (uri == NULL || ! g_path_is_absolute (uri), NULL);

	new = gtk_type_new (e_shell_view_get_type ());
	e_shell_view_construct (E_SHELL_VIEW (new), shell, uri);

	return new;
}


/* This displays the specified page, doing the appropriate Bonobo
   activation/deactivation magic to make sure things work nicely.
   FIXME: Crappy way to solve the issue.  */
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
	char *s;

	priv = shell_view->priv;

	s = g_strdup_printf (_("Cannot open location: %s\n"), uri);
	label = gtk_label_new (s);
	g_free (s);

	gtk_widget_show (label);

	gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), 0);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (priv->notebook), label, NULL);
}

/* Create a new view for @uri with @control.  It assumes a view for @uri does
   not exist yet.  */
static GtkWidget *
get_control_for_uri (EShellView *shell_view,
		     const char *uri)
{
	EShellViewPrivate *priv;
	EFolderTypeRepository *folder_type_repository;
	EStorageSet *storage_set;
	EFolder *folder;
	Bonobo_UIHandler corba_uih;
	const char *control_id;
	const char *path;
	const char *folder_type;
	GtkWidget *control;

	priv = shell_view->priv;

	path = strchr (uri, ':');
	if (path == NULL)
		return NULL;

	path++;
	if (*path == '\0')
		return NULL;

	storage_set = e_shell_get_storage_set (priv->shell);
	folder_type_repository = e_shell_get_folder_type_repository (priv->shell);

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder == NULL)
		return NULL;

	folder_type = e_folder_get_type_string (folder);
	if (folder_type == NULL)
		return NULL;

	control_id = e_folder_type_repository_get_control_id_for_type (folder_type_repository,
								       folder_type);
	if (control_id == NULL)
		return NULL;

	corba_uih = bonobo_object_corba_objref (BONOBO_OBJECT (priv->uih));
	control = bonobo_widget_new_control (control_id, corba_uih);

	if (control == NULL)
		return NULL;

	bonobo_widget_set_property (BONOBO_WIDGET (control),
				    "folder_uri", e_folder_get_physical_uri (folder),
				    NULL);

	return control;
}

static gboolean
show_existing_view (EShellView *shell_view,
		    const char *uri,
		    GtkWidget *control)
{
	EShellViewPrivate *priv;
	int notebook_page;

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

		return TRUE;
	}

	g_free (priv->uri);
	priv->uri = g_strdup (uri);

	if (strncmp (uri, "evolution:", 10) != 0) {
		show_error (shell_view, uri);
		return FALSE;
	}

	control = g_hash_table_lookup (priv->uri_to_control, uri);
	if (control != NULL) {
		g_assert (GTK_IS_WIDGET (control));
		show_existing_view (shell_view, uri, control);
		return TRUE;
	}

	if (! create_new_view_for_uri (shell_view, uri)) {
		show_error (shell_view, uri);
		return FALSE;
	}

	return TRUE;
}


EShell *
e_shell_view_get_shell (EShellView *shell_view)
{
	g_return_val_if_fail (shell_view != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->shell;
}


E_MAKE_TYPE (e_shell_view, "EShellView", EShellView, class_init, init, PARENT_TYPE)
