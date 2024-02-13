/*
 * e-settings-mail-browser.c
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

#include "e-settings-mail-browser.h"

#include <mail/e-mail-browser.h>

G_DEFINE_DYNAMIC_TYPE (ESettingsMailBrowser, e_settings_mail_browser, E_TYPE_EXTENSION)

static void
settings_mail_browser_constructed (GObject *object)
{
	EExtensible *extensible;
	GSettings *settings;

	/* Chain up parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_mail_browser_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* This preference is selected directly from the mail
	 * browser window, so the binding must be bi-directional. */
	g_settings_bind (
		settings, "browser-close-on-reply-policy",
		extensible, "close-on-reply-policy",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_SET);

	g_settings_bind (
		settings, "show-deleted",
		extensible, "show-deleted",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "show-junk",
		extensible, "show-junk",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "browser-close-on-delete-or-junk",
		extensible, "close-on-delete-or-junk",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);
}

static void
e_settings_mail_browser_class_init (ESettingsMailBrowserClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_mail_browser_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_BROWSER;
}

static void
e_settings_mail_browser_class_finalize (ESettingsMailBrowserClass *class)
{
}

static void
e_settings_mail_browser_init (ESettingsMailBrowser *extension)
{
}

void
e_settings_mail_browser_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_mail_browser_register_type (type_module);
}

