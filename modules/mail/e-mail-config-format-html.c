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

#include <libebackend/libebackend.h>

#include <shell/e-shell.h>
#include <e-util/e-util.h>
#include <em-format/e-mail-formatter.h>
#include <mail/e-mail-reader-utils.h>

static gpointer parent_class;

static void
headers_changed_cb (GSettings *settings,
                    const gchar *key,
                    gpointer user_data)
{
	gint ii;
	gchar **headers;
	EExtension *extension;
	EMailFormatter *formatter;

	g_return_if_fail (settings != NULL);

	if (key && !g_str_equal (key, "headers"))
		return;

	extension = user_data;
	formatter = E_MAIL_FORMATTER (e_extension_get_extensible (extension));

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
mail_config_format_html_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	EShellSettings *shell_settings;
	EShell *shell;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	g_object_bind_property_full (
		shell_settings, "mail-citation-color",
		extensible, "citation-color",
		G_BINDING_SYNC_CREATE,
		e_binding_transform_string_to_color,
		NULL, NULL, (GDestroyNotify) NULL);

	g_object_bind_property (
		shell_settings, "mail-mark-citations",
		extensible, "mark-citations",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-image-loading-policy",
		extensible, "image-loading-policy",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-only-local-photos",
		extensible, "only-local-photos",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-sender-photo",
		extensible, "show-sender-photo",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-real-date",
		extensible, "show-real-date",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "mail-show-animated-images",
		extensible, "animate-images",
		G_BINDING_SYNC_CREATE);

	settings = g_settings_new ("org.gnome.evolution.mail");
	g_signal_connect (settings, "changed", G_CALLBACK (headers_changed_cb), object);

	g_object_set_data_full (
		G_OBJECT (extensible), "reader-header-settings",
		settings, g_object_unref);

	/* Initial synchronization */
	headers_changed_cb (settings, NULL, object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
mail_config_format_html_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_config_format_html_constructed;

	class->extensible_type = E_TYPE_MAIL_FORMATTER;
}

void
e_mail_config_format_html_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EExtensionClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_config_format_html_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EExtension),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"EMailConfigFormatHTML", &type_info, 0);
}
