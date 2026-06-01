/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat <www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>

#include <libedataserver/libedataserver.h>

#include "e-mail-composer-mode-override.h"

#define FOLDERS_SECTION		"Folders"
#define RECIPIENTS_SECTION	"Recipients"
#define OPTIONS_SECTION		"Options"

#define OPTION_PREFER_FOLDER	"PreferFolder"

struct _EMailComposerModeOverride {
	GObject parent;

	GKeyFile *key_file;
	gchar *config_filename;
	gboolean prefer_folder;

	gboolean need_save;
	guint save_frozen;

	GMutex property_lock;
};

enum {
	PROP_0,
	PROP_PREFER_FOLDER,
	N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EMailComposerModeOverride, e_mail_composer_mode_override, G_TYPE_OBJECT)

static gboolean
e_mail_composer_mode_override_save_locked (EMailComposerModeOverride *self)
{
	gchar *contents;
	GError *error = NULL;

	g_return_val_if_fail (self->key_file != NULL, FALSE);

	self->need_save = FALSE;

	if (self->config_filename == NULL)
		return FALSE;

	contents = g_key_file_to_data (self->key_file, NULL, &error);

	if (contents)
		g_file_set_contents (self->config_filename, contents, -1, &error);

	g_free (contents);

	if (error) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
e_mail_composer_mode_override_maybe_save_locked (EMailComposerModeOverride *self)
{
	if (self->save_frozen > 0) {
		self->need_save = TRUE;
		return FALSE;
	}

	return e_mail_composer_mode_override_save_locked (self);
}

static EContentEditorMode
get_override_for_folder_uri_locked (EMailComposerModeOverride *self,
				    const gchar *folder_uri)
{
	GError *error = NULL;
	EContentEditorMode mode;

	if (folder_uri == NULL || *folder_uri == '\0')
		return E_CONTENT_EDITOR_MODE_UNKNOWN;

	mode = g_key_file_get_integer (self->key_file, FOLDERS_SECTION, folder_uri, &error);

	if (error != NULL) {
		g_clear_error (&error);
		return E_CONTENT_EDITOR_MODE_UNKNOWN;
	}

	return mode;
}

static EContentEditorMode
test_one_recipient (gchar **keys,
		    EContentEditorMode *values,
		    guint n_keys,
		    const gchar *name,
		    const gchar *address)
{
	guint ii;

	g_return_val_if_fail (keys != NULL, E_CONTENT_EDITOR_MODE_UNKNOWN);
	g_return_val_if_fail (values != NULL, E_CONTENT_EDITOR_MODE_UNKNOWN);

	if (name != NULL && *name == '\0')
		name = NULL;

	if (address != NULL && *address == '\0')
		address = NULL;

	if (name == NULL && address == NULL)
		return E_CONTENT_EDITOR_MODE_UNKNOWN;

	for (ii = 0; ii < n_keys && keys[ii]; ii++) {
		if (name != NULL && e_util_utf8_strstrcase (name, keys[ii]) != NULL)
			return values[ii];

		if (address != NULL && e_util_utf8_strstrcase (address, keys[ii]) != NULL)
			return values[ii];
	}

	return E_CONTENT_EDITOR_MODE_UNKNOWN;
}

static EContentEditorMode
get_override_for_recipients_locked (EMailComposerModeOverride *self,
				    CamelAddress *recipients)
{
	CamelInternetAddress *iaddress;
	EContentEditorMode *values;
	EContentEditorMode result = E_CONTENT_EDITOR_MODE_UNKNOWN;
	gchar **keys;
	gsize n_keys = 0;
	guint ii, len;

	if (!CAMEL_IS_INTERNET_ADDRESS (recipients))
		return E_CONTENT_EDITOR_MODE_UNKNOWN;

	keys = g_key_file_get_keys (self->key_file, RECIPIENTS_SECTION, &n_keys, NULL);
	if (!keys || !n_keys) {
		g_strfreev (keys);
		return E_CONTENT_EDITOR_MODE_UNKNOWN;
	}

	values = g_new (EContentEditorMode, n_keys);
	for (ii = 0; keys[ii]; ii++) {
		GError *error = NULL;

		values[ii] = g_key_file_get_integer (self->key_file, RECIPIENTS_SECTION, keys[ii], &error);

		if (error != NULL) {
			values[ii] = E_CONTENT_EDITOR_MODE_UNKNOWN;
			g_clear_error (&error);
		}
	}

	iaddress = CAMEL_INTERNET_ADDRESS (recipients);
	len = camel_address_length (recipients);
	for (ii = 0; ii < len; ii++) {
		const gchar *name, *address;

		if (camel_internet_address_get (iaddress, ii, &name, &address)) {
			result = test_one_recipient (keys, values, n_keys, name, address);
			if (result != E_CONTENT_EDITOR_MODE_UNKNOWN)
				break;
		}
	}

	g_free (values);
	g_strfreev (keys);

	return result;
}

static void
mail_composer_mode_override_entry_free (gpointer ptr)
{
	EMailComposerModeOverrideEntry *entry = ptr;

	if (entry) {
		g_free (entry->key);
		g_free (entry);
	}
}

static GPtrArray *
list_all_section_locked (EMailComposerModeOverride *self,
			 const gchar *section)
{
	GPtrArray *array;
	gchar **keys;

	array = g_ptr_array_new_with_free_func (mail_composer_mode_override_entry_free);

	keys = g_key_file_get_keys (self->key_file, section, NULL, NULL);
	if (keys != NULL) {
		gint ii;

		for (ii = 0; keys[ii]; ii++) {
			EMailComposerModeOverrideEntry *entry;
			GError *error = NULL;
			EContentEditorMode mode;

			mode = g_key_file_get_integer (self->key_file, section, keys[ii], &error);

			if (error != NULL) {
				g_clear_error (&error);
				continue;
			}

			entry = g_new0 (EMailComposerModeOverrideEntry, 1);
			entry->key = g_strdup (keys[ii]);
			entry->mode = mode;

			g_ptr_array_add (array, entry);
		}
	}

	g_strfreev (keys);

	return array;
}

static void
mail_composer_mode_override_set_property (GObject *object,
					  guint property_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_FOLDER:
			e_mail_composer_mode_override_set_prefer_folder (
				E_MAIL_COMPOSER_MODE_OVERRIDE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_composer_mode_override_get_property (GObject *object,
					  guint property_id,
					  GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_FOLDER:
			g_value_set_boolean (
				value,
				e_mail_composer_mode_override_get_prefer_folder (
				E_MAIL_COMPOSER_MODE_OVERRIDE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_composer_mode_override_finalize (GObject *object)
{
	EMailComposerModeOverride *self = E_MAIL_COMPOSER_MODE_OVERRIDE (object);

	g_key_file_free (self->key_file);
	g_free (self->config_filename);

	g_mutex_clear (&self->property_lock);

	G_OBJECT_CLASS (e_mail_composer_mode_override_parent_class)->finalize (object);
}

static void
e_mail_composer_mode_override_class_init (EMailComposerModeOverrideClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_composer_mode_override_set_property;
	object_class->get_property = mail_composer_mode_override_get_property;
	object_class->finalize = mail_composer_mode_override_finalize;

	properties[PROP_PREFER_FOLDER] =
		g_param_spec_boolean (
			"prefer-folder", NULL, NULL,
			TRUE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

static void
e_mail_composer_mode_override_init (EMailComposerModeOverride *self)
{
	g_mutex_init (&self->property_lock);
	self->key_file = g_key_file_new ();
	self->prefer_folder = TRUE;
}

EMailComposerModeOverride *
e_mail_composer_mode_override_new (const gchar *config_filename)
{
	EMailComposerModeOverride *self;

	self = g_object_new (E_TYPE_MAIL_COMPOSER_MODE_OVERRIDE, NULL);

	if (config_filename != NULL)
		e_mail_composer_mode_override_set_config_filename (self, config_filename);

	return self;
}

void
e_mail_composer_mode_override_set_config_filename (EMailComposerModeOverride *self,
						   const gchar *config_filename)
{
	GError *error = NULL;
	gboolean old_prefer_folder, prefer_folder_changed;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));
	g_return_if_fail (config_filename != NULL);
	g_return_if_fail (*config_filename != '\0');

	g_mutex_lock (&self->property_lock);

	if (g_strcmp0 (config_filename, self->config_filename) == 0) {
		g_mutex_unlock (&self->property_lock);
		return;
	}

	g_free (self->config_filename);
	self->config_filename = g_strdup (config_filename);

	g_key_file_load_from_file (self->key_file, self->config_filename, G_KEY_FILE_NONE, NULL);

	old_prefer_folder = self->prefer_folder;
	self->prefer_folder = g_key_file_get_boolean (self->key_file, OPTIONS_SECTION, OPTION_PREFER_FOLDER, &error);

	if (error != NULL) {
		self->prefer_folder = TRUE;
		g_clear_error (&error);
	}

	prefer_folder_changed = (self->prefer_folder != old_prefer_folder);

	g_mutex_unlock (&self->property_lock);

	if (prefer_folder_changed)
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PREFER_FOLDER]);
}

gchar *
e_mail_composer_mode_override_dup_config_filename (EMailComposerModeOverride *self)
{
	gchar *config_filename;

	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), NULL);

	g_mutex_lock (&self->property_lock);
	config_filename = g_strdup (self->config_filename);
	g_mutex_unlock (&self->property_lock);

	return config_filename;
}

void
e_mail_composer_mode_override_set_prefer_folder (EMailComposerModeOverride *self,
						 gboolean prefer_folder)
{
	gboolean changed, saved = FALSE;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));

	g_mutex_lock (&self->property_lock);

	changed = (prefer_folder != self->prefer_folder);

	if (changed) {
		self->prefer_folder = prefer_folder;

		g_key_file_set_boolean (self->key_file, OPTIONS_SECTION, OPTION_PREFER_FOLDER, prefer_folder);

		saved = e_mail_composer_mode_override_maybe_save_locked (self);
	}

	g_mutex_unlock (&self->property_lock);

	if (changed)
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PREFER_FOLDER]);
	if (saved)
		g_signal_emit (self, signals[CHANGED], 0);
}

gboolean
e_mail_composer_mode_override_get_prefer_folder (EMailComposerModeOverride *self)
{
	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), FALSE);

	return self->prefer_folder;
}

EContentEditorMode
e_mail_composer_mode_override_get_mode (EMailComposerModeOverride *self,
					const gchar *folder_uri,
					CamelInternetAddress *recipients_to,
					CamelInternetAddress *recipients_cc,
					CamelInternetAddress *recipients_bcc)
{
	EContentEditorMode mode = E_CONTENT_EDITOR_MODE_UNKNOWN;

	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), E_CONTENT_EDITOR_MODE_UNKNOWN);
	g_return_val_if_fail (self->config_filename != NULL, E_CONTENT_EDITOR_MODE_UNKNOWN);

	g_mutex_lock (&self->property_lock);

	if (self->prefer_folder)
		mode = get_override_for_folder_uri_locked (self, folder_uri);

	if (mode == E_CONTENT_EDITOR_MODE_UNKNOWN && recipients_to != NULL)
		mode = get_override_for_recipients_locked (self, CAMEL_ADDRESS (recipients_to));

	if (mode == E_CONTENT_EDITOR_MODE_UNKNOWN && recipients_cc != NULL)
		mode = get_override_for_recipients_locked (self, CAMEL_ADDRESS (recipients_cc));

	if (mode == E_CONTENT_EDITOR_MODE_UNKNOWN && recipients_bcc != NULL)
		mode = get_override_for_recipients_locked (self, CAMEL_ADDRESS (recipients_bcc));

	if (mode == E_CONTENT_EDITOR_MODE_UNKNOWN && !self->prefer_folder)
		mode = get_override_for_folder_uri_locked (self, folder_uri);

	g_mutex_unlock (&self->property_lock);

	return mode;
}

EContentEditorMode
e_mail_composer_mode_override_get_for_folder (EMailComposerModeOverride *self,
					      const gchar *folder_uri)
{
	EContentEditorMode mode;

	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), E_CONTENT_EDITOR_MODE_UNKNOWN);

	g_mutex_lock (&self->property_lock);

	mode = get_override_for_folder_uri_locked (self, folder_uri);

	g_mutex_unlock (&self->property_lock);

	return mode;
}

void
e_mail_composer_mode_override_set_for_folder (EMailComposerModeOverride *self,
					      const gchar *folder_uri,
					      EContentEditorMode mode)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));
	g_return_if_fail (folder_uri != NULL);

	g_mutex_lock (&self->property_lock);

	g_key_file_set_integer (self->key_file, FOLDERS_SECTION, folder_uri, mode);

	saved = e_mail_composer_mode_override_maybe_save_locked (self);

	g_mutex_unlock (&self->property_lock);

	if (saved)
		g_signal_emit (self, signals[CHANGED], 0);
}

void
e_mail_composer_mode_override_remove_for_folder (EMailComposerModeOverride *self,
						 const gchar *folder_uri)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));
	g_return_if_fail (folder_uri != NULL);

	g_mutex_lock (&self->property_lock);

	g_key_file_remove_key (self->key_file, FOLDERS_SECTION, folder_uri, NULL);

	saved = e_mail_composer_mode_override_maybe_save_locked (self);

	g_mutex_unlock (&self->property_lock);

	if (saved)
		g_signal_emit (self, signals[CHANGED], 0);
}

EContentEditorMode
e_mail_composer_mode_override_get_for_recipient (EMailComposerModeOverride *self,
						 CamelInternetAddress *recipients)
{
	EContentEditorMode mode;

	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), E_CONTENT_EDITOR_MODE_UNKNOWN);
	if (recipients == NULL)
		return E_CONTENT_EDITOR_MODE_UNKNOWN;

	g_mutex_lock (&self->property_lock);

	mode = get_override_for_recipients_locked (self, CAMEL_ADDRESS (recipients));

	g_mutex_unlock (&self->property_lock);

	return mode;
}

void
e_mail_composer_mode_override_set_for_recipient (EMailComposerModeOverride *self,
						 const gchar *recipient,
						 EContentEditorMode mode)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));
	g_return_if_fail (recipient != NULL);

	g_mutex_lock (&self->property_lock);

	g_key_file_set_integer (self->key_file, RECIPIENTS_SECTION, recipient, mode);

	saved = e_mail_composer_mode_override_maybe_save_locked (self);

	g_mutex_unlock (&self->property_lock);

	if (saved)
		g_signal_emit (self, signals[CHANGED], 0);
}

void
e_mail_composer_mode_override_remove_for_recipient (EMailComposerModeOverride *self,
						    const gchar *recipient)
{
	gboolean saved;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));
	g_return_if_fail (recipient != NULL);

	g_mutex_lock (&self->property_lock);

	g_key_file_remove_key (self->key_file, RECIPIENTS_SECTION, recipient, NULL);

	saved = e_mail_composer_mode_override_maybe_save_locked (self);

	g_mutex_unlock (&self->property_lock);

	if (saved)
		g_signal_emit (self, signals[CHANGED], 0);
}

GPtrArray *
e_mail_composer_mode_override_list_all_folders (EMailComposerModeOverride *self)
{
	GPtrArray *array;

	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), NULL);

	g_mutex_lock (&self->property_lock);

	array = list_all_section_locked (self, FOLDERS_SECTION);

	g_mutex_unlock (&self->property_lock);

	return array;
}

GPtrArray *
e_mail_composer_mode_override_list_all_recipients (EMailComposerModeOverride *self)
{
	GPtrArray *array;

	g_return_val_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self), NULL);

	g_mutex_lock (&self->property_lock);

	array = list_all_section_locked (self, RECIPIENTS_SECTION);

	g_mutex_unlock (&self->property_lock);

	return array;
}

void
e_mail_composer_mode_override_freeze_save (EMailComposerModeOverride *self)
{
	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));

	g_mutex_lock (&self->property_lock);

	self->save_frozen++;
	if (!self->save_frozen) {
		g_warn_if_reached ();
	}

	g_mutex_unlock (&self->property_lock);
}

void
e_mail_composer_mode_override_thaw_save (EMailComposerModeOverride *self)
{
	gboolean saved = FALSE;

	g_return_if_fail (E_IS_MAIL_COMPOSER_MODE_OVERRIDE (self));

	g_mutex_lock (&self->property_lock);

	if (!self->save_frozen) {
		g_warn_if_reached ();
	} else {
		self->save_frozen--;
		if (!self->save_frozen && self->need_save)
			saved = e_mail_composer_mode_override_save_locked (self);
	}

	g_mutex_unlock (&self->property_lock);

	if (saved)
		g_signal_emit (self, signals[CHANGED], 0);
}
