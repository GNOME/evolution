/*
 * e-mail-formatter-attachment-bar.c
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
#include "e-mail-format-extensions.h"
#include "e-mail-part-attachment-bar.h"
#include <misc/e-attachment-bar.h>

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>

typedef struct _EMailFormatterAttachmentBar {
	GObject parent;
} EMailFormatterAttachmentBar;

typedef struct _EMailFormatterAttachmentBarClass {
	GObjectClass parent_class;
} EMailFormatterAttachmentBarClass;

static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterAttachmentBar,
	e_mail_formatter_attachment_bar,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static const gchar *formatter_mime_types[] = { "application/vnd.evolution.widget.attachment-bar", NULL };

static gboolean
emfe_attachment_bar_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            CamelStream *stream,
                            GCancellable *cancellable)
{
	gchar *str;

	if ((context->mode != E_MAIL_FORMATTER_MODE_NORMAL) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_RAW) &&
	    (context->mode != E_MAIL_FORMATTER_MODE_ALL_HEADERS))
		return FALSE;

	str = g_strdup_printf (
		"<object type=\"application/vnd.evolution.widget.attachment-bar\" "
		"height=\"0\" width=\"100%%\" data=\"%s\" id=\"%s\"></object>",
		part->id, part->id);

	camel_stream_write_string (stream, str, cancellable, NULL);

	g_free (str);
	return TRUE;
}

static void
unset_bar_from_store_data (GObject *store,
                           EAttachmentBar *bar)
{
	/*
	if (E_IS_ATTACHMENT_STORE (store))
		g_object_set_data (store, "attachment-bar", NULL);
	*/
}

static GtkWidget *
emfe_attachment_bar_get_widget (EMailFormatterExtension *extension,
                                EMailPartList *context,
                                EMailPart *part,
                                GHashTable *params)
{
	EMailPartAttachmentBar *empab;
	GtkWidget *widget;

	g_return_val_if_fail (E_MAIL_PART_IS (part, EMailPartAttachmentBar), NULL);

	empab = (EMailPartAttachmentBar *) part;
	widget = e_attachment_bar_new (empab->store);
	g_object_set_data (G_OBJECT (empab->store), "attachment-bar", widget);
	g_object_weak_ref (G_OBJECT (widget),
			(GWeakNotify) unset_bar_from_store_data, empab->store);

	return widget;
}

static const gchar *
emfe_attachment_bar_get_display_name (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar *
emfe_attachment_bar_get_description (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar **
emfe_attachment_bar_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_attachment_bar_class_init (EMailFormatterAttachmentBarClass *class)
{
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_attachment_bar_format;
	iface->get_widget = emfe_attachment_bar_get_widget;
	iface->get_display_name = emfe_attachment_bar_get_display_name;
	iface->get_description = emfe_attachment_bar_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_attachment_bar_mime_types;
}

static void
e_mail_formatter_attachment_bar_init (EMailFormatterAttachmentBar *extension)
{

}
