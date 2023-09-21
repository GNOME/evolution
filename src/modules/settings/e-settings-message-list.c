/*
 * e-settings-message-list.c
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

#include "e-settings-message-list.h"

#include <mail/message-list.h>

G_DEFINE_DYNAMIC_TYPE (
	ESettingsMessageList,
	e_settings_message_list,
	E_TYPE_EXTENSION)

static MessageList *
settings_message_list_get_extensible (ESettingsMessageList *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return MESSAGE_LIST (extensible);
}

static void
settings_message_list_constructed (GObject *object)
{
	ESettingsMessageList *extension;
	MessageList *message_list;
	GSettings *settings;

	extension = E_SETTINGS_MESSAGE_LIST (object);
	message_list = settings_message_list_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "show-deleted",
		message_list, "show-deleted",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "show-junk",
		message_list, "show-junk",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "thread-latest",
		message_list, "thread-latest",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "thread-subject",
		message_list, "thread-subject",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "thread-children-ascending",
		message_list, "sort-children-ascending",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "thread-compress",
		message_list, "thread-compress",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "thread-flat",
		message_list, "thread-flat",
		G_SETTINGS_BIND_GET);

	/* This setting only controls the initial message list
	 * state when in threaded mode, so just apply it here. */
	message_list_set_expanded_default (
		message_list,
		g_settings_get_boolean (settings, "thread-expand"));

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_message_list_parent_class)->constructed (object);
}

static void
e_settings_message_list_class_init (ESettingsMessageListClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_message_list_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = MESSAGE_LIST_TYPE;
}

static void
e_settings_message_list_class_finalize (ESettingsMessageListClass *class)
{
}

static void
e_settings_message_list_init (ESettingsMessageList *extension)
{
}

void
e_settings_message_list_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_message_list_register_type (type_module);
}

