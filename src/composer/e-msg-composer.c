/*
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
 *
 * Authors:
 *		Ettore Perazzoli (ettore@ximian.com)
 *		Jeffrey Stedfast (fejj@ximian.com)
 *		Miguel de Icaza  (miguel@ximian.com)
 *		Radek Doulik     (rodo@ximian.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <enchant.h>

#ifdef ENABLE_SMIME
#include <cert.h>
#endif

#include "e-composer-from-header.h"
#include "e-composer-text-header.h"
#include "e-composer-private.h"

#include <em-format/e-mail-part.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-formatter-quote.h>

#include <shell/e-shell.h>

#include <libemail-engine/libemail-engine.h>

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;

	CamelMimeMessage *message;
	CamelDataWrapper *top_level_part;
	CamelDataWrapper *text_plain_part;

	ESource *source;
	CamelSession *session;
	CamelInternetAddress *from;

	CamelTransferEncoding plain_encoding;
	GtkPrintOperationAction print_action;

	GPtrArray *recipients;
	GSList *recipients_with_certificate; /* EContact * */

	guint skip_content : 1;
	guint is_redirect : 1;
	guint need_thread : 1;
	guint pgp_sign : 1;
	guint pgp_encrypt : 1;
	guint smime_sign : 1;
	guint smime_encrypt : 1;
	guint is_draft : 1;
};

/* Flags for building a message. */
typedef enum {
	COMPOSER_FLAG_HTML_CONTENT			= 1 << 0,
	COMPOSER_FLAG_SAVE_OBJECT_DATA			= 1 << 1,
	COMPOSER_FLAG_PRIORITIZE_MESSAGE		= 1 << 2,
	COMPOSER_FLAG_REQUEST_READ_RECEIPT		= 1 << 3,
	COMPOSER_FLAG_DELIVERY_STATUS_NOTIFICATION	= 1 << 4,
	COMPOSER_FLAG_PGP_SIGN				= 1 << 5,
	COMPOSER_FLAG_PGP_ENCRYPT			= 1 << 6,
	COMPOSER_FLAG_SMIME_SIGN			= 1 << 7,
	COMPOSER_FLAG_SMIME_ENCRYPT			= 1 << 8,
	COMPOSER_FLAG_SAVE_DRAFT			= 1 << 9
} ComposerFlags;

enum {
	PROP_0,
	PROP_BUSY,
	PROP_SOFT_BUSY,
	PROP_EDITOR,
	PROP_FOCUS_TRACKER,
	PROP_SHELL,
	PROP_IS_REPLY_OR_FORWARD
};

enum {
	PRESEND,
	SEND,
	SAVE_TO_DRAFTS,
	SAVE_TO_OUTBOX,
	PRINT,
	BEFORE_DESTROY,
	LAST_SIGNAL
};

static GtkTargetEntry drag_dest_targets[] = {
	{ (gchar *) "text/uri-list", 0, E_DND_TARGET_TYPE_TEXT_URI_LIST },
	{ (gchar *) "_NETSCAPE_URL", 0, E_DND_TARGET_TYPE_MOZILLA_URL },
	{ (gchar *) "text/x-moz-url", 0, E_DND_TARGET_TYPE_TEXT_X_MOZ_URL },
	{ (gchar *) "text/html", 0, E_DND_TARGET_TYPE_TEXT_HTML },
	{ (gchar *) "UTF8_STRING", 0, E_DND_TARGET_TYPE_UTF8_STRING },
	{ (gchar *) "text/plain", 0, E_DND_TARGET_TYPE_TEXT_PLAIN },
	{ (gchar *) "STRING", 0, E_DND_TARGET_TYPE_STRING },
	{ (gchar *) "text/plain;charset=utf-8", 0, E_DND_TARGET_TYPE_TEXT_PLAIN_UTF8 },
};

static guint signals[LAST_SIGNAL];

/* used by e_msg_composer_add_message_attachments () */
static void	add_attachments_from_multipart	(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 gboolean just_inlines,
						 gint depth);

/* used by e_msg_composer_setup_with_message () */
static void	handle_multipart		(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 CamelMimePart *parent_part,
						 gboolean keep_signature,
						 gboolean is_signed_or_encrypted,
						 GCancellable *cancellable,
						 gint depth);
static void	handle_multipart_alternative	(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 CamelMimePart *parent_part,
						 gboolean keep_signature,
						 GCancellable *cancellable,
						 gint depth);
static void	handle_multipart_encrypted	(EMsgComposer *composer,
						 CamelMimePart *multipart,
						 CamelMimePart *parent_part,
						 gboolean keep_signature,
						 GCancellable *cancellable,
						 gint depth);
static void	handle_multipart_signed		(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 CamelMimePart *parent_part,
						 gboolean keep_signature,
						 GCancellable *cancellable,
						 gint depth);

G_DEFINE_TYPE_WITH_CODE (EMsgComposer, e_msg_composer, GTK_TYPE_WINDOW,
	G_ADD_PRIVATE (EMsgComposer)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
async_context_free (AsyncContext *context)
{
	g_clear_object (&context->activity);
	g_clear_object (&context->message);
	g_clear_object (&context->top_level_part);
	g_clear_object (&context->text_plain_part);
	g_clear_object (&context->source);
	g_clear_object (&context->session);
	g_clear_object (&context->from);

	if (context->recipients != NULL)
		g_ptr_array_free (context->recipients, TRUE);

	if (context->recipients_with_certificate)
		g_slist_free_full (context->recipients_with_certificate, g_object_unref);

	g_slice_free (AsyncContext, context);
}

static void
e_msg_composer_unref_content_hash (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (composer->priv->content_hash_ref_count > 0);

	composer->priv->content_hash_ref_count--;

	if (!composer->priv->content_hash_ref_count) {
		g_clear_pointer (&composer->priv->content_hash, e_content_editor_util_free_content_hash);
	}
}

static void
e_msg_composer_inc_soft_busy (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (composer->priv->soft_busy_count + 1 > composer->priv->soft_busy_count);

	composer->priv->soft_busy_count++;

	if (composer->priv->soft_busy_count == 1)
		g_object_notify (G_OBJECT (composer), "soft-busy");
}

static void
e_msg_composer_dec_soft_busy (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (composer->priv->soft_busy_count > 0);

	composer->priv->soft_busy_count--;

	if (composer->priv->soft_busy_count == 0)
		g_object_notify (G_OBJECT (composer), "soft-busy");
}

static gchar *
emcu_part_as_text (EMsgComposer *composer,
                   CamelMimePart *part,
                   gssize *out_len,
                   GCancellable *cancellable);

/*
 * emcu_part_to_html:
 * @part:
 *
 * Converts a mime part's contents into html text.  If @credits is given,
 * then it will be used as an attribution string, and the
 * content will be cited.  Otherwise no citation or attribution
 * will be performed.
 *
 * Return Value: The part in displayable html format.
 */
static gchar *
emcu_part_to_html (EMsgComposer *composer,
                   CamelMimePart *part,
                   gssize *len,
                   gboolean keep_signature,
                   GCancellable *cancellable)
{
	CamelSession *session;
	GOutputStream *stream;
	gchar *text;
	EMailParser *parser;
	EMailFormatter *formatter;
	EMailPartList *part_list;
	GString *part_id;
	EShell *shell;
	GtkWindow *window;
	gsize n_bytes_written = 0;
	GQueue queue = G_QUEUE_INIT;

	if (keep_signature) {
		CamelContentType *content_type;

		content_type = camel_mime_part_get_content_type (part);
		if (camel_content_type_is (content_type, "text", "plain")) {
			gchar *body;

			/* easy case, just enclose the plain text into <pre> to preserve white-spaces */
			body = emcu_part_as_text (composer, part, NULL, cancellable);
			if (body) {
				text = camel_text_to_html (body, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
				g_free (body);

				if (text) {
					EHTMLEditor *editor;

					if (len)
						*len = strlen (text);

					editor = e_msg_composer_get_editor (composer);
					/* switch from HTML to plain text, if needed */
					if (e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML)
						e_html_editor_set_mode (editor, E_CONTENT_EDITOR_MODE_PLAIN_TEXT);

					return text;
				}
			}
		}
	}

	shell = e_shell_get_default ();
	window = e_shell_get_active_window (shell);

	session = e_msg_composer_ref_session (composer);

	part_list = e_mail_part_list_new (NULL, NULL, NULL);

	part_id = g_string_sized_new (0);
	parser = e_mail_parser_new (session);
	e_mail_parser_parse_part (
		parser, part, part_id, cancellable, &queue);
	while (!g_queue_is_empty (&queue)) {
		EMailPart *mail_part = g_queue_pop_head (&queue);

		if (!e_mail_part_get_is_attachment (mail_part) &&
		    !mail_part->is_hidden)
			e_mail_part_list_add_part (part_list, mail_part);

		g_object_unref (mail_part);
	}
	g_string_free (part_id, TRUE);
	g_object_unref (parser);
	g_object_unref (session);

	if (e_mail_part_list_is_empty (part_list)) {
		g_object_unref (part_list);
		return NULL;
	}

	stream = g_memory_output_stream_new_resizable ();

	formatter = e_mail_formatter_quote_new (
		NULL, keep_signature ? E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG : 0);
	e_mail_formatter_update_style (
		formatter,
		gtk_widget_get_state_flags (GTK_WIDGET (window)));

	e_mail_formatter_format_sync (
		formatter, part_list, stream,
		0, E_MAIL_FORMATTER_MODE_PRINTING, cancellable);

	g_object_unref (formatter);
	g_object_unref (part_list);

	g_output_stream_write_all (stream, "", 1, &n_bytes_written, NULL, NULL);

	g_output_stream_close (stream, NULL, NULL);

	text = g_memory_output_stream_steal_data (
		G_MEMORY_OUTPUT_STREAM (stream));

	if (len != NULL)
		*len = strlen (text);

	g_object_unref (stream);

	return text;
}

static gchar *
emcu_part_as_text (EMsgComposer *composer,
                   CamelMimePart *part,
                   gssize *out_len,
                   GCancellable *cancellable)
{
	CamelDataWrapper *dw;
	gchar *text;
	gssize length;

	dw = camel_medium_get_content (CAMEL_MEDIUM (part));
	if (dw) {
		CamelStream *mem = camel_stream_mem_new (), *stream = g_object_ref (mem);
		CamelContentType *ct;
		GByteArray *bytes;

		ct = camel_mime_part_get_content_type (part);
		if (ct) {
			const gchar *charset = camel_content_type_param (ct, "charset");
			if (charset && *charset) {
				CamelMimeFilter *filter = camel_mime_filter_charset_new (charset, "UTF-8");
				if (filter) {
					CamelStream *filtered = camel_stream_filter_new (stream);

					if (filtered) {
						camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered), filter);
						g_object_unref (stream);
						stream = filtered;
					}

					g_object_unref (filter);
				}
			}
		}

		camel_data_wrapper_decode_to_stream_sync (dw, stream, cancellable, NULL);
		camel_stream_close (stream, cancellable, NULL);
		if (stream != mem)
			camel_stream_close (mem, cancellable, NULL);

		bytes = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (mem));
		if (bytes && bytes->len) {
			text = g_strndup ((const gchar *) bytes->data, bytes->len);
			length = bytes->len;
		} else {
			text = g_strdup ("");
			length = 0;
		}

		g_object_unref (stream);
		g_object_unref (mem);
	} else {
		text = g_strdup ("");
		length = 0;
	}

	if (out_len)
		*out_len = length;

	return text;
}

static EDestination **
destination_list_to_vector_sized (GList *list,
                                  gint n)
{
	EDestination **destv;
	gint i = 0;

	if (n == -1)
		n = g_list_length (list);

	if (n == 0)
		return NULL;

	destv = g_new (EDestination *, n + 1);
	while (list != NULL && i < n) {
		destv[i] = E_DESTINATION (list->data);
		list->data = NULL;
		i++;
		list = g_list_next (list);
	}
	destv[i] = NULL;

	return destv;
}

static EDestination **
destination_list_to_vector (GList *list)
{
	return destination_list_to_vector_sized (list, -1);
}

#define LINE_LEN 72

static gboolean
text_requires_quoted_printable (const gchar *text,
                                gsize len)
{
	const gchar *p;
	gsize pos;

	if (!text)
		return FALSE;

	if (len == -1)
		len = strlen (text);

	if (len >= 5 && strncmp (text, "From ", 5) == 0)
		return TRUE;

	for (p = text, pos = 0; pos + 6 <= len; pos++, p++) {
		if (*p == '\n' && strncmp (p + 1, "From ", 5) == 0)
			return TRUE;
	}

	return FALSE;
}

static gboolean
best_encoding (GByteArray *buf,
               const gchar *charset,
	       CamelTransferEncoding *encoding)
{
	gchar *in, *out, outbuf[256], *ch;
	gsize inlen, outlen;
	gint status, count = 0;
	iconv_t cd;

	if (!charset)
		return FALSE;

	cd = camel_iconv_open (charset, "utf-8");
	if (cd == (iconv_t) -1)
		return FALSE;

	in = (gchar *) buf->data;
	inlen = buf->len;
	do {
		out = outbuf;
		outlen = sizeof (outbuf);
		status = camel_iconv (cd, (const gchar **) &in, &inlen, &out, &outlen);
		for (ch = out - 1; ch >= outbuf; ch--) {
			if ((guchar) *ch > 127)
				count++;
		}
	} while (status == (gsize) -1 && errno == E2BIG);
	camel_iconv_close (cd);

	if (status == (gsize) -1 || status > 0)
		return FALSE;

	if ((count == 0) && (buf->len < LINE_LEN) &&
		!text_requires_quoted_printable (
		(const gchar *) buf->data, buf->len))
		*encoding = CAMEL_TRANSFER_ENCODING_7BIT;
	else if (count <= buf->len * 0.17)
		*encoding = CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
	else
		*encoding = CAMEL_TRANSFER_ENCODING_BASE64;

	return TRUE;
}

static gchar *
best_charset (GByteArray *buf,
              const gchar *default_charset,
              CamelTransferEncoding *encoding)
{
	const gchar *charset;

	/* First try US-ASCII */
	if (best_encoding (buf, "US-ASCII", encoding) &&
	    *encoding == CAMEL_TRANSFER_ENCODING_7BIT)
		return NULL;

	/* Next try the user-specified charset for this message */
	if (best_encoding (buf, default_charset, encoding))
		return g_strdup (default_charset);

	/* Now try the user's default charset from the mail config */
	charset = e_composer_get_default_charset ();
	if (best_encoding (buf, charset, encoding))
		return g_strdup (charset);

	/* Try to find something that will work */
	charset = camel_charset_best (
		(const gchar *) buf->data, buf->len);
	if (charset == NULL) {
		*encoding = CAMEL_TRANSFER_ENCODING_7BIT;
		return NULL;
	}

	if (!best_encoding (buf, charset, encoding))
		*encoding = CAMEL_TRANSFER_ENCODING_BASE64;

	return g_strdup (charset);
}

/* These functions builds a CamelMimeMessage for the message that the user has
 * composed in 'composer'.
 */

static void
set_recipients_from_destv (CamelMimeMessage *msg,
                           EDestination **to_destv,
                           EDestination **cc_destv,
                           EDestination **bcc_destv,
                           gboolean redirect)
{
	CamelInternetAddress *to_addr;
	CamelInternetAddress *cc_addr;
	CamelInternetAddress *bcc_addr;
	CamelInternetAddress *target;
	const gchar *text_addr, *header;
	gboolean seen_hidden_list = FALSE;
	gint i;

	to_addr = camel_internet_address_new ();
	cc_addr = camel_internet_address_new ();
	bcc_addr = camel_internet_address_new ();

	for (i = 0; to_destv != NULL && to_destv[i] != NULL; ++i) {
		text_addr = e_destination_get_address (to_destv[i]);

		if (text_addr && *text_addr) {
			target = to_addr;
			if (e_destination_is_evolution_list (to_destv[i])
			    && !e_destination_list_show_addresses (to_destv[i])) {
				target = bcc_addr;
				seen_hidden_list = TRUE;
			}

			if (camel_address_decode (CAMEL_ADDRESS (target), text_addr) <= 0)
				camel_internet_address_add (target, "", text_addr);
		}
	}

	for (i = 0; cc_destv != NULL && cc_destv[i] != NULL; ++i) {
		text_addr = e_destination_get_address (cc_destv[i]);
		if (text_addr && *text_addr) {
			target = cc_addr;
			if (e_destination_is_evolution_list (cc_destv[i])
			    && !e_destination_list_show_addresses (cc_destv[i])) {
				target = bcc_addr;
				seen_hidden_list = TRUE;
			}

			if (camel_address_decode (CAMEL_ADDRESS (target), text_addr) <= 0)
				camel_internet_address_add (target, "", text_addr);
		}
	}

	for (i = 0; bcc_destv != NULL && bcc_destv[i] != NULL; ++i) {
		text_addr = e_destination_get_address (bcc_destv[i]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (bcc_addr), text_addr) <= 0)
				camel_internet_address_add (bcc_addr, "", text_addr);
		}
	}

	if (redirect)
		header = CAMEL_RECIPIENT_TYPE_RESENT_TO;
	else
		header = CAMEL_RECIPIENT_TYPE_TO;

	if (camel_address_length (CAMEL_ADDRESS (to_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, to_addr);
	} else if (seen_hidden_list) {
		camel_medium_set_header (
			CAMEL_MEDIUM (msg), header, "Undisclosed-Recipient:;");
	}

	header = redirect ? CAMEL_RECIPIENT_TYPE_RESENT_CC : CAMEL_RECIPIENT_TYPE_CC;
	if (camel_address_length (CAMEL_ADDRESS (cc_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, cc_addr);
	}

	header = redirect ? CAMEL_RECIPIENT_TYPE_RESENT_BCC : CAMEL_RECIPIENT_TYPE_BCC;
	if (camel_address_length (CAMEL_ADDRESS (bcc_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, bcc_addr);
	}

	g_object_unref (to_addr);
	g_object_unref (cc_addr);
	g_object_unref (bcc_addr);
}

static void
build_message_headers (EMsgComposer *composer,
                       CamelMimeMessage *message,
                       gboolean redirect)
{
	EComposerHeaderTable *table;
	EComposerHeader *header;
	ESource *source;
	gchar *alias_name = NULL, *alias_address = NULL, *uid;
	const gchar *subject;
	const gchar *reply_to;
	const gchar *mail_reply_to;
	const gchar *mail_followup_to;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	table = e_msg_composer_get_header_table (composer);

	uid = e_composer_header_table_dup_identity_uid (table, &alias_name, &alias_address);
	if (uid)
		source = e_composer_header_table_ref_source (table, uid);
	else
		source = NULL;

	/* Subject: */
	subject = e_composer_header_table_get_subject (table);
	if (!redirect || g_strcmp0 (subject, camel_mime_message_get_subject (message)) != 0)
		camel_mime_message_set_subject (message, subject);

	if (source != NULL) {
		CamelMedium *medium;
		CamelInternetAddress *addr;
		ESourceMailSubmission *ms;
		EComposerHeader *composer_header;
		const gchar *extension_name;
		const gchar *header_name;
		const gchar *name = NULL, *address = NULL;
		const gchar *transport_uid;
		const gchar *sent_folder = NULL;
		gboolean is_from_override = FALSE;

		composer_header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_FROM);
		if (e_composer_from_header_get_override_visible (E_COMPOSER_FROM_HEADER (composer_header))) {
			name = e_composer_header_table_get_from_name (table);
			address = e_composer_header_table_get_from_address (table);

			if (address && !*address) {
				name = NULL;
				address = NULL;
			}

			is_from_override = address != NULL;
		}

		if (!address) {
			if (alias_name)
				name = alias_name;
			if (alias_address)
				address = alias_address;
		}

		if (!is_from_override && (!address || !name || !*name)) {
			ESourceMailIdentity *mail_identity;

			mail_identity = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY);

			if (!name || !*name)
				name = e_source_mail_identity_get_name (mail_identity);

			if (!address)
				address = e_source_mail_identity_get_address (mail_identity);
		}

		extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
		ms = e_source_get_extension (source, extension_name);

		if (e_source_mail_submission_get_use_sent_folder (ms))
			sent_folder = e_source_mail_submission_get_sent_folder (ms);
		transport_uid = e_source_mail_submission_get_transport_uid (ms);

		medium = CAMEL_MEDIUM (message);

		/* From: / Resent-From: */
		addr = camel_internet_address_new ();
		camel_internet_address_add (addr, name, address);
		if (redirect) {
			gchar *value;

			value = camel_address_encode (CAMEL_ADDRESS (addr));
			camel_medium_set_header (medium, "Resent-From", value);
			g_free (value);
		} else {
			camel_mime_message_set_from (message, addr);
		}
		g_object_unref (addr);

		/* X-Evolution-Identity */
		header_name = "X-Evolution-Identity";
		camel_medium_set_header (medium, header_name, uid);

		/* X-Evolution-Fcc */
		header_name = "X-Evolution-Fcc";
		camel_medium_set_header (medium, header_name, sent_folder);

		/* X-Evolution-Transport */
		header_name = "X-Evolution-Transport";
		camel_medium_set_header (medium, header_name, transport_uid);

		g_object_unref (source);
	}

	if (redirect)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Is-Redirect", "1");

	/* Reply-To: */
	reply_to = e_composer_header_table_get_reply_to (table);
	if (reply_to != NULL && *reply_to != '\0') {
		CamelInternetAddress *addr;

		addr = camel_internet_address_new ();

		if (camel_address_unformat (CAMEL_ADDRESS (addr), reply_to) > 0)
			camel_mime_message_set_reply_to (message, addr);

		g_object_unref (addr);
	}

	/* Mail-Followup-To: */
	mail_followup_to = e_composer_header_table_get_mail_followup_to (table);
	if (mail_followup_to != NULL && *mail_followup_to != '\0') {
		CamelInternetAddress *addr;

		addr = camel_internet_address_new ();

		if (camel_address_unformat (CAMEL_ADDRESS (addr), mail_followup_to) > 0) {
			gchar *str;

			str = camel_address_encode (CAMEL_ADDRESS (addr));
			camel_medium_set_header (CAMEL_MEDIUM (message), "Mail-Followup-To", str);
			g_free (str);
		}

		g_object_unref (addr);
	}

	/* Mail-Reply-To: */
	mail_reply_to = e_composer_header_table_get_mail_reply_to (table);
	if (mail_reply_to != NULL && *mail_reply_to != '\0') {
		CamelInternetAddress *addr;

		addr = camel_internet_address_new ();

		if (camel_address_unformat (CAMEL_ADDRESS (addr), mail_reply_to) > 0) {
			gchar *str;

			str = camel_address_encode (CAMEL_ADDRESS (addr));
			camel_medium_set_header (CAMEL_MEDIUM (message), "Mail-Reply-To", str);
			g_free (str);
		}

		g_object_unref (addr);
	}

	/* To:, Cc:, Bcc: */
	header = e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_TO);
	if (e_composer_header_get_visible (header)) {
		EDestination **to, **cc, **bcc;

		to = e_composer_header_table_get_destinations_to (table);
		cc = e_composer_header_table_get_destinations_cc (table);
		bcc = e_composer_header_table_get_destinations_bcc (table);

		set_recipients_from_destv (message, to, cc, bcc, redirect);

		e_destination_freev (to);
		e_destination_freev (cc);
		e_destination_freev (bcc);
	}

	/* Date: */
	if (redirect) {
		struct tm local;
		gint tz, offset;
		time_t date;
		gchar *datestr;

		date = time (NULL);
		camel_localtime_with_offset (date, &local, &tz);
		offset = (((tz / 60 / 60) * 100) + (tz / 60 % 60));

		datestr = camel_header_format_date (date, offset);
		camel_medium_set_header (CAMEL_MEDIUM (message), "Resent-Date", datestr);
		g_free (datestr);
	} else {
		camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	}

	/* X-Evolution-PostTo: */
	header = e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_POST_TO);
	if (e_composer_header_get_visible (header)) {
		CamelMedium *medium;
		const gchar *name = "X-Evolution-PostTo";
		GList *list, *iter;

		medium = CAMEL_MEDIUM (message);
		camel_medium_remove_header (medium, name);

		list = e_composer_header_table_get_post_to (table);
		for (iter = list; iter != NULL; iter = iter->next) {
			gchar *folder = iter->data;
			camel_medium_add_header (medium, name, folder);
			g_free (folder);
		}
		g_list_free (list);
	}

	g_free (uid);
	g_free (alias_name);
	g_free (alias_address);
}

static CamelCipherHash
account_hash_algo_to_camel_hash (const gchar *hash_algo)
{
	CamelCipherHash res = CAMEL_CIPHER_HASH_DEFAULT;

	if (hash_algo && *hash_algo) {
		if (g_ascii_strcasecmp (hash_algo, "sha1") == 0)
			res = CAMEL_CIPHER_HASH_SHA1;
		else if (g_ascii_strcasecmp (hash_algo, "sha256") == 0)
			res = CAMEL_CIPHER_HASH_SHA256;
		else if (g_ascii_strcasecmp (hash_algo, "sha384") == 0)
			res = CAMEL_CIPHER_HASH_SHA384;
		else if (g_ascii_strcasecmp (hash_algo, "sha512") == 0)
			res = CAMEL_CIPHER_HASH_SHA512;
	}

	return res;
}

static void
composer_add_charset_filter (CamelStream *stream,
                             const gchar *charset)
{
	CamelMimeFilter *filter;

	filter = camel_mime_filter_charset_new ("UTF-8", charset);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), filter);
	g_object_unref (filter);
}

static void
composer_add_quoted_printable_filter (CamelStream *stream)
{
	CamelMimeFilter *filter;

	filter = camel_mime_filter_basic_new (CAMEL_MIME_FILTER_BASIC_QP_ENC);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), filter);
	g_object_unref (filter);
}

/* Extracts auto-completed contacts which have X.509 or PGP certificate set.
   This should be called in the GUI thread, because it accesses GtkWidget-s. */
static GSList * /* EContact * */
composer_get_completed_recipients_with_certificate (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	GSList *contacts = NULL;
	EDestination **to, **cc, **bcc;
	gint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);
	to = e_composer_header_table_get_destinations_to (table);
	cc = e_composer_header_table_get_destinations_cc (table);
	bcc = e_composer_header_table_get_destinations_bcc (table);

	#define traverse_destv(x) \
		for (ii = 0; x && x[ii]; ii++) { \
			EDestination *dest = x[ii]; \
			EContactCert *x509cert, *pgpcert; \
			EContact *contact; \
			 \
			contact = e_destination_get_contact (dest); \
			 \
			/* Get certificates only for individuals, not for lists */ \
			if (!contact || e_destination_is_evolution_list (dest)) \
				continue; \
			 \
			x509cert = e_contact_get (contact, E_CONTACT_X509_CERT); \
			pgpcert = e_contact_get (contact, E_CONTACT_PGP_CERT); \
			 \
			if (x509cert || pgpcert) \
				contacts = g_slist_prepend (contacts, e_contact_duplicate (contact)); \
			 \
			e_contact_cert_free (x509cert); \
			e_contact_cert_free (pgpcert); \
		}

	traverse_destv (to);
	traverse_destv (cc);
	traverse_destv (bcc);

	#undef traverse_destv

	e_destination_freev (to);
	e_destination_freev (cc);
	e_destination_freev (bcc);

	return contacts;
}

static gchar *
composer_get_recipient_certificate_cb (EMailSession *session,
				       guint32 flags, /* bit-or of CamelRecipientCertificateFlags */
				       const gchar *email_address,
				       gpointer user_data)
{
	AsyncContext *context = user_data;
	const gchar *field_type;
	GSList *link;
	gchar *base64_cert = NULL;

	g_return_val_if_fail (context != NULL, NULL);

	if (!email_address || !*email_address)
		return NULL;

	if ((flags & CAMEL_RECIPIENT_CERTIFICATE_SMIME) != 0)
		field_type = "X509";
	else
		field_type = "PGP";

	for (link = context->recipients_with_certificate; link && !base64_cert; link = g_slist_next (link)) {
		EContact *contact = link->data;
		GList *emails, *elink;
		gboolean email_matches = FALSE;

		emails = e_contact_get (contact, E_CONTACT_EMAIL);
		for (elink = emails; elink && !email_matches; elink = g_list_next (elink)) {
			const gchar *contact_email = elink->data;

			email_matches = contact_email && g_ascii_strcasecmp (contact_email, email_address) == 0;
		}

		if (email_matches) {
			GList *attrs, *alink;

			attrs = e_vcard_get_attributes (E_VCARD (contact));
			for (alink = attrs; alink && !base64_cert; alink = g_list_next (alink)) {
				EVCardAttribute *attr = alink->data;
				GString *value;

				if (!e_vcard_attribute_has_type (attr, field_type))
					continue;

				value = e_vcard_attribute_get_value_decoded (attr);
				if (!value || !value->len) {
					if (value)
						g_string_free (value, TRUE);
					continue;
				}

				/* Looking for an encryption certificate, while S/MIME can have
				   disabled usage for encryption, thus verify this will skip
				   such certificates. */
				#ifdef ENABLE_SMIME
				if ((flags & CAMEL_RECIPIENT_CERTIFICATE_SMIME) != 0) {
					CERTCertificate *nss_cert;
					gboolean usable;

					nss_cert = CERT_DecodeCertFromPackage (value->str, value->len);
					usable = nss_cert && (nss_cert->keyUsage & (KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT)) != 0;
					if (nss_cert)
						CERT_DestroyCertificate (nss_cert);

					if (!usable) {
						g_string_free (value, TRUE);
						continue;
					}
				}
				#endif

				base64_cert = g_base64_encode ((const guchar *) value->str, value->len);

				g_string_free (value, TRUE);
			}
		}

		g_list_free_full (emails, g_free);
	}

	return base64_cert;
}

/* Helper for composer_build_message_thread() */
static gboolean
composer_build_message_pgp (AsyncContext *context,
                            GCancellable *cancellable,
                            GError **error)
{
	ESourceOpenPGP *extension;
	CamelCipherContext *cipher;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	const gchar *extension_name;
	const gchar *pgp_key_id;
	const gchar *signing_algorithm;
	gboolean always_trust;
	gboolean encrypt_to_self;
	gboolean prefer_inline;
	gboolean locate_keys;

	/* Return silently if we're not signing or encrypting with PGP. */
	if (!context->pgp_sign && !context->pgp_encrypt)
		return TRUE;

	extension_name = E_SOURCE_EXTENSION_OPENPGP;
	extension = e_source_get_extension (context->source, extension_name);

	always_trust = e_source_openpgp_get_always_trust (extension);
	encrypt_to_self = context->is_draft || e_source_openpgp_get_encrypt_to_self (extension);
	prefer_inline = e_source_openpgp_get_prefer_inline (extension);
	locate_keys = e_source_openpgp_get_locate_keys (extension);
	pgp_key_id = e_source_openpgp_get_key_id (extension);
	signing_algorithm = e_source_openpgp_get_signing_algorithm (extension);

	mime_part = camel_mime_part_new ();

	camel_medium_set_content (
		CAMEL_MEDIUM (mime_part),
		context->top_level_part);

	if (context->top_level_part == context->text_plain_part)
		camel_mime_part_set_encoding (
			mime_part, context->plain_encoding);

	g_object_unref (context->top_level_part);
	context->top_level_part = NULL;

	if ((pgp_key_id == NULL || *pgp_key_id == '\0') &&
	    !camel_internet_address_get (context->from, 0, NULL, &pgp_key_id))
		pgp_key_id = NULL;

	if (context->pgp_sign) {
		CamelMimePart *npart;
		gboolean success;

		npart = camel_mime_part_new ();

		cipher = camel_gpg_context_new (context->session);
		camel_gpg_context_set_always_trust (CAMEL_GPG_CONTEXT (cipher), always_trust);
		camel_gpg_context_set_prefer_inline (CAMEL_GPG_CONTEXT (cipher), prefer_inline);

		success = camel_cipher_context_sign_sync (
			cipher, pgp_key_id,
			account_hash_algo_to_camel_hash (signing_algorithm),
			mime_part, npart, cancellable, error);

		g_object_unref (cipher);

		g_object_unref (mime_part);

		if (!success) {
			g_object_unref (npart);
			return FALSE;
		}

		mime_part = npart;
	}

	if (context->pgp_encrypt) {
		CamelMimePart *npart;
		gulong handler_id;
		gboolean success;

		npart = camel_mime_part_new ();

		/* Check to see if we should encrypt to self.
		 * NB: Gets removed immediately after use. */
		if (encrypt_to_self && pgp_key_id != NULL)
			g_ptr_array_add (
				context->recipients,
				g_strdup (pgp_key_id));

		cipher = camel_gpg_context_new (context->session);
		camel_gpg_context_set_always_trust (CAMEL_GPG_CONTEXT (cipher), always_trust);
		camel_gpg_context_set_prefer_inline (CAMEL_GPG_CONTEXT (cipher), prefer_inline);
		camel_gpg_context_set_locate_keys (CAMEL_GPG_CONTEXT (cipher), locate_keys);

		handler_id = g_signal_connect (context->session, "get-recipient-certificate",
			G_CALLBACK (composer_get_recipient_certificate_cb), context);

		success = camel_cipher_context_encrypt_sync (
			cipher, pgp_key_id, context->recipients,
			mime_part, npart, cancellable, error);

		if (handler_id)
			g_signal_handler_disconnect (context->session, handler_id);

		g_object_unref (cipher);

		if (encrypt_to_self && pgp_key_id != NULL)
			g_ptr_array_set_size (
				context->recipients,
				context->recipients->len - 1);

		g_object_unref (mime_part);

		if (!success) {
			g_object_unref (npart);
			return FALSE;
		}

		mime_part = npart;
	}

	content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	context->top_level_part = g_object_ref (content);

	g_object_unref (mime_part);

	return TRUE;
}

#ifdef ENABLE_SMIME
static gboolean
composer_build_message_smime (AsyncContext *context,
                              GCancellable *cancellable,
                              GError **error)
{
	ESourceSMIME *extension;
	CamelCipherContext *cipher;
	CamelMimePart *mime_part;
	const gchar *extension_name;
	const gchar *signing_algorithm;
	const gchar *signing_certificate;
	const gchar *encryption_certificate;
	gboolean encrypt_to_self;
	gboolean have_signing_certificate;
	gboolean have_encryption_certificate;

	/* Return silently if we're not signing or encrypting with S/MIME. */
	if (!context->smime_sign && !context->smime_encrypt)
		return TRUE;

	extension_name = E_SOURCE_EXTENSION_SMIME;
	extension = e_source_get_extension (context->source, extension_name);

	encrypt_to_self = context->is_draft ||
		e_source_smime_get_encrypt_to_self (extension);

	signing_algorithm =
		e_source_smime_get_signing_algorithm (extension);

	signing_certificate =
		e_source_smime_get_signing_certificate (extension);

	encryption_certificate =
		e_source_smime_get_encryption_certificate (extension);

	have_signing_certificate =
		(signing_certificate != NULL) &&
		(*signing_certificate != '\0');

	have_encryption_certificate =
		(encryption_certificate != NULL) &&
		(*encryption_certificate != '\0');

	if (context->smime_sign && !have_signing_certificate) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot sign outgoing message: "
			"No signing certificate set for "
			"this account"));
		return FALSE;
	}

	if (context->smime_encrypt && !have_encryption_certificate) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Cannot encrypt outgoing message: "
			"No encryption certificate set for "
			"this account"));
		return FALSE;
	}

	mime_part = camel_mime_part_new ();

	camel_medium_set_content (
		CAMEL_MEDIUM (mime_part),
		context->top_level_part);

	if (context->top_level_part == context->text_plain_part)
		camel_mime_part_set_encoding (
			mime_part, context->plain_encoding);

	g_object_unref (context->top_level_part);
	context->top_level_part = NULL;

	if (context->smime_sign) {
		CamelMimePart *npart;
		gboolean success;

		npart = camel_mime_part_new ();

		cipher = camel_smime_context_new (context->session);

		/* if we're also encrypting, envelope-sign rather than clear-sign */
		if (context->smime_encrypt) {
			camel_smime_context_set_sign_mode (
				(CamelSMIMEContext *) cipher,
				CAMEL_SMIME_SIGN_ENVELOPED);
			camel_smime_context_set_encrypt_key (
				(CamelSMIMEContext *) cipher,
				TRUE, encryption_certificate);
		} else if (have_encryption_certificate) {
			camel_smime_context_set_encrypt_key (
				(CamelSMIMEContext *) cipher,
				TRUE, encryption_certificate);
		}

		success = camel_cipher_context_sign_sync (
			cipher, signing_certificate,
			account_hash_algo_to_camel_hash (signing_algorithm),
			mime_part, npart, cancellable, error);

		g_object_unref (cipher);

		g_object_unref (mime_part);

		if (!success) {
			g_object_unref (npart);
			return FALSE;
		}

		mime_part = npart;
	}

	if (context->smime_encrypt) {
		gulong handler_id;
		gboolean success;

		/* Check to see if we should encrypt to self.
		 * NB: Gets removed immediately after use. */
		if (encrypt_to_self)
			g_ptr_array_add (
				context->recipients, g_strdup (
				encryption_certificate));

		cipher = camel_smime_context_new (context->session);
		camel_smime_context_set_encrypt_key (
			(CamelSMIMEContext *) cipher, TRUE,
			encryption_certificate);

		handler_id = g_signal_connect (context->session, "get-recipient-certificate",
			G_CALLBACK (composer_get_recipient_certificate_cb), context);

		success = camel_cipher_context_encrypt_sync (
			cipher, NULL,
			context->recipients, mime_part,
			CAMEL_MIME_PART (context->message),
			cancellable, error);

		if (handler_id)
			g_signal_handler_disconnect (context->session, handler_id);

		g_object_unref (cipher);

		if (!success)
			return FALSE;

		if (encrypt_to_self)
			g_ptr_array_set_size (
				context->recipients,
				context->recipients->len - 1);
	}

	/* we replaced the message directly, we don't want to do reparenting foo */
	if (context->smime_encrypt) {
		context->skip_content = TRUE;
	} else {
		CamelDataWrapper *content;

		content = camel_medium_get_content (
			CAMEL_MEDIUM (mime_part));
		context->top_level_part = g_object_ref (content);
	}

	g_object_unref (mime_part);

	return TRUE;
}
#endif

static void
composer_build_message_thread (GTask *task,
                               gpointer source_object,
                               gpointer task_data,
                               GCancellable *cancellable)
{
	AsyncContext *context = task_data;
	GError *error = NULL;

	/* Setup working recipient list if we're encrypting. */
	if (context->pgp_encrypt || context->smime_encrypt) {
		gint ii, jj;

		const gchar *types[] = {
			CAMEL_RECIPIENT_TYPE_TO,
			CAMEL_RECIPIENT_TYPE_CC,
			CAMEL_RECIPIENT_TYPE_BCC
		};

		context->recipients = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
		for (ii = 0; ii < G_N_ELEMENTS (types) && !context->is_draft; ii++) {
			CamelInternetAddress *addr;
			const gchar *address;

			addr = camel_mime_message_get_recipients (
				context->message, types[ii]);
			for (jj = 0; camel_internet_address_get (addr, jj, NULL, &address); jj++)
				g_ptr_array_add (
					context->recipients,
					g_strdup (address));
		}
	}

	if (!composer_build_message_pgp (context, cancellable, &error)) {
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}

#if defined (ENABLE_SMIME)
	if (!composer_build_message_smime (context, cancellable, &error)) {
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}
#endif /* ENABLE_SMIME */
	g_task_return_boolean (task, TRUE);
}

static const gchar *
composer_get_editor_mode_format_text (EContentEditorMode mode)
{
	switch (mode) {
	case E_CONTENT_EDITOR_MODE_UNKNOWN:
		g_warn_if_reached ();
		break;
	case E_CONTENT_EDITOR_MODE_PLAIN_TEXT:
		return "text/plain";
	case E_CONTENT_EDITOR_MODE_HTML:
		return "text/html";
	case E_CONTENT_EDITOR_MODE_MARKDOWN:
		return "text/markdown";
	case E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT:
		return "text/markdown-plain";
	case E_CONTENT_EDITOR_MODE_MARKDOWN_HTML:
		return "text/markdown-html";
	}

	return "text/plain";
}

static void
composer_add_evolution_composer_mode_header (CamelMedium *medium,
                                             EMsgComposer *composer)
{
	EHTMLEditor *editor;
	const gchar *mode;

	editor = e_msg_composer_get_editor (composer);
	mode = composer_get_editor_mode_format_text (e_html_editor_get_mode (editor));

	camel_medium_set_header (medium, "X-Evolution-Composer-Mode", mode);
}

static void
composer_add_evolution_format_header (CamelMedium *medium,
				      ComposerFlags flags,
				      EContentEditorMode mode)
{
	GString *string;

	string = g_string_sized_new (128);

	if ((flags & COMPOSER_FLAG_HTML_CONTENT) != 0 || (
	    (flags & COMPOSER_FLAG_SAVE_DRAFT) != 0 &&
	    mode != E_CONTENT_EDITOR_MODE_MARKDOWN &&
	    mode != E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT &&
	    mode != E_CONTENT_EDITOR_MODE_MARKDOWN_HTML))
		g_string_append (string, "text/html");
	else
		g_string_append (string, "text/plain");

	if (flags & COMPOSER_FLAG_PGP_SIGN)
		g_string_append (string, ", pgp-sign");

	if (flags & COMPOSER_FLAG_PGP_ENCRYPT)
		g_string_append (string, ", pgp-encrypt");

	if (flags & COMPOSER_FLAG_SMIME_SIGN)
		g_string_append (string, ", smime-sign");

	if (flags & COMPOSER_FLAG_SMIME_ENCRYPT)
		g_string_append (string, ", smime-encrypt");

	camel_medium_set_header (
		medium, "X-Evolution-Format", string->str);

	g_string_free (string, TRUE);
}

static void
composer_build_message (EMsgComposer *composer,
                        ComposerFlags flags,
                        gint io_priority,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
	EMsgComposerPrivate *priv;
	GTask *task;
	AsyncContext *context;
	EAttachmentView *view;
	EAttachmentStore *store;
	EAttachment *alternative_body;
	EComposerHeaderTable *table;
	EHTMLEditor *editor;
	CamelDataWrapper *html;
	ESourceMailIdentity *mi;
	const gchar *extension_name;
	const gchar *iconv_charset = NULL;
	const gchar *organization;
	gchar *identity_uid;
	CamelMultipart *body = NULL;
	CamelContentType *type;
	CamelStream *stream;
	CamelStream *mem_stream;
	CamelMimePart *part;
	GByteArray *data;
	ESource *source;
	gchar *charset, *message_uid;
	const gchar *from_domain;
	gboolean mode_is_markdown;
	gint i;
	GError *last_error = NULL;

	e_msg_composer_inc_soft_busy (composer);

	editor = e_msg_composer_get_editor (composer);

	mode_is_markdown = e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_MARKDOWN ||
		e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT ||
		e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;

	priv = composer->priv;
	table = e_msg_composer_get_header_table (composer);

	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
	if (identity_uid) {
		source = e_composer_header_table_ref_source (table, identity_uid);
		g_free (identity_uid);

		g_warn_if_fail (source != NULL);
	} else {
		source = NULL;
	}

	/* Do all the non-blocking work here, and defer
	 * any blocking operations to a separate thread. */

	context = g_slice_new0 (AsyncContext);
	context->source = source;  /* takes the reference */
	context->session = e_msg_composer_ref_session (composer);
	context->from = e_msg_composer_get_from (composer);
	context->is_draft = (flags & COMPOSER_FLAG_SAVE_DRAFT) != 0;
	context->pgp_sign = !context->is_draft && (flags & COMPOSER_FLAG_PGP_SIGN) != 0;
	context->pgp_encrypt = (flags & COMPOSER_FLAG_PGP_ENCRYPT) != 0;
	context->smime_sign = !context->is_draft && (flags & COMPOSER_FLAG_SMIME_SIGN) != 0;
	context->smime_encrypt = (flags & COMPOSER_FLAG_SMIME_ENCRYPT) != 0;
	context->need_thread =
		context->pgp_sign || context->pgp_encrypt ||
		context->smime_sign || context->smime_encrypt;

	task = g_task_new (composer, cancellable, callback, user_data);
	g_task_set_source_tag (task, composer_build_message);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, context, (GDestroyNotify) async_context_free);

	/* If this is a redirected message, just tweak the headers. */
	if (priv->redirect) {
		e_msg_composer_dec_soft_busy (composer);

		context->skip_content = TRUE;
		context->is_redirect = TRUE;
		context->message = g_object_ref (priv->redirect);
		build_message_headers (composer, context->message, TRUE);
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}

	context->message = camel_mime_message_new ();

	if (context->from && camel_internet_address_get (context->from, 0, NULL, &from_domain)) {
		const gchar *at = strchr (from_domain, '@');
		if (at)
			from_domain = at + 1;
		else
			from_domain = NULL;
	} else {
		from_domain = NULL;
	}

	if (!from_domain || !*from_domain)
		from_domain = "localhost";

	message_uid = camel_header_msgid_generate (from_domain);

	/* Explicitly generate a Message-ID header here so it's
	 * consistent for all outbound streams (SMTP, Fcc, etc). */
	camel_mime_message_set_message_id (context->message, message_uid);
	g_free (message_uid);

	build_message_headers (composer, context->message, FALSE);
	for (i = 0; i < priv->extra_hdr_names->len; i++) {
		/* Skip headers related to the templates, they are not meant to be part of the message */
		if (g_ascii_strncasecmp (priv->extra_hdr_names->pdata[i], "X-Evolution-Templates-", 22) != 0) {
			camel_medium_add_header (
				CAMEL_MEDIUM (context->message),
				priv->extra_hdr_names->pdata[i],
				priv->extra_hdr_values->pdata[i]);
		}
	}

	if (source) {
		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		mi = e_source_get_extension (source, extension_name);
		organization = e_source_mail_identity_get_organization (mi);

		/* Disposition-Notification-To */
		if (flags & COMPOSER_FLAG_REQUEST_READ_RECEIPT) {
			EComposerHeader *header;
			const gchar *mdn_address;

			header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_REPLY_TO);
			mdn_address = e_composer_text_header_get_text (E_COMPOSER_TEXT_HEADER (header));

			if (!mdn_address || !*mdn_address) {
				header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_FROM);
				mdn_address = e_composer_from_header_get_address (E_COMPOSER_FROM_HEADER (header));
			}

			if (!mdn_address || !*mdn_address)
				mdn_address = e_source_mail_identity_get_reply_to (mi);
			if (mdn_address == NULL)
				mdn_address = e_source_mail_identity_get_address (mi);
			if (mdn_address != NULL)
				camel_medium_add_header (
					CAMEL_MEDIUM (context->message),
					"Disposition-Notification-To", mdn_address);
		}

		/* Organization */
		if (organization != NULL && *organization != '\0') {
			gchar *encoded_organization;

			encoded_organization = camel_header_encode_string (
				(const guchar *) organization);
			camel_medium_set_header (
				CAMEL_MEDIUM (context->message),
				"Organization", encoded_organization);
			g_free (encoded_organization);
		}
	}

	/* X-Priority */
	if (flags & COMPOSER_FLAG_PRIORITIZE_MESSAGE)
		camel_medium_add_header (
			CAMEL_MEDIUM (context->message),
			"X-Priority", "1");

	if ((flags & COMPOSER_FLAG_DELIVERY_STATUS_NOTIFICATION) != 0)
		camel_medium_add_header (CAMEL_MEDIUM (context->message), "X-Evolution-Request-DSN", "1");

	/* Build the text/plain part. */

	if (priv->mime_body) {
		if (text_requires_quoted_printable (priv->mime_body, -1)) {
			context->plain_encoding =
				CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
		} else {
			context->plain_encoding = CAMEL_TRANSFER_ENCODING_7BIT;
			for (i = 0; priv->mime_body[i]; i++) {
				if ((guchar) priv->mime_body[i] > 127) {
					context->plain_encoding =
					CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
					break;
				}
			}
		}

		data = g_byte_array_new ();
		g_byte_array_append (
			data, (const guint8 *) priv->mime_body,
			strlen (priv->mime_body));
		type = camel_content_type_decode (priv->mime_type);

	} else {
		const gchar *text;
		EContentEditor *cnt_editor;

		cnt_editor = e_html_editor_get_content_editor (editor);
		data = g_byte_array_new ();

		text = e_content_editor_util_get_content_data (e_msg_composer_get_content_hash (composer),
			E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);

		if (!text) {
			g_warning ("%s: Failed to retrieve text/plain processed content", G_STRFUNC);
			text = "";

			last_error = e_content_editor_dup_last_error (cnt_editor);
			if (!last_error) {
				last_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
					_("Failed to retrieve text/plain processed content"));
			}
		}

		g_byte_array_append (data, (guint8 *) text, strlen (text));
		if (!g_str_has_suffix (text, "\r\n") && !g_str_has_suffix (text, "\n"))
			g_byte_array_append (data, (const guint8 *) "\r\n", 2);

		if (e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_MARKDOWN || (
		    (flags & COMPOSER_FLAG_SAVE_DRAFT) != 0 &&
		    e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML))
			type = camel_content_type_new ("text", "markdown");
		else
			type = camel_content_type_new ("text", "plain");
		charset = best_charset (
			data, priv->charset, &context->plain_encoding);
		if (charset != NULL) {
			camel_content_type_set_param (type, "charset", charset);
			iconv_charset = camel_iconv_charset_name (charset);
			g_free (charset);
		}

		if ((flags & COMPOSER_FLAG_SAVE_DRAFT) == 0 && mode_is_markdown)
			composer_add_evolution_composer_mode_header (CAMEL_MEDIUM (context->message), composer);
	}

	mem_stream = camel_stream_mem_new_with_byte_array (data);
	stream = camel_stream_filter_new (mem_stream);
	g_object_unref (mem_stream);

	/* Convert the stream to the appropriate charset. */
	if (iconv_charset && g_ascii_strcasecmp (iconv_charset, "UTF-8") != 0)
		composer_add_charset_filter (stream, iconv_charset);

	/* Encode the stream to quoted-printable if necessary. */
	if (context->plain_encoding == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE)
		composer_add_quoted_printable_filter (stream);

	/* Construct the content object.  This does not block since
	 * we're constructing the data wrapper from a memory stream. */
	context->top_level_part = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream_sync (
		context->top_level_part, stream, NULL, NULL);
	g_object_unref (stream);

	context->text_plain_part = g_object_ref (context->top_level_part);

	/* Avoid re-encoding the data when adding it to a MIME part. */
	if (context->plain_encoding == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE)
		camel_data_wrapper_set_encoding (context->top_level_part, context->plain_encoding);

	camel_data_wrapper_set_mime_type_field (
		context->top_level_part, type);

	camel_content_type_unref (type);

	alternative_body = NULL;
	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	if (composer->priv->alternative_body_attachment) {
		GList *attachments, *link;

		attachments = e_attachment_store_get_attachments (store);

		for (link = attachments; link; link = g_list_next (link)) {
			EAttachment *attachment = link->data;

			/* Skip the attachment if it's still loading. */
			if (!e_attachment_get_loading (attachment) &&
			    attachment == composer->priv->alternative_body_attachment) {
				alternative_body = g_object_ref (attachment);
				break;
			}
		}

		g_list_free_full (attachments, g_object_unref);

		if (!alternative_body)
			composer->priv->alternative_body_attachment = NULL;
	}

	/* Build the text/html part, and wrap it and the text/plain part
	 * in a multipart/alternative part.  Additionally, if there are
	 * inline images then wrap the multipart/alternative part along
	 * with the images in a multipart/related part.
	 *
	 * So the structure of all this will be:
	 *
	 *    multipart/related
	 *        multipart/alternative
	 *            text/plain
	 *            text/html
	 *        image/<<whatever>>
	 *        image/<<whatever>>
	 *        ...
	 */

	if ((flags & COMPOSER_FLAG_HTML_CONTENT) != 0 ||
	    ((flags & COMPOSER_FLAG_SAVE_DRAFT) != 0 && !mode_is_markdown)) {
		const gchar *text;
		gsize length;
		gboolean pre_encode;
		GSList *inline_images_parts = NULL, *link;

		data = g_byte_array_new ();
		if ((flags & COMPOSER_FLAG_SAVE_DRAFT) != 0) {
			/* X-Evolution-Format */
			composer_add_evolution_format_header (
				CAMEL_MEDIUM (context->message), flags, e_html_editor_get_mode (composer->priv->editor));

			/* X-Evolution-Composer-Mode */
			composer_add_evolution_composer_mode_header (
				CAMEL_MEDIUM (context->message), composer);

			text = e_content_editor_util_get_content_data (e_msg_composer_get_content_hash (composer),
				E_CONTENT_EDITOR_GET_RAW_DRAFT);

			if (!text) {
				g_warning ("%s: Failed to retrieve draft content", G_STRFUNC);
				text = "";

				if (!last_error) {
					last_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
						_("Failed to retrieve draft content"));
				}
			}
		} else {
			text = e_content_editor_util_get_content_data (e_msg_composer_get_content_hash (composer),
				E_CONTENT_EDITOR_GET_TO_SEND_HTML);

			if (!text) {
				g_warning ("%s: Failed to retrieve HTML processed content", G_STRFUNC);
				text = "";

				if (!last_error) {
					last_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
						_("Failed to retrieve HTML processed content"));
				}
			}
		}

		inline_images_parts = e_content_editor_util_get_content_data (e_msg_composer_get_content_hash (composer),
			E_CONTENT_EDITOR_GET_INLINE_IMAGES);

		length = strlen (text);
		g_byte_array_append (data, (guint8 *) text, (guint) length);
		if (!g_str_has_suffix (text, "\r\n") && !g_str_has_suffix (text, "\n"))
			g_byte_array_append (data, (const guint8 *) "\r\n", 2);
		pre_encode = text_requires_quoted_printable (text, length);

		mem_stream = camel_stream_mem_new_with_byte_array (data);
		stream = camel_stream_filter_new (mem_stream);
		g_object_unref (mem_stream);

		if (pre_encode)
			composer_add_quoted_printable_filter (stream);

		/* Construct the content object.  This does not block since
		 * we're constructing the data wrapper from a memory stream. */
		html = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream_sync (
			html, stream, NULL, NULL);
		g_object_unref (stream);

		camel_data_wrapper_set_mime_type (html, "text/html; charset=utf-8");

		/* Avoid re-encoding the data when adding it to a MIME part. */
		if (pre_encode)
			camel_data_wrapper_set_encoding (html, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);

		/* Build the multipart/alternative */
		body = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (
			CAMEL_DATA_WRAPPER (body), "multipart/alternative");
		camel_multipart_set_boundary (body, NULL);

		/* Add the text/plain part. */
		part = camel_mime_part_new ();
		camel_medium_set_content (
			CAMEL_MEDIUM (part), context->top_level_part);
		camel_mime_part_set_encoding (part, context->plain_encoding);
		camel_multipart_add_part (body, part);
		g_object_unref (part);

		/* Add the text/html part. */
		part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (part), html);
		camel_mime_part_set_encoding (
			part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
		camel_multipart_add_part (body, part);
		g_object_unref (part);

		g_object_unref (context->top_level_part);
		g_object_unref (html);

		if (alternative_body)
			e_attachment_add_to_multipart (alternative_body, body, composer->priv->charset);

		/* If there are inlined images, construct a multipart/related
		 * containing the multipart/alternative and the images. */
		if (inline_images_parts) {
			CamelMultipart *html_with_images;

			html_with_images = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (
				CAMEL_DATA_WRAPPER (html_with_images),
				"multipart/related; "
				"type=\"multipart/alternative\"");
			camel_multipart_set_boundary (html_with_images, NULL);

			part = camel_mime_part_new ();
			camel_medium_set_content (
				CAMEL_MEDIUM (part),
				CAMEL_DATA_WRAPPER (body));
			camel_multipart_add_part (html_with_images, part);
			g_object_unref (part);

			g_object_unref (body);

			for (link = inline_images_parts; link; link = g_slist_next (link)) {
				part = link->data;

				camel_multipart_add_part (html_with_images, g_object_ref (part));
			}

			context->top_level_part =
				CAMEL_DATA_WRAPPER (html_with_images);
		} else {
			context->top_level_part =
				CAMEL_DATA_WRAPPER (body);
		}
	} else {
		/* cover drafts in the markdown mode */
		if ((flags & COMPOSER_FLAG_SAVE_DRAFT) != 0 && mode_is_markdown) {
			/* X-Evolution-Format */
			composer_add_evolution_format_header (
				CAMEL_MEDIUM (context->message), flags, e_html_editor_get_mode (composer->priv->editor));

			/* X-Evolution-Composer-Mode */
			composer_add_evolution_composer_mode_header (
				CAMEL_MEDIUM (context->message), composer);
		}

		if (alternative_body) {
			body = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body), "multipart/alternative");
			camel_multipart_set_boundary (body, NULL);

			part = camel_mime_part_new ();
			camel_medium_set_content (CAMEL_MEDIUM (part), context->top_level_part);
			camel_mime_part_set_encoding (part, context->plain_encoding);
			camel_multipart_add_part (body, part);
			g_object_unref (part);

			e_attachment_add_to_multipart (alternative_body, body, composer->priv->charset);

			g_object_unref (context->top_level_part);
			context->top_level_part = CAMEL_DATA_WRAPPER (body);
		}
	}

	/* If there are attachments, wrap what we've built so far
	 * along with the attachments in a multipart/mixed part. */
	if (e_attachment_store_get_num_attachments (store) - (alternative_body ? 1 : 0) > 0) {
		CamelMultipart *multipart = camel_multipart_new ();

		/* Generate a random boundary. */
		camel_multipart_set_boundary (multipart, NULL);

		part = camel_mime_part_new ();
		camel_medium_set_content (
			CAMEL_MEDIUM (part),
			context->top_level_part);
		if (context->top_level_part == context->text_plain_part)
			camel_mime_part_set_encoding (
				part, context->plain_encoding);
		camel_multipart_add_part (multipart, part);
		g_object_unref (part);

		if (alternative_body) {
			GList *attachments, *link;

			attachments = e_attachment_store_get_attachments (store);

			for (link = attachments; link; link = g_list_next (link)) {
				EAttachment *attachment = link->data;

				/* Skip the attachment if it's still loading. */
				if (!e_attachment_get_loading (attachment) &&
				    attachment != alternative_body) {
					e_attachment_add_to_multipart (attachment, multipart, composer->priv->charset);
				}
			}

			g_list_free_full (attachments, g_object_unref);
		} else {
			e_attachment_store_add_to_multipart (store, multipart, priv->charset);
		}

		g_object_unref (context->top_level_part);
		context->top_level_part = CAMEL_DATA_WRAPPER (multipart);
	}

	g_clear_object (&alternative_body);

	if (last_error) {
		g_task_return_error (task, g_steal_pointer (&last_error));
	/* Run any blocking operations in a separate thread. */
	} else if (context->need_thread) {
		if (!context->is_draft)
			context->recipients_with_certificate = composer_get_completed_recipients_with_certificate (composer);

		g_task_run_in_thread (task, composer_build_message_thread);
	} else {
		g_task_return_boolean (task, TRUE);
	}

	e_msg_composer_dec_soft_busy (composer);

	g_object_unref (task);
}

static CamelMimeMessage *
composer_build_message_finish (EMsgComposer *composer,
                               GAsyncResult *result,
                               GError **error)
{
	GTask *task;
	AsyncContext *context;

	g_return_val_if_fail (g_task_is_valid (result, composer), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, composer_build_message), NULL);

	task = G_TASK (result);
	context = g_task_get_task_data (task);
	if (!g_task_propagate_boolean (task, error))
		return NULL;

	/* Finalize some details before returning. */

	if (!context->skip_content) {
		if (context->top_level_part != context->text_plain_part &&
		    CAMEL_IS_MIME_PART (context->top_level_part)) {
			CamelDataWrapper *content;
			CamelMedium *imedium, *omedium;
			const CamelNameValueArray *headers;

			imedium = CAMEL_MEDIUM (context->top_level_part);
			omedium = CAMEL_MEDIUM (context->message);

			content = camel_medium_get_content (imedium);
			camel_medium_set_content (omedium, content);
			camel_data_wrapper_set_encoding (CAMEL_DATA_WRAPPER (omedium), camel_data_wrapper_get_encoding (CAMEL_DATA_WRAPPER (imedium)));

			headers = camel_medium_get_headers (imedium);
			if (headers) {
				gint ii, length;
				length = camel_name_value_array_get_length (headers);

				for (ii = 0; ii < length; ii++) {
					const gchar *header_name = NULL;
					const gchar *header_value = NULL;

					if (camel_name_value_array_get (headers, ii, &header_name, &header_value))
						camel_medium_set_header (omedium, header_name, header_value);
				}
			}
		} else {
			camel_medium_set_content (
				CAMEL_MEDIUM (context->message),
				context->top_level_part);
		}
	}

	if (!context->is_redirect && context->top_level_part == context->text_plain_part) {
		camel_mime_part_set_encoding (
			CAMEL_MIME_PART (context->message),
			context->plain_encoding);
	}

	return g_object_ref (context->message);
}

/* Signatures */

static void
set_editor_text (EMsgComposer *composer,
                 const gchar *text,
                 gboolean is_html,
                 gboolean set_signature)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (text != NULL);

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_html_editor_cancel_mode_change_content_update (editor);

	if (is_html)
		e_content_editor_insert_content (
			cnt_editor,
			text,
			E_CONTENT_EDITOR_INSERT_TEXT_HTML |
			E_CONTENT_EDITOR_INSERT_REPLACE_ALL |
			(e_msg_composer_get_is_reply_or_forward (composer) ? E_CONTENT_EDITOR_INSERT_CLEANUP_SIGNATURE_ID : 0));
	else
		e_content_editor_insert_content (
			cnt_editor,
			text,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
			E_CONTENT_EDITOR_INSERT_REPLACE_ALL);

	if (set_signature)
		e_composer_update_signature (composer);
}

/* Miscellaneous callbacks.  */

static void
attachment_store_changed_cb (EMsgComposer *composer)
{
	EHTMLEditor *editor;

	/* Mark the editor as changed so it prompts about unsaved
	 * changes on close. */
	editor = e_msg_composer_get_editor (composer);
	if (editor) {
		EContentEditor *cnt_editor;

		cnt_editor = e_html_editor_get_content_editor (editor);
		e_content_editor_set_changed (cnt_editor, TRUE);
	}
}

static void
msg_composer_subject_changed_cb (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	const gchar *subject;

	table = e_msg_composer_get_header_table (composer);
	subject = e_composer_header_table_get_subject (table);

	if (subject == NULL || *subject == '\0')
		subject = _("Compose Message");

	gtk_window_set_title (GTK_WINDOW (composer), subject);
}

static gboolean
msg_composer_get_can_sign (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	ESource *source;
	gboolean can_sign = TRUE;
	gchar *uid;

	if (!e_msg_composer_get_is_imip (composer))
		return TRUE;

	table = e_msg_composer_get_header_table (composer);
	uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);

	if (!uid)
		return TRUE;

	source = e_composer_header_table_ref_source (table, uid);
	if (source) {
		ESourceMailComposition *mc;

		mc = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
		can_sign = e_source_mail_composition_get_sign_imip (mc);

		g_object_unref (source);
	}

	g_free (uid);

	return can_sign;
}

static void
msg_composer_mail_identity_changed_cb (EMsgComposer *composer)
{
	EMailSignatureComboBox *combo_box;
	ESourceMailComposition *mc;
	ESourceOpenPGP *pgp;
	ESourceSMIME *smime;
	EComposerHeaderTable *table;
	EContentEditor *cnt_editor;
	EUIAction *action;
	ESource *source;
	gboolean active;
	gboolean can_sign;
	gboolean pgp_sign;
	gboolean pgp_encrypt;
	gboolean smime_sign;
	gboolean smime_encrypt;
	gboolean composer_realized;
	gboolean was_disable_signature, unset_signature = FALSE;
	const gchar *extension_name;
	const gchar *active_signature_id;
	gchar *uid, *alias_name = NULL, *alias_address = NULL, *smime_cert;

	cnt_editor = e_html_editor_get_content_editor (e_msg_composer_get_editor (composer));
	table = e_msg_composer_get_header_table (composer);
	uid = e_composer_header_table_dup_identity_uid (table, &alias_name, &alias_address);

	/* Silently return if no identity is selected. */
	if (!uid) {
		e_msg_composer_check_autocrypt (composer, NULL);
		e_content_editor_set_start_bottom (cnt_editor, E_THREE_STATE_INCONSISTENT);
		e_content_editor_set_top_signature (cnt_editor,
			e_msg_composer_get_is_reply_or_forward (composer) ? E_THREE_STATE_INCONSISTENT :
			E_THREE_STATE_OFF);

		g_free (alias_name);
		g_free (alias_address);
		return;
	}

	source = e_composer_header_table_ref_source (table, uid);
	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	mc = e_source_get_extension (source, extension_name);

	e_content_editor_set_start_bottom (cnt_editor,
		e_source_mail_composition_get_start_bottom (mc));
	e_content_editor_set_top_signature (cnt_editor,
		e_msg_composer_get_is_reply_or_forward (composer) ? e_source_mail_composition_get_top_signature (mc) :
		E_THREE_STATE_OFF);

	extension_name = E_SOURCE_EXTENSION_OPENPGP;
	pgp = e_source_get_extension (source, extension_name);
	pgp_sign = e_source_openpgp_get_sign_by_default (pgp);
	pgp_encrypt = e_source_openpgp_get_encrypt_by_default (pgp);

	extension_name = E_SOURCE_EXTENSION_SMIME;
	smime = e_source_get_extension (source, extension_name);
	smime_cert = e_source_smime_dup_signing_certificate (smime);
	smime_sign = smime_cert && *smime_cert && e_source_smime_get_sign_by_default (smime);
	g_free (smime_cert);
	smime_cert = e_source_smime_dup_encryption_certificate (smime);
	smime_encrypt = smime_cert && *smime_cert && e_source_smime_get_encrypt_by_default (smime);
	g_free (smime_cert);

	can_sign = msg_composer_get_can_sign (composer);

	/* Preserve options only if the composer was realized, otherwise an account
	   change according to current folder or similar reasons can cause the options
	   to be set, when the default account has it set, but the other not. */
	composer_realized = gtk_widget_get_realized (GTK_WIDGET (composer));

	action = ACTION (PGP_SIGN);
	active = composer_realized && e_ui_action_get_active (action);
	active |= (can_sign && pgp_sign);
	e_ui_action_set_active (action, active);

	action = ACTION (PGP_ENCRYPT);
	active = composer_realized && e_ui_action_get_active (action);
	active |= pgp_encrypt;
	e_ui_action_set_active (action, active);

	action = ACTION (SMIME_SIGN);
	active = composer_realized && e_ui_action_get_active (action);
	active |= (can_sign && smime_sign);
	e_ui_action_set_active (action, active);

	action = ACTION (SMIME_ENCRYPT);
	active = composer_realized && e_ui_action_get_active (action);
	active |= smime_encrypt;
	e_ui_action_set_active (action, active);

	was_disable_signature = composer->priv->disable_signature;

	if (e_msg_composer_get_is_reply_or_forward (composer)) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		unset_signature = g_settings_get_boolean (settings, "composer-signature-in-new-only");
		g_object_unref (settings);
	}

	combo_box = e_composer_header_table_get_signature_combo_box (table);

	if (unset_signature)
		composer->priv->disable_signature = TRUE;

	e_mail_signature_combo_box_set_identity (combo_box, uid, alias_name, alias_address);

	if (unset_signature)
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), "none");

	composer->priv->disable_signature = was_disable_signature;

	g_object_unref (source);
	g_free (uid);

	active_signature_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
	if (unset_signature || g_strcmp0 (active_signature_id, E_MAIL_SIGNATURE_AUTOGENERATED_UID) == 0)
		e_composer_update_signature (composer);

	g_free (alias_name);
	g_free (alias_address);

	e_msg_composer_check_autocrypt (composer, NULL);
}

static gboolean
msg_composer_paste_clipboard_targets (EMsgComposer *composer,
				      GtkClipboard *clipboard,
				      GdkAtom *targets,
				      gint n_targets)
{
	EHTMLEditor *editor;

	if (targets == NULL || n_targets < 0)
		return FALSE;

	editor = e_msg_composer_get_editor (composer);

	if (e_html_editor_get_mode (editor) != E_CONTENT_EDITOR_MODE_HTML &&
	    gtk_targets_include_image (targets, n_targets, TRUE)) {
		e_composer_paste_image (composer, clipboard);
		return TRUE;
	}

	if (gtk_targets_include_uri (targets, n_targets)) {
		e_composer_paste_uris (composer, clipboard);
		return TRUE;
	}

	return FALSE;
}

static gboolean
msg_composer_paste_clipboard (EMsgComposer *composer,
			      GdkAtom selection)
{
	GtkClipboard *clipboard;
	GdkAtom *targets = NULL;
	gint n_targets;
	gboolean handled = FALSE;

	clipboard = gtk_clipboard_get (selection);

	if (gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets)) {
		handled = msg_composer_paste_clipboard_targets (composer, clipboard, targets, n_targets);
		g_free (targets);
	}

	return handled;
}

static gboolean
msg_composer_paste_primary_clipboard_cb (EContentEditor *cnt_editor,
                                         EMsgComposer *composer)
{
	return msg_composer_paste_clipboard (composer, GDK_SELECTION_PRIMARY);
}

static gboolean
msg_composer_paste_clipboard_cb (EContentEditor *cnt_editor,
                                 EMsgComposer *composer)
{
	return msg_composer_paste_clipboard (composer, GDK_SELECTION_CLIPBOARD);
}

static void
msg_composer_drag_data_received_cb (GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection,
                                    guint info,
                                    guint time,
                                    EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean html_mode, is_move;
	gchar *moz_url = NULL;

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	html_mode = e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML;

	g_signal_handler_disconnect (cnt_editor, composer->priv->drag_data_received_handler_id);
	composer->priv->drag_data_received_handler_id = 0;

	is_move = gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE;

	/* HTML mode has a few special cases for drops... */
	/* If we're receiving URIs and -all- the URIs point to
	 * image files, we want the image(s) to be inserted in
	 * the message body. */
	if (html_mode &&
	    (e_composer_selection_is_image_uris (composer, selection) ||
	     e_composer_selection_is_base64_uris (composer, selection))) {
		const guchar *data;
		gint length;
		gint list_len, len;
		gchar *uri;

		data = gtk_selection_data_get_data (selection);
		length = gtk_selection_data_get_length (selection);

		if (!data || length < 0) {
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}

		e_content_editor_move_caret_on_coordinates (cnt_editor, x, y, FALSE);

		list_len = length;
		do {
			uri = e_util_next_uri_from_uri_list ((guchar **) &data, &len, &list_len);
			e_content_editor_insert_image (cnt_editor, uri);
			g_free (uri);
		} while (list_len);

		gtk_drag_finish (context, TRUE, is_move, time);
	} else if (html_mode && e_composer_selection_is_moz_url_image (composer, selection, &moz_url)) {
		e_content_editor_move_caret_on_coordinates (cnt_editor, x, y, FALSE);
		e_content_editor_insert_image (cnt_editor, moz_url);

		g_free (moz_url);

		gtk_drag_finish (context, TRUE, is_move, time);
	} else {
		EAttachmentView *attachment_view =
			e_msg_composer_get_attachment_view (composer);
		/* Forward the data to the attachment view.  Note that calling
		 * e_attachment_view_drag_data_received() will not work because
		 * that function only handles the case where all the other drag
		 * handlers have failed. */
		e_attachment_paned_drag_data_received (
			E_ATTACHMENT_PANED (attachment_view),
			context, x, y, selection, info, time);
	}
}

static gboolean
msg_composer_drag_drop_cb (GtkWidget *widget,
                           GdkDragContext *context,
                           gint x,
                           gint y,
                           guint time,
                           EMsgComposer *composer)
{
	GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

	if (target == GDK_NONE) {
		gdk_drag_status (context, 0, time);
	} else {
		composer->priv->drag_data_received_handler_id = g_signal_connect (
			E_CONTENT_EDITOR (widget), "drag-data-received",
			G_CALLBACK (msg_composer_drag_data_received_cb), composer);

		gtk_drag_get_data (widget, context, target, time);

		return TRUE;
	}

	return FALSE;
}

static void
msg_composer_drop_handled_cb (EContentEditor *cnt_editor,
                              EMsgComposer *composer)
{
	if (composer->priv->drag_data_received_handler_id != 0) {
		g_signal_handler_disconnect (cnt_editor, composer->priv->drag_data_received_handler_id);
		composer->priv->drag_data_received_handler_id = 0;
	}
}

static void
msg_composer_drag_begin_cb (GtkWidget *widget,
                            GdkDragContext *context,
                            EMsgComposer *composer)
{
	if (composer->priv->drag_data_received_handler_id != 0) {
		g_signal_handler_disconnect (E_CONTENT_EDITOR( widget), composer->priv->drag_data_received_handler_id);
		composer->priv->drag_data_received_handler_id = 0;
	}
}

static void
msg_composer_notify_header_cb (EMsgComposer *composer)
{
	EContentEditor *cnt_editor;
	EHTMLEditor *editor;

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_changed (cnt_editor, TRUE);
}

static gboolean
msg_composer_delete_event_cb (EMsgComposer *composer)
{
	/* If the "async" action group is insensitive, it means an
	 * asynchronous operation is in progress.  Block the event. */
	if (!e_ui_action_group_get_sensitive (composer->priv->async_actions))
		return TRUE;

	g_action_activate (G_ACTION (ACTION (CLOSE)), NULL);

	return TRUE;
}

static void
msg_composer_realize_cb (EMsgComposer *composer)
{
	GSettings *settings;
	EUIAction *action;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	action = ACTION (TOOLBAR_PGP_SIGN);
	if (e_ui_action_get_visible (action) && !e_ui_action_get_active (action))
		e_ui_action_set_visible (action, FALSE);

	action = ACTION (TOOLBAR_PGP_ENCRYPT);
	if (e_ui_action_get_visible (action) && !e_ui_action_get_active (action))
		e_ui_action_set_visible (action, FALSE);

	action = ACTION (TOOLBAR_SMIME_SIGN);
	if (e_ui_action_get_visible (action) && !e_ui_action_get_active (action))
		e_ui_action_set_visible (action, FALSE);

	action = ACTION (TOOLBAR_SMIME_ENCRYPT);
	if (e_ui_action_get_visible (action) && !e_ui_action_get_active (action))
		e_ui_action_set_visible (action, FALSE);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (g_settings_get_boolean (settings, "composer-toolbar-show-sign-encrypt")) {
		EComposerHeaderTable *table;
		ESource *source;
		gchar *identity_uid;

		table = e_msg_composer_get_header_table (composer);
		identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
		source = e_composer_header_table_ref_source (table, identity_uid);

		if (source) {
			if (e_source_has_extension (source, E_SOURCE_EXTENSION_OPENPGP)) {
				gchar *key_id;

				key_id = e_source_openpgp_dup_key_id (e_source_get_extension (source, E_SOURCE_EXTENSION_OPENPGP));

				if (key_id && *key_id) {
					e_ui_action_set_visible (ACTION (TOOLBAR_PGP_SIGN), TRUE);
					e_ui_action_set_visible (ACTION (TOOLBAR_PGP_ENCRYPT), TRUE);
				}

				g_free (key_id);
			}

			if (e_source_has_extension (source, E_SOURCE_EXTENSION_SMIME)) {
				ESourceSMIME *smime_extension;
				gchar *certificate;

				smime_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_SMIME);

				certificate = e_source_smime_dup_signing_certificate (smime_extension);
				if (certificate && *certificate)
					e_ui_action_set_visible (ACTION (TOOLBAR_SMIME_SIGN), TRUE);
				g_free (certificate);

				certificate = e_source_smime_dup_encryption_certificate (smime_extension);
				if (certificate && *certificate)
					e_ui_action_set_visible (ACTION (TOOLBAR_SMIME_ENCRYPT), TRUE);
				g_free (certificate);
			}

			g_clear_object (&source);
		}

		g_free (identity_uid);
	}

	g_clear_object (&settings);
}

static void
msg_composer_prepare_for_quit_cb (EShell *shell,
                                  EActivity *activity,
                                  EMsgComposer *composer)
{
	if (e_msg_composer_is_exiting (composer)) {
		/* needs save draft first */
		g_object_ref (activity);
		g_object_weak_ref (
			G_OBJECT (composer), (GWeakNotify)
			g_object_unref, activity);
		g_action_activate (G_ACTION (ACTION (SAVE_DRAFT)), NULL);
	}
}

static void
msg_composer_quit_requested_cb (EShell *shell,
                                EShellQuitReason reason,
                                EMsgComposer *composer)
{
	if (e_msg_composer_is_exiting (composer)) {
		g_signal_handlers_disconnect_by_func (
			shell, msg_composer_quit_requested_cb, composer);
		g_signal_handlers_disconnect_by_func (
			shell, msg_composer_prepare_for_quit_cb, composer);
	} else if (!e_msg_composer_can_close (composer, FALSE) &&
			!e_msg_composer_is_exiting (composer)) {
		e_shell_cancel_quit (shell);
	}
}

static void
msg_composer_set_editor (EMsgComposer *composer,
			 EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (composer->priv->editor == NULL);

	composer->priv->editor = g_object_ref_sink (editor);
}

static void
msg_composer_set_shell (EMsgComposer *composer,
                        EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (composer->priv->shell == NULL);

	composer->priv->shell = shell;

	g_object_add_weak_pointer (
		G_OBJECT (shell), &composer->priv->shell);
}

static void
msg_composer_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			msg_composer_set_editor (
				E_MSG_COMPOSER (object),
				g_value_get_object (value));
			return;

		case PROP_IS_REPLY_OR_FORWARD:
			e_msg_composer_set_is_reply_or_forward (
				E_MSG_COMPOSER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHELL:
			msg_composer_set_shell (
				E_MSG_COMPOSER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
msg_composer_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BUSY:
			g_value_set_boolean (
				value, e_msg_composer_is_busy (
				E_MSG_COMPOSER (object)));
			return;

		case PROP_SOFT_BUSY:
			g_value_set_boolean (
				value, e_msg_composer_is_soft_busy (
				E_MSG_COMPOSER (object)));
			return;

		case PROP_EDITOR:
			g_value_set_object (
				value, e_msg_composer_get_editor (
				E_MSG_COMPOSER (object)));
			return;

		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value, e_msg_composer_get_focus_tracker (
				E_MSG_COMPOSER (object)));
			return;

		case PROP_IS_REPLY_OR_FORWARD:
			g_value_set_boolean (
				value, e_msg_composer_get_is_reply_or_forward (
				E_MSG_COMPOSER (object)));
			return;

		case PROP_SHELL:
			g_value_set_object (
				value, e_msg_composer_get_shell (
				E_MSG_COMPOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
msg_composer_finalize (GObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);

	e_composer_private_finalize (composer);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_msg_composer_parent_class)->finalize (object);
}

static void
msg_composer_gallery_drag_data_get (GtkIconView *icon_view,
                                    GdkDragContext *context,
                                    GtkSelectionData *selection_data,
                                    guint target_type,
                                    guint time)
{
	GtkTreePath *path;
	GtkCellRenderer *cell;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkAtom target;
	gchar *str_data;

	if (!gtk_icon_view_get_cursor (icon_view, &path, &cell))
		return;

	target = gtk_selection_data_get_target (selection_data);

	model = gtk_icon_view_get_model (icon_view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 1, &str_data, -1);
	gtk_tree_path_free (path);

	/* only supports "text/uri-list" */
	gtk_selection_data_set (
		selection_data, target, 8,
		(guchar *) str_data, strlen (str_data));
	g_free (str_data);
}

static void
composer_notify_activity_cb (EActivityBar *activity_bar,
                             GParamSpec *pspec,
                             EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean has_activities;

	has_activities = (e_activity_bar_get_activity (activity_bar) != NULL);

	if (has_activities == composer->priv->had_activities)
		return;

	composer->priv->had_activities = has_activities;

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (has_activities) {
		e_msg_composer_save_focused_widget (composer);

		composer->priv->saved_editable = e_content_editor_is_editable (cnt_editor);
		e_content_editor_set_editable (cnt_editor, FALSE);
	} else {
		e_content_editor_set_editable (cnt_editor, composer->priv->saved_editable);

		e_msg_composer_restore_focus_on_composer (composer);
	}

	g_object_notify (G_OBJECT (composer), "busy");
	g_object_notify (G_OBJECT (composer), "soft-busy");
}

static void
msg_composer_notify_mode_cb (GObject *editor,
			     GParamSpec *param,
			     gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (E_HTML_EDITOR (editor));
	if (!cnt_editor)
		return;

	/* in case the cnt_editor has the handler already connected, which can
	   happen when the user changes the editor mode forth and back */
	g_signal_handlers_disconnect_by_func (cnt_editor, G_CALLBACK (msg_composer_paste_clipboard_cb), composer);
	g_signal_handlers_disconnect_by_func (cnt_editor, G_CALLBACK (msg_composer_paste_primary_clipboard_cb), composer);

	g_signal_connect (
		cnt_editor, "paste-clipboard",
		G_CALLBACK (msg_composer_paste_clipboard_cb), composer);

	g_signal_connect (
		cnt_editor, "paste-primary-clipboard",
		G_CALLBACK (msg_composer_paste_primary_clipboard_cb), composer);
}

static void
msg_composer_constructed (GObject *object)
{
	EShell *shell;
	EMsgComposer *composer;
	EActivityBar *activity_bar;
	EAttachmentView *attachment_view;
	EAttachmentStore *store;
	EComposerHeaderTable *table;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EUIManager *ui_manager;
	EUIAction *action;
	GtkTargetList *target_list;
	GtkTargetEntry *targets;
	gint n_targets;
	GSettings *settings;
	gboolean active;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_msg_composer_parent_class)->constructed (object);

	composer = E_MSG_COMPOSER (object);

	g_return_if_fail (E_IS_HTML_EDITOR (composer->priv->editor));

	shell = e_msg_composer_get_shell (composer);
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	ui_manager = e_html_editor_get_ui_manager (editor);

	e_ui_manager_freeze (ui_manager);

	e_composer_private_constructed (composer);

	attachment_view = e_msg_composer_get_attachment_view (composer);
	table = E_COMPOSER_HEADER_TABLE (composer->priv->header_table);

	gtk_window_set_title (GTK_WINDOW (composer), _("Compose Message"));
	gtk_window_set_icon_name (GTK_WINDOW (composer), "mail-message-new");
	gtk_window_set_default_size (GTK_WINDOW (composer), 600, 500);
	gtk_window_set_position (GTK_WINDOW (composer), GTK_WIN_POS_CENTER);

	g_signal_connect (
		object, "delete-event",
		G_CALLBACK (msg_composer_delete_event_cb), NULL);

	g_signal_connect (
		object, "realize",
		G_CALLBACK (msg_composer_realize_cb), NULL);

	gtk_application_add_window (
		GTK_APPLICATION (shell), GTK_WINDOW (object));

	g_signal_connect (
		shell, "quit-requested",
		G_CALLBACK (msg_composer_quit_requested_cb), composer);

	g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (msg_composer_prepare_for_quit_cb), composer);

	/* Restore Persistent State */

	e_restore_window (
		GTK_WINDOW (composer),
		"/org/gnome/evolution/mail/composer-window/",
		E_RESTORE_WINDOW_SIZE);

	activity_bar = e_html_editor_get_activity_bar (editor);
	g_signal_connect (
		activity_bar, "notify::activity",
		G_CALLBACK (composer_notify_activity_cb), composer);


	/* Honor User Preferences */

	/* FIXME This should be an EMsgComposer property. */
	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	action = ACTION (REQUEST_READ_RECEIPT);
	active = g_settings_get_boolean (settings, "composer-request-receipt");
	e_ui_action_set_active (action, active);

	action = ACTION (DELIVERY_STATUS_NOTIFICATION);
	active = g_settings_get_boolean (settings, "composer-request-dsn");
	e_ui_action_set_active (action, active);

	g_object_unref (settings);

	g_signal_connect_object (editor, "notify::mode",
		G_CALLBACK (msg_composer_notify_mode_cb), composer, 0);

	/* Clipboard Support */

	g_signal_connect (
		cnt_editor, "paste-clipboard",
		G_CALLBACK (msg_composer_paste_clipboard_cb), composer);

	g_signal_connect (
		cnt_editor, "paste-primary-clipboard",
		G_CALLBACK (msg_composer_paste_primary_clipboard_cb), composer);

	/* Drag-and-Drop Support */
	g_signal_connect (
		cnt_editor, "drag-drop",
		G_CALLBACK (msg_composer_drag_drop_cb), composer);

	g_signal_connect (
		cnt_editor, "drag-begin",
		G_CALLBACK (msg_composer_drag_begin_cb), composer);

	g_signal_connect (
		cnt_editor, "drop-handled",
		G_CALLBACK (msg_composer_drop_handled_cb), composer);

	g_signal_connect (
		composer->priv->gallery_icon_view, "drag-data-get",
		G_CALLBACK (msg_composer_gallery_drag_data_get), NULL);

	/* Configure Headers */

	composer->priv->notify_destinations_bcc_handler = e_signal_connect_notify_swapped (
		table, "notify::destinations-bcc",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	composer->priv->notify_destinations_cc_handler = e_signal_connect_notify_swapped (
		table, "notify::destinations-cc",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	composer->priv->notify_destinations_to_handler = e_signal_connect_notify_swapped (
		table, "notify::destinations-to",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	/* Do not use e_signal_connect_notify_swapped() here, it it avoids notification
	   when the property didn't change, but it's about the consolidated property,
	   identity uid, name and address, where only one of the three can change. */
	composer->priv->notify_identity_uid_handler = g_signal_connect_swapped (
		table, "notify::identity-uid",
		G_CALLBACK (msg_composer_mail_identity_changed_cb), composer);
	composer->priv->notify_reply_to_handler = e_signal_connect_notify_swapped (
		table, "notify::reply-to",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	composer->priv->notify_mail_followup_to_handler = e_signal_connect_notify_swapped (
		table, "notify::mail-followup-to",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	composer->priv->notify_mail_reply_to_handler = e_signal_connect_notify_swapped (
		table, "notify::mail-reply-to",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	composer->priv->notify_signature_uid_handler = e_signal_connect_notify_swapped (
		table, "notify::signature-uid",
		G_CALLBACK (e_composer_update_signature), composer);
	composer->priv->notify_subject_changed_handler = e_signal_connect_notify_swapped (
		table, "notify::subject",
		G_CALLBACK (msg_composer_subject_changed_cb), composer);
	composer->priv->notify_subject_handler = e_signal_connect_notify_swapped (
		table, "notify::subject",
		G_CALLBACK (msg_composer_notify_header_cb), composer);

	msg_composer_mail_identity_changed_cb (composer);

	/* Attachments */

	store = e_attachment_view_get_store (attachment_view);

	g_signal_connect_swapped (
		store, "row-deleted",
		G_CALLBACK (attachment_store_changed_cb), composer);

	g_signal_connect_swapped (
		store, "row-inserted",
		G_CALLBACK (attachment_store_changed_cb), composer);

	/* Initialization may have tripped the "changed" state. */
	e_content_editor_set_changed (cnt_editor, FALSE);

	target_list = e_attachment_view_get_target_list (attachment_view);
	targets = gtk_target_table_new_from_list (target_list, &n_targets);

	target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (cnt_editor));
	if (target_list) {
		gtk_target_list_add_table (target_list, drag_dest_targets, G_N_ELEMENTS (drag_dest_targets));
		gtk_target_list_add_table (target_list, targets, n_targets);
	} else {
		gtk_drag_dest_set (GTK_WIDGET (cnt_editor), 0, NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

		target_list = gtk_target_list_new (drag_dest_targets, G_N_ELEMENTS (drag_dest_targets));
		gtk_target_list_add_table (target_list, targets, n_targets);
		gtk_drag_dest_set_target_list (GTK_WIDGET (cnt_editor), target_list);
		gtk_target_list_unref (target_list);
	}

	gtk_target_table_free (targets, n_targets);

	e_plugin_ui_register_manager (ui_manager, "org.gnome.evolution.composer", composer);

	e_extensible_load_extensions (E_EXTENSIBLE (composer));

	e_ui_manager_thaw (ui_manager);

	e_msg_composer_set_body_text (composer, "", TRUE);
}

static void
msg_composer_dispose (GObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);
	EShell *shell;

	g_clear_pointer (&composer->priv->address_dialog, gtk_widget_destroy);

	/* FIXME Our EShell is already unreferenced. */
	shell = e_shell_get_default ();

	g_signal_handlers_disconnect_by_func (
		shell, msg_composer_quit_requested_cb, composer);
	g_signal_handlers_disconnect_by_func (
		shell, msg_composer_prepare_for_quit_cb, composer);

	if (composer->priv->header_table != NULL) {
		EComposerHeaderTable *table;

		table = E_COMPOSER_HEADER_TABLE (composer->priv->header_table);

		e_signal_disconnect_notify_handler (table, &composer->priv->notify_destinations_bcc_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_destinations_cc_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_destinations_to_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_identity_uid_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_reply_to_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_mail_followup_to_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_mail_reply_to_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_destinations_to_handler);
		e_signal_disconnect_notify_handler (table, &composer->priv->notify_subject_changed_handler);
	}

	e_composer_private_dispose (composer);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_msg_composer_parent_class)->dispose (object);
}

static void
msg_composer_map (GtkWidget *widget)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	GtkWidget *input_widget;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *text;

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_msg_composer_parent_class)->map (widget);

	composer = E_MSG_COMPOSER (widget);
	editor = e_msg_composer_get_editor (composer);
	table = e_msg_composer_get_header_table (composer);

	/* If the 'To' field is empty, focus it. */
	input_widget =
		e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_TO)->input_widget;
	text = gtk_entry_get_text (GTK_ENTRY (input_widget));
	if (gtk_widget_is_visible (input_widget) && (text == NULL || *text == '\0')) {
		gtk_widget_grab_focus (input_widget);
		return;
	}

	/* If not, check the 'Subject' field. */
	input_widget =
		e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_SUBJECT)->input_widget;
	text = gtk_entry_get_text (GTK_ENTRY (input_widget));
	if (gtk_widget_is_visible (input_widget) && (text == NULL || *text == '\0')) {
		gtk_widget_grab_focus (input_widget);
		return;
	}

	/* Jump to the editor as a last resort. */
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_grab_focus (cnt_editor);
}

static gboolean
msg_composer_key_press_event (GtkWidget *widget,
                              GdkEventKey *event)
{
	EMsgComposer *composer;
	GtkWidget *input_widget;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	composer = E_MSG_COMPOSER (widget);
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	input_widget =
		e_composer_header_table_get_header (
		e_msg_composer_get_header_table (composer),
		E_COMPOSER_HEADER_SUBJECT)->input_widget;

	if (event->keyval == GDK_KEY_Tab && gtk_widget_is_focus (input_widget)) {
		e_content_editor_grab_focus (cnt_editor);
		return TRUE;
	}

	if (e_content_editor_is_focus (cnt_editor)) {
		if (event->keyval == GDK_KEY_ISO_Left_Tab) {
			gboolean view_processed = FALSE;

			g_signal_emit_by_name (cnt_editor, "key-press-event", event, &view_processed);

			if (!view_processed)
				gtk_widget_grab_focus (input_widget);

			return TRUE;
		}
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_msg_composer_parent_class)->
		key_press_event (widget, event);
}

static gboolean
msg_composer_presend (EMsgComposer *composer)
{
	/* This keeps the signal accumulator at TRUE. */
	return TRUE;
}

static gboolean
msg_composer_accumulator_false_abort (GSignalInvocationHint *ihint,
                                      GValue *return_accu,
                                      const GValue *handler_return,
                                      gpointer dummy)
{
	gboolean v_boolean;

	v_boolean = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, v_boolean);

	/* FALSE means abort the signal emission. */
	return v_boolean;
}

/**
 * e_msg_composer_is_busy:
 * @composer: an #EMsgComposer
 *
 * Returns %TRUE only while an #EActivity is in progress.
 *
 * Returns: whether @composer is busy
 **/
gboolean
e_msg_composer_is_busy (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->priv->had_activities;
}

/**
 * e_msg_composer_is_soft_busy:
 * @composer: an #EMsgComposer
 *
 * Returns: %TRUE when e_msg_composer_is_busy() returns %TRUE or
 *    when the asynchronous operations are disabled.
 *
 * Since: 3.30
 **/
gboolean
e_msg_composer_is_soft_busy (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->priv->soft_busy_count > 0 || e_msg_composer_is_busy (composer);
}

static void
e_msg_composer_class_init (EMsgComposerClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = msg_composer_set_property;
	object_class->get_property = msg_composer_get_property;
	object_class->dispose = msg_composer_dispose;
	object_class->finalize = msg_composer_finalize;
	object_class->constructed = msg_composer_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = msg_composer_map;
	widget_class->key_press_event = msg_composer_key_press_event;

	class->presend = msg_composer_presend;

	g_object_class_install_property (
		object_class,
		PROP_BUSY,
		g_param_spec_boolean (
			"busy",
			"Busy",
			"Whether an activity is in progress",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SOFT_BUSY,
		g_param_spec_boolean (
			"soft-busy",
			"Soft Busy",
			"Whether asynchronous actions are disabled",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
			NULL,
			NULL,
			E_TYPE_HTML_EDITOR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_FOCUS_TRACKER,
		g_param_spec_object (
			"focus-tracker",
			NULL,
			NULL,
			E_TYPE_FOCUS_TRACKER,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_IS_REPLY_OR_FORWARD,
		g_param_spec_boolean (
			"is-reply-or-forward",
			"Is Reply Or Forward",
			"Whether the composed message is a reply or a forward message",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			"Shell",
			"The EShell singleton",
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[PRESEND] = g_signal_new (
		"presend",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMsgComposerClass, presend),
		msg_composer_accumulator_false_abort,
		NULL,
		e_marshal_BOOLEAN__VOID,
		G_TYPE_BOOLEAN, 0);

	signals[SEND] = g_signal_new (
		"send",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMsgComposerClass, send),
		NULL, NULL,
		e_marshal_VOID__OBJECT_OBJECT,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_MIME_MESSAGE,
		E_TYPE_ACTIVITY);

	signals[SAVE_TO_DRAFTS] = g_signal_new (
		"save-to-drafts",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMsgComposerClass, save_to_drafts),
		NULL, NULL,
		e_marshal_VOID__OBJECT_OBJECT,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_MIME_MESSAGE,
		E_TYPE_ACTIVITY);

	signals[SAVE_TO_OUTBOX] = g_signal_new (
		"save-to-outbox",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMsgComposerClass, save_to_outbox),
		NULL, NULL,
		e_marshal_VOID__OBJECT_OBJECT,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_MIME_MESSAGE,
		E_TYPE_ACTIVITY);

	signals[PRINT] = g_signal_new (
		"print",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		e_marshal_VOID__ENUM_OBJECT_OBJECT,
		G_TYPE_NONE, 3,
		GTK_TYPE_PRINT_OPERATION_ACTION,
		CAMEL_TYPE_MIME_MESSAGE,
		E_TYPE_ACTIVITY);

	signals[BEFORE_DESTROY] = g_signal_new (
		"before-destroy",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

void
e_composer_emit_before_destroy (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	g_signal_emit (composer, signals[BEFORE_DESTROY], 0);
}

static void
e_msg_composer_init (EMsgComposer *composer)
{
	composer->priv = e_msg_composer_get_instance_private (composer);
}

static void
e_msg_composer_editor_created_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	GtkWidget *editor;
	ESimpleAsyncResult *eresult = user_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (eresult));

	editor = e_html_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create HTML editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		e_simple_async_result_set_op_pointer (eresult, editor, NULL);
		e_simple_async_result_complete (eresult);
	}

	g_object_unref (eresult);
}

/**
 * e_msg_composer_new:
 * @shell: an #EShell
 * @callback: called when the composer is ready
 * @user_data: user data passed to @callback
 *
 * Asynchronously creates an #EMsgComposer. The operation is finished
 * with e_msg_composer_new_finish() called from within the @callback.
 *
 * Since: 3.22
 **/
void
e_msg_composer_new (EShell *shell,
		    GAsyncReadyCallback callback,
		    gpointer user_data)
{
	ESimpleAsyncResult *eresult;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (callback != NULL);

	eresult = e_simple_async_result_new (NULL, callback, user_data, e_msg_composer_new);
	e_simple_async_result_set_user_data (eresult, g_object_ref (shell), g_object_unref);

	e_html_editor_new (e_msg_composer_editor_created_cb, eresult);
}

/**
 * e_msg_composer_new_finish:
 * @result: a #GAsyncResult provided by the callback from e_msg_composer_new()
 * @error: optional #GError for errors
 *
 * Finishes call of e_msg_composer_new().
 *
 * Since: 3.22
 **/
EMsgComposer *
e_msg_composer_new_finish (GAsyncResult *result,
			   GError **error)
{
	ESimpleAsyncResult *eresult;
	EHTMLEditor *html_editor;

	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_msg_composer_new), NULL);

	eresult = E_SIMPLE_ASYNC_RESULT (result);

	html_editor = e_simple_async_result_get_op_pointer (eresult);
	g_return_val_if_fail (E_IS_HTML_EDITOR (html_editor), NULL);

	return g_object_new (E_TYPE_MSG_COMPOSER,
		"shell", e_simple_async_result_get_user_data (eresult),
		"editor", html_editor,
		NULL);
}

/**
 * e_msg_composer_get_editor:
 * @composer: an #EMsgComposer
 *
 * Returns @composer's internal #EHTMLEditor instance.
 *
 * Returns: an #EHTMLEditor
 **/
EHTMLEditor *
e_msg_composer_get_editor (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return composer->priv->editor;
}

EFocusTracker *
e_msg_composer_get_focus_tracker (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return composer->priv->focus_tracker;
}

static void
e_msg_composer_set_pending_body (EMsgComposer *composer,
                                 gchar *text,
                                 gssize length,
                                 gboolean is_html)
{
	g_object_set_data_full (
		G_OBJECT (composer), "body:text_mime_type",
		GINT_TO_POINTER (is_html), NULL);
	g_object_set_data_full (
		G_OBJECT (composer), "body:text",
		text, (GDestroyNotify) g_free);
}

static gboolean
e_msg_composer_has_pending_body (EMsgComposer *composer)
{
	return g_object_get_data (G_OBJECT (composer), "body:text") != NULL;
}

static void
e_msg_composer_flush_pending_body (EMsgComposer *composer)
{
	const gchar *body;
	gboolean is_html;

	body = g_object_get_data (G_OBJECT (composer), "body:text");
	is_html = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (composer), "body:text_mime_type"));

	if (body != NULL) {
		const gchar *signature_uid;

		signature_uid = e_composer_header_table_get_signature_uid (e_msg_composer_get_header_table (composer));

		set_editor_text (composer, body, is_html, g_strcmp0 (signature_uid, "none") != 0);
	}

	g_object_set_data (G_OBJECT (composer), "body:text", NULL);
}

static gboolean
emc_is_attachment_part (CamelMimePart *mime_part,
			CamelMimePart *parent_part)
{
	const CamelContentDisposition *cd;
	CamelContentType *ct, *parent_ct = NULL;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), FALSE);

	ct = camel_mime_part_get_content_type (mime_part);
	cd = camel_mime_part_get_content_disposition (mime_part);

	if (parent_part)
		parent_ct = camel_mime_part_get_content_type (parent_part);

	if (!camel_content_disposition_is_attachment_ex (cd, ct, parent_ct))
		return FALSE;

	/* It looks like an attachment now. Make it an attachment for all but images
	   under multipart/related, to avoid this group of false positives. */
	return !(parent_ct && ct &&
		camel_content_type_is (parent_ct, "multipart", "related") &&
		camel_content_type_is (ct, "image", "*"));
}

static void
add_attachments_handle_mime_part (EMsgComposer *composer,
                                  CamelMimePart *mime_part,
                                  gboolean just_inlines,
                                  gboolean related,
                                  gint depth)
{
	CamelContentType *content_type;
	CamelDataWrapper *wrapper;
	EHTMLEditor *editor;

	if (!mime_part)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);
	wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	editor = e_msg_composer_get_editor (composer);

	if (CAMEL_IS_MULTIPART (wrapper)) {
		/* another layer of multipartness... */
		add_attachments_from_multipart (
			composer, (CamelMultipart *) wrapper,
			just_inlines, depth + 1);
	} else if (just_inlines) {
		if (camel_content_type_is (content_type, "image", "*") && (
		    camel_mime_part_get_content_id (mime_part) ||
		    camel_mime_part_get_content_location (mime_part)))
			e_html_editor_add_cid_part (editor, mime_part);
	} else if (related && camel_content_type_is (content_type, "image", "*")) {
		e_html_editor_add_cid_part (editor, mime_part);
	} else if (camel_content_type_is (content_type, "text", "*") &&
		camel_mime_part_get_filename (mime_part) == NULL) {
		/* Do nothing if this is a text/anything without a
		 * filename, otherwise attach it too. */
	} else {
		e_msg_composer_attach (composer, mime_part);
	}
}

static void
add_attachments_from_multipart (EMsgComposer *composer,
                                CamelMultipart *multipart,
                                gboolean just_inlines,
                                gint depth)
{
	/* find appropriate message attachments to add to the composer */
	CamelMimePart *mime_part;
	gboolean related;
	gint i, nparts;

	related = camel_content_type_is (
		camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (multipart)),
		"multipart", "related");

	if (CAMEL_IS_MULTIPART_SIGNED (multipart)) {
		mime_part = camel_multipart_get_part (
			multipart, CAMEL_MULTIPART_SIGNED_CONTENT);
		add_attachments_handle_mime_part (
			composer, mime_part, just_inlines, related, depth);
	} else if (CAMEL_IS_MULTIPART_ENCRYPTED (multipart)) {
		/* XXX What should we do in this case? */
	} else {
		nparts = camel_multipart_get_number (multipart);

		for (i = 0; i < nparts; i++) {
			mime_part = camel_multipart_get_part (multipart, i);
			add_attachments_handle_mime_part (
				composer, mime_part, just_inlines,
				related, depth);
		}
	}
}

/**
 * e_msg_composer_add_message_attachments:
 * @composer: the composer to add the attachments to.
 * @message: the source message to copy the attachments from.
 * @just_inlines: whether to attach all attachments or just add
 * inline images.
 *
 * Walk through all the mime parts in @message and add them to the composer
 * specified in @composer.
 */
void
e_msg_composer_add_message_attachments (EMsgComposer *composer,
                                        CamelMimeMessage *message,
                                        gboolean just_inlines)
{
	CamelDataWrapper *wrapper;

	wrapper = camel_medium_get_content (CAMEL_MEDIUM (message));
	if (!CAMEL_IS_MULTIPART (wrapper))
		return;

	add_attachments_from_multipart (
		composer, (CamelMultipart *) wrapper, just_inlines, 0);
}

/**
 * e_msg_composer_add_attachments_from_part_list:
 * @composer: the composer to add the attachments to
 * @part_list: an #EMailPartList with parts used to format the message
 * @just_inlines: whether to attach all attachments or just add inline images
 *
 * Walk through all the parts in @part_list and add them to the @composer.
 *
 * Since: 3.42
 */
void
e_msg_composer_add_attachments_from_part_list (EMsgComposer *composer,
					       EMailPartList *part_list,
					       gboolean just_inlines)
{
	EHTMLEditor *editor;
	GHashTable *added_mime_parts;
	GQueue queue = G_QUEUE_INIT;
	GList *link;
	guint in_message_attachment = 0;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (!part_list)
		return;

	/* One mime part can be in the part list multiple times */
	added_mime_parts = g_hash_table_new (g_direct_hash, g_direct_equal);
	editor = e_msg_composer_get_editor (composer);

	e_mail_part_list_queue_parts (part_list, NULL, &queue);

	for (link = g_queue_peek_head_link (&queue); link; link = g_list_next (link)) {
		EMailPart *part = link->data;
		CamelMimePart *mime_part;
		CamelContentType *content_type;

		if (e_mail_part_id_has_suffix (part, ".rfc822")) {
			in_message_attachment++;
			continue;
		}

		if (e_mail_part_id_has_suffix (part, ".rfc822.end")) {
			if (in_message_attachment)
				in_message_attachment--;
			continue;
		}

		/* Do not attach attachments from attached messages */
		if (in_message_attachment)
			continue;

		if (!e_mail_part_get_is_attachment (part))
			continue;

		mime_part = e_mail_part_ref_mime_part (part);
		if (!mime_part)
			continue;

		if (g_hash_table_contains (added_mime_parts, mime_part)) {
			g_object_unref (mime_part);
			continue;
		}

		content_type = camel_mime_part_get_content_type (mime_part);
		if (!content_type) {
			g_object_unref (mime_part);
			continue;
		}

		if (!just_inlines &&
		    camel_content_type_is (content_type, "text", "*") &&
		    camel_mime_part_get_filename (mime_part) == NULL) {
			/* Do nothing if this is a text/anything without a
			 * filename, otherwise attach it too. */
		} else if (camel_content_type_is (content_type, "image", "*") && (
			   camel_mime_part_get_content_id (mime_part) ||
			   camel_mime_part_get_content_location (mime_part))) {
				e_html_editor_add_cid_part (editor, mime_part);
				g_hash_table_add (added_mime_parts, mime_part);
		} else if (!just_inlines) {
			e_msg_composer_attach (composer, mime_part);
			g_hash_table_add (added_mime_parts, mime_part);
		}

		g_object_unref (mime_part);
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	g_hash_table_destroy (added_mime_parts);
}

static void
e_msg_composer_filter_inline_attachments (EMsgComposer *composer,
					  GSList *used_mime_parts) /* CamelMimePart * */
{
	GSList *removed_parts = NULL, *link;
	gboolean been_changed;
	EHTMLEditor *editor;
	EContentEditor *content_editor;

	editor = e_msg_composer_get_editor (composer);
	content_editor = e_html_editor_get_content_editor (editor);

	been_changed = e_content_editor_get_changed (content_editor);
	e_html_editor_remove_unused_cid_parts (editor, used_mime_parts, &removed_parts);

	for (link = removed_parts; link; link = g_slist_next (link)) {
		CamelMimePart *mime_part = link->data;
		e_msg_composer_attach (composer, mime_part);
	}

	g_slist_free_full (removed_parts, g_object_unref);

	/* This is not a user change */
	e_content_editor_set_changed (content_editor, been_changed);
}

static void
e_mg_composer_got_used_inline_images_cb (GObject *source_object,
					 GAsyncResult *result,
					 gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EContentEditorContentHash *content_hash;

	content_hash = e_content_editor_get_content_finish (E_CONTENT_EDITOR (source_object), result, NULL);
	if (content_hash) {
		GSList *inline_images_parts;

		inline_images_parts = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_INLINE_IMAGES);

		e_msg_composer_filter_inline_attachments (composer, inline_images_parts);

		g_hash_table_destroy (content_hash);
	} else {
		e_msg_composer_filter_inline_attachments (composer, NULL);
	}

	g_object_unref (composer);
}

/**
 * e_msg_composer_check_inline_attachments:
 * @composer: an #EMsgComposer
 *
 * Checks which inline attachments are referenced in the message body
 * and those which are not referenced are added as regular attachments.
 *
 * Since: 3.44
 **/
void
e_msg_composer_check_inline_attachments (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *content_editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	content_editor = e_html_editor_get_content_editor (editor);

	if (e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML) {
		e_content_editor_get_content (content_editor, E_CONTENT_EDITOR_GET_INLINE_IMAGES,
			"localhost", NULL, e_mg_composer_got_used_inline_images_cb, g_object_ref (composer));
	} else {
		e_msg_composer_filter_inline_attachments (composer, NULL);
	}
}

static gboolean
emcu_format_as_plain_text (EMsgComposer *composer,
			   CamelContentType *content_type)
{
	EContentEditorMode mode;

	if (!camel_content_type_is (content_type, "text", "plain") &&
	    !camel_content_type_is (content_type, "text", "markdown"))
		return FALSE;

	mode = e_html_editor_get_mode (e_msg_composer_get_editor (composer));

	return mode == E_CONTENT_EDITOR_MODE_MARKDOWN ||
		mode == E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT ||
		mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;
}

static void
handle_multipart_signed (EMsgComposer *composer,
                         CamelMultipart *multipart,
			 CamelMimePart *parent_part,
                         gboolean keep_signature,
                         GCancellable *cancellable,
                         gint depth)
{
	CamelContentType *content_type;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	EUIAction *action = NULL;
	const gchar *protocol;

	content = CAMEL_DATA_WRAPPER (multipart);
	content_type = camel_data_wrapper_get_mime_type_field (content);
	protocol = camel_content_type_param (content_type, "protocol");

	if (protocol == NULL) {
		action = NULL;
	} else if (g_ascii_strcasecmp (protocol, "application/pgp-signature") == 0) {
		if (!e_ui_action_get_active (ACTION (SMIME_SIGN)) &&
		    !e_ui_action_get_active (ACTION (SMIME_ENCRYPT)))
			action = ACTION (PGP_SIGN);
	} else if (g_ascii_strcasecmp (protocol, "application/pkcs7-signature") == 0 ||
		   g_ascii_strcasecmp (protocol, "application/xpkcs7signature") == 0 ||
		   g_ascii_strcasecmp (protocol, "application/xpkcs7-signature") == 0 ||
		   g_ascii_strcasecmp (protocol, "application/x-pkcs7-signature") == 0) {
		if (!e_ui_action_get_active (ACTION (PGP_SIGN)) &&
		    !e_ui_action_get_active (ACTION (PGP_ENCRYPT)))
			action = ACTION (SMIME_SIGN);
	}

	if (action)
		e_ui_action_set_active (action, TRUE);

	mime_part = camel_multipart_get_part (
		multipart, CAMEL_MULTIPART_SIGNED_CONTENT);

	if (mime_part == NULL)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);
	content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (content)) {
		multipart = CAMEL_MULTIPART (content);

		/* Note: depth is preserved here because we're not
		 * counting multipart/signed as a multipart, instead
		 * we want to treat the content part as our mime part
		 * here. */

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* Handle the signed content and configure
			 * the composer to sign outgoing messages. */
			handle_multipart_signed (
				composer, multipart, parent_part, keep_signature, cancellable, depth);

		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* Decrypt the encrypted content and configure
			 * the composer to encrypt outgoing messages. */
			handle_multipart_encrypted (
				composer, mime_part, parent_part, keep_signature, cancellable, depth);

		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* This contains the text/plain and text/html
			 * versions of the message body. */
			handle_multipart_alternative (
				composer, multipart, parent_part, keep_signature, cancellable, depth);

		} else {
			/* There must be attachments... */
			handle_multipart (composer, multipart, parent_part, keep_signature, TRUE, cancellable, depth);
		}
	} else if (camel_content_type_is (content_type, "text", "markdown") ||
		   emcu_format_as_plain_text (composer, content_type)) {
		gchar *text;
		gssize length;

		text = emcu_part_as_text (composer, mime_part, &length, cancellable);
		if (text)
			e_msg_composer_set_pending_body (composer, text, length, FALSE);
	} else if (camel_content_type_is (content_type, "text", "*")) {
		gchar *html;
		gssize length;

		html = emcu_part_to_html (
			composer, mime_part, &length, keep_signature, cancellable);
		if (html)
			e_msg_composer_set_pending_body (composer, html, length, TRUE);

	} else {
		e_msg_composer_attach (composer, mime_part);
	}
}

static void
handle_multipart_encrypted (EMsgComposer *composer,
                            CamelMimePart *multipart,
			    CamelMimePart *parent_part,
                            gboolean keep_signature,
                            GCancellable *cancellable,
                            gint depth)
{
	CamelContentType *content_type;
	CamelCipherContext *cipher;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	CamelSession *session;
	CamelCipherValidity *valid;
	EUIAction *action = NULL;
	const gchar *protocol;

	content_type = camel_mime_part_get_content_type (multipart);
	protocol = camel_content_type_param (content_type, "protocol");

	if (protocol && g_ascii_strcasecmp (protocol, "application/pgp-encrypted") == 0) {
		if (!e_ui_action_get_active (ACTION (SMIME_SIGN)) &&
		    !e_ui_action_get_active (ACTION (SMIME_ENCRYPT)))
			action = ACTION (PGP_ENCRYPT);
	} else if (content_type && (
		   camel_content_type_is (content_type, "application", "pkcs7-mime") ||
		   camel_content_type_is (content_type, "application", "xpkcs7mime") ||
		   camel_content_type_is (content_type, "application", "xpkcs7-mime") ||
		   camel_content_type_is (content_type, "application", "x-pkcs7-mime"))) {
		if (!e_ui_action_get_active (ACTION (PGP_SIGN)) &&
		    !e_ui_action_get_active (ACTION (PGP_ENCRYPT)))
			action = ACTION (SMIME_ENCRYPT);
	}

	if (action)
		e_ui_action_set_active (action, TRUE);

	session = e_msg_composer_ref_session (composer);
	cipher = camel_gpg_context_new (session);
	mime_part = camel_mime_part_new ();
	valid = camel_cipher_context_decrypt_sync (
		cipher, multipart, mime_part, cancellable, NULL);
	g_object_unref (cipher);
	g_object_unref (session);

	if (valid == NULL) {
		g_object_unref (mime_part);
		return;
	}

	camel_cipher_validity_free (valid);

	content_type = camel_mime_part_get_content_type (mime_part);

	content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (content)) {
		CamelMultipart *content_multipart = CAMEL_MULTIPART (content);

		/* Note: depth is preserved here because we're not
		 * counting multipart/encrypted as a multipart, instead
		 * we want to treat the content part as our mime part
		 * here. */

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* Handle the signed content and configure the
			 * composer to sign outgoing messages. */
			handle_multipart_signed (
				composer, content_multipart, multipart, keep_signature, cancellable, depth);

		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* Decrypt the encrypted content and configure the
			 * composer to encrypt outgoing messages. */
			handle_multipart_encrypted (
				composer, mime_part, multipart, keep_signature, cancellable, depth);

		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* This contains the text/plain and text/html
			 * versions of the message body. */
			handle_multipart_alternative (
				composer, content_multipart, multipart, keep_signature, cancellable, depth);

		} else {
			/* There must be attachments... */
			handle_multipart (
				composer, content_multipart, multipart, keep_signature, TRUE, cancellable, depth);
		}

	} else if (camel_content_type_is (content_type, "text", "markdown") ||
		   emcu_format_as_plain_text (composer, content_type)) {
		gchar *text;
		gssize length;

		text = emcu_part_as_text (composer, mime_part, &length, cancellable);
		if (text)
			e_msg_composer_set_pending_body (composer, text, length, FALSE);
	} else if (camel_content_type_is (content_type, "text", "*")) {
		gchar *html;
		gssize length;

		html = emcu_part_to_html (
			composer, mime_part, &length, keep_signature, cancellable);
		if (html)
			e_msg_composer_set_pending_body (composer, html, length, TRUE);

	} else {
		e_msg_composer_attach (composer, mime_part);
	}

	g_object_unref (mime_part);
}

static void
handle_multipart_alternative (EMsgComposer *composer,
                              CamelMultipart *multipart,
			      CamelMimePart *parent_part,
                              gboolean keep_signature,
                              GCancellable *cancellable,
                              gint depth)
{
	/* Find the text/html part and set the composer body to its content */
	CamelMimePart *text_part = NULL, *fallback_text_part = NULL;
	gint i, nparts;

	nparts = camel_multipart_get_number (multipart);

	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelDataWrapper *content;
		CamelMimePart *mime_part;

		mime_part = camel_multipart_get_part (multipart, i);

		if (!mime_part)
			continue;

		content_type = camel_mime_part_get_content_type (mime_part);
		content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

		if (CAMEL_IS_MULTIPART (content)) {
			CamelMultipart *mp;

			mp = CAMEL_MULTIPART (content);

			if (CAMEL_IS_MULTIPART_SIGNED (content)) {
				/* Handle the signed content and configure
				 * the composer to sign outgoing messages. */
				handle_multipart_signed (
					composer, mp, parent_part, keep_signature, cancellable, depth + 1);

			} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
				/* Decrypt the encrypted content and configure
				 * the composer to encrypt outgoing messages. */
				handle_multipart_encrypted (
					composer, mime_part, parent_part, keep_signature,
					cancellable, depth + 1);

			} else {
				/* Depth doesn't matter so long as we
				 * don't pass 0. */
				handle_multipart (composer, mp, parent_part, keep_signature, FALSE, cancellable, depth + 1);
			}

		} else if (camel_content_type_is (content_type, "text", "html")) {
			/* text/html is preferable, so once we find it we're done... */
			text_part = mime_part;
			i++;
			break;
		} else if (camel_content_type_is (content_type, "text", "markdown") ||
			   emcu_format_as_plain_text (composer, content_type)) {
			gchar *text;
			gssize length;

			text = emcu_part_as_text (composer, mime_part, &length, cancellable);
			if (text) {
				e_msg_composer_set_pending_body (composer, text, length, FALSE);
				text_part = NULL;
				i++;
				break;
			}
		} else if (camel_content_type_is (content_type, "text", "*")) {
			/* any text part not text/html is second rate so the first
			 * text part we find isn't necessarily the one we'll use. */
			if (!text_part)
				text_part = mime_part;

			/* this is when prefer-plain filters out text/html part, then
			 * the text/plain should be used */
			if (camel_content_type_is (content_type, "text", "plain"))
				fallback_text_part = mime_part;
			else if (!composer->priv->alternative_body_attachment && depth == 0)
				e_msg_composer_set_alternative_body (composer, mime_part);
		} else {
			if (!composer->priv->alternative_body_attachment && depth == 0)
				e_msg_composer_set_alternative_body (composer, mime_part);
			else
				e_msg_composer_attach (composer, mime_part);
		}
	}

	if (!composer->priv->alternative_body_attachment && depth == 0 && i < nparts) {
		while (i < nparts) {
			CamelMimePart *mime_part;

			mime_part = camel_multipart_get_part (multipart, i);

			if (mime_part && !CAMEL_IS_MULTIPART (camel_medium_get_content (CAMEL_MEDIUM (mime_part)))) {
				CamelContentType *content_type;

				content_type = camel_mime_part_get_content_type (mime_part);

				if (!camel_content_type_is (content_type, "text", "plain") &&
				    !camel_content_type_is (content_type, "text", "html") &&
				    !camel_content_type_is (content_type, "text", "markdown")) {
					e_msg_composer_set_alternative_body (composer, mime_part);
					break;
				}
			}

			i++;
		}
	}

	if (text_part) {
		gchar *html;
		gssize length;

		if (emcu_format_as_plain_text (composer, camel_mime_part_get_content_type (text_part))) {
			gchar *text;

			text = emcu_part_as_text (composer, text_part, &length, cancellable);
			if (text) {
				e_msg_composer_set_pending_body (composer, text, length, FALSE);
				return;
			}
		}

		html = emcu_part_to_html (composer, text_part, &length, keep_signature, cancellable);
		if (!html && fallback_text_part) {
			if (emcu_format_as_plain_text (composer, camel_mime_part_get_content_type (fallback_text_part))) {
				gchar *text;

				text = emcu_part_as_text (composer, fallback_text_part, &length, cancellable);
				if (text) {
					e_msg_composer_set_pending_body (composer, text, length, FALSE);
					return;
				}
			}

			html = emcu_part_to_html (composer, fallback_text_part, &length, keep_signature, cancellable);
		}

		if (html)
			e_msg_composer_set_pending_body (composer, html, length, TRUE);
	}
}

static gboolean
mime_part_is_evolution_note (CamelMimePart *mime_part)
{
	CamelContentType *content_type;

	if (!mime_part)
		return FALSE;

	content_type = camel_mime_part_get_content_type (mime_part);

	return camel_content_type_is (content_type, "message", "rfc822") &&
	       camel_medium_get_header (CAMEL_MEDIUM (mime_part), "X-Evolution-Note") &&
	       g_ascii_strcasecmp (camel_medium_get_header (CAMEL_MEDIUM (mime_part), "X-Evolution-Note"), "True") == 0;
}

static void
handle_multipart (EMsgComposer *composer,
                  CamelMultipart *multipart,
		  CamelMimePart *parent_part,
                  gboolean keep_signature,
		  gboolean is_signed_or_encrypted,
                  GCancellable *cancellable,
                  gint depth)
{
	gint i, nparts;

	nparts = camel_multipart_get_number (multipart);

	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelDataWrapper *content;
		CamelMimePart *mime_part;

		mime_part = camel_multipart_get_part (multipart, i);

		if (!mime_part)
			continue;

		content_type = camel_mime_part_get_content_type (mime_part);
		content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

		if (CAMEL_IS_MULTIPART (content)) {
			CamelMultipart *mp;

			mp = CAMEL_MULTIPART (content);

			if (CAMEL_IS_MULTIPART_SIGNED (content)) {
				/* Handle the signed content and configure
				 * the composer to sign outgoing messages. */
				handle_multipart_signed (
					composer, mp, parent_part, keep_signature, cancellable, depth + 1);

			} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
				/* Decrypt the encrypted content and configure
				 * the composer to encrypt outgoing messages. */
				handle_multipart_encrypted (
					composer, mime_part, parent_part, keep_signature,
					cancellable, depth + 1);

			} else if (camel_content_type_is (
				content_type, "multipart", "alternative")) {
				handle_multipart_alternative (
					composer, mp, parent_part, keep_signature, cancellable, depth + 1);

			} else {
				gint depth_inc = 1;

				/* this can be a multipart/mixed with user's Note on the message, thus treat
				   the inner part as the top part, not as one level lower in such case */
				if (depth == 0 && i == 0 && nparts == 2) {
					CamelMimePart *second_part;

					second_part = camel_multipart_get_part (multipart, 1);

					if (mime_part_is_evolution_note (second_part))
						depth_inc = 0;
				}

				handle_multipart (composer, mp, parent_part, keep_signature, FALSE, cancellable, depth + depth_inc);
			}

		} else if (depth == 0 && (i == 0 || (is_signed_or_encrypted && i == 1 && !e_msg_composer_has_pending_body (composer)))) {
			/* Since the first part is not multipart/alternative,
			 * this must be the body. */

			if (is_signed_or_encrypted && camel_content_type_is (content_type, "text", "rfc822-headers")) {
				/* ignore this part, it's not a body */
			} else if (camel_content_type_is (content_type, "text", "markdown") ||
			    emcu_format_as_plain_text (composer, content_type)) {
				gchar *text;
				gssize length;

				text = emcu_part_as_text (composer, mime_part, &length, cancellable);
				if (text)
					e_msg_composer_set_pending_body (composer, text, length, FALSE);
			} else {
				gchar *html = NULL;
				gssize length = 0;

				html = emcu_part_to_html (
					composer, mime_part, &length, keep_signature, cancellable);

				e_msg_composer_set_pending_body (composer, html, length, TRUE);
			}

		} else if (camel_content_type_is (content_type, "image", "*") && (
			   camel_mime_part_get_content_id (mime_part) ||
			   camel_mime_part_get_content_location (mime_part))) {
			/* special in-line attachment */
			EHTMLEditor *editor;

			editor = e_msg_composer_get_editor (composer);

			e_html_editor_add_cid_part (editor, mime_part);

			/* Add it to both, to not lose attachments not referenced in HTML body.
			   The inserted images are not included in the message when not referenced. */
			if (emc_is_attachment_part (mime_part, parent_part))
				e_msg_composer_attach (composer, mime_part);
		} else {
			if (mime_part_is_evolution_note (mime_part)) {
				/* do not attach user notes on the message created by Evolution */
			} else {
				/* normal attachment */
				e_msg_composer_attach (composer, mime_part);
			}
		}
	}
}

static void
set_signature_gui (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EComposerHeaderTable *table;
	EMailSignatureComboBox *combo_box;
	gchar *uid;

	table = e_msg_composer_get_header_table (composer);
	combo_box = e_composer_header_table_get_signature_combo_box (table);

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	uid = e_content_editor_get_current_signature_uid (cnt_editor);
	if (uid) {
		/* The combo box active ID is the signature's ESource UID. */
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), uid);
		g_free (uid);
	}
}

static void
composer_add_auto_recipients (ESource *source,
                              const gchar *property_name,
                              GHashTable *hash_table,
			      GList **inout_destinations)
{
	ESourceMailComposition *extension;
	CamelInternetAddress *inet_addr;
	const gchar *extension_name;
	gchar *comma_separated_addrs;
	gchar **addr_array = NULL;
	gint length, ii;
	gint retval;

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	extension = e_source_get_extension (source, extension_name);

	g_object_get (extension, property_name, &addr_array, NULL);

	if (addr_array == NULL)
		return;

	inet_addr = camel_internet_address_new ();
	comma_separated_addrs = g_strjoinv (", ", addr_array);

	retval = camel_address_decode (
		CAMEL_ADDRESS (inet_addr), comma_separated_addrs);

	g_free (comma_separated_addrs);
	g_strfreev (addr_array);

	if (retval == -1)
		return;

	length = camel_address_length (CAMEL_ADDRESS (inet_addr));

	for (ii = 0; ii < length; ii++) {
		const gchar *name;
		const gchar *addr;

		if (camel_internet_address_get (inet_addr, ii, &name, &addr)) {
			EDestination *dest;

			g_hash_table_add (hash_table, g_strdup (addr));

			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);
			e_destination_set_auto_recipient (dest, TRUE);

			*inout_destinations = g_list_append (*inout_destinations, dest);
		}
	}

	g_object_unref (inet_addr);
}

/**
 * e_msg_composer_setup_with_message:
 * @composer: an #EMsgComposer
 * @message: The message to use as the source
 * @keep_signature: Keep message signature, if any
 * @override_identity_uid: (allow none): Optional identity UID to use, or %NULL
 * @override_alias_name: (nullable): an alias name to use together with the override_identity_uid, or %NULL
 * @override_alias_address: (nullable): an alias address to use together with the override_identity_uid, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 *
 * Sets up the message @composer with a specific @message.
 *
 * Note: Designed to work only for messages constructed using Evolution.
 *
 * Since: 3.22
 **/
void
e_msg_composer_setup_with_message (EMsgComposer *composer,
				   CamelMimeMessage *message,
				   gboolean keep_signature,
				   const gchar *override_identity_uid,
				   const gchar *override_alias_name,
				   const gchar *override_alias_address,
				   GCancellable *cancellable)
{
	CamelInternetAddress *from, *to, *cc, *bcc;
	GList *To = NULL, *Cc = NULL, *Bcc = NULL, *postto = NULL;
	const gchar *format, *subject, *composer_mode;
	EDestination **Tov, **Ccv, **Bccv;
	GHashTable *auto_cc, *auto_bcc;
	CamelMimePart *mime_part;
	CamelContentType *content_type;
	const CamelNameValueArray *headers;
	CamelDataWrapper *content;
	EComposerHeaderTable *table;
	ESource *source = NULL;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *identity_uid;
	gint len, i;
	guint jj, jjlen;
	gboolean is_message_from_draft = FALSE;
	gboolean is_editor_ready;
	#ifdef ENABLE_SMIME
	CamelMimePart *decrypted_part = NULL;
	#endif

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	headers = camel_medium_get_headers (CAMEL_MEDIUM (message));
	jjlen = camel_name_value_array_get_length (headers);
	for (jj = 0; jj < jjlen; jj++) {
		const gchar *header_name = NULL, *header_value = NULL;
		gchar *value;

		if (!camel_name_value_array_get (headers, jj, &header_name, &header_value) ||
		    !header_name)
			continue;

		if (g_ascii_strcasecmp (header_name, "X-Evolution-PostTo") == 0) {
			value = g_strstrip (g_strdup (header_value));
			postto = g_list_append (postto, value);
		}
	}

	table = e_msg_composer_get_header_table (composer);
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* Editing message as new, then keep the signature always below the original message */
	if (keep_signature && !e_msg_composer_get_is_reply_or_forward (composer))
		e_content_editor_set_top_signature (cnt_editor, E_THREE_STATE_OFF);

	if (postto) {
		e_composer_header_table_set_post_to_list (table, postto);
		g_list_foreach (postto, (GFunc) g_free, NULL);
		g_list_free (postto);
		postto = NULL;
	}

	if (override_identity_uid && *override_identity_uid) {
		identity_uid = (gchar *) override_identity_uid;
	} else {
		/* Restore the mail identity preference. */
		identity_uid = (gchar *) camel_medium_get_header (
			CAMEL_MEDIUM (message), "X-Evolution-Identity");
		if (!identity_uid) {
			/* for backward compatibility */
			identity_uid = (gchar *) camel_medium_get_header (
				CAMEL_MEDIUM (message), "X-Evolution-Account");
		}
		if (!identity_uid) {
			source = em_utils_guess_mail_identity_with_recipients (
				e_shell_get_registry (e_msg_composer_get_shell (composer)), message, NULL, NULL, NULL, NULL);
			if (source)
				identity_uid = e_source_dup_uid (source);
		}
	}

	if (identity_uid != NULL && !source) {
		identity_uid = g_strstrip (g_strdup (identity_uid));
		source = e_composer_header_table_ref_source (
			table, identity_uid);
	}

	auto_cc = g_hash_table_new_full (
		(GHashFunc) camel_strcase_hash,
		(GEqualFunc) camel_strcase_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	auto_bcc = g_hash_table_new_full (
		(GHashFunc) camel_strcase_hash,
		(GEqualFunc) camel_strcase_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	if (source != NULL) {
		composer_add_auto_recipients (source, "cc", auto_cc, &Cc);
		composer_add_auto_recipients (source, "bcc", auto_bcc, &Bcc);
	}

	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);

	len = camel_address_length (CAMEL_ADDRESS (to));
	for (i = 0; i < len; i++) {
		const gchar *name, *addr;

		if (camel_internet_address_get (to, i, &name, &addr)) {
			EDestination *dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);
			To = g_list_append (To, dest);
		}
	}

	Tov = destination_list_to_vector (To);
	g_list_free (To);

	len = camel_address_length (CAMEL_ADDRESS (cc));
	for (i = 0; i < len; i++) {
		const gchar *name, *addr;

		if (camel_internet_address_get (cc, i, &name, &addr)) {
			EDestination *dest;

			if (g_hash_table_contains (auto_cc, addr))
				continue;

			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);

			Cc = g_list_append (Cc, dest);
		}
	}

	Ccv = destination_list_to_vector (Cc);
	g_hash_table_destroy (auto_cc);
	g_list_free (Cc);

	len = camel_address_length (CAMEL_ADDRESS (bcc));
	for (i = 0; i < len; i++) {
		const gchar *name, *addr;

		if (camel_internet_address_get (bcc, i, &name, &addr)) {
			EDestination *dest;

			if (g_hash_table_contains (auto_bcc, addr))
				continue;

			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);

			Bcc = g_list_append (Bcc, dest);
		}
	}

	Bccv = destination_list_to_vector (Bcc);
	g_hash_table_destroy (auto_bcc);
	g_list_free (Bcc);

	if (source != NULL)
		g_object_unref (source);

	subject = camel_mime_message_get_subject (message);

	e_composer_header_table_set_destinations_to (table, Tov);
	e_composer_header_table_set_destinations_cc (table, Ccv);
	e_composer_header_table_set_destinations_bcc (table, Bccv);
	e_composer_header_table_set_subject (table, subject);

	e_destination_freev (Tov);
	e_destination_freev (Ccv);
	e_destination_freev (Bccv);

	from = camel_mime_message_get_from (message);
	if ((!override_identity_uid || !*override_identity_uid) && from) {
		const gchar *name = NULL, *address = NULL;

		if (camel_address_length (CAMEL_ADDRESS (from)) == 1 &&
		    camel_internet_address_get (from, 0, &name, &address)) {
			EComposerFromHeader *header_from;
			const gchar *filled_name, *filled_address;

			/* First try whether such alias exists... */
			e_composer_header_table_set_identity_uid (table, identity_uid, name, address);

			header_from = E_COMPOSER_FROM_HEADER (e_composer_header_table_get_header (table, E_COMPOSER_HEADER_FROM));

			filled_name = e_composer_from_header_get_name (header_from);
			filled_address = e_composer_from_header_get_address (header_from);

			if (name && !*name)
				name = NULL;

			if (address && !*address)
				address = NULL;

			if (g_strcmp0 (filled_name, name) != 0 ||
			    g_strcmp0 (filled_address, address) != 0) {
				/* ... and if not, then reset to the main identity address */
				e_composer_header_table_set_identity_uid (table, identity_uid, NULL, NULL);
				e_composer_from_header_set_name (header_from, name);
				e_composer_from_header_set_address (header_from, address);
				e_composer_from_header_set_override_visible (header_from, TRUE);
			}
		} else {
			e_composer_header_table_set_identity_uid (table, identity_uid, NULL, NULL);
		}
	} else {
		e_composer_header_table_set_identity_uid (table, identity_uid, override_alias_name, override_alias_address);
	}

	g_free (identity_uid);

	/* Restore the format editing preference */
	format = camel_medium_get_header (
		CAMEL_MEDIUM (message), "X-Evolution-Format");

	composer_mode = camel_medium_get_header (
		CAMEL_MEDIUM (message), "X-Evolution-Composer-Mode");

	if (format && *format && composer_mode && *composer_mode)
		is_message_from_draft = TRUE;

	if (format != NULL) {
		gchar **flags;

		while (*format && camel_mime_is_lwsp (*format))
			format++;

		flags = g_strsplit (format, ", ", 0);
		for (i = 0; flags[i]; i++) {
			if (g_ascii_strcasecmp (flags[i], "text/html") == 0 ||
			    g_ascii_strcasecmp (flags[i], "text/plain") == 0) {
				EContentEditorMode mode = E_CONTENT_EDITOR_MODE_HTML;

				if (composer_mode) {
					if (!g_ascii_strcasecmp (composer_mode, "text/plain"))
						mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;
					else if (!g_ascii_strcasecmp (composer_mode, "text/html"))
						mode = E_CONTENT_EDITOR_MODE_HTML;
					else if (!g_ascii_strcasecmp (composer_mode, "text/markdown"))
						mode = E_CONTENT_EDITOR_MODE_MARKDOWN;
					else if (!g_ascii_strcasecmp (composer_mode, "text/markdown-plain"))
						mode = E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
					else if (!g_ascii_strcasecmp (composer_mode, "text/markdown-html"))
						mode = E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;
				} else if (!g_ascii_strcasecmp (flags[i], "text/plain")) {
					mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;
				}

				e_html_editor_set_mode (editor, mode);
			} else if (g_ascii_strcasecmp (flags[i], "pgp-sign") == 0) {
				e_ui_action_set_active (ACTION (PGP_SIGN), TRUE);
			} else if (g_ascii_strcasecmp (flags[i], "pgp-encrypt") == 0) {
				e_ui_action_set_active (ACTION (PGP_ENCRYPT), TRUE);
			} else if (g_ascii_strcasecmp (flags[i], "smime-sign") == 0) {
				e_ui_action_set_active (ACTION (SMIME_SIGN), TRUE);
			} else if (g_ascii_strcasecmp (flags[i], "smime-encrypt") == 0) {
				e_ui_action_set_active (ACTION (SMIME_ENCRYPT), TRUE);
			}
		}
		g_strfreev (flags);
	} else if (composer_mode != NULL) {
		EContentEditorMode mode = E_CONTENT_EDITOR_MODE_HTML;

		if (!g_ascii_strcasecmp (composer_mode, "text/plain"))
			mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;
		else if (!g_ascii_strcasecmp (composer_mode, "text/html"))
			mode = E_CONTENT_EDITOR_MODE_HTML;
		else if (!g_ascii_strcasecmp (composer_mode, "text/markdown"))
			mode = E_CONTENT_EDITOR_MODE_MARKDOWN;
		else if (!g_ascii_strcasecmp (composer_mode, "text/markdown-plain"))
			mode = E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
		else if (!g_ascii_strcasecmp (composer_mode, "text/markdown-html"))
			mode = E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;

		e_html_editor_set_mode (editor, mode);
	}

	if (is_message_from_draft || (
	    camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Identity") &&
	    camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Transport"))) {
		const gchar *addr;

		addr = camel_medium_get_header (CAMEL_MEDIUM (message), "Reply-To");

		if (addr)
			e_composer_header_table_set_reply_to (table, addr);

		addr = camel_medium_get_header (CAMEL_MEDIUM (message), "Mail-Followup-To");

		if (addr)
			e_composer_header_table_set_mail_followup_to (table, addr);

		addr = camel_medium_get_header (CAMEL_MEDIUM (message), "Mail-Reply-To");

		if (addr)
			e_composer_header_table_set_mail_reply_to (table, addr);
	}

	if (g_strcmp0 (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Request-DSN"), "1") == 0) {
		e_ui_action_set_active (ACTION (DELIVERY_STATUS_NOTIFICATION), TRUE);
	}

	/* Remove any other X-Evolution-* headers that may have been set */
	camel_name_value_array_free (mail_tool_remove_xevolution_headers (message));

	/* Check for receipt request */
	if (camel_medium_get_header (CAMEL_MEDIUM (message), "Disposition-Notification-To")) {
		e_ui_action_set_active (ACTION (REQUEST_READ_RECEIPT), TRUE);
	}

	/* Check for mail priority */
	if (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Priority")) {
		e_ui_action_set_active (ACTION (PRIORITIZE_MESSAGE), TRUE);
	}

	/* set extra headers */
	headers = camel_medium_get_headers (CAMEL_MEDIUM (message));
	jjlen = camel_name_value_array_get_length (headers);
	for (jj = 0; jj < jjlen; jj++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (headers, jj, &header_name, &header_value) || !header_name)
			continue;

		if (g_ascii_strcasecmp (header_name, "References") == 0 ||
		    g_ascii_strcasecmp (header_name, "In-Reply-To") == 0) {
			g_ptr_array_add (
				composer->priv->extra_hdr_names,
				g_strdup (header_name));
			g_ptr_array_add (
				composer->priv->extra_hdr_values,
				camel_header_unfold (header_value));
		}
	}

	/* Restore the attachments and body text */
	content = camel_medium_get_content (CAMEL_MEDIUM (message));
	if (CAMEL_IS_MULTIPART (content)) {
		CamelMultipart *multipart;

		mime_part = CAMEL_MIME_PART (message);
 multipart_content:
		multipart = CAMEL_MULTIPART (content);
		content_type = camel_mime_part_get_content_type (mime_part);

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* Handle the signed content and configure the
			 * composer to sign outgoing messages. */
			handle_multipart_signed (
				composer, multipart, mime_part, keep_signature, cancellable, 0);

		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* Decrypt the encrypted content and configure the
			 * composer to encrypt outgoing messages. */
			handle_multipart_encrypted (
				composer, mime_part, mime_part, keep_signature, cancellable, 0);

		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* This contains the text/plain and text/html
			 * versions of the message body. */
			handle_multipart_alternative (
				composer, multipart, mime_part, keep_signature, cancellable, 0);

		} else {
			/* There must be attachments... */
			handle_multipart (composer, multipart, mime_part, keep_signature, FALSE, cancellable, 0);
		}
	} else {
		gboolean is_html = FALSE;
		#ifdef ENABLE_SMIME
		gboolean is_smime_encrypted = FALSE;
		#endif
		gchar *html = NULL;
		gssize length = 0;

		mime_part = CAMEL_MIME_PART (message);
		content_type = camel_mime_part_get_content_type (mime_part);
		is_html = camel_content_type_is (content_type, "text", "html");

		if (content_type != NULL && (
		    camel_content_type_is (content_type, "application", "pkcs7-mime") ||
		    camel_content_type_is (content_type, "application", "xpkcs7mime") ||
		    camel_content_type_is (content_type, "application", "xpkcs7-mime") ||
		    camel_content_type_is (content_type, "application", "x-pkcs7-mime"))) {
			#ifdef ENABLE_SMIME
			e_ui_action_set_active (ACTION (SMIME_ENCRYPT), TRUE);
			is_smime_encrypted = TRUE;
			#endif
		}

		/* If we are opening message from Drafts */
		if (is_message_from_draft) {
			/* Extract the body */

			#ifdef ENABLE_SMIME
			if (is_smime_encrypted) {
				CamelSession *session;
				CamelCipherContext *cipher;
				CamelCipherValidity *validity;

				session = e_msg_composer_ref_session (composer);
				cipher = camel_smime_context_new (session);
				decrypted_part = camel_mime_part_new ();
				validity = camel_cipher_context_decrypt_sync (cipher, mime_part, decrypted_part, cancellable, NULL);
				g_object_unref (cipher);
				g_object_unref (session);

				if (validity) {
					camel_cipher_validity_free (validity);

					mime_part = decrypted_part;
					content = camel_medium_get_content (CAMEL_MEDIUM (decrypted_part));

					if (CAMEL_IS_MULTIPART (content))
						goto multipart_content;
				} else {
					g_clear_object (&decrypted_part);
				}
			}
			#endif

			html = emcu_part_as_text (composer, mime_part, &length, cancellable);
		} else if (camel_content_type_is (content_type, "text", "markdown") ||
			   emcu_format_as_plain_text (composer, content_type)) {
			is_html = FALSE;
			html = emcu_part_as_text (composer, CAMEL_MIME_PART (message), &length, cancellable);
		} else {
			is_html = TRUE;
			html = emcu_part_to_html (
				composer, CAMEL_MIME_PART (message),
				&length, keep_signature, cancellable);
		}
		e_msg_composer_set_pending_body (composer, html, length, is_html);
	}

	composer->priv->set_signature_from_message = TRUE;

	is_editor_ready = e_content_editor_is_ready (cnt_editor);

	/* We wait until now to set the body text because we need to
	 * ensure that the attachment bar has all the attachments before
	 * we request them. */
	e_msg_composer_flush_pending_body (composer);
	e_msg_composer_check_autocrypt (composer, message);

	set_signature_gui (composer);

	/* This makes sure the signature is used from the real message body,
	   not from the empty body when the composer is in the HTML mode */
	if (!is_editor_ready)
		composer->priv->set_signature_from_message = TRUE;

	#ifdef ENABLE_SMIME
	g_clear_object (&decrypted_part);
	#endif
}

/**
 * e_msg_composer_setup_redirect:
 * @composer: an #EMsgComposer
 * @message: The message to use as the source
 * @identity_uid: (nullable): an identity UID to use, if any
 * @alias_name: (nullable): an alias name to use together with the identity_uid, or %NULL
 * @alias_address: (nullable): an alias address to use together with the identity_uid, or %NULL
 * @cancellable: an optional #GCancellable
 *
 * Sets up the message @composer as a redirect of the @message.
 *
 * Since: 3.22
 **/
void
e_msg_composer_setup_redirect (EMsgComposer *composer,
			       CamelMimeMessage *message,
			       const gchar *identity_uid,
			       const gchar *alias_name,
			       const gchar *alias_address,
			       GCancellable *cancellable)
{
	EComposerHeaderTable *table;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *subject;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	composer->priv->redirect = g_object_ref (message);

	e_msg_composer_setup_with_message (composer, message, TRUE, identity_uid, alias_name, alias_address, cancellable);

	table = e_msg_composer_get_header_table (composer);
	subject = camel_mime_message_get_subject (message);

	e_composer_header_table_set_subject (table, subject);

	gtk_widget_hide (GTK_WIDGET (e_composer_header_table_get_signature_combo_box (table)));

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_editable (cnt_editor, FALSE);

	e_alert_submit (E_ALERT_SINK (editor),
		"mail-composer:info-message-redirect", NULL);
}

/**
 * e_msg_composer_ref_session:
 * @composer: an #EMsgComposer
 *
 * Returns the mail module's global #CamelSession instance.  Calling
 * this function will load the mail module if it isn't already loaded.
 *
 * The returned #CamelSession is referenced for thread-safety and must
 * be unreferenced with g_object_unref() when finished with it.
 *
 * Returns: the mail module's #CamelSession
 **/
CamelSession *
e_msg_composer_ref_session (EMsgComposer *composer)
{
	EShell *shell;
	EShellBackend *shell_backend;
	CamelSession *session = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	shell = e_msg_composer_get_shell (composer);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	g_object_get (shell_backend, "session", &session, NULL);
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	return session;
}

/**
 * e_msg_composer_get_shell:
 * @composer: an #EMsgComposer
 *
 * Returns the #EShell that was passed to e_msg_composer_new().
 *
 * Returns: the #EShell
 **/
EShell *
e_msg_composer_get_shell (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return E_SHELL (composer->priv->shell);
}

/**
 * e_msg_composer_get_content_hash:
 * @composer: an #EMsgComposer
 *
 * Returns current #EContentEditorContentHash with content
 * of the composer. It's valid, and available, only during
 * operations/signals, which construct message from the @composer
 * content. The @composer precaches the content, thus it can
 * be accessed in a synchronous way (in constrast to EContentEditor,
 * which allows getting the content only asynchronously).
 * The content hash is owned by the @composer and it is freed
 * as soon as the respective operation is finished.
 *
 * Returns: (transfer none) (nullable): an #EContentEditorContentHash
 *    with current content data, or %NULL, when it is not loaded.
 *
 * Since: 3.38
 **/
EContentEditorContentHash *
e_msg_composer_get_content_hash (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	/* Calling the function out of expected place should warn that something goes wrong */
	g_warn_if_fail (composer->priv->content_hash != NULL);

	return composer->priv->content_hash;
}

typedef void (* PrepareContentHashCallback) (EMsgComposer *composer,
					     gpointer user_data,
					     const GError *error);

typedef struct _PrepareContentHashData {
	EMsgComposer *composer;
	PrepareContentHashCallback callback;
	gpointer user_data;
} PrepareContentHashData;

static PrepareContentHashData *
prepare_content_hash_data_new (EMsgComposer *composer,
			       PrepareContentHashCallback callback,
			       gpointer user_data)
{
	PrepareContentHashData *pchd;

	pchd = g_slice_new (PrepareContentHashData);
	pchd->composer = g_object_ref (composer);
	pchd->callback = callback;
	pchd->user_data = user_data;

	return pchd;
}

static void
prepare_content_hash_data_free (gpointer ptr)
{
	PrepareContentHashData *pchd = ptr;

	if (pchd) {
		g_clear_object (&pchd->composer);
		g_slice_free (PrepareContentHashData, pchd);
	}
}

static void
e_msg_composer_prepare_content_hash_ready_cb (GObject *source_object,
					      GAsyncResult *result,
					      gpointer user_data)
{
	PrepareContentHashData *pchd = user_data;
	EContentEditorContentHash *content_hash;
	GError *error = NULL;

	g_return_if_fail (pchd != NULL);
	g_return_if_fail (E_IS_CONTENT_EDITOR (source_object));

	content_hash = e_content_editor_get_content_finish (E_CONTENT_EDITOR (source_object), result, &error);

	if (content_hash) {
		g_warn_if_fail (pchd->composer->priv->content_hash == NULL);
		g_warn_if_fail (pchd->composer->priv->content_hash_ref_count == 0);

		pchd->composer->priv->content_hash = content_hash;
		pchd->composer->priv->content_hash_ref_count = 1;
	}

	pchd->callback (pchd->composer, pchd->user_data, error);

	prepare_content_hash_data_free (pchd);
	g_clear_error (&error);
}

static void
e_msg_composer_prepare_content_hash (EMsgComposer *composer,
				     GCancellable *cancellable,
				     EActivity *activity,
				     PrepareContentHashCallback callback,
				     gpointer user_data)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	CamelInternetAddress *from;
	PrepareContentHashData *pchd;
	const gchar *from_domain = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (callback != NULL);

	/* Cannot use e_msg_composer_get_content_hash() here, because it prints
	   a runtime warning when the content_hash is NULL. */
	if (composer->priv->content_hash) {
		composer->priv->content_hash_ref_count++;

		callback (composer, user_data, NULL);
		return;
	}

	if (activity)
		e_activity_set_text (activity, _("Reading text content…"));

	pchd = prepare_content_hash_data_new (composer, callback, user_data);
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	from = e_msg_composer_get_from (composer);

	if (from && camel_internet_address_get (from, 0, NULL, &from_domain)) {
		const gchar *at = strchr (from_domain, '@');

		if (at)
			from_domain = at + 1;
		else
			from_domain = NULL;
	}

	if (!from_domain || !*from_domain)
		from_domain = "localhost";

	e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_ALL, from_domain, cancellable,
		e_msg_composer_prepare_content_hash_ready_cb, pchd);

	g_clear_object (&from);
}

static void
msg_composer_alert_response_cb (EAlert *alert,
				gint response_id,
				gpointer user_data)
{
	if (response_id == GTK_RESPONSE_ACCEPT) {
		EMsgComposer *composer = user_data;

		g_return_if_fail (E_IS_MSG_COMPOSER (composer));

		e_ui_action_set_active (ACTION (PGP_ENCRYPT), FALSE);

		#ifdef ENABLE_SMIME
		e_ui_action_set_active (ACTION (SMIME_ENCRYPT), FALSE);
		#endif

		e_msg_composer_send (composer);
	}
}

static gboolean
e_msg_composer_claim_no_build_message_error (EMsgComposer *composer,
					     EActivity *activity,
					     const GError *error,
					     gboolean unref_content_hash_on_error,
					     gboolean is_send_op)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	if (error) {
		if (!e_activity_handle_cancellation (activity, error)) {
			EAlertSink *alert_sink;
			EAlert *alert;

			alert_sink = e_activity_get_alert_sink (activity);
			alert = e_alert_new ("mail-composer:no-build-message", error->message, NULL);

			if (is_send_op && g_error_matches (error, CAMEL_CIPHER_CONTEXT_ERROR, CAMEL_CIPHER_CONTEXT_ERROR_KEY_NOT_FOUND)) {
				EUIAction *action;

				action = e_ui_action_new ("msg-composer-map", "msg-composer-alert-action-0", NULL);
				e_ui_action_set_label (action, _("Send _without encryption"));
				e_alert_add_action (alert, action, GTK_RESPONSE_ACCEPT, FALSE);
				g_object_unref (action);

				g_signal_connect_object (alert, "response",
					G_CALLBACK (msg_composer_alert_response_cb), composer, 0);
			}

			e_alert_sink_submit_alert (alert_sink, alert);
			g_object_unref (alert);
		}

		if (e_msg_composer_is_exiting (composer)) {
			gtk_window_present (GTK_WINDOW (composer));
			composer->priv->application_exiting = FALSE;
		}

		gtk_window_present (GTK_WINDOW (composer));

		if (unref_content_hash_on_error)
			e_msg_composer_unref_content_hash (composer);
	}

	return error != NULL;
}

static void
msg_composer_send_cb (EMsgComposer *composer,
                      GAsyncResult *result,
                      AsyncContext *context)
{
	CamelMimeMessage *message;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GError *error = NULL;

	message = e_msg_composer_get_message_finish (composer, result, &error);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, TRUE, TRUE)) {
		g_warn_if_fail (message == NULL);
		async_context_free (context);
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	/* The callback can set editor 'changed' if anything failed. */
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_changed (cnt_editor, TRUE);

	composer->priv->is_sending_message = TRUE;

	g_signal_emit (
		composer, signals[SEND], 0,
		message, context->activity);

	composer->priv->is_sending_message = FALSE;

	g_object_unref (message);

	e_msg_composer_unref_content_hash (composer);
	async_context_free (context);
}

static void
e_msg_composer_send_content_hash_ready_cb (EMsgComposer *composer,
					   gpointer user_data,
					   const GError *error)
{
	AsyncContext *context = user_data;
	gboolean proceed_with_send = TRUE;

	g_return_if_fail (context != NULL);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, FALSE, FALSE)) {
		async_context_free (context);
		return;
	}

	/* This gives the user a chance to abort the send. */
	g_signal_emit (composer, signals[PRESEND], 0, &proceed_with_send);

	if (!proceed_with_send) {
		gtk_window_present (GTK_WINDOW (composer));
		e_msg_composer_unref_content_hash (composer);

		if (e_msg_composer_is_exiting (composer)) {
			gtk_window_present (GTK_WINDOW (composer));
			composer->priv->application_exiting = FALSE;
		}

		async_context_free (context);
		return;
	}

	e_msg_composer_get_message (
		composer, G_PRIORITY_DEFAULT, e_activity_get_cancellable (context->activity),
		(GAsyncReadyCallback) msg_composer_send_cb,
		context);
}

/**
 * e_msg_composer_send:
 * @composer: an #EMsgComposer
 *
 * Send the message in @composer.
 **/
void
e_msg_composer_send (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	AsyncContext *context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	e_html_editor_clear_alerts (editor);

	context = g_slice_new0 (AsyncContext);
	context->activity = e_html_editor_new_activity (editor);

	cancellable = e_activity_get_cancellable (context->activity);

	e_msg_composer_prepare_content_hash (composer, cancellable, context->activity,
		e_msg_composer_send_content_hash_ready_cb, context);
}

static void
msg_composer_save_to_drafts_done_cb (gpointer user_data,
				     GObject *gone_object)
{
	EMsgComposer *composer = user_data;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (e_msg_composer_is_exiting (composer) &&
	    !e_content_editor_get_changed (cnt_editor)) {
		e_composer_emit_before_destroy (composer);
		gtk_widget_destroy (GTK_WIDGET (composer));
	} else if (e_msg_composer_is_exiting (composer)) {
		gtk_widget_set_sensitive (GTK_WIDGET (composer), TRUE);
		gtk_window_present (GTK_WINDOW (composer));
		composer->priv->application_exiting = FALSE;
	}
}

static void
msg_composer_save_to_drafts_cb (EMsgComposer *composer,
                                GAsyncResult *result,
                                AsyncContext *context)
{
	CamelMimeMessage *message;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GError *error = NULL;

	message = e_msg_composer_get_message_draft_finish (composer, result, &error);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, TRUE, FALSE)) {
		g_warn_if_fail (message == NULL);
		async_context_free (context);
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	/* The callback can set editor 'changed' if anything failed. */
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_changed (cnt_editor, TRUE);

	g_signal_emit (
		composer, signals[SAVE_TO_DRAFTS],
		0, message, context->activity);

	g_object_unref (message);

	if (e_msg_composer_is_exiting (composer))
		g_object_weak_ref (
			G_OBJECT (context->activity),
			msg_composer_save_to_drafts_done_cb, composer);

	e_msg_composer_unref_content_hash (composer);
	async_context_free (context);
}

static void
e_msg_composer_save_to_drafts_content_hash_ready_cb (EMsgComposer *composer,
						     gpointer user_data,
						     const GError *error)
{
	AsyncContext *context = user_data;

	g_return_if_fail (context != NULL);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, FALSE, FALSE)) {
		if (e_msg_composer_is_exiting (composer)) {
			gtk_window_present (GTK_WINDOW (composer));
			composer->priv->application_exiting = FALSE;
		}
		async_context_free (context);
		return;
	}

	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT, e_activity_get_cancellable (context->activity),
		(GAsyncReadyCallback) msg_composer_save_to_drafts_cb,
		context);
}

/**
 * e_msg_composer_save_to_drafts:
 * @composer: an #EMsgComposer
 *
 * Save the message in @composer to the selected account's Drafts folder.
 **/
void
e_msg_composer_save_to_drafts (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	AsyncContext *context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);

	context = g_slice_new0 (AsyncContext);
	context->activity = e_html_editor_new_activity (editor);
	context->is_draft = TRUE;

	cancellable = e_activity_get_cancellable (context->activity);

	e_msg_composer_prepare_content_hash (composer, cancellable, context->activity,
		e_msg_composer_save_to_drafts_content_hash_ready_cb, context);
}

static void
msg_composer_save_to_outbox_cb (EMsgComposer *composer,
                                GAsyncResult *result,
                                AsyncContext *context)
{
	CamelMimeMessage *message;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GError *error = NULL;

	message = e_msg_composer_get_message_finish (composer, result, &error);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, TRUE, FALSE)) {
		g_warn_if_fail (message == NULL);
		async_context_free (context);
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	g_signal_emit (
		composer, signals[SAVE_TO_OUTBOX],
		0, message, context->activity);

	g_object_unref (message);

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_changed (cnt_editor, TRUE);

	e_msg_composer_unref_content_hash (composer);

	async_context_free (context);
}

static void
e_msg_composer_save_to_outbox_content_hash_ready_cb (EMsgComposer *composer,
						     gpointer user_data,
						     const GError *error)
{
	AsyncContext *context = user_data;

	g_return_if_fail (context != NULL);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, FALSE, FALSE)) {
		async_context_free (context);
		return;
	}

	if (!composer->priv->is_sending_message) {
		gboolean proceed_with_save = TRUE;

		/* This gives the user a chance to abort the save. */
		g_signal_emit (composer, signals[PRESEND], 0, &proceed_with_save);

		if (!proceed_with_save) {
			if (e_msg_composer_is_exiting (composer)) {
				gtk_window_present (GTK_WINDOW (composer));
				composer->priv->application_exiting = FALSE;
			}

			e_msg_composer_unref_content_hash (composer);
			async_context_free (context);
			return;
		}
	}

	e_msg_composer_get_message (
		composer, G_PRIORITY_DEFAULT, e_activity_get_cancellable (context->activity),
		(GAsyncReadyCallback) msg_composer_save_to_outbox_cb,
		context);
}

/**
 * e_msg_composer_save_to_outbox:
 * @composer: an #EMsgComposer
 *
 * Save the message in @composer to the local Outbox folder.
 **/
void
e_msg_composer_save_to_outbox (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	AsyncContext *context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);

	context = g_slice_new0 (AsyncContext);
	context->activity = e_html_editor_new_activity (editor);

	cancellable = e_activity_get_cancellable (context->activity);

	e_msg_composer_prepare_content_hash (composer, cancellable, context->activity,
		e_msg_composer_save_to_outbox_content_hash_ready_cb, context);
}

static void
msg_composer_print_cb (EMsgComposer *composer,
                       GAsyncResult *result,
                       AsyncContext *context)
{
	CamelMimeMessage *message;
	GError *error = NULL;

	message = e_msg_composer_get_message_print_finish (composer, result, &error);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, TRUE, FALSE)) {
		g_warn_if_fail (message == NULL);
		async_context_free (context);
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	g_signal_emit (
		composer, signals[PRINT], 0,
		context->print_action, message, context->activity);

	g_object_unref (message);

	e_msg_composer_unref_content_hash (composer);

	async_context_free (context);
}

static void
e_msg_composer_print_content_hash_ready_cb (EMsgComposer *composer,
					    gpointer user_data,
					    const GError *error)
{
	AsyncContext *context = user_data;

	g_return_if_fail (context != NULL);

	if (e_msg_composer_claim_no_build_message_error (composer, context->activity, error, FALSE, FALSE)) {
		async_context_free (context);
		return;
	}

	e_msg_composer_get_message_print (
		composer, G_PRIORITY_DEFAULT, e_activity_get_cancellable (context->activity),
		(GAsyncReadyCallback) msg_composer_print_cb,
		context);
}

/**
 * e_msg_composer_print:
 * @composer: an #EMsgComposer
 * @print_action: the print action to start
 *
 * Print the message in @composer.
 **/
void
e_msg_composer_print (EMsgComposer *composer,
                      GtkPrintOperationAction print_action)
{
	EHTMLEditor *editor;
	AsyncContext *context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);

	context = g_slice_new0 (AsyncContext);
	context->activity = e_html_editor_new_activity (editor);
	context->print_action = print_action;

	cancellable = e_activity_get_cancellable (context->activity);

	e_msg_composer_prepare_content_hash (composer, cancellable, context->activity,
		e_msg_composer_print_content_hash_ready_cb, context);
}

static GList *
add_recipients (GList *list,
                const gchar *recips)
{
	CamelInternetAddress *cia;
	const gchar *name, *addr;
	gint num, i;

	cia = camel_internet_address_new ();
	num = camel_address_decode (CAMEL_ADDRESS (cia), recips);

	for (i = 0; i < num; i++) {
		if (camel_internet_address_get (cia, i, &name, &addr)) {
			EDestination *dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);

			list = g_list_append (list, dest);
		}
	}

	g_object_unref (cia);

	return list;
}

static gboolean
list_contains_addr (const GList *list,
                    EDestination *dest)
{
	g_return_val_if_fail (dest != NULL, FALSE);

	while (list != NULL) {
		if (e_destination_equal (dest, list->data))
			return TRUE;

		list = list->next;
	}

	return FALSE;
}

static void
merge_cc_bcc (EDestination **addrv,
              GList **merge_into,
              const GList *to,
              const GList *cc,
              const GList *bcc)
{
	gint ii;

	for (ii = 0; addrv && addrv[ii]; ii++) {
		if (!list_contains_addr (to, addrv[ii]) &&
		    !list_contains_addr (cc, addrv[ii]) &&
		    !list_contains_addr (bcc, addrv[ii])) {
			*merge_into = g_list_append (
				*merge_into, g_object_ref (addrv[ii]));
		}
	}
}

static void
merge_always_cc_and_bcc (EComposerHeaderTable *table,
                         const GList *to,
                         GList **cc,
                         GList **bcc)
{
	EDestination **addrv;

	g_return_if_fail (table != NULL);
	g_return_if_fail (cc != NULL);
	g_return_if_fail (bcc != NULL);

	addrv = e_composer_header_table_get_destinations_cc (table);
	merge_cc_bcc (addrv, cc, to, *cc, *bcc);
	e_destination_freev (addrv);

	addrv = e_composer_header_table_get_destinations_bcc (table);
	merge_cc_bcc (addrv, bcc, to, *cc, *bcc);
	e_destination_freev (addrv);
}

static const gchar *blacklist[] = { ".", "etc", ".." };

static gboolean
file_is_blacklisted (const gchar *argument)
{
	GFile *file;
	gboolean blacklisted = FALSE;
	guint ii, jj, n_parts;
	gchar *filename;
	gchar **parts;

	/* The "attach" argument may be a URI or local path.  Normalize
	 * it to a local path if we can.  We only blacklist local files. */
	file = g_file_new_for_commandline_arg (argument);
	filename = g_file_get_path (file);
	g_object_unref (file);

	if (filename == NULL)
		return FALSE;

	parts = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
	n_parts = g_strv_length (parts);

	for (ii = 0; ii < G_N_ELEMENTS (blacklist); ii++) {
		for (jj = 0; jj < n_parts; jj++) {
			if (g_str_has_prefix (parts[jj], blacklist[ii])) {
				blacklisted = TRUE;
				break;
			}
		}
	}

	if (blacklisted) {
		gchar *base_dir;

		/* Don't blacklist files in trusted base directories. */
		if (g_str_has_prefix (filename, g_get_user_data_dir ()))
			blacklisted = FALSE;
		if (g_str_has_prefix (filename, g_get_user_cache_dir ()))
			blacklisted = FALSE;
		if (g_str_has_prefix (filename, g_get_user_config_dir ()))
			blacklisted = FALSE;

		/* Apparently KDE still uses ~/.kde heavily, and some
		 * distributions use ~/.kde4 to distinguish KDE4 data
		 * from KDE3 data.  Trust these directories as well. */

		base_dir = g_build_filename (g_get_home_dir (), ".kde", NULL);
		if (g_str_has_prefix (filename, base_dir))
			blacklisted = FALSE;
		g_free (base_dir);

		base_dir = g_build_filename (g_get_home_dir (), ".kde4", NULL);
		if (g_str_has_prefix (filename, base_dir))
			blacklisted = FALSE;
		g_free (base_dir);
	}

	g_strfreev (parts);
	g_free (filename);

	return blacklisted;
}

static void
handle_mailto (EMsgComposer *composer,
               const gchar *mailto)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	EComposerHeaderTable *table;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	EDestination **tov, **ccv, **bccv;
	gchar *subject = NULL, *body = NULL;
	gchar *header, *content, *buf;
	gsize nread, nwritten;
	const gchar *p;
	gint len, clen, has_attachments = 0;
	gboolean has_blacklisted_attachment = FALSE;

	table = e_msg_composer_get_header_table (composer);
	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	buf = g_strdup (mailto);

	/* Parse recipients (everything after ':' and up to three leading forward slashes until '?' or eos). */
	p = buf + 7;

	while (*p == '/' && p - buf < 10)
		p++;

	len = strcspn (p, "?");
	if (len) {
		content = g_strndup (p, len);
		camel_url_decode (content);
		to = add_recipients (to, content);
		g_free (content);
	}

	p += len;
	if (*p == '?') {
		p++;

		while (*p) {
			len = strcspn (p, "=&");

			/* If it's malformed, give up. */
			if (p[len] != '=')
				break;

			header = (gchar *) p;
			header[len] = '\0';
			p += len + 1;

			clen = strcspn (p, "&");

			content = g_strndup (p, clen);

			if (!g_ascii_strcasecmp (header, "to")) {
				camel_url_decode (content);
				to = add_recipients (to, content);
			} else if (!g_ascii_strcasecmp (header, "cc")) {
				camel_url_decode (content);
				cc = add_recipients (cc, content);
			} else if (!g_ascii_strcasecmp (header, "bcc")) {
				camel_url_decode (content);
				bcc = add_recipients (bcc, content);
			} else if (!g_ascii_strcasecmp (header, "subject")) {
				g_free (subject);
				camel_url_decode (content);
				if (g_utf8_validate (content, -1, NULL)) {
					subject = content;
					content = NULL;
				} else {
					subject = g_locale_to_utf8 (
						content, clen, &nread,
						&nwritten, NULL);
					if (subject) {
						subject = g_realloc (subject, nwritten + 1);
						subject[nwritten] = '\0';
					}
				}
			} else if (!g_ascii_strcasecmp (header, "body")) {
				g_free (body);
				camel_url_decode (content);
				if (g_utf8_validate (content, -1, NULL)) {
					body = content;
					content = NULL;
				} else {
					body = g_locale_to_utf8 (
						content, clen, &nread,
						&nwritten, NULL);
					if (body) {
						body = g_realloc (body, nwritten + 1);
						body[nwritten] = '\0';
					}
				}
			} else if (!g_ascii_strcasecmp (header, "attach") ||
				   !g_ascii_strcasecmp (header, "attachment")) {
				EAttachment *attachment;
				GFile *file;

				camel_url_decode (content);
				if (g_ascii_strncasecmp (content, "file:", 5) == 0)
					attachment = e_attachment_new_for_uri (content);
				else
					attachment = e_attachment_new_for_path (content);
				file = e_attachment_ref_file (attachment);
				if (!file || !g_file_peek_path (file) ||
				    !g_file_test (g_file_peek_path (file), G_FILE_TEST_EXISTS) ||
				    g_file_test (g_file_peek_path (file), G_FILE_TEST_IS_DIR)) {
					/* Do nothing, simply ignore the attachment request */
				} else {
					has_attachments++;

					if (file_is_blacklisted (content)) {
						has_blacklisted_attachment = TRUE;
						e_alert_submit (
							E_ALERT_SINK (e_msg_composer_get_editor (composer)),
							"mail:blacklisted-file",
							content, NULL);
					}

					e_attachment_store_add_attachment (store, attachment);
					e_attachment_load_async (
						attachment, (GAsyncReadyCallback)
						e_attachment_load_handle_error, composer);
				}
				g_object_unref (attachment);
				g_clear_object (&file);
			} else if (!g_ascii_strcasecmp (header, "from")) {
				EComposerHeader *composer_header;

				camel_url_decode (content);

				composer_header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_FROM);

				if (content && *content && composer_header && composer_header->input_widget) {
					GtkTreeModel *model;
					GtkTreeIter iter;

					model = gtk_combo_box_get_model (GTK_COMBO_BOX (composer_header->input_widget));

					if (model && gtk_tree_model_get_iter_first (model, &iter)) {
						ESourceRegistry *registry;
						gchar *combo_id = NULL, *address = NULL, *uid = NULL;
						gboolean done;

						registry = e_mail_identity_combo_box_get_registry (E_MAIL_IDENTITY_COMBO_BOX (composer_header->input_widget));

						do {
							gtk_tree_model_get (model, &iter,
								E_MAIL_IDENTITY_COMBO_BOX_COLUMN_COMBO_ID, &combo_id,
								E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID, &uid,
								E_MAIL_IDENTITY_COMBO_BOX_COLUMN_ADDRESS, &address,
								-1);

							done = combo_id && address && g_ascii_strcasecmp (address, content) == 0;

							if (!done && uid) {
								ESource *source;

								source = e_source_registry_ref_source (registry, uid);

								if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY)) {
									ESourceMailIdentity *extension;

									g_clear_pointer (&address, g_free);

									extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY);
									address = e_source_mail_identity_dup_address (extension);

									done = combo_id && address && g_ascii_strcasecmp (address, content) == 0;
								}

								g_clear_object (&source);
							}

							if (done)
								gtk_combo_box_set_active_id (GTK_COMBO_BOX (composer_header->input_widget), combo_id);

							g_clear_pointer (&combo_id, g_free);
							g_clear_pointer (&address, g_free);
							g_clear_pointer (&uid, g_free);
						} while (!done && gtk_tree_model_iter_next (model, &iter));
					}
				}
			} else if (!g_ascii_strcasecmp (header, "reply-to")) {
				camel_url_decode (content);
				e_composer_header_table_set_reply_to (table, content);
			} else if (!g_ascii_strcasecmp (header, "mail-followup-to")) {
				camel_url_decode (content);
				e_composer_header_table_set_mail_followup_to (table, content);
			} else if (!g_ascii_strcasecmp (header, "mail-reply-to")) {
				camel_url_decode (content);
				e_composer_header_table_set_mail_reply_to (table, content);
			} else {
				/* add an arbitrary header? */
				camel_url_decode (content);
				e_msg_composer_add_header (composer, header, content);
			}

			g_free (content);

			p += clen;
			if (*p == '&') {
				p++;
				if (!g_ascii_strncasecmp (p, "amp;", 4))
					p += 4;
			}
		}
	}

	g_free (buf);

	if (has_attachments && !has_blacklisted_attachment) {
		const gchar *primary;
		gchar *secondary;

		primary = g_dngettext (GETTEXT_PACKAGE,
			"Review attachment before sending.",
			"Review attachments before sending.",
			has_attachments);

		secondary = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
			"%d attachment was added by an external application. Make sure it does not contain any sensitive information before sending the message.",
			"%d attachments were added by an external application. Make sure they do not contain any sensitive information before sending the message.",
			has_attachments),
			has_attachments);

		e_alert_submit (
			E_ALERT_SINK (e_msg_composer_get_editor (composer)),
			"system:generic-warning",
			primary, secondary, NULL);

		g_free (secondary);
	}

	merge_always_cc_and_bcc (table, to, &cc, &bcc);

	tov = destination_list_to_vector (to);
	ccv = destination_list_to_vector (cc);
	bccv = destination_list_to_vector (bcc);

	g_list_free (to);
	g_list_free (cc);
	g_list_free (bcc);

	e_composer_header_table_set_destinations_to (table, tov);
	e_composer_header_table_set_destinations_cc (table, ccv);
	e_composer_header_table_set_destinations_bcc (table, bccv);

	e_destination_freev (tov);
	e_destination_freev (ccv);
	e_destination_freev (bccv);

	e_composer_header_table_set_subject (table, subject);
	g_free (subject);

	if (body) {
		GSettings *settings;
		gchar *html_body;
		guint32 flags = 0;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		if (g_settings_get_boolean (settings, "composer-magic-links")) {
			flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
		}

		if (g_settings_get_boolean (settings, "composer-mailto-body-in-pre"))
			flags |= CAMEL_MIME_FILTER_TOHTML_PRE;
		else
			flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_NL | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES | CAMEL_MIME_FILTER_TOHTML_DIV;

		g_clear_object (&settings);

		html_body = camel_text_to_html (body, flags, 0);
		set_editor_text (composer, html_body, TRUE, TRUE);
		g_free (html_body);

		g_free (body);
	}
}

/**
 * e_msg_composer_setup_from_url:
 * @composer: an #EMsgComposer
 * @url: a mailto URL
 *
 * Sets up the message @composer content as defined by the provided URL.
 *
 * Since: 3.22
 **/
void
e_msg_composer_setup_from_url (EMsgComposer *composer,
			       const gchar *url)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (g_ascii_strncasecmp (url, "mailto:", 7) == 0);

	handle_mailto (composer, url);
}

/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 * @update_signature: whether update signature in the text after setting it;
 *    Might be usually called with TRUE.
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer,
                              const gchar *text,
                              gboolean update_signature)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (text != NULL);

	/* Every usage of e_msg_composer_set_body_text is called with HTML text */
	set_editor_text (composer, text, TRUE, update_signature);
}

/**
 * e_msg_composer_set_body:
 * @composer: a composer object
 * @body: the data to initialize the composer with
 * @mime_type: the MIME type of data
 *
 * Loads the given data into the composer as the message body.
 **/
void
e_msg_composer_set_body (EMsgComposer *composer,
                         const gchar *body,
                         const gchar *mime_type)
{
	EMsgComposerPrivate *priv = composer->priv;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *content;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* Disable signature */
	priv->disable_signature = TRUE;

	content = _("The composer contains a non-text message body, which cannot be edited.");
	set_editor_text (composer, content, TRUE, FALSE);

	e_html_editor_set_mode (editor, E_CONTENT_EDITOR_MODE_PLAIN_TEXT);
	e_content_editor_set_editable (cnt_editor, FALSE);

	g_free (priv->mime_body);
	priv->mime_body = g_strdup (body);
	g_free (priv->mime_type);
	priv->mime_type = g_strdup (mime_type);

	if (!msg_composer_get_can_sign (composer)) {
		e_ui_action_set_active (ACTION (PGP_SIGN), FALSE);
		e_ui_action_set_active (ACTION (SMIME_SIGN), FALSE);
	}
}

/**
 * e_msg_composer_add_header:
 * @composer: an #EMsgComposer
 * @name: the header's name
 * @value: the header's value
 *
 * Adds a new custom header created from @name and @value.  The header
 * is not shown in the user interface but will be added to the resulting
 * MIME message when sending or saving.
 **/
void
e_msg_composer_add_header (EMsgComposer *composer,
                           const gchar *name,
                           const gchar *value)
{
	EMsgComposerPrivate *priv;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	priv = composer->priv;

	g_ptr_array_add (priv->extra_hdr_names, g_strdup (name));
	g_ptr_array_add (priv->extra_hdr_values, g_strdup (value));
}

/**
 * e_msg_composer_set_header:
 * @composer: an #EMsgComposer
 * @name: the header's name
 * @value: the header's value
 *
 * Replaces all custom headers matching @name that were added with
 * e_msg_composer_add_header() or e_msg_composer_set_header(), with
 * a new custom header created from @name and @value.  The header is
 * not shown in the user interface but will be added to the resulting
 * MIME message when sending or saving.
 **/
void
e_msg_composer_set_header (EMsgComposer *composer,
                           const gchar *name,
                           const gchar *value)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	e_msg_composer_remove_header (composer, name);
	e_msg_composer_add_header (composer, name, value);
}

/**
 * e_msg_composer_remove_header:
 * @composer: an #EMsgComposer
 * @name: the header's name
 *
 * Removes all custom headers matching @name that were added with
 * e_msg_composer_add_header() or e_msg_composer_set_header().
 **/
void
e_msg_composer_remove_header (EMsgComposer *composer,
                              const gchar *name)
{
	EMsgComposerPrivate *priv;
	guint ii;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);

	priv = composer->priv;

	for (ii = 0; ii < priv->extra_hdr_names->len; ii++) {
		if (g_strcmp0 (priv->extra_hdr_names->pdata[ii], name) == 0) {
			g_free (priv->extra_hdr_names->pdata[ii]);
			g_free (priv->extra_hdr_values->pdata[ii]);
			g_ptr_array_remove_index (priv->extra_hdr_names, ii);
			g_ptr_array_remove_index (priv->extra_hdr_values, ii);
			ii--;
		}
	}
}

/**
 * e_msg_composer_get_header:
 * @composer: an #EMsgComposer
 * @name: the header's name
 * @index: index of the header, 0-based
 *
 * Returns header value of the header named @name previously added
 * by e_msg_composer_add_header() or set by e_msg_composer_set_header().
 * The @index is which header index to return. Returns %NULL on error
 * or when the given index of the header couldn't be found.
 *
 * Returns: stored header value or NULL, if couldn't be found.
 *
 * Since: 3.20
 **/
const gchar *
e_msg_composer_get_header (EMsgComposer *composer,
			   const gchar *name,
			   gint index)
{
	EMsgComposerPrivate *priv;
	guint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = composer->priv;

	for (ii = 0; ii < priv->extra_hdr_names->len; ii++) {
		if (g_strcmp0 (priv->extra_hdr_names->pdata[ii], name) == 0) {
			if (index <= 0)
				return priv->extra_hdr_values->pdata[ii];

			index--;
		}
	}

	return NULL;
}

/**
 * e_msg_composer_set_draft_headers:
 * @composer: an #EMsgComposer
 * @folder_uri: folder URI of the last saved draft
 * @message_uid: message UID of the last saved draft
 *
 * Add special X-Evolution-Draft headers to remember the most recently
 * saved draft message, even across Evolution sessions.  These headers
 * can be used to mark the draft message for deletion after saving a
 * newer draft or sending the composed message.
 **/
void
e_msg_composer_set_draft_headers (EMsgComposer *composer,
                                  const gchar *folder_uri,
                                  const gchar *message_uid)
{
	const gchar *header_name;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (message_uid != NULL);

	header_name = "X-Evolution-Draft-Folder";
	e_msg_composer_set_header (composer, header_name, folder_uri);

	header_name = "X-Evolution-Draft-Message";
	e_msg_composer_set_header (composer, header_name, message_uid);
}

/**
 * e_msg_composer_set_source_headers:
 * @composer: an #EMsgComposer
 * @folder_uri: folder URI of the source message
 * @message_uid: message UID of the source message
 * @flags: flags to set on the source message after sending
 *
 * Add special X-Evolution-Source headers to remember the message being
 * forwarded or replied to, even across Evolution sessions.  These headers
 * can be used to set appropriate flags on the source message after sending
 * the composed message.
 **/
void
e_msg_composer_set_source_headers (EMsgComposer *composer,
                                   const gchar *folder_uri,
                                   const gchar *message_uid,
                                   CamelMessageFlags flags)
{
	GString *buffer;
	const gchar *header_name;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (message_uid != NULL);

	buffer = g_string_sized_new (32);

	if (flags & CAMEL_MESSAGE_ANSWERED)
		g_string_append (buffer, "ANSWERED ");
	if (flags & CAMEL_MESSAGE_ANSWERED_ALL)
		g_string_append (buffer, "ANSWERED_ALL ");
	if (flags & CAMEL_MESSAGE_FORWARDED)
		g_string_append (buffer, "FORWARDED ");
	if (flags & CAMEL_MESSAGE_SEEN)
		g_string_append (buffer, "SEEN ");

	header_name = "X-Evolution-Source-Folder";
	e_msg_composer_set_header (composer, header_name, folder_uri);

	header_name = "X-Evolution-Source-Message";
	e_msg_composer_set_header (composer, header_name, message_uid);

	header_name = "X-Evolution-Source-Flags";
	e_msg_composer_set_header (composer, header_name, buffer->str);

	g_string_free (buffer, TRUE);
}

/**
 * e_msg_composer_attach:
 * @composer: a composer object
 * @mime_part: the #CamelMimePart to attach
 *
 * Attaches @attachment to the message being composed in the composer.
 **/
void
e_msg_composer_attach (EMsgComposer *composer,
                       CamelMimePart *mime_part)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	EAttachment *attachment;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, composer);
	g_object_unref (attachment);
}

/**
 * e_msg_composer_set_alternative_body:
 * @composer: an #EMsgComposer
 * @mime_part: the #CamelMimePart to attach
 *
 * Sets the @mime_part as an alternative body to the composer
 * content. It should not be "text/plain" nor "text/html" and
 * it should be the best format, because it's added as the last
 * part of the "multipart/alternative" code.
 *
 * It is still shown as a regular attachment in the GUI, thus
 * the user can review or delete it.
 *
 * Since: 3.50
 **/
void
e_msg_composer_set_alternative_body (EMsgComposer *composer,
				     CamelMimePart *mime_part)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	EAttachment *attachment;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	attachment = e_attachment_new ();
	composer->priv->alternative_body_attachment = attachment;
	e_attachment_set_mime_part (attachment, mime_part);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, composer);
	g_object_unref (attachment);
}

static void
composer_get_message_ready (GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (source_object);
	CamelMimeMessage *message;
	GError *error = NULL;
	GTask *task = user_data;

	message = composer_build_message_finish (composer, res, &error);

	if (message != NULL)
		g_task_return_pointer (task, message, g_object_unref);

	if (error != NULL) {
		g_warn_if_fail (message == NULL);
		g_task_return_error (task, g_steal_pointer (&error));
	}

	e_msg_composer_unref_content_hash (composer);

	g_object_unref (task);
}

static void
composer_build_message_content_hash_ready_cb (EMsgComposer *composer,
                                              gpointer user_data,
                                              const GError *error)
{
	GTask *task = user_data;

	g_return_if_fail (task != NULL);

	if (error) {
		g_task_return_error (task, g_error_copy (error));
		e_msg_composer_unref_content_hash (composer);
	} else {
		ComposerFlags flags = GPOINTER_TO_UINT (g_task_get_task_data (task));
		guint io_priority = g_task_get_priority (task);
		GCancellable *cancellable = g_task_get_cancellable (task);
		composer_build_message (composer, flags, io_priority, cancellable,
			composer_get_message_ready, g_steal_pointer (&task));
	}

	g_clear_object (&task);
}

/**
 * e_msg_composer_get_message:
 * @composer: an #EMsgComposer
 *
 * Retrieve the message edited by the user as a #CamelMimeMessage.  The
 * #CamelMimeMessage object is created on the fly; subsequent calls to this
 * function will always create new objects from scratch.
 **/
void
e_msg_composer_get_message (EMsgComposer *composer,
                            gint io_priority,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
	GTask *task;
	ComposerFlags flags = 0;
	EHTMLEditor *editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);

	if (e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML ||
	    e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML)
		flags |= COMPOSER_FLAG_HTML_CONTENT;

	if (e_ui_action_get_active (ACTION (PRIORITIZE_MESSAGE)))
		flags |= COMPOSER_FLAG_PRIORITIZE_MESSAGE;

	if (e_ui_action_get_active (ACTION (REQUEST_READ_RECEIPT)))
		flags |= COMPOSER_FLAG_REQUEST_READ_RECEIPT;

	if (e_ui_action_get_active (ACTION (DELIVERY_STATUS_NOTIFICATION)))
		flags |= COMPOSER_FLAG_DELIVERY_STATUS_NOTIFICATION;

	if (e_ui_action_get_active (ACTION (PGP_SIGN)))
		flags |= COMPOSER_FLAG_PGP_SIGN;

	if (e_ui_action_get_active (ACTION (PGP_ENCRYPT)))
		flags |= COMPOSER_FLAG_PGP_ENCRYPT;

#ifdef ENABLE_SMIME
	if (e_ui_action_get_active (ACTION (SMIME_SIGN)))
		flags |= COMPOSER_FLAG_SMIME_SIGN;

	if (e_ui_action_get_active (ACTION (SMIME_ENCRYPT)))
		flags |= COMPOSER_FLAG_SMIME_ENCRYPT;
#endif

	task = g_task_new (composer, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_msg_composer_get_message);
	g_task_set_task_data (task, GUINT_TO_POINTER (flags), NULL);
	g_task_set_priority (task, io_priority);

	e_msg_composer_prepare_content_hash (
		composer,
		cancellable,
		NULL,
		composer_build_message_content_hash_ready_cb,
		g_steal_pointer (&task));
}

CamelMimeMessage *
e_msg_composer_get_message_finish (EMsgComposer *composer,
                                   GAsyncResult *result,
                                   GError **error)
{
	CamelMimeMessage *message;

	g_return_val_if_fail (g_task_is_valid (result, composer), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_msg_composer_get_message), NULL);

	message = g_task_propagate_pointer (G_TASK (result), error);
	if (!message)
		return NULL;

	return g_steal_pointer (&message);
}

void
e_msg_composer_get_message_print (EMsgComposer *composer,
                                  gint io_priority,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GTask *task;
	ComposerFlags flags = 0;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	flags |= COMPOSER_FLAG_HTML_CONTENT;
	flags |= COMPOSER_FLAG_SAVE_OBJECT_DATA;

	task = g_task_new (composer, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_msg_composer_get_message_print);
	g_task_set_task_data (task, GUINT_TO_POINTER (flags), NULL);
	g_task_set_priority (task, io_priority);

	e_msg_composer_prepare_content_hash (
		composer,
		cancellable,
		NULL,
		composer_build_message_content_hash_ready_cb,
		g_steal_pointer (&task));
}

CamelMimeMessage *
e_msg_composer_get_message_print_finish (EMsgComposer *composer,
                                         GAsyncResult *result,
                                         GError **error)
{
	CamelMimeMessage *message;

	g_return_val_if_fail (g_task_is_valid (result, composer), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_msg_composer_get_message_print), NULL);

	message = g_task_propagate_pointer (G_TASK (result), error);
	if (!message)
		return NULL;

	return g_steal_pointer (&message);
}

void
e_msg_composer_get_message_draft (EMsgComposer *composer,
                                  gint io_priority,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GTask *task;
	ComposerFlags flags = COMPOSER_FLAG_SAVE_DRAFT;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* We want to save HTML content everytime when we save as draft */
	flags |= COMPOSER_FLAG_SAVE_DRAFT;

	if (e_ui_action_get_active (ACTION (PRIORITIZE_MESSAGE)))
		flags |= COMPOSER_FLAG_PRIORITIZE_MESSAGE;

	if (e_ui_action_get_active (ACTION (REQUEST_READ_RECEIPT)))
		flags |= COMPOSER_FLAG_REQUEST_READ_RECEIPT;

	if (e_ui_action_get_active (ACTION (DELIVERY_STATUS_NOTIFICATION)))
		flags |= COMPOSER_FLAG_DELIVERY_STATUS_NOTIFICATION;

	if (e_ui_action_get_active (ACTION (PGP_SIGN)))
		flags |= COMPOSER_FLAG_PGP_SIGN;

	if (e_ui_action_get_active (ACTION (PGP_ENCRYPT)))
		flags |= COMPOSER_FLAG_PGP_ENCRYPT;

#ifdef ENABLE_SMIME
	if (e_ui_action_get_active (ACTION (SMIME_SIGN)))
		flags |= COMPOSER_FLAG_SMIME_SIGN;

	if (e_ui_action_get_active (ACTION (SMIME_ENCRYPT)))
		flags |= COMPOSER_FLAG_SMIME_ENCRYPT;
#endif

	task = g_task_new (composer, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_msg_composer_get_message_draft);
	g_task_set_task_data (task, GUINT_TO_POINTER (flags), NULL);
	g_task_set_priority (task, io_priority);

	e_msg_composer_prepare_content_hash (
		composer,
		cancellable,
		NULL,
		composer_build_message_content_hash_ready_cb,
		g_steal_pointer (&task));
}

CamelMimeMessage *
e_msg_composer_get_message_draft_finish (EMsgComposer *composer,
                                         GAsyncResult *result,
                                         GError **error)
{
	CamelMimeMessage *message;

	g_return_val_if_fail (g_task_is_valid (result, composer), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_msg_composer_get_message_draft), NULL);

	message = g_task_propagate_pointer (G_TASK (result), error);
	if (!message)
		return NULL;

	return g_steal_pointer (&message);
}

CamelInternetAddress *
e_msg_composer_get_from (EMsgComposer *composer)
{
	CamelInternetAddress *inet_address = NULL;
	ESourceMailIdentity *mail_identity;
	EComposerHeaderTable *table;
	ESource *source;
	const gchar *extension_name;
	gchar *uid, *alias_name = NULL, *alias_address = NULL;
	gchar *name;
	gchar *address;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);

	uid = e_composer_header_table_dup_identity_uid (table, &alias_name, &alias_address);
	if (!uid)
		return NULL;

	source = e_composer_header_table_ref_source (table, uid);
	g_return_val_if_fail (source != NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	mail_identity = e_source_get_extension (source, extension_name);

	if (alias_name) {
		name = alias_name;
		alias_name = NULL;
	} else {
		name = e_source_mail_identity_dup_name (mail_identity);
	}

	if (!name)
		name = e_source_mail_identity_dup_name (mail_identity);

	if (alias_address) {
		address = alias_address;
		alias_address = NULL;
	} else {
		address = e_source_mail_identity_dup_address (mail_identity);
	}

	g_object_unref (source);

	if (address != NULL) {
		inet_address = camel_internet_address_new ();
		camel_internet_address_add (inet_address, name, address);
	}

	g_free (uid);
	g_free (name);
	g_free (address);
	g_free (alias_name);
	g_free (alias_address);

	return inet_address;
}

CamelInternetAddress *
e_msg_composer_get_reply_to (EMsgComposer *composer)
{
	CamelInternetAddress *address;
	EComposerHeaderTable *table;
	const gchar *reply_to;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);

	reply_to = e_composer_header_table_get_reply_to (table);
	if (reply_to == NULL || *reply_to == '\0')
		return NULL;

	address = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (address), reply_to) == -1) {
		g_object_unref (address);
		address = NULL;
	}

	return address;
}

/**
 * e_msg_composer_get_raw_message_text_without_signature:
 *
 * Returns the text/plain of the message from composer without signature
 **/
GByteArray *
e_msg_composer_get_raw_message_text_without_signature (EMsgComposer *composer)
{
	EContentEditorContentHash *content_hash;
	const gchar *content;
	gsize content_length;
	GByteArray *bytes;
	gboolean needs_crlf;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	content_hash = e_msg_composer_get_content_hash (composer);
	g_return_val_if_fail (content_hash != NULL, NULL);

	content = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED);

	if (!content) {
		g_warning ("%s: Failed to retrieve content", G_STRFUNC);

		content = "";
	}

	needs_crlf = !g_str_has_suffix (content, "\r\n") && !g_str_has_suffix (content, "\n");

	content_length = strlen (content);

	bytes = g_byte_array_sized_new (content_length + 3);

	g_byte_array_append (bytes, (const guint8 *) content, content_length);

	if (needs_crlf)
		g_byte_array_append (bytes, (const guint8 *) "\r\n", 2);

	return bytes;
}

/**
 * e_msg_composer_get_raw_message_text:
 *
 * Returns the text/plain of the message from composer
 **/
GByteArray *
e_msg_composer_get_raw_message_text (EMsgComposer *composer)
{
	EContentEditorContentHash *content_hash;
	const gchar *content;
	gsize content_length;
	GByteArray *bytes;
	gboolean needs_crlf;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	content_hash = e_msg_composer_get_content_hash (composer);
	g_return_val_if_fail (content_hash != NULL, NULL);

	content = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN);

	if (!content) {
		g_warning ("%s: Failed to retrieve content", G_STRFUNC);

		content = "";
	}

	needs_crlf = !g_str_has_suffix (content, "\r\n") && !g_str_has_suffix (content, "\n");

	content_length = strlen (content);

	bytes = g_byte_array_sized_new (content_length + 3);

	g_byte_array_append (bytes, (const guint8 *) content, content_length);

	if (needs_crlf)
		g_byte_array_append (bytes, (const guint8 *) "\r\n", 2);

	return bytes;
}

gboolean
e_msg_composer_is_exiting (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, FALSE);

	return composer->priv->application_exiting;
}

void
e_msg_composer_request_close (EMsgComposer *composer)
{
	g_return_if_fail (composer != NULL);

	composer->priv->application_exiting = TRUE;
}

/* Returns whether can close the composer immediately. It will return FALSE
 * also when saving to drafts, but the e_msg_composer_is_exiting will return
 * TRUE for this case.  can_save_draft means whether can save draft
 * immediately, or rather keep it on the caller (when FALSE). If kept on the
 * folder, then returns FALSE and sets interval variable to return TRUE in
 * e_msg_composer_is_exiting. */
gboolean
e_msg_composer_can_close (EMsgComposer *composer,
                          gboolean can_save_draft)
{
	gboolean res = FALSE;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EComposerHeaderTable *table;
	GdkWindow *window;
	GtkWidget *widget;
	const gchar *subject, *message_name;
	gint response;

	widget = GTK_WIDGET (composer);
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* this means that there is an async operation running,
	 * in which case the composer cannot be closed */
	if (!e_ui_action_group_get_sensitive (composer->priv->async_actions))
		return FALSE;

	if (!e_content_editor_get_changed (cnt_editor) ||
	    e_content_editor_is_malfunction (cnt_editor))
		return TRUE;

	window = gtk_widget_get_window (widget);
	gdk_window_raise (window);

	table = e_msg_composer_get_header_table (composer);
	subject = e_composer_header_table_get_subject (table);

	if (subject == NULL || *subject == '\0')
		message_name = "mail-composer:exit-unsaved-no-subject";
	else
		message_name = "mail-composer:exit-unsaved";

	response = e_alert_run_dialog_for_args (
		GTK_WINDOW (composer),
		message_name,
		subject, NULL);

	switch (response) {
		case GTK_RESPONSE_YES:
			e_msg_composer_request_close (composer);
			if (can_save_draft)
				g_action_activate (G_ACTION (ACTION (SAVE_DRAFT)), NULL);
			break;

		case GTK_RESPONSE_NO:
			res = TRUE;
			break;

		case GTK_RESPONSE_CANCEL:
			break;
	}

	return res;
}

EComposerHeaderTable *
e_msg_composer_get_header_table (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return E_COMPOSER_HEADER_TABLE (composer->priv->header_table);
}

EAttachmentView *
e_msg_composer_get_attachment_view (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return E_ATTACHMENT_VIEW (composer->priv->attachment_paned);
}

void
e_save_spell_languages (const GList *spell_dicts)
{
	GSettings *settings;
	GPtrArray *lang_array;

	/* Build a list of spell check language codes. */
	lang_array = g_ptr_array_new ();

	while (spell_dicts != NULL) {
		ESpellDictionary *dict = spell_dicts->data;
		const gchar *language_code;

		language_code = e_spell_dictionary_get_code (dict);
		g_ptr_array_add (lang_array, (gpointer) language_code);

		spell_dicts = g_list_next (spell_dicts);
	}

	g_ptr_array_add (lang_array, NULL);

	/* Save the language codes to GSettings. */
	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_settings_set_strv (
		settings, "composer-spell-languages",
		(const gchar * const *) lang_array->pdata);
	g_object_unref (settings);

	g_ptr_array_free (lang_array, TRUE);
}

void
e_msg_composer_save_focused_widget (EMsgComposer *composer)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	widget = gtk_window_get_focus (GTK_WINDOW (composer));
	composer->priv->focused_entry = widget;

	if (E_IS_CONTENT_EDITOR (widget))
		e_content_editor_selection_save (E_CONTENT_EDITOR (widget));

	if (GTK_IS_EDITABLE (widget)) {
		gtk_editable_get_selection_bounds (
			GTK_EDITABLE (widget),
			&composer->priv->focused_entry_selection_start,
			&composer->priv->focused_entry_selection_end);
	}
}

void
e_msg_composer_restore_focus_on_composer (EMsgComposer *composer)
{
	GtkWidget *widget = composer->priv->focused_entry;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (!widget)
		return;

	gtk_window_set_focus (GTK_WINDOW (composer), widget);

	if (GTK_IS_EDITABLE (widget)) {
		gtk_editable_select_region (
			GTK_EDITABLE (widget),
			composer->priv->focused_entry_selection_start,
			composer->priv->focused_entry_selection_end);
	}

	if (E_IS_CONTENT_EDITOR (widget)) {
		EContentEditor *cnt_editor = E_CONTENT_EDITOR (widget);
		e_content_editor_selection_restore (cnt_editor);
	}

	composer->priv->focused_entry = NULL;
}

gboolean
e_msg_composer_get_is_reply_or_forward (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->priv->is_reply_or_forward;
}

void
e_msg_composer_set_is_reply_or_forward (EMsgComposer *composer,
					gboolean is_reply_or_forward)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if ((composer->priv->is_reply_or_forward ? 1 : 0) == (is_reply_or_forward ? 1 : 0))
		return;

	composer->priv->is_reply_or_forward = is_reply_or_forward;

	g_object_notify (G_OBJECT (composer), "is-reply-or-forward");

	msg_composer_mail_identity_changed_cb (composer);
}

static void
e_msg_composer_get_autocrypt_settings (EMsgComposer *composer,
				       gchar **out_from_email,
				       gchar **out_keyid,
				       gboolean *out_send_public_key,
				       gboolean *out_send_prefer_encrypt)
{
	EComposerHeaderTable *table;
	gchar *identity_uid;

	table = e_msg_composer_get_header_table (composer);

	*out_from_email = NULL;
	*out_keyid = NULL;
	*out_send_public_key = FALSE;
	*out_send_prefer_encrypt = FALSE;

	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, out_from_email);
	if (identity_uid) {
		ESource *source;

		source = e_composer_header_table_ref_source (table, identity_uid);

		g_free (identity_uid);

		if (source) {
			if (e_source_has_extension (source, E_SOURCE_EXTENSION_OPENPGP)) {
				ESourceOpenPGP *extension;

				extension = e_source_get_extension (source, E_SOURCE_EXTENSION_OPENPGP);

				*out_keyid = e_source_openpgp_dup_key_id (extension);
				*out_send_public_key = e_source_openpgp_get_send_public_key (extension);
				*out_send_prefer_encrypt = e_source_openpgp_get_send_prefer_encrypt (extension);

				if (!*out_from_email) {
					ESourceMailIdentity *identity_extension;

					identity_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY);

					*out_from_email = e_source_mail_identity_dup_address (identity_extension);
				}
			}

			g_object_unref (source);
		}
	}
}

static gboolean
e_msg_composer_set_destination_autocrypt_key_in_destinations (EDestination **dests,
							      const gchar *email,
							      const guchar *keydata,
							      gsize keydata_size)
{
	guint ii;

	if (!dests)
		return FALSE;

	for (ii = 0; dests[ii]; ii++) {
		EDestination *dest = dests[ii];

		if (e_destination_get_email (dest) && g_ascii_strcasecmp (e_destination_get_email (dest), email) == 0) {
			EContact *contact;
			EContactCert *cert;
			gint email_num;

			contact = e_destination_get_contact (dest);
			if (contact) {
				g_object_ref (contact);
				email_num = e_destination_get_email_num (dest);
			} else {
				email_num = 0;
				contact = e_contact_new ();
				e_contact_set (contact, E_CONTACT_FULL_NAME, e_destination_get_name (dest));
				e_contact_set (contact, E_CONTACT_EMAIL_1, e_destination_get_email (dest));
			}

			cert = e_contact_cert_new ();
			cert->length = keydata_size;
			cert->data = (gchar *) keydata;

			e_contact_set (contact, E_CONTACT_PGP_CERT, cert);

			e_destination_set_contact (dest, contact, email_num);

			/* Cannot free the 'data', it's only borrowed */
			cert->data = NULL;

			e_contact_cert_free (cert);
			g_clear_object (&contact);

			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
e_msg_composer_set_destination_autocrypt_key (EMsgComposer *composer,
					      const gchar *email,
					      const guchar *keydata,
					      gsize keydata_size)
{
	EComposerHeaderTable *table;
	EDestination **dests;

	if (!email || !*email || !keydata || !keydata_size)
		return FALSE;

	table = e_msg_composer_get_header_table (composer);

	dests = e_composer_header_table_get_destinations_to (table);
	if (e_msg_composer_set_destination_autocrypt_key_in_destinations (dests, email, keydata, keydata_size)) {
		e_composer_header_table_set_destinations_to (table, dests);
		e_destination_freev (dests);

		return TRUE;
	}

	e_destination_freev (dests);

	dests = e_composer_header_table_get_destinations_cc (table);
	if (e_msg_composer_set_destination_autocrypt_key_in_destinations (dests, email, keydata, keydata_size)) {
		e_composer_header_table_set_destinations_cc (table, dests);
		e_destination_freev (dests);

		return TRUE;
	}

	e_destination_freev (dests);

	dests = e_composer_header_table_get_destinations_bcc (table);
	if (e_msg_composer_set_destination_autocrypt_key_in_destinations (dests, email, keydata, keydata_size)) {
		e_composer_header_table_set_destinations_bcc (table, dests);
		e_destination_freev (dests);

		return TRUE;
	}

	e_destination_freev (dests);

	return FALSE;
}

void
e_msg_composer_check_autocrypt (EMsgComposer *composer,
				CamelMimeMessage *original_message)
{
	EAlertBar *alert_bar;
	gchar *from_email = NULL;
	gchar *keyid = NULL;
	gboolean send_public_key = FALSE;
	gboolean send_prefer_encrypt = FALSE;
	gboolean sender_prefer_encrypt = FALSE;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	if (original_message)
		g_return_if_fail (CAMEL_IS_MIME_MESSAGE (original_message));

	e_msg_composer_remove_header (composer, "Autocrypt");

	alert_bar = e_html_editor_get_alert_bar (e_msg_composer_get_editor (composer));
	if (alert_bar)
		e_alert_bar_remove_alert_by_tag (alert_bar, "mail-composer:info-autocrypt-header-too-large");

	if (e_ui_action_get_active (ACTION (SMIME_SIGN)) ||
	    e_ui_action_get_active (ACTION (SMIME_ENCRYPT))) {
		/* Autocrypt is about GPG, thus ignore it when the user uses S/MIME */
		return;
	}

	e_msg_composer_get_autocrypt_settings (composer, &from_email, &keyid, &send_public_key, &send_prefer_encrypt);

	if (original_message && camel_mime_message_get_from (original_message) &&
	    camel_medium_get_header (CAMEL_MEDIUM (original_message), "Autocrypt")) {
		gboolean done = FALSE;
		guint ii;

		for (ii = 0; !done; ii++) {
			guchar *sender_keydata = NULL;
			gsize sender_keydata_size = 0;

			done = !em_utils_decode_autocrypt_header (original_message, ii, &sender_prefer_encrypt, &sender_keydata, &sender_keydata_size);

			if (!done && sender_keydata && sender_keydata_size > 0) {
				/* Extract the sender's key even if the sender does not prefer encrypt,
				   thus the key is available later, if needed */
				sender_prefer_encrypt = e_msg_composer_set_destination_autocrypt_key (composer, from_email, sender_keydata, sender_keydata_size) &&
					sender_prefer_encrypt;
				done = TRUE;
			}

			g_free (sender_keydata);
		}
	}

	if (send_public_key && from_email && *from_email) {
		CamelSession *session;
		CamelGpgContext *gpgctx;
		gchar *keydata = NULL;

		session = e_msg_composer_ref_session (composer);
		gpgctx = CAMEL_GPG_CONTEXT (camel_gpg_context_new (session));

		if (gpgctx) {
			guint8 *data = NULL;
			gsize data_size = 0;

			/* This should do no network I/O, aka be lightning fast */
			if (camel_gpg_context_get_public_key_sync (gpgctx, keyid ? keyid : from_email, 0, &data, &data_size, NULL, NULL) && data && data_size > 0) {
				keydata = g_base64_encode ((const guchar *) data, data_size);

				g_free (data);
			}
		}

		if (keydata) {
			GString *value;
			gint ii;

			value = g_string_sized_new (strlen (keydata) + strlen (from_email) + 64 + (strlen (keydata) / CAMEL_FOLD_SIZE) + 1);

			g_string_append (value, "addr=");
			g_string_append (value, from_email);

			if (send_prefer_encrypt)
				g_string_append (value, "; prefer-encrypt=mutual");

			ii = value->len + 2; /* just at "keydata=" */

			/* keep it as the last parameter */
			g_string_append (value, "; keydata=");
			g_string_append (value, keydata);

			/* Ignore headers above 10KB in size. See:
			   https://autocrypt.org/level1.html#id74 */
			if (value->len <= 10240) {
				/* insert "folding spaces" into the encoded key, to not have too long header lines;
				   these spaces are ignored during decode of the key */
				for (ii += CAMEL_FOLD_SIZE; ii < value->len - 1; ii += CAMEL_FOLD_SIZE + 1) {
					g_string_insert_c (value, ii, ' ');
				}
				e_msg_composer_add_header (composer, "Autocrypt", value->str);
			} else {
				e_alert_submit (E_ALERT_SINK (e_msg_composer_get_editor (composer)),
					"mail-composer:info-autocrypt-header-too-large", from_email, NULL);
			}

			g_string_free (value, TRUE);
		}

		g_clear_object (&gpgctx);
		g_clear_object (&session);
		g_free (keydata);
	}

	if (send_prefer_encrypt && sender_prefer_encrypt && msg_composer_get_can_sign (composer)) {
		/* Set both sign & encrypt, not only encrypt */
		e_ui_action_set_active (ACTION (PGP_SIGN), TRUE);
		e_ui_action_set_active (ACTION (PGP_ENCRYPT), TRUE);
	}

	g_free (from_email);
	g_free (keyid);
}

void
e_msg_composer_set_is_imip (EMsgComposer *composer,
			    gboolean is_imip)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	composer->priv->is_imip = is_imip;

	if (!msg_composer_get_can_sign (composer)) {
		e_ui_action_set_active (ACTION (PGP_SIGN), FALSE);
		e_ui_action_set_active (ACTION (PGP_ENCRYPT), FALSE);
		e_ui_action_set_active (ACTION (SMIME_SIGN), FALSE);
		e_ui_action_set_active (ACTION (SMIME_ENCRYPT), FALSE);
	}
}

gboolean
e_msg_composer_get_is_imip (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->priv->is_imip;
}
