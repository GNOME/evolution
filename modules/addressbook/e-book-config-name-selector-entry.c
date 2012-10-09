/*
 * e-book-config-name-selector-entry.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-book-config-name-selector-entry.h"

#include <libedataserverui/libedataserverui.h>

#define E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_CONFIG_NAME_SELECTOR_ENTRY, EBookConfigNameSelectorEntryPrivate))

struct _EBookConfigNameSelectorEntryPrivate {
	GSettings *settings;
};

G_DEFINE_DYNAMIC_TYPE (
	EBookConfigNameSelectorEntry,
	e_book_config_name_selector_entry,
	E_TYPE_EXTENSION)

static void
book_config_name_selector_entry_dispose (GObject *object)
{
	EBookConfigNameSelectorEntryPrivate *priv;

	priv = E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_GET_PRIVATE (object);

	if (priv->settings != NULL) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_config_name_selector_entry_parent_class)->
		dispose (object);
}

static void
book_config_name_selector_entry_constructed (GObject *object)
{
	EBookConfigNameSelectorEntry *extension;
	EExtensible *extensible;

	extension = E_BOOK_CONFIG_NAME_SELECTOR_ENTRY (object);
	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	/* Chain up to parent's consturcted() method. */
	G_OBJECT_CLASS (e_book_config_name_selector_entry_parent_class)->
		constructed (object);

	g_settings_bind (
		extension->priv->settings, "completion-minimum-query-length",
		extensible, "minimum-query-length",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_settings_bind (
		extension->priv->settings, "completion-show-address",
		extensible, "show-address",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
}

static void
e_book_config_name_selector_entry_class_init (EBookConfigNameSelectorEntryClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (
		class, sizeof (EBookConfigNameSelectorEntryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_config_name_selector_entry_dispose;
	object_class->constructed = book_config_name_selector_entry_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_NAME_SELECTOR_ENTRY;
}

static void
e_book_config_name_selector_entry_class_finalize (EBookConfigNameSelectorEntryClass *class)
{
}

static void
e_book_config_name_selector_entry_init (EBookConfigNameSelectorEntry *extension)
{
	extension->priv =
		E_BOOK_CONFIG_NAME_SELECTOR_ENTRY_GET_PRIVATE (extension);
	extension->priv->settings =
		g_settings_new ("org.gnome.evolution.addressbook");
}

void
e_book_config_name_selector_entry_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_book_config_name_selector_entry_register_type (type_module);
}

