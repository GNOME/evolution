/*
 * e-book-shell-sidebar.c
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

#include "evolution-config.h"

#include "e-book-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "e-book-shell-view.h"
#include "e-book-shell-backend.h"
#include "e-addressbook-selector.h"

struct _EBookShellSidebarPrivate {
	GtkWidget *selector;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EBookShellSidebar, e_book_shell_sidebar, E_TYPE_SHELL_SIDEBAR, 0,
	G_ADD_PRIVATE_DYNAMIC (EBookShellSidebar))

static gboolean
book_shell_sidebar_map_uid_to_source (GValue *value,
                                      GVariant *variant,
                                      gpointer user_data)
{
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	registry = E_SOURCE_REGISTRY (user_data);
	uid = g_variant_get_string (variant, NULL);
	if (uid != NULL && *uid != '\0')
		source = e_source_registry_ref_source (registry, uid);
	else
		source = e_source_registry_ref_default_address_book (registry);
	g_value_take_object (value, source);

	return (source != NULL);
}

static GVariant *
book_shell_sidebar_map_source_to_uid (const GValue *value,
                                      const GVariantType *expected_type,
                                      gpointer user_data)
{
	GVariant *variant = NULL;
	ESource *source;

	source = g_value_get_object (value);

	if (source != NULL) {
		const gchar *uid;

		uid = e_source_get_uid (source);
		variant = g_variant_new_string (uid);
	}

	return variant;
}

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
	EBookShellSidebar *self = E_BOOK_SHELL_SIDEBAR (object);

	g_clear_object (&self->priv->selector);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_shell_sidebar_parent_class)->dispose (object);
}

static void
book_shell_sidebar_constructed (GObject *object)
{
	EBookShellSidebar *self = E_BOOK_SHELL_SIDEBAR (object);
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	EClientCache *client_cache;
	GtkContainer *container;
	GtkWidget *widget;
	GSettings *settings;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_sidebar_parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	client_cache = e_shell_get_client_cache (shell);
	widget = e_addressbook_selector_new (client_cache);
	gtk_container_add (GTK_CONTAINER (container), widget);
	self->priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	e_source_selector_load_groups_setup (E_SOURCE_SELECTOR (self->priv->selector),
		e_shell_view_get_state_key_file (shell_view));

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	g_settings_bind_with_mapping (
		settings, "primary-addressbook",
		widget, "primary-selection",
		G_SETTINGS_BIND_DEFAULT,
		book_shell_sidebar_map_uid_to_source,
		book_shell_sidebar_map_source_to_uid,
		e_client_cache_ref_registry (client_cache),
		(GDestroyNotify) g_object_unref);

	g_object_unref (settings);
}

static guint32
book_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	EBookShellSidebar *book_shell_sidebar;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source, *clicked_source;
	gboolean is_writable = FALSE;
	gboolean is_removable = FALSE;
	gboolean is_remote_creatable = FALSE;
	gboolean is_remote_deletable = FALSE;
	gboolean in_collection = FALSE;
	gboolean has_primary_source = FALSE;
	gboolean refresh_supported = FALSE;
	guint32 state = 0;

	book_shell_sidebar = E_BOOK_SHELL_SIDEBAR (shell_sidebar);
	selector = e_book_shell_sidebar_get_selector (book_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	registry = e_source_selector_get_registry (selector);

	if (source != NULL) {
		EClient *client;
		ESource *collection;

		has_primary_source = TRUE;
		is_writable = e_source_get_writable (source);
		is_removable = e_source_get_removable (source);
		is_remote_creatable = e_source_get_remote_creatable (source);
		is_remote_deletable = e_source_get_remote_deletable (source);

		collection = e_source_registry_find_extension (
			registry, source, E_SOURCE_EXTENSION_COLLECTION);
		if (collection != NULL) {
			in_collection = TRUE;
			g_object_unref (collection);
		}

		client = e_client_selector_ref_cached_client (
			E_CLIENT_SELECTOR (selector), source);

		if (client != NULL) {
			refresh_supported =
				e_client_check_refresh_supported (client);
			g_object_unref (client);
		} else {
			/* It's also used to allow-auth-prompt for the source */
			refresh_supported = TRUE;
		}

		g_object_unref (source);
	}

	clicked_source = e_book_shell_view_get_clicked_source (e_shell_sidebar_get_shell_view (shell_sidebar));
	if (clicked_source && clicked_source == source)
		state |= E_BOOK_SHELL_SIDEBAR_CLICKED_SOURCE_IS_PRIMARY;
	if (clicked_source && e_source_has_extension (clicked_source, E_SOURCE_EXTENSION_COLLECTION))
		state |= E_BOOK_SHELL_SIDEBAR_CLICKED_SOURCE_IS_COLLECTION;
	if (has_primary_source)
		state |= E_BOOK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (is_writable)
		state |= E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE;
	if (is_removable)
		state |= E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE;
	if (is_remote_creatable)
		state |= E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_CREATABLE;
	if (is_remote_deletable)
		state |= E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE;
	if (in_collection)
		state |= E_BOOK_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION;
	if (refresh_supported)
		state |= E_BOOK_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static void
e_book_shell_sidebar_class_init (EBookShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

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
	book_shell_sidebar->priv = e_book_shell_sidebar_get_instance_private (book_shell_sidebar);

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
