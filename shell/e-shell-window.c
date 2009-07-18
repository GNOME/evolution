/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-window.h"
#include "e-shell-view.h"

#include "Evolution.h"

#include "e-util/e-util-private.h"
#include "widgets/misc/e-online-button.h"

#include "e-component-registry.h"
#include "e-shell-window-commands.h"
#include "e-sidebar.h"
#include "es-menu.h"
#include "es-event.h"

#include <gtk/gtk.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <string.h>

#if defined(NM_SUPPORT) && NM_SUPPORT
gboolean e_shell_dbus_initialise (EShell *shell);
#endif

/* A view for each component.  These are all created when EShellWindow is
   instantiated, but with the widget pointers to NULL and the page number set
   to -1.  When the views are created the first time, the widget pointers as
   well as the notebook page value get set.  */
struct _ComponentView {
	gint button_id;
	gchar *component_id;
	gchar *component_alias;

	GNOME_Evolution_ComponentView component_view;
	gchar *title;

	GtkWidget *sidebar_widget;
	GtkWidget *view_widget;
	GtkWidget *statusbar_widget;

	gint notebook_page_num;
};
typedef struct _ComponentView ComponentView;

struct _EShellWindowPrivate {
	union {
		EShell *eshell;
		gpointer pointer;
	} shell;

	EShellView *shell_view;	/* CORBA wrapper for this, just a placeholder */

	/* plugin menu manager */
	ESMenu *menu;

	/* All the ComponentViews.  */
	GSList *component_views;

	/* The paned widget for the sidebar and component views */
	GtkWidget *paned;

	/* The sidebar.  */
	GtkWidget *sidebar;

	/* Notebooks used to switch between components.  */
	GtkWidget *sidebar_notebook;
	GtkWidget *view_notebook;
	GtkWidget *statusbar_notebook;

	/* Bonobo foo.  */
	BonoboUIComponent *ui_component;

	/* The current view (can be NULL initially).  */
	ComponentView *current_view;

	/* The status bar widgetry.  */
	GtkWidget *status_bar;
	GtkWidget *offline_toggle;
	GtkWidget *menu_hint_label;

	/* The timeout for saving the window size */
	guint      store_window_gsizeimer;
	gboolean destroyed;
};

enum {
	COMPONENT_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EShellWindow, e_shell_window, BONOBO_TYPE_WINDOW)

static gboolean store_window_size (GtkWidget* widget);

/* ComponentView handling.  */

static ComponentView *
component_view_new (const gchar *id, const gchar *alias, gint button_id)
{
	ComponentView *view = g_new0 (ComponentView, 1);

	view->component_id = g_strdup (id);
	view->component_alias = g_strdup (alias);
	view->button_id = button_id;
	view->notebook_page_num = -1;

	return view;
}

static void
component_view_free (ComponentView *view)
{
	if (view->component_view) {
		CORBA_Environment ev = { NULL };

		CORBA_Object_release(view->component_view, &ev);
		CORBA_exception_free(&ev);
	}

	g_free (view->component_id);
	g_free (view->component_alias);
	g_free (view->title);
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

static void
init_view (EShellWindow *window,
	   ComponentView *view)
{
	EShellWindowPrivate *priv = window->priv;
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell.eshell);
	GNOME_Evolution_Component component_iface;
	GNOME_Evolution_ComponentView component_view;
	Bonobo_UIContainer container;
	Bonobo_Control sidebar_control;
	Bonobo_Control view_control;
	Bonobo_Control statusbar_control;
	CORBA_boolean select_item;
	CORBA_Environment ev;
	gint sidebar_notebook_page_num;
	gint view_notebook_page_num;

	g_return_if_fail (view->view_widget == NULL);
	g_return_if_fail (view->sidebar_widget == NULL);
	g_return_if_fail (view->notebook_page_num == -1);

	select_item = !e_shell_get_crash_recovery (priv->shell.eshell);
	e_shell_set_crash_recovery (priv->shell.eshell, FALSE);

	CORBA_exception_init (&ev);

	/* 1. Activate component.  (FIXME: Shouldn't do this here.)  */

	component_iface = e_component_registry_activate (registry, view->component_id, &ev);
	if (BONOBO_EX (&ev) || component_iface == CORBA_OBJECT_NIL) {
		gchar *ex_text = bonobo_exception_get_text (&ev);
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

	component_view = GNOME_Evolution_Component_createView(component_iface, BONOBO_OBJREF(priv->shell_view), select_item, &ev);
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
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->view_notebook), view->view_widget, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->statusbar_notebook), view->statusbar_widget, NULL);

	sidebar_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget);
	view_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->view_notebook), view->view_widget);

	/* Since we always add a view page and a sidebar page at the same time...  */
	g_return_if_fail (sidebar_notebook_page_num == view_notebook_page_num);

	view->notebook_page_num = view_notebook_page_num;

	/* 3. Switch to the new page.  */

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->view_notebook), view_notebook_page_num);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->sidebar_notebook), view_notebook_page_num);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->statusbar_notebook), view_notebook_page_num);

	if (priv->current_view != NULL)
		component_view_deactivate (priv->current_view);
	priv->current_view = view;
	component_view_activate (view);

	bonobo_object_release_unref (component_iface, NULL);
}

static void
switch_view (EShellWindow *window, ComponentView *component_view)
{
	EShellWindowPrivate *priv = window->priv;
	GConfClient *gconf_client = gconf_client_get_default ();
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell.eshell);
	EComponentInfo *info = e_component_registry_peek_info (registry,
							       ECR_FIELD_ID,
							       component_view->component_id);
	gchar *title;

	if (component_view->sidebar_widget == NULL) {
		init_view (window, component_view);
	} else {
		if (priv->current_view != NULL)
			component_view_deactivate (priv->current_view);
		priv->current_view = component_view;
		component_view_activate (component_view);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->view_notebook), component_view->notebook_page_num);
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

	if (info->icon_name)
		gtk_window_set_icon_name (GTK_WINDOW (window), info->icon_name);

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

	g_signal_emit (window, signals[COMPONENT_CHANGED], 0);
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

	switch (e_shell_get_line_status (priv->shell.eshell)) {
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
	if (e_shell_get_line_status (window->priv->shell.eshell) == E_SHELL_LINE_STATUS_OFFLINE ||
		e_shell_get_line_status (window->priv->shell.eshell) == E_SHELL_LINE_STATUS_FORCED_OFFLINE)
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
get_component_view (EShellWindow *window, gint id)
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
				  gint button_id,
				  EShellWindow *window)
{
	ComponentView *component_view;

	if ((component_view = get_component_view (window, button_id)))
		switch_view (window, component_view);
}

static gboolean
sidebar_button_pressed_callback (ESidebar       *sidebar,
				 GdkEventButton *event,
				 gint             button_id,
				 EShellWindow   *window)
{
	if (event->type == GDK_BUTTON_PRESS &&
	    event->button == 2) {
		/* open it in a new window */
		ComponentView *component_view;

		if ((component_view = get_component_view (window, button_id))) {
			e_shell_create_window (window->priv->shell.eshell,
					       component_view->component_id,
					       window);
		}
		return TRUE;
	}
	return FALSE;
}

static void
offline_toggle_clicked_cb (EShellWindow *window)
{
	EShell *shell;
	GNOME_Evolution_ShellState shell_state;

	shell = window->priv->shell.eshell;

	switch (e_shell_get_line_status (shell)) {
	case E_SHELL_LINE_STATUS_ONLINE:
		shell_state = GNOME_Evolution_USER_OFFLINE;
		break;
	case E_SHELL_LINE_STATUS_OFFLINE:
	case E_SHELL_LINE_STATUS_FORCED_OFFLINE:
		shell_state = GNOME_Evolution_USER_ONLINE;
		break;
	default:
		g_return_if_reached();
	}

	e_shell_set_line_status (shell, shell_state);
}

static void
shell_line_status_changed_callback (EShell *shell,
				    EShellLineStatus new_status,
				    EShellWindow *window)
{
	update_offline_toggle_status (window);
	update_send_receive_sensitivity (window);
}

static void
ui_engine_add_hint_callback (BonoboUIEngine *engine,
			     const gchar *hint,
			     EShellWindow *window)
{
	gtk_label_set_text (GTK_LABEL (window->priv->menu_hint_label), hint);
	gtk_widget_show (window->priv->menu_hint_label);
	gtk_widget_hide (window->priv->statusbar_notebook);
}

static void
ui_engine_remove_hint_callback (BonoboUIEngine *engine,
				EShellWindow *window)
{
	gtk_widget_hide (window->priv->menu_hint_label);
	gtk_widget_show (window->priv->statusbar_notebook);
}

/* Widgetry.  */

static void
setup_offline_toggle (EShellWindow *window)
{
	GtkWidget *widget;

	g_return_if_fail (window->priv->status_bar != NULL);

	widget = e_online_button_new ();
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (offline_toggle_clicked_cb), window);
	gtk_box_pack_start (
		GTK_BOX (window->priv->status_bar),
		widget, FALSE, TRUE, 0);
	window->priv->offline_toggle = widget;
	gtk_widget_show (widget);

	update_offline_toggle_status (window);
}

static void
setup_menu_hint_label (EShellWindow *window)
{
	EShellWindowPrivate *priv;

	priv = window->priv;

	priv->menu_hint_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->menu_hint_label), 0.0, 0.5);

	gtk_box_pack_start (GTK_BOX (priv->status_bar), priv->menu_hint_label, TRUE, TRUE, 0);
}

static void
setup_statusbar_notebook (EShellWindow *window)
{
	EShellWindowPrivate *priv;

	priv = window->priv;

	priv->statusbar_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->statusbar_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->statusbar_notebook), FALSE);

	gtk_box_pack_start (GTK_BOX (priv->status_bar), priv->statusbar_notebook, TRUE, TRUE, 0);
	gtk_widget_show (priv->statusbar_notebook);
}

static void
setup_nm_support (EShellWindow *window)
{
#if defined(NM_SUPPORT) && NM_SUPPORT
       e_shell_dbus_initialise (window->priv->shell.eshell);
#endif
}

static void
setup_status_bar (EShellWindow *window)
{
	EShellWindowPrivate *priv;
	BonoboUIEngine *ui_engine;
	GConfClient *gconf_client;
	gint height;

	priv = window->priv;

	priv->status_bar = gtk_hbox_new (FALSE, 2);

	/* Make the status bar as large as the task bar. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (GTK_WIDGET (priv->status_bar), -1, height * 2);

	gconf_client = gconf_client_get_default ();
	if (gconf_client_get_bool (gconf_client,"/apps/evolution/shell/view_defaults/statusbar_visible",NULL))
		gtk_widget_show (priv->status_bar);
	g_object_unref (gconf_client);

	/* setup dbus interface here*/
	setup_nm_support (window);

	setup_offline_toggle (window);
	setup_menu_hint_label (window);
	setup_statusbar_notebook (window);

	ui_engine = bonobo_window_get_ui_engine (BONOBO_WINDOW (window));

	g_signal_connect (ui_engine, "add_hint", G_CALLBACK (ui_engine_add_hint_callback), window);
	g_signal_connect (ui_engine, "remove_hint", G_CALLBACK (ui_engine_remove_hint_callback), window);
}

static void
menu_component_selected (BonoboUIComponent *uic,
			 EShellWindow *window,
			 const gchar *path)
{
	gchar *component_id;

	component_id = strchr(path, '-');
	if (component_id)
		e_shell_window_switch_to_component (window, component_id+1);
}

static GConfEnumStringPair button_styles[] = {
         { E_SIDEBAR_MODE_TEXT, "text" },
         { E_SIDEBAR_MODE_ICON, "icons" },
         { E_SIDEBAR_MODE_BOTH, "both" },
         { E_SIDEBAR_MODE_TOOLBAR, "toolbar" },
	{ -1, NULL }
};

static void
setup_widgets (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	EComponentRegistry *registry = e_shell_peek_component_registry (priv->shell.eshell);
	GConfClient *gconf_client = gconf_client_get_default ();
	GtkWidget *contents_vbox;
	GSList *p;
	GString *xml;
	gint button_id;
	gboolean visible;
	gchar *style;
	gint mode;

	priv->paned = gtk_hpaned_new ();
	gtk_widget_show (priv->paned);

	priv->sidebar = e_sidebar_new ();
	g_signal_connect (priv->sidebar, "button_selected",
			  G_CALLBACK (sidebar_button_selected_callback), window);
	g_signal_connect (priv->sidebar, "button_pressed",
			  G_CALLBACK (sidebar_button_pressed_callback), window);
	gtk_paned_pack1 (GTK_PANED (priv->paned), priv->sidebar, FALSE, FALSE);
	gtk_widget_show (priv->sidebar);

	priv->sidebar_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	e_sidebar_set_selection_widget (E_SIDEBAR (priv->sidebar), priv->sidebar_notebook);
	gtk_widget_show (priv->sidebar_notebook);

	priv->view_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_paned_pack2 (GTK_PANED (priv->paned), priv->view_notebook, TRUE, FALSE);
	gtk_widget_show (priv->view_notebook);

	gtk_paned_set_position (GTK_PANED (priv->paned),
				gconf_client_get_int (gconf_client, "/apps/evolution/shell/view_defaults/folder_bar/width", NULL));

	/* The buttons */
	visible = gconf_client_get_bool (gconf_client,
						 "/apps/evolution/shell/view_defaults/buttons_visible",
						 NULL);
	bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
				      "/commands/ViewButtonsHide",
				      "state",
				      visible ? "0" : "1",
				      NULL);

	e_sidebar_set_show_buttons (E_SIDEBAR (priv->sidebar), visible);

	style = gconf_client_get_string (gconf_client,
					 "/apps/evolution/shell/view_defaults/buttons_style",
					 NULL);

	if (gconf_string_to_enum (button_styles, style, &mode)) {
		switch (mode) {
		case E_SIDEBAR_MODE_TEXT:
			bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
						      "/commands/ViewButtonsText",
						      "state", "1", NULL);
			break;
		case E_SIDEBAR_MODE_ICON:
			bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
						      "/commands/ViewButtonsIcon",
						      "state", "1", NULL);
			break;
		case E_SIDEBAR_MODE_BOTH:
			bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
						      "/commands/ViewButtonsIconText",
						      "state", "1", NULL);
			break;

		case E_SIDEBAR_MODE_TOOLBAR:
			bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
						      "/commands/ViewButtonsToolbar",
						      "state", "1", NULL);
			break;
		}

		e_sidebar_set_mode (E_SIDEBAR (priv->sidebar), mode);
	}
	g_free (style);

	/* Status Bar*/
	visible = gconf_client_get_bool (gconf_client,
					 "/apps/evolution/shell/view_defaults/statusbar_visible",
					 NULL);
	bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
				      "/commands/ViewStatusBar",
				      "state",
				      visible ? "1" : "0",
				      NULL);

	/* Side Bar*/
	visible = gconf_client_get_bool (gconf_client,
					 "/apps/evolution/shell/view_defaults/sidebar_visible",
					 NULL);
	bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
				      "/commands/ViewSideBar",
				      "state",
				      visible ? "1" : "0",
				      NULL);

	/* The tool bar */
	visible = gconf_client_get_bool (gconf_client,
					 "/apps/evolution/shell/view_defaults/toolbar_visible",
					 NULL);
	bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
				      "/commands/ViewToolbar",
				      "state",
				      visible ? "1" : "0",
				      NULL);
	bonobo_ui_component_set_prop (e_shell_window_peek_bonobo_ui_component (window),
				      "/Toolbar",
				      "hidden",
				      visible ? "0" : "1",
				      NULL);

	button_id = 0;
	xml = g_string_new("");
	for (p = e_component_registry_peek_list (registry); p != NULL; p = p->next) {
		gchar *tmp, *tmp2;
		EComponentInfo *info = p->data;
		ComponentView *view = component_view_new (info->id, info->alias, button_id);
		GtkIconInfo *icon_info;
		gint width;

		window->priv->component_views = g_slist_prepend (window->priv->component_views, view);

		if (!info->button_label || !info->menu_label)
			continue;
		e_sidebar_add_button (E_SIDEBAR (priv->sidebar), info->button_label, info->button_tooltips, info->icon_name, button_id);

		g_string_printf(xml, "SwitchComponent-%s", info->alias);
		bonobo_ui_component_add_verb (e_shell_window_peek_bonobo_ui_component (window),
					      xml->str,
					      (BonoboUIVerbFn)menu_component_selected,
					      window);

		g_string_printf(xml, "<submenu name=\"View\">"
				"<submenu name=\"Window\">"
				"<placeholder name=\"WindowComponent\">"
				"<menuitem name=\"SwitchComponent-%s\" "
				"verb=\"\" label=\"%s\" accel=\"%s\" tip=\"",
				info->alias,
				info->menu_label,
				info->menu_accelerator);
		tmp = g_strdup_printf (_("Switch to %s"), info->button_label);
		tmp2 = g_markup_escape_text (tmp, -1);
		g_string_append (xml, tmp2);
		g_free (tmp2);
		g_free (tmp);

		gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, NULL);
		icon_info = gtk_icon_theme_lookup_icon (
			gtk_icon_theme_get_default (),
			info->icon_name, width, 0);
		g_string_append_printf(xml, "\" pixtype=\"filename\" pixname=\"%s\"/>"
				       "</placeholder></submenu></submenu>\n",
				       icon_info ? gtk_icon_info_get_filename (icon_info) : "");
		gtk_icon_info_free (icon_info);
		bonobo_ui_component_set_translate (e_shell_window_peek_bonobo_ui_component (window),
						   "/menu",
						   xml->str,
						   NULL);
		g_string_printf(xml, "<cmd name=\"SwitchComponent-%s\"/>\n", info->alias);
		bonobo_ui_component_set_translate (e_shell_window_peek_bonobo_ui_component (window),
						   "/commands",
						   xml->str,
						   NULL);
		button_id ++;
	}
	g_string_free(xml, TRUE);

	setup_status_bar (window);

	contents_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (contents_vbox), priv->paned, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (contents_vbox), priv->status_bar, FALSE, TRUE, 0);
	gtk_widget_show (contents_vbox);

	/* We only display this when a menu item is actually selected.  */
	gtk_widget_hide (priv->menu_hint_label);

	bonobo_window_set_contents (BONOBO_WINDOW (window), contents_vbox);
	g_object_unref (gconf_client);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShellWindow *self = E_SHELL_WINDOW (object);
	EShellWindowPrivate *priv = self->priv;

	priv->destroyed = TRUE;

	if (priv->shell.eshell != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (priv->shell.eshell), &priv->shell.pointer);
		priv->shell.eshell = NULL;
	}

	if (priv->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->ui_component));
		priv->ui_component = NULL;
	}

	if (priv->store_window_gsizeimer) {
		g_source_remove (priv->store_window_gsizeimer);
		self->priv->store_window_gsizeimer = 0;

		/* There was a timer. Let us store the settings.*/
		store_window_size (GTK_WIDGET (self));
	}

	(* G_OBJECT_CLASS (e_shell_window_parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShellWindowPrivate *priv = E_SHELL_WINDOW (object)->priv;

	g_slist_foreach (priv->component_views, (GFunc) component_view_free, NULL);
	g_slist_free (priv->component_views);

	g_object_unref(priv->menu);

	g_free (priv);

	(* G_OBJECT_CLASS (e_shell_window_parent_class)->finalize) (object);
}

/* GtkWidget methods */
static void
e_shell_window_remove_regsizeimer (EShellWindow* self)
{
	if (self->priv->store_window_gsizeimer) {
		g_source_remove (self->priv->store_window_gsizeimer);
		self->priv->store_window_gsizeimer = 0;
	}
}

static gboolean
impl_window_state (GtkWidget *widget, GdkEventWindowState* ev)
{
	gboolean retval = FALSE;

	/* store only if the window state really changed */
	if ((ev->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) != 0) {
		GConfClient* client = gconf_client_get_default ();
		gconf_client_set_bool (client, "/apps/evolution/shell/view_defaults/maximized",
				       (ev->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0, NULL);
		g_object_unref(client);
	}

	if ((ev->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0) {
		e_shell_window_remove_regsizeimer (E_SHELL_WINDOW (widget));
	}

	if (GTK_WIDGET_CLASS (e_shell_window_parent_class)->window_state_event) {
		retval |= GTK_WIDGET_CLASS (e_shell_window_parent_class)->window_state_event (widget, ev);
	}

	return retval;
}

static gboolean
store_window_size (GtkWidget* widget)
{
	GConfClient* client = gconf_client_get_default ();
	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/width",
			      widget->allocation.width, NULL);
	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/height",
			      widget->allocation.height, NULL);
	g_object_unref(client);

	E_SHELL_WINDOW (widget)->priv->store_window_gsizeimer = 0;
	return FALSE; /* remove this timeout */
}

static void
impl_size_alloc (GtkWidget* widget, GtkAllocation* alloc)
{
	EShellWindow* self = E_SHELL_WINDOW (widget);
	e_shell_window_remove_regsizeimer(self);

	if (GTK_WIDGET_REALIZED(widget) && !(gdk_window_get_state(widget->window) & GDK_WINDOW_STATE_MAXIMIZED)) {
		/* update the size storage timer */
		self->priv->store_window_gsizeimer = g_timeout_add_seconds (1, (GSourceFunc)store_window_size, self);
	}

	if (GTK_WIDGET_CLASS (e_shell_window_parent_class)->size_allocate) {
		GTK_WIDGET_CLASS (e_shell_window_parent_class)->size_allocate (widget, alloc);
	}
}

/* Initialization.  */

static void
e_shell_window_class_init (EShellWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	widget_class->window_state_event = impl_window_state;
	widget_class->size_allocate      = impl_size_alloc;

	signals[COMPONENT_CHANGED] = g_signal_new ("component_changed",
						   G_OBJECT_CLASS_TYPE (object_class),
						   G_SIGNAL_RUN_FIRST,
						   G_STRUCT_OFFSET (EShellWindowClass, component_changed),
						   NULL, NULL,
						   g_cclosure_marshal_VOID__VOID,
						   G_TYPE_NONE, 0);
}

static void
e_shell_window_init (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = g_new0 (EShellWindowPrivate, 1);

	priv->shell_view = e_shell_view_new(shell_window);
	priv->destroyed = FALSE;

	shell_window->priv = priv;

	/** @HookPoint: Shell Main Menu
	 * @Id: org.gnome.evolution.shell
	 * @Type: ESMenu
	 * @Target: ESMenuTargetShell
	 *
	 * This hook point is used to add bonobo menu's to the main
	 * evolution shell window, used for global commands not
	 * requiring a specific component.
	 */
	priv->menu = es_menu_new("org.gnome.evolution.shell");

}

/* Instantiation.  */

GtkWidget *
e_shell_window_new (EShell *shell,
		    const gchar *component_id)
{
	EShellWindow *window = g_object_new (e_shell_window_get_type (), NULL);
	EShellWindowPrivate *priv = window->priv;
	GConfClient *gconf_client = gconf_client_get_default ();
	BonoboUIContainer *ui_container;
	gchar *default_component_id = NULL;
	gchar *xmlfile;
	gint width, height;

	if (bonobo_window_construct (BONOBO_WINDOW (window),
				     bonobo_ui_container_new (),
				     "evolution", "Evolution") == NULL) {
		g_object_unref (window);
		g_object_unref (gconf_client);
		return NULL;
	}

	window->priv->shell.eshell = shell;
	g_object_add_weak_pointer (G_OBJECT (shell), &window->priv->shell.pointer);

	/* FIXME TODO: Add system_exception signal handling and all the other
	   stuff from e_shell_view_construct().  */

	ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));

	priv->ui_component = bonobo_ui_component_new ("evolution");
	bonobo_ui_component_set_container (priv->ui_component,
					   bonobo_object_corba_objref (BONOBO_OBJECT (ui_container)),
					   NULL);

	xmlfile = g_build_filename (EVOLUTION_UIDIR, "evolution.xml", NULL);
	bonobo_ui_util_set_ui (priv->ui_component,
			       PREFIX,
			       xmlfile,
			       "evolution", NULL);
	g_free (xmlfile);

	e_shell_window_commands_setup (window);
	e_menu_activate((EMenu *)priv->menu, priv->ui_component, TRUE);

	setup_widgets (window);

	if (gconf_client_get_bool (gconf_client,"/apps/evolution/shell/view_defaults/sidebar_visible",NULL))
		gtk_widget_show (priv->sidebar);
	else
		gtk_widget_hide (priv->sidebar);

	update_send_receive_sensitivity (window);
	g_signal_connect_object (shell, "line_status_changed", G_CALLBACK (shell_line_status_changed_callback), window, 0);

	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

	if (component_id == NULL) {
		component_id = default_component_id =
			gconf_client_get_string (gconf_client,
						 "/apps/evolution/shell/view_defaults/component_id",
						 NULL);
		if (component_id == NULL)
			component_id = "mail";
	}

	e_shell_window_switch_to_component (window, component_id);
	g_free(default_component_id);
	g_object_unref (gconf_client);

	width = gconf_client_get_int (gconf_client, "/apps/evolution/shell/view_defaults/width", NULL);
	height = gconf_client_get_int (gconf_client, "/apps/evolution/shell/view_defaults/height", NULL);
	gtk_window_set_default_size (GTK_WINDOW (window), (width >= 0) ? width : 0,
			(height >= 0) ? height : 0);
	if (gconf_client_get_bool (gconf_client, "/apps/evolution/shell/view_defaults/maximized", NULL)) {
		gtk_window_maximize (GTK_WINDOW (window));
	}

	g_object_unref (gconf_client);
	return GTK_WIDGET (window);
}

void
e_shell_window_switch_to_component (EShellWindow *window, const gchar *component_id)
{
	EShellWindowPrivate *priv = window->priv;
	ComponentView *view = NULL;
	GSList *p;

	g_return_if_fail (E_IS_SHELL_WINDOW (window));
	g_return_if_fail (component_id != NULL);

	for (p = priv->component_views; p != NULL; p = p->next) {
		ComponentView *this_view = p->data;

		if (strcmp (this_view->component_id, component_id) == 0
		    || (this_view->component_alias != NULL
			&& strcmp (this_view->component_alias, component_id) == 0))
		{
			view = p->data;
			break;
		}
	}

	if (view == NULL) {
		g_warning ("Unknown component %s", component_id);
		return;
	}

	e_sidebar_select_button (E_SIDEBAR (priv->sidebar), view->button_id);
}

const gchar *
e_shell_window_peek_current_component_id (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	if (window->priv->current_view == NULL)
		return NULL;

	return window->priv->current_view->component_id;
}

EShell *
e_shell_window_peek_shell (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	return window->priv->shell.eshell;
}

BonoboUIComponent *
e_shell_window_peek_bonobo_ui_component (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	return window->priv->ui_component;
}

ESidebar *
e_shell_window_peek_sidebar (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	return E_SIDEBAR (window->priv->sidebar);
}

GtkWidget *
e_shell_window_peek_statusbar (EShellWindow *window)
{
	return window->priv->status_bar;
}

void
e_shell_window_save_defaults (EShellWindow *window)
{
	GConfClient *client = gconf_client_get_default ();
	gchar *prop;
	const gchar *style;
	gboolean visible;

	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/folder_bar/width",
			      gtk_paned_get_position (GTK_PANED (window->priv->paned)), NULL);

	/* The button styles */
	if ((style = gconf_enum_to_string (button_styles, e_sidebar_get_mode (E_SIDEBAR (window->priv->sidebar))))) {
		gconf_client_set_string (client,
				       "/apps/evolution/shell/view_defaults/buttons_style",
				       style, NULL);
	}

	/* Button hiding setting */
	prop = bonobo_ui_component_get_prop (e_shell_window_peek_bonobo_ui_component (window),
					     "/commands/ViewButtonsHide",
					     "state",
					     NULL);
	if (prop) {
		visible = prop[0] == '0';
		gconf_client_set_bool (client,
				       "/apps/evolution/shell/view_defaults/buttons_visible",
				       visible,
				       NULL);
		g_free (prop);
	}

	/* Toolbar visibility setting */
	prop = bonobo_ui_component_get_prop (e_shell_window_peek_bonobo_ui_component (window),
					     "/commands/ViewToolbar",
					     "state",
					     NULL);
	if (prop) {
		visible = prop[0] == '1';
		gconf_client_set_bool (client,
				       "/apps/evolution/shell/view_defaults/toolbar_visible",
				       visible,
				       NULL);
		g_free (prop);
	}

	/* SideBar visibility setting */
	prop = bonobo_ui_component_get_prop (e_shell_window_peek_bonobo_ui_component (window),
					     "/commands/ViewSideBar",
					     "state",
					     NULL);
	if (prop) {
		visible = prop[0] == '1';
		gconf_client_set_bool (client,
				       "/apps/evolution/shell/view_defaults/sidebar_visible",
				       visible,
				       NULL);
		g_free (prop);
	}

	g_object_unref (client);
}

void
e_shell_window_show_settings (EShellWindow *window)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	e_shell_show_settings (window->priv->shell.eshell, window->priv->current_view ? window->priv->current_view->component_alias : NULL, window);
}

void
e_shell_window_set_title(EShellWindow *window, const gchar *component_id, const gchar *title)
{
	EShellWindowPrivate *priv = window->priv;
	ComponentView *view = NULL;
	GSList *p;

	if (priv->destroyed)
		return;

	for (p = priv->component_views; p != NULL; p = p->next) {
		ComponentView *this_view = p->data;

		if (strcmp (this_view->component_id, component_id) == 0
		    || (this_view->component_alias != NULL
			&& strcmp (this_view->component_alias, component_id) == 0)) {
			view = p->data;
			break;
		}
	}

	if (view) {
		g_free(view->title);
		view->title = g_strdup(title);
		if (view->title && view == priv->current_view)
			gtk_window_set_title((GtkWindow *)window, title);
	}
}

/**
 * e_shell_window_change_component_button_icon
 * Changes icon of components button at sidebar. For more info how this behaves see
 * info at @ref e_sidebar_change_button_icon.
 * @param window EShellWindow instance.
 * @param component_id ID of the component.
 * @param icon Icon buffer.
 **/
void
e_shell_window_change_component_button_icon (EShellWindow *window, const gchar *component_id, const gchar *icon_name)
{
	EShellWindowPrivate *priv;
	GSList *p;

	g_return_if_fail (window != NULL);
	g_return_if_fail (component_id != NULL);

	priv = window->priv;

	if (priv->destroyed)
		return;

	for (p = priv->component_views; p != NULL; p = p->next) {
		ComponentView *this_view = p->data;

		if (strcmp (this_view->component_id, component_id) == 0
		    || (this_view->component_alias != NULL
			&& strcmp (this_view->component_alias, component_id) == 0)) {
			e_sidebar_change_button_icon (E_SIDEBAR (priv->sidebar), icon_name, this_view->button_id);
			break;
		}
	}
}
