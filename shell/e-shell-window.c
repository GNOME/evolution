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

#include "Evolution.h"

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

#include "e-shell-window.h"


#define PARENT_TYPE gtk_window_get_type ()
static GtkWindowClass *parent_class = NULL;


/* A view for each component.  These are all created when EShellWindow is
   instantiated, but with the widget pointers to NULL and the page number set
   to -1.  When the views are created the first time, the widget pointers as
   well as the notebook page value get set.  */
struct _ComponentView {
	char *component_id;

	GNOME_Evolution_Component component_iface;

	GtkWidget *sidebar_widget;
	GtkWidget *view_widget;

	int notebook_page_num;
};
typedef struct _ComponentView ComponentView;


struct _EShellWindowPrivate {
	EShell *shell;

	/* All the ComponentViews.  */
	GSList *component_views;

	/* Notebooks used to switch between components.  */
	GtkWidget *sidebar_notebook;
	GtkWidget *view_notebook;

	/* Bonobo foo.  */
	BonoboUIComponent *ui_component;
	BonoboUIContainer *ui_container;
};


/* ComponentView handling.  */

static ComponentView *
component_view_new (const char *id)
{
	ComponentView *view = g_new0 (ComponentView, 1);

	view->component_id = g_strdup (id);
	view->notebook_page_num = -1;

	return view;
}

static void
component_view_free (ComponentView *view)
{
	g_free (view->component_id);
	bonobo_object_release_unref (view->component_iface, NULL);
	g_free (view);
}


/* Utility functions.  */

static void
init_view (EShellWindow *window,
	   ComponentView *view)
{
	EShellWindowPrivate *priv = window->priv;
	Bonobo_UIContainer container;
	Bonobo_Control sidebar_control;
	Bonobo_Control view_control;
	CORBA_Environment ev;
	int sidebar_notebook_page_num;
	int view_notebook_page_num;

	g_assert (view->component_iface == CORBA_OBJECT_NIL);
	g_assert (view->view_widget == NULL);
	g_assert (view->sidebar_widget == NULL);
	g_assert (view->notebook_page_num == -1);

	CORBA_exception_init (&ev);

	/* 1. Activate component.  (FIXME: Shouldn't do this here.)  */

	view->component_iface = bonobo_activation_activate_from_id (view->component_id, 0, NULL, &ev);
	if (BONOBO_EX (&ev) || view->component_iface == CORBA_OBJECT_NIL) {
		char *ex_text = bonobo_exception_get_text (&ev);
		g_warning ("Cannot activate component  %s: %s", view->component_id, ex_text);
		g_free (ex_text);

		view->component_iface = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return;
	}

	/* 2. Set up view.  */

	GNOME_Evolution_Component_createControls (view->component_iface, &sidebar_control, &view_control, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot create view for %s", view->component_id);
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
}


/* Callbacks.  */

static void
component_button_clicked_callback (GtkButton *button,
				   EShellWindow *window)
{
	ComponentView *component_view = g_object_get_data (G_OBJECT (button), "ComponentView");
	EShellWindowPrivate *priv = window->priv;

	g_assert (component_view != NULL);

	if (component_view->sidebar_widget == NULL) {
		init_view (window, component_view);
	} else {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->view_notebook), component_view->notebook_page_num);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->sidebar_notebook), component_view->notebook_page_num);
	}
}


/* Widget layout.  */

static GtkWidget *
create_component_button (EShellWindow *window,
			 ComponentView *component_view)
{
	GtkWidget *button;
	const char *id = component_view->component_id;
	const char *p, *q;
	char *label;

	/* FIXME: Need a "name" property on the component or somesuch.  */

	p = strrchr (id, '_');
	if (p == NULL || p == id) {
		label = g_strdup (id);
	} else {
		for (q = p - 1; q != id; q--) {
			if (*q == '_')
				break;
		}

		if (*q != '_') {
			label = g_strdup (id);
		} else {
			label = g_strndup (q + 1, p - q - 1);
		}
	}

	button = gtk_button_new_with_label (label);

	g_object_set_data (G_OBJECT (button), "ComponentView", component_view);
	g_signal_connect (button, "clicked", G_CALLBACK (component_button_clicked_callback), window);

	g_free (label);

	return button;
}

static void
setup_widgets (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	Bonobo_ServerInfoList *info_list;
	CORBA_Environment ev;
	GtkWidget *paned;
	GtkWidget *sidebar_vbox;
	GtkWidget *button_box;
	int i;

	paned = gtk_hpaned_new ();
	bonobo_window_set_contents (BONOBO_WINDOW (window), paned);

	sidebar_vbox = gtk_vbox_new (FALSE, 6);
	gtk_paned_pack1 (GTK_PANED (paned), sidebar_vbox, FALSE, FALSE);

	priv->sidebar_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (sidebar_vbox), priv->sidebar_notebook, TRUE, TRUE, 0);

	priv->view_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_paned_pack2 (GTK_PANED (paned), priv->view_notebook, FALSE, FALSE);

	button_box = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (sidebar_vbox), button_box, FALSE, FALSE, 0);

	CORBA_exception_init (&ev);

	/* FIXME: Shouldn't be doing this here.  */

	info_list = bonobo_activation_query ("repo_ids.has ('IDL:GNOME/Evolution/Component:1.0')", NULL, &ev);
	if (BONOBO_EX (&ev)) {
		char *ex_text = bonobo_exception_get_text (&ev);
		g_warning ("Cannot query for components: %s\n", ex_text);
		g_free (ex_text);
		CORBA_exception_free (&ev);
		return;
	}

	for (i = 0; i < info_list->_length; i++) {
		ComponentView *component_view = component_view_new (info_list->_buffer[i].iid);
		GtkWidget *component_button = create_component_button (window, component_view);

		priv->component_views = g_slist_prepend (priv->component_views, component_view);

		gtk_box_pack_start (GTK_BOX (button_box), component_button, FALSE, FALSE, 0);
	}

	CORBA_free (info_list);
	CORBA_exception_free (&ev);

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
	EShellWindowPrivate *priv;

	priv = g_new0 (EShellWindowPrivate, 1);

	shell_window->priv = priv;
}


/* Instantiation.  */

GtkWidget *
e_shell_window_new (EShell *shell)
{
	EShellWindow *window = g_object_new (e_shell_window_get_type (), NULL);
	EShellWindowPrivate *priv = window->priv;

	if (bonobo_window_construct (BONOBO_WINDOW (window),
				     bonobo_ui_container_new (),
				     "evolution", "Ximian Evolution") == NULL) {
		g_object_unref (window);
		return NULL;
	}

	/* FIXME TODO: Add system_exception signal handling and all the other
	   stuff from e_shell_view_construct().  */

	priv->ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));

	priv->ui_component = bonobo_ui_component_new ("evolution");
	bonobo_ui_component_set_container (priv->ui_component,
					   bonobo_object_corba_objref (BONOBO_OBJECT (priv->ui_container)),
					   NULL);

	bonobo_ui_util_set_ui (priv->ui_component,
			       PREFIX,
			       EVOLUTION_UIDIR "/evolution.xml",
			       "evolution-1.4", NULL);

	window->priv->shell = shell;
	g_object_add_weak_pointer (G_OBJECT (shell), (void **) &window->priv->shell);

	setup_widgets (window);

	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

	return GTK_WIDGET (window);
}


E_MAKE_TYPE (e_shell_window, "EShellWindow", EShellWindow, class_init, init, BONOBO_TYPE_WINDOW)
