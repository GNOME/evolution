/*
 * Copyright (C) 2019 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#include "libemail-engine/libemail-engine.h"

#include "e-mail-folder-tweaks.h"

#define KEY_TEXT_COLOR		"Color"
#define KEY_ICON_FILENAME	"Icon"
#define KEY_SORT_ORDER		"Sort"

struct _EMailFolderTweaksPrivate {
	gchar *config_filename;
	GKeyFile *config;
	gboolean saving;
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailFolderTweaks, e_mail_folder_tweaks, G_TYPE_OBJECT)

static gboolean
mail_folder_tweaks_save_idle_cb (gpointer user_data)
{
	EMailFolderTweaks *tweaks = user_data;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks), FALSE);

	if (!g_key_file_save_to_file (tweaks->priv->config, tweaks->priv->config_filename, &error)) {
		g_warning ("%s: Failed to save tweaks to '%s': %s", G_STRFUNC,
			tweaks->priv->config_filename, error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	tweaks->priv->saving = FALSE;

	return FALSE;
}

static void
mail_folder_tweaks_schedule_save (EMailFolderTweaks *tweaks)
{
	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));

	if (!tweaks->priv->saving) {
		tweaks->priv->saving = TRUE;

		g_idle_add_full (G_PRIORITY_LOW, mail_folder_tweaks_save_idle_cb, g_object_ref (tweaks), g_object_unref);
	}
}

static gboolean
mail_folder_tweaks_remove_key (EMailFolderTweaks *tweaks,
			       const gchar *folder_uri,
			       const gchar *key)
{
	gboolean changed;

	changed = g_key_file_remove_key (tweaks->priv->config, folder_uri, key, NULL);

	if (changed) {
		gchar **keys;

		keys = g_key_file_get_keys (tweaks->priv->config, folder_uri, NULL, NULL);

		/* Remove the whole group, if it's the last key in it */
		if (!keys || !keys[0]) {
			g_key_file_remove_group (tweaks->priv->config, folder_uri, NULL);
		}

		g_strfreev (keys);
	}

	return changed;
}

static gchar *
mail_folder_tweaks_dup_string (EMailFolderTweaks *tweaks,
			       const gchar *folder_uri,
			       const gchar *key)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks), NULL);
	g_return_val_if_fail (folder_uri != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	return g_key_file_get_string (tweaks->priv->config, folder_uri, key, NULL);
}

static void
mail_folder_tweaks_set_string (EMailFolderTweaks *tweaks,
			       const gchar *folder_uri,
			       const gchar *key,
			       const gchar *value)
{
	gboolean changed;

	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (key != NULL);

	if (!value || !*value) {
		changed = mail_folder_tweaks_remove_key (tweaks, folder_uri, key);
	} else {
		gchar *stored;

		stored = mail_folder_tweaks_dup_string (tweaks, folder_uri, key);
		changed = g_strcmp0 (stored, value) != 0;
		g_free (stored);

		if (changed)
			g_key_file_set_string (tweaks->priv->config, folder_uri, key, value);
	}

	if (changed) {
		mail_folder_tweaks_schedule_save (tweaks);

		g_signal_emit (tweaks, signals[CHANGED], 0, folder_uri, NULL);
	}
}

static guint
mail_folder_tweaks_get_uint (EMailFolderTweaks *tweaks,
			     const gchar *folder_uri,
			     const gchar *key)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks), 0);
	g_return_val_if_fail (folder_uri != NULL, 0);
	g_return_val_if_fail (key != NULL, 0);

	return (guint) g_key_file_get_uint64 (tweaks->priv->config, folder_uri, key, NULL);
}

static void
mail_folder_tweaks_set_uint (EMailFolderTweaks *tweaks,
			     const gchar *folder_uri,
			     const gchar *key,
			     guint value)
{
	gboolean changed;

	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (key != NULL);

	if (!value) {
		changed = mail_folder_tweaks_remove_key (tweaks, folder_uri, key);
	} else {
		guint stored;

		stored = mail_folder_tweaks_get_uint (tweaks, folder_uri, key);
		changed = stored != value;

		if (changed)
			g_key_file_set_uint64 (tweaks->priv->config, folder_uri, key, (guint64) value);
	}

	if (changed) {
		mail_folder_tweaks_schedule_save (tweaks);

		g_signal_emit (tweaks, signals[CHANGED], 0, folder_uri, NULL);
	}
}

static GObject *
e_mail_folder_tweaks_constructor (GType type,
				  guint n_construct_properties,
				  GObjectConstructParam *construct_properties)
{
	static GWeakRef singleton;
	GObject *result;

	result = g_weak_ref_get (&singleton);
	if (!result) {
		result = G_OBJECT_CLASS (e_mail_folder_tweaks_parent_class)->constructor (type, n_construct_properties, construct_properties);

		if (result)
			g_weak_ref_set (&singleton, result);
	}

	return result;
}

static void
e_mail_folder_tweaks_finalize (GObject *object)
{
	EMailFolderTweaks *tweaks = E_MAIL_FOLDER_TWEAKS (object);

	g_free (tweaks->priv->config_filename);
	g_key_file_free (tweaks->priv->config);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_folder_tweaks_parent_class)->finalize (object);
}

static void
e_mail_folder_tweaks_class_init (EMailFolderTweaksClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructor = e_mail_folder_tweaks_constructor;
	object_class->finalize = e_mail_folder_tweaks_finalize;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailFolderTweaksClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
e_mail_folder_tweaks_init (EMailFolderTweaks *tweaks)
{
	tweaks->priv = e_mail_folder_tweaks_get_instance_private (tweaks);
	tweaks->priv->config_filename = g_build_filename (mail_session_get_config_dir (), "folder-tweaks.ini", NULL);
	tweaks->priv->config = g_key_file_new ();

	/* Ignore errors */
	g_key_file_load_from_file (tweaks->priv->config, tweaks->priv->config_filename, G_KEY_FILE_NONE, NULL);
}

EMailFolderTweaks *
e_mail_folder_tweaks_new (void)
{
	return g_object_new (E_TYPE_MAIL_FOLDER_TWEAKS, NULL);
}

void
e_mail_folder_tweaks_remove_for_folders (EMailFolderTweaks *tweaks,
					 const gchar *top_folder_uri)
{
	gboolean changed = FALSE;
	gchar **groups;
	gint ii;

	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (top_folder_uri != NULL);

	groups = g_key_file_get_groups (tweaks->priv->config, NULL);

	if (!groups)
		return;

	for (ii = 0; groups[ii]; ii++) {
		if (g_str_has_prefix (groups[ii], top_folder_uri)) {
			changed = g_key_file_remove_group (tweaks->priv->config, groups[ii], NULL) || changed;
		}
	}

	g_strfreev (groups);

	if (changed)
		mail_folder_tweaks_schedule_save (tweaks);
}

gboolean
e_mail_folder_tweaks_get_color (EMailFolderTweaks *tweaks,
				const gchar *folder_uri,
				GdkRGBA *out_rgba)
{
	gchar *stored;
	gboolean success;

	g_return_val_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks), FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);
	g_return_val_if_fail (out_rgba != NULL, FALSE);

	stored = mail_folder_tweaks_dup_string (tweaks, folder_uri, KEY_TEXT_COLOR);
	if (!stored)
		return FALSE;

	success = gdk_rgba_parse (out_rgba, stored);

	g_free (stored);

	return success;
}

void
e_mail_folder_tweaks_set_color (EMailFolderTweaks *tweaks,
				const gchar *folder_uri,
				const GdkRGBA *rgba)
{
	gchar *value;

	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (folder_uri != NULL);

	if (rgba)
		value = gdk_rgba_to_string (rgba);
	else
		value = NULL;

	mail_folder_tweaks_set_string (tweaks, folder_uri, KEY_TEXT_COLOR, value);

	g_free (value);
}

gchar *
e_mail_folder_tweaks_dup_icon_filename (EMailFolderTweaks *tweaks,
					const gchar *folder_uri)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks), NULL);
	g_return_val_if_fail (folder_uri != NULL, NULL);

	return mail_folder_tweaks_dup_string (tweaks, folder_uri, KEY_ICON_FILENAME);
}

void
e_mail_folder_tweaks_set_icon_filename (EMailFolderTweaks *tweaks,
					const gchar *folder_uri,
					const gchar *icon_filename)
{
	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (folder_uri != NULL);

	mail_folder_tweaks_set_string (tweaks, folder_uri, KEY_ICON_FILENAME, icon_filename);
}

/* returns 0 as not set/do not know */
guint
e_mail_folder_tweaks_get_sort_order (EMailFolderTweaks *tweaks,
					const gchar *folder_uri)
{
	g_return_val_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks), 0);
	g_return_val_if_fail (folder_uri != NULL, 0);

	return mail_folder_tweaks_get_uint (tweaks, folder_uri, KEY_SORT_ORDER);
}

/* Use 0 as 'sort_order' to unset the value */
void
e_mail_folder_tweaks_set_sort_order (EMailFolderTweaks *tweaks,
				     const gchar *folder_uri,
				     guint sort_order)
{
	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (folder_uri != NULL);

	mail_folder_tweaks_set_uint (tweaks, folder_uri, KEY_SORT_ORDER, sort_order);
}

void
e_mail_folder_tweaks_remove_sort_order_for_folders (EMailFolderTweaks *tweaks,
						    const gchar *top_folder_uri)
{
	gchar **groups;
	gint ii;

	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (top_folder_uri != NULL);

	groups = g_key_file_get_groups (tweaks->priv->config, NULL);

	if (!groups)
		return;

	for (ii = 0; groups[ii]; ii++) {
		if (g_str_has_prefix (groups[ii], top_folder_uri) &&
		    g_key_file_has_key (tweaks->priv->config, groups[ii], KEY_SORT_ORDER, NULL)) {
			e_mail_folder_tweaks_set_sort_order (tweaks, groups[ii], 0);
		}
	}

	g_strfreev (groups);
}

void
e_mail_folder_tweaks_folder_renamed (EMailFolderTweaks *tweaks,
				     const gchar *old_folder_uri,
				     const gchar *new_folder_uri)
{
	gchar **keys;

	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (old_folder_uri != NULL);
	g_return_if_fail (new_folder_uri != NULL);

	if (g_strcmp0 (old_folder_uri, new_folder_uri) == 0)
		return;

	if (!g_key_file_has_group (tweaks->priv->config, old_folder_uri))
		return;

	keys = g_key_file_get_keys (tweaks->priv->config, old_folder_uri, NULL, NULL);
	if (keys) {
		guint ii;

		for (ii = 0; keys[ii]; ii++) {
			gchar *value;

			value = mail_folder_tweaks_dup_string (tweaks, old_folder_uri, keys[ii]);
			mail_folder_tweaks_set_string (tweaks, new_folder_uri, keys[ii], value);

			g_free (value);
		}

		g_strfreev (keys);
	}

	if (g_key_file_remove_group (tweaks->priv->config, old_folder_uri, NULL)) {
		mail_folder_tweaks_schedule_save (tweaks);

		g_signal_emit (tweaks, signals[CHANGED], 0, old_folder_uri, NULL);
		g_signal_emit (tweaks, signals[CHANGED], 0, new_folder_uri, NULL);
	}
}

void
e_mail_folder_tweaks_folder_deleted (EMailFolderTweaks *tweaks,
				     const gchar *folder_uri)
{
	g_return_if_fail (E_IS_MAIL_FOLDER_TWEAKS (tweaks));
	g_return_if_fail (folder_uri != NULL);

	if (g_key_file_remove_group (tweaks->priv->config, folder_uri, NULL)) {
		mail_folder_tweaks_schedule_save (tweaks);

		g_signal_emit (tweaks, signals[CHANGED], 0, folder_uri, NULL);
	}
}
