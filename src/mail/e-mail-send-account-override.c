/*
 * e-mail-send-account-override.c
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
 *
 * Copyright (C) 2013 Red Hat, Inc. (www.redhat.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <libedataserver/libedataserver.h>

#include "e-mail-send-account-override.h"

#define FOLDERS_SECTION				"Folders"
#define FOLDERS_ALIAS_NAME_SECTION		"Folders-Alias-Name"
#define FOLDERS_ALIAS_ADDRESS_SECTION		"Folders-Alias-Address"
#define RECIPIENTS_SECTION			"Recipients"
#define RECIPIENTS_ALIAS_NAME_SECTION		"Recipients-Alias-Name"
#define RECIPIENTS_ALIAS_ADDRESS_SECTION	"Recipients-Alias-Address"
#define OPTIONS_SECTION				"Options"

#define OPTION_PREFER_FOLDER			"PreferFolder"

struct _EMailSendAccountOverridePrivate {
	GKeyFile *key_file;
	gchar *config_filename;
	gboolean prefer_folder;

	gboolean need_save;
	guint save_frozen;

	GMutex property_lock;
};

enum {
	PROP_0,
	PROP_PREFER_FOLDER
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailSendAccountOverride, e_mail_send_account_override, G_TYPE_OBJECT)

static gboolean
e_mail_send_account_override_save_locked (EMailSendAccountOverride *override)
{
	gchar *contents;
	GError *error = NULL;

	g_return_val_if_fail (override->priv->key_file != NULL, FALSE);

	override->priv->need_save = FALSE;

	if (override->priv->config_filename == NULL)
		return FALSE;

	contents = g_key_file_to_data (override->priv->key_file, NULL, NULL);
	if (contents == NULL)
		return FALSE;

	g_file_set_contents (
		override->priv->config_filename, contents, -1, &error);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}

	g_free (contents);

	return TRUE;
}

static gboolean
e_mail_send_account_override_maybe_save_locked (EMailSendAccountOverride *override)
{
	if (override->priv->save_frozen > 0) {
		override->priv->need_save = TRUE;
		return FALSE;
	}

	return e_mail_send_account_override_save_locked (override);
}

static void
read_alias_info_locked (EMailSendAccountOverride *override,
			const gchar *alias_name_section,
			const gchar *alias_address_section,
			const gchar *key,
			gchar **out_alias_name,
			gchar **out_alias_address)
{
	if (out_alias_name) {
		gchar *alias_name;

		alias_name = g_key_file_get_string (
			override->priv->key_file, alias_name_section, key, NULL);

		if (alias_name)
			g_strchomp (alias_name);

		if (alias_name && !*alias_name) {
			g_free (alias_name);
			alias_name = NULL;
		}

		*out_alias_name = alias_name;
	}

	if (out_alias_address) {
		gchar *alias_address;

		alias_address = g_key_file_get_string (
			override->priv->key_file, alias_address_section, key, NULL);

		if (alias_address)
			g_strchomp (alias_address);

		if (alias_address && !*alias_address) {
			g_free (alias_address);
			alias_address = NULL;
		}

		*out_alias_address = alias_address;
	}
}

static void
write_alias_info_locked (EMailSendAccountOverride *override,
			 const gchar *alias_name_section,
			 const gchar *alias_address_section,
			 const gchar *key,
			 const gchar *alias_name,
			 const gchar *alias_address)
{
	if (alias_name && *alias_name) {
		g_key_file_set_string (override->priv->key_file, alias_name_section, key, alias_name);
	} else {
		g_key_file_remove_key (override->priv->key_file, alias_name_section, key, NULL);
	}

	if (alias_address && *alias_address) {
		g_key_file_set_string (override->priv->key_file, alias_address_section, key, alias_address);
	} else {
		g_key_file_remove_key (override->priv->key_file, alias_address_section, key, NULL);
	}
}

static gchar *
get_override_for_folder_uri_locked (EMailSendAccountOverride *override,
                                    const gchar *folder_uri,
				    gchar **out_alias_name,
				    gchar **out_alias_address)
{
	gchar *account_uid;

	if (folder_uri == NULL || *folder_uri == '\0')
		return NULL;

	account_uid = g_key_file_get_string (
		override->priv->key_file, FOLDERS_SECTION, folder_uri, NULL);

	if (account_uid != NULL)
		g_strchomp (account_uid);

	if (account_uid != NULL && *account_uid == '\0') {
		g_free (account_uid);
		account_uid = NULL;
	}

	if (account_uid) {
		read_alias_info_locked (override,
			FOLDERS_ALIAS_NAME_SECTION,
			FOLDERS_ALIAS_ADDRESS_SECTION,
			folder_uri,
			out_alias_name,
			out_alias_address);
	}

	return account_uid;
}

static gchar *
test_one_recipient (gchar **keys,
                    GPtrArray *values,
                    const gchar *name,
                    const gchar *address,
		    gint *out_keys_index)
{
	gint ii;

	g_return_val_if_fail (keys != NULL, NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (out_keys_index != NULL, NULL);

	if (name != NULL && *name == '\0')
		name = NULL;

	if (address != NULL && *address == '\0')
		address = NULL;

	if (name == NULL && address == NULL)
		return NULL;

	for (ii = 0; keys[ii] && ii < values->len; ii++) {
		if (name != NULL && e_util_utf8_strstrcase (name, keys[ii]) != NULL) {
			*out_keys_index = ii;

			return g_strdup (values->pdata[ii]);
		}

		if (address != NULL && e_util_utf8_strstrcase (address, keys[ii]) != NULL) {
			*out_keys_index = ii;

			return g_strdup (values->pdata[ii]);
		}
	}

	return NULL;
}

static gchar *
get_override_for_recipients_locked (EMailSendAccountOverride *override,
                                    CamelAddress *recipients,
				    gchar **out_alias_name,
				    gchar **out_alias_address)
{
	CamelInternetAddress *iaddress;
	gchar *account_uid = NULL;
	GPtrArray *values;
	gchar **keys;
	gint ii, len;

	if (!CAMEL_IS_INTERNET_ADDRESS (recipients))
		return NULL;

	keys = g_key_file_get_keys (
		override->priv->key_file, RECIPIENTS_SECTION, NULL, NULL);
	if (keys == NULL)
		return NULL;

	values = g_ptr_array_new_full (g_strv_length (keys), g_free);
	for (ii = 0; keys[ii]; ii++) {
		g_ptr_array_add (
			values,
			g_key_file_get_string (
				override->priv->key_file,
				RECIPIENTS_SECTION, keys[ii], NULL));
	}

	iaddress = CAMEL_INTERNET_ADDRESS (recipients);
	len = camel_address_length (recipients);
	for (ii = 0; ii < len; ii++) {
		const gchar *name, *address;

		if (camel_internet_address_get (iaddress, ii, &name, &address)) {
			gint keys_index = -1;

			account_uid = test_one_recipient (keys, values, name, address, &keys_index);

			if (account_uid != NULL)
				g_strchomp (account_uid);

			if (account_uid != NULL && *account_uid == '\0') {
				g_free (account_uid);
				account_uid = NULL;
			}

			if (account_uid != NULL) {
				g_warn_if_fail (keys_index >= 0 && keys_index < g_strv_length (keys));
				read_alias_info_locked (override,
					RECIPIENTS_ALIAS_NAME_SECTION,
					RECIPIENTS_ALIAS_ADDRESS_SECTION,
					keys[keys_index],
					out_alias_name,
					out_alias_address);
				break;
			}
		}
	}

	g_ptr_array_free (values, TRUE);
	g_strfreev (keys);

	return account_uid;
}

static void
list_overrides_section_for_account_locked (EMailSendAccountOverride *override,
                                           const gchar *account_uid,
					   const gchar *alias_name,
					   const gchar *alias_address,
                                           const gchar *section,
					   const gchar *alias_name_section,
					   const gchar *alias_address_section,
                                           GList **overrides)
{
	gchar **keys;

	g_return_if_fail (account_uid != NULL);
	g_return_if_fail (section != NULL);

	if (overrides == NULL)
		return;

	*overrides = NULL;

	keys = g_key_file_get_keys (
		override->priv->key_file, section, NULL, NULL);
	if (keys != NULL) {
		gint ii;

		for (ii = 0; keys[ii]; ii++) {
			const gchar *key = keys[ii];
			gchar *value;

			value = g_key_file_get_string (
				override->priv->key_file, section, key, NULL);
			if (g_strcmp0 (value, account_uid) == 0) {
				gchar *stored_alias_name = NULL, *stored_alias_address = NULL;

				read_alias_info_locked (override, alias_name_section, alias_address_section,
					key, &stored_alias_name, &stored_alias_address);

				if (g_strcmp0 (stored_alias_name, alias_name) == 0 &&
				    g_strcmp0 (stored_alias_address, alias_address) == 0) {
					*overrides = g_list_prepend (*overrides, g_strdup (key));
				}

				g_free (stored_alias_name);
				g_free (stored_alias_address);
			}
			g_free (value);
		}
	}

	g_strfreev (keys);

	*overrides = g_list_reverse (*overrides);
}

static void
list_overrides_for_account_locked (EMailSendAccountOverride *override,
                                   const gchar *account_uid,
				   const gchar *alias_name,
				   const gchar *alias_address,
                                   GList **folder_overrides,
                                   GList **recipient_overrides)
{
	if (account_uid == NULL)
		return;

	list_overrides_section_for_account_locked (
		override, account_uid, alias_name, alias_address, FOLDERS_SECTION, FOLDERS_ALIAS_NAME_SECTION,
		FOLDERS_ALIAS_ADDRESS_SECTION, folder_overrides);

	list_overrides_section_for_account_locked (
		override, account_uid, alias_name, alias_address, RECIPIENTS_SECTION, RECIPIENTS_ALIAS_NAME_SECTION,
		RECIPIENTS_ALIAS_ADDRESS_SECTION, recipient_overrides);
}

static void
mail_send_account_override_set_property (GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_FOLDER:
			e_mail_send_account_override_set_prefer_folder (
				E_MAIL_SEND_ACCOUNT_OVERRIDE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_send_account_override_get_property (GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_FOLDER:
			g_value_set_boolean (
				value,
				e_mail_send_account_override_get_prefer_folder (
				E_MAIL_SEND_ACCOUNT_OVERRIDE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_send_account_override_finalize (GObject *object)
{
	EMailSendAccountOverride *self = E_MAIL_SEND_ACCOUNT_OVERRIDE (object);

	g_key_file_free (self->priv->key_file);
	g_free (self->priv->config_filename);

	g_mutex_clear (&self->priv->property_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_send_account_override_parent_class)->finalize (object);
}

static void
e_mail_send_account_override_class_init (EMailSendAccountOverrideClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_send_account_override_set_property;
	object_class->get_property = mail_send_account_override_get_property;
	object_class->finalize = mail_send_account_override_finalize;

	g_object_class_install_property (
		object_class,
		PROP_PREFER_FOLDER,
		g_param_spec_boolean (
			"prefer-folder",
			"Prefer Folder",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailSendAccountOverrideClass, changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

static void
e_mail_send_account_override_init (EMailSendAccountOverride *override)
{
	override->priv = e_mail_send_account_override_get_instance_private (override);

	g_mutex_init (&override->priv->property_lock);
	override->priv->key_file = g_key_file_new ();
	override->priv->prefer_folder = TRUE;
}

EMailSendAccountOverride *
e_mail_send_account_override_new (const gchar *config_filename)
{
	EMailSendAccountOverride *override;

	override = g_object_new (E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE, NULL);

	if (config_filename != NULL)
		e_mail_send_account_override_set_config_filename (
			override, config_filename);

	return override;
}

void
e_mail_send_account_override_set_config_filename (EMailSendAccountOverride *override,
                                                  const gchar *config_filename)
{
	GError *error = NULL;
	gboolean old_prefer_folder, prefer_folder_changed;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (config_filename != NULL);
	g_return_if_fail (*config_filename != '\0');

	g_mutex_lock (&override->priv->property_lock);

	if (g_strcmp0 (config_filename, override->priv->config_filename) == 0) {
		g_mutex_unlock (&override->priv->property_lock);
		return;
	}

	g_free (override->priv->config_filename);
	override->priv->config_filename = g_strdup (config_filename);

	g_key_file_load_from_file (
		override->priv->key_file,
		override->priv->config_filename, G_KEY_FILE_NONE, NULL);

	old_prefer_folder = override->priv->prefer_folder;
	override->priv->prefer_folder = g_key_file_get_boolean (
		override->priv->key_file,
		OPTIONS_SECTION, OPTION_PREFER_FOLDER, &error);

	if (error != NULL) {
		/* default value is to prefer the folder override over the recipients */
		override->priv->prefer_folder = TRUE;
		g_clear_error (&error);
	}

	prefer_folder_changed =
		(override->priv->prefer_folder != old_prefer_folder);

	g_mutex_unlock (&override->priv->property_lock);

	if (prefer_folder_changed)
		g_object_notify (G_OBJECT (override), "prefer-folder");
}

gchar *
e_mail_send_account_override_dup_config_filename (EMailSendAccountOverride *override)
{
	gchar *config_filename;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override), NULL);

	g_mutex_lock (&override->priv->property_lock);
	config_filename = g_strdup (override->priv->config_filename);
	g_mutex_unlock (&override->priv->property_lock);

	return config_filename;
}

void
e_mail_send_account_override_set_prefer_folder (EMailSendAccountOverride *override,
                                                gboolean prefer_folder)
{
	gboolean changed, saved = FALSE;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));

	g_mutex_lock (&override->priv->property_lock);

	changed = (prefer_folder != override->priv->prefer_folder);

	if (changed) {
		override->priv->prefer_folder = prefer_folder;

		g_key_file_set_boolean (
			override->priv->key_file,
			OPTIONS_SECTION, OPTION_PREFER_FOLDER, prefer_folder);

		saved = e_mail_send_account_override_maybe_save_locked (override);
	}

	g_mutex_unlock (&override->priv->property_lock);

	if (changed)
		g_object_notify (G_OBJECT (override), "prefer-folder");
	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

gboolean
e_mail_send_account_override_get_prefer_folder (EMailSendAccountOverride *override)
{
	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override), FALSE);

	return override->priv->prefer_folder;
}

/* free returned pointers with g_free() */
gchar *
e_mail_send_account_override_get_account_uid (EMailSendAccountOverride *override,
                                              const gchar *folder_uri,
                                              CamelInternetAddress *recipients_to,
                                              CamelInternetAddress *recipients_cc,
                                              CamelInternetAddress *recipients_bcc,
					      gchar **out_alias_name,
					      gchar **out_alias_address)
{
	gchar *account_uid = NULL;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override), NULL);
	g_return_val_if_fail (override->priv->config_filename != NULL, NULL);

	g_mutex_lock (&override->priv->property_lock);

	if (override->priv->prefer_folder)
		account_uid = get_override_for_folder_uri_locked (
			override, folder_uri, out_alias_name, out_alias_address);

	if (account_uid == NULL)
		account_uid = get_override_for_recipients_locked (
			override, CAMEL_ADDRESS (recipients_to), out_alias_name, out_alias_address);

	if (account_uid == NULL)
		account_uid = get_override_for_recipients_locked (
			override, CAMEL_ADDRESS (recipients_cc), out_alias_name, out_alias_address);

	if (account_uid == NULL)
		account_uid = get_override_for_recipients_locked (
			override, CAMEL_ADDRESS (recipients_bcc), out_alias_name, out_alias_address);

	if (account_uid == NULL && !override->priv->prefer_folder)
		account_uid = get_override_for_folder_uri_locked (
			override, folder_uri, out_alias_name, out_alias_address);

	g_mutex_unlock (&override->priv->property_lock);

	return account_uid;
}

void
e_mail_send_account_override_remove_for_account_uid (EMailSendAccountOverride *override,
                                                     const gchar *account_uid,
						     const gchar *alias_name,
						     const gchar *alias_address)
{
	GList *folders = NULL, *recipients = NULL;
	gboolean saved = FALSE;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&override->priv->property_lock);

	list_overrides_for_account_locked (
		override, account_uid, alias_name, alias_address, &folders, &recipients);

	if (folders != NULL || recipients != NULL) {
		GList *link;

		for (link = folders; link != NULL; link = g_list_next (link)) {
			const gchar *key = link->data;

			g_key_file_remove_key (
				override->priv->key_file,
				FOLDERS_SECTION, key, NULL);

			write_alias_info_locked (override, FOLDERS_ALIAS_NAME_SECTION,
				FOLDERS_ALIAS_ADDRESS_SECTION, key, NULL, NULL);
		}

		for (link = recipients; link != NULL; link = g_list_next (link)) {
			const gchar *key = link->data;

			g_key_file_remove_key (
				override->priv->key_file,
				RECIPIENTS_SECTION, key, NULL);

			write_alias_info_locked (override, RECIPIENTS_ALIAS_NAME_SECTION,
				RECIPIENTS_ALIAS_ADDRESS_SECTION, key, NULL, NULL);
		}

		saved = e_mail_send_account_override_maybe_save_locked (override);
	}

	g_list_free_full (folders, g_free);
	g_list_free_full (recipients, g_free);

	g_mutex_unlock (&override->priv->property_lock);

	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

gchar *
e_mail_send_account_override_get_for_folder (EMailSendAccountOverride *override,
                                             const gchar *folder_uri,
					     gchar **out_alias_name,
					     gchar **out_alias_address)
{
	gchar *account_uid = NULL;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override), NULL);

	g_mutex_lock (&override->priv->property_lock);

	account_uid = get_override_for_folder_uri_locked (override, folder_uri, out_alias_name, out_alias_address);

	g_mutex_unlock (&override->priv->property_lock);

	return account_uid;
}

void
e_mail_send_account_override_set_for_folder (EMailSendAccountOverride *override,
                                             const gchar *folder_uri,
                                             const gchar *account_uid,
					     const gchar *alias_name,
					     const gchar *alias_address)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&override->priv->property_lock);

	g_key_file_set_string (
		override->priv->key_file,
		FOLDERS_SECTION, folder_uri, account_uid);

	write_alias_info_locked (override, FOLDERS_ALIAS_NAME_SECTION,
		FOLDERS_ALIAS_ADDRESS_SECTION, folder_uri, alias_name, alias_address);

	saved = e_mail_send_account_override_maybe_save_locked (override);

	g_mutex_unlock (&override->priv->property_lock);

	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

void
e_mail_send_account_override_remove_for_folder (EMailSendAccountOverride *override,
                                                const gchar *folder_uri)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (folder_uri != NULL);

	g_mutex_lock (&override->priv->property_lock);

	g_key_file_remove_key (
		override->priv->key_file, FOLDERS_SECTION, folder_uri, NULL);

	write_alias_info_locked (override, FOLDERS_ALIAS_NAME_SECTION,
		FOLDERS_ALIAS_ADDRESS_SECTION, folder_uri, NULL, NULL);

	saved = e_mail_send_account_override_maybe_save_locked (override);

	g_mutex_unlock (&override->priv->property_lock);

	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

gchar *
e_mail_send_account_override_get_for_recipient (EMailSendAccountOverride *override,
                                                CamelInternetAddress *recipients,
						gchar **out_alias_name,
						gchar **out_alias_address)
{
	gchar *account_uid;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override), NULL);
	g_return_val_if_fail (recipients != NULL, NULL);

	g_mutex_lock (&override->priv->property_lock);

	account_uid = get_override_for_recipients_locked (
		override, CAMEL_ADDRESS (recipients), out_alias_name, out_alias_address);

	g_mutex_unlock (&override->priv->property_lock);

	return account_uid;
}

void
e_mail_send_account_override_set_for_recipient (EMailSendAccountOverride *override,
                                                const gchar *recipient,
                                                const gchar *account_uid,
						const gchar *alias_name,
						const gchar *alias_address)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (recipient != NULL);
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&override->priv->property_lock);

	g_key_file_set_string (
		override->priv->key_file,
		RECIPIENTS_SECTION, recipient, account_uid);

	write_alias_info_locked (override, RECIPIENTS_ALIAS_NAME_SECTION,
		RECIPIENTS_ALIAS_ADDRESS_SECTION, recipient, alias_name, alias_address);

	saved = e_mail_send_account_override_maybe_save_locked (override);

	g_mutex_unlock (&override->priv->property_lock);

	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

void
e_mail_send_account_override_remove_for_recipient (EMailSendAccountOverride *override,
                                                   const gchar *recipient)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (recipient != NULL);

	g_mutex_lock (&override->priv->property_lock);

	g_key_file_remove_key (
		override->priv->key_file, RECIPIENTS_SECTION, recipient, NULL);

	write_alias_info_locked (override, RECIPIENTS_ALIAS_NAME_SECTION,
		RECIPIENTS_ALIAS_ADDRESS_SECTION, recipient, NULL, NULL);

	saved = e_mail_send_account_override_maybe_save_locked (override);

	g_mutex_unlock (&override->priv->property_lock);

	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

void
e_mail_send_account_override_list_for_account (EMailSendAccountOverride *override,
                                               const gchar *account_uid,
					       const gchar *alias_name,
					       const gchar *alias_address,
                                               GList **folder_overrides,
                                               GList **recipient_overrides)
{
	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&override->priv->property_lock);

	list_overrides_for_account_locked (
		override, account_uid, alias_name, alias_address, folder_overrides, recipient_overrides);

	g_mutex_unlock (&override->priv->property_lock);
}

void
e_mail_send_account_override_freeze_save (EMailSendAccountOverride *override)
{
	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));

	g_mutex_lock (&override->priv->property_lock);

	override->priv->save_frozen++;
	if (!override->priv->save_frozen) {
		g_warn_if_reached ();
	}

	g_mutex_unlock (&override->priv->property_lock);
}

void
e_mail_send_account_override_thaw_save (EMailSendAccountOverride *override)
{
	gboolean saved = FALSE;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (override));

	g_mutex_lock (&override->priv->property_lock);

	if (!override->priv->save_frozen) {
		g_warn_if_reached ();
	} else {
		override->priv->save_frozen--;
		if (!override->priv->save_frozen && override->priv->need_save)
			saved = e_mail_send_account_override_save_locked (override);
	}

	g_mutex_unlock (&override->priv->property_lock);

	if (saved)
		g_signal_emit (override, signals[CHANGED], 0);
}

