/*
 * e-mail-config-web-view.c
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

#include <stdio.h>
#include <string.h>

#include "e-mail-config-web-view.h"

#include <shell/e-shell.h>

#define E_MAIL_CONFIG_WEB_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_WEB_VIEW, EMailConfigWebViewPrivate))

struct _EMailConfigWebViewPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigWebView,
	e_mail_config_web_view,
	E_TYPE_EXTENSION)

static void
mail_config_web_view_constructed (GObject *object)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EExtensible *extensible;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	g_object_bind_property (
		shell_settings, "composer-inline-spelling",
		extensible, "inline-spelling",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "composer-magic-links",
		extensible, "magic-links",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "composer-magic-smileys",
		extensible, "magic-smileys",
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_web_view_parent_class)->
		constructed (object);
}

static void
e_mail_config_web_view_class_init (EMailConfigWebViewClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (EMailConfigWebViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_web_view_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_WEB_VIEW;
}

static void
e_mail_config_web_view_class_finalize (EMailConfigWebViewClass *class)
{
}

static void
e_mail_config_web_view_init (EMailConfigWebView *extension)
{
	extension->priv = E_MAIL_CONFIG_WEB_VIEW_GET_PRIVATE (extension);
}

void
e_mail_config_web_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_web_view_register_type (type_module);
}

