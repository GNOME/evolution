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

#include <libebackend/libebackend.h>
#include <libedataserverui/libedataserverui.h>

typedef struct _EBookConfigNameSelectorEntry EBookConfigNameSelectorEntry;
typedef struct _EBookConfigNameSelectorEntryClass EBookConfigNameSelectorEntryClass;

struct _EBookConfigNameSelectorEntry {
	EExtension parent;
	GSettings *settings;
};

struct _EBookConfigNameSelectorEntryClass {
	EExtensionClass parent_class;
};

static gpointer parent_class;

static void
book_config_name_selector_entry_realize (GtkWidget *widget,
					 EBookConfigNameSelectorEntry *extension)
{
	g_settings_bind (
		extension->settings, "completion-minimum-query-length",
		widget, "minimum-query-length",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_settings_bind (
		extension->settings, "completion-show-address",
		widget, "show-address",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
}

static void
book_config_name_selector_entry_dispose (GObject *object)
{
	EBookConfigNameSelectorEntry *extension;

	extension = (EBookConfigNameSelectorEntry *) object;

	if (extension->settings != NULL) {
		g_object_unref (extension->settings);
		extension->settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
book_config_name_selector_entry_constructed (GObject *object)
{
	EBookConfigNameSelectorEntry *extension;
	EExtensible *extensible;

	extension = (EBookConfigNameSelectorEntry *) object;
	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	extension->settings = g_settings_new ("org.gnome.evolution.addressbook");

	/* Wait to bind settings until the ENameSelectorEntry is realized */

	/*g_signal_connect (
		extensible, "realize",
		G_CALLBACK (book_config_name_selector_entry_realize), extension);*/

	/* Chain up to parent's consturcted() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	book_config_name_selector_entry_realize (extensible, extension);
}

static void
book_config_name_selector_entry_class_init (EBookConfigNameSelectorEntryClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_config_name_selector_entry_dispose;
	object_class->constructed = book_config_name_selector_entry_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_NAME_SELECTOR_ENTRY;
}

void
e_book_config_name_selector_entry_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EBookConfigNameSelectorEntryClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) book_config_name_selector_entry_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EBookConfigNameSelectorEntry),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"EBookConfigNameSelectorEntry", &type_info, 0);
}
