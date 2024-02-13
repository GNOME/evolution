/*
 * e-settings-spell-checker.c
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

#include "e-settings-spell-checker.h"

#include <e-util/e-util.h>

struct _ESettingsSpellCheckerPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsSpellChecker, e_settings_spell_checker, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsSpellChecker))

static ESpellChecker *
settings_spell_checker_get_extensible (ESettingsSpellChecker *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SPELL_CHECKER (extensible);
}

static void
settings_spell_checker_constructed (GObject *object)
{
	ESpellChecker *spell_checker;
	GSettings *settings;
	gchar **strv;
	guint ii;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_spell_checker_parent_class)->constructed (object);

	/* This only initializes the active spell languages, it does not
	 * write changes back to GSettings.  Only the ESpellChecker used
	 * in Composer Preferences should be doing that. */

	spell_checker = settings_spell_checker_get_extensible (
		E_SETTINGS_SPELL_CHECKER (object));

	/* Make sure there are no active languages at this point. */
	g_warn_if_fail (e_spell_checker_count_active_languages (spell_checker) == 0);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	strv = g_settings_get_strv (settings, "composer-spell-languages");
	g_object_unref (settings);

	g_return_if_fail (strv != NULL);

	for (ii = 0; strv[ii] != NULL; ii++)
		e_spell_checker_set_language_active (
			spell_checker, strv[ii], TRUE);

	g_strfreev (strv);
}

static void
e_settings_spell_checker_class_init (ESettingsSpellCheckerClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_spell_checker_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SPELL_CHECKER;
}

static void
e_settings_spell_checker_class_finalize (ESettingsSpellCheckerClass *class)
{
}

static void
e_settings_spell_checker_init (ESettingsSpellChecker *extension)
{
	extension->priv = e_settings_spell_checker_get_instance_private (extension);
}

void
e_settings_spell_checker_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_spell_checker_register_type (type_module);
}

