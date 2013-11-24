/*
 * e-settings-mail-session.c
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

#include "e-settings-mail-session.h"

#include <mail/e-mail-ui-session.h>

#define E_SETTINGS_MAIL_SESSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_MAIL_SESSION, ESettingsMailSessionPrivate))

struct _ESettingsMailSessionPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsMailSession,
	e_settings_mail_session,
	E_TYPE_EXTENSION)

static void
settings_mail_session_constructed (GObject *object)
{
	EExtensible *extensible;
	GSettings *settings;

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	settings = g_settings_new ("org.gnome.evolution.mail");

	if (E_IS_MAIL_UI_SESSION (extensible)) {
		g_settings_bind (
			settings, "junk-check-incoming",
			extensible, "check-junk",
			G_SETTINGS_BIND_DEFAULT);
	}

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_mail_session_parent_class)->
		constructed (object);
}

static void
e_settings_mail_session_class_init (ESettingsMailSessionClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsMailSessionPrivate));

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
	extension->priv = E_SETTINGS_MAIL_SESSION_GET_PRIVATE (extension);
}

void
e_settings_mail_session_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_mail_session_register_type (type_module);
}

