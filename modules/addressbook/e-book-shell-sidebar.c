/*
 * e-book-shell-sidebar.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-book-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "e-book-shell-view.h"
#include "e-book-shell-backend.h"
#include "e-addressbook-selector.h"

#define E_BOOK_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_SHELL_SIDEBAR, EBookShellSidebarPrivate))

struct _EBookShellSidebarPrivate {
	GtkWidget *selector;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

G_DEFINE_DYNAMIC_TYPE (
	EBookShellSidebar,
	e_book_shell_sidebar,
	E_TYPE_SHELL_SIDEBAR)

static void
book_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_book_shell_sidebar_get_selector (
				E_BOOK_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_sidebar_dispose (GObject *object)
{
	EBookShellSidebarPrivate *priv;

	priv = E_BOOK_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_shell_sidebar_parent_class)->dispose (object);
}

static void
book_shell_sidebar_constructed (GObject *object)
{
	EBookShellSidebarPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	EShellSettings *shell_settings;
	ESourceList *source_list;
	GtkContainer *container;
	GtkWidget *widget;

	priv = E_BOOK_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_sidebar_parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	source_list = e_book_shell_backend_get_source_list (
		E_BOOK_SHELL_BACKEND (shell_backend));

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	widget = e_addressbook_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	g_object_bind_property_full (
		shell_settings, "book-primary-selection",
		widget, "primary-selection",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		(GBindingTransformFunc) e_binding_transform_uid_to_source,
		(GBindingTransformFunc) e_binding_transform_source_to_uid,
		g_object_ref (source_list),
		(GDestroyNotify) g_object_unref);
}

static guint32
book_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	EBookShellSidebar *book_shell_sidebar;
	ESourceSelector *selector;
	ESource *source;
	gboolean can_delete = FALSE;
	gboolean is_system = FALSE;
	guint32 state = 0;

	book_shell_sidebar = E_BOOK_SHELL_SIDEBAR (shell_sidebar);
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_get_primary_selection (selector);

	if (source != NULL) {
		const gchar *uri;
		const gchar *delete;

		uri = e_source_peek_relative_uri (source);
		is_system = (uri == NULL || strcmp (uri, "system") == 0);

		can_delete = !is_system;
		delete = e_source_get_property (source, "delete");
		can_delete &= (delete == NULL || strcmp (delete, "no") != 0);
	}

	if (source != NULL)
		state |= E_BOOK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (can_delete)
		state |= E_BOOK_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE;
	if (is_system)
		state |= E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM;

	return state;
}

static void
e_book_shell_sidebar_class_init (EBookShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	g_type_class_add_private (class, sizeof (EBookShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = book_shell_sidebar_get_property;
	object_class->dispose = book_shell_sidebar_dispose;
	object_class->constructed = book_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = book_shell_sidebar_check_state;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			"Source Selector Widget",
			"This widget displays groups of address books",
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));
}

static void
e_book_shell_sidebar_class_finalize (EBookShellSidebarClass *class)
{
}

static void
e_book_shell_sidebar_init (EBookShellSidebar *book_shell_sidebar)
{
	book_shell_sidebar->priv =
		E_BOOK_SHELL_SIDEBAR_GET_PRIVATE (book_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

void
e_book_shell_sidebar_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_book_shell_sidebar_register_type (type_module);
}

GtkWidget *
e_book_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_BOOK_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

ESourceSelector *
e_book_shell_sidebar_get_selector (EBookShellSidebar *book_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_BOOK_SHELL_SIDEBAR (book_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (book_shell_sidebar->priv->selector);
}
