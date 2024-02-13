/*
 * e-settings-mail-session.c
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

#include "e-settings-mail-session.h"

#include <mail/e-mail-ui-session.h>

struct _ESettingsMailSessionPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsMailSession, e_settings_mail_session, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsMailSession))

static gboolean
settings_mail_session_name_to_junk_filter (GValue *value,
                                           GVariant *variant,
                                           gpointer user_data)
{
	const gchar *filter_name;

	filter_name = g_variant_get_string (variant, NULL);

	if (filter_name != NULL) {
		EMailJunkFilter *junk_filter;

		junk_filter = e_mail_session_get_junk_filter_by_name (E_MAIL_SESSION (user_data), filter_name);
		if (junk_filter && e_mail_junk_filter_available (E_MAIL_JUNK_FILTER (junk_filter)))
			g_value_set_object (value, junk_filter);
	}

	/* XXX Always return success, even if we cannot find a matching
	 *     EMailJunkFilter.  The default value is 'Bogofilter', but
	 *     if the Bogofilter module is not installed then GSettings
	 *     will actually abort the program.  Nice. */

	return TRUE;
}

static GVariant *
settings_mail_session_junk_filter_to_name (const GValue *value,
                                           const GVariantType *expected_type,
                                           gpointer user_data)
{
	EMailJunkFilter *junk_filter;
	GVariant *result = NULL;

	junk_filter = g_value_get_object (value);

	if (E_IS_MAIL_JUNK_FILTER (junk_filter)) {
		EMailJunkFilterClass *class;

		class = E_MAIL_JUNK_FILTER_GET_CLASS (junk_filter);
		result = g_variant_new_string (class->filter_name);
	}

	return result;
}

static gboolean
settings_mail_session_idle_cb (gpointer user_data)
{
	EMailSession *session;
	GSettings *settings;

	session = E_MAIL_SESSION (user_data);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* Need to delay the settings binding to give EMailSession a
	 * chance to set up its junk filter table.  The junk filters
	 * are also EMailSession extensions. */
	g_settings_bind_with_mapping (
		settings, "junk-default-plugin",
		session, "junk-filter",
		G_SETTINGS_BIND_DEFAULT,
		settings_mail_session_name_to_junk_filter,
		settings_mail_session_junk_filter_to_name,
		session, (GDestroyNotify) NULL);

	g_object_unref (settings);

	return G_SOURCE_REMOVE;
}

static void
settings_mail_session_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (E_IS_MAIL_UI_SESSION (extensible)) {
		g_settings_bind (
			settings, "junk-check-incoming",
			extensible, "check-junk",
			G_SETTINGS_BIND_DEFAULT);
	}

	g_object_unref (settings);

	g_idle_add_full (
		G_PRIORITY_HIGH_IDLE,
		settings_mail_session_idle_cb,
		g_object_ref (extensible),
		(GDestroyNotify) g_object_unref);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_mail_session_parent_class)->constructed (object);
}

static void
e_settings_mail_session_class_init (ESettingsMailSessionClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_mail_session_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_SESSION;
}

static void
e_settings_mail_session_class_finalize (ESettingsMailSessionClass *class)
{
}

static void
e_settings_mail_session_init (ESettingsMailSession *extension)
{
	extension->priv = e_settings_mail_session_get_instance_private (extension);
}

void
e_settings_mail_session_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_mail_session_register_type (type_module);
}

