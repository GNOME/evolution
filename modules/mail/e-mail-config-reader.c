/*
 * e-mail-config-reader.c
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

#include "e-mail-config-reader.h"

#include <libebackend/libebackend.h>

#include <shell/e-shell.h>
#include <mail/e-mail-reader.h>

static gpointer parent_class;

static gboolean
mail_config_reader_idle_cb (EExtension *extension)
{
	EExtensible *extensible;
	GtkActionGroup *action_group;
	EShellSettings *shell_settings;
	EShell *shell;

	extensible = e_extension_get_extensible (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	g_object_bind_property (
		shell_settings, "mail-forward-style",
		extensible, "forward-style",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-reply-style",
		extensible, "reply-style",
		G_BINDING_SYNC_CREATE);

	action_group = e_mail_reader_get_action_group (
		E_MAIL_READER (extensible),
		E_MAIL_READER_ACTION_GROUP_SEARCH_FOLDERS);

	g_object_bind_property (
		shell_settings, "mail-enable-search-folders",
		action_group, "visible",
		G_BINDING_SYNC_CREATE);

	return FALSE;
}

static void
mail_config_reader_constructed (GObject *object)
{
	/* Bind properties to settings from an idle callback so the
	 * EMailReader interface has a chance to be initialized first. */
	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		(GSourceFunc) mail_config_reader_idle_cb,
		g_object_ref (object),
		(GDestroyNotify) g_object_unref);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
mail_config_reader_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_reader_constructed;

	class->extensible_type = E_TYPE_MAIL_READER;
}

void
e_mail_config_reader_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EExtensionClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_config_reader_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EExtension),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"EMailConfigReader", &type_info, 0);
}
