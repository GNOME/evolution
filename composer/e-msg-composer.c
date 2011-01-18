/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "e-util/e-account-utils.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-extensible.h"
#include "e-util/e-plugin-ui.h"
#include "e-util/e-signature-utils.h"
#include "e-util/e-util-private.h"
#include "e-signature-combo-box.h"
#include "shell/e-shell.h"
#include "em-format/em-format.h"
#include "em-format/em-format-quote.h"
#include "misc/e-web-view.h"

#include "e-msg-composer.h"
#include "e-attachment.h"
#include "e-composer-private.h"
#include "e-composer-header-table.h"

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#define d(x)

#define E_MSG_COMPOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MSG_COMPOSER, EMsgComposerPrivate))

enum {
	PROP_0,
	PROP_FOCUS_TRACKER,
	PROP_SHELL
};

enum {
	SEND,
	SAVE_DRAFT,
	PRINT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* local prototypes */
static GList *add_recipients (GList *list, const gchar *recips);

static void handle_mailto (EMsgComposer *composer, const gchar *mailto);

/* used by e_msg_composer_add_message_attachments () */
static void	add_attachments_from_multipart	(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 gboolean just_inlines,
						 gint depth);

/* used by e_msg_composer_new_with_message () */
static void	handle_multipart		(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 gint depth);
static void	handle_multipart_alternative	(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 gint depth);
static void	handle_multipart_encrypted	(EMsgComposer *composer,
						 CamelMimePart *multipart,
						 gint depth);
static void	handle_multipart_signed		(EMsgComposer *composer,
						 CamelMultipart *multipart,
						 gint depth);

G_DEFINE_TYPE_WITH_CODE (
	EMsgComposer,
	e_msg_composer,
	GTKHTML_TYPE_EDITOR,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

/**
 * emcu_part_to_html:
 * @part:
 *
 * Converts a mime part's contents into html text.  If @credits is given,
 * then it will be used as an attribution string, and the
 * content will be cited.  Otherwise no citation or attribution
 * will be performed.
 *
 * Return Value: The part in displayable html format.
 **/
static gchar *
emcu_part_to_html (CamelMimePart *part, gssize *len, EMFormat *source)
{
	EMFormatQuote *emfq;
	CamelStreamMem *mem;
	GByteArray *buf;
	gchar *text;

	buf = g_byte_array_new ();
	mem = (CamelStreamMem *) camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (mem, buf);

	emfq = em_format_quote_new (NULL, (CamelStream *)mem, EM_FORMAT_QUOTE_KEEP_SIG);
	((EMFormat *) emfq)->composer = TRUE;
	if (source) {
		/* Copy over things we can, other things are internal.
		 * XXX Perhaps need different api than 'clone'. */
		if (source->default_charset)
			em_format_set_default_charset (
				(EMFormat *) emfq, source->default_charset);
		if (source->charset)
			em_format_set_default_charset (
				(EMFormat *) emfq, source->charset);
	}
	em_format_part ((EMFormat *) emfq, (CamelStream *)mem, part);
	g_object_unref (emfq);

	camel_stream_write((CamelStream *) mem, "", 1, NULL);
	g_object_unref (mem);

	text = (gchar *)buf->data;
	if (len)
		*len = buf->len-1;
	g_byte_array_free (buf, FALSE);

	return text;
}

/* copy of em_utils_prompt_user from mailer */
static gboolean
emcu_prompt_user (GtkWindow *parent, const gchar *promptkey, const gchar *tag, ...)
{
	GtkDialog *mbox;
	GtkWidget *check = NULL;
	GtkWidget *container;
	va_list ap;
	gint button;
	GConfClient *gconf = gconf_client_get_default ();
	EAlert *alert = NULL;

	if (promptkey
	    && !gconf_client_get_bool (gconf, promptkey, NULL)) {
		g_object_unref (gconf);
		return TRUE;
	}

	va_start (ap, tag);
	alert = e_alert_new_valist (tag, ap);
	va_end (ap);

	mbox = (GtkDialog*) e_alert_dialog_new (parent, alert);
	g_object_unref (alert);

	if (promptkey) {
		check = gtk_check_button_new_with_mnemonic (_("_Do not show this message again."));
		gtk_container_set_border_width ((GtkContainer *)check, 12);
		container = gtk_dialog_get_content_area (mbox);
		gtk_box_pack_start (GTK_BOX (container), check, TRUE, TRUE, 0);
		gtk_widget_show (check);
	}

	button = gtk_dialog_run (mbox);
	if (promptkey)
		gconf_client_set_bool (
			gconf, promptkey,
			!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (check)), NULL);

	gtk_widget_destroy ((GtkWidget*) mbox);
	g_object_unref (gconf);

	return button == GTK_RESPONSE_YES;
}

/* copy of mail_tool_remove_xevolution_headers */
static struct _camel_header_raw *
emcu_remove_xevolution_headers (CamelMimeMessage *message)
{
	struct _camel_header_raw *scan, *list = NULL;

	for (scan = ((CamelMimePart *)message)->headers;scan;scan=scan->next)
		if (!strncmp(scan->name, "X-Evolution", 11))
			camel_header_raw_append (&list, scan->name, scan->value, scan->offset);

	for (scan=list;scan;scan=scan->next)
		camel_medium_remove_header ((CamelMedium *)message, scan->name);

	return list;
}

static EDestination**
destination_list_to_vector_sized (GList *list, gint n)
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

static EDestination**
destination_list_to_vector (GList *list)
{
	return destination_list_to_vector_sized (list, -1);
}

#define LINE_LEN 72

static gboolean
text_requires_quoted_printable (const gchar *text, gsize len)
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

static CamelTransferEncoding
best_encoding (GByteArray *buf, const gchar *charset)
{
	gchar *in, *out, outbuf[256], *ch;
	gsize inlen, outlen;
	gint status, count = 0;
	iconv_t cd;

	if (!charset)
		return -1;

	cd = camel_iconv_open (charset, "utf-8");
	if (cd == (iconv_t) -1)
		return -1;

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
		return -1;

	if ((count == 0) && (buf->len < LINE_LEN) &&
		!text_requires_quoted_printable (
		(const gchar *) buf->data, buf->len))
		return CAMEL_TRANSFER_ENCODING_7BIT;
	else if (count <= buf->len * 0.17)
		return CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
	else
		return CAMEL_TRANSFER_ENCODING_BASE64;
}

static gchar *
best_charset (GByteArray *buf,
              const gchar *default_charset,
              CamelTransferEncoding *encoding)
{
	gchar *charset;

	/* First try US-ASCII */
	*encoding = best_encoding (buf, "US-ASCII");
	if (*encoding == CAMEL_TRANSFER_ENCODING_7BIT)
		return NULL;

	/* Next try the user-specified charset for this message */
	*encoding = best_encoding (buf, default_charset);
	if (*encoding != -1)
		return g_strdup (default_charset);

	/* Now try the user's default charset from the mail config */
	charset = e_composer_get_default_charset ();
	*encoding = best_encoding (buf, charset);
	if (*encoding != -1)
		return charset;

	/* Try to find something that will work */
	if (!(charset = (gchar *) camel_charset_best ((const gchar *)buf->data, buf->len))) {
		*encoding = CAMEL_TRANSFER_ENCODING_7BIT;
		return NULL;
	}

	*encoding = best_encoding (buf, charset);

	return g_strdup (charset);
}

static void
clear_current_images (EMsgComposer *composer)
{
	EMsgComposerPrivate *p = composer->priv;
	g_list_free (p->current_images);
	p->current_images = NULL;
}

void
e_msg_composer_clear_inlined_table (EMsgComposer *composer)
{
	EMsgComposerPrivate *p = composer->priv;

	g_hash_table_remove_all (p->inline_images);
	g_hash_table_remove_all (p->inline_images_by_url);
}

static void
add_inlined_images (EMsgComposer *composer, CamelMultipart *multipart)
{
	EMsgComposerPrivate *p = composer->priv;

	GList *d = p->current_images;
	GHashTable *added;

	added = g_hash_table_new (g_direct_hash, g_direct_equal);
	while (d) {
		CamelMimePart *part = d->data;

		if (!g_hash_table_lookup (added, part)) {
			camel_multipart_add_part (multipart, part);
			g_hash_table_insert (added, part, part);
		}
		d = d->next;
	}
	g_hash_table_destroy (added);
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

	to_addr  = camel_internet_address_new ();
	cc_addr  = camel_internet_address_new ();
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

	header = redirect ? CAMEL_RECIPIENT_TYPE_RESENT_TO : CAMEL_RECIPIENT_TYPE_TO;
	if (camel_address_length (CAMEL_ADDRESS (to_addr)) > 0) {
		camel_mime_message_set_recipients (msg, header, to_addr);
	} else if (seen_hidden_list) {
		camel_medium_set_header (CAMEL_MEDIUM (msg), header, "Undisclosed-Recipient:;");
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
                       CamelMimeMessage *msg,
                       gboolean redirect)
{
	EComposerHeaderTable *table;
	EComposerHeader *header;
	EAccount *account;
	const gchar *subject;
	const gchar *reply_to;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));

	table = e_msg_composer_get_header_table (composer);

	/* Subject: */
	subject = e_composer_header_table_get_subject (table);
	camel_mime_message_set_subject (msg, subject);

	/* From: / Resent-From: */
	account = e_composer_header_table_get_account (table);
	if (account != NULL) {
		CamelInternetAddress *addr;
		const gchar *name = account->id->name;
		const gchar *address = account->id->address;

		addr = camel_internet_address_new ();
		camel_internet_address_add (addr, name, address);

		if (redirect) {
			gchar *value;

			value = camel_address_encode (CAMEL_ADDRESS (addr));
			camel_medium_set_header (
				CAMEL_MEDIUM (msg), "Resent-From", value);
			g_free (value);
		} else
			camel_mime_message_set_from (msg, addr);

		g_object_unref (addr);
	}

	/* Reply-To: */
	reply_to = e_composer_header_table_get_reply_to (table);
	if (reply_to != NULL && *reply_to != '\0') {
		CamelInternetAddress *addr;

		addr = camel_internet_address_new ();

		if (camel_address_unformat (CAMEL_ADDRESS (addr), reply_to) > 0)
			camel_mime_message_set_reply_to (msg, addr);

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

		set_recipients_from_destv (msg, to, cc, bcc, redirect);

		e_destination_freev (to);
		e_destination_freev (cc);
		e_destination_freev (bcc);
	}

	/* X-Evolution-PostTo: */
	header = e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_POST_TO);
	if (e_composer_header_get_visible (header)) {
		CamelMedium *medium = CAMEL_MEDIUM (msg);
		const gchar *name = "X-Evolution-PostTo";
		GList *list, *iter;

		camel_medium_remove_header (medium, name);

		list = e_composer_header_table_get_post_to (table);
		for (iter = list; iter != NULL; iter = iter->next) {
			gchar *folder = iter->data;
			camel_medium_add_header (medium, name, folder);
			g_free (folder);
		}
		g_list_free (list);
	}
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

static CamelMimeMessage *
build_message (EMsgComposer *composer,
               gboolean html_content,
               gboolean save_html_object_data,
               GError **error)
{
	GtkhtmlEditor *editor;
	EMsgComposerPrivate *p = composer->priv;

	EAttachmentView *view;
	EAttachmentStore *store;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	CamelDataWrapper *plain, *html, *current;
	CamelTransferEncoding plain_encoding;
	const gchar *iconv_charset = NULL;
	GPtrArray *recipients = NULL;
	CamelMultipart *body = NULL;
	CamelContentType *type;
	CamelMimeMessage *new;
	CamelSession *session;
	CamelStream *stream;
	CamelMimePart *part;
	GByteArray *data;
	EAccount *account;
	gchar *charset;
	gboolean pgp_sign;
	gboolean pgp_encrypt;
	gboolean smime_sign;
	gboolean smime_encrypt;
	gint i;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	editor = GTKHTML_EDITOR (composer);
	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);
	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);
	session = e_msg_composer_get_session (composer);

	/* evil kludgy hack for Redirect */
	if (p->redirect) {
		build_message_headers (composer, p->redirect, TRUE);
		g_object_ref (p->redirect);
		return p->redirect;
	}

	new = camel_mime_message_new ();
	build_message_headers (composer, new, FALSE);
	for (i = 0; i < p->extra_hdr_names->len; i++) {
		camel_medium_add_header (
			CAMEL_MEDIUM (new),
			p->extra_hdr_names->pdata[i],
			p->extra_hdr_values->pdata[i]);
	}

	/* Message Disposition Notification */
	action = GTK_TOGGLE_ACTION (ACTION (REQUEST_READ_RECEIPT));
	if (gtk_toggle_action_get_active (action)) {
		gchar *mdn_address = account->id->reply_to;
		if (!mdn_address || !*mdn_address)
			mdn_address = account->id->address;

		camel_medium_add_header (
			CAMEL_MEDIUM (new),
			"Disposition-Notification-To", mdn_address);
	}

	/* Message Priority */
	action = GTK_TOGGLE_ACTION (ACTION (PRIORITIZE_MESSAGE));
	if (gtk_toggle_action_get_active (action))
		camel_medium_add_header (
			CAMEL_MEDIUM (new), "X-Priority", "1");

	if (p->mime_body) {
		if (text_requires_quoted_printable (p->mime_body, -1)) {
			plain_encoding = CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
		} else {
			plain_encoding = CAMEL_TRANSFER_ENCODING_7BIT;
			for (i = 0; p->mime_body[i]; i++) {
				if ((guchar) p->mime_body[i] > 127) {
					plain_encoding = CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
					break;
				}
			}
		}
		data = g_byte_array_new ();
		g_byte_array_append (data, (const guint8 *)p->mime_body, strlen (p->mime_body));
		type = camel_content_type_decode (p->mime_type);
	} else {
		gchar *text;
		gsize length;

		data = g_byte_array_new ();
		text = gtkhtml_editor_get_text_plain (editor, &length);
		g_byte_array_append (data, (guint8 *) text, (guint) length);
		g_free (text);

		/* FIXME: we may want to do better than this... */

		type = camel_content_type_new ("text", "plain");
		if ((charset = best_charset (data, p->charset, &plain_encoding))) {
			camel_content_type_set_param (type, "charset", charset);
			iconv_charset = camel_iconv_charset_name (charset);
			g_free (charset);
		}
	}

	stream = camel_stream_mem_new_with_byte_array (data);

	/* convert the stream to the appropriate charset */
	if (iconv_charset && g_ascii_strcasecmp (iconv_charset, "UTF-8") != 0) {
		CamelStream *filter_stream;
		CamelMimeFilter *filter;

		filter_stream = camel_stream_filter_new (stream);
		g_object_unref (stream);

		stream = filter_stream;
		filter = camel_mime_filter_charset_new ("UTF-8", iconv_charset);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filter_stream), filter);
		g_object_unref (filter);
	}

	if (plain_encoding == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE) {
		/* encode to quoted-printable by ourself, together with
		 * taking care of "\nFrom " text */
		CamelStream *filter_stream;
		CamelMimeFilter *mf, *qp;

		if (!CAMEL_IS_STREAM_FILTER (stream)) {
			filter_stream = camel_stream_filter_new (stream);
			g_object_unref (stream);

			stream = filter_stream;
		}

		qp = camel_mime_filter_basic_new (
			CAMEL_MIME_FILTER_BASIC_QP_ENC);
		camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), qp);
		g_object_unref (qp);

		mf = camel_mime_filter_canon_new (CAMEL_MIME_FILTER_CANON_FROM);
		camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), mf);
		g_object_unref (mf);
	}

	/* construct the content object */
	plain = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (plain, stream, NULL);
	g_object_unref (stream);

	if (plain_encoding == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE) {
		/* to not re-encode the data when pushing it to a part */
		plain->encoding = plain_encoding;
	}

	camel_data_wrapper_set_mime_type_field (plain, type);
	camel_content_type_unref (type);

	if (html_content) {
		gchar *text;
		gsize length;
		gboolean pre_encode;

		clear_current_images (composer);

		if (save_html_object_data)
			gtkhtml_editor_run_command (editor, "save-data-on");

		data = g_byte_array_new ();
		text = gtkhtml_editor_get_text_html (editor, &length);
		g_byte_array_append (data, (guint8 *) text, (guint) length);
		pre_encode = text_requires_quoted_printable (text, length);
		g_free (text);

		if (save_html_object_data)
			gtkhtml_editor_run_command (editor, "save-data-off");

		html = camel_data_wrapper_new ();

		stream = camel_stream_mem_new_with_byte_array (data);

		if (pre_encode) {
			/* encode to quoted-printable by ourself, together with
			 * taking care of "\nFrom " text */
			CamelStream *filter_stream;
			CamelMimeFilter *mf, *qp;

			if (!CAMEL_IS_STREAM_FILTER (stream)) {
				filter_stream = camel_stream_filter_new (stream);
				g_object_unref (stream);

				stream = filter_stream;
			}

			qp = camel_mime_filter_basic_new (
				CAMEL_MIME_FILTER_BASIC_QP_ENC);
			camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), qp);
			g_object_unref (qp);

			mf = camel_mime_filter_canon_new (CAMEL_MIME_FILTER_CANON_FROM);
			camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), mf);
			g_object_unref (mf);
		}

		camel_data_wrapper_construct_from_stream (html, stream, NULL);
		g_object_unref (stream);
		camel_data_wrapper_set_mime_type (html, "text/html; charset=utf-8");

		if (pre_encode) {
			/* to not re-encode the data when pushing it to a part */
			html->encoding = CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
		}

		/* Build the multipart/alternative */
		body = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (
			CAMEL_DATA_WRAPPER (body), "multipart/alternative");
		camel_multipart_set_boundary (body, NULL);

		part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (part), plain);
		g_object_unref (plain);
		camel_mime_part_set_encoding (part, plain_encoding);
		camel_multipart_add_part (body, part);
		g_object_unref (part);

		part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (part), html);
		g_object_unref (html);
		camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
		camel_multipart_add_part (body, part);
		g_object_unref (part);

		/* If there are inlined images, construct a
		 * multipart/related containing the
		 * multipart/alternative and the images.
		 */
		if (p->current_images) {
			CamelMultipart *html_with_images;

			html_with_images = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (
				CAMEL_DATA_WRAPPER (html_with_images),
				"multipart/related; type=\"multipart/alternative\"");
			camel_multipart_set_boundary (html_with_images, NULL);

			part = camel_mime_part_new ();
			camel_medium_set_content (CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (body));
			g_object_unref (body);
			camel_multipart_add_part (html_with_images, part);
			g_object_unref (part);

			add_inlined_images (composer, html_with_images);
			clear_current_images (composer);

			current = CAMEL_DATA_WRAPPER (html_with_images);
		} else
			current = CAMEL_DATA_WRAPPER (body);
	} else
		current = plain;

	if (e_attachment_store_get_num_attachments (store) > 0) {
		CamelMultipart *multipart = camel_multipart_new ();

		if (p->is_alternative) {
			camel_data_wrapper_set_mime_type (
				CAMEL_DATA_WRAPPER (multipart),
				"multipart/alternative");
		}

		/* Generate a random boundary. */
		camel_multipart_set_boundary (multipart, NULL);

		part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (part), current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		g_object_unref (current);
		camel_multipart_add_part (multipart, part);
		g_object_unref (part);

		e_attachment_store_add_to_multipart (
			store, multipart, p->charset);

		if (p->is_alternative) {
			for (i = camel_multipart_get_number (multipart); i > 1; i--) {
				part = camel_multipart_get_part (multipart, i - 1);
				camel_medium_remove_header (CAMEL_MEDIUM (part), "Content-Disposition");
			}
		}

		current = CAMEL_DATA_WRAPPER (multipart);
	}

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	pgp_sign = gtk_toggle_action_get_active (action);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	pgp_encrypt = gtk_toggle_action_get_active (action);

#if defined (HAVE_NSS)
	action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
	smime_sign = gtk_toggle_action_get_active (action);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
	smime_encrypt = gtk_toggle_action_get_active (action);
#else
	smime_sign = FALSE;
	smime_encrypt = FALSE;
#endif

	/* Setup working recipient list if we're encrypting */
	if (pgp_encrypt || smime_encrypt) {
		gint j;
		const gchar *types[] = {
			CAMEL_RECIPIENT_TYPE_TO,
			CAMEL_RECIPIENT_TYPE_CC,
			CAMEL_RECIPIENT_TYPE_BCC
		};

		recipients = g_ptr_array_new ();
		for (i = 0; i < G_N_ELEMENTS (types); i++) {
			CamelInternetAddress *addr;
			const gchar *address;

			addr = camel_mime_message_get_recipients (new, types[i]);
			for (j=0;camel_internet_address_get (addr, j, NULL, &address); j++)
				g_ptr_array_add (recipients, g_strdup (address));

		}
	}

	if (pgp_sign || pgp_encrypt) {
		const gchar *pgp_userid;
		CamelInternetAddress *from = NULL;
		CamelCipherContext *cipher;

		part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (part), current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		g_object_unref (current);

		if (account && account->pgp_key && *account->pgp_key) {
			pgp_userid = account->pgp_key;
		} else {
			from = e_msg_composer_get_from (composer);
			camel_internet_address_get (from, 0, NULL, &pgp_userid);
		}

		if (pgp_sign) {
			CamelMimePart *npart = camel_mime_part_new ();

			cipher = camel_gpg_context_new (session);
			if (account != NULL)
				camel_gpg_context_set_always_trust (
					CAMEL_GPG_CONTEXT (cipher),
					account->pgp_always_trust);
			camel_cipher_sign (
				cipher, pgp_userid, account_hash_algo_to_camel_hash (
				account ? e_account_get_string (account, E_ACCOUNT_PGP_HASH_ALGORITHM) : NULL),
				part, npart, &local_error);
			g_object_unref (cipher);

			if (local_error != NULL) {
				g_object_unref (npart);
				goto exception;
			}

			g_object_unref (part);
			part = npart;
		}

		if (pgp_encrypt) {
			CamelMimePart *npart = camel_mime_part_new ();

			/* Check to see if we should encrypt to self.
			 * NB gets removed immediately after use */
			if (account && account->pgp_encrypt_to_self && pgp_userid)
				g_ptr_array_add (recipients, g_strdup (pgp_userid));

			cipher = camel_gpg_context_new (session);
			if (account != NULL)
				camel_gpg_context_set_always_trust (
					CAMEL_GPG_CONTEXT (cipher),
					account->pgp_always_trust);
			camel_cipher_encrypt (
				cipher, pgp_userid, recipients,
				part, npart, &local_error);
			g_object_unref (cipher);

			if (account && account->pgp_encrypt_to_self && pgp_userid)
				g_ptr_array_set_size (recipients, recipients->len - 1);

			if (local_error != NULL) {
				g_object_unref (npart);
				goto exception;
			}

			g_object_unref (part);
			part = npart;
		}

		if (from)
			g_object_unref (from);

		current = camel_medium_get_content (CAMEL_MEDIUM (part));
		g_object_ref (current);
		g_object_unref (part);
	}

#if defined (HAVE_NSS)
	if (smime_sign || smime_encrypt) {
		CamelInternetAddress *from = NULL;
		CamelCipherContext *cipher;

		part = camel_mime_part_new ();
		camel_medium_set_content ((CamelMedium *)part, current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		g_object_unref (current);

		if (smime_sign && (account == NULL ||
				account->smime_sign_key == NULL ||
				account->smime_sign_key[0] == 0)) {
			g_set_error (
				&local_error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				_("Cannot sign outgoing message: "
				  "No signing certificate set for "
				  "this account"));
			goto exception;
		}

		if (smime_encrypt && (account == NULL ||
				account->smime_sign_key == NULL ||
				account->smime_sign_key[0] == 0)) {
			g_set_error (
				&local_error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				_("Cannot encrypt outgoing message: "
				  "No encryption certificate set for "
				  "this account"));
			goto exception;
		}

		if (smime_sign) {
			CamelMimePart *npart = camel_mime_part_new ();

			cipher = camel_smime_context_new (session);

			/* if we're also encrypting, envelope-sign rather than clear-sign */
			if (smime_encrypt) {
				camel_smime_context_set_sign_mode (
					(CamelSMIMEContext *) cipher,
					CAMEL_SMIME_SIGN_ENVELOPED);
				camel_smime_context_set_encrypt_key (
					(CamelSMIMEContext *) cipher,
					TRUE, account->smime_encrypt_key);
			} else if (account &&
					account->smime_encrypt_key &&
					*account->smime_encrypt_key) {
				camel_smime_context_set_encrypt_key (
					(CamelSMIMEContext *) cipher,
					TRUE, account->smime_encrypt_key);
			}

			camel_cipher_sign (cipher, account->smime_sign_key,
				account_hash_algo_to_camel_hash (
					(account != NULL) ?
					e_account_get_string (account,
					E_ACCOUNT_SMIME_HASH_ALGORITHM) : NULL),
				part, npart, &local_error);
			g_object_unref (cipher);

			if (local_error != NULL) {
				g_object_unref (npart);
				goto exception;
			}

			g_object_unref (part);
			part = npart;
		}

		if (smime_encrypt) {

			/* check to see if we should encrypt to self, NB removed after use */
			if (account->smime_encrypt_to_self)
				g_ptr_array_add (
					recipients, g_strdup (
					account->smime_encrypt_key));

			cipher = camel_smime_context_new (session);
			camel_smime_context_set_encrypt_key (
				(CamelSMIMEContext *) cipher, TRUE,
				account->smime_encrypt_key);

			camel_cipher_encrypt (
				cipher, NULL, recipients, part,
				(CamelMimePart *) new, &local_error);
			g_object_unref (cipher);

			if (local_error != NULL)
				goto exception;

			if (account->smime_encrypt_to_self)
				g_ptr_array_set_size (recipients, recipients->len - 1);
		}

		if (from)
			g_object_unref (from);

		/* we replaced the message directly, we don't want to do reparenting foo */
		if (smime_encrypt) {
			g_object_unref (part);
			goto skip_content;
		} else {
			current = camel_medium_get_content ((CamelMedium *)part);
			g_object_ref (current);
			g_object_unref (part);
		}
	}
#endif /* HAVE_NSS */

	camel_medium_set_content (CAMEL_MEDIUM (new), current);
	if (current == plain)
		camel_mime_part_set_encoding (CAMEL_MIME_PART (new), plain_encoding);
	g_object_unref (current);

#if defined (HAVE_NSS)
skip_content:
#endif
	if (recipients) {
		for (i=0; i<recipients->len; i++)
			g_free (recipients->pdata[i]);
		g_ptr_array_free (recipients, TRUE);
	}

	/* Attach whether this message was written in HTML */
	camel_medium_set_header (
		CAMEL_MEDIUM (new), "X-Evolution-Format",
		html_content ? "text/html" : "text/plain");

	return new;

exception:

	if (part != CAMEL_MIME_PART (new))
		g_object_unref (part);

	g_object_unref (new);

	if (recipients) {
		for (i=0; i<recipients->len; i++)
			g_free (recipients->pdata[i]);
		g_ptr_array_free (recipients, TRUE);
	}

	g_propagate_error (error, local_error);

	return NULL;
}

/* Signatures */

static gchar *
encode_signature_uid (ESignature *signature)
{
	const gchar *uid;
	const gchar *s;
	gchar *ename, *e;
	gint len = 0;

	uid = e_signature_get_uid (signature);

	s = uid;
	while (*s) {
		len++;
		if (*s == '"' || *s == '.' || *s == '=')
			len++;
		s++;
	}

	ename = g_new (gchar, len + 1);

	s = uid;
	e = ename;
	while (*s) {
		if (*s == '"') {
			*e = '.';
			e++;
			*e = '1';
			e++;
		} else if (*s == '=') {
			*e = '.';
			e++;
			*e = '2';
			e++;
		} else {
			*e = *s;
			e++;
		}
		if (*s == '.') {
			*e = '.';
			e++;
		}
		s++;
	}
	*e = 0;

	return ename;
}

static gchar *
decode_signature_name (const gchar *name)
{
	const gchar *s;
	gchar *dname, *d;
	gint len = 0;

	s = name;
	while (*s) {
		len++;
		if (*s == '.') {
			s++;
			if (!*s || !(*s == '.' || *s == '1' || *s == '2'))
				return NULL;
		}
		s++;
	}

	dname = g_new (char, len + 1);

	s = name;
	d = dname;
	while (*s) {
		if (*s == '.') {
			s++;
			if (!*s || !(*s == '.' || *s == '1' || *s == '2')) {
				g_free (dname);
				return NULL;
			}
			if (*s == '1')
				*d = '"';
			else if (*s == '2')
				*d = '=';
			else
				*d = '.';
		} else
			*d = *s;
		d++;
		s++;
	}
	*d = 0;

	return dname;
}

static gboolean
is_top_signature (EMsgComposer *composer)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EMsgComposerPrivate *priv = E_MSG_COMPOSER_GET_PRIVATE (composer);

	g_return_val_if_fail (priv != NULL, FALSE);

	/* The composer had been created from a stored message, thus the
	 * signature placement is either there already, or pt it at the
	 * bottom regardless of a preferences (which is for reply anyway,
	 * not for Edit as new) */
	if (priv->is_from_message)
		return FALSE;

	shell = e_msg_composer_get_shell (composer);
	shell_settings = e_shell_get_shell_settings (shell);

	return e_shell_settings_get_boolean (shell_settings, "composer-top-signature");
}

static gboolean
add_signature_delim (EMsgComposer *composer)
{
	EShell *shell;
	EShellSettings *shell_settings;

	shell = e_msg_composer_get_shell (composer);
	shell_settings = e_shell_get_shell_settings (shell);

	return !e_shell_settings_get_boolean (shell_settings, "composer-no-signature-delim");
}

#define CONVERT_SPACES CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES
#define NO_SIGNATURE_TEXT	\
	"<!--+GtkHTML:<DATA class=\"ClueFlow\" " \
	"                     key=\"signature\" " \
	"                   value=\"1\">-->" \
	"<!--+GtkHTML:<DATA class=\"ClueFlow\" " \
	"                     key=\"signature_name\" " \
	"                   value=\"uid:Noname\">--><BR>"

static gchar *
get_signature_html (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gchar *text = NULL, *html = NULL;
	ESignature *signature;
	gboolean format_html, add_delim;

	table = e_msg_composer_get_header_table (composer);
	signature = e_composer_header_table_get_signature (table);

	if (!signature)
		return NULL;

	add_delim = add_signature_delim (composer);

	if (!e_signature_get_autogenerated (signature)) {
		const gchar *filename;

		filename = e_signature_get_filename (signature);
		if (filename == NULL)
			return NULL;

		format_html = e_signature_get_is_html (signature);

		if (e_signature_get_is_script (signature))
			text = e_run_signature_script (filename);
		else
			text = e_read_signature_file (signature, TRUE, NULL);
	} else {
		EAccount *account;
		EAccountIdentity *id;
		gchar *organization;
		gchar *address;
		gchar *name;

		account = e_composer_header_table_get_account (table);
		if (!account)
			return NULL;

		id = account->id;
		address = id->address ? camel_text_to_html (id->address, CONVERT_SPACES, 0) : NULL;
		name = id->name ? camel_text_to_html (id->name, CONVERT_SPACES, 0) : NULL;
		organization =
			id->organization ? camel_text_to_html (
			id->organization, CONVERT_SPACES, 0) : NULL;

		text = g_strdup_printf ("%s%s%s%s%s%s%s%s%s",
					add_delim ? "-- \n<BR>" : "",
					name ? name : "",
					(address && *address) ? " &lt;<A HREF=\"mailto:" : "",
					address ? address : "",
					(address && *address) ? "\">" : "",
					address ? address : "",
					(address && *address) ? "</A>&gt;" : "",
					(organization && *organization) ? "<BR>" : "",
					organization ? organization : "");
		g_free (address);
		g_free (name);
		g_free (organization);
		format_html = TRUE;
	}

	/* printf ("text: %s\n", text); */
	if (text) {
		gchar *encoded_uid = NULL;
		const gchar *sig_delim = format_html ? "-- \n<BR>" : "-- \n";
		const gchar *sig_delim_ent = format_html ? "\n-- \n<BR>" : "\n-- \n";

		if (signature)
			encoded_uid = encode_signature_uid (signature);

		/* The signature dash convention ("-- \n") is specified
		 * in the "Son of RFC 1036", section 4.3.2.
		 * http://www.chemie.fu-berlin.de/outerspace/netnews/son-of-1036.html
		 */
		html = g_strdup_printf (
			"<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature\" value=\"1\">-->"
			"<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature_name\" value=\"uid:%s\">-->"
			"<TABLE WIDTH=\"100%%\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>"
			"%s%s%s%s"
			"%s</TD></TR></TABLE>",
			encoded_uid ? encoded_uid : "",
			format_html ? "" : "<PRE>\n",
			!add_delim ? "" :
				(!strncmp (
				sig_delim, text, strlen (sig_delim)) ||
				strstr (text, sig_delim_ent))
				? "" : sig_delim,
			text,
			format_html ? "" : "</PRE>\n",
			is_top_signature (composer) ? "<BR>" : "");
		g_free (text);
		g_free (encoded_uid);
		text = html;
	}

	return text;
}

static void
set_editor_text (EMsgComposer *composer,
                 const gchar *text,
                 gboolean set_signature)
{
	gchar *body = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (text != NULL);

	/*
	 *
	 * Keeping Signatures in the beginning of composer
	 * ------------------------------------------------
	 *
	 * Purists are gonna blast me for this.
	 * But there are so many people (read Outlook users) who want this.
	 * And Evo is an exchange-client, Outlook-replacement etc.
	 * So Here it goes :(
	 *
	 * -- Sankar
	 *
	 */

	if (is_top_signature (composer)) {
		/* put marker to the top */
		body = g_strdup_printf ("<BR>" NO_SIGNATURE_TEXT "%s", text);
	} else {
		/* no marker => to the bottom */
		body = g_strdup_printf ("%s<BR>", text);
	}

	gtkhtml_editor_set_text_html (GTKHTML_EDITOR (composer), body, -1);

	if (set_signature)
		e_msg_composer_show_sig_file (composer);

	g_free (body);
}

/* Miscellaneous callbacks.  */

static void
attachment_store_changed_cb (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	/* Mark the editor as changed so it prompts about unsaved
	 * changes on close. */
	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
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

static void
msg_composer_account_changed_cb (EMsgComposer *composer)
{
	EMsgComposerPrivate *p = composer->priv;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	ESignature *signature;
	EAccount *account;
	gboolean active, can_sign;
	const gchar *uid;

	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);

	if (account == NULL) {
		e_msg_composer_show_sig_file (composer);
		return;
	}

	can_sign = (!account->pgp_no_imip_sign || p->mime_type == NULL ||
		g_ascii_strncasecmp (p->mime_type, "text/calendar", 13) != 0);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	active = account->pgp_always_sign && can_sign;
	gtk_toggle_action_set_active (action, active);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
	active = account->smime_sign_default && can_sign;
	gtk_toggle_action_set_active (action, active);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
	active = account->smime_encrypt_default;
	gtk_toggle_action_set_active (action, active);

	uid = account->id->sig_uid;
	signature = uid ? e_get_signature_by_uid (uid) : NULL;
	e_composer_header_table_set_signature (table, signature);
}

static void
msg_composer_paste_clipboard_targets_cb (GtkClipboard *clipboard,
                                         GdkAtom *targets,
                                         gint n_targets,
                                         EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	gboolean html_mode;

	editor = GTKHTML_EDITOR (composer);
	html_mode = gtkhtml_editor_get_html_mode (editor);

	/* Order is important here to ensure common use cases are
	 * handled correctly.  See GNOME bug #603715 for details. */

	if (gtk_targets_include_uri (targets, n_targets)) {
		e_composer_paste_uris (composer, clipboard);
		return;
	}

	/* Only paste HTML content in HTML mode. */
	if (html_mode) {
		if (e_targets_include_html (targets, n_targets)) {
			e_composer_paste_html (composer, clipboard);
			return;
		}
	}

	if (gtk_targets_include_text (targets, n_targets)) {
		e_composer_paste_text (composer, clipboard);
		return;
	}

	if (gtk_targets_include_image (targets, n_targets, TRUE)) {
		e_composer_paste_image (composer, clipboard);
		return;
	}
}

static void
msg_composer_paste_clipboard_cb (EWebView *web_view,
                                 EMsgComposer *composer)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_targets (
		clipboard, (GtkClipboardTargetsReceivedFunc)
		msg_composer_paste_clipboard_targets_cb, composer);

	g_signal_stop_emission_by_name (web_view, "paste-clipboard");
}

static void
msg_composer_realize_gtkhtml_cb (GtkWidget *widget,
                                 EMsgComposer *composer)
{
	EAttachmentView *view;
	GtkTargetList *target_list;
	GtkTargetEntry *targets;
	gint n_targets;

	/* XXX GtkHTML doesn't set itself up as a drag destination until
	 *     it's realized, and we need to amend to its target list so
	 *     it will accept the same drag targets as the attachment bar.
	 *     Do this any earlier and GtkHTML will just overwrite us. */

	/* When redirecting a message, the message body is not
	 * editable and therefore cannot be a drag destination. */
	if (!e_web_view_get_editable (E_WEB_VIEW (widget)))
		return;

	view = e_msg_composer_get_attachment_view (composer);

	target_list = e_attachment_view_get_target_list (view);
	targets = gtk_target_table_new_from_list (target_list, &n_targets);

	target_list = gtk_drag_dest_get_target_list (widget);
	gtk_target_list_add_table (target_list, targets, n_targets);

	gtk_target_table_free (targets, n_targets);
}

static gboolean
msg_composer_drag_motion_cb (GtkWidget *widget,
                             GdkDragContext *context,
                             gint x,
                             gint y,
                             guint time,
                             EMsgComposer *composer)
{
	EAttachmentView *view;

	view = e_msg_composer_get_attachment_view (composer);

	/* Stop the signal from propagating to GtkHtml. */
	g_signal_stop_emission_by_name (widget, "drag-motion");

	return e_attachment_view_drag_motion (view, context, x, y, time);
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
	EAttachmentView *view;

	view = e_msg_composer_get_attachment_view (composer);

	/* Forward the data to the attachment view.  Note that calling
	 * e_attachment_view_drag_data_received() will not work because
	 * that function only handles the case where all the other drag
	 * handlers have failed. */
	e_attachment_paned_drag_data_received (
		E_ATTACHMENT_PANED (view),
		context, x, y, selection, info, time);

	/* Stop the signal from propagating to GtkHtml. */
	g_signal_stop_emission_by_name (widget, "drag-data-received");
}

static void
msg_composer_notify_header_cb (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
}

static gboolean
msg_composer_delete_event_cb (EMsgComposer *composer)
{
	EShell *shell;

	shell = e_msg_composer_get_shell (composer);

	if (g_list_length (e_shell_get_watched_windows (shell)) == 1) {
		/* This is the last watched window, use the quit
		 * mechanism to have a draft saved properly */
		e_shell_quit (shell, E_SHELL_QUIT_ACTION);
	} else {
		/* There are more watched windows opened,
		 * invoke only a close action */
		gtk_action_activate (ACTION (CLOSE));
	}

	return TRUE;
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
		gtk_action_activate (ACTION (SAVE_DRAFT));
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
		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value, e_msg_composer_get_focus_tracker (
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
msg_composer_dispose (GObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);

	e_composer_private_dispose (composer);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_msg_composer_parent_class)->dispose (object);
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
msg_composer_constructed (GObject *object)
{
	EShell *shell;
	EShellSettings *shell_settings;
	GtkhtmlEditor *editor;
	EMsgComposer *composer;
	EAttachmentView *view;
	EAttachmentStore *store;
	EComposerHeaderTable *table;
	GtkUIManager *ui_manager;
	GtkToggleAction *action;
	GtkHTML *html;
	GArray *array;
	const gchar *id;
	gboolean active;
	guint binding_id;

	editor = GTKHTML_EDITOR (object);
	composer = E_MSG_COMPOSER (object);

	shell = e_msg_composer_get_shell (composer);
	shell_settings = e_shell_get_shell_settings (shell);

	if (e_shell_get_express_mode (shell)) {
		GtkWindow *parent = e_shell_get_active_window (shell);
		gtk_window_set_transient_for (GTK_WINDOW (composer), parent);
	}

	e_composer_private_constructed (composer);

	html = gtkhtml_editor_get_html (editor);
	ui_manager = gtkhtml_editor_get_ui_manager (editor);
	view = e_msg_composer_get_attachment_view (composer);
	table = E_COMPOSER_HEADER_TABLE (composer->priv->header_table);

	gtk_window_set_title (GTK_WINDOW (composer), _("Compose Message"));
	gtk_window_set_icon_name (GTK_WINDOW (composer), "mail-message-new");

	g_signal_connect (
		object, "delete-event",
		G_CALLBACK (msg_composer_delete_event_cb), NULL);

	e_shell_adapt_window_size (shell, GTK_WINDOW (composer));
	e_shell_watch_window (shell, GTK_WINDOW (object));

	g_signal_connect (
		shell, "quit-requested",
		G_CALLBACK (msg_composer_quit_requested_cb), composer);

	g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (msg_composer_prepare_for_quit_cb), composer);

	/* Restore Persistent State */

	array = composer->priv->gconf_bridge_binding_ids;

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_CURRENT_FOLDER_KEY,
		G_OBJECT (composer), "current-folder");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_window (
		gconf_bridge_get (),
		COMPOSER_GCONF_WINDOW_PREFIX,
		GTK_WINDOW (composer), TRUE, FALSE);
	g_array_append_val (array, binding_id);

	/* Honor User Preferences */

	action = GTK_TOGGLE_ACTION (ACTION (REQUEST_READ_RECEIPT));
	active = e_shell_settings_get_boolean (
		shell_settings, "composer-request-receipt");
	gtk_toggle_action_set_active (action, active);

	/* Clipboard Support */

	g_signal_connect (
		html, "paste-clipboard",
		G_CALLBACK (msg_composer_paste_clipboard_cb), composer);

	/* Drag-and-Drop Support */

	g_signal_connect (
		html, "realize",
		G_CALLBACK (msg_composer_realize_gtkhtml_cb), composer);

	g_signal_connect (
		html, "drag-motion",
		G_CALLBACK (msg_composer_drag_motion_cb), composer);

	g_signal_connect (
		html, "drag-data-received",
		G_CALLBACK (msg_composer_drag_data_received_cb), composer);

	/* Configure Headers */

	e_composer_header_table_set_account_list (
		table, e_get_account_list ());
	e_composer_header_table_set_signature_list (
		table, e_get_signature_list ());

	g_signal_connect_swapped (
		table, "notify::account",
		G_CALLBACK (msg_composer_account_changed_cb), composer);
	g_signal_connect_swapped (
		table, "notify::destinations-bcc",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	g_signal_connect_swapped (
		table, "notify::destinations-cc",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	g_signal_connect_swapped (
		table, "notify::destinations-to",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	g_signal_connect_swapped (
		table, "notify::reply-to",
		G_CALLBACK (msg_composer_notify_header_cb), composer);
	g_signal_connect_swapped (
		table, "notify::signature",
		G_CALLBACK (e_msg_composer_show_sig_file), composer);
	g_signal_connect_swapped (
		table, "notify::subject",
		G_CALLBACK (msg_composer_subject_changed_cb), composer);
	g_signal_connect_swapped (
		table, "notify::subject",
		G_CALLBACK (msg_composer_notify_header_cb), composer);

	msg_composer_account_changed_cb (composer);

	/* Attachments */

	store = e_attachment_view_get_store (view);

	g_signal_connect_swapped (
		store, "row-deleted",
		G_CALLBACK (attachment_store_changed_cb), composer);

	g_signal_connect_swapped (
		store, "row-inserted",
		G_CALLBACK (attachment_store_changed_cb), composer);

	/* Initialization may have tripped the "changed" state. */
	gtkhtml_editor_set_changed (editor, FALSE);

	id = "org.gnome.evolution.composer";
	e_plugin_ui_register_manager (ui_manager, id, composer);
	e_plugin_ui_enable_manager (ui_manager, id);

	e_extensible_load_extensions (E_EXTENSIBLE (composer));
}

static void
msg_composer_destroy (GtkObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);
	EShell *shell;

	if (composer->priv->address_dialog != NULL) {
		gtk_widget_destroy (composer->priv->address_dialog);
		composer->priv->address_dialog = NULL;
	}

	/* FIXME Our EShell is already unreferenced. */
	shell = e_shell_get_default ();

	g_signal_handlers_disconnect_by_func (
		shell, msg_composer_quit_requested_cb, composer);
	g_signal_handlers_disconnect_by_func (
		shell, msg_composer_prepare_for_quit_cb, composer);

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (e_msg_composer_parent_class)->destroy (object);
}

static void
msg_composer_map (GtkWidget *widget)
{
	EComposerHeaderTable *table;
	GtkWidget *input_widget;
	const gchar *text;

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_msg_composer_parent_class)->map (widget);

	table = e_msg_composer_get_header_table (E_MSG_COMPOSER (widget));

	/* If the 'To' field is empty, focus it. */
	input_widget =
		e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_TO)->input_widget;
	text = gtk_entry_get_text (GTK_ENTRY (input_widget));
	if (text == NULL || *text == '\0') {
		gtk_widget_grab_focus (input_widget);
		return;
	}

	/* If not, check the 'Subject' field. */
	input_widget =
		e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_SUBJECT)->input_widget;
	text = gtk_entry_get_text (GTK_ENTRY (input_widget));
	if (text == NULL || *text == '\0') {
		gtk_widget_grab_focus (input_widget);
		return;
	}

	/* Jump to the editor as a last resort. */
	gtkhtml_editor_run_command (GTKHTML_EDITOR (widget), "grab-focus");
}

static gboolean
msg_composer_key_press_event (GtkWidget *widget,
                              GdkEventKey *event)
{
	EMsgComposer *composer = E_MSG_COMPOSER (widget);
	GtkWidget *input_widget;
	GtkhtmlEditor *editor;
	GtkHTML *html;

	editor = GTKHTML_EDITOR (widget);
	html = gtkhtml_editor_get_html (editor);

	input_widget =
		e_composer_header_table_get_header (
		e_msg_composer_get_header_table (composer),
		E_COMPOSER_HEADER_SUBJECT)->input_widget;

#ifdef HAVE_XFREE
	if (event->keyval == XF86XK_Send) {
		e_msg_composer_send (composer);
		return TRUE;
	}
#endif /* HAVE_XFREE */

	if (event->keyval == GDK_Escape) {
		gtk_action_activate (ACTION (CLOSE));
		return TRUE;
	}

	if (event->keyval == GDK_Tab && gtk_widget_is_focus (input_widget)) {
		gtkhtml_editor_run_command (editor, "grab-focus");
		return TRUE;
	}

	if (event->keyval == GDK_ISO_Left_Tab &&
		gtk_widget_is_focus (GTK_WIDGET (html))) {
		gtk_widget_grab_focus (input_widget);
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_msg_composer_parent_class)->
		key_press_event (widget, event);
}

static void
msg_composer_cut_clipboard (GtkhtmlEditor *editor)
{
	/* Do nothing.  EFocusTracker handles this. */
}

static void
msg_composer_copy_clipboard (GtkhtmlEditor *editor)
{
	/* Do nothing.  EFocusTracker handles this. */
}

static void
msg_composer_paste_clipboard (GtkhtmlEditor *editor)
{
	/* Do nothing.  EFocusTracker handles this. */
}

static void
msg_composer_select_all (GtkhtmlEditor *editor)
{
	/* Do nothing.  EFocusTracker handles this. */
}

static void
msg_composer_command_before (GtkhtmlEditor *editor,
                             const gchar *command)
{
	EMsgComposer *composer;
	const gchar *data;

	composer = E_MSG_COMPOSER (editor);

	if (strcmp (command, "insert-paragraph") != 0)
		return;

	if (composer->priv->in_signature_insert)
		return;

	data = gtkhtml_editor_get_paragraph_data (editor, "orig");
	if (data != NULL && *data == '1') {
		gtkhtml_editor_run_command (editor, "text-default-color");
		gtkhtml_editor_run_command (editor, "italic-off");
		return;
	};

	data = gtkhtml_editor_get_paragraph_data (editor, "signature");
	if (data != NULL && *data == '1') {
		gtkhtml_editor_run_command (editor, "text-default-color");
		gtkhtml_editor_run_command (editor, "italic-off");
	}
}

static void
msg_composer_command_after (GtkhtmlEditor *editor,
                            const gchar *command)
{
	EMsgComposer *composer;
	const gchar *data;

	composer = E_MSG_COMPOSER (editor);

	if (strcmp (command, "insert-paragraph") != 0)
		return;

	if (composer->priv->in_signature_insert)
		return;

	gtkhtml_editor_run_command (editor, "italic-off");

	data = gtkhtml_editor_get_paragraph_data (editor, "orig");
	if (data != NULL && *data == '1')
		e_msg_composer_reply_indent (composer);
	gtkhtml_editor_set_paragraph_data (editor, "orig", "0");

	data = gtkhtml_editor_get_paragraph_data (editor, "signature");
	if (data == NULL || *data != '1')
		return;

	/* Clear the signature. */
	if (gtkhtml_editor_is_paragraph_empty (editor))
		gtkhtml_editor_set_paragraph_data (editor, "signature" ,"0");

	else if (gtkhtml_editor_is_previous_paragraph_empty (editor) &&
		gtkhtml_editor_run_command (editor, "cursor-backward")) {

		gtkhtml_editor_set_paragraph_data (editor, "signature", "0");
		gtkhtml_editor_run_command (editor, "cursor-forward");
	}

	gtkhtml_editor_run_command (editor, "text-default-color");
	gtkhtml_editor_run_command (editor, "italic-off");
}

static gchar *
msg_composer_image_uri (GtkhtmlEditor *editor,
                        const gchar *uri)
{
	EMsgComposer *composer;
	GHashTable *hash_table;
	CamelMimePart *part;
	const gchar *cid;

	composer = E_MSG_COMPOSER (editor);

	hash_table = composer->priv->inline_images_by_url;
	part = g_hash_table_lookup (hash_table, uri);

	if (part == NULL && g_str_has_prefix (uri, "file:"))
		part = e_msg_composer_add_inline_image_from_file (
			composer, uri + 5);

	if (part == NULL && g_str_has_prefix (uri, "cid:")) {
		hash_table = composer->priv->inline_images;
		part = g_hash_table_lookup (hash_table, uri);
	}

	if (part == NULL)
		return NULL;

	composer->priv->current_images =
		g_list_prepend (composer->priv->current_images, part);

	cid = camel_mime_part_get_content_id (part);
	if (cid == NULL)
		return NULL;

	return g_strconcat ("cid:", cid, NULL);
}

static void
msg_composer_link_clicked (GtkhtmlEditor *editor,
                           const gchar *uri)
{
	if (uri == NULL || *uri == '\0')
		return;

	if (g_ascii_strncasecmp (uri, "mailto:", 7) == 0)
		return;

	if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0)
		return;

	if (g_ascii_strncasecmp (uri, "cid:", 4) == 0)
		return;

	e_show_uri (GTK_WINDOW (editor), uri);
}

static void
msg_composer_object_deleted (GtkhtmlEditor *editor)
{
	const gchar *data;

	if (!gtkhtml_editor_is_paragraph_empty (editor))
		return;

	data = gtkhtml_editor_get_paragraph_data (editor, "orig");
	if (data != NULL && *data == '1') {
		gtkhtml_editor_set_paragraph_data (editor, "orig", "0");
		gtkhtml_editor_run_command (editor, "indent-zero");
		gtkhtml_editor_run_command (editor, "style-normal");
		gtkhtml_editor_run_command (editor, "text-default-color");
		gtkhtml_editor_run_command (editor, "italic-off");
		gtkhtml_editor_run_command (editor, "insert-paragraph");
		gtkhtml_editor_run_command (editor, "delete-back");
	}

	data = gtkhtml_editor_get_paragraph_data (editor, "signature");
	if (data != NULL && *data == '1')
		gtkhtml_editor_set_paragraph_data (editor, "signature", "0");
}

static void
e_msg_composer_class_init (EMsgComposerClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	GtkWidgetClass *widget_class;
	GtkhtmlEditorClass *editor_class;

	g_type_class_add_private (class, sizeof (EMsgComposerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = msg_composer_set_property;
	object_class->get_property = msg_composer_get_property;
	object_class->dispose = msg_composer_dispose;
	object_class->finalize = msg_composer_finalize;
	object_class->constructed = msg_composer_constructed;

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = msg_composer_destroy;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = msg_composer_map;
	widget_class->key_press_event = msg_composer_key_press_event;

	editor_class = GTKHTML_EDITOR_CLASS (class);
	editor_class->cut_clipboard = msg_composer_cut_clipboard;
	editor_class->copy_clipboard = msg_composer_copy_clipboard;
	editor_class->paste_clipboard = msg_composer_paste_clipboard;
	editor_class->select_all = msg_composer_select_all;
	editor_class->command_before = msg_composer_command_before;
	editor_class->command_after = msg_composer_command_after;
	editor_class->image_uri = msg_composer_image_uri;
	editor_class->link_clicked = msg_composer_link_clicked;
	editor_class->object_deleted = msg_composer_object_deleted;

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
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			"Shell",
			"The EShell singleton",
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[SEND] = g_signal_new (
		"send",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SAVE_DRAFT] = g_signal_new (
		"save-draft",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[PRINT] = g_signal_new (
		"print",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__ENUM,
		G_TYPE_NONE, 1,
		GTK_TYPE_PRINT_OPERATION_ACTION);
}

static void
e_msg_composer_init (EMsgComposer *composer)
{
	composer->priv = E_MSG_COMPOSER_GET_PRIVATE (composer);
}

/* Callbacks.  */

/**
 * e_msg_composer_new:
 * @shell: an #EShell
 *
 * Create a new message composer widget.
 *
 * Returns: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		E_TYPE_MSG_COMPOSER,
		"html", e_web_view_new (), "shell", shell, NULL);
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
                                 gssize length)
{
	g_object_set_data_full (
		G_OBJECT (composer), "body:text",
		text, (GDestroyNotify) g_free);
}

static void
e_msg_composer_flush_pending_body (EMsgComposer *composer)
{
	const gchar *body;

	body = g_object_get_data (G_OBJECT (composer), "body:text");

	if (body != NULL)
		set_editor_text (composer, body, FALSE);

	g_object_set_data (G_OBJECT (composer), "body:text", NULL);
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

	if (!mime_part)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);
	wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (wrapper)) {
		/* another layer of multipartness... */
		add_attachments_from_multipart (
			composer, (CamelMultipart *) wrapper,
			just_inlines, depth + 1);
	} else if (just_inlines) {
		if (camel_mime_part_get_content_id (mime_part) ||
		    camel_mime_part_get_content_location (mime_part))
			e_msg_composer_add_inline_image_from_mime_part (
				composer, mime_part);
	} else if (related && camel_content_type_is (content_type, "image", "*")) {
		e_msg_composer_add_inline_image_from_mime_part (composer, mime_part);
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
		CAMEL_DATA_WRAPPER (multipart)->mime_type,
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

static void
handle_multipart_signed (EMsgComposer *composer,
                         CamelMultipart *multipart,
                         gint depth)
{
	CamelContentType *content_type;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	GtkToggleAction *action = NULL;
	const gchar *protocol;

	content = CAMEL_DATA_WRAPPER (multipart);
	content_type = camel_data_wrapper_get_mime_type_field (content);
	protocol = camel_content_type_param (content_type, "protocol");

	if (protocol == NULL)
		action = NULL;
	else if (g_ascii_strcasecmp (protocol, "application/pgp-signature") == 0)
		action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	else if (g_ascii_strcasecmp (protocol, "application/x-pkcs7-signature") == 0)
		action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));

	if (action)
		gtk_toggle_action_set_active (action, TRUE);

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
			handle_multipart_signed (composer, multipart, depth);

		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* Decrypt the encrypted content and configure
			 * the composer to encrypt outgoing messages. */
			handle_multipart_encrypted (composer, mime_part, depth);

		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* This contains the text/plain and text/html
			 * versions of the message body. */
			handle_multipart_alternative (
				composer, multipart, depth);

		} else {
			/* There must be attachments... */
			handle_multipart (composer, multipart, depth);
		}

	} else if (camel_content_type_is (content_type, "text", "*")) {
		gchar *html;
		gssize length;

		html = emcu_part_to_html (mime_part, &length, NULL);
		e_msg_composer_set_pending_body (composer, html, length);
	} else {
		e_msg_composer_attach (composer, mime_part);
	}
}

static void
handle_multipart_encrypted (EMsgComposer *composer,
                            CamelMimePart *multipart,
                            gint depth)
{
	CamelContentType *content_type;
	CamelCipherContext *cipher;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	CamelSession *session;
	CamelCipherValidity *valid;
	GtkToggleAction *action = NULL;
	const gchar *protocol;

	content_type = camel_mime_part_get_content_type (multipart);
	protocol = camel_content_type_param (content_type, "protocol");

	if (protocol && g_ascii_strcasecmp (protocol, "application/pgp-encrypted") == 0)
		action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	else if (content_type && (
		    camel_content_type_is (content_type, "application", "x-pkcs7-mime")
		 || camel_content_type_is (content_type, "application", "pkcs7-mime")))
		action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));

	if (action)
		gtk_toggle_action_set_active (action, TRUE);

	session = e_msg_composer_get_session (composer);
	cipher = camel_gpg_context_new (session);
	mime_part = camel_mime_part_new ();
	valid = camel_cipher_decrypt (cipher, multipart, mime_part, NULL);
	g_object_unref (cipher);

	if (valid == NULL)
		return;

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
				composer, content_multipart, depth);

		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* Decrypt the encrypted content and configure the
			 * composer to encrypt outgoing messages. */
			handle_multipart_encrypted (composer, mime_part, depth);

		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* This contains the text/plain and text/html
			 * versions of the message body. */
			handle_multipart_alternative (
				composer, content_multipart, depth);

		} else {
			/* There must be attachments... */
			handle_multipart (composer, content_multipart, depth);
		}

	} else if (camel_content_type_is (content_type, "text", "*")) {
		gchar *html;
		gssize length;

		html = emcu_part_to_html (mime_part, &length, NULL);
		e_msg_composer_set_pending_body (composer, html, length);
	} else {
		e_msg_composer_attach (composer, mime_part);
	}

	g_object_unref (mime_part);
}

static void
handle_multipart_alternative (EMsgComposer *composer,
                              CamelMultipart *multipart,
                              gint depth)
{
	/* Find the text/html part and set the composer body to it's contents */
	CamelMimePart *text_part = NULL;
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
				handle_multipart_signed (composer, mp, depth + 1);

			} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
				/* Decrypt the encrypted content and configure
				 * the composer to encrypt outgoing messages. */
				handle_multipart_encrypted (
					composer, mime_part, depth + 1);

			} else {
				/* Depth doesn't matter so long as we
				 * don't pass 0. */
				handle_multipart (composer, mp, depth + 1);
			}

		} else if (camel_content_type_is (content_type, "text", "html")) {
			/* text/html is preferable, so once we find it we're done... */
			text_part = mime_part;
			break;
		} else if (camel_content_type_is (content_type, "text", "*")) {
			/* anyt text part not text/html is second rate so the first
			   text part we find isn't necessarily the one we'll use. */
			if (!text_part)
				text_part = mime_part;
		} else {
			e_msg_composer_attach (composer, mime_part);
		}
	}

	if (text_part) {
		gchar *html;
		gssize length;

		html = emcu_part_to_html (text_part, &length, NULL);
		e_msg_composer_set_pending_body (composer, html, length);
	}
}

static void
handle_multipart (EMsgComposer *composer,
                  CamelMultipart *multipart,
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
				handle_multipart_signed (composer, mp, depth + 1);

			} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
				/* Decrypt the encrypted content and configure
				 * the composer to encrypt outgoing messages. */
				handle_multipart_encrypted (
					composer, mime_part, depth + 1);

			} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
				handle_multipart_alternative (
					composer, mp, depth + 1);

			} else {
				/* Depth doesn't matter so long as we
				 * don't pass 0. */
				handle_multipart (composer, mp, depth + 1);
			}

		} else if (depth == 0 && i == 0) {
			gchar *html;
			gssize length;

			/* Since the first part is not multipart/alternative,
			 * this must be the body. */
			html = emcu_part_to_html (mime_part, &length, NULL);
			e_msg_composer_set_pending_body (composer, html, length);
		} else if (camel_mime_part_get_content_id (mime_part) ||
			   camel_mime_part_get_content_location (mime_part)) {
			/* special in-line attachment */
			e_msg_composer_add_inline_image_from_mime_part (composer, mime_part);
		} else {
			/* normal attachment */
			e_msg_composer_attach (composer, mime_part);
		}
	}
}

static void
set_signature_gui (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	EComposerHeaderTable *table;
	ESignature *signature = NULL;
	const gchar *data;
	gchar *decoded;

	editor = GTKHTML_EDITOR (composer);
	table = e_msg_composer_get_header_table (composer);

	if (!gtkhtml_editor_search_by_data (editor, 1, "ClueFlow", "signature", "1"))
		return;

	data = gtkhtml_editor_get_paragraph_data (editor, "signature_name");
	if (g_str_has_prefix (data, "uid:")) {
		decoded = decode_signature_name (data + 4);
		signature = e_get_signature_by_uid (decoded);
		g_free (decoded);
	} else if (g_str_has_prefix (data, "name:")) {
		decoded = decode_signature_name (data + 5);
		signature = e_get_signature_by_name (decoded);
		g_free (decoded);
	}

	e_composer_header_table_set_signature (table, signature);
}

/**
 * e_msg_composer_new_with_message:
 * @shell: an #EShell
 * @message: The message to use as the source
 *
 * Create a new message composer widget.
 *
 * Note: Designed to work only for messages constructed using Evolution.
 *
 * Returns: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_with_message (EShell *shell,
                                 CamelMimeMessage *message)
{
	CamelInternetAddress *to, *cc, *bcc;
	GList *To = NULL, *Cc = NULL, *Bcc = NULL, *postto = NULL;
	const gchar *format, *subject;
	EDestination **Tov, **Ccv, **Bccv;
	GHashTable *auto_cc, *auto_bcc;
	CamelContentType *content_type;
	struct _camel_header_raw *headers;
	CamelDataWrapper *content;
	EAccount *account = NULL;
	gchar *account_name;
	EMsgComposer *composer;
	EMsgComposerPrivate *priv;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	struct _camel_header_raw *xev;
	gint len, i;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	for (headers = CAMEL_MIME_PART (message)->headers;headers;headers = headers->next) {
		if (!strcmp (headers->name, "X-Evolution-PostTo"))
			postto = g_list_append (postto, g_strstrip (g_strdup (headers->value)));
	}

	composer = e_msg_composer_new (shell);
	priv = E_MSG_COMPOSER_GET_PRIVATE (composer);
	table = e_msg_composer_get_header_table (composer);

	if (postto) {
		e_composer_header_table_set_post_to_list (table, postto);
		g_list_foreach (postto, (GFunc)g_free, NULL);
		g_list_free (postto);
		postto = NULL;
	}

	/* Restore the Account preference */
	account_name = (gchar *) camel_medium_get_header (
		CAMEL_MEDIUM (message), "X-Evolution-Account");
	if (account_name) {
		account_name = g_strdup (account_name);
		g_strstrip (account_name);

		account = e_get_account_by_uid (account_name);
		if (account == NULL)
			/* XXX Backwards compatibility */
			account = e_get_account_by_name (account_name);

		if (account != NULL) {
			g_free (account_name);
			account_name = g_strdup (account->name);
		}
	}

	if (postto == NULL) {
		auto_cc = g_hash_table_new_full (
			camel_strcase_hash, camel_strcase_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);

		auto_bcc = g_hash_table_new_full (
			camel_strcase_hash, camel_strcase_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) NULL);

		if (account) {
			CamelInternetAddress *iaddr;

			/* hash our auto-recipients for this account */
			if (account->always_cc) {
				iaddr = camel_internet_address_new ();
				if (camel_address_decode (CAMEL_ADDRESS (iaddr), account->cc_addrs) != -1) {
					for (i = 0; i < camel_address_length (CAMEL_ADDRESS (iaddr)); i++) {
						const gchar *name, *addr;

						if (!camel_internet_address_get (iaddr, i, &name, &addr))
							continue;

						g_hash_table_insert (auto_cc, g_strdup (addr), GINT_TO_POINTER (TRUE));
					}
				}
				g_object_unref (iaddr);
			}

			if (account->always_bcc) {
				iaddr = camel_internet_address_new ();
				if (camel_address_decode (CAMEL_ADDRESS (iaddr), account->bcc_addrs) != -1) {
					for (i = 0; i < camel_address_length (CAMEL_ADDRESS (iaddr)); i++) {
						const gchar *name, *addr;

						if (!camel_internet_address_get (iaddr, i, &name, &addr))
							continue;

						g_hash_table_insert (auto_bcc, g_strdup (addr), GINT_TO_POINTER (TRUE));
					}
				}
				g_object_unref (iaddr);
			}
		}

		to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
		cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
		bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);

		len = CAMEL_ADDRESS (to)->addresses->len;
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

		len = CAMEL_ADDRESS (cc)->addresses->len;
		for (i = 0; i < len; i++) {
			const gchar *name, *addr;

			if (camel_internet_address_get (cc, i, &name, &addr)) {
				EDestination *dest = e_destination_new ();
				e_destination_set_name (dest, name);
				e_destination_set_email (dest, addr);

				if (g_hash_table_lookup (auto_cc, addr))
					e_destination_set_auto_recipient (dest, TRUE);

				Cc = g_list_append (Cc, dest);
			}
		}

		Ccv = destination_list_to_vector (Cc);
		g_hash_table_destroy (auto_cc);
		g_list_free (Cc);

		len = CAMEL_ADDRESS (bcc)->addresses->len;
		for (i = 0; i < len; i++) {
			const gchar *name, *addr;

			if (camel_internet_address_get (bcc, i, &name, &addr)) {
				EDestination *dest = e_destination_new ();
				e_destination_set_name (dest, name);
				e_destination_set_email (dest, addr);

				if (g_hash_table_lookup (auto_bcc, addr))
					e_destination_set_auto_recipient (dest, TRUE);

				Bcc = g_list_append (Bcc, dest);
			}
		}

		Bccv = destination_list_to_vector (Bcc);
		g_hash_table_destroy (auto_bcc);
		g_list_free (Bcc);
	} else {
		Tov = NULL;
		Ccv = NULL;
		Bccv = NULL;
	}

	subject = camel_mime_message_get_subject (message);

	e_composer_header_table_set_account_name (table, account_name);
	e_composer_header_table_set_destinations_to (table, Tov);
	e_composer_header_table_set_destinations_cc (table, Ccv);
	e_composer_header_table_set_destinations_bcc (table, Bccv);
	e_composer_header_table_set_subject (table, subject);

	g_free (account_name);

	e_destination_freev (Tov);
	e_destination_freev (Ccv);
	e_destination_freev (Bccv);

	/* Restore the format editing preference */
	format = camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Format");
	if (format) {
		gchar **flags;

		while (*format && camel_mime_is_lwsp (*format))
			format++;

		flags = g_strsplit (format, ", ", 0);
		for (i=0;flags[i];i++) {
			if (g_ascii_strcasecmp (flags[i], "text/html") == 0)
				gtkhtml_editor_set_html_mode (
					GTKHTML_EDITOR (composer), TRUE);
			else if (g_ascii_strcasecmp (flags[i], "text/plain") == 0)
				gtkhtml_editor_set_html_mode (
					GTKHTML_EDITOR (composer), FALSE);
			else if (g_ascii_strcasecmp (flags[i], "pgp-sign") == 0) {
				action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
				gtk_toggle_action_set_active (action, TRUE);
			} else if (g_ascii_strcasecmp (flags[i], "pgp-encrypt") == 0) {
				action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
				gtk_toggle_action_set_active (action, TRUE);
			} else if (g_ascii_strcasecmp (flags[i], "smime-sign") == 0) {
				action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
				gtk_toggle_action_set_active (action, TRUE);
			} else if (g_ascii_strcasecmp (flags[i], "smime-encrypt") == 0) {
				action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
				gtk_toggle_action_set_active (action, TRUE);
			}
		}
		g_strfreev (flags);
	}

	/* Remove any other X-Evolution-* headers that may have been set */
	xev = emcu_remove_xevolution_headers (message);
	camel_header_raw_clear (&xev);

	/* Check for receipt request */
	if (camel_medium_get_header (CAMEL_MEDIUM (message), "Disposition-Notification-To")) {
		action = GTK_TOGGLE_ACTION (ACTION (REQUEST_READ_RECEIPT));
		gtk_toggle_action_set_active (action, TRUE);
	}

	/* Check for mail priority */
	if (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Priority")) {
		action = GTK_TOGGLE_ACTION (ACTION (PRIORITIZE_MESSAGE));
		gtk_toggle_action_set_active (action, TRUE);
	}

	/* set extra headers */
	headers = CAMEL_MIME_PART (message)->headers;
	while (headers) {
		if (g_ascii_strcasecmp (headers->name, "References") == 0 ||
		    g_ascii_strcasecmp (headers->name, "In-Reply-To") == 0) {
			g_ptr_array_add (
				composer->priv->extra_hdr_names,
				g_strdup (headers->name));
			g_ptr_array_add (
				composer->priv->extra_hdr_values,
				g_strdup (headers->value));
		}

		headers = headers->next;
	}

	/* Restore the attachments and body text */
	content = camel_medium_get_content (CAMEL_MEDIUM (message));
	if (CAMEL_IS_MULTIPART (content)) {
		CamelMultipart *multipart;

		multipart = CAMEL_MULTIPART (content);
		content_type = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* Handle the signed content and configure the
			 * composer to sign outgoing messages. */
			handle_multipart_signed (composer, multipart, 0);

		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* Decrypt the encrypted content and configure the
			 * composer to encrypt outgoing messages. */
			handle_multipart_encrypted (
				composer, CAMEL_MIME_PART (message), 0);

		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* This contains the text/plain and text/html
			 * versions of the message body. */
			handle_multipart_alternative (composer, multipart, 0);

		} else {
			/* There must be attachments... */
			handle_multipart (composer, multipart, 0);
		}
	} else {
		gchar *html;
		gssize length;

		content_type = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));

		if (content_type && (
		       camel_content_type_is (content_type, "application", "x-pkcs7-mime")
		    || camel_content_type_is (content_type, "application", "pkcs7-mime")))
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT)), TRUE);

		html = emcu_part_to_html ((CamelMimePart *)message, &length, NULL);
		e_msg_composer_set_pending_body (composer, html, length);
	}

	priv->is_from_message = TRUE;

	/* We wait until now to set the body text because we need to
	 * ensure that the attachment bar has all the attachments before
	 * we request them. */
	e_msg_composer_flush_pending_body (composer);

	set_signature_gui (composer);

	return composer;
}

static void
disable_editor (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	GtkAction *action;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	gtkhtml_editor_run_command (editor, "editable-off");

	action = GTKHTML_EDITOR_ACTION_EDIT_MENU (composer);
	gtk_action_set_sensitive (action, FALSE);

	action = GTKHTML_EDITOR_ACTION_FORMAT_MENU (composer);
	gtk_action_set_sensitive (action, FALSE);

	action = GTKHTML_EDITOR_ACTION_INSERT_MENU (composer);
	gtk_action_set_sensitive (action, FALSE);

	gtk_widget_set_sensitive (composer->priv->attachment_paned, FALSE);
}

/**
 * e_msg_composer_new_redirect:
 * @shell: an #EShell
 * @message: The message to use as the source
 *
 * Create a new message composer widget.
 *
 * Returns: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_redirect (EShell *shell,
                             CamelMimeMessage *message,
                             const gchar *resent_from)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	const gchar *subject;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	composer = e_msg_composer_new_with_message (shell, message);
	table = e_msg_composer_get_header_table (composer);

	subject = camel_mime_message_get_subject (message);

	composer->priv->redirect = message;
	g_object_ref (message);

	e_composer_header_table_set_account_name (table, resent_from);
	e_composer_header_table_set_subject (table, subject);

	disable_editor (composer);

	return composer;
}

/**
 * e_msg_composer_get_session:
 * @composer: an #EMsgComposer
 *
 * Returns the mail module's global #CamelSession instance.  Calling
 * this function will load the mail module if it isn't already loaded.
 *
 * Returns: the mail module's #CamelSession
 **/
CamelSession *
e_msg_composer_get_session (EMsgComposer *composer)
{
	EShell *shell;
	EShellSettings *shell_settings;
	CamelSession *session;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	shell = e_msg_composer_get_shell (composer);
	shell_settings = e_shell_get_shell_settings (shell);

	session = e_shell_settings_get_pointer (shell_settings, "mail-session");
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
 * e_msg_composer_send:
 * @composer: an #EMsgComposer
 *
 * Send the message in @composer.
 **/
void
e_msg_composer_send (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	g_signal_emit (composer, signals[SEND], 0);

	/* XXX This should be elsewhere. */
	gtkhtml_editor_set_changed (editor, FALSE);
}

/**
 * e_msg_composer_save_draft:
 * @composer: an #EMsgComposer
 *
 * Save the message in @composer to the selected account's Drafts folder.
 **/
void
e_msg_composer_save_draft (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	g_signal_emit (composer, signals[SAVE_DRAFT], 0);

	/* XXX This should be elsewhere. */
	gtkhtml_editor_set_changed (editor, FALSE);
}

/**
 * e_msg_composer_print:
 * @composer: an #EMsgComposer
 * @action: the print action to start
 *
 * Print the message in @composer.
 **/
void
e_msg_composer_print (EMsgComposer *composer,
                      GtkPrintOperationAction action)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	g_signal_emit (composer, signals[PRINT], 0, action);
}

static GList *
add_recipients (GList *list, const gchar *recips)
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

	return list;
}

static void
handle_mailto (EMsgComposer *composer, const gchar *mailto)
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
	gint len, clen;

	table = e_msg_composer_get_header_table (composer);
	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	buf = g_strdup (mailto);

	/* Parse recipients (everything after ':' until '?' or eos). */
	p = buf + 7;
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
					subject = g_locale_to_utf8 (content, clen, &nread,
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

				camel_url_decode (content);
				if (g_ascii_strncasecmp (content, "file:", 5) == 0)
					attachment = e_attachment_new_for_uri (content);
				else
					attachment = e_attachment_new_for_path (content);
				e_attachment_store_add_attachment (store, attachment);
				e_attachment_load_async (
					attachment, (GAsyncReadyCallback)
					e_attachment_load_handle_error, composer);
				g_object_unref (attachment);
			} else if (!g_ascii_strcasecmp (header, "from")) {
				/* Ignore */
			} else if (!g_ascii_strcasecmp (header, "reply-to")) {
				/* ignore */
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

	tov  = destination_list_to_vector (to);
	ccv  = destination_list_to_vector (cc);
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
		gchar *htmlbody;

		htmlbody = camel_text_to_html (body, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
		set_editor_text (composer, htmlbody, TRUE);
		g_free (htmlbody);
	}
}

/**
 * e_msg_composer_new_from_url:
 * @shell: an #EShell
 * @url: a mailto URL
 *
 * Create a new message composer widget, and fill in fields as
 * defined by the provided URL.
 **/
EMsgComposer *
e_msg_composer_new_from_url (EShell *shell,
                             const gchar *url)
{
	EMsgComposer *composer;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (g_ascii_strncasecmp (url, "mailto:", 7) == 0, NULL);

	composer = e_msg_composer_new (shell);

	handle_mailto (composer, url);

	return composer;
}

/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer,
                              const gchar *text,
                              gssize len)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (text != NULL);

	set_editor_text (composer, text, TRUE);
}

/**
 * e_msg_composer_set_body:
 * @composer: a composer object
 * @body: the data to initialize the composer with
 * @mime_type: the MIME type of data
 *
 * Loads the given data ginto the composer as the message body.
 * This function should only be used by the CORBA composer factory.
 **/
void
e_msg_composer_set_body (EMsgComposer *composer,
                         const gchar *body,
                         const gchar *mime_type)
{
	EMsgComposerPrivate *p = composer->priv;
	EComposerHeaderTable *table;
	gchar *buff;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	table = e_msg_composer_get_header_table (composer);

	buff = g_markup_printf_escaped ("<b>%s</b>",
		_("The composer contains a non-text "
		  "message body, which cannot be edited."));
	set_editor_text (composer, buff, FALSE);
	g_free (buff);
	gtkhtml_editor_set_html_mode (GTKHTML_EDITOR (composer), FALSE);
	disable_editor (composer);

	g_free (p->mime_body);
	p->mime_body = g_strdup (body);
	g_free (p->mime_type);
	p->mime_type = g_strdup (mime_type);

	if (g_ascii_strncasecmp (p->mime_type, "text/calendar", 13) == 0) {
		EAccount *account;

		account = e_composer_header_table_get_account (table);
		if (account && account->pgp_no_imip_sign) {
			GtkToggleAction *action;

			action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
			gtk_toggle_action_set_active (action, FALSE);

			action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
			gtk_toggle_action_set_active (action, FALSE);
		}
	}
}

/**
 * e_msg_composer_add_header:
 * @composer: a composer object
 * @name: the header name
 * @value: the header value
 *
 * Adds a header with @name and @value to the message. This header
 * may not be displayed by the composer, but will be included in
 * the message it outputs.
 **/
void
e_msg_composer_add_header (EMsgComposer *composer,
                           const gchar *name,
                           const gchar *value)
{
	EMsgComposerPrivate *p = composer->priv;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	g_ptr_array_add (p->extra_hdr_names, g_strdup (name));
	g_ptr_array_add (p->extra_hdr_values, g_strdup (value));
}

/**
 * e_msg_composer_modify_header :
 * @composer : a composer object
 * @name: the header name
 * @change_value: the header value to put in place of the previous
 *                value
 *
 * Searches for a header with name=@name ,if found it removes
 * that header and adds a new header with the @name and @change_value .
 * If not found then it creates a new header with @name and @change_value .
 **/
void
e_msg_composer_modify_header (EMsgComposer *composer,
                              const gchar *name,
                              const gchar *change_value)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (change_value != NULL);

	e_msg_composer_remove_header (composer, name);
	e_msg_composer_add_header (composer, name, change_value);
}

/**
 * e_msg_composer_modify_header :
 * @composer : a composer object
 * @name: the header name
 *
 * Searches for the header and if found it removes it .
 **/
void
e_msg_composer_remove_header (EMsgComposer *composer,
                              const gchar *name)
{
	EMsgComposerPrivate *p;
	gint i;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);

	p = composer->priv;

	for (i = 0; i < p->extra_hdr_names->len; i++) {
		if (strcmp (p->extra_hdr_names->pdata[i], name) == 0) {
			g_ptr_array_remove_index (p->extra_hdr_names, i);
			g_ptr_array_remove_index (p->extra_hdr_values, i);
		}
	}
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
 * e_msg_composer_add_inline_image_from_file:
 * @composer: a composer object
 * @filename: the name of the file containing the image
 *
 * This reads in the image in @filename and adds it to @composer
 * as an inline image, to be wrapped in a multipart/related.
 *
 * Returns: the newly-created CamelMimePart (which must be reffed
 * if the caller wants to keep its own reference), or %NULL on error.
 **/
CamelMimePart *
e_msg_composer_add_inline_image_from_file (EMsgComposer *composer,
                                           const gchar *filename)
{
	gchar *mime_type, *cid, *url, *name, *dec_file_name;
	CamelStream *stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part;
	EMsgComposerPrivate *p = composer->priv;

	dec_file_name = g_strdup (filename);
	camel_url_decode (dec_file_name);

	if (!g_file_test (dec_file_name, G_FILE_TEST_IS_REGULAR))
		return NULL;

	stream = camel_stream_fs_new_with_name (
		dec_file_name, O_RDONLY, 0, NULL);
	if (!stream)
		return NULL;

	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, stream, NULL);
	g_object_unref (CAMEL_OBJECT (stream));

	mime_type = e_util_guess_mime_type (dec_file_name, TRUE);
	if (mime_type == NULL)
		mime_type = g_strdup ("application/octet-stream");
	camel_data_wrapper_set_mime_type (wrapper, mime_type);
	g_free (mime_type);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), wrapper);
	g_object_unref (wrapper);

	cid = camel_header_msgid_generate ();
	camel_mime_part_set_content_id (part, cid);
	name = g_path_get_basename (dec_file_name);
	camel_mime_part_set_filename (part, name);
	g_free (name);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);

	url = g_strdup_printf ("file:%s", dec_file_name);
	g_hash_table_insert (p->inline_images_by_url, url, part);

	url = g_strdup_printf ("cid:%s", cid);
	g_hash_table_insert (p->inline_images, url, part);
	g_free (cid);

	g_free (dec_file_name);

	return part;
}

/**
 * e_msg_composer_add_inline_image_from_mime_part:
 * @composer: a composer object
 * @part: a CamelMimePart containing image data
 *
 * This adds the mime part @part to @composer as an inline image, to
 * be wrapped in a multipart/related.
 **/
void
e_msg_composer_add_inline_image_from_mime_part (EMsgComposer  *composer,
                                                CamelMimePart *part)
{
	gchar *url;
	const gchar *location, *cid;
	EMsgComposerPrivate *p = composer->priv;

	cid = camel_mime_part_get_content_id (part);
	if (!cid) {
		camel_mime_part_set_content_id (part, NULL);
		cid = camel_mime_part_get_content_id (part);
	}

	url = g_strdup_printf ("cid:%s", cid);
	g_hash_table_insert (p->inline_images, url, part);
	g_object_ref (part);

	location = camel_mime_part_get_content_location (part);
	if (location != NULL)
		g_hash_table_insert (
			p->inline_images_by_url,
			g_strdup (location), part);
}

/**
 * e_msg_composer_get_message:
 * @composer: A message composer widget
 *
 * Retrieve the message edited by the user as a CamelMimeMessage.  The
 * CamelMimeMessage object is created on the fly; subsequent calls to this
 * function will always create new objects from scratch.
 *
 * Returns: A pointer to the new CamelMimeMessage object
 **/
CamelMimeMessage *
e_msg_composer_get_message (EMsgComposer *composer,
                            gboolean save_html_object_data,
                            GError **error)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	GtkhtmlEditor *editor;
	gboolean html_content;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	if (e_attachment_store_get_num_loading (store) > 0) {
		if (!emcu_prompt_user (GTK_WINDOW (composer), NULL,
			"mail-composer:ask-send-message-pending-download", NULL)) {
			return NULL;
		}
	}

	editor = GTKHTML_EDITOR (composer);
	html_content = gtkhtml_editor_get_html_mode (editor);

	return build_message (
		composer, html_content,
		save_html_object_data, error);
}

static gchar *
msg_composer_get_message_print_helper (EMsgComposer *composer,
                                       gboolean html_content)
{
	GtkToggleAction *action;
	GString *string;

	string = g_string_sized_new (128);

	if (html_content)
		g_string_append (string, "text/html");
	else
		g_string_append (string, "text/plain");

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	if (gtk_toggle_action_get_active (action))
		g_string_append (string, ", pgp-sign");
	gtk_toggle_action_set_active (action, FALSE);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	if (gtk_toggle_action_get_active (action))
		g_string_append (string, ", pgp-encrypt");
	gtk_toggle_action_set_active (action, FALSE);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
	if (gtk_toggle_action_get_active (action))
		g_string_append (string, ", smime-sign");
	gtk_toggle_action_set_active (action, FALSE);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
	if (gtk_toggle_action_get_active (action))
		g_string_append (string, ", smime-encrypt");
	gtk_toggle_action_set_active (action, FALSE);

	return g_string_free (string, FALSE);
}

CamelMimeMessage *
e_msg_composer_get_message_print (EMsgComposer *composer,
                                  gboolean save_html_object_data)
{
	EShell *shell;
	GtkhtmlEditor *editor;
	EMsgComposer *temp_composer;
	CamelMimeMessage *msg;
	gboolean html_content;
	gchar *flags;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	editor = GTKHTML_EDITOR (composer);
	html_content = gtkhtml_editor_get_html_mode (editor);

	msg = build_message (
		composer, html_content, save_html_object_data, NULL);
	if (msg == NULL)
		return NULL;

	shell = e_msg_composer_get_shell (composer);
	temp_composer = e_msg_composer_new_with_message (shell, msg);
	g_object_unref (msg);

	/* Override composer flags. */
	flags = msg_composer_get_message_print_helper (
		temp_composer, html_content);

	msg = build_message (
		temp_composer, TRUE, save_html_object_data, NULL);
	if (msg != NULL)
		camel_medium_set_header (
			CAMEL_MEDIUM (msg), "X-Evolution-Format", flags);

	gtk_widget_destroy (GTK_WIDGET (temp_composer));
	g_free (flags);

	return msg;
}

CamelMimeMessage *
e_msg_composer_get_message_draft (EMsgComposer *composer,
                                  GError **error)
{
	GtkhtmlEditor *editor;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	CamelMimeMessage *msg;
	EAccount *account;
	gboolean html_content;
	gboolean pgp_encrypt;
	gboolean pgp_sign;
	gboolean smime_encrypt;
	gboolean smime_sign;
	GString *flags;

	editor = GTKHTML_EDITOR (composer);
	table = e_msg_composer_get_header_table (composer);
	html_content = gtkhtml_editor_get_html_mode (editor);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	pgp_sign = gtk_toggle_action_get_active (action);
	gtk_toggle_action_set_active (action, FALSE);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	pgp_encrypt = gtk_toggle_action_get_active (action);
	gtk_toggle_action_set_active (action, FALSE);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
	smime_sign = gtk_toggle_action_get_active (action);
	gtk_toggle_action_set_active (action, FALSE);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
	smime_encrypt = gtk_toggle_action_get_active (action);
	gtk_toggle_action_set_active (action, FALSE);

	msg = build_message (composer, TRUE, TRUE, error);
	if (msg == NULL)
		return NULL;

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	gtk_toggle_action_set_active (action, pgp_sign);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	gtk_toggle_action_set_active (action, pgp_encrypt);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
	gtk_toggle_action_set_active (action, smime_sign);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
	gtk_toggle_action_set_active (action, smime_encrypt);

	/* Attach account info to the draft. */
	account = e_composer_header_table_get_account (table);
	if (account && account->name)
		camel_medium_set_header (
			CAMEL_MEDIUM (msg),
			"X-Evolution-Account", account->uid);

	flags = g_string_new (html_content ? "text/html" : "text/plain");

	/* This should probably only save the setting if it is
	 * different from the from-account default? */
	if (pgp_sign)
		g_string_append (flags, ", pgp-sign");
	if (pgp_encrypt)
		g_string_append (flags, ", pgp-encrypt");
	if (smime_sign)
		g_string_append (flags, ", smime-sign");
	if (smime_encrypt)
		g_string_append (flags, ", smime-encrypt");

	camel_medium_set_header (
		CAMEL_MEDIUM (msg), "X-Evolution-Format", flags->str);
	g_string_free (flags, TRUE);

	return msg;
}

/**
 * e_msg_composer_show_sig:
 * @composer: A message composer widget
 *
 * Set a signature
 **/
void
e_msg_composer_show_sig_file (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	gchar *html_text;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	if (composer->priv->redirect)
		return;

	composer->priv->in_signature_insert = TRUE;

	gtkhtml_editor_freeze (editor);
	gtkhtml_editor_run_command (editor, "cursor-position-save");
	gtkhtml_editor_undo_begin (editor, "Set signature", "Reset signature");

	/* Delete the old signature. */
	gtkhtml_editor_run_command (editor, "block-selection");
	gtkhtml_editor_run_command (editor, "cursor-bod");
	if (gtkhtml_editor_search_by_data (editor, 1, "ClueFlow", "signature", "1")) {
		gtkhtml_editor_run_command (editor, "select-paragraph");
		gtkhtml_editor_run_command (editor, "delete");
		gtkhtml_editor_set_paragraph_data (editor, "signature", "0");
		gtkhtml_editor_run_command (editor, "delete-back");
	}
	gtkhtml_editor_run_command (editor, "unblock-selection");

	html_text = get_signature_html (composer);
	if (html_text) {
		gtkhtml_editor_run_command (editor, "insert-paragraph");
		if (!gtkhtml_editor_run_command (editor, "cursor-backward"))
			gtkhtml_editor_run_command (editor, "insert-paragraph");
		else
			gtkhtml_editor_run_command (editor, "cursor-forward");

		gtkhtml_editor_set_paragraph_data (editor, "orig", "0");
		gtkhtml_editor_run_command (editor, "indent-zero");
		gtkhtml_editor_run_command (editor, "style-normal");
		gtkhtml_editor_insert_html (editor, html_text);
		g_free (html_text);
	} else if (is_top_signature (composer)) {
		/* insert paragraph after the signature ClueFlow things */
		if (gtkhtml_editor_run_command (editor, "cursor-forward"))
			gtkhtml_editor_run_command (editor, "insert-paragraph");
	}

	gtkhtml_editor_undo_end (editor);
	gtkhtml_editor_run_command (editor, "cursor-position-restore");
	gtkhtml_editor_thaw (editor);

	composer->priv->in_signature_insert = FALSE;
}

CamelInternetAddress *
e_msg_composer_get_from (EMsgComposer *composer)
{
	CamelInternetAddress *address;
	EComposerHeaderTable *table;
	EAccount *account;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);

	account = e_composer_header_table_get_account (table);
	if (account == NULL)
		return NULL;

	address = camel_internet_address_new ();
	camel_internet_address_add (
		address, account->id->name, account->id->address);

	return address;
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
		g_object_unref (CAMEL_OBJECT (address));
		return NULL;
	}

	return address;
}

/**
 * e_msg_composer_get_raw_message_text:
 *
 * Returns the text/plain of the message from composer
 **/
GByteArray *
e_msg_composer_get_raw_message_text (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	GByteArray *array;
	gchar *text;
	gsize length;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	array = g_byte_array_new ();
	editor = GTKHTML_EDITOR (composer);
	text = gtkhtml_editor_get_text_plain (editor, &length);
	g_byte_array_append (array, (guint8 *) text, (guint) length);
	g_free (text);

	return array;
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
	GtkhtmlEditor *editor;
	EComposerHeaderTable *table;
	GdkWindow *window;
	GtkWidget *widget;
	const gchar *subject;
	gint response;

	editor = GTKHTML_EDITOR (composer);
	widget = GTK_WIDGET (composer);

	if (!gtkhtml_editor_get_changed (editor))
		return TRUE;

	window = gtk_widget_get_window (widget);
	gdk_window_raise (window);

	table = e_msg_composer_get_header_table (composer);
	subject = e_composer_header_table_get_subject (table);

	if (subject == NULL || *subject == '\0')
		subject = _("Untitled Message");

	response = e_alert_run_dialog_for_args (
		GTK_WINDOW (composer),
		"mail-composer:exit-unsaved",
		subject, NULL);

	switch (response) {
		case GTK_RESPONSE_YES:
			gtk_widget_hide (widget);
			e_msg_composer_request_close (composer);
			if (can_save_draft)
				gtk_action_activate (ACTION (SAVE_DRAFT));
			break;

		case GTK_RESPONSE_NO:
			res = TRUE;
			break;

		case GTK_RESPONSE_CANCEL:
			break;
	}

	return res;
}

EMsgComposer *
e_msg_composer_load_from_file (EShell *shell,
                               const gchar *filename)
{
	CamelStream *stream;
	CamelMimeMessage *message;
	EMsgComposer *composer;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	stream = camel_stream_fs_new_with_name (
		filename, O_RDONLY, 0, NULL);
	if (stream == NULL)
		return NULL;

	message = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (message), stream, NULL);
	g_object_unref (stream);

	composer = e_msg_composer_new_with_message (shell, message);
	if (composer != NULL)
		gtk_widget_show (GTK_WIDGET (composer));

	return composer;
}

void
e_msg_composer_set_alternative (EMsgComposer *composer,
                                gboolean alt)
{
	GtkhtmlEditor *editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	composer->priv->is_alternative = alt;
	gtkhtml_editor_set_html_mode (editor, !alt);
}

void
e_msg_composer_reply_indent (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);

	if (!gtkhtml_editor_is_paragraph_empty (editor)) {
		if (gtkhtml_editor_is_previous_paragraph_empty (editor))
			gtkhtml_editor_run_command (editor, "cursor-backward");
		else {
			gtkhtml_editor_run_command (editor, "text-default-color");
			gtkhtml_editor_run_command (editor, "italic-off");
			gtkhtml_editor_run_command (editor, "insert-paragraph");
			return;
		}
	}

	gtkhtml_editor_run_command (editor, "style-normal");
	gtkhtml_editor_run_command (editor, "indent-zero");
	gtkhtml_editor_run_command (editor, "text-default-color");
	gtkhtml_editor_run_command (editor, "italic-off");
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

GList *
e_load_spell_languages (void)
{
	GConfClient *client;
	GList *spell_languages = NULL;
	GSList *list;
	const gchar *key;
	GError *error = NULL;

	/* Ask GConf for a list of spell check language codes. */
	client = gconf_client_get_default ();
	key = COMPOSER_GCONF_SPELL_LANGUAGES_KEY;
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, &error);
	g_object_unref (client);

	/* Convert the codes to spell language structs. */
	while (list != NULL) {
		gchar *language_code = list->data;
		const GtkhtmlSpellLanguage *language;

		language = gtkhtml_spell_language_lookup (language_code);
		if (language != NULL)
			spell_languages = g_list_prepend (
				spell_languages, (gpointer) language);

		list = g_slist_delete_link (list, list);
		g_free (language_code);
	}

	spell_languages = g_list_reverse (spell_languages);

	/* Pick a default spell language if GConf came back empty. */
	if (spell_languages == NULL) {
		const GtkhtmlSpellLanguage *language;

		language = gtkhtml_spell_language_lookup (NULL);

		if (language) {
			spell_languages = g_list_prepend (
				spell_languages, (gpointer) language);

		/* Don't overwrite the stored spell check language
		 * codes if there was a problem retrieving them. */
			if (error == NULL)
				e_save_spell_languages (spell_languages);
		}
	}

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	return spell_languages;
}

void
e_save_spell_languages (GList *spell_languages)
{
	GConfClient *client;
	GSList *list = NULL;
	const gchar *key;
	GError *error = NULL;

	/* Build a list of spell check language codes. */
	while (spell_languages != NULL) {
		const GtkhtmlSpellLanguage *language;
		const gchar *language_code;

		language = spell_languages->data;
		language_code = gtkhtml_spell_language_get_code (language);
		list = g_slist_prepend (list, (gpointer) language_code);

		spell_languages = g_list_next (spell_languages);
	}

	list = g_slist_reverse (list);

	/* Save the language codes to GConf. */
	client = gconf_client_get_default ();
	key = COMPOSER_GCONF_SPELL_LANGUAGES_KEY;
	gconf_client_set_list (client, key, GCONF_VALUE_STRING, list, &error);
	g_object_unref (client);

	g_slist_free (list);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

void
e_msg_composer_set_mail_sent (EMsgComposer *composer,
                              gboolean mail_sent)
{
	g_return_if_fail (composer != NULL);

	composer->priv->mail_sent = mail_sent;
}

gboolean
e_msg_composer_get_mail_sent (EMsgComposer *composer)
{
	g_return_val_if_fail (composer != NULL, FALSE);

	return composer->priv->mail_sent;
}
