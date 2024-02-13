/*
 * e-mail-formatter-quote.c
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

#include "e-mail-formatter-quote.h"

#include <camel/camel.h>

#include "e-mail-formatter-utils.h"
#include "e-mail-part.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-utils.h"

#include <libebackend/libebackend.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

struct _EMailFormatterQuotePrivate {
	gchar *credits;
	EMailFormatterQuoteFlags flags;
};

/* internal formatter extensions */
GType e_mail_formatter_quote_headers_get_type (void);
GType e_mail_formatter_quote_message_rfc822_get_type (void);
GType e_mail_formatter_quote_text_enriched_get_type (void);
GType e_mail_formatter_quote_text_html_get_type (void);
GType e_mail_formatter_quote_text_plain_get_type (void);

static gpointer e_mail_formatter_quote_parent_class = NULL;
static gint EMailFormatterQuote_private_offset = 0;

static inline gpointer
e_mail_formatter_quote_get_instance_private (EMailFormatterQuote *self)
{
	return (G_STRUCT_MEMBER_P (self, EMailFormatterQuote_private_offset));
}

static void
mail_formatter_quote_run (EMailFormatter *formatter,
                          EMailFormatterContext *context,
                          GOutputStream *stream,
                          GCancellable *cancellable)
{
	EMailFormatterQuote *qf;
	EMailFormatterQuoteContext *qf_context;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	const gchar *string;
	GHashTable *secured_message_ids = NULL;
	gboolean has_encrypted_part = FALSE;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	qf = E_MAIL_FORMATTER_QUOTE (formatter);

	qf_context = (EMailFormatterQuoteContext *) context;
	qf_context->qf_flags = qf->priv->flags;

	if ((qf_context->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_NO_FORMATTING) != 0)
		context->flags |= E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING;

	g_seekable_seek (
		G_SEEKABLE (stream),
		0, G_SEEK_SET, NULL, NULL);

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	if ((qf->priv->flags & E_MAIL_FORMATTER_QUOTE_FLAG_SKIP_INSECURE_PARTS) != 0)
		secured_message_ids = e_mail_formatter_utils_extract_secured_message_ids (head);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = E_MAIL_PART (link->data);
		const gchar *mime_type;

		if (e_mail_part_id_has_suffix (part, ".headers") &&
		   !(qf_context->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS)) {
			continue;
		}

		if (e_mail_part_id_has_suffix (part, ".rfc822")) {
			link = e_mail_formatter_find_rfc822_end_iter (link);
			continue;
		}

		if (part->is_hidden)
			continue;

		if (e_mail_part_get_is_attachment (part))
			continue;

		if (secured_message_ids &&
		    e_mail_formatter_utils_consider_as_secured_part (part, secured_message_ids)) {
			if (!e_mail_part_has_validity (part))
				continue;

			if (e_mail_part_get_validity (part, E_MAIL_PART_VALIDITY_ENCRYPTED)) {
				/* consider the second and following encrypted parts as evil */
				if (has_encrypted_part)
					continue;

				has_encrypted_part = TRUE;
			}
		}

		mime_type = e_mail_part_get_mime_type (part);

		/* Skip error messages in the quoted part */
		if (g_strcmp0 (mime_type, "application/vnd.evolution.error") == 0)
			continue;

		e_mail_formatter_format_as (
			formatter, context, part, stream,
			mime_type, cancellable);
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	g_clear_pointer (&secured_message_ids, g_hash_table_destroy);

	/* Before we were inserting the BR elements and the credits in front of
	 * the actual HTML code of the message. But this was wrong as when WebKit
	 * was loading the given HTML code that looked like
	 * <br>CREDITS<html>MESSAGE_CODE</html> WebKit parsed it like
	 * <html><br>CREDITS</html><html>MESSAGE_CODE</html>. As no elements are
	 * allowed outside of the HTML root element WebKit wrapped them into
	 * another HTML root element. Afterwards the first root element was
	 * treated as the primary one and all the elements from the second's root
	 * HEAD and BODY elements were moved to the first one.
	 * Thus the HTML that was loaded into composer contained the i.e. META
	 * or STYLE definitions in the body.
	 * So if we want to put something into the message we have to put it into
	 * the special span element and it will be moved to body in EHTMLEditorView */
	if (qf->priv->credits && *qf->priv->credits) {
		gchar *credits = g_markup_printf_escaped (
			"<span class=\"-x-evo-to-body\" data-credits=\"%s\"></span>",
			qf->priv->credits);
		g_output_stream_write_all (
			stream, credits, strlen (credits), NULL, cancellable, NULL);
		g_free (credits);
	}

	/* If we want to cite the message we have to append the special span element
	 * after the message and cite it in EHTMLEditorView because of reasons
	 * mentioned above */
	if (qf->priv->flags & E_MAIL_FORMATTER_QUOTE_FLAG_CITE) {
		string = "<span class=\"-x-evo-cite-body\"></span>";
		g_output_stream_write_all (
			stream, string, strlen (string), NULL, cancellable, NULL);
	}
}

static void
e_mail_formatter_quote_init (EMailFormatterQuote *formatter)
{
	formatter->priv = e_mail_formatter_quote_get_instance_private (formatter);
}

static void
e_mail_formatter_quote_finalize (GObject *object)
{
	EMailFormatterQuote *formatter;

	formatter = E_MAIL_FORMATTER_QUOTE (object);

	g_free (formatter->priv->credits);
	formatter->priv->credits = NULL;

	/* Chain up to parent's finalize() */
	G_OBJECT_CLASS (e_mail_formatter_quote_parent_class)->finalize (object);
}

static void
e_mail_formatter_quote_base_init (EMailFormatterQuoteClass *class)
{
	/* Register internal extensions. */
	g_type_ensure (e_mail_formatter_quote_headers_get_type ());
	g_type_ensure (e_mail_formatter_quote_message_rfc822_get_type ());
	g_type_ensure (e_mail_formatter_quote_text_enriched_get_type ());
	g_type_ensure (e_mail_formatter_quote_text_html_get_type ());
	g_type_ensure (e_mail_formatter_quote_text_plain_get_type ());

	e_mail_formatter_extension_registry_load (
		E_MAIL_FORMATTER_CLASS (class)->extension_registry,
		E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION);

	E_MAIL_FORMATTER_CLASS (class)->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_PRE |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
}

static void
e_mail_formatter_quote_class_init (EMailFormatterQuoteClass *class)
{
	GObjectClass *object_class;
	EMailFormatterClass *formatter_class;

	e_mail_formatter_quote_parent_class = g_type_class_peek_parent (class);
	if (EMailFormatterQuote_private_offset != 0)
		g_type_class_adjust_private_offset (class, &EMailFormatterQuote_private_offset);

	formatter_class = E_MAIL_FORMATTER_CLASS (class);
	formatter_class->context_size = sizeof (EMailFormatterQuoteContext);
	formatter_class->run = mail_formatter_quote_run;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_mail_formatter_quote_finalize;
}

GType
e_mail_formatter_quote_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EMailFormatterClass),
			(GBaseInitFunc) e_mail_formatter_quote_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) e_mail_formatter_quote_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */
			sizeof (EMailFormatterQuote),
			0,	/* n_preallocs */
			(GInstanceInitFunc) e_mail_formatter_quote_init,
			NULL	/* value_table */
		};

		type = g_type_register_static (
			E_TYPE_MAIL_FORMATTER,
			"EMailFormatterQuote", &type_info, 0);

		EMailFormatterQuote_private_offset = g_type_add_instance_private (type, sizeof (EMailFormatterQuotePrivate));
	}

	return type;
}

EMailFormatter *
e_mail_formatter_quote_new (const gchar *credits,
                            EMailFormatterQuoteFlags flags)
{
	EMailFormatterQuote *formatter;
	formatter = g_object_new (E_TYPE_MAIL_FORMATTER_QUOTE, NULL);

	formatter->priv->credits = g_strdup (credits);
	formatter->priv->flags = flags;

	return (EMailFormatter *) formatter;
}

/* ------------------------------------------------------------------------- */

G_DEFINE_ABSTRACT_TYPE (
	EMailFormatterQuoteExtension,
	e_mail_formatter_quote_extension,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static void
e_mail_formatter_quote_extension_class_init (EMailFormatterQuoteExtensionClass *class)
{
}

static void
e_mail_formatter_quote_extension_init (EMailFormatterQuoteExtension *extension)
{
}

