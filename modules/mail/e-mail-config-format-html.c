/*
 * e-mail-config-format-html.c
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

#include "e-mail-config-format-html.h"

#include <shell/e-shell.h>
#include <e-util/e-util.h>
#include <em-format/e-mail-formatter.h>
#include <mail/e-mail-reader-utils.h>

#define E_MAIL_CONFIG_FORMAT_HTML_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_FORMAT_HTML, EMailConfigFormatHTMLPrivate))

struct _EMailConfigFormatHTMLPrivate {
	GSettings *settings;
	gulong headers_changed_id;
};

G_DEFINE_DYNAMIC_TYPE (
	EMailConfigFormatHTML,
	e_mail_config_format_html,
	E_TYPE_EXTENSION)

static EMailFormatter *
mail_config_format_html_get_formatter (EMailConfigFormatHTML *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_MAIL_FORMATTER (extensible);
}

static void
mail_config_format_html_headers_changed_cb (GSettings *settings,
                                            const gchar *key,
                                            EMailConfigFormatHTML *extension)
{
	EMailFormatter *formatter;
	gchar **headers;
	gint ii;

	formatter = mail_config_format_html_get_formatter (extension);

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
mail_config_format_html_dispose (GObject *object)
{
	EMailConfigFormatHTMLPrivate *priv;

	priv = E_MAIL_CONFIG_FORMAT_HTML_GET_PRIVATE (object);

	if (priv->settings != NULL) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->headers_changed_id);
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_format_html_parent_class)->
		dispose (object);
}

static void
mail_config_format_html_constructed (GObject *object)
{
	EMailConfigFormatHTML *extension;
	EMailFormatter *formatter;
	EShellSettings *shell_settings;
	EShell *shell;

	extension = E_MAIL_CONFIG_FORMAT_HTML (object);
	formatter = mail_config_format_html_get_formatter (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	g_object_bind_property_full (
		shell_settings, "mail-citation-color",
		formatter, "citation-color",
		G_BINDING_SYNC_CREATE,
		e_binding_transform_string_to_color,
		NULL, NULL, (GDestroyNotify) NULL);

	g_object_bind_property (
		shell_settings, "mail-mark-citations",
		formatter, "mark-citations",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-image-loading-policy",
		formatter, "image-loading-policy",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-only-local-photos",
		formatter, "only-local-photos",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-sender-photo",
		formatter, "show-sender-photo",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-real-date",
		formatter, "show-real-date",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-animated-images",
		formatter, "animate-images",
		G_BINDING_SYNC_CREATE);

	extension->priv->headers_changed_id = g_signal_connect (
		extension->priv->settings, "changed::headers",
		G_CALLBACK (mail_config_format_html_headers_changed_cb),
		extension);

	/* Initial synchronization */
	mail_config_format_html_headers_changed_cb (
		extension->priv->settings, NULL, extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_format_html_parent_class)->
		constructed (object);
}

static void
e_mail_config_format_html_class_init (EMailConfigFormatHTMLClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigFormatHTMLPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_config_format_html_dispose;
	object_class->constructed = mail_config_format_html_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER;
}

static void
e_mail_config_format_html_class_finalize (EMailConfigFormatHTMLClass *class)
{
}

static void
e_mail_config_format_html_init (EMailConfigFormatHTML *extension)
{
	extension->priv = E_MAIL_CONFIG_FORMAT_HTML_GET_PRIVATE (extension);

	extension->priv->settings =
		g_settings_new ("org.gnome.evolution.mail");
}

void
e_mail_config_format_html_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_format_html_register_type (type_module);
}

