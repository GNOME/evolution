/*
 * e-mail-send-account-override.c
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
 *
 * Copyright (C) 2013 Red Hat, Inc. (www.redhat.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libedataserver/libedataserver.h>

#include "e-mail-send-account-override.h"

#define FOLDERS_SECTION		"Folders"
#define RECIPIENTS_SECTION	"Recipients"
#define OPTIONS_SECTION		"Options"

#define OPTION_PREFER_FOLDER	"PreferFolder"

struct _EMailSendAccountOverridePrivate
{
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

static gint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EMailSendAccountOverride, e_mail_send_account_override, G_TYPE_OBJECT)

static void
e_mail_send_account_override_get_property (GObject *object,
					   guint property_id,
					   GValue *value,
					   GParamSpec *pspec)
{
	EMailSendAccountOverride *account_override = E_MAIL_SEND_ACCOUNT_OVERRIDE (object);

	g_return_if_fail (account_override != NULL);

	switch (property_id) {
		case PROP_PREFER_FOLDER:
			g_value_set_boolean (value,
				e_mail_send_account_override_get_prefer_folder (account_override));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
e_mail_send_account_override_set_property (GObject *object,
					   guint property_id,
					   const GValue *value,
					   GParamSpec *pspec)
{
	EMailSendAccountOverride *account_override = E_MAIL_SEND_ACCOUNT_OVERRIDE (object);

	g_return_if_fail (account_override != NULL);

	switch (property_id) {
		case PROP_PREFER_FOLDER:
			e_mail_send_account_override_set_prefer_folder (account_override,
				g_value_get_boolean (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
e_mail_send_account_override_finalize (GObject *object)
{
	EMailSendAccountOverride *account_override = E_MAIL_SEND_ACCOUNT_OVERRIDE (object);

	g_return_if_fail (account_override != NULL);

	if (account_override->priv->key_file) {
		g_key_file_free (account_override->priv->key_file);
		account_override->priv->key_file = NULL;
	}

	if (account_override->priv->config_filename) {
		g_free (account_override->priv->config_filename);
		account_override->priv->config_filename = NULL;
	}

	g_mutex_clear (&account_override->priv->property_lock);

	G_OBJECT_CLASS (e_mail_send_account_override_parent_class)->finalize (object);
}

static void
e_mail_send_account_override_class_init (EMailSendAccountOverrideClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (EMailSendAccountOverridePrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_mail_send_account_override_get_property;
	object_class->set_property = e_mail_send_account_override_set_property;
	object_class->finalize = e_mail_send_account_override_finalize;

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
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_send_account_override_init (EMailSendAccountOverride *account_override)
{
	account_override->priv = G_TYPE_INSTANCE_GET_PRIVATE (account_override,
		E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE, EMailSendAccountOverridePrivate);

	g_mutex_init (&account_override->priv->property_lock);
	account_override->priv->key_file = g_key_file_new ();
	account_override->priv->config_filename = NULL;
	account_override->priv->prefer_folder = TRUE;
	account_override->priv->need_save = FALSE;
	account_override->priv->save_frozen = 0;
}

static gboolean
e_mail_send_account_override_save_locked (EMailSendAccountOverride *account_override)
{
	gchar *contents;
	GError *error = NULL;

	g_return_val_if_fail (account_override->priv->key_file != NULL, FALSE);

	account_override->priv->need_save = FALSE;

	if (!account_override->priv->config_filename)
		return FALSE;

	contents = g_key_file_to_data (account_override->priv->key_file, NULL, NULL);
	if (!contents)
		return FALSE;

	g_file_set_contents (account_override->priv->config_filename, contents, -1, &error);

	if (error) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}

	g_free (contents);

	return TRUE;
}

static gboolean
e_mail_send_account_override_maybe_save_locked (EMailSendAccountOverride *account_override)
{
	if (account_override->priv->save_frozen)
		account_override->priv->need_save = TRUE;
	else
		return e_mail_send_account_override_save_locked (account_override);

	return FALSE;
}

static gchar *
get_override_for_folder_uri_locked (EMailSendAccountOverride *account_override,
				    const gchar *folder_uri)
{
	gchar *account_uid;

	if (!folder_uri || !*folder_uri)
		return NULL;

	account_uid = g_key_file_get_string (account_override->priv->key_file, FOLDERS_SECTION, folder_uri, NULL);

	if (account_uid)
		g_strchomp (account_uid);

	if (account_uid && !*account_uid) {
		g_free (account_uid);
		account_uid = NULL;
	}

	return account_uid;
}

static gchar *
test_one_recipient (/* const */ gchar **keys,
		    /* const */ GPtrArray *values,
		    const gchar *name,
		    const gchar *address)
{
	gint ii;

	g_return_val_if_fail (keys != NULL, NULL);
	g_return_val_if_fail (values != NULL, NULL);

	if ((!name || !*name) && (!address || !*address))
		return NULL;

	for (ii = 0; keys[ii] && ii < values->len; ii++) {
		if (name && *name && e_util_utf8_strstrcase (name, keys[ii]) != NULL) {
			return g_strdup (values->pdata[ii]);
		}

		if (address && *address && e_util_utf8_strstrcase (address, keys[ii]) != NULL) {
			return g_strdup (values->pdata[ii]);
		}
	}

	return NULL;
}

static gchar *
get_override_for_recipients_locked (EMailSendAccountOverride *account_override,
				    CamelAddress *recipients)
{
	CamelInternetAddress *iaddress;
	gchar *account_uid = NULL;
	GPtrArray *values;
	gchar **keys;
	gint ii, len;

	if (!recipients || !CAMEL_IS_INTERNET_ADDRESS (recipients))
		return NULL;

	keys = g_key_file_get_keys (account_override->priv->key_file, RECIPIENTS_SECTION, NULL, NULL);
	if (!keys)
		return NULL;

	values = g_ptr_array_new_full (g_strv_length (keys), g_free);
	for (ii = 0; keys[ii]; ii++) {
		g_ptr_array_add (values, g_key_file_get_string (account_override->priv->key_file,
			RECIPIENTS_SECTION, keys[ii], NULL));
	}

	iaddress = CAMEL_INTERNET_ADDRESS (recipients);
	len = camel_address_length (recipients);
	for (ii = 0; ii < len; ii++) {
		const gchar *name, *address;

		if (camel_internet_address_get (iaddress, ii, &name, &address)) {
			account_uid = test_one_recipient (keys, values, name, address);

			if (account_uid)
				g_strchomp (account_uid);

			if (account_uid && !*account_uid) {
				g_free (account_uid);
				account_uid = NULL;
			}

			if (account_uid)
				break;
		}
	}

	g_ptr_array_free (values, TRUE);
	g_strfreev (keys);

	return account_uid;
}

static void
list_overrides_section_for_account_locked (EMailSendAccountOverride *account_override,
					   const gchar *account_uid,
					   const gchar *section,
					   GSList **overrides)
{
	gchar **keys;

	g_return_if_fail (account_uid != NULL);
	g_return_if_fail (section != NULL);

	if (!overrides)
		return;

	*overrides = NULL;

	keys = g_key_file_get_keys (account_override->priv->key_file, section, NULL, NULL);
	if (keys) {
		gint ii;

		for (ii = 0; keys[ii]; ii++) {
			const gchar *key = keys[ii];
			gchar *value;

			value = g_key_file_get_string (account_override->priv->key_file, section, key, NULL);
			if (g_strcmp0 (value, account_uid) == 0)
				*overrides = g_slist_prepend (*overrides, g_strdup (key));
			g_free (value);
		}
	}

	g_strfreev (keys);

	*overrides = g_slist_reverse (*overrides);
}

static void
list_overrides_for_account_locked (EMailSendAccountOverride *account_override,
				   const gchar *account_uid,
				   GSList **folder_overrides,
				   GSList **recipient_overrides)
{
	if (!account_uid)
		return;

	list_overrides_section_for_account_locked (account_override, account_uid, FOLDERS_SECTION, folder_overrides);
	list_overrides_section_for_account_locked (account_override, account_uid, RECIPIENTS_SECTION, recipient_overrides);
}

EMailSendAccountOverride *
e_mail_send_account_override_new (const gchar *config_filename)
{
	EMailSendAccountOverride *account_override;

	account_override = g_object_new (E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE, NULL);

	if (config_filename)
		e_mail_send_account_override_set_config_filename (account_override, config_filename);

	return account_override;
}

void
e_mail_send_account_override_set_config_filename (EMailSendAccountOverride *account_override,
						  const gchar *config_filename)
{
	GError *error = NULL;
	gboolean old_prefer_folder, prefer_folder_changed;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (config_filename != NULL);
	g_return_if_fail (*config_filename);

	g_mutex_lock (&account_override->priv->property_lock);
	if (g_strcmp0 (config_filename, account_override->priv->config_filename) == 0) {
		g_mutex_unlock (&account_override->priv->property_lock);
		return;
	}

	g_free (account_override->priv->config_filename);
	account_override->priv->config_filename = g_strdup (config_filename);

	g_key_file_load_from_file (account_override->priv->key_file,
		account_override->priv->config_filename, G_KEY_FILE_NONE, NULL);

	old_prefer_folder = account_override->priv->prefer_folder;
	account_override->priv->prefer_folder = g_key_file_get_boolean (account_override->priv->key_file,
		OPTIONS_SECTION, OPTION_PREFER_FOLDER, &error);

	if (error) {
		/* default value is to prefer the folder override over the recipients */
		account_override->priv->prefer_folder = TRUE;
		g_clear_error (&error);
	}

	prefer_folder_changed = (account_override->priv->prefer_folder ? 1 : 0) != (old_prefer_folder ? 1 : 0);

	g_mutex_unlock (&account_override->priv->property_lock);

	if (prefer_folder_changed)
		g_object_notify (G_OBJECT (account_override), "prefer-folder");
}

gchar *
e_mail_send_account_override_dup_config_filename (EMailSendAccountOverride *account_override)
{
	gchar *config_filename;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override), NULL);

	g_mutex_lock (&account_override->priv->property_lock);
	config_filename = g_strdup (account_override->priv->config_filename);
	g_mutex_unlock (&account_override->priv->property_lock);

	return config_filename;
}

void
e_mail_send_account_override_set_prefer_folder (EMailSendAccountOverride *account_override,
						gboolean prefer_folder)
{
	gboolean changed, saved = FALSE;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));

	g_mutex_lock (&account_override->priv->property_lock);

	changed = (account_override->priv->prefer_folder ? 1 : 0) != (prefer_folder ? 1 : 0);
	if (changed) {
		account_override->priv->prefer_folder = prefer_folder;

		g_key_file_set_boolean (account_override->priv->key_file,
			OPTIONS_SECTION, OPTION_PREFER_FOLDER, prefer_folder);

		saved = e_mail_send_account_override_maybe_save_locked (account_override);
	}

	g_mutex_unlock (&account_override->priv->property_lock);

	if (changed)
		g_object_notify (G_OBJECT (account_override), "prefer-folder");
	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}

gboolean
e_mail_send_account_override_get_prefer_folder (EMailSendAccountOverride *account_override)
{
	gboolean prefer_folder;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override), FALSE);

	g_mutex_lock (&account_override->priv->property_lock);
	prefer_folder = account_override->priv->prefer_folder;
	g_mutex_unlock (&account_override->priv->property_lock);

	return prefer_folder;
}

/* free returned pointer with g_free() */
gchar *
e_mail_send_account_override_get_account_uid (EMailSendAccountOverride *account_override,
					      const gchar *folder_uri,
					      const CamelInternetAddress *recipients_to,
					      const CamelInternetAddress *recipients_cc,
					      const CamelInternetAddress *recipients_bcc)
{
	gchar *account_uid = NULL;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override), NULL);
	g_return_val_if_fail (account_override->priv->config_filename != NULL, NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	if (account_override->priv->prefer_folder)
		account_uid = get_override_for_folder_uri_locked (account_override, folder_uri);

	if (!account_uid)
		account_uid = get_override_for_recipients_locked (account_override, (CamelAddress *) recipients_to);

	if (!account_uid)
		account_uid = get_override_for_recipients_locked (account_override, (CamelAddress *) recipients_cc);

	if (!account_uid)
		account_uid = get_override_for_recipients_locked (account_override, (CamelAddress *) recipients_bcc);

	if (!account_uid && !account_override->priv->prefer_folder)
		account_uid = get_override_for_folder_uri_locked (account_override, folder_uri);

	g_mutex_unlock (&account_override->priv->property_lock);

	return account_uid;
}

void
e_mail_send_account_override_remove_for_account_uid (EMailSendAccountOverride *account_override,
						     const gchar *account_uid)
{
	GSList *folders = NULL, *recipients = NULL, *iter;
	gboolean saved = FALSE;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&account_override->priv->property_lock);
	
	list_overrides_for_account_locked (account_override, account_uid, &folders, &recipients);

	if (folders || recipients) {
		for (iter = folders; iter; iter = g_slist_next (iter)) {
			const gchar *key = iter->data;

			g_key_file_remove_key (account_override->priv->key_file, FOLDERS_SECTION, key, NULL);
		}

		for (iter = recipients; iter; iter = g_slist_next (iter)) {
			const gchar *key = iter->data;

			g_key_file_remove_key (account_override->priv->key_file, RECIPIENTS_SECTION, key, NULL);
		}

		saved = e_mail_send_account_override_maybe_save_locked (account_override);
	}

	g_slist_free_full (folders, g_free);
	g_slist_free_full (recipients, g_free);

	g_mutex_unlock (&account_override->priv->property_lock);

	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}

gchar *
e_mail_send_account_override_get_for_folder (EMailSendAccountOverride *account_override,
					     const gchar *folder_uri)
{
	gchar *account_uid = NULL;

	g_return_val_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override), NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	account_uid = get_override_for_folder_uri_locked (account_override, folder_uri);

	g_mutex_unlock (&account_override->priv->property_lock);

	return account_uid;
}

void
e_mail_send_account_override_set_for_folder (EMailSendAccountOverride *account_override,
					     const gchar *folder_uri,
					     const gchar *account_uid)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	g_key_file_set_string (account_override->priv->key_file, FOLDERS_SECTION, folder_uri, account_uid);
	saved = e_mail_send_account_override_maybe_save_locked (account_override);

	g_mutex_unlock (&account_override->priv->property_lock);

	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}

void
e_mail_send_account_override_remove_for_folder (EMailSendAccountOverride *account_override,
						const gchar *folder_uri)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (folder_uri != NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	g_key_file_remove_key (account_override->priv->key_file, FOLDERS_SECTION, folder_uri, NULL);
	saved = e_mail_send_account_override_maybe_save_locked (account_override);

	g_mutex_unlock (&account_override->priv->property_lock);

	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}

gchar *
e_mail_send_account_override_get_for_recipient (EMailSendAccountOverride *account_override,
						const CamelInternetAddress *recipients)
{
	gchar *account_uid;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (recipients != NULL);

	g_mutex_lock (&account_override->priv->property_lock);
	account_uid = get_override_for_recipients_locked (account_override, (CamelAddress *) recipients);
	g_mutex_unlock (&account_override->priv->property_lock);

	return account_uid;
}

void
e_mail_send_account_override_set_for_recipient (EMailSendAccountOverride *account_override,
						const gchar *recipient,
						const gchar *account_uid)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (recipient != NULL);
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	g_key_file_set_string (account_override->priv->key_file, RECIPIENTS_SECTION, recipient, account_uid);
	saved = e_mail_send_account_override_maybe_save_locked (account_override);

	g_mutex_unlock (&account_override->priv->property_lock);

	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}

void
e_mail_send_account_override_remove_for_recipient (EMailSendAccountOverride *account_override,
						   const gchar *recipient)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (recipient != NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	g_key_file_remove_key (account_override->priv->key_file, RECIPIENTS_SECTION, recipient, NULL);
	saved = e_mail_send_account_override_maybe_save_locked (account_override);

	g_mutex_unlock (&account_override->priv->property_lock);

	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}

void
e_mail_send_account_override_list_for_account (EMailSendAccountOverride *account_override,
					       const gchar *account_uid,
					       GSList **folder_overrides,
					       GSList **recipient_overrides)
{
	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));
	g_return_if_fail (account_uid != NULL);

	g_mutex_lock (&account_override->priv->property_lock);

	list_overrides_for_account_locked (account_override, account_uid, folder_overrides, recipient_overrides);

	g_mutex_unlock (&account_override->priv->property_lock);
}

void
e_mail_send_account_override_freeze_save (EMailSendAccountOverride *account_override)
{
	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));

	g_mutex_lock (&account_override->priv->property_lock);

	account_override->priv->save_frozen++;
	if (!account_override->priv->save_frozen) {
		g_warn_if_reached ();
	}

	g_mutex_unlock (&account_override->priv->property_lock);
}

void
e_mail_send_account_override_thaw_save (EMailSendAccountOverride *account_override)
{
	gboolean saved = FALSE;

	g_return_if_fail (E_IS_MAIL_SEND_ACCOUNT_OVERRIDE (account_override));

	g_mutex_lock (&account_override->priv->property_lock);

	if (!account_override->priv->save_frozen) {
		g_warn_if_reached ();
	} else {
		account_override->priv->save_frozen--;
		if (!account_override->priv->save_frozen &&
		    account_override->priv->need_save)
			saved = e_mail_send_account_override_save_locked (account_override);
	}

	g_mutex_unlock (&account_override->priv->property_lock);

	if (saved)
		g_signal_emit (account_override, signals[CHANGED], 0);
}
