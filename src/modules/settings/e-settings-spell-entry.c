/*
 * e-settings-spell-entry.c
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

#include "e-settings-spell-entry.h"

#include <e-util/e-util.h>

struct _ESettingsSpellEntryPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsSpellEntry, e_settings_spell_entry, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsSpellEntry))

static void
settings_spell_entry_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "composer-inline-spelling",
		extensible, "checking-enabled",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_spell_entry_parent_class)->constructed (object);
}

static void
e_settings_spell_entry_class_init (ESettingsSpellEntryClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_spell_entry_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SPELL_ENTRY;
}

static void
e_settings_spell_entry_class_finalize (ESettingsSpellEntryClass *class)
{
}

static void
e_settings_spell_entry_init (ESettingsSpellEntry *extension)
{
	extension->priv = e_settings_spell_entry_get_instance_private (extension);
}

void
e_settings_spell_entry_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_spell_entry_register_type (type_module);
}

