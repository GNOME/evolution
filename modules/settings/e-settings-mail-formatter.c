/*
 * e-settings-mail-formatter.c
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

#include "e-settings-mail-formatter.h"

#include <e-util/e-util.h>
#include <em-format/e-mail-formatter.h>
#include <mail/e-mail-reader-utils.h>

#define E_SETTINGS_MAIL_FORMATTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_MAIL_FORMATTER, ESettingsMailFormatterPrivate))

struct _ESettingsMailFormatterPrivate {
	GSettings *settings;
	gulong headers_changed_id;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsMailFormatter,
	e_settings_mail_formatter,
	E_TYPE_EXTENSION)

static EMailFormatter *
settings_mail_formatter_get_extensible (ESettingsMailFormatter *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_MAIL_FORMATTER (extensible);
}

static gboolean
settings_mail_formatter_map_string_to_rgba (GValue *value,
					    GVariant *variant,
					    gpointer user_data)
{
	GdkRGBA rgba;
	const gchar *string;
	gboolean success = FALSE;

	string = g_variant_get_string (variant, NULL);
	if (gdk_rgba_parse (&rgba, string)) {
		g_value_set_boxed (value, &rgba);
		success = TRUE;
	}

	return success;
}

static void
settings_mail_formatter_headers_changed_cb (GSettings *settings,
                                            const gchar *key,
                                            ESettingsMailFormatter *extension)
{
	EMailFormatter *formatter;
	gchar **headers;
	gint ii;

	formatter = settings_mail_formatter_get_extensible (extension);

	headers = g_settings_get_strv (settings, "headers");

	e_mail_formatter_clear_headers (formatter);
	for (ii = 0; headers && headers[ii]; ii++) {
		EMailReaderHeader *h;
		const gchar *xml = headers[ii];

		h = e_mail_reader_header_from_xml (xml);
		if (h && h->enabled)
			e_mail_formatter_add_header (
				formatter, h->name, NULL,
				E_MAIL_FORMATTER_HEADER_FLAG_BOLD);

		e_mail_reader_header_free (h);
	}

	if (!headers || !headers[0])
		e_mail_formatter_set_default_headers (formatter);

	g_strfreev (headers);
}

static void
settings_mail_formatter_dispose (GObject *object)
{
	ESettingsMailFormatterPrivate *priv;

	priv = E_SETTINGS_MAIL_FORMATTER_GET_PRIVATE (object);

	if (priv->headers_changed_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->headers_changed_id);
		priv->headers_changed_id = 0;
	}

	g_clear_object (&priv->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_settings_mail_formatter_parent_class)->
		dispose (object);
}

static void
settings_mail_formatter_constructed (GObject *object)
{
	ESettingsMailFormatter *extension;
	EMailFormatter *formatter;
	GSettings *settings;

	extension = E_SETTINGS_MAIL_FORMATTER (object);
	formatter = settings_mail_formatter_get_extensible (extension);

	settings = extension->priv->settings;

	g_settings_bind_with_mapping (
		settings, "citation-color",
		formatter, "citation-color",
		G_SETTINGS_BIND_GET,
		settings_mail_formatter_map_string_to_rgba,
		(GSettingsBindSetMapping) NULL,
		NULL, (GDestroyNotify) NULL);

	g_settings_bind (
		settings, "mark-citations",
		formatter, "mark-citations",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "image-loading-policy",
		formatter, "image-loading-policy",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "show-sender-photo",
		formatter, "show-sender-photo",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "show-real-date",
		formatter, "show-real-date",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "show-animated-images",
		formatter, "animate-images",
		G_SETTINGS_BIND_GET);

	extension->priv->headers_changed_id = g_signal_connect (
		extension->priv->settings, "changed::headers",
		G_CALLBACK (settings_mail_formatter_headers_changed_cb),
		extension);

	/* Initial synchronization */
	settings_mail_formatter_headers_changed_cb (
		extension->priv->settings, NULL, extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_mail_formatter_parent_class)->
		constructed (object);
}

static void
e_settings_mail_formatter_class_init (ESettingsMailFormatterClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (
		class, sizeof (ESettingsMailFormatterPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = settings_mail_formatter_dispose;
	object_class->constructed = settings_mail_formatter_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER;
}

static void
e_settings_mail_formatter_class_finalize (ESettingsMailFormatterClass *class)
{
}

static void
e_settings_mail_formatter_init (ESettingsMailFormatter *extension)
{
	GSettings *settings;

	extension->priv = E_SETTINGS_MAIL_FORMATTER_GET_PRIVATE (extension);

	settings = g_settings_new ("org.gnome.evolution.mail");
	extension->priv->settings = settings;
}

void
e_settings_mail_formatter_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_mail_formatter_register_type (type_module);
}

