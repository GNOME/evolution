/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-window.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
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

#include <config.h>

#include "e-shell-window.h"

#include "Evolution.h"

#include "e-component-registry.h"
#include "e-shell-window-commands.h"
#include "e-sidebar.h"

#include "e-util/e-lang-utils.h"

#include <gal/util/e-util.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkvbox.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include <string.h>


#define PARENT_TYPE gtk_window_get_type ()
static GtkWindowClass *parent_class = NULL;


/* A view for each component.  These are all created when EShellWindow is
   instantiated, but with the widget pointers to NULL and the page number set
   to -1.  When the views are created the first time, the widget pointers as
   well as the notebook page value get set.  */
struct _ComponentView {
	int button_id;
	char *component_id;

	GtkWidget *sidebar_widget;
	GtkWidget *view_widget;

	int notebook_page_num;
};
typedef struct _ComponentView ComponentView;


struct _EShellWindowPrivate {
	EShell *shell;

	/* All the ComponentViews.  */
	GSList *component_views;

	/* The sidebar.  */
	GtkWidget *sidebar;

	/* Notebooks used to switch between components.  */
	GtkWidget *sidebar_notebook;
	GtkWidget *view_notebook;

	/* Bonobo foo.  */
	BonoboUIComponent *ui_component;

	/* The current view (can be NULL initially).  */
	ComponentView *current_view;
};


/* ComponentView handling.  */

static ComponentView *
component_view_new (const char *id, int button_id)
{
	ComponentView *view = g_new0 (ComponentView, 1);

	view->component_id = g_strdup (id);
	view->button_id = button_id;
	view->notebook_page_num = -1;

	return view;
}

static void
component_view_free (ComponentView *view)
{
	g_free (view->component_id);
	g_free (view);
}

static void
component_view_deactivate (ComponentView *view)
{
	BonoboControlFrame *view_control_frame;
	BonoboControlFrame *sidebar_control_frame;

	g_return_if_fail (view->sidebar_widget != NULL);
	g_return_if_fail (view->view_widget != NULL);

	view_control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (view->view_widget));
	bonobo_control_frame_control_deactivate (view_control_frame);

	sidebar_control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (view->sidebar_widget));
	bonobo_control_frame_control_deactivate (sidebar_control_frame);
}

static void
component_view_activate (ComponentView *view)
{
	BonoboControlFrame *view_control_frame;
	BonoboControlFrame *sidebar_control_frame;

	g_return_if_fail (view->sidebar_widget != NULL);
	g_return_if_fail (view->view_widget != NULL);

	view_control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (view->view_widget));
	bonobo_control_frame_control_activate (view_control_frame);

	sidebar_control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (view->sidebar_widget));
	bonobo_control_frame_control_activate (sidebar_control_frame);
}


/* Utility functions.  */

static void
init_view (EShellWindow *window,
	   ComponentView *view)
{
	EShellWindowPrivate *priv = window->priv;
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell);
	GNOME_Evolution_Component component_iface;
	Bonobo_UIContainer container;
	Bonobo_Control sidebar_control;
	Bonobo_Control view_control;
	CORBA_Environment ev;
	int sidebar_notebook_page_num;
	int view_notebook_page_num;

	g_assert (view->view_widget == NULL);
	g_assert (view->sidebar_widget == NULL);
	g_assert (view->notebook_page_num == -1);

	CORBA_exception_init (&ev);

	/* 1. Activate component.  (FIXME: Shouldn't do this here.)  */

	component_iface = e_component_registry_activate (registry, view->component_id, &ev);
	if (BONOBO_EX (&ev) || component_iface == CORBA_OBJECT_NIL) {
		char *ex_text = bonobo_exception_get_text (&ev);
		g_warning ("Cannot activate component  %s: %s", view->component_id, ex_text);
		g_free (ex_text);
		CORBA_exception_free (&ev);
		return;
	}

	/* 2. Set up view.  */

	GNOME_Evolution_Component_createControls (component_iface, &sidebar_control, &view_control, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot create view for %s", view->component_id);

		/* The rest of the code assumes that the component is valid and can create
		   controls; if this fails something is really wrong in the component
		   (e.g. methods not implemented)...  So handle it as if there was no
		   component at all.  */
		bonobo_object_release_unref (component_iface, NULL);
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	container = bonobo_ui_component_get_container (priv->ui_component);

	view->sidebar_widget = bonobo_widget_new_control_from_objref (sidebar_control, container);
	gtk_widget_show (view->sidebar_widget);
	bonobo_object_release_unref (sidebar_control, NULL);

	view->view_widget = bonobo_widget_new_control_from_objref (view_control, container);
	gtk_widget_show (view->view_widget);
	bonobo_object_release_unref (view_control, NULL);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->view_notebook), view->view_widget, NULL);

	sidebar_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget);
	view_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->view_notebook), view->view_widget);

	/* Since we always add a view page and a sidebar page at the same time...  */
	g_assert (sidebar_notebook_page_num == view_notebook_page_num);

	view->notebook_page_num = view_notebook_page_num;

	/* 3. Switch to the new page.  */

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->view_notebook), view_notebook_page_num);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->sidebar_notebook), view_notebook_page_num);

	if (priv->current_view != NULL)
		component_view_deactivate (priv->current_view);
	priv->current_view = view;
	component_view_activate (view);

	bonobo_object_release_unref (component_iface, NULL);
}


/* Callbacks.  */

static void
sidebar_button_selected_callback (ESidebar *sidebar,
				  int button_id,
				  EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	ComponentView *component_view;
	GSList *p;

	for (p = priv->component_views; p != NULL; p = p->next) {
		if (((ComponentView *) p->data)->button_id == button_id) {
			component_view = p->data;
			break;
		}
	}

	if (component_view == NULL) {
		g_warning ("Unknown component id %d", button_id);
		return;
	}

	if (component_view->sidebar_widget == NULL) {
		init_view (window, component_view);
	} else {
		if (priv->current_view != NULL)
			component_view_deactivate (priv->current_view);
		priv->current_view = component_view;
		component_view_activate (component_view);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->view_notebook), component_view->notebook_page_num);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->sidebar_notebook), component_view->notebook_page_num);
	}
}


/* Widget layout.  */

static void
setup_widgets (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	EComponentRegistry *registry = e_shell_peek_component_registry (priv->shell);
	GtkWidget *paned;
	GSList *p;
	int button_id;

	paned = gtk_hpaned_new ();
	bonobo_window_set_contents (BONOBO_WINDOW (window), paned);

	priv->sidebar = e_sidebar_new ();
	g_signal_connect (priv->sidebar, "button_selected",
			  G_CALLBACK (sidebar_button_selected_callback), window);
	gtk_paned_pack1 (GTK_PANED (paned), priv->sidebar, FALSE, FALSE);

	priv->sidebar_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	e_sidebar_set_selection_widget (E_SIDEBAR (priv->sidebar), priv->sidebar_notebook);

	priv->view_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_paned_pack2 (GTK_PANED (paned), priv->view_notebook, TRUE, TRUE);

	gtk_paned_set_position (GTK_PANED (paned), 200);

	button_id = 0;
	for (p = e_component_registry_peek_list (registry); p != NULL; p = p->next) {
		EComponentInfo *info = p->data;
		ComponentView *view = component_view_new (info->id, button_id);

		window->priv->component_views = g_slist_prepend (window->priv->component_views, view);
		e_sidebar_add_button (E_SIDEBAR (priv->sidebar), info->button_label, info->button_icon, button_id);

		button_id ++;
	}

	gtk_widget_show_all (paned);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShellWindowPrivate *priv = E_SHELL_WINDOW (object)->priv;

	if (priv->shell != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (priv->shell), (void **) &priv->shell);
		priv->shell = NULL;
	}

	if (priv->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->ui_component));
		priv->ui_component = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShellWindowPrivate *priv = E_SHELL_WINDOW (object)->priv;

	g_slist_foreach (priv->component_views, (GFunc) component_view_free, NULL);
	g_slist_free (priv->component_views);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (EShellWindowClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);
}

static void
init (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = g_new0 (EShellWindowPrivate, 1);

	shell_window->priv = priv;
}


/* Instantiation.  */

GtkWidget *
e_shell_window_new (EShell *shell)
{
	EShellWindow *window = g_object_new (e_shell_window_get_type (), NULL);
	EShellWindowPrivate *priv = window->priv;
	BonoboUIContainer *ui_container;

	if (bonobo_window_construct (BONOBO_WINDOW (window),
				     bonobo_ui_container_new (),
				     "evolution", "Ximian Evolution") == NULL) {
		g_object_unref (window);
		return NULL;
	}

	window->priv->shell = shell;
	g_object_add_weak_pointer (G_OBJECT (shell), (void **) &window->priv->shell);

	/* FIXME TODO: Add system_exception signal handling and all the other
	   stuff from e_shell_view_construct().  */

	ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));

	priv->ui_component = bonobo_ui_component_new ("evolution");
	bonobo_ui_component_set_container (priv->ui_component,
					   bonobo_object_corba_objref (BONOBO_OBJECT (ui_container)),
					   NULL);

	bonobo_ui_util_set_ui (priv->ui_component,
			       PREFIX,
			       EVOLUTION_UIDIR "/evolution.xml",
			       "evolution-" BASE_VERSION, NULL);

	e_shell_window_commands_setup (window);

	setup_widgets (window);

	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

	return GTK_WIDGET (window);
}


EShell *
e_shell_window_peek_shell (EShellWindow *window)
{
	return window->priv->shell;
}


BonoboUIComponent *
e_shell_window_peek_bonobo_ui_component (EShellWindow *window)
{
	return window->priv->ui_component;
}

void
e_shell_window_save_defaults (EShellWindow *window)
{
	/* FIXME */
	g_warning ("e_shell_window_save_defaults() unimplemented");
}

void
e_shell_window_show_settings (EShellWindow *window)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	e_shell_show_settings (window->priv->shell, NULL, window);
}


E_MAKE_TYPE (e_shell_window, "EShellWindow", EShellWindow, class_init, init, BONOBO_TYPE_WINDOW)
