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

#include "e-mail-config-web-view.h"

#include <shell/e-shell.h>
#include <e-util/e-binding.h>
#include <e-util/e-extension.h>
#include <misc/e-web-view.h>

static void
mail_config_web_view_realize (GtkWidget *widget)
{
	EShell *shell;
	EShellSettings *shell_settings;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	e_binding_new (
		shell_settings, "mail-show-animated-images",
		widget, "animate");

	e_binding_new (
		shell_settings, "composer-inline-spelling",
		widget, "inline-spelling");

	e_binding_new (
		shell_settings, "composer-magic-links",
		widget, "magic-links");

	e_binding_new (
		shell_settings, "composer-magic-smileys",
		widget, "magic-smileys");
}

static void
mail_config_web_view_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	/* Wait to bind shell settings until the EWebView is realized
	 * so GtkhtmlEditor has a chance to install a GtkHTMLEditorAPI.
	 * Otherwise our settings will have no effect. */

	g_signal_connect (
		extensible, "realize",
		G_CALLBACK (mail_config_web_view_realize), NULL);
}

static void
mail_config_web_view_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_web_view_constructed;

	class->extensible_type = E_TYPE_WEB_VIEW;
}

void
e_mail_config_web_view_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EExtensionClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_config_web_view_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EExtension),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"EMailConfigWebView", &type_info, 0);
}
