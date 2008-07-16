/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-window.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-window.h"
#include "e-shell-view.h"

#include "e-util/e-plugin-ui.h"
#include "e-util/e-util-private.h"
#include "e-util/gconf-bridge.h"
#include "widgets/misc/e-online-button.h"

#include "e-component-registry.h"
#include "e-shell-marshal.h"
#include "e-sidebar.h"
#include "es-menu.h"
#include "es-event.h"

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <string.h>

static gpointer parent_class;

static void
shell_window_update_title (EShellWindow *window)
{
	EShellView *shell_view;
	const gchar *title = _("Evolution");
	gchar *buffer = NULL;

	shell_view = window->priv->current_view;
	if (shell_view != NULL)
		title = e_shell_view_get_title (shell_view);

	if (shell_view != NULL && title == NULL) {
		const gchar *id;

		id = e_shell_view_get_id (shell_view);

		/* Translators: This is the window title and %s is the
		 * view name.  Most translators will want to keep it as is. */
		buffer = g_strdup_printf (_("%s - Evolution"), id);

		title = buffer;
	}

	gtk_window_set_title (GTK_WINDOW (window), title);

	g_free (buffer);
}

static void
shell_window_dispose (GObject *object)
{
	EShellWindowPrivate *priv;

	priv = E_SHELL_WINDOW_GET_PRIVATE (object);

	priv->destroyed = TRUE;

	e_shell_window_private_dispose (E_SHELL_WINDOW (object));

	if (priv->shell != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell), &priv->shell);
		priv->shell = NULL;
	}

	if (priv->current_view != NULL) {
		g_object_unref (priv->current_view);
		priv->current_view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_window_finalize (GObject *object)
{
	EShellWindowPrivate *priv;

	priv = E_SHELL_WINDOW_GET_PRIVATE (object);

	e_shell_window_private_finalize (E_SHELL_WINDOW (object));

	g_slist_foreach (priv->component_views, (GFunc) component_view_free, NULL);
	g_slist_free (priv->component_views);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_window_class_init (EShellWindowClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellWindowPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = shell_window_dispose;
	object_class->finalize = shell_window_finalize;
}

static void
shell_window_init (EShellWindow *window)
{
	GtkUIManager *manager;

	window->priv = E_SHELL_WINDOW_GET_PRIVATE (window);

	e_shell_window_private_init (window);

	manager = e_shell_window_get_ui_manager (window);

	window->priv->shell_view = e_shell_view_new (window);
	window->priv->destroyed = FALSE;

	e_plugin_ui_register_manager (
		"org.gnome.evolution.shell", manager, window);
}

GType
e_shell_window_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellWindowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_window_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellWindow),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_window_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_WINDOW, "EShellWindow", &type_info, 0);
	}

	return type;
}

const gchar *
e_shell_window_get_current_view (EShellWindow *window)
{
}

void
e_shell_window_set_current_view (EShellWindow *window,
                                 const gchar *shell_view_id)
{
}

GtkUIManager *
e_shell_window_get_ui_manager (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	return window->priv->manager;
}

GtkAction *
e_shell_window_get_action (EShellWindow *window,
                           const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = e_shell_window_get_ui_manager (window);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL && action == NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		iter = g_list_next (iter);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

GtkActionGroup *
e_shell_window_get_action_group (EShellWindow *window,
                                 const gchar *group_name)
{
	GtkUIManager *manager;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = e_shell_window_get_ui_manager (window);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;

		iter = g_list_next (iter);
	}

	g_return_val_if_reached (NULL);
}

GtkWidget *
e_shell_window_get_managed_widget (EShellWindow *window,
                                   const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = e_shell_window_get_ui_manager (window);
	widget = gtk_ui_manager_get_widget (manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

static void
init_view (EShellWindow *window,
	   ComponentView *view)
{
	EShellWindowPrivate *priv = window->priv;
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell);
	GNOME_Evolution_Component component_iface;
	GNOME_Evolution_ComponentView component_view;
	Bonobo_UIContainer container;
	Bonobo_Control sidebar_control;
	Bonobo_Control view_control;
	Bonobo_Control statusbar_control;
	CORBA_Environment ev;
	int sidebar_notebook_page_num;
	int content_notebook_page_num;

	g_return_if_fail (view->view_widget == NULL);
	g_return_if_fail (view->sidebar_widget == NULL);
	g_return_if_fail (view->notebook_page_num == -1);

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

	/* The rest of the code assumes that the component is valid and can create
	   controls; if this fails something is really wrong in the component
	   (e.g. methods not implemented)...  So handle it as if there was no
	   component at all.  */

	component_view = GNOME_Evolution_Component_createView(component_iface, BONOBO_OBJREF(priv->shell_view), &ev);
	if (component_view == NULL || BONOBO_EX (&ev)) {
		g_warning ("Cannot create view for %s", view->component_id);
		bonobo_object_release_unref (component_iface, NULL);
		CORBA_exception_free (&ev);
		return;
	}

	GNOME_Evolution_ComponentView_getControls(component_view, &sidebar_control, &view_control, &statusbar_control, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot create view for %s", view->component_id);
		bonobo_object_release_unref (component_iface, NULL);
		CORBA_exception_free (&ev);
		return;
	}

	view->component_view = component_view;

	CORBA_exception_free (&ev);

	container = bonobo_ui_component_get_container (priv->ui_component);

	view->sidebar_widget = bonobo_widget_new_control_from_objref (sidebar_control, container);
	gtk_widget_show (view->sidebar_widget);
	bonobo_object_release_unref (sidebar_control, NULL);

	view->view_widget = bonobo_widget_new_control_from_objref (view_control, container);
	gtk_widget_show (view->view_widget);
	bonobo_object_release_unref (view_control, NULL);

	view->statusbar_widget = bonobo_widget_new_control_from_objref (statusbar_control, container);
	gtk_widget_show (view->statusbar_widget);
	bonobo_object_release_unref (statusbar_control, NULL);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->content_notebook), view->view_widget, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->statusbar_notebook), view->statusbar_widget, NULL);

	sidebar_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget);
	content_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->content_notebook), view->view_widget);

	/* Since we always add a view page and a sidebar page at the same time...  */
	g_return_if_fail (sidebar_notebook_page_num == content_notebook_page_num);

	view->notebook_page_num = content_notebook_page_num;

	/* 3. Switch to the new page.  */

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->content_notebook), content_notebook_page_num);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->sidebar_notebook), content_notebook_page_num);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->statusbar_notebook), content_notebook_page_num);

	priv->current_view = view;

	bonobo_object_release_unref (component_iface, NULL);
}

static void
switch_view (EShellWindow *window, ComponentView *component_view)
{
	EShellWindowPrivate *priv = window->priv;
	GConfClient *gconf_client = gconf_client_get_default ();
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell);
	EComponentInfo *info = e_component_registry_peek_info (registry,
							       ECR_FIELD_ID,
							       component_view->component_id);
	char *title;

	if (component_view->sidebar_widget == NULL) {
		init_view (window, component_view);
	} else {
		priv->current_view = component_view;

		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->content_notebook), component_view->notebook_page_num);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->sidebar_notebook), component_view->notebook_page_num);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->statusbar_notebook), component_view->notebook_page_num);
	}

	if (component_view->title == NULL) {
		/* To translators: This is the window title and %s is the 
		component name. Most translators will want to keep it as is. */
		title = g_strdup_printf (_("%s - Evolution"), info->button_label);
		gtk_window_set_title (GTK_WINDOW (window), title);
		g_free (title);
	} else
		gtk_window_set_title (GTK_WINDOW (window), component_view->title);

	if (info->button_icon)
		gtk_window_set_icon (GTK_WINDOW (window), info->button_icon);

	gconf_client_set_string (gconf_client, "/apps/evolution/shell/view_defaults/component_id",
				 (component_view->component_alias != NULL
				  ? component_view->component_alias
				  : component_view->component_id),
				 NULL);

	g_object_unref (gconf_client);

	/** @Event: Shell component activated or switched to.
	 * @Id: component.activated
	 * @Target: ESEventTargetComponent
	 * 
	 * This event is emitted whenever the shell successfully activates component
	 * view.
	 */
	e_event_emit ((EEvent *) es_event_peek (), "component.activated", (EEventTarget *) es_event_target_new_component (es_event_peek (), component_view->component_id));

	g_object_notify (G_OBJECT (window), "current-view");
}


/* Functions to update the sensitivity of buttons and menu items depending on the status.  */

static void
update_offline_toggle_status (EShellWindow *window)
{
	EShellWindowPrivate *priv;
	GtkWidget *widget;
	const gchar *tooltip;
	gboolean online;
	gboolean sensitive;
	guint32 flags = 0;
	ESMenuTargetShell *t;

	priv = window->priv;

	switch (e_shell_get_line_status (priv->shell)) {
	case E_SHELL_LINE_STATUS_ONLINE:
		online      = TRUE;
		sensitive   = TRUE;
		tooltip     = _("Evolution is currently online.\n"
				"Click on this button to work offline.");
		flags = ES_MENU_SHELL_ONLINE;
		break;
	case E_SHELL_LINE_STATUS_GOING_OFFLINE:
		online      = TRUE;
		sensitive   = FALSE;
		tooltip     = _("Evolution is in the process of going offline.");
		flags = ES_MENU_SHELL_OFFLINE;
		break;
	case E_SHELL_LINE_STATUS_OFFLINE:
	case E_SHELL_LINE_STATUS_FORCED_OFFLINE:
		online      = FALSE;
		sensitive   = TRUE;
		tooltip     = _("Evolution is currently offline.\n"
				"Click on this button to work online.");
		flags = ES_MENU_SHELL_OFFLINE;
		break;
	default:
		g_return_if_reached ();
	}

	widget = window->priv->offline_toggle;
	gtk_widget_set_sensitive (widget, sensitive);
	gtk_widget_set_tooltip_text (widget, tooltip);
	e_online_button_set_online (E_ONLINE_BUTTON (widget), online);

	/* TODO: If we get more shell flags, this should be centralised */
	t = es_menu_target_new_shell(priv->menu, flags);
	t->target.widget = (GtkWidget *)window;
	e_menu_update_target((EMenu *)priv->menu, t);
}

static void
update_send_receive_sensitivity (EShellWindow *window)
{
	if (e_shell_get_line_status (window->priv->shell) == E_SHELL_LINE_STATUS_OFFLINE || 
		e_shell_get_line_status (window->priv->shell) == E_SHELL_LINE_STATUS_FORCED_OFFLINE)
		bonobo_ui_component_set_prop (window->priv->ui_component,
					      "/commands/SendReceive",
					      "sensitive", "0", NULL);
	else
		bonobo_ui_component_set_prop (window->priv->ui_component,
					      "/commands/SendReceive",
					      "sensitive", "1", NULL);
}


/* Callbacks.  */

static ComponentView *
get_component_view (EShellWindow *window, int id)
{
	GSList *p;

	for (p = window->priv->component_views; p; p = p->next) {
		if (((ComponentView *) p->data)->button_id == id)
			return p->data;
	}

	g_warning ("Unknown component button id %d", id);
	return NULL;
}

static void
sidebar_button_selected_callback (ESidebar *sidebar,
				  int button_id,
				  EShellWindow *window)
{
	ComponentView *component_view;

	if ((component_view = get_component_view (window, button_id)))
		switch_view (window, component_view);
}

static gboolean
sidebar_button_pressed_callback (ESidebar       *sidebar,
				 GdkEventButton *event,
				 int             button_id,
				 EShellWindow   *window)
{
	if (event->type == GDK_BUTTON_PRESS &&
	    event->button == 2) {
		/* open it in a new window */
		ComponentView *component_view;

		if ((component_view = get_component_view (window, button_id))) {
			e_shell_create_window (window->priv->shell,
					       component_view->component_id);
		}
		return TRUE;
	}
	return FALSE;
}

static void
shell_line_status_changed_callback (EShell *shell,
				    EShellLineStatus new_status,
				    EShellWindow *window)
{
	update_offline_toggle_status (window);
	update_send_receive_sensitivity (window);
}
