/*
 * e-settings-web-view.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "e-settings-web-view.h"

#include <e-util/e-util.h>

#define E_SETTINGS_WEB_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_WEB_VIEW, ESettingsWebViewPrivate))

struct _ESettingsWebViewPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsWebView,
	e_settings_web_view,
	E_TYPE_EXTENSION)

static void
settings_web_view_constructed (GObject *object)
{
	GSettings *settings;
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	settings = g_settings_new ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "composer-inline-spelling",
		extensible, "inline-spelling",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-magic-links",
		extensible, "magic-links",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-magic-smileys",
		extensible, "magic-smileys",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_web_view_parent_class)->constructed (object);
}

static void
e_settings_web_view_class_init (ESettingsWebViewClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsWebViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_web_view_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_WEB_VIEW;
}

static void
e_settings_web_view_class_finalize (ESettingsWebViewClass *class)
{
}

static void
e_settings_web_view_init (ESettingsWebView *extension)
{
	extension->priv = E_SETTINGS_WEB_VIEW_GET_PRIVATE (extension);
}

void
e_settings_web_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_web_view_register_type (type_module);
}

