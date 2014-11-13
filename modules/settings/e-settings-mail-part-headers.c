/*
 * e-settings-mail-part-headers.c
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

#include "e-settings-mail-part-headers.h"

#include <em-format/e-mail-part-headers.h>

G_DEFINE_DYNAMIC_TYPE (
	ESettingsMailPartHeaders,
	e_settings_mail_part_headers,
	E_TYPE_EXTENSION)

static EMailPartHeaders *
settings_mail_part_headers_get_extensible (ESettingsMailPartHeaders *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_MAIL_PART_HEADERS (extensible);
}

static gboolean
settings_mail_part_headers_get_mapping (GValue *value,
                                        GVariant *variant,
                                        gpointer user_data)
{
	gchar **strv;
	gsize ii, n_children;
	guint index = 0;

	/* Form a string array of enabled header names. */

	n_children = g_variant_n_children (variant);

	strv = g_new0 (gchar *, n_children + 1);

	for (ii = 0; ii < n_children; ii++) {
		const gchar *name = NULL;
		gboolean enabled = FALSE;

		g_variant_get_child (variant, ii, "(&sb)", &name, &enabled);

		if (enabled && name != NULL)
			strv[index++] = g_strdup (name);
	}

	g_value_take_boxed (value, strv);

	return TRUE;
}

static void
settings_mail_part_headers_constructed (GObject *object)
{
	ESettingsMailPartHeaders *extension;
	EMailPartHeaders *part;
	GSettings *settings;

	extension = E_SETTINGS_MAIL_PART_HEADERS (object);
	part = settings_mail_part_headers_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind_with_mapping (
		settings, "show-headers",
		part, "default-headers",
		G_SETTINGS_BIND_GET,
		settings_mail_part_headers_get_mapping,
		(GSettingsBindSetMapping) NULL,
		NULL, (GDestroyNotify) NULL);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_mail_part_headers_parent_class)->constructed (object);
}

static void
e_settings_mail_part_headers_class_init (ESettingsMailPartHeadersClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_mail_part_headers_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PART_HEADERS;
}

static void
e_settings_mail_part_headers_class_finalize (ESettingsMailPartHeadersClass *class)
{
}

static void
e_settings_mail_part_headers_init (ESettingsMailPartHeaders *extension)
{
}

void
e_settings_mail_part_headers_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_mail_part_headers_register_type (type_module);
}

