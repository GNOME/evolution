/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.c
 *
 * Copyright (C) 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Ettore Perazzoli (ettore@ximian.com)
 *   Jeffrey Stedfast (fejj@ximian.com)
 *   Miguel de Icaza  (miguel@ximian.com)
 *   Radek Doulik     (rodo@ximian.com)
 *
 */

/*

   TODO

   - Somehow users should be able to see if any file (s) are attached even when
     the attachment bar is not shown.

   Should use EventSources to keep track of global changes made to configuration
   values.  Right now it ignores the problem olympically. Miguel.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define SMIME_SUPPORTED 1

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs.h>

#include <glade/glade.h>

#include "e-util/e-dialog-utils.h"
#include "misc/e-charset-picker.h"
#include "misc/e-expander.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#include <mail/em-event.h>
#include "e-signature-combo-box.h"

#include <camel/camel-session.h>
#include <camel/camel-charset-map.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-multipart-signed.h>
#include <camel/camel-multipart-encrypted.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-cipher-context.h>
#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
#include <camel/camel-smime-context.h>
#endif

#include "mail/em-utils.h"
#include "mail/em-composer-utils.h"
#include "mail/mail-config.h"
#include "mail/mail-crypto.h"
#include "mail/mail-tools.h"
#include "mail/mail-ops.h"
#include "mail/mail-mt.h"
#include "mail/mail-session.h"
#include "mail/em-popup.h"
#include "mail/em-menu.h"

#include "e-msg-composer.h"
#include "e-attachment.h"
#include "e-attachment-bar.h"
#include "e-composer-autosave.h"
#include "e-composer-private.h"
#include "e-composer-header-table.h"

#include "evolution-shell-component-utils.h"
#include <e-util/e-icon-factory.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#define d(x)

#define E_MSG_COMPOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MSG_COMPOSER, EMsgComposerPrivate))

#define E_MSG_COMPOSER_VISIBLE_MASK_SENDER \
	(E_MSG_COMPOSER_VISIBLE_FROM | \
	 E_MSG_COMPOSER_VISIBLE_REPLYTO)

#define E_MSG_COMPOSER_VISIBLE_MASK_BASIC \
	(E_MSG_COMPOSER_VISIBLE_MASK_SENDER | \
	 E_MSG_COMPOSER_VISIBLE_SUBJECT)

#define E_MSG_COMPOSER_VISIBLE_MASK_RECIPIENTS \
	(E_MSG_COMPOSER_VISIBLE_TO | \
	 E_MSG_COMPOSER_VISIBLE_CC | \
	 E_MSG_COMPOSER_VISIBLE_BCC)

#define E_MSG_COMPOSER_VISIBLE_MASK_MAIL \
	(E_MSG_COMPOSER_VISIBLE_MASK_BASIC | \
	 E_MSG_COMPOSER_VISIBLE_MASK_RECIPIENTS)

#define E_MSG_COMPOSER_VISIBLE_MASK_POST \
	(E_MSG_COMPOSER_VISIBLE_MASK_BASIC | \
	 E_MSG_COMPOSER_VISIBLE_POSTTO)

typedef enum {
	E_MSG_COMPOSER_VISIBLE_FROM       = (1 << 0),
	E_MSG_COMPOSER_VISIBLE_REPLYTO    = (1 << 1),
	E_MSG_COMPOSER_VISIBLE_TO         = (1 << 2),
	E_MSG_COMPOSER_VISIBLE_CC         = (1 << 3),
	E_MSG_COMPOSER_VISIBLE_BCC        = (1 << 4),
	E_MSG_COMPOSER_VISIBLE_POSTTO     = (1 << 5),
	E_MSG_COMPOSER_VISIBLE_SUBJECT    = (1 << 7)
} EMsgComposerHeaderVisibleFlags;

enum {
	SEND,
	SAVE_DRAFT,
	LAST_SIGNAL
};

enum {
	DND_TYPE_MESSAGE_RFC822,
	DND_TYPE_X_UID_LIST,
	DND_TYPE_TEXT_URI_LIST,
	DND_TYPE_NETSCAPE_URL,
	DND_TYPE_TEXT_VCARD,
	DND_TYPE_TEXT_CALENDAR
};

static GtkTargetEntry drop_types[] = {
	{ "message/rfc822", 0, DND_TYPE_MESSAGE_RFC822 },
	{ "x-uid-list",     0, DND_TYPE_X_UID_LIST },
	{ "text/uri-list",  0, DND_TYPE_TEXT_URI_LIST },
	{ "_NETSCAPE_URL",  0, DND_TYPE_NETSCAPE_URL },
	{ "text/x-vcard",   0, DND_TYPE_TEXT_VCARD },
	{ "text/calendar",  0, DND_TYPE_TEXT_CALENDAR }
};

static struct {
	gchar *target;
	GdkAtom atom;
	guint32 actions;
} drag_info[] = {
	{ "message/rfc822", NULL, GDK_ACTION_COPY },
	{ "x-uid-list",     NULL, GDK_ACTION_ASK |
                                  GDK_ACTION_MOVE |
                                  GDK_ACTION_COPY },
	{ "text/uri-list",  NULL, GDK_ACTION_COPY },
	{ "_NETSCAPE_URL",  NULL, GDK_ACTION_COPY },
	{ "text/x-vcard",   NULL, GDK_ACTION_COPY },
	{ "text/calendar",  NULL, GDK_ACTION_COPY }
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

/* All the composer windows open, for bookkeeping purposes.  */
static GSList *all_composers = NULL;

/* local prototypes */
static GList *add_recipients (GList *list, const gchar *recips);

static void handle_mailto (EMsgComposer *composer, const gchar *mailto);
static void handle_uri    (EMsgComposer *composer, const gchar *uri, gboolean html_dnd);

/* used by e_msg_composer_add_message_attachments () */
static void add_attachments_from_multipart (EMsgComposer *composer, CamelMultipart *multipart,
					    gboolean just_inlines, gint depth);

/* used by e_msg_composer_new_with_message () */
static void handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, gint depth);
static void handle_multipart_alternative (EMsgComposer *composer, CamelMultipart *multipart, gint depth);
static void handle_multipart_encrypted (EMsgComposer *composer, CamelMimePart *multipart, gint depth);
static void handle_multipart_signed (EMsgComposer *composer, CamelMultipart *multipart, gint depth);

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

static CamelTransferEncoding
best_encoding (GByteArray *buf, const gchar *charset)
{
	gchar *in, *out, outbuf[256], *ch;
	gsize inlen, outlen;
	gint status, count = 0;
	iconv_t cd;

	if (!charset)
		return -1;

	cd = e_iconv_open (charset, "utf-8");
	if (cd == (iconv_t) -1)
		return -1;

	in = (gchar *) buf->data;
	inlen = buf->len;
	do {
		out = outbuf;
		outlen = sizeof (outbuf);
		status = e_iconv (cd, (const gchar **) &in, &inlen, &out, &outlen);
		for (ch = out - 1; ch >= outbuf; ch--) {
			if ((guchar) *ch > 127)
				count++;
		}
	} while (status == (gsize) -1 && errno == E2BIG);
	e_iconv_close (cd);

	if (status == (gsize) -1 || status > 0)
		return -1;

	if (count == 0)
		return CAMEL_TRANSFER_ENCODING_7BIT;
	else if (count <= buf->len * 0.17)
		return CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
	else
		return CAMEL_TRANSFER_ENCODING_BASE64;
}

static gchar *
best_charset (GByteArray *buf, const gchar *default_charset, CamelTransferEncoding *encoding)
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
	if (!(charset = (char *) camel_charset_best ((const gchar *)buf->data, buf->len))) {
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
 * composed in `composer'.
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

			camel_address_decode (CAMEL_ADDRESS (target), text_addr);
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

			camel_address_decode (CAMEL_ADDRESS (target), text_addr);
		}
	}

	for (i = 0; bcc_destv != NULL && bcc_destv[i] != NULL; ++i) {
		text_addr = e_destination_get_address (bcc_destv[i]);
		if (text_addr && *text_addr) {
			camel_address_decode (CAMEL_ADDRESS (bcc_addr), text_addr);
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

	camel_object_unref (to_addr);
	camel_object_unref (cc_addr);
	camel_object_unref (bcc_addr);
}

static void
build_message_headers (EMsgComposer *composer,
                       CamelMimeMessage *msg,
                       gboolean redirect)
{
	EComposerHeaderTable *table;
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

		camel_object_unref (addr);
	}

	/* Reply-To: */
	reply_to = e_composer_header_table_get_reply_to (table);
	if (reply_to != NULL && *reply_to != '\0') {
		CamelInternetAddress *addr;

		addr = camel_internet_address_new ();

		if (camel_address_unformat (CAMEL_ADDRESS (addr), reply_to) > 0)
			camel_mime_message_set_reply_to (msg, addr);

		camel_object_unref (addr);
	}

	/* To:, Cc:, Bcc: */
	if (e_composer_header_table_get_header_visible (table, E_COMPOSER_HEADER_TO) ||
	    e_composer_header_table_get_header_visible (table, E_COMPOSER_HEADER_CC) ||
	    e_composer_header_table_get_header_visible (table, E_COMPOSER_HEADER_BCC)) {
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
	if (e_composer_header_table_get_header_visible (table, E_COMPOSER_HEADER_POST_TO)) {
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

static CamelMimeMessage *
build_message (EMsgComposer *composer,
               gboolean html_content,
               gboolean save_html_object_data)
{
	GtkhtmlEditor *editor;
	EMsgComposerPrivate *p = composer->priv;

	EAttachmentBar *attachment_bar;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	CamelDataWrapper *plain, *html, *current;
	CamelTransferEncoding plain_encoding;
	const gchar *iconv_charset = NULL;
	GPtrArray *recipients = NULL;
	CamelMultipart *body = NULL;
	CamelContentType *type;
	CamelMimeMessage *new;
	CamelStream *stream;
	CamelMimePart *part;
	CamelException ex;
	GByteArray *data;
	EAccount *account;
	gchar *charset;
	gboolean pgp_sign;
	gboolean pgp_encrypt;
	gboolean smime_sign;
	gboolean smime_encrypt;
	gint i;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	editor = GTKHTML_EDITOR (composer);
	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);
	attachment_bar = E_ATTACHMENT_BAR (p->attachment_bar);

	/* evil kludgy hack for Redirect */
	if (p->redirect) {
		build_message_headers (composer, p->redirect, TRUE);
		camel_object_ref (p->redirect);
		return p->redirect;
	}

	new = camel_mime_message_new ();
	build_message_headers (composer, new, FALSE);
	for (i = 0; i < p->extra_hdr_names->len; i++) {
		camel_medium_add_header (CAMEL_MEDIUM (new),
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
		plain_encoding = CAMEL_TRANSFER_ENCODING_7BIT;
		for (i = 0; p->mime_body[i]; i++) {
			if ((guchar) p->mime_body[i] > 127) {
				plain_encoding = CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
				break;
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
			iconv_charset = e_iconv_charset_name (charset);
			g_free (charset);
		}
	}

	stream = camel_stream_mem_new_with_byte_array (data);

	/* convert the stream to the appropriate charset */
	if (iconv_charset && g_ascii_strcasecmp (iconv_charset, "UTF-8") != 0) {
		CamelStreamFilter *filter_stream;
		CamelMimeFilterCharset *filter;

		filter_stream = camel_stream_filter_new_with_stream (stream);
		camel_object_unref (stream);

		stream = (CamelStream *) filter_stream;
		filter = camel_mime_filter_charset_new_convert ("UTF-8", iconv_charset);
		camel_stream_filter_add (filter_stream, (CamelMimeFilter *) filter);
		camel_object_unref (filter);
	}

	/* construct the content object */
	plain = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (plain, stream);
	camel_object_unref (stream);

	camel_data_wrapper_set_mime_type_field (plain, type);
	camel_content_type_unref (type);

	if (html_content) {
		gchar *text;
		gsize length;

		clear_current_images (composer);

		if (save_html_object_data)
			gtkhtml_editor_run_command (editor, "save-data-on");

		data = g_byte_array_new ();
		text = gtkhtml_editor_get_text_html (editor, &length);
		g_byte_array_append (data, (guint8 *) text, (guint) length);
		g_free (text);

		if (save_html_object_data)
			gtkhtml_editor_run_command (editor, "save-data-off");

		html = camel_data_wrapper_new ();

		stream = camel_stream_mem_new_with_byte_array (data);
		camel_data_wrapper_construct_from_stream (html, stream);
		camel_object_unref (stream);
		camel_data_wrapper_set_mime_type (html, "text/html; charset=utf-8");

		/* Build the multipart/alternative */
		body = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
						  "multipart/alternative");
		camel_multipart_set_boundary (body, NULL);

		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), plain);
		camel_object_unref (plain);
		camel_mime_part_set_encoding (part, plain_encoding);
		camel_multipart_add_part (body, part);
		camel_object_unref (part);

		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), html);
		camel_object_unref (html);
		camel_multipart_add_part (body, part);
		camel_object_unref (part);

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
			camel_medium_set_content_object (CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (body));
			camel_object_unref (body);
			camel_multipart_add_part (html_with_images, part);
			camel_object_unref (part);

			add_inlined_images (composer, html_with_images);
			clear_current_images (composer);

			current = CAMEL_DATA_WRAPPER (html_with_images);
		} else
			current = CAMEL_DATA_WRAPPER (body);
	} else
		current = plain;

	if (e_attachment_bar_get_num_attachments (attachment_bar)) {
		CamelMultipart *multipart = camel_multipart_new ();

		if (p->is_alternative) {
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart),
							  "multipart/alternative");
		}

		/* Generate a random boundary. */
		camel_multipart_set_boundary (multipart, NULL);

		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		camel_object_unref (current);
		camel_multipart_add_part (multipart, part);
		camel_object_unref (part);

		e_attachment_bar_to_multipart (attachment_bar, multipart, p->charset);

		if (p->is_alternative) {
			for (i = camel_multipart_get_number (multipart); i > 1; i--) {
				part = camel_multipart_get_part (multipart, i - 1);
				camel_medium_remove_header (CAMEL_MEDIUM (part), "Content-Disposition");
			}
		}

		current = CAMEL_DATA_WRAPPER (multipart);
	}

	camel_exception_init (&ex);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	pgp_sign = gtk_toggle_action_get_active (action);

	action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	pgp_encrypt = gtk_toggle_action_get_active (action);

#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
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
		const gchar *types[] = { CAMEL_RECIPIENT_TYPE_TO, CAMEL_RECIPIENT_TYPE_CC, CAMEL_RECIPIENT_TYPE_BCC };

		recipients = g_ptr_array_new ();
		for (i = 0; i < G_N_ELEMENTS (types); i++) {
			const CamelInternetAddress *addr;
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
		EAccount *account;

		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		camel_object_unref (current);

		account = e_composer_header_table_get_account (table);

		if (account && account->pgp_key && *account->pgp_key) {
			pgp_userid = account->pgp_key;
		} else {
			from = e_msg_composer_get_from (composer);
			camel_internet_address_get (from, 0, NULL, &pgp_userid);
		}

		if (pgp_sign) {
			CamelMimePart *npart = camel_mime_part_new ();

			cipher = mail_crypto_get_pgp_cipher_context (account);
			camel_cipher_sign (cipher, pgp_userid, CAMEL_CIPHER_HASH_SHA1, part, npart, &ex);
			camel_object_unref (cipher);

			if (camel_exception_is_set (&ex)) {
				camel_object_unref (npart);
				goto exception;
			}

			camel_object_unref (part);
			part = npart;
		}

		if (pgp_encrypt) {
			CamelMimePart *npart = camel_mime_part_new ();

			/* check to see if we should encrypt to self, NB gets removed immediately after use */
			if (account && account->pgp_encrypt_to_self && pgp_userid)
				g_ptr_array_add (recipients, g_strdup (pgp_userid));

			cipher = mail_crypto_get_pgp_cipher_context (account);
			camel_cipher_encrypt (cipher, pgp_userid, recipients, part, npart, &ex);
			camel_object_unref (cipher);

			if (account && account->pgp_encrypt_to_self && pgp_userid)
				g_ptr_array_set_size (recipients, recipients->len - 1);

			if (camel_exception_is_set (&ex)) {
				camel_object_unref (npart);
				goto exception;
			}

			camel_object_unref (part);
			part = npart;
		}

		if (from)
			camel_object_unref (from);

		current = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		camel_object_ref (current);
		camel_object_unref (part);
	}

#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
	if (smime_sign || smime_encrypt) {
		CamelInternetAddress *from = NULL;
		CamelCipherContext *cipher;

		part = camel_mime_part_new ();
		camel_medium_set_content_object ((CamelMedium *)part, current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		camel_object_unref (current);

		if (smime_sign
		    && (account == NULL || account->smime_sign_key == NULL || account->smime_sign_key[0] == 0)) {
			camel_exception_set (&ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot sign outgoing message: No signing certificate set for this account"));
			goto exception;
		}

		if (smime_encrypt
		    && (account == NULL || account->smime_sign_key == NULL || account->smime_sign_key[0] == 0)) {
			camel_exception_set (&ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot encrypt outgoing message: No encryption certificate set for this account"));
			goto exception;
		}

		if (smime_sign) {
			CamelMimePart *npart = camel_mime_part_new ();

			cipher = camel_smime_context_new (session);

			/* if we're also encrypting, envelope-sign rather than clear-sign */
			if (smime_encrypt) {
				camel_smime_context_set_sign_mode ((CamelSMIMEContext *)cipher, CAMEL_SMIME_SIGN_ENVELOPED);
				camel_smime_context_set_encrypt_key ((CamelSMIMEContext *)cipher, TRUE, account->smime_encrypt_key);
			} else if (account && account->smime_encrypt_key && *account->smime_encrypt_key) {
				camel_smime_context_set_encrypt_key ((CamelSMIMEContext *)cipher, TRUE, account->smime_encrypt_key);
			}

			camel_cipher_sign (cipher, account->smime_sign_key, CAMEL_CIPHER_HASH_SHA1, part, npart, &ex);
			camel_object_unref (cipher);

			if (camel_exception_is_set (&ex)) {
				camel_object_unref (npart);
				goto exception;
			}

			camel_object_unref (part);
			part = npart;
		}

		if (smime_encrypt) {
			/* check to see if we should encrypt to self, NB removed after use */
			if (account->smime_encrypt_to_self)
				g_ptr_array_add (recipients, g_strdup (account->smime_encrypt_key));

			cipher = camel_smime_context_new (session);
			camel_smime_context_set_encrypt_key ((CamelSMIMEContext *)cipher, TRUE, account->smime_encrypt_key);

			camel_cipher_encrypt (cipher, NULL, recipients, part, (CamelMimePart *)new, &ex);
			camel_object_unref (cipher);

			if (camel_exception_is_set (&ex))
				goto exception;

			if (account->smime_encrypt_to_self)
				g_ptr_array_set_size (recipients, recipients->len - 1);
		}

		if (from)
			camel_object_unref (from);

		/* we replaced the message directly, we don't want to do reparenting foo */
		if (smime_encrypt) {
			camel_object_unref (part);
			goto skip_content;
		} else {
			current = camel_medium_get_content_object ((CamelMedium *)part);
			camel_object_ref (current);
			camel_object_unref (part);
		}
	}
#endif /* HAVE_NSS */

	camel_medium_set_content_object (CAMEL_MEDIUM (new), current);
	if (current == plain)
		camel_mime_part_set_encoding (CAMEL_MIME_PART (new), plain_encoding);
	camel_object_unref (current);

#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
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
		camel_object_unref (part);

	camel_object_unref (new);

	if (ex.id != CAMEL_EXCEPTION_USER_CANCEL) {
		e_error_run ((GtkWindow *)composer, "mail-composer:no-build-message",
			    camel_exception_get_description (&ex), NULL);
	}

	camel_exception_clear (&ex);

	if (recipients) {
		for (i=0; i<recipients->len; i++)
			g_free (recipients->pdata[i]);
		g_ptr_array_free (recipients, TRUE);
	}

	return NULL;
}

/* Attachment Bar */

static void
emcab_add (EPopup *ep, EPopupItem *item, gpointer data)
{
	GtkWidget *widget = data;
	GtkWidget *composer;

	composer = gtk_widget_get_toplevel (widget);
	gtk_action_activate (ACTION (ATTACH));
}

static void
emcab_properties (EPopup *ep, EPopupItem *item, gpointer data)
{
	EAttachmentBar *attachment_bar = data;

	e_attachment_bar_edit_selected (attachment_bar);
}

static void
emcab_remove (EPopup *ep, EPopupItem *item, gpointer data)
{
	EAttachmentBar *attachment_bar = data;

	e_attachment_bar_remove_selected (attachment_bar);
}

static void
emcab_popup_position (GtkMenu *menu, int *x, int *y, gboolean *push_in, gpointer user_data)
{
	GtkWidget *widget = user_data;
	GnomeIconList *icon_list = user_data;
	GList *selection;
	GnomeCanvasPixbuf *image;

	gdk_window_get_origin (widget->window, x, y);

	selection = gnome_icon_list_get_selection (icon_list);
	if (selection == NULL)
		return;

	image = gnome_icon_list_get_icon_pixbuf_item (
		icon_list, GPOINTER_TO_INT(selection->data));
	if (image == NULL)
		return;

	/* Put menu to the center of icon. */
	*x += (int)(image->item.x1 + image->item.x2) / 2;
	*y += (int)(image->item.y1 + image->item.y2) / 2;
}

static void
emcab_popups_free (EPopup *ep, GSList *list, gpointer data)
{
	g_slist_free (list);
}

/* Popup menu handling.  */
static EPopupItem emcab_popups[] = {
	{ E_POPUP_ITEM, "10.attach", N_("_Remove"), emcab_remove, NULL, GTK_STOCK_REMOVE, EM_POPUP_ATTACHMENTS_MANY },
	{ E_POPUP_ITEM, "20.attach", N_("_Properties"), emcab_properties, NULL, GTK_STOCK_PROPERTIES, EM_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_BAR, "30.attach.00", NULL, NULL, NULL, NULL, EM_POPUP_ATTACHMENTS_MANY|EM_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_ITEM, "30.attach.01", N_("_Add attachment..."), emcab_add, NULL, GTK_STOCK_ADD, 0 },
};

/* if id != -1, then use it as an index for target of the popup */

static void
emcab_popup (EAttachmentBar *bar, GdkEventButton *event, int id)
{
	GSList *attachments = NULL, *menus = NULL;
	int i;
	EMPopup *emp;
	EMPopupTargetAttachments *t;
	GtkMenu *menu;

	attachments = e_attachment_bar_get_attachment (bar, id);

	for (i=0;i<sizeof (emcab_popups)/sizeof (emcab_popups[0]);i++)
		menus = g_slist_prepend (menus, &emcab_popups[i]);

	/** @HookPoint-EMPopup: Composer Attachment Bar Context Menu
	 * @Id: org.gnome.evolution.mail.composer.attachmentbar.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: EMPopupTargetAttachments
	 *
	 * This is the context menu on the composer attachment bar.
	 */
	emp = em_popup_new ("org.gnome.evolution.mail.composer.attachmentbar.popup");
	e_popup_add_items ((EPopup *)emp, menus, NULL, emcab_popups_free, bar);
	t = em_popup_target_new_attachments (emp, attachments);
	t->target.widget = (GtkWidget *)bar;
	menu = e_popup_create_menu_once ((EPopup *)emp, (EPopupTarget *)t, 0);

	if (event == NULL)
		gtk_menu_popup (menu, NULL, NULL, emcab_popup_position, bar, 0, gtk_get_current_event_time ());
	else
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
}

/* Signatures */

static gchar *
get_file_content (EMsgComposer *composer,
                  const gchar *filename,
                  gboolean want_html,
                  guint flags,
                  gboolean warn)
{
	CamelStreamFilter *filtered_stream;
	CamelStreamMem *memstream;
	CamelMimeFilter *html, *charenc;
	CamelStream *stream;
	GByteArray *buffer;
	gchar *charset;
	gchar *content;
	gint fd;

	fd = g_open (filename, O_RDONLY, 0);
	if (fd == -1) {
		if (warn)
			e_error_run ((GtkWindow *)composer, "mail-composer:no-sig-file",
				    filename, g_strerror (errno), NULL);
		return g_strdup ("");
	}

	stream = camel_stream_fs_new_with_fd (fd);

	if (want_html) {
		filtered_stream = camel_stream_filter_new_with_stream (stream);
		camel_object_unref (stream);

		html = camel_mime_filter_tohtml_new (flags, 0);
		camel_stream_filter_add (filtered_stream, html);
		camel_object_unref (html);

		stream = (CamelStream *) filtered_stream;
	}

	memstream = (CamelStreamMem *) camel_stream_mem_new ();
	buffer = g_byte_array_new ();
	camel_stream_mem_set_byte_array (memstream, buffer);

	camel_stream_write_to_stream (stream, (CamelStream *) memstream);
	camel_object_unref (stream);

	/* The newer signature UI saves signatures in UTF-8, but we still need to check that
	   the signature is valid UTF-8 because it is possible that the user imported a
	   signature file that is in his/her locale charset. If it's not in UTF-8 and not in
	   the charset the composer is in (or their default mail charset) then fuck it,
	   there's nothing we can do. */
	if (buffer->len && !g_utf8_validate ((const gchar *)buffer->data, buffer->len, NULL)) {
		stream = (CamelStream *) memstream;
		memstream = (CamelStreamMem *) camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (memstream, g_byte_array_new ());

		filtered_stream = camel_stream_filter_new_with_stream (stream);
		camel_object_unref (stream);

		charset = composer && composer->priv->charset ? composer->priv->charset : NULL;
		charset = charset ? g_strdup (charset) : e_composer_get_default_charset ();
		if ((charenc = (CamelMimeFilter *) camel_mime_filter_charset_new_convert (charset, "UTF-8"))) {
			camel_stream_filter_add (filtered_stream, charenc);
			camel_object_unref (charenc);
		}

		g_free (charset);

		camel_stream_write_to_stream ((CamelStream *) filtered_stream, (CamelStream *) memstream);
		camel_object_unref (filtered_stream);
		g_byte_array_free (buffer, TRUE);

		buffer = memstream->buffer;
	}

	camel_object_unref (memstream);

	g_byte_array_append (buffer, (const guint8 *)"", 1);
	content = (char*)buffer->data;
	g_byte_array_free (buffer, FALSE);

	return content;
}

gchar *
e_msg_composer_get_sig_file_content (const gchar *sigfile, gboolean in_html)
{
	if (!sigfile || !*sigfile) {
		return NULL;
	}

	return get_file_content (NULL, sigfile, !in_html,
				 CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT |
				 CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				 CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
				 CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES,
				 FALSE);
}

static gchar *
encode_signature_name (const gchar *name)
{
	const gchar *s;
	gchar *ename, *e;
	gint len = 0;

	s = name;
	while (*s) {
		len ++;
		if (*s == '"' || *s == '.' || *s == '=')
			len ++;
		s ++;
	}

	ename = g_new (gchar, len + 1);

	s = name;
	e = ename;
	while (*s) {
		if (*s == '"') {
			*e = '.';
			e ++;
			*e = '1';
			e ++;
		} else if (*s == '=') {
			*e = '.';
			e ++;
			*e = '2';
			e ++;
		} else {
			*e = *s;
			e ++;
		}
		if (*s == '.') {
			*e = '.';
			e ++;
		}
		s ++;
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
		len ++;
		if (*s == '.') {
			s ++;
			if (!*s || !(*s == '.' || *s == '1' || *s == '2'))
				return NULL;
		}
		s ++;
	}

	dname = g_new (char, len + 1);

	s = name;
	d = dname;
	while (*s) {
		if (*s == '.') {
			s ++;
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
		d ++;
		s ++;
	}
	*d = 0;

	return dname;
}

#define CONVERT_SPACES CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES

static gchar *
get_signature_html (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gchar *text = NULL, *html = NULL;
	ESignature *signature;
	gboolean format_html;

	table = e_msg_composer_get_header_table (composer);
	signature = e_composer_header_table_get_signature (table);

	if (!signature)
		return NULL;

	if (!signature->autogen) {
		if (!signature->filename)
			return NULL;

		format_html = signature->html;

		if (signature->script) {
			text = mail_config_signature_run_script (signature->filename);
		} else {
			text = e_msg_composer_get_sig_file_content (signature->filename, format_html);
		}
	} else {
		EAccountIdentity *id;
		gchar *organization;
		gchar *address;
		gchar *name;

		id = e_composer_header_table_get_account (table)->id;
		address = id->address ? camel_text_to_html (id->address, CONVERT_SPACES, 0) : NULL;
		name = id->name ? camel_text_to_html (id->name, CONVERT_SPACES, 0) : NULL;
		organization = id->organization ? camel_text_to_html (id->organization, CONVERT_SPACES, 0) : NULL;

		text = g_strdup_printf ("-- <BR>%s%s%s%s%s%s%s%s",
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

		if (signature)
			encoded_uid = encode_signature_name (signature->uid);

		/* The signature dash convention ("-- \n") is specified in the
		 * "Son of RFC 1036": http://www.chemie.fu-berlin.de/outerspace/netnews/son-of-1036.html,
		 * section 4.3.2.
		 */
		html = g_strdup_printf ("<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature\" value=\"1\">-->"
					"<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature_name\" value=\"uid:%s\">-->"
					"<TABLE WIDTH=\"100%%\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>"
					"%s%s%s%s"
					"</TD></TR></TABLE>",
				        encoded_uid ? encoded_uid : "",
					format_html ? "" : "<PRE>\n",
					format_html || (!strncmp ("-- \n", text, 4) || strstr (text, "\n-- \n")) ? "" : "-- \n",
					text,
					format_html ? "" : "</PRE>\n");
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
	gboolean reply_signature_on_top;
	gchar *body = NULL, *html = NULL;
	GConfClient *gconf;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (text != NULL);

	gconf = gconf_client_get_default ();

	/*

	   Keeping Signatures in the beginning of composer
	   ------------------------------------------------

	   Purists are gonna blast me for this.
	   But there are so many people (read Outlook users) who want this.
	   And Evo is an exchange-client, Outlook-replacement etc.
	   So Here it goes :(

	   -- Sankar

	 */

	reply_signature_on_top = gconf_client_get_bool (gconf, COMPOSER_GCONF_TOP_SIGNATURE_KEY, NULL);

	g_object_unref (gconf);

	if (set_signature && reply_signature_on_top) {
		gchar *tmp = NULL;
		tmp = get_signature_html (composer);
		if (tmp) {
			/* Minimizing the damage. Make it just a part of the body instead of a signature */
			html = strstr (tmp, "-- \n");
			if (html) {
				/* That two consecutive - symbols followed by a space */
				*(html+1) = ' ';
				body = g_strdup_printf ("</br>%s</br>%s", tmp, text);
			} else {
				/* HTML Signature. Make it as part of body */
				body = g_strdup_printf ("</br>%s</br>%s", tmp, text);
			}
			g_free (tmp);
		} else {
			/* No signature set */
			body = g_strdup_printf ("<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature\" value=\"1\">-->"
					"<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature_name\" value=\"uid:Noname\">-->"
					"<TABLE WIDTH=\"100%%\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD> </TD></TR></TABLE>%s", text);
		}
	} else {
		body = g_strdup (text);
	}

	gtkhtml_editor_set_text_html (GTKHTML_EDITOR (composer), body, -1);

	if (set_signature && !reply_signature_on_top)
		e_msg_composer_show_sig_file (composer);
}

/* Commands.  */

static EMsgComposer *
autosave_load_draft (const gchar *filename)
{
	CamelStream *stream;
	CamelMimeMessage *msg;
	EMsgComposer *composer;

	g_return_val_if_fail (filename != NULL, NULL);

	g_warning ("autosave load filename = \"%s\"", filename);

	if (!(stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0)))
		return NULL;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream);
	camel_object_unref (stream);

	composer = e_msg_composer_new_with_message (msg);
	if (composer) {
		if (e_composer_autosave_snapshot (composer))
			g_unlink (filename);

		g_signal_connect (
			composer, "send",
			G_CALLBACK (em_utils_composer_send_cb), NULL);

		g_signal_connect (
			composer, "save-draft",
			G_CALLBACK (em_utils_composer_save_draft_cb), NULL);

		gtk_widget_show (GTK_WIDGET (composer));
	}

	return composer;
}

/* Miscellaneous callbacks.  */

static gint
attachment_bar_button_press_event_cb (EAttachmentBar *attachment_bar,
                                      GdkEventButton *event)
{
	GnomeIconList *icon_list;
	gint icon_number;

	if (event->button != 3)
		return FALSE;

	icon_list = GNOME_ICON_LIST (attachment_bar);
	icon_number = gnome_icon_list_get_icon_at (
		icon_list, event->x, event->y);
	if (icon_number >= 0) {
		gnome_icon_list_unselect_all (icon_list);
		gnome_icon_list_select_icon (icon_list, icon_number);
	}

	emcab_popup (attachment_bar, event, icon_number);

	return TRUE;
}

static void
attachment_bar_changed_cb (EAttachmentBar *attachment_bar,
                           EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	GtkWidget *widget;
	guint attachment_num;

	editor = GTKHTML_EDITOR (composer);
	attachment_num = e_attachment_bar_get_num_attachments (attachment_bar);

	if (attachment_num > 0) {
		gchar *markup;

		markup = g_strdup_printf (
			"<b>%d</b> %s", attachment_num, ngettext (
			"Attachment", "Attachments", attachment_num));
		widget = composer->priv->attachment_expander_num;
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		g_free (markup);

		gtk_widget_show (composer->priv->attachment_expander_icon);

		widget = composer->priv->attachment_expander;
		gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);
	} else {
		widget = composer->priv->attachment_expander_num;
		gtk_label_set_text (GTK_LABEL (widget), "");

		gtk_widget_hide (composer->priv->attachment_expander_icon);

		widget = composer->priv->attachment_expander;
		gtk_expander_set_expanded (GTK_EXPANDER (widget), FALSE);
	}

	/* Mark the editor as changed so it prompts about unsaved
	   changes on close. */
	gtkhtml_editor_set_changed (editor, TRUE);
}

static gint
attachment_bar_key_press_event_cb (EAttachmentBar *attachment_bar,
                                   GdkEventKey *event)
{
	if (event->keyval == GDK_Delete) {
		e_attachment_bar_remove_selected (attachment_bar);
		return TRUE;
	}

	return FALSE;
}

static gboolean
attachment_bar_popup_menu_cb (EAttachmentBar *attachment_bar)
{
	emcab_popup (attachment_bar, NULL, -1);

	return TRUE;
}

static void
attachment_expander_notify_cb (GtkExpander *expander,
                               GParamSpec *pspec,
                               EMsgComposer *composer)
{
	GtkLabel *label;
	const gchar *text;

	label = GTK_LABEL (composer->priv->attachment_expander_label);

	/* Update the expander label */
	if (gtk_expander_get_expanded (expander))
		text = _("Hide _Attachment Bar");
	else
		text = _("Show _Attachment Bar");

	gtk_label_set_text_with_mnemonic (label, text);
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

enum {
	UPDATE_AUTO_CC,
	UPDATE_AUTO_BCC,
};

static void
update_auto_recipients (EComposerHeaderTable *table,
                        gint mode,
                        const gchar *auto_addrs)
{
	EDestination *dest, **destv = NULL;
	CamelInternetAddress *iaddr;
	GList *list = NULL;
	guint length;
	gint i;

	if (auto_addrs) {
		iaddr = camel_internet_address_new ();
		if (camel_address_decode (CAMEL_ADDRESS (iaddr), auto_addrs) != -1) {
			for (i = 0; i < camel_address_length (CAMEL_ADDRESS (iaddr)); i++) {
				const gchar *name, *addr;

				if (!camel_internet_address_get (iaddr, i, &name, &addr))
					continue;

				dest = e_destination_new ();
				e_destination_set_auto_recipient (dest, TRUE);

				if (name)
					e_destination_set_name (dest, name);

				if (addr)
					e_destination_set_email (dest, addr);

				list = g_list_prepend (list, dest);
			}
		}

		camel_object_unref (iaddr);
	}

	switch (mode) {
	case UPDATE_AUTO_CC:
		destv = e_composer_header_table_get_destinations_cc (table);
		break;
	case UPDATE_AUTO_BCC:
		destv = e_composer_header_table_get_destinations_bcc (table);
		break;
	default:
		g_return_if_reached ();
	}

	if (destv) {
		for (i = 0; destv[i]; i++) {
			if (!e_destination_is_auto_recipient (destv[i])) {
				dest = e_destination_copy (destv[i]);
				list = g_list_prepend (list, dest);
			}
		}

		e_destination_freev (destv);
	}

	list = g_list_reverse (list);

	length = g_list_length (list);
	destv = destination_list_to_vector_sized (list, length);

	g_list_free (list);

	switch (mode) {
	case UPDATE_AUTO_CC:
		e_composer_header_table_set_destinations_cc (table, destv);
		break;
	case UPDATE_AUTO_BCC:
		e_composer_header_table_set_destinations_bcc (table, destv);
		break;
	default:
		g_return_if_reached ();
	}

	e_destination_freev (destv);
}

static void
msg_composer_account_changed_cb (EMsgComposer *composer)
{
	EMsgComposerPrivate *p = composer->priv;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	ESignature *signature;
	EAccount *account;
	gboolean active;
	const gchar *cc_addrs = NULL;
	const gchar *bcc_addrs = NULL;
	const gchar *uid;

	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);

	if (account == NULL)
		goto exit;

	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	active = account->pgp_always_sign &&
		(!account->pgp_no_imip_sign || p->mime_type == NULL ||
		g_ascii_strncasecmp (p->mime_type, "text/calendar", 13) != 0);
	gtk_toggle_action_set_active (action, active);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_SIGN));
	active = account->smime_sign_default;
	gtk_toggle_action_set_active (action, active);

	action = GTK_TOGGLE_ACTION (ACTION (SMIME_ENCRYPT));
	active = account->smime_encrypt_default;
	gtk_toggle_action_set_active (action, active);

	if (account->always_cc)
		cc_addrs = account->cc_addrs;
	if (account->always_bcc)
		bcc_addrs = account->bcc_addrs;

	uid = account->id->sig_uid;
	signature = uid ? mail_config_get_signature_by_uid (uid) : NULL;
	e_composer_header_table_set_signature (table, signature);

exit:
	update_auto_recipients (table, UPDATE_AUTO_CC, cc_addrs);
	update_auto_recipients (table, UPDATE_AUTO_BCC, bcc_addrs);

	e_msg_composer_show_sig_file (composer);
}

static void
msg_composer_attach_message (EMsgComposer *composer,
                             CamelMimeMessage *msg)
{
	CamelMimePart *mime_part;
	GString *description;
	const gchar *subject;
	EMsgComposerPrivate *p = composer->priv;

	mime_part = camel_mime_part_new ();
	camel_mime_part_set_disposition (mime_part, "inline");
	subject = camel_mime_message_get_subject (msg);

	description = g_string_new (_("Attached message"));
	if (subject != NULL)
		g_string_append_printf (description, " - %s", subject);
	camel_mime_part_set_description (mime_part, description->str);
	g_string_free (description, TRUE);

	camel_medium_set_content_object (
		(CamelMedium *) mime_part, (CamelDataWrapper *) msg);
	camel_mime_part_set_content_type (mime_part, "message/rfc822");

	e_attachment_bar_attach_mime_part (
		E_ATTACHMENT_BAR (p->attachment_bar), mime_part);

	camel_object_unref (mime_part);
}

static void
msg_composer_update_preferences (GConfClient *client,
                                 guint cnxn_id,
                                 GConfEntry *entry,
                                 EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	gboolean enable;
	GError *error = NULL;

	editor = GTKHTML_EDITOR (composer);

	enable = gconf_client_get_bool (
		client, COMPOSER_GCONF_INLINE_SPELLING_KEY, &error);
	if (error == NULL)
		gtkhtml_editor_set_inline_spelling (editor, enable);
	else {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	enable = gconf_client_get_bool (
		client, COMPOSER_GCONF_MAGIC_LINKS_KEY, &error);
	if (error == NULL)
		gtkhtml_editor_set_magic_links (editor, enable);
	else {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	enable = gconf_client_get_bool (
		client, COMPOSER_GCONF_MAGIC_SMILEYS_KEY, &error);
	if (error == NULL)
		gtkhtml_editor_set_magic_smileys (editor, enable);
	else {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}
}

struct _drop_data {
	EMsgComposer *composer;

	GdkDragContext *context;
	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	guint32 action;
	guint info;
	guint time;

	unsigned int move:1;
	unsigned int moved:1;
	unsigned int aborted:1;
};

int
e_msg_composer_get_remote_download_count (EMsgComposer *composer)
{
	EAttachmentBar *attachment_bar;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), 0);

	attachment_bar = E_ATTACHMENT_BAR (composer->priv->attachment_bar);
	return e_attachment_bar_get_download_count (attachment_bar);
}

static void
drop_action (EMsgComposer *composer,
             GdkDragContext *context,
             guint32 action,
             GtkSelectionData *selection,
             guint info,
             guint time,
             gboolean html_dnd)
{
	char *tmp, *str, **urls;
	CamelMimePart *mime_part;
	CamelStream *stream;
	CamelMimeMessage *msg;
	char *content_type;
	int i, success = FALSE, delete = FALSE;
	EMsgComposerPrivate *p = composer->priv;

	switch (info) {
	case DND_TYPE_MESSAGE_RFC822:
		d (printf ("dropping a message/rfc822\n"));
		/* write the message (s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, (const gchar *)selection->data, selection->length);
		camel_stream_reset (stream);

		msg = camel_mime_message_new ();
		if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *)msg, stream) != -1) {
			msg_composer_attach_message (composer, msg);
			success = TRUE;
			delete = action == GDK_ACTION_MOVE;
		}

		camel_object_unref (msg);
		camel_object_unref (stream);
		break;
	case DND_TYPE_NETSCAPE_URL:
		d (printf ("dropping a _NETSCAPE_URL\n"));
		tmp = g_strndup ((const gchar *) selection->data, selection->length);
		urls = g_strsplit (tmp, "\n", 2);
		g_free (tmp);

		/* _NETSCAPE_URL is represented as "URI\nTITLE" */
		handle_uri (composer, urls[0], html_dnd);

		g_strfreev (urls);
		success = TRUE;
		break;
	case DND_TYPE_TEXT_URI_LIST:
		d (printf ("dropping a text/uri-list\n"));
		tmp = g_strndup ((const gchar *) selection->data, selection->length);
		urls = g_strsplit (tmp, "\n", 0);
		g_free (tmp);

		for (i = 0; urls[i] != NULL; i++) {
			str = g_strstrip (urls[i]);
			if (str[0] == '#' || str[0] == '\0')
				continue;

			handle_uri (composer, str, html_dnd);
		}

		g_strfreev (urls);
		success = TRUE;
		break;
	case DND_TYPE_TEXT_VCARD:
	case DND_TYPE_TEXT_CALENDAR:
		content_type = gdk_atom_name (selection->type);
		d (printf ("dropping a %s\n", content_type));

		mime_part = camel_mime_part_new ();
		camel_mime_part_set_content (mime_part, (const gchar *)selection->data, selection->length, content_type);
		camel_mime_part_set_disposition (mime_part, "inline");

		e_attachment_bar_attach_mime_part (E_ATTACHMENT_BAR (p->attachment_bar), mime_part);

		camel_object_unref (mime_part);
		g_free (content_type);

		success = TRUE;
		break;
	case DND_TYPE_X_UID_LIST: {
		GPtrArray *uids;
		char *inptr, *inend;
		CamelFolder *folder;
		CamelException ex = CAMEL_EXCEPTION_INITIALISER;

		/* NB: This all runs synchronously, could be very slow/hang/block the ui */

		uids = g_ptr_array_new ();

		inptr = (char*)selection->data;
		inend = (char*)(selection->data + selection->length);
		while (inptr < inend) {
			char *start = inptr;

			while (inptr < inend && *inptr)
				inptr++;

			if (start > (char *)selection->data)
				g_ptr_array_add (uids, g_strndup (start, inptr-start));

			inptr++;
		}

		if (uids->len > 0) {
			folder = mail_tool_uri_to_folder ((const gchar *)selection->data, 0, &ex);
			if (folder) {
				if (uids->len == 1) {
					msg = camel_folder_get_message (folder, uids->pdata[0], &ex);
					if (msg == NULL)
						goto fail;
					msg_composer_attach_message (composer, msg);
				} else {
					CamelMultipart *mp = camel_multipart_new ();
					char *desc;

					camel_data_wrapper_set_mime_type ((CamelDataWrapper *)mp, "multipart/digest");
					camel_multipart_set_boundary (mp, NULL);
					for (i=0;i<uids->len;i++) {
						msg = camel_folder_get_message (folder, uids->pdata[i], &ex);
						if (msg) {
							mime_part = camel_mime_part_new ();
							camel_mime_part_set_disposition (mime_part, "inline");
							camel_medium_set_content_object ((CamelMedium *)mime_part, (CamelDataWrapper *)msg);
							camel_mime_part_set_content_type (mime_part, "message/rfc822");
							camel_multipart_add_part (mp, mime_part);
							camel_object_unref (mime_part);
							camel_object_unref (msg);
						} else {
							camel_object_unref (mp);
							goto fail;
						}
					}
					mime_part = camel_mime_part_new ();
					camel_medium_set_content_object ((CamelMedium *)mime_part, (CamelDataWrapper *)mp);
					/* translators, this count will always be >1 */
					desc = g_strdup_printf (ngettext ("Attached message", "%d attached messages", uids->len), uids->len);
					camel_mime_part_set_description (mime_part, desc);
					g_free (desc);
					e_attachment_bar_attach_mime_part (E_ATTACHMENT_BAR(p->attachment_bar), mime_part);
					camel_object_unref (mime_part);
					camel_object_unref (mp);
				}
				success = TRUE;
				delete = action == GDK_ACTION_MOVE;
			fail:
				if (camel_exception_is_set (&ex)) {
					char *name;

					camel_object_get (folder, NULL, CAMEL_FOLDER_NAME, &name, NULL);
					e_error_run ((GtkWindow *)composer, "mail-composer:attach-nomessages",
						    name?name:(char *)selection->data, camel_exception_get_description (&ex), NULL);
					camel_object_free (folder, CAMEL_FOLDER_NAME, name);
				}
				camel_object_unref (folder);
			} else {
				e_error_run ((GtkWindow *)composer, "mail-composer:attach-nomessages",
					    (const gchar*)selection->data, camel_exception_get_description (&ex), NULL);
			}

			camel_exception_clear (&ex);
		}

		g_ptr_array_free (uids, TRUE);

		break; }
	default:
		d (printf ("dropping an unknown\n"));
		break;
	}

	gtk_drag_finish (context, success, delete, time);
}

static void
drop_popup_copy (EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _drop_data *m = data;

	drop_action (
		m->composer, m->context, GDK_ACTION_COPY,
		m->selection, m->info, m->time, FALSE);
}

static void
drop_popup_move (EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _drop_data *m = data;

	drop_action (
		m->composer, m->context, GDK_ACTION_MOVE,
		m->selection, m->info, m->time, FALSE);
}

static void
drop_popup_cancel (EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _drop_data *m = data;

	gtk_drag_finish (m->context, FALSE, FALSE, m->time);
}

static EPopupItem drop_popup_menu[] = {
	{ E_POPUP_ITEM, "00.emc.02", N_("_Copy"), drop_popup_copy, NULL, "mail-copy", 0 },
	{ E_POPUP_ITEM, "00.emc.03", N_("_Move"), drop_popup_move, NULL, "mail-move", 0 },
	{ E_POPUP_BAR, "10.emc" },
	{ E_POPUP_ITEM, "99.emc.00", N_("Cancel _Drag"), drop_popup_cancel, NULL, NULL, 0 },
};

static void
drop_popup_free (EPopup *ep, GSList *items, gpointer data)
{
	struct _drop_data *m = data;

	g_slist_free (items);

	g_object_unref (m->context);
	g_object_unref (m->composer);
	g_free (m->selection->data);
	g_free (m->selection);
	g_free (m);
}

static void
msg_composer_notify_header_cb (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
}

static GObject *
msg_composer_constructor (GType type,
                          guint n_construct_properties,
                          GObjectConstructParam *construct_properties)
{
	GObject *object;
	EMsgComposer *composer;
	GtkToggleAction *action;
	GConfClient *client;
	GArray *array;
	gboolean active;
	guint binding_id;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	composer = E_MSG_COMPOSER (object);
	client = gconf_client_get_default ();
	array = composer->priv->gconf_bridge_binding_ids;

	/* Restore Persistent State */

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_CURRENT_FOLDER_KEY,
		G_OBJECT (composer), "current-folder");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_VIEW_BCC_KEY,
		G_OBJECT (ACTION (VIEW_BCC)), "active");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_VIEW_CC_KEY,
		G_OBJECT (ACTION (VIEW_CC)), "active");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_VIEW_FROM_KEY,
		G_OBJECT (ACTION (VIEW_FROM)), "active");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_VIEW_POST_TO_KEY,
		G_OBJECT (ACTION (VIEW_POST_TO)), "active");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_property (
		gconf_bridge_get (),
		COMPOSER_GCONF_VIEW_REPLY_TO_KEY,
		G_OBJECT (ACTION (VIEW_REPLY_TO)), "active");
	g_array_append_val (array, binding_id);

	binding_id = gconf_bridge_bind_window (
		gconf_bridge_get (),
		COMPOSER_GCONF_WINDOW_PREFIX,
		GTK_WINDOW (composer), TRUE, FALSE);
	g_array_append_val (array, binding_id);

	/* Honor User Preferences */

	active = gconf_client_get_bool (
		client, COMPOSER_GCONF_SEND_HTML_KEY, NULL);
	gtkhtml_editor_set_html_mode (GTKHTML_EDITOR (composer), active);

	action = GTK_TOGGLE_ACTION (ACTION (REQUEST_READ_RECEIPT));
	active = gconf_client_get_bool (
		client, COMPOSER_GCONF_REQUEST_RECEIPT_KEY, NULL);
	gtk_toggle_action_set_active (action, active);

	gconf_client_add_dir (
		client, COMPOSER_GCONF_PREFIX,
		GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	composer->priv->notify_id = gconf_client_notify_add (
		client, COMPOSER_GCONF_PREFIX, (GConfClientNotifyFunc)
		msg_composer_update_preferences, composer, NULL, NULL);
	msg_composer_update_preferences (client, 0, NULL, composer);

	g_object_unref (client);

	return object;
}

static void
msg_composer_dispose (GObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);

	e_composer_autosave_unregister (composer);
	e_composer_private_dispose (composer);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
msg_composer_finalize (GObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);

	e_composer_private_finalize (composer);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
msg_composer_destroy (GtkObject *object)
{
	EMsgComposer *composer = E_MSG_COMPOSER (object);

	all_composers = g_slist_remove (all_composers, object);

#if 0 /* GTKHTML-EDITOR */
	if (composer->priv->menu) {
		e_menu_update_target ((EMenu *)composer->priv->menu, NULL);
		g_object_unref (composer->priv->menu);
		composer->priv->menu = NULL;
	}
#endif

	if (composer->priv->address_dialog != NULL) {
		gtk_widget_destroy (composer->priv->address_dialog);
		composer->priv->address_dialog = NULL;
	}

	if (composer->priv->notify_id) {
		GConfClient *client;

		client = gconf_client_get_default ();
		gconf_client_notify_remove (client, composer->priv->notify_id);
		composer->priv->notify_id = 0;
		g_object_unref (client);
	}

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
msg_composer_map (GtkWidget *widget)
{
	EComposerHeaderTable *table;
	GtkWidget *input_widget;
	const gchar *text;

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (parent_class)->map (widget);

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

static gint
msg_composer_delete_event (GtkWidget *widget,
	                   GdkEventAny *event)
{
	/* This is needed for the ACTION macro. */
	EMsgComposer *composer = E_MSG_COMPOSER (widget);

	gtk_action_activate (ACTION (CLOSE));

	return TRUE;
}

static gboolean
msg_composer_key_press_event (GtkWidget *widget,
                              GdkEventKey *event)
{
	EMsgComposer *composer = E_MSG_COMPOSER (widget);
	GtkWidget *input_widget;

	input_widget =
		e_composer_header_table_get_header (
		e_msg_composer_get_header_table (composer),
		E_COMPOSER_HEADER_SUBJECT)->input_widget;

#ifdef HAVE_XFREE
	if (event->keyval == XF86XK_Send) {
		g_signal_emit (G_OBJECT (composer), signals[SEND], 0);
		return TRUE;
	}
#endif /* HAVE_XFREE */

	if (event->keyval == GDK_Escape) {
		gtk_action_activate (ACTION (CLOSE));
		return TRUE;
	}

	if (event->keyval == GDK_Tab && gtk_widget_is_focus (input_widget)) {
		gtkhtml_editor_run_command (
			GTKHTML_EDITOR (composer), "grab-focus");
		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}

static gboolean
msg_composer_drag_motion (GtkWidget *widget,
                          GdkDragContext *context,
                          gint x,
                          gint y,
                          guint time)
{
	GList *targets;
	GdkDragAction actions = 0;
	GdkDragAction chosen_action;

	targets = context->targets;
	while (targets != NULL) {
		gint ii;

		for (ii = 0; ii < G_N_ELEMENTS (drag_info); ii++)
			if (targets->data == (gpointer) drag_info[ii].atom)
				actions |= drag_info[ii].actions;

		targets = g_list_next (targets);
	}

	actions &= context->actions;
	chosen_action = context->suggested_action;

	/* we default to copy */
	if (chosen_action == GDK_ACTION_ASK &&
		(actions & (GDK_ACTION_MOVE|GDK_ACTION_COPY)) !=
		(GDK_ACTION_MOVE|GDK_ACTION_COPY))
		chosen_action = GDK_ACTION_COPY;

	gdk_drag_status (context, chosen_action, time);

	return (chosen_action != 0);
}

static void
msg_composer_drag_data_received (GtkWidget *widget,
                                 GdkDragContext *context,
                                 gint x,
                                 gint y,
                                 GtkSelectionData *selection,
                                 guint info,
                                 guint time)
{
	EMsgComposer *composer;

	/* Widget may be EMsgComposer or GtkHTML. */
	composer = E_MSG_COMPOSER (gtk_widget_get_toplevel (widget));

	if (selection->data == NULL)
		return;

	if (selection->length == -1)
		return;

	if (context->action == GDK_ACTION_ASK) {
		EMPopup *emp;
		GSList *menus = NULL;
		GtkMenu *menu;
		gint ii;
		struct _drop_data *m;

		m = g_malloc0(sizeof (*m));
		m->context = g_object_ref (context);
		m->composer = g_object_ref (composer);
		m->action = context->action;
		m->info = info;
		m->time = time;
		m->selection = g_malloc0(sizeof (*m->selection));
		m->selection->data = g_malloc (selection->length);
		memcpy (m->selection->data, selection->data, selection->length);
		m->selection->length = selection->length;

		emp = em_popup_new ("org.gnome.evolution.mail.composer.popup.drop");
		for (ii = 0; ii < G_N_ELEMENTS (drop_popup_menu); ii++)
			menus = g_slist_append (menus, &drop_popup_menu[ii]);

		e_popup_add_items ((EPopup *)emp, menus, NULL, drop_popup_free, m);
		menu = e_popup_create_menu_once ((EPopup *)emp, NULL, 0);
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, time);
	} else {
		drop_action (
			composer, context, context->action, selection,
			info, time, !GTK_WIDGET_TOPLEVEL (widget));
	}
}

static void
msg_composer_cut_clipboard (GtkhtmlEditor *editor)
{
	EMsgComposer *composer;
	GtkWidget *parent;
	GtkWidget *widget;

	composer = E_MSG_COMPOSER (editor);
	widget = gtk_window_get_focus (GTK_WINDOW (editor));
	parent = gtk_widget_get_parent (widget);

	if (parent == composer->priv->header_table) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
		return;
	}

	/* Chain up to parent's cut_clipboard() method. */
	GTKHTML_EDITOR_CLASS (parent_class)->cut_clipboard (editor);
}

static void
msg_composer_copy_clipboard (GtkhtmlEditor *editor)
{
	EMsgComposer *composer;
	GtkWidget *parent;
	GtkWidget *widget;

	composer = E_MSG_COMPOSER (editor);
	widget = gtk_window_get_focus (GTK_WINDOW (editor));
	parent = gtk_widget_get_parent (widget);

	if (parent == composer->priv->header_table) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
		return;
	}

	/* Chain up to parent's copy_clipboard() method. */
	GTKHTML_EDITOR_CLASS (parent_class)->copy_clipboard (editor);
}

static void
msg_composer_paste_clipboard (GtkhtmlEditor *editor)
{
	EMsgComposer *composer;
	GtkWidget *parent;
	GtkWidget *widget;

	composer = E_MSG_COMPOSER (editor);
	widget = gtk_window_get_focus (GTK_WINDOW (editor));
	parent = gtk_widget_get_parent (widget);

	if (parent == composer->priv->header_table) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
		return;
	}

	/* Chain up to parent's paste_clipboard() method. */
	GTKHTML_EDITOR_CLASS (parent_class)->paste_clipboard (editor);
}

static void
msg_composer_select_all (GtkhtmlEditor *editor)
{
	EMsgComposer *composer;
	GtkWidget *parent;
	GtkWidget *widget;

	composer = E_MSG_COMPOSER (editor);
	widget = gtk_window_get_focus (GTK_WINDOW (editor));
	parent = gtk_widget_get_parent (widget);

	if (parent == composer->priv->header_table) {
		gtk_editable_set_position (GTK_EDITABLE (widget), -1);
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
	} else
		/* Chain up to the parent's select_all() method. */
		GTKHTML_EDITOR_CLASS (parent_class)->select_all (editor);
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
	GError *error = NULL;

	if (uri == NULL || *uri == '\0')
		return;

	if (g_ascii_strncasecmp (uri, "mailto:", 7) == 0)
		return;

	if (g_ascii_strncasecmp (uri, "thismessage:", 12) == 0)
		return;

	if (g_ascii_strncasecmp (uri, "cid:", 4) == 0)
		return;

	gnome_url_show (uri, &error);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
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
msg_composer_uri_requested (GtkhtmlEditor *editor,
                            const gchar *uri,
                            GtkHTMLStream *stream)
{
	EMsgComposer *composer;
	GHashTable *hash_table;
	GByteArray *array;
	CamelDataWrapper *wrapper;
	CamelStream *camel_stream;
	CamelMimePart *part;
	GtkHTML *html;

	/* XXX It's unfortunate we have to expose GtkHTML structs here.
	 *     Maybe we could rework this to use a GOutputStream. */

	composer = E_MSG_COMPOSER (editor);
	html = gtkhtml_editor_get_html (editor);

	hash_table = composer->priv->inline_images_by_url;
	part = g_hash_table_lookup (hash_table, uri);

	if (part == NULL) {
		hash_table = composer->priv->inline_images;
		part = g_hash_table_lookup (hash_table, uri);
	}

	if (part == NULL) {
		gtk_html_end (html, stream, GTK_HTML_STREAM_ERROR);
		return;
	}

	array = g_byte_array_new ();
	camel_stream = camel_stream_mem_new_with_byte_array (array);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	camel_data_wrapper_decode_to_stream (wrapper, camel_stream);

	gtk_html_write (
		gtkhtml_editor_get_html (editor), stream,
		(gchar *) array->data, array->len);

	camel_object_unref (camel_stream);

	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);
}

static void
msg_composer_class_init (EMsgComposerClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	GtkWidgetClass *widget_class;
	GtkhtmlEditorClass *editor_class;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (drag_info); ii++)
		drag_info[ii].atom =
			gdk_atom_intern (drag_info[ii].target, FALSE);

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMsgComposerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = msg_composer_constructor;
	object_class->dispose = msg_composer_dispose;
	object_class->finalize = msg_composer_finalize;

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = msg_composer_destroy;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = msg_composer_map;
	widget_class->delete_event = msg_composer_delete_event;
	widget_class->key_press_event = msg_composer_key_press_event;
	widget_class->drag_motion = msg_composer_drag_motion;
	widget_class->drag_data_received = msg_composer_drag_data_received;

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
	editor_class->uri_requested = msg_composer_uri_requested;

	signals[SEND] = g_signal_new (
		"send",
		E_TYPE_MSG_COMPOSER,
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SAVE_DRAFT] = g_signal_new (
		"save-draft",
		E_TYPE_MSG_COMPOSER,
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);
}

static void
msg_composer_init (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
#if 0 /* GTKHTML-EDITOR */
	EMMenuTargetWidget *target;
#endif
	GtkHTML *html;

	composer->priv = E_MSG_COMPOSER_GET_PRIVATE (composer);

	e_composer_private_init (composer);

	all_composers = g_slist_prepend (all_composers, composer);
	html = gtkhtml_editor_get_html (GTKHTML_EDITOR (composer));
	table = E_COMPOSER_HEADER_TABLE (composer->priv->header_table);

	gtk_window_set_title (GTK_WINDOW (composer), _("Compose Message"));
	gtk_window_set_icon_name (GTK_WINDOW (composer), "mail-message-new");

	/* Drag-and-Drop Support */

	gtk_drag_dest_set (
		GTK_WIDGET (composer), GTK_DEST_DEFAULT_ALL,
		drop_types, G_N_ELEMENTS (drop_types),
		GDK_ACTION_COPY | GDK_ACTION_ASK | GDK_ACTION_MOVE);

	g_signal_connect (
		html, "drag-data-received",
		G_CALLBACK (msg_composer_drag_data_received), NULL);

	/* Plugin Support */

#if 0 /* GTKHTML-EDITOR */
	/** @HookPoint-EMMenu: Main Mail Menu
	 * @Id: org.gnome.evolution.mail.composer
	 * @Class: org.gnome.evolution.mail.bonobomenu:1.0
	 * @Target: EMMenuTargetWidget
	 *
	 * The main menu of the composer window.  The widget of the
	 * target will point to the EMsgComposer object.
	 */
	composer->priv->menu = em_menu_new ("org.gnome.evolution.mail.composer");
	target = em_menu_target_new_widget (p->menu, (GtkWidget *)composer);
	e_menu_update_target ((EMenu *)p->menu, target);
	e_menu_activate ((EMenu *)p->menu, p->uic, TRUE);

#endif

	/* Configure Headers */

	e_composer_header_table_set_account_list (
		table, mail_config_get_accounts ());
	e_composer_header_table_set_signature_list (
		table, mail_config_get_signatures ());

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

	/* Attachment Bar */

	g_signal_connect (
		composer->priv->attachment_bar, "button_press_event",
		G_CALLBACK (attachment_bar_button_press_event_cb), NULL);
	g_signal_connect (
		composer->priv->attachment_bar, "key_press_event",
		G_CALLBACK (attachment_bar_key_press_event_cb), NULL);
	g_signal_connect (
		composer->priv->attachment_bar, "popup-menu",
		G_CALLBACK (attachment_bar_popup_menu_cb), NULL);
	g_signal_connect (
		composer->priv->attachment_bar, "changed",
		G_CALLBACK (attachment_bar_changed_cb), composer);
	g_signal_connect_after (
		composer->priv->attachment_expander, "notify::expanded",
		G_CALLBACK (attachment_expander_notify_cb), composer);

	e_composer_autosave_register (composer);

	/* Initialization may have tripped the "changed" state. */
	gtkhtml_editor_set_changed (GTKHTML_EDITOR (composer), FALSE);
}

GType
e_msg_composer_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMsgComposerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) msg_composer_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMsgComposer),
			0,     /* n_preallocs */
			(GInstanceInitFunc) msg_composer_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTKHTML_TYPE_EDITOR, "EMsgComposer", &type_info, 0);
	}

	return type;
}

/* Callbacks.  */

static EMsgComposer *
create_composer (gint visible_mask)
{
	EMsgComposer *composer;
	EMsgComposerPrivate *p;
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	gboolean active;

	composer = g_object_new (E_TYPE_MSG_COMPOSER, NULL);
	table = E_COMPOSER_HEADER_TABLE (composer->priv->header_table);
	p = composer->priv;

	/* Configure View Menu */

	/* If we're mailing, you cannot disable "To". */
	action = GTK_TOGGLE_ACTION (ACTION (VIEW_TO));
	active = visible_mask & E_MSG_COMPOSER_VISIBLE_TO;
	gtk_action_set_sensitive (ACTION (VIEW_TO), active);
	gtk_toggle_action_set_active (action, active);

	/* Ditto for "Post-To". */
	action = GTK_TOGGLE_ACTION (ACTION (VIEW_POST_TO));
	active = visible_mask & E_MSG_COMPOSER_VISIBLE_POSTTO;
	gtk_action_set_sensitive (ACTION (VIEW_POST_TO), active);
	gtk_toggle_action_set_active (action, active);

	/* Disable "Cc" if we're posting. */
	if (!(visible_mask & E_MSG_COMPOSER_VISIBLE_CC)) {
		action = GTK_TOGGLE_ACTION (ACTION (VIEW_CC));
		gtk_toggle_action_set_active (action, FALSE);
	}

	/* Disable "Bcc" if we're posting. */
	if (!(visible_mask & E_MSG_COMPOSER_VISIBLE_BCC)) {
		action = GTK_TOGGLE_ACTION (ACTION (VIEW_BCC));
		gtk_toggle_action_set_active (action, FALSE);
	}

	action = GTK_TOGGLE_ACTION (ACTION (VIEW_SUBJECT));
	gtk_toggle_action_set_active (action, TRUE);

	return composer;
}

/**
 * e_msg_composer_new_with_type:
 *
 * Create a new message composer widget. The type can be
 * E_MSG_COMPOSER_MAIL, E_MSG_COMPOSER_POST or E_MSG_COMPOSER_MAIL_POST.
 *
 * Returns: A pointer to the newly created widget
 **/

EMsgComposer *
e_msg_composer_new_with_type (int type)
{
	EMsgComposer *composer;
	gint visible_mask;

	switch (type) {
		case E_MSG_COMPOSER_MAIL:
			visible_mask = E_MSG_COMPOSER_VISIBLE_MASK_MAIL;
			break;

		case E_MSG_COMPOSER_POST:
			visible_mask = E_MSG_COMPOSER_VISIBLE_MASK_POST;
			break;

		default:
			visible_mask =
				E_MSG_COMPOSER_VISIBLE_MASK_MAIL |
				E_MSG_COMPOSER_VISIBLE_MASK_POST;
			break;
	}

	composer = create_composer (visible_mask);

	set_editor_text (composer, "", TRUE);

	return composer;
}

/**
 * e_msg_composer_new:
 *
 * Create a new message composer widget.
 *
 * Returns: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new (void)
{
	return e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL);
}

static gboolean
is_special_header (const gchar *hdr_name)
{
	/* Note: a header is a "special header" if it has any meaning:
	   1. it's not a X-* header or
	   2. it's an X-Evolution* header
	*/
	if (g_ascii_strncasecmp (hdr_name, "X-", 2))
		return TRUE;

	if (!g_ascii_strncasecmp (hdr_name, "X-Evolution", 11))
		return TRUE;

	/* we can keep all other X-* headers */

	return FALSE;
}

static void
e_msg_composer_set_pending_body (EMsgComposer *composer,
                                 gchar *text,
                                 gssize length)
{
	g_object_set_data_full (
		G_OBJECT (composer), "body:text",
		text, (GDestroyNotify) g_free);

	g_object_set_data (
		G_OBJECT (composer), "body:length",
		GSIZE_TO_POINTER (length));
}

static void
e_msg_composer_flush_pending_body (EMsgComposer *composer)
{
	const gchar *body;
	gpointer data;
	gssize length;

	body = g_object_get_data (G_OBJECT (composer), "body:text");
	data = g_object_get_data (G_OBJECT (composer), "body:length");
	length = GPOINTER_TO_SIZE (data);

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
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

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
	} else if (CAMEL_IS_MIME_MESSAGE (wrapper)) {
		/* do nothing */
	} else if (related && camel_content_type_is (content_type, "image", "*")) {
		e_msg_composer_add_inline_image_from_mime_part (composer, mime_part);
	} else if (camel_content_type_is (content_type, "text", "*")) {
		/* do nothing */
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

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
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
	GtkToggleAction *action;

	/* FIXME: make sure this isn't an s/mime signed part?? */
	action = GTK_TOGGLE_ACTION (ACTION (PGP_SIGN));
	gtk_toggle_action_set_active (action, TRUE);

	mime_part = camel_multipart_get_part (
		multipart, CAMEL_MULTIPART_SIGNED_CONTENT);

	if (mime_part != NULL)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);

	content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (content)) {
		multipart = CAMEL_MULTIPART (content);

		/* Note: depth is preserved here because we're not
		   counting multipart/signed as a multipart, instead
		   we want to treat the content part as our mime part
		   here. */

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* handle the signed content and configure the composer to sign outgoing messages */
			handle_multipart_signed (composer, multipart, depth);
		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
			handle_multipart_encrypted (composer, mime_part, depth);
		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* this contains the text/plain and text/html versions of the message body */
			handle_multipart_alternative (composer, multipart, depth);
		} else {
			/* there must be attachments... */
			handle_multipart (composer, multipart, depth);
		}
	} else if (camel_content_type_is (content_type, "text", "*")) {
		gchar *html;
		gssize length;

		html = em_utils_part_to_html (mime_part, &length, NULL);
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
	CamelException ex;
	CamelCipherValidity *valid;
	GtkToggleAction *action;

	/* FIXME: make sure this is a PGP/MIME encrypted part?? */
	action = GTK_TOGGLE_ACTION (ACTION (PGP_ENCRYPT));
	gtk_toggle_action_set_active (action, TRUE);

	camel_exception_init (&ex);
	cipher = mail_crypto_get_pgp_cipher_context (NULL);
	mime_part = camel_mime_part_new ();
	valid = camel_cipher_decrypt (cipher, multipart, mime_part, &ex);
	camel_object_unref (cipher);
	camel_exception_clear (&ex);
	if (valid == NULL)
		return;
	camel_cipher_validity_free (valid);

	content_type = camel_mime_part_get_content_type (mime_part);

	content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (content)) {
		CamelMultipart *content_multipart = CAMEL_MULTIPART (content);

		/* Note: depth is preserved here because we're not
		   counting multipart/encrypted as a multipart, instead
		   we want to treat the content part as our mime part
		   here. */

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* handle the signed content and configure the composer to sign outgoing messages */
			handle_multipart_signed (composer, content_multipart, depth);
		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
			handle_multipart_encrypted (composer, mime_part, depth);
		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* this contains the text/plain and text/html versions of the message body */
			handle_multipart_alternative (composer, content_multipart, depth);
		} else {
			/* there must be attachments... */
			handle_multipart (composer, content_multipart, depth);
		}
	} else if (camel_content_type_is (content_type, "text", "*")) {
		gchar *html;
		gssize length;

		html = em_utils_part_to_html (mime_part, &length, NULL);
		e_msg_composer_set_pending_body (composer, html, length);
	} else {
		e_msg_composer_attach (composer, mime_part);
	}

	camel_object_unref (mime_part);
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
		content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

		if (CAMEL_IS_MULTIPART (content)) {
			CamelMultipart *mp;

			mp = CAMEL_MULTIPART (content);

			if (CAMEL_IS_MULTIPART_SIGNED (content)) {
				/* handle the signed content and configure the composer to sign outgoing messages */
				handle_multipart_signed (composer, mp, depth + 1);
			} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
				/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
				handle_multipart_encrypted (composer, mime_part, depth + 1);
			} else {
				/* depth doesn't matter so long as we don't pass 0 */
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

		html = em_utils_part_to_html (text_part, &length, NULL);
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
		content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

		if (CAMEL_IS_MULTIPART (content)) {
			CamelMultipart *mp;

			mp = CAMEL_MULTIPART (content);

			if (CAMEL_IS_MULTIPART_SIGNED (content)) {
				/* handle the signed content and configure the composer to sign outgoing messages */
				handle_multipart_signed (composer, mp, depth + 1);
			} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
				/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
				handle_multipart_encrypted (composer, mime_part, depth + 1);
			} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
				handle_multipart_alternative (composer, mp, depth + 1);
			} else {
				/* depth doesn't matter so long as we don't pass 0 */
				handle_multipart (composer, mp, depth + 1);
			}
		} else if (depth == 0 && i == 0) {
			gchar *html;
			gssize length;

			/* Since the first part is not multipart/alternative,
			 * this must be the body. */
			html = em_utils_part_to_html (mime_part, &length, NULL);
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
		signature = mail_config_get_signature_by_uid (decoded);
		g_free (decoded);
	} else if (g_str_has_prefix (data, "name:")) {
		decoded = decode_signature_name (data + 5);
		signature = mail_config_get_signature_by_name (decoded);
		g_free (decoded);
	}

	e_composer_header_table_set_signature (table, signature);
}

/**
 * e_msg_composer_new_with_message:
 * @message: The message to use as the source
 *
 * Create a new message composer widget.
 *
 * Note: Designed to work only for messages constructed using Evolution.
 *
 * Returns: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_with_message (CamelMimeMessage *message)
{
	const CamelInternetAddress *to, *cc, *bcc;
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
	EComposerHeaderTable *table;
	GtkToggleAction *action;
	struct _camel_header_raw *xev;
	gint len, i;
	EMsgComposerPrivate *p;

	for (headers = CAMEL_MIME_PART (message)->headers;headers;headers = headers->next) {
		if (!strcmp (headers->name, "X-Evolution-PostTo"))
			postto = g_list_append (postto, g_strstrip (g_strdup (headers->value)));
	}

	if (postto != NULL)
		composer = create_composer (E_MSG_COMPOSER_VISIBLE_MASK_POST);
	else
		composer = create_composer (E_MSG_COMPOSER_VISIBLE_MASK_MAIL);
	p = composer->priv;

	if (!composer) {
		g_list_foreach (postto, (GFunc)g_free, NULL);
		g_list_free (postto);
		return NULL;
	}

	table = e_msg_composer_get_header_table (composer);

	if (postto) {
		e_composer_header_table_set_post_to_list (table, postto);
		g_list_foreach (postto, (GFunc)g_free, NULL);
		g_list_free (postto);
		postto = NULL;
	}

	/* Restore the Account preference */
	account_name = (char *) camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Account");
	if (account_name) {
		account_name = g_strdup (account_name);
		g_strstrip (account_name);

		if ((account = mail_config_get_account_by_uid (account_name)) == NULL)
			/* 'old' setting */
			account = mail_config_get_account_by_name (account_name);
		if (account) {
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
				camel_object_unref (iaddr);
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
				camel_object_unref (iaddr);
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
			printf ("restoring draft flag '%s'\n", flags[i]);

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
	xev = mail_tool_remove_xevolution_headers (message);
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
		if (!is_special_header (headers->name) ||
		    !g_ascii_strcasecmp (headers->name, "References") ||
		    !g_ascii_strcasecmp (headers->name, "In-Reply-To")) {
			g_ptr_array_add (p->extra_hdr_names, g_strdup (headers->name));
			g_ptr_array_add (p->extra_hdr_values, g_strdup (headers->value));
		}

		headers = headers->next;
	}

	/* Restore the attachments and body text */
	content = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	if (CAMEL_IS_MULTIPART (content)) {
		CamelMultipart *multipart;

		multipart = CAMEL_MULTIPART (content);
		content_type = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));

		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* handle the signed content and configure the composer to sign outgoing messages */
			handle_multipart_signed (composer, multipart, 0);
		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
			handle_multipart_encrypted (composer, CAMEL_MIME_PART (message), 0);
		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* this contains the text/plain and text/html versions of the message body */
			handle_multipart_alternative (composer, multipart, 0);
		} else {
			/* there must be attachments... */
			handle_multipart (composer, multipart, 0);
		}
	} else {
		gchar *html;
		gssize length;

		html = em_utils_part_to_html ((CamelMimePart *)message, &length, NULL);
		e_msg_composer_set_pending_body (composer, html, length);
	}

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

	gtk_widget_set_sensitive (composer->priv->attachment_bar, FALSE);
}

/**
 * e_msg_composer_new_redirect:
 * @message: The message to use as the source
 *
 * Create a new message composer widget.
 *
 * Returns: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_redirect (CamelMimeMessage *message,
                             const gchar *resent_from)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	const gchar *subject;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	composer = e_msg_composer_new_with_message (message);
	table = e_msg_composer_get_header_table (composer);

	subject = camel_mime_message_get_subject (message);

	composer->priv->redirect = message;
	camel_object_ref (message);

	e_composer_header_table_set_account_name (table, resent_from);
	e_composer_header_table_set_subject (table, subject);

	disable_editor (composer);

	return composer;
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
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	g_signal_emit (composer, signals[SEND], 0);
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

	g_signal_emit (composer, signals[SAVE_DRAFT], 0, FALSE);

	/* XXX This should be elsewhere. */
	gtkhtml_editor_set_changed (editor, FALSE);
	e_composer_autosave_set_saved (composer, FALSE);
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
	EMsgComposerPrivate *priv = composer->priv;
	EComposerHeaderTable *table;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	EDestination **tov, **ccv, **bccv;
	gchar *subject = NULL, *body = NULL;
	gchar *header, *content, *buf;
	gsize nread, nwritten;
	const gchar *p;
	gint len, clen;
	CamelURL *url;

	table = e_msg_composer_get_header_table (composer);

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

			header = (char *) p;
			header[len] = '\0';
			p += len + 1;

			clen = strcspn (p, "&");

			content = g_strndup (p, clen);
			camel_url_decode (content);

			if (!g_ascii_strcasecmp (header, "to")) {
				to = add_recipients (to, content);
			} else if (!g_ascii_strcasecmp (header, "cc")) {
				cc = add_recipients (cc, content);
			} else if (!g_ascii_strcasecmp (header, "bcc")) {
				bcc = add_recipients (bcc, content);
			} else if (!g_ascii_strcasecmp (header, "subject")) {
				g_free (subject);
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
				if (g_utf8_validate (content, -1, NULL)) {
					body = content;
					content = NULL;
				} else {
					body = g_locale_to_utf8 (content, clen, &nread,
								 &nwritten, NULL);
					if (body) {
						body = g_realloc (body, nwritten + 1);
						body[nwritten] = '\0';
					}
				}
			} else if (!g_ascii_strcasecmp (header, "attach") ||
				   !g_ascii_strcasecmp (header, "attachment")) {
				/* Change file url to absolute path */
				if (!g_ascii_strncasecmp (content, "file:", 5)) {
					url = camel_url_new (content, NULL);
					e_attachment_bar_attach (E_ATTACHMENT_BAR (priv->attachment_bar),
							 	 url->path,
								 "attachment");
					camel_url_free (url);
				} else {
					e_attachment_bar_attach (E_ATTACHMENT_BAR (priv->attachment_bar),
								 content,
								 "attachment");
				}
				gtk_widget_show (priv->attachment_expander);
				gtk_widget_show (priv->attachment_scrolled_window);
			} else if (!g_ascii_strcasecmp (header, "from")) {
				/* Ignore */
			} else if (!g_ascii_strcasecmp (header, "reply-to")) {
				/* ignore */
			} else {
				/* add an arbitrary header? */
				e_msg_composer_add_header (composer, header, content);
			}

			g_free (content);

			p += clen;
			if (*p == '&') {
				p++;
				if (!strcmp (p, "amp;"))
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
		set_editor_text (composer, htmlbody, FALSE);
		g_free (htmlbody);
	}
}

static void
handle_uri (EMsgComposer *composer,
            const gchar *uri,
            gboolean html_dnd)
{
	EMsgComposerPrivate *p = composer->priv;
	GtkhtmlEditor *editor;
	gboolean html_content;

	editor = GTKHTML_EDITOR (composer);
	html_content = gtkhtml_editor_get_html_mode (editor);

	if (!g_ascii_strncasecmp (uri, "mailto:", 7)) {
		handle_mailto (composer, uri);
	} else {
		CamelURL *url = camel_url_new (uri, NULL);
		gchar *type;

		if (!url)
			return;

		if (!g_ascii_strcasecmp (url->protocol, "file")) {
			type = e_msg_composer_guess_mime_type (uri);
			if (!type)
				return;

			if (strncmp (type, "image", 5) || !html_dnd || (!html_content && !strncmp (type, "image", 5))) {
				e_attachment_bar_attach (E_ATTACHMENT_BAR (p->attachment_bar),
						url->path, "attachment");
			}
			g_free (type);
		} else	{
			e_attachment_bar_attach_remote_file (E_ATTACHMENT_BAR (p->attachment_bar),
					uri, "attachment");
		}
		camel_url_free (url);
	}
}

/**
 * e_msg_composer_new_from_url:
 * @url: a mailto URL
 *
 * Create a new message composer widget, and fill in fields as
 * defined by the provided URL.
 **/
EMsgComposer *
e_msg_composer_new_from_url (const gchar *url)
{
	EMsgComposer *composer;

	g_return_val_if_fail (g_ascii_strncasecmp (url, "mailto:", 7) == 0, NULL);

	composer = e_msg_composer_new ();
	if (!composer)
		return NULL;

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

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	table = e_msg_composer_get_header_table (composer);

	set_editor_text (composer, _("<b>(The composer contains a non-text message body, which cannot be edited.)</b>"), FALSE);
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
 * @attachment: the CamelMimePart to attach
 *
 * Attaches @attachment to the message being composed in the composer.
 **/
void
e_msg_composer_attach (EMsgComposer *composer, CamelMimePart *attachment)
{
	EAttachmentBar *bar;
	EMsgComposerPrivate *p = composer->priv;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (attachment));

	bar = E_ATTACHMENT_BAR (p->attachment_bar);
	e_attachment_bar_attach_mime_part (bar, attachment);
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

	stream = camel_stream_fs_new_with_name (dec_file_name, O_RDONLY, 0);
	if (!stream)
		return NULL;

	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	camel_object_unref (CAMEL_OBJECT (stream));

	mime_type = e_msg_composer_guess_mime_type (dec_file_name);
	if (mime_type == NULL)
		mime_type = g_strdup ("application/octet-stream");
	camel_data_wrapper_set_mime_type (wrapper, mime_type);
	g_free (mime_type);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);

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
	camel_object_ref (part);

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
                            gboolean save_html_object_data)
{
	GtkhtmlEditor *editor;
	gboolean html_content;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	if (e_msg_composer_get_remote_download_count (composer) != 0) {
		if (!em_utils_prompt_user (GTK_WINDOW (composer), NULL,
			"mail-composer:ask-send-message-pending-download", NULL)) {
			return NULL;
		}
	}

	editor = GTKHTML_EDITOR (composer);
	html_content = gtkhtml_editor_get_html_mode (editor);

	return build_message (composer, html_content, save_html_object_data);
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
	GtkhtmlEditor *editor;
	EMsgComposer *temp_composer;
	CamelMimeMessage *msg;
	gboolean html_content;
	gchar *flags;

	editor = GTKHTML_EDITOR (composer);
	html_content = gtkhtml_editor_get_html_mode (editor);

	msg = build_message (composer, html_content, save_html_object_data);
	if (msg == NULL)
		return NULL;

	temp_composer = e_msg_composer_new_with_message (msg);
	camel_object_unref (msg);

	/* Override composer flags. */
	flags = msg_composer_get_message_print_helper (
		temp_composer, html_content);

	msg = build_message (temp_composer, TRUE, save_html_object_data);
	if (msg != NULL)
		camel_medium_set_header (
			CAMEL_MEDIUM (msg), "X-Evolution-Format", flags);

	gtk_widget_destroy (GTK_WIDGET (temp_composer));
	g_free (flags);

	return msg;
}

CamelMimeMessage *
e_msg_composer_get_message_draft (EMsgComposer *composer)
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

	msg = build_message (composer, TRUE, TRUE);
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

	if (msg == NULL)
		return NULL;

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
	GtkHTML *html;
	gchar *html_text;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = GTKHTML_EDITOR (composer);
	html = gtkhtml_editor_get_html (editor);

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
		camel_object_unref (CAMEL_OBJECT (address));
		return NULL;
	}

	return address;
}

/**
 * e_msg_composer_guess_mime_type:
 * @filename: filename
 *
 * Returns the guessed mime type of the file given by @filename.
 **/
gchar *
e_msg_composer_guess_mime_type (const gchar *filename)
{
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	gchar *type = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (
		filename, info,
		GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
		GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE |
		GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result == GNOME_VFS_OK)
		type = g_strdup (gnome_vfs_file_info_get_mime_type (info));

	gnome_vfs_file_info_unref (info);

	return type;
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

EAttachmentBar *
e_msg_composer_get_attachment_bar (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return E_ATTACHMENT_BAR (composer->priv->attachment_bar);
}

void
e_msg_composer_set_enable_autosave (EMsgComposer *composer,
                                    gboolean enabled)
{
	e_composer_autosave_set_enabled (composer, enabled);
}

gboolean
e_msg_composer_request_close_all (void)
{
	GSList *iter, *next;

	for (iter = all_composers; iter != NULL; iter = next) {
		EMsgComposer *composer = iter->data;
		next = iter->next;
		gtk_action_activate (ACTION (CLOSE));
	}

	return (all_composers == NULL);
}

EMsgComposer *
e_msg_composer_load_from_file (const gchar *filename)
{
	CamelStream *stream;
	CamelMimeMessage *msg;
	EMsgComposer *composer;

	g_return_val_if_fail (filename != NULL, NULL);

	stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0);
	if (stream == NULL)
		return NULL;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (msg), stream);
	camel_object_unref (stream);

	composer = e_msg_composer_new_with_message (msg);
	if (composer != NULL) {
		g_signal_connect (
			composer, "send",
			G_CALLBACK (em_utils_composer_send_cb), NULL);

		g_signal_connect (
			composer, "save-draft",
			G_CALLBACK (em_utils_composer_save_draft_cb), NULL);

		gtk_widget_show (GTK_WIDGET (composer));
	}

	return composer;
}

void
e_msg_composer_check_autosave (GtkWindow *parent)
{
	GList *orphans = NULL;
	gint response;
	GError *error = NULL;

	/* Look for orphaned autosave files. */
	orphans = e_composer_autosave_find_orphans (&error);
	if (orphans == NULL) {
		if (error != NULL) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		return;
	}

	/* Ask if the user wants to recover the orphaned files. */
	response = e_error_run (
		parent, "mail-composer:recover-autosave", NULL);

	/* Based on the user's response, recover or delete them. */
	while (orphans != NULL) {
		const gchar *filename = orphans->data;
		EMsgComposer *composer;

		if (response == GTK_RESPONSE_YES) {
			/* FIXME: composer is never used */
			composer = autosave_load_draft (filename);
		} else {
			g_unlink (filename);
		}

		g_free (orphans->data);
		orphans = g_list_delete_link (orphans, orphans);
	}
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

void
e_msg_composer_set_send_options (EMsgComposer *composer,
                                 gboolean send_enable)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	composer->priv->send_invoked = send_enable;
}
