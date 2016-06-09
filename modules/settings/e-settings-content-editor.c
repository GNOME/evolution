/*
 * e-settings-content-editor.c
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

#include "e-settings-content-editor.h"

#include <e-util/e-util.h>

#define E_SETTINGS_CONTENT_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_CONTENT_EDITOR, ESettingsContentEditorPrivate))

struct _ESettingsContentEditorPrivate {
	GSettings *settings;

	GHashTable *old_settings;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsContentEditor,
	e_settings_content_editor,
	E_TYPE_EXTENSION)

static void
settings_content_editor_load_style (ESettingsContentEditor *extension)
{
	EExtensible *extensible;
	EContentEditor *cnt_editor;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	cnt_editor = e_html_editor_get_content_editor (E_HTML_EDITOR (extensible));
	e_content_editor_update_styles (cnt_editor);
}

static void
settings_content_editor_changed_cb (GSettings *settings,
                                    const gchar *key,
                                    ESettingsContentEditor *extension)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (extension->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (extension->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (extension->priv->old_settings, key);

		settings_content_editor_load_style (extension);
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
settings_content_editor_realize (GtkWidget *widget,
                                 ESettingsContentEditor *extension)
{
	GSettings *settings;

	settings = extension->priv->settings;

	g_settings_bind (
		settings, "composer-inline-spelling",
		widget, "inline-spelling",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "composer-magic-links",
		widget, "magic-links",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "composer-magic-smileys",
		widget, "magic-smileys",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "composer-unicode-smileys",
		widget, "unicode-smileys",
		G_SETTINGS_BIND_DEFAULT);

	settings_content_editor_load_style (extension);

	/* Reload the web view when certain settings change. */

	g_signal_connect (
		settings, "changed::use-custom-font",
		G_CALLBACK (settings_content_editor_changed_cb), extension);

	g_signal_connect (
		settings, "changed::monospace-font",
		G_CALLBACK (settings_content_editor_changed_cb), extension);

	g_signal_connect (
		settings, "changed::variable-width-font",
		G_CALLBACK (settings_content_editor_changed_cb), extension);

	g_signal_connect (
		settings, "changed::mark-citations",
		G_CALLBACK (settings_content_editor_changed_cb), extension);

	g_signal_connect (
		settings, "changed::citation-color",
		G_CALLBACK (settings_content_editor_changed_cb), extension);
}

static void
settings_content_editor_dispose (GObject *object)
{
	ESettingsContentEditorPrivate *priv;

	priv = E_SETTINGS_CONTENT_EDITOR_GET_PRIVATE (object);

	if (priv->settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->settings,
			settings_content_editor_changed_cb, object);
	}

	g_clear_object (&priv->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_settings_content_editor_parent_class)->dispose (object);
}

static void
settings_content_editor_finalize (GObject *object)
{
	ESettingsContentEditorPrivate *priv;

	priv = E_SETTINGS_CONTENT_EDITOR_GET_PRIVATE (object);

	if (priv->old_settings) {
		g_hash_table_destroy (priv->old_settings);
		priv->old_settings = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_settings_content_editor_parent_class)->finalize (object);
}

static void
settings_content_editor_constructed (GObject *object)
{
	EExtensible *extensible;
	EContentEditor *cnt_editor;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	cnt_editor = e_html_editor_get_content_editor (E_HTML_EDITOR (extensible));

	g_signal_connect (
		cnt_editor, "realize",
		G_CALLBACK (settings_content_editor_realize), object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_content_editor_parent_class)->constructed (object);
}

static void
e_settings_content_editor_class_init (ESettingsContentEditorClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsContentEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = settings_content_editor_dispose;
	object_class->finalize = settings_content_editor_finalize;
	object_class->constructed = settings_content_editor_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_HTML_EDITOR;
}

static void
e_settings_content_editor_class_finalize (ESettingsContentEditorClass *class)
{
}

static void
e_settings_content_editor_init (ESettingsContentEditor *extension)
{
	GSettings *settings;

	extension->priv = E_SETTINGS_CONTENT_EDITOR_GET_PRIVATE (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	extension->priv->settings = settings;

	extension->priv->old_settings = g_hash_table_new_full (
		g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
}

void
e_settings_content_editor_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_content_editor_register_type (type_module);
}

