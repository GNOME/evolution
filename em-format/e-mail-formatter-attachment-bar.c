/*
 * e-mail-formatter-attachment-bar.c
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

#include "e-mail-part-attachment-bar.h"

#include <glib/gi18n-lib.h>

#include "e-mail-formatter-extension.h"

typedef EMailFormatterExtension EMailFormatterAttachmentBar;
typedef EMailFormatterExtensionClass EMailFormatterAttachmentBarClass;

GType e_mail_formatter_attachment_bar_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterAttachmentBar,
	e_mail_formatter_attachment_bar,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.widget.attachment-bar",
	NULL
};

static gboolean
emfe_attachment_bar_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            GOutputStream *stream,
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
		e_mail_part_get_id (part),
		e_mail_part_get_id (part));

	g_output_stream_write_all (
		stream, str, strlen (str), NULL, cancellable, NULL);

	g_free (str);

	return TRUE;
}

static GtkWidget *
emfe_attachment_bar_get_widget (EMailFormatterExtension *extension,
                                EMailPartList *context,
                                EMailPart *part,
                                GHashTable *params)
{
	EAttachmentStore *store;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_MAIL_PART_ATTACHMENT_BAR (part), NULL);

	store = e_mail_part_attachment_bar_get_store (
		E_MAIL_PART_ATTACHMENT_BAR (part));

	widget = e_attachment_bar_new (store);
	g_object_set_data (G_OBJECT (store), "attachment-bar", widget);

	return widget;
}

static void
e_mail_formatter_attachment_bar_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_attachment_bar_format;
	class->get_widget = emfe_attachment_bar_get_widget;
}

static void
e_mail_formatter_attachment_bar_init (EMailFormatterExtension *extension)
{
}
