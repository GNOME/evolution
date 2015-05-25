/*
 * e-mail-shell-sidebar.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-shell-sidebar.h"

#include "mail/e-mail-backend.h"
#include "mail/e-mail-sidebar.h"
#include "mail/em-folder-utils.h"

#define E_MAIL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_SIDEBAR, EMailShellSidebarPrivate))

struct _EMailShellSidebarPrivate {
	GtkWidget *folder_tree;
};

enum {
	PROP_0,
	PROP_FOLDER_TREE
};

G_DEFINE_DYNAMIC_TYPE (
	EMailShellSidebar,
	e_mail_shell_sidebar,
	E_TYPE_SHELL_SIDEBAR)

static void
mail_shell_sidebar_selection_changed_cb (EShellSidebar *shell_sidebar,
                                         GtkTreeSelection *selection)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *icon_name;
	gchar *display_name = NULL;
	gboolean is_folder = FALSE;
	guint flags = 0;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (
			model, &iter,
			COL_STRING_DISPLAY_NAME, &display_name,
			COL_BOOL_IS_FOLDER, &is_folder,
			COL_UINT_FLAGS, &flags, -1);

	if (is_folder)
		icon_name = em_folder_utils_get_icon_name (flags);
	else {
		g_free (display_name);

		icon_name = shell_view_class->icon_name;
		display_name = g_strdup (shell_view_class->label);
	}

	e_shell_sidebar_set_icon_name (shell_sidebar, icon_name);
	e_shell_sidebar_set_primary_text (shell_sidebar, display_name);

	g_free (display_name);
}

static void
mail_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOLDER_TREE:
			g_value_set_object (
				value, e_mail_shell_sidebar_get_folder_tree (
				E_MAIL_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_shell_sidebar_dispose (GObject *object)
{
	EMailShellSidebarPrivate *priv;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->folder_tree != NULL) {
		g_object_unref (priv->folder_tree);
		priv->folder_tree = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_shell_sidebar_parent_class)->dispose (object);
}

static void
mail_shell_sidebar_constructed (GObject *object)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EMailBackend *backend;
	EMailSession *session;
	EAlertSink *alert_sink;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkWidget *container;
	GtkWidget *widget;
	GSettings *settings;

	/* Chain up to parent's constructed method. */
	G_OBJECT_CLASS (e_mail_shell_sidebar_parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	alert_sink = E_ALERT_SINK (shell_sidebar);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (object);

	/* Build sidebar widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget),
		GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_sidebar_new (session, alert_sink);
	gtk_container_add (GTK_CONTAINER (container), widget);
	mail_shell_sidebar->priv->folder_tree = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		shell_view, "state-key-file",
		widget, "key-file",
		G_BINDING_SYNC_CREATE);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "side-bar-search",
		widget, "enable-search",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	g_signal_connect_swapped (
		widget, "key-file-changed",
		G_CALLBACK (e_shell_view_set_state_dirty), shell_view);

	tree_view = GTK_TREE_VIEW (mail_shell_sidebar->priv->folder_tree);
	selection = gtk_tree_view_get_selection (tree_view);

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (mail_shell_sidebar_selection_changed_cb),
		shell_sidebar);
}

static gint
guess_screen_width (EMailShellSidebar *sidebar)
{
	GtkWidget *widget;
	GdkScreen *screen;
	gint screen_width;

	widget = GTK_WIDGET (sidebar);

	screen_width = 0;

	screen = gtk_widget_get_screen (widget);
	if (screen) {
		GtkWidget *toplevel;
		gint monitor;
		GdkRectangle rect;

		toplevel = gtk_widget_get_toplevel (widget);
		if (toplevel && gtk_widget_get_realized (toplevel))
			monitor = gdk_screen_get_monitor_at_window (
				screen, gtk_widget_get_window (toplevel));
		else {
			/* We don't know in which monitor the window manager
			 * will put us.  So we will just use the geometry of
			 * the first monitor.
			 */
			monitor = 0;
		}

		gdk_screen_get_monitor_geometry (screen, monitor, &rect);
		screen_width = rect.width;
	}

	if (screen_width == 0)
		screen_width = 1024;

	return screen_width;
}

static void
mail_shell_sidebar_get_preferred_height (GtkWidget *widget,
                                         gint *minimum_height,
                                         gint *natural_height)
{
	GTK_WIDGET_CLASS (e_mail_shell_sidebar_parent_class)->
		get_preferred_height (widget, minimum_height, natural_height);
}

static void
mail_shell_sidebar_get_preferred_width (GtkWidget *widget,
                                        gint *minimum_width,
                                        gint *natural_width)
{
	/* We override the normal size-request handler so that we can
	 * spit out a treeview with a suitable width.  We measure the
	 * length of a typical string and use that as the requisition's
	 * width.
	 *
	 * EMFolderTreeClass, our parent class, is based on GtkTreeView,
	 * which doesn't really have a good way of figuring out a minimum
	 * width for the tree.  This is really GTK+'s fault at large, as
	 * it only has "minimum size / allocated size", instead of
	 * "minimum size / preferred size / allocated size".  Hopefully
	 * the extended-layout branch of GTK+ will get merged soon and
	 * then we can remove this crap.
	 */

	EMailShellSidebar *sidebar;
	PangoLayout *layout;
	PangoRectangle ink_rect;
	GtkBorder padding;
	gint border;
	gint sidebar_width;
	gint screen_width;

	sidebar = E_MAIL_SHELL_SIDEBAR (widget);

	GTK_WIDGET_CLASS (e_mail_shell_sidebar_parent_class)->
		get_preferred_width (widget, minimum_width, natural_width);

	/* This string is a mockup only; it doesn't need to be translated */
	layout = gtk_widget_create_pango_layout (
		widget, "typical.account@mailservice.com");
	pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
	g_object_unref (layout);

	gtk_style_context_get_padding (gtk_widget_get_style_context (widget), 0, &padding);

	screen_width = guess_screen_width (sidebar);

	/* Thickness of frame shadow plus some slack for padding. */
	border = 2 * padding.left + 4;
	sidebar_width = ink_rect.width + border;
	sidebar_width = MIN (sidebar_width, screen_width / 4);
	*minimum_width = *natural_width = MAX (*natural_width, sidebar_width);
}

static guint32
mail_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	EMailShellSidebarPrivate *priv;
	EMailSidebar *sidebar;

	priv = E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);
	sidebar = E_MAIL_SIDEBAR (priv->folder_tree);

	return e_mail_sidebar_check_state (sidebar);
}

static void
e_mail_shell_sidebar_class_init (EMailShellSidebarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	EShellSidebarClass *shell_sidebar_class;

	g_type_class_add_private (class, sizeof (EMailShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_shell_sidebar_get_property;
	object_class->dispose = mail_shell_sidebar_dispose;
	object_class->constructed = mail_shell_sidebar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = mail_shell_sidebar_get_preferred_width;
	widget_class->get_preferred_height = mail_shell_sidebar_get_preferred_height;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = mail_shell_sidebar_check_state;

	g_object_class_install_property (
		object_class,
		PROP_FOLDER_TREE,
		g_param_spec_object (
			"folder-tree",
			NULL,
			NULL,
			EM_TYPE_FOLDER_TREE,
			G_PARAM_READABLE));
}

static void
e_mail_shell_sidebar_class_finalize (EMailShellSidebarClass *class)
{
}

static void
e_mail_shell_sidebar_init (EMailShellSidebar *mail_shell_sidebar)
{
	mail_shell_sidebar->priv =
		E_MAIL_SHELL_SIDEBAR_GET_PRIVATE (mail_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

void
e_mail_shell_sidebar_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_shell_sidebar_register_type (type_module);
}

GtkWidget *
e_mail_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

EMFolderTree *
e_mail_shell_sidebar_get_folder_tree (EMailShellSidebar *mail_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MAIL_SHELL_SIDEBAR (mail_shell_sidebar), NULL);

	return EM_FOLDER_TREE (mail_shell_sidebar->priv->folder_tree);
}
