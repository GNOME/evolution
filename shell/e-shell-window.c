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
#include "e-shell-marshal.h"
#include "e-sidebar.h"

#include <gal/util/e-util.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkvbox.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include <libgnome/gnome-i18n.h>

#include <gconf/gconf-client.h>

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
	char *component_alias;

	GtkWidget *sidebar_widget;
	GtkWidget *view_widget;
	GtkWidget *statusbar_widget;

	int notebook_page_num;
};
typedef struct _ComponentView ComponentView;


struct _EShellWindowPrivate {
	EShell *shell;

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

	/* Tooltips.  */
	GtkTooltips *tooltips;

	/* The status bar widgetry.  */
	GtkWidget *status_bar;
	GtkWidget *offline_toggle;
	GtkWidget *offline_toggle_image;
	GtkWidget *menu_hint_label;
};


enum {
	COMPONENT_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* The icons for the offline/online status.  */

static GdkPixmap *offline_pixmap = NULL;
static GdkBitmap *offline_mask = NULL;

static GdkPixmap *online_pixmap = NULL;
static GdkBitmap *online_mask = NULL;


/* ComponentView handling.  */

static ComponentView *
component_view_new (const char *id, const char *alias, int button_id)
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
	g_free (view->component_id);
	g_free (view->component_alias);
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
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell);
	GNOME_Evolution_Component component_iface;
	Bonobo_UIContainer container;
	Bonobo_Control sidebar_control;
	Bonobo_Control view_control;
	Bonobo_Control statusbar_control;
	CORBA_Environment ev;
	int sidebar_notebook_page_num;
	int view_notebook_page_num;
	int statusbar_notebook_page_num;

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

	GNOME_Evolution_Component_createControls (component_iface, &sidebar_control, &view_control, &statusbar_control, &ev);
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

	view->statusbar_widget = bonobo_widget_new_control_from_objref (statusbar_control, container);
	gtk_widget_show (view->statusbar_widget);
	bonobo_object_release_unref (statusbar_control, NULL);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->view_notebook), view->view_widget, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->statusbar_notebook), view->statusbar_widget, NULL);

	sidebar_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->sidebar_notebook), view->sidebar_widget);
	view_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->view_notebook), view->view_widget);
	statusbar_notebook_page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->view_notebook), view->statusbar_widget);

	/* Since we always add a view page and a sidebar page at the same time...  */
	g_assert (sidebar_notebook_page_num == view_notebook_page_num);

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
	EComponentRegistry *registry = e_shell_peek_component_registry (window->priv->shell);
	EComponentInfo *info = e_component_registry_peek_info (registry,
							       component_view->component_id);
	char *title;

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

	title = g_strdup_printf ("Evolution - %s", info->button_label);
	gtk_window_set_title (GTK_WINDOW (window), title);
	g_free (title);

	if (info->button_icon)
		gtk_window_set_icon (GTK_WINDOW (window), info->button_icon);

	gconf_client_set_string (gconf_client, "/apps/evolution/shell/view_defaults/component_id",
				 (component_view->component_alias != NULL
				  ? component_view->component_alias
				  : component_view->component_id),
				 NULL);

	g_object_unref (gconf_client);

	g_signal_emit (window, signals[COMPONENT_CHANGED], 0);
}


/* Functions to update the sensitivity of buttons and menu items depending on the status.  */

static void
update_offline_toggle_status (EShellWindow *window)
{
	EShellWindowPrivate *priv;
	GdkPixmap *icon_pixmap;
	GdkBitmap *icon_mask;
	const char *tooltip;
	gboolean sensitive;

	priv = window->priv;

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

static void
update_send_receive_sensitivity (EShellWindow *window)
{
	if (e_shell_get_line_status (window->priv->shell) == E_SHELL_LINE_STATUS_OFFLINE)
		bonobo_ui_component_set_prop (window->priv->ui_component,
					      "/commands/SendReceive",
					      "sensitive", "0", NULL);
	else
		bonobo_ui_component_set_prop (window->priv->ui_component,
					      "/commands/SendReceive",
					      "sensitive", "1", NULL);
}


/* Callbacks.  */

static void
sidebar_button_selected_callback (ESidebar *sidebar,
				  int button_id,
				  EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	ComponentView *component_view = NULL;
	GSList *p;

	for (p = priv->component_views; p != NULL; p = p->next) {
		if (((ComponentView *) p->data)->button_id == button_id) {
			component_view = p->data;
			break;
		}
	}

	if (component_view == NULL) {
		g_warning ("Unknown component button id %d", button_id);
		return;
	}

	switch_view (window, component_view);
}

static void
offline_toggle_clicked_callback (GtkButton *button,
				 EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;

	switch (e_shell_get_line_status (priv->shell)) {
	case E_SHELL_LINE_STATUS_ONLINE:
		e_shell_go_offline (priv->shell, window);
		break;
	case E_SHELL_LINE_STATUS_OFFLINE:
		e_shell_go_online (priv->shell, window);
		break;
	default:
		g_assert_not_reached ();
	}
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
			     const char *hint,
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
load_icons (void)
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
setup_offline_toggle (EShellWindow *window)
{
	EShellWindowPrivate *priv;
	GtkWidget *toggle;
	GtkWidget *image;

	priv = window->priv;

	toggle = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (toggle, GTK_CAN_FOCUS);
	gtk_button_set_relief (GTK_BUTTON (toggle), GTK_RELIEF_NONE);

	g_signal_connect (toggle, "clicked",
			  G_CALLBACK (offline_toggle_clicked_callback), window);

	image = gtk_image_new_from_pixmap (offline_pixmap, offline_mask);

	gtk_container_add (GTK_CONTAINER (toggle), image);

	gtk_widget_show (toggle);
	gtk_widget_show (image);

	priv->offline_toggle       = toggle;
	priv->offline_toggle_image = image;

	update_offline_toggle_status (window);

	g_assert (priv->status_bar != NULL);

	gtk_box_pack_start (GTK_BOX (priv->status_bar), priv->offline_toggle, FALSE, TRUE, 0);
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
setup_status_bar (EShellWindow *window)
{
	EShellWindowPrivate *priv;
	BonoboUIEngine *ui_engine;

	priv = window->priv;

	priv->status_bar = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (priv->status_bar);

	setup_offline_toggle (window);
	setup_menu_hint_label (window);
	setup_statusbar_notebook (window);

	ui_engine = bonobo_window_get_ui_engine (BONOBO_WINDOW (window));

	g_signal_connect (ui_engine, "add_hint", G_CALLBACK (ui_engine_add_hint_callback), window);
	g_signal_connect (ui_engine, "remove_hint", G_CALLBACK (ui_engine_remove_hint_callback), window);
}

static void
setup_widgets (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	EComponentRegistry *registry = e_shell_peek_component_registry (priv->shell);
	GConfClient *gconf_client = gconf_client_get_default ();
	GtkWidget *contents_vbox;
	GSList *p;
	int button_id;

	priv->paned = gtk_hpaned_new ();

	priv->sidebar = e_sidebar_new ();
	g_signal_connect (priv->sidebar, "button_selected",
			  G_CALLBACK (sidebar_button_selected_callback), window);
	gtk_paned_pack1 (GTK_PANED (priv->paned), priv->sidebar, FALSE, FALSE);

	priv->sidebar_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->sidebar_notebook), FALSE);
	e_sidebar_set_selection_widget (E_SIDEBAR (priv->sidebar), priv->sidebar_notebook);

	priv->view_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->view_notebook), FALSE);
	gtk_paned_pack2 (GTK_PANED (priv->paned), priv->view_notebook, TRUE, TRUE);

	gtk_paned_set_position (GTK_PANED (priv->paned),
				gconf_client_get_int (gconf_client, "/apps/evolution/shell/view_defaults/folder_bar/width", NULL));

	button_id = 0;
	for (p = e_component_registry_peek_list (registry); p != NULL; p = p->next) {
		EComponentInfo *info = p->data;
		ComponentView *view = component_view_new (info->id, info->alias, button_id);

		window->priv->component_views = g_slist_prepend (window->priv->component_views, view);
		e_sidebar_add_button (E_SIDEBAR (priv->sidebar), info->button_label, info->button_icon, button_id);

		button_id ++;
	}

	setup_status_bar (window);

	contents_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (contents_vbox), priv->paned, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (contents_vbox), priv->status_bar, FALSE, TRUE, 0);
	gtk_widget_show_all (contents_vbox);

	/* We only display this when a menu item is actually selected.  */
	gtk_widget_hide (priv->menu_hint_label);

	bonobo_window_set_contents (BONOBO_WINDOW (window), contents_vbox);
	g_object_unref (gconf_client);
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

	if (priv->tooltips != NULL) {
		gtk_object_destroy (GTK_OBJECT (priv->tooltips));
		priv->tooltips = NULL;
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

	signals[COMPONENT_CHANGED] = g_signal_new ("component_changed",
						   G_OBJECT_CLASS_TYPE (object_class),
						   G_SIGNAL_RUN_FIRST,
						   G_STRUCT_OFFSET (EShellWindowClass, component_changed),
						   NULL, NULL,
						   e_shell_marshal_NONE__NONE,
						   G_TYPE_NONE, 0);

	load_icons ();
}

static void
init (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = g_new0 (EShellWindowPrivate, 1);

	priv->tooltips = gtk_tooltips_new ();

	shell_window->priv = priv;
}


/* Instantiation.  */

GtkWidget *
e_shell_window_new (EShell *shell,
		    const char *component_id)
{
	EShellWindow *window = g_object_new (e_shell_window_get_type (), NULL);
	EShellWindowPrivate *priv = window->priv;
	GConfClient *gconf_client = gconf_client_get_default ();
	BonoboUIContainer *ui_container;

	if (bonobo_window_construct (BONOBO_WINDOW (window),
				     bonobo_ui_container_new (),
				     "evolution", "Ximian Evolution") == NULL) {
		g_object_unref (window);
		g_object_unref (gconf_client);
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

	g_signal_connect_object (shell, "line_status_changed", G_CALLBACK (shell_line_status_changed_callback), window, 0);

	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

	if (component_id != NULL) {
		e_shell_window_switch_to_component (window, component_id);
	} else {
		char *default_component_id;

		default_component_id = gconf_client_get_string (gconf_client,
								"/apps/evolution/shell/view_defaults/component_id",
								NULL);
		g_object_unref (gconf_client);

		if (default_component_id == NULL) {
			e_shell_window_switch_to_component (window, "mail");
		} else {
			e_shell_window_switch_to_component (window, default_component_id);
			g_free (default_component_id);
		}
	}

	gtk_window_set_default_size (GTK_WINDOW (window),
				     gconf_client_get_int (gconf_client, "/apps/evolution/shell/view_defaults/width", NULL),
				     gconf_client_get_int (gconf_client, "/apps/evolution/shell/view_defaults/height", NULL));

	g_object_unref (gconf_client);
	return GTK_WIDGET (window);
}


void
e_shell_window_switch_to_component (EShellWindow *window, const char *component_id)
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


const char *
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

	return window->priv->shell;
}


BonoboUIComponent *
e_shell_window_peek_bonobo_ui_component (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	return window->priv->ui_component;
}

void
e_shell_window_save_defaults (EShellWindow *window)
{
	GConfClient *client = gconf_client_get_default ();

	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/width",
			      GTK_WIDGET (window)->allocation.width, NULL);
	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/height",
			      GTK_WIDGET (window)->allocation.height, NULL);

	gconf_client_set_int (client, "/apps/evolution/shell/view_defaults/folder_bar/width",
			      gtk_paned_get_position (GTK_PANED (window->priv->paned)), NULL);

	g_object_unref (client);
}

void
e_shell_window_show_settings (EShellWindow *window)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	e_shell_show_settings (window->priv->shell, window->priv->current_view ? window->priv->current_view->component_alias : NULL, window);
}


E_MAKE_TYPE (e_shell_window, "EShellWindow", EShellWindow, class_init, init, BONOBO_TYPE_WINDOW)
