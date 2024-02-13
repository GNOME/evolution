/*
 * e-book-source-config.c
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
 */

#include "evolution-config.h"

#include "e-book-source-config.h"

#include <glib/gi18n-lib.h>

struct _EBookSourceConfigPrivate {
	GtkWidget *default_button;
	GtkWidget *autocomplete_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (EBookSourceConfig, e_book_source_config, E_TYPE_SOURCE_CONFIG)

static ESource *
book_source_config_ref_default (ESourceConfig *config)
{
	ESourceRegistry *registry;

	registry = e_source_config_get_registry (config);

	return e_source_registry_ref_default_address_book (registry);
}

static void
book_source_config_set_default (ESourceConfig *config,
                                ESource *source)
{
	ESourceRegistry *registry;

	registry = e_source_config_get_registry (config);

	e_source_registry_set_default_address_book (registry, source);
}

static void
book_source_config_dispose (GObject *object)
{
	EBookSourceConfig *self = E_BOOK_SOURCE_CONFIG (object);

	g_clear_object (&self->priv->default_button);
	g_clear_object (&self->priv->autocomplete_button);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_source_config_parent_class)->dispose (object);
}

static void
book_source_config_constructed (GObject *object)
{
	EBookSourceConfig *self;
	ESource *default_source;
	ESource *original_source;
	ESourceConfig *config;
	GtkWidget *widget;
	const gchar *label;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_source_config_parent_class)->constructed (object);

	config = E_SOURCE_CONFIG (object);
	self = E_BOOK_SOURCE_CONFIG (object);

	label = _("Mark as default address book");
	widget = gtk_check_button_new_with_label (label);
	self->priv->default_button = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	label = _("Autocomplete with this address book");
	widget = gtk_check_button_new_with_label (label);
	self->priv->autocomplete_button = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	default_source = book_source_config_ref_default (config);
	original_source = e_source_config_get_original_source (config);

	if (original_source != NULL) {
		gboolean active;

		active = e_source_equal (original_source, default_source);
		g_object_set (self->priv->default_button, "active", active, NULL);
	}

	g_object_unref (default_source);

	e_source_config_insert_widget (
		config, NULL, NULL, self->priv->default_button);

	e_source_config_insert_widget (
		config, NULL, NULL, self->priv->autocomplete_button);
}

static const gchar *
book_source_config_get_backend_extension_name (ESourceConfig *config)
{
	return E_SOURCE_EXTENSION_ADDRESS_BOOK;
}

static GList *
book_source_config_list_eligible_collections (ESourceConfig *config)
{
	GQueue trash = G_QUEUE_INIT;
	GList *list, *link;

	/* Chain up to parent's list_eligible_collections() method. */
	list = E_SOURCE_CONFIG_CLASS (e_book_source_config_parent_class)->
		list_eligible_collections (config);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceCollection *extension;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_COLLECTION;
		extension = e_source_get_extension (source, extension_name);

		if (!e_source_collection_get_contacts_enabled (extension))
			g_queue_push_tail (&trash, link);
	}

	/* Remove ineligible collections from the list. */
	while ((link = g_queue_pop_head (&trash)) != NULL) {
		g_object_unref (link->data);
		list = g_list_delete_link (list, link);
	}

	return list;
}

static void
book_source_config_init_candidate (ESourceConfig *config,
                                   ESource *scratch_source)
{
	EBookSourceConfig *self;
	ESourceExtension *extension;
	const gchar *extension_name;

	/* Chain up to parent's init_candidate() method. */
	E_SOURCE_CONFIG_CLASS (e_book_source_config_parent_class)->init_candidate (config, scratch_source);

	self = E_BOOK_SOURCE_CONFIG (config);

	extension_name = E_SOURCE_EXTENSION_AUTOCOMPLETE;
	extension = e_source_get_extension (scratch_source, extension_name);

	e_binding_bind_property (
		extension, "include-me",
		self->priv->autocomplete_button, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static void
book_source_config_commit_changes (ESourceConfig *config,
                                   ESource *scratch_source)
{
	EBookSourceConfig *self;
	ESource *default_source;
	GtkToggleButton *toggle_button;

	self = E_BOOK_SOURCE_CONFIG (config);
	toggle_button = GTK_TOGGLE_BUTTON (self->priv->default_button);

	/* Chain up to parent's commit_changes() method. */
	E_SOURCE_CONFIG_CLASS (e_book_source_config_parent_class)->commit_changes (config, scratch_source);

	default_source = book_source_config_ref_default (config);

	/* The default setting is a little tricky to get right.  If
	 * the toggle button is active, this ESource is now the default.
	 * That much is simple.  But if the toggle button is NOT active,
	 * then we have to inspect the old default.  If this ESource WAS
	 * the default, reset the default to 'system'.  If this ESource
	 * WAS NOT the old default, leave it alone. */
	if (gtk_toggle_button_get_active (toggle_button))
		book_source_config_set_default (config, scratch_source);
	else if (e_source_equal (scratch_source, default_source))
		book_source_config_set_default (config, NULL);

	g_object_unref (default_source);
}

static void
e_book_source_config_class_init (EBookSourceConfigClass *class)
{
	GObjectClass *object_class;
	ESourceConfigClass *source_config_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_source_config_dispose;
	object_class->constructed = book_source_config_constructed;

	source_config_class = E_SOURCE_CONFIG_CLASS (class);
	source_config_class->get_backend_extension_name =
		book_source_config_get_backend_extension_name;
	source_config_class->list_eligible_collections =
		book_source_config_list_eligible_collections;
	source_config_class->init_candidate = book_source_config_init_candidate;
	source_config_class->commit_changes = book_source_config_commit_changes;
}

static void
e_book_source_config_init (EBookSourceConfig *config)
{
	config->priv = e_book_source_config_get_instance_private (config);
}

GtkWidget *
e_book_source_config_new (ESourceRegistry *registry,
                          ESource *original_source)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (original_source != NULL)
		g_return_val_if_fail (E_IS_SOURCE (original_source), NULL);

	return g_object_new (
		E_TYPE_BOOK_SOURCE_CONFIG, "registry", registry,
		"original-source", original_source, NULL);
}

void
e_book_source_config_add_offline_toggle (EBookSourceConfig *config,
                                         ESource *scratch_source)
{
	GtkWidget *widget;
	ESourceExtension *extension;
	const gchar *extension_name;

	g_return_if_fail (E_IS_BOOK_SOURCE_CONFIG (config));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	extension_name = E_SOURCE_EXTENSION_OFFLINE;
	extension = e_source_get_extension (scratch_source, extension_name);

	widget = gtk_check_button_new_with_label (
		_("Copy book content locally for offline operation"));
	e_source_config_insert_widget (
		E_SOURCE_CONFIG (config), scratch_source, NULL, widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "stay-synchronized",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}
