/*
 * e-settings-mail-reader.c
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

#include "evolution-config.h"

#include "e-settings-mail-reader.h"

#include <shell/e-shell.h>
#include <mail/e-mail-reader.h>

struct _ESettingsMailReaderPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsMailReader, e_settings_mail_reader, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsMailReader))

static gboolean
settings_mail_reader_idle_cb (EExtension *extension)
{
	EExtensible *extensible;
	EUIManager *ui_manager;
	EUIActionGroup *action_group;
	ESourceRegistry *registry;
	GSettings *settings;
	ESource *source;
	EShell *shell;

	extensible = e_extension_get_extensible (extension);
	if (!extensible)
		return FALSE;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "forward-style-name",
		extensible, "forward-style",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "reply-style-name",
		extensible, "reply-style",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "mark-seen-always",
		extensible, "mark-seen-always",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "delete-selects-previous",
		extensible, "delete-selects-previous",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	ui_manager = e_mail_reader_get_ui_manager (E_MAIL_READER (extensible));
	action_group = e_ui_manager_get_action_group (ui_manager, "search-folders");

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	source = e_source_registry_ref_source (registry, "vfolder");

	e_binding_bind_property (
		source, "enabled",
		action_group, "visible",
		G_BINDING_SYNC_CREATE);

	g_object_unref (source);

	return FALSE;
}

static void
settings_mail_reader_constructed (GObject *object)
{
	/* Bind properties to settings from an idle callback so the
	 * EMailReader interface has a chance to be initialized first.
	 * Prioritize ahead of GTK+ redraws. */
	g_idle_add_full (
		G_PRIORITY_HIGH_IDLE,
		(GSourceFunc) settings_mail_reader_idle_cb,
		g_object_ref (object),
		(GDestroyNotify) g_object_unref);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_mail_reader_parent_class)->constructed (object);
}

static void
e_settings_mail_reader_class_init (ESettingsMailReaderClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_mail_reader_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_READER;
}

static void
e_settings_mail_reader_class_finalize (ESettingsMailReaderClass *class)
{
}

static void
e_settings_mail_reader_init (ESettingsMailReader *extension)
{
	extension->priv = e_settings_mail_reader_get_instance_private (extension);
}

void
e_settings_mail_reader_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_mail_reader_register_type (type_module);
}

