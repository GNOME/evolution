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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

   - Somehow users should be able to see if any file(s) are attached even when
     the attachment bar is not shown.

   Should use EventSources to keep track of global changes made to configuration
   values.  Right now it ignores the problem olympically. Miguel.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define SMIME_SUPPORTED 1

#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-window-icon.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-stream-memory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include <libgnomevfs/gnome-vfs.h>

#include <gtkhtml/htmlselection.h>

#include <glade/glade.h>

#include <gal/util/e-iconv.h>
#include <gal/e-text/e-entry.h>

#include "e-util/e-dialog-utils.h"
#include "e-util/e-signature-list.h"
#include "widgets/misc/e-charset-picker.h"
#include "widgets/misc/e-expander.h"
#include "widgets/misc/e-error.h"

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

#include "e-msg-composer.h"
#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"
#include "e-msg-composer-select-file.h"

#include "evolution-shell-component-utils.h"
#include <e-util/e-icon-factory.h>

#include "Editor.h"
#include "listener.h"

#define GNOME_GTKHTML_EDITOR_CONTROL_ID "OAFIID:GNOME_GtkHTML_Editor:" GTKHTML_API_VERSION

#define d(x) x

enum {
	SEND,
	SAVE_DRAFT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
	DND_TYPE_MESSAGE_RFC822,
	DND_TYPE_X_UID_LIST,
	DND_TYPE_TEXT_URI_LIST,
	DND_TYPE_NETSCAPE_URL,
	DND_TYPE_TEXT_VCARD,
	DND_TYPE_TEXT_CALENDAR,
};

static GtkTargetEntry drop_types[] = {
	{ "message/rfc822", 0, DND_TYPE_MESSAGE_RFC822 },
	{ "x-uid-list", 0, DND_TYPE_X_UID_LIST },
	{ "text/uri-list", 0, DND_TYPE_TEXT_URI_LIST },
	{ "_NETSCAPE_URL", 0, DND_TYPE_NETSCAPE_URL },
	{ "text/x-vcard", 0, DND_TYPE_TEXT_VCARD },
	{ "text/calendar", 0, DND_TYPE_TEXT_CALENDAR },
};

#define num_drop_types (sizeof (drop_types) / sizeof (drop_types[0]))

static struct {
	char *target;
	GdkAtom atom;
	guint32 actions;
} drag_info[] = {
	{ "message/rfc822", 0, GDK_ACTION_COPY },
	{ "x-uid-list", 0, GDK_ACTION_ASK|GDK_ACTION_MOVE|GDK_ACTION_COPY },
	{ "text/uri-list", 0, GDK_ACTION_COPY },
	{ "_NETSCAPE_URL", 0, GDK_ACTION_COPY },
	{ "text/x-vcard", 0, GDK_ACTION_COPY },
	{ "text/calendar", 0, GDK_ACTION_COPY },
};

static const char *emc_draft_format_names[] = { "pgp-sign", "pgp-encrypt", "smime-sign", "smime-encrypt" };


/* The parent class.  */
static BonoboWindowClass *parent_class = NULL;

/* All the composer windows open, for bookkeeping purposes.  */
static GSList *all_composers = NULL;


/* local prototypes */
static GList *add_recipients (GList *list, const char *recips);

static void handle_mailto (EMsgComposer *composer, const char *mailto);

/* used by e_msg_composer_add_message_attachments() */
static void add_attachments_from_multipart (EMsgComposer *composer, CamelMultipart *multipart,
					    gboolean just_inlines, int depth);

/* used by e_msg_composer_new_with_message() */
static void handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, int depth);
static void handle_multipart_alternative (EMsgComposer *composer, CamelMultipart *multipart, int depth);
static void handle_multipart_encrypted (EMsgComposer *composer, CamelMultipart *multipart, int depth);
static void handle_multipart_signed (EMsgComposer *composer, CamelMultipart *multipart, int depth);

static void set_editor_signature (EMsgComposer *composer);


static EDestination**
destination_list_to_vector_sized (GList *list, int n)
{
	EDestination **destv;
	int i = 0;
	
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

static GByteArray *
get_text (Bonobo_PersistStream persist, char *format)
{
	BonoboStream *stream;
	BonoboStreamMem *stream_mem;
	CORBA_Environment ev;
	GByteArray *text;
	
	CORBA_exception_init (&ev);
	
	stream = bonobo_stream_mem_create (NULL, 0, FALSE, TRUE);
	Bonobo_PersistStream_save (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
				   format, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception getting mail '%s'",
			   bonobo_exception_get_text (&ev));
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	stream_mem = BONOBO_STREAM_MEM (stream);
	text = g_byte_array_new ();
	g_byte_array_append (text, stream_mem->buffer, stream_mem->pos);
	bonobo_object_unref (BONOBO_OBJECT (stream));
	
	return text;
}

#define LINE_LEN 72

static CamelTransferEncoding
best_encoding (GByteArray *buf, const char *charset)
{
	char *in, *out, outbuf[256], *ch;
	size_t inlen, outlen;
	int status, count = 0;
	iconv_t cd;
	
	if (!charset)
		return -1;
	
	cd = e_iconv_open (charset, "utf-8");
	if (cd == (iconv_t) -1)
		return -1;
	
	in = buf->data;
	inlen = buf->len;
	do {
		out = outbuf;
		outlen = sizeof (outbuf);
		status = e_iconv (cd, (const char **) &in, &inlen, &out, &outlen);
		for (ch = out - 1; ch >= outbuf; ch--) {
			if ((unsigned char)*ch > 127)
				count++;
		}
	} while (status == (size_t) -1 && errno == E2BIG);
	e_iconv_close (cd);
	
	if (status == (size_t) -1)
		return -1;
	
	if (count == 0)
		return CAMEL_TRANSFER_ENCODING_7BIT;
	else if (count <= buf->len * 0.17)
		return CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
	else
		return CAMEL_TRANSFER_ENCODING_BASE64;
}

static char *
composer_get_default_charset_setting (void)
{
	GConfClient *gconf;
	const char *locale;
	char *charset;
	
	gconf = gconf_client_get_default ();
	charset = gconf_client_get_string (gconf, "/apps/evolution/mail/composer/charset", NULL);
	
	if (!charset || charset[0] == '\0') {
		g_free (charset);
		charset = gconf_client_get_string (gconf, "/apps/evolution/mail/format/charset", NULL);
		if (charset && charset[0] == '\0') {
			g_free (charset);
			charset = NULL;
		}
	}
	
	g_object_unref (gconf);
	
	if (!charset && (locale = e_iconv_locale_charset ()))
		charset = g_strdup (locale);
	
	return charset ? charset : g_strdup ("us-ascii");
}

static char *
best_charset (GByteArray *buf, const char *default_charset, CamelTransferEncoding *encoding)
{
	char *charset;
	
	/* First try US-ASCII */
	*encoding = best_encoding (buf, "US-ASCII");
	if (*encoding == CAMEL_TRANSFER_ENCODING_7BIT)
		return NULL;
	
	/* Next try the user-specified charset for this message */
	*encoding = best_encoding (buf, default_charset);
	if (*encoding != -1)
		return g_strdup (default_charset);
	
	/* Now try the user's default charset from the mail config */
	charset = composer_get_default_charset_setting ();
	*encoding = best_encoding (buf, charset);
	if (*encoding != -1)
		return charset;
	
	/* Try to find something that will work */
	if (!(charset = (char *) camel_charset_best (buf->data, buf->len))) {
		*encoding = CAMEL_TRANSFER_ENCODING_7BIT;
		return NULL;
	}
	
	*encoding = best_encoding (buf, charset);
	
	return g_strdup (charset);
}

static gboolean
clear_inline_images (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	camel_object_unref (value);

	return TRUE;
}

static void
clear_current_images (EMsgComposer *composer)
{
	g_list_free (composer->current_images);
	composer->current_images = NULL;
}

static gboolean
clear_url (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);

	return TRUE;
}

void
e_msg_composer_clear_inlined_table (EMsgComposer *composer)
{
	g_hash_table_foreach_remove (composer->inline_images, clear_inline_images, NULL);
	g_hash_table_foreach_remove (composer->inline_images_by_url, clear_url, NULL);
}

static void
add_inlined_images (EMsgComposer *composer, CamelMultipart *multipart)
{
	GList *d = composer->current_images;
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

/* This functions builds a CamelMimeMessage for the message that the user has
 * composed in `composer'.
 */
static CamelMimeMessage *
build_message (EMsgComposer *composer, gboolean save_html_object_data)
{
	EMsgComposerAttachmentBar *attachment_bar =
		E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	CamelDataWrapper *plain, *html, *current;
	CamelTransferEncoding plain_encoding;
	const char *iconv_charset = NULL;
	GPtrArray *recipients = NULL;
	CamelMultipart *body = NULL;
	CamelContentType *type;
	CamelMimeMessage *new;
	CamelStream *stream;
	CamelMimePart *part;
	CamelException ex;
	GByteArray *data;
	char *charset;
	int i;
	
	if (composer->persist_stream_interface == CORBA_OBJECT_NIL)
		return NULL;
	
	/* evil kludgy hack for Redirect */
	if (composer->redirect) {
		e_msg_composer_hdrs_to_redirect (hdrs, composer->redirect);
		camel_object_ref (composer->redirect);
		return composer->redirect;
	}
	
	new = camel_mime_message_new ();
	e_msg_composer_hdrs_to_message (hdrs, new);
	for (i = 0; i < composer->extra_hdr_names->len; i++) {
		camel_medium_add_header (CAMEL_MEDIUM (new),
					 composer->extra_hdr_names->pdata[i],
					 composer->extra_hdr_values->pdata[i]);
	}
	
	if (composer->mime_body) {
		plain_encoding = CAMEL_TRANSFER_ENCODING_7BIT;
		for (i = 0; composer->mime_body[i]; i++) {
			if ((unsigned char) composer->mime_body[i] > 127) {
				plain_encoding = CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE;
				break;
			}
		}
		data = g_byte_array_new ();
		g_byte_array_append (data, composer->mime_body, strlen (composer->mime_body));
		type = camel_content_type_decode (composer->mime_type);
	} else {
		data = get_text (composer->persist_stream_interface, "text/plain");
		if (!data) {
			/* The component has probably died */
			camel_object_unref (CAMEL_OBJECT (new));
			return NULL;
		}
		
		/* FIXME: we may want to do better than this... */
		charset = best_charset (data, composer->charset, &plain_encoding);
		type = camel_content_type_new ("text", "plain");
		if ((charset = best_charset (data, composer->charset, &plain_encoding))) {
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
	
	if (composer->send_html) {
		CORBA_Environment ev;
		clear_current_images (composer);
		
		if (save_html_object_data) {
			CORBA_exception_init (&ev);
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "save-data-on", &ev);
		}
		data = get_text (composer->persist_stream_interface, "text/html");		
		if (save_html_object_data) {
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "save-data-off", &ev);
			CORBA_exception_free (&ev);
		}
		
		if (!data) {
			/* The component has probably died */
			camel_object_unref (new);
			camel_object_unref (plain);
			return NULL;
		}
		
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
		if (composer->current_images) {
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
	
	if (e_msg_composer_attachment_bar_get_num_attachments (attachment_bar)) {
		CamelMultipart *multipart = camel_multipart_new ();
		
		if (composer->is_alternative) {
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
		
		e_msg_composer_attachment_bar_to_multipart (attachment_bar, multipart, composer->charset);
		
		if (composer->is_alternative) {
			int i;
			
			for (i = camel_multipart_get_number (multipart); i > 1; i--) {
				part = camel_multipart_get_part (multipart, i - 1);
				camel_medium_remove_header (CAMEL_MEDIUM (part), "Content-Disposition");
			}
		}
		
		current = CAMEL_DATA_WRAPPER (multipart);
	}
	
	camel_exception_init (&ex);

	/* Setup working recipient list if we're encrypting */
	if (composer->pgp_encrypt
#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
	    || composer->smime_encrypt
#endif
		) {
		int i, j;
		const char *types[] = { CAMEL_RECIPIENT_TYPE_TO, CAMEL_RECIPIENT_TYPE_CC, CAMEL_RECIPIENT_TYPE_BCC };

		recipients = g_ptr_array_new();
		for (i=0; i < sizeof(types)/sizeof(types[0]); i++) {
			const CamelInternetAddress *addr;
			const char *address;

			addr = camel_mime_message_get_recipients(new, types[i]);
			for (j=0;camel_internet_address_get(addr, j, NULL, &address); j++)
				g_ptr_array_add(recipients, g_strdup (address));

		}
	}
	
	if (composer->pgp_sign || composer->pgp_encrypt) {
		const char *pgp_userid;
		CamelInternetAddress *from = NULL;
		CamelCipherContext *cipher;

		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		camel_object_unref (current);

		if (hdrs->account && hdrs->account->pgp_key && *hdrs->account->pgp_key) {
			pgp_userid = hdrs->account->pgp_key;
		} else {
			from = e_msg_composer_hdrs_get_from(hdrs);
			camel_internet_address_get(from, 0, NULL, &pgp_userid);
		}
		
		if (composer->pgp_sign) {
			CamelMimePart *npart = camel_mime_part_new();

			cipher = mail_crypto_get_pgp_cipher_context(hdrs->account);
			camel_cipher_sign(cipher, pgp_userid, CAMEL_CIPHER_HASH_SHA1, part, npart, &ex);
			camel_object_unref(cipher);
			
			if (camel_exception_is_set(&ex)) {
				camel_object_unref(npart);
				goto exception;
			}

			camel_object_unref(part);
			part = npart;
		}
		
		if (composer->pgp_encrypt) {
			CamelMimePart *npart = camel_mime_part_new();

			/* check to see if we should encrypt to self, NB gets removed immediately after use */
			if (hdrs->account && hdrs->account->pgp_encrypt_to_self && pgp_userid)
				g_ptr_array_add (recipients, g_strdup (pgp_userid));

			cipher = mail_crypto_get_pgp_cipher_context (hdrs->account);
			camel_cipher_encrypt(cipher, pgp_userid, recipients, part, npart, &ex);
			camel_object_unref (cipher);

			if (hdrs->account && hdrs->account->pgp_encrypt_to_self && pgp_userid)
				g_ptr_array_set_size(recipients, recipients->len - 1);

			if (camel_exception_is_set (&ex)) {
				camel_object_unref(npart);
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
	if (composer->smime_sign || composer->smime_encrypt) {
		CamelInternetAddress *from = NULL;
		CamelCipherContext *cipher;

		part = camel_mime_part_new();
		camel_medium_set_content_object((CamelMedium *)part, current);
		if (current == plain)
			camel_mime_part_set_encoding(part, plain_encoding);
		camel_object_unref(current);

		if (composer->smime_sign
		    && (hdrs->account == NULL || hdrs->account->smime_sign_key == NULL || hdrs->account->smime_sign_key[0] == 0)) {
			camel_exception_set (&ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot sign outgoing message: No signing certificate set for this account"));
			goto exception;
		}
		
		if (composer->smime_encrypt
		    && (hdrs->account == NULL || hdrs->account->smime_sign_key == NULL || hdrs->account->smime_sign_key[0] == 0)) {
			camel_exception_set (&ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot encrypt outgoing message: No encryption certificate set for this account"));
			goto exception;
		}

		if (composer->smime_sign) {
			CamelMimePart *npart = camel_mime_part_new();

			cipher = camel_smime_context_new(session);

			/* if we're also encrypting, envelope-sign rather than clear-sign */
			if (composer->smime_encrypt) {
				camel_smime_context_set_sign_mode((CamelSMIMEContext *)cipher, CAMEL_SMIME_SIGN_ENVELOPED);
				camel_smime_context_set_encrypt_key((CamelSMIMEContext *)cipher, TRUE, hdrs->account->smime_encrypt_key);
			} else if (hdrs->account && hdrs->account->smime_encrypt_key && *hdrs->account->smime_encrypt_key) {
				camel_smime_context_set_encrypt_key((CamelSMIMEContext *)cipher, TRUE, hdrs->account->smime_encrypt_key);
			}

			camel_cipher_sign(cipher, hdrs->account->smime_sign_key, CAMEL_CIPHER_HASH_SHA1, part, npart, &ex);
			camel_object_unref(cipher);
			
			if (camel_exception_is_set(&ex)) {
				camel_object_unref(npart);
				goto exception;
			}

			camel_object_unref(part);
			part = npart;
		}
	
		if (composer->smime_encrypt) {
			/* check to see if we should encrypt to self, NB removed after use */
			if (hdrs->account->smime_encrypt_to_self)
				g_ptr_array_add(recipients, g_strdup (hdrs->account->smime_encrypt_key));

			cipher = camel_smime_context_new(session);
			camel_smime_context_set_encrypt_key((CamelSMIMEContext *)cipher, TRUE, hdrs->account->smime_encrypt_key);

			camel_cipher_encrypt(cipher, NULL, recipients, part, (CamelMimePart *)new, &ex);
			camel_object_unref(cipher);

			if (camel_exception_is_set(&ex))
				goto exception;

			if (hdrs->account->smime_encrypt_to_self)
				g_ptr_array_set_size(recipients, recipients->len - 1);
		}

		if (from)
			camel_object_unref(from);

		/* we replaced the message directly, we don't want to do reparenting foo */
		if (composer->smime_encrypt) {
			camel_object_unref(part);
			goto skip_content;
		} else {
			current = camel_medium_get_content_object((CamelMedium *)part);
			camel_object_ref(current);
			camel_object_unref(part);
		}
	}
#endif /* HAVE_NSS */

	camel_medium_set_content_object (CAMEL_MEDIUM (new), current);
	if (current == plain)
		camel_mime_part_set_encoding (CAMEL_MIME_PART (new), plain_encoding);
	camel_object_unref (current);

skip_content:

	if (recipients) {
		for (i=0; i<recipients->len; i++)
			g_free(recipients->pdata[i]);
		g_ptr_array_free(recipients, TRUE);
	}

	/* Attach whether this message was written in HTML */
	camel_medium_set_header (CAMEL_MEDIUM (new), "X-Evolution-Format",
				 composer->send_html ? "text/html" : "text/plain");
	
	return new;
	
 exception:
	
	if (part != CAMEL_MIME_PART (new))
		camel_object_unref (part);
	
	camel_object_unref (new);
	
	if (ex.id != CAMEL_EXCEPTION_USER_CANCEL) {
		e_error_run((GtkWindow *)composer, "mail-composer:no-build-message",
			    camel_exception_get_description(&ex), NULL);
	}

	camel_exception_clear (&ex);

	if (recipients) {
		for (i=0; i<recipients->len; i++)
			g_free(recipients->pdata[i]);
		g_ptr_array_free(recipients, TRUE);
	}
	
	return NULL;
}


static char *
get_file_content (EMsgComposer *composer, const char *file_name, gboolean want_html, guint flags, gboolean warn)
{
	CamelStreamFilter *filtered_stream;
	CamelStreamMem *memstream;
	CamelMimeFilter *html, *charenc;
	CamelStream *stream;
	GByteArray *buffer;
	char *charset;
	char *content;
	int fd;
	
	fd = open (file_name, O_RDONLY);
	if (fd == -1) {
		if (warn)
			e_error_run((GtkWindow *)composer, "mail-composer:no-sig-file",
				    file_name, g_strerror(errno), NULL);
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
	if (buffer->len && !g_utf8_validate (buffer->data, buffer->len, NULL)) {
		stream = (CamelStream *) memstream;
		memstream = (CamelStreamMem *) camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (memstream, g_byte_array_new ());
		
		filtered_stream = camel_stream_filter_new_with_stream (stream);
		camel_object_unref (stream);
		
		charset = composer && composer->charset ? composer->charset : NULL;
		charset = charset ? g_strdup (charset) : composer_get_default_charset_setting ();
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
	
	g_byte_array_append (buffer, "", 1);
	content = buffer->data;
	g_byte_array_free (buffer, FALSE);
	
	return content;
}

char *
e_msg_composer_get_sig_file_content (const char *sigfile, gboolean in_html)
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

static void
prepare_engine (EMsgComposer *composer)
{
	CORBA_Environment ev;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	/* printf ("prepare_engine\n"); */
	
	CORBA_exception_init (&ev);
	composer->editor_engine = (GNOME_GtkHTML_Editor_Engine) Bonobo_Unknown_queryInterface
		(bonobo_widget_get_objref (BONOBO_WIDGET (composer->editor)), "IDL:GNOME/GtkHTML/Editor/Engine:1.0", &ev);
	if ((composer->editor_engine != CORBA_OBJECT_NIL) && (ev._major == CORBA_NO_EXCEPTION)) {
		
		/* printf ("trying set listener\n"); */
		composer->editor_listener = BONOBO_OBJECT (listener_new (composer));
		if (composer->editor_listener != NULL)
			GNOME_GtkHTML_Editor_Engine__set_listener (composer->editor_engine,
								   (GNOME_GtkHTML_Editor_Listener)
								   bonobo_object_dup_ref
								   (bonobo_object_corba_objref (composer->editor_listener),
								    &ev),
								   &ev);
		
		if ((ev._major != CORBA_NO_EXCEPTION) || (composer->editor_listener == NULL)) {
			CORBA_Environment err_ev;

			CORBA_exception_init (&err_ev);
			
			Bonobo_Unknown_unref (composer->editor_engine, &err_ev);
			CORBA_Object_release (composer->editor_engine, &err_ev);

			CORBA_exception_free (&err_ev);

			composer->editor_engine = CORBA_OBJECT_NIL;
			g_warning ("Can't establish Editor Listener\n");
		}
	} else {
		composer->editor_engine = CORBA_OBJECT_NIL;
		g_warning ("Can't get Editor Engine\n");
	}

	CORBA_exception_free (&ev);
}

static char *
encode_signature_name (const char *name)
{
	const char *s;
	char *ename, *e;
	int len = 0;

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

static char *
decode_signature_name (const char *name)
{
	const char *s;
	char *dname, *d;
	int len = 0;

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

static char *
get_signature_html (EMsgComposer *composer)
{
	char *text = NULL, *html = NULL;
	gboolean format_html;
	
	if (!composer->signature)
		return NULL;
	
	if (!composer->signature->autogen) {
		if (!composer->signature->filename)
			return NULL;
		
		format_html = composer->signature->html;
		
		if (composer->signature->script) {
			text = mail_config_signature_run_script (composer->signature->filename);
		} else {
			text = e_msg_composer_get_sig_file_content (composer->signature->filename, format_html);
		}
	} else {
		EAccountIdentity *id;
		char *organization;
		char *address;
		char *name;
		
		id = E_MSG_COMPOSER_HDRS (composer->hdrs)->account->id;
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
		char *encoded_uid = NULL;
		
		if (composer->signature)
			encoded_uid = encode_signature_name (composer->signature->uid);
		
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
					format_html || (!strncmp ("-- \n", text, 4) || strstr(text, "\n-- \n")) ? "" : "-- \n",
					text,
					format_html ? "" : "</PRE>\n");
		g_free (text);
		g_free (encoded_uid);
		text = html;
	}
	
	return text;
}

static void
set_editor_text(EMsgComposer *composer, const char *text, ssize_t len, int set_signature, int pad_signature)
{
	Bonobo_PersistStream persist;
	BonoboStream *stream;
	BonoboWidget *editor;
	CORBA_Environment ev;
	Bonobo_Unknown object;

	g_return_if_fail (composer->persist_stream_interface != CORBA_OBJECT_NIL);
	
	persist = composer->persist_stream_interface;
	
	editor = BONOBO_WIDGET (composer->editor);
	
	CORBA_exception_init (&ev);

	if (len == -1)
		len = strlen (text);

	stream = bonobo_stream_mem_create (text, len, TRUE, FALSE);
	object = bonobo_object_corba_objref (BONOBO_OBJECT (stream));
	Bonobo_PersistStream_load (persist, (Bonobo_Stream) object, "text/html", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME. Some error message. */
		bonobo_object_unref (BONOBO_OBJECT (stream));
		CORBA_exception_free (&ev);
		return;
	}
	
	CORBA_exception_free (&ev);
	
	bonobo_object_unref (BONOBO_OBJECT (stream));

	if (set_signature)
		e_msg_composer_show_sig_file (composer);
}

/* Commands.  */

static void
show_attachments (EMsgComposer *composer,
		  gboolean show)
{
	e_expander_set_expanded (E_EXPANDER (composer->attachment_expander), show);
}

static void
save (EMsgComposer *composer, const char *default_filename)
{
	CORBA_Environment ev;
	char *filename;
	int fd;
	
	if (default_filename != NULL)
		filename = g_strdup (default_filename);
	else
		filename = e_msg_composer_select_file (composer, _("Save as..."), TRUE);
	
	if (filename == NULL)
		return;
	
	/* check to see if we already have the file and that we can create it */
	if ((fd = open (filename, O_RDONLY | O_CREAT | O_EXCL, 0777)) == -1) {
		int resp, errnosav = errno;
		struct stat st;
		
		if (stat (filename, &st) == 0 && S_ISREG (st.st_mode)) {
			resp = e_error_run((GtkWindow *)composer, E_ERROR_ASK_FILE_EXISTS_OVERWRITE,
					   filename, NULL);
			if (resp != GTK_RESPONSE_OK) {
				g_free (filename);
				return;
			}
		} else {
			e_error_run((GtkWindow *)composer, E_ERROR_NO_SAVE_FILE,
				    filename, g_strerror(errnosav));
			g_free (filename);
			return;
		}
	} else
		close (fd);
	
	CORBA_exception_init (&ev);
	
	Bonobo_PersistFile_save (composer->persist_file_interface, filename, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		e_error_run((GtkWindow *)composer, E_ERROR_NO_SAVE_FILE,
			    filename, _("Unknown reason"));
	} else {
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "saved", &ev);
		e_msg_composer_unset_autosaved (composer);
	}
	CORBA_exception_free (&ev);
	
	g_free (filename);
}

static void
load (EMsgComposer *composer, const char *file_name)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	Bonobo_PersistFile_load (composer->persist_file_interface, file_name, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION)
		e_error_run((GtkWindow *)composer, E_ERROR_NO_LOAD_FILE,
			    file_name, _("Unknown reason"), NULL);
	
	CORBA_exception_free (&ev);
}

#define AUTOSAVE_SEED ".evolution-composer.autosave-XXXXXX"
#define AUTOSAVE_INTERVAL 60000

typedef struct _AutosaveManager AutosaveManager;
struct _AutosaveManager {
	GHashTable *table;
	guint id;
	gboolean ask;
};

static AutosaveManager *am = NULL;
static void autosave_manager_start (AutosaveManager *am);
static void autosave_manager_stop (AutosaveManager *am);

static gboolean
autosave_save_draft (EMsgComposer *composer)
{
	CamelMimeMessage *message;
	CamelStream *stream;
	char *file;
	int fd, camelfd;
	gboolean success = TRUE;
	
	if (!e_msg_composer_is_dirty (composer))
		return TRUE;

	fd = composer->autosave_fd;
	file = composer->autosave_file;
	
	if (fd == -1) {
		/* This code is odd, the fd is opened elsewhere but a failure is ignored */
		e_error_run((GtkWindow *)composer, "mail-composer:no-autosave",
			    file, _("Could not open file"), NULL);
		return FALSE;
	}
	
	message = e_msg_composer_get_message_draft (composer);
	
	if (message == NULL) {
		e_error_run((GtkWindow *)composer, "mail-composer:no-autosave",
			    file, _("Unable to retrieve message from editor"), NULL);
		return FALSE;
	}
	
	if (lseek (fd, (off_t)0, SEEK_SET) == -1
	    || ftruncate (fd, (off_t)0) == -1
	    || (camelfd = dup(fd)) == -1) {
		camel_object_unref (message);
		e_error_run((GtkWindow *)composer, "mail-composer:no-autosave",
			    file, g_strerror(errno), NULL);
		return FALSE;
	}
	
	/* this does an lseek so we don't have to */
	stream = camel_stream_fs_new_with_fd (camelfd);
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream) == -1
	    || camel_stream_close (CAMEL_STREAM (stream)) == -1) {
		e_error_run((GtkWindow *)composer, "mail-composer:no-autosave",
			    file, g_strerror(errno), NULL);
		success = FALSE;
	} else {
		CORBA_Environment ev;
		CORBA_exception_init (&ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "saved", &ev);
		CORBA_exception_free (&ev);
		e_msg_composer_unset_changed (composer);
		e_msg_composer_set_autosaved (composer);
	}
	
	camel_object_unref (stream);
	
	camel_object_unref (message);
	
	return success;
}

static EMsgComposer * 
autosave_load_draft (const char *filename)
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
		if (autosave_save_draft (composer))
			unlink (filename);
		
		g_signal_connect (GTK_OBJECT (composer), "send",
				  G_CALLBACK (em_utils_composer_send_cb), NULL);
		
		g_signal_connect (GTK_OBJECT (composer), "save-draft",
				  G_CALLBACK (em_utils_composer_save_draft_cb), NULL);
		
		gtk_widget_show (GTK_WIDGET (composer));
	}
	
	return composer;
}

static gboolean
autosave_is_owned (AutosaveManager *am, const char *file)
{
        return g_hash_table_lookup (am->table, file) != NULL;
}

static void
autosave_manager_query_load_orphans (AutosaveManager *am, GtkWindow *parent)
{
	DIR *dir;
	struct dirent *d;
	GSList *match = NULL;
	gint len = strlen (AUTOSAVE_SEED);
	gint load = FALSE;
	
	dir = opendir (g_get_home_dir());
	if (!dir) {
		return;
	}
	
	while ((d = readdir (dir))) {
		if ((!strncmp (d->d_name, AUTOSAVE_SEED, len - 6))
		    && (strlen (d->d_name) == len)
		    && (!autosave_is_owned (am, d->d_name))) {
			char *filename =  g_strdup_printf ("%s/%s", g_get_home_dir(), d->d_name);
			struct stat st;
		
			/*
			 * check if the file has any length,  It is a valid case if it doesn't 
			 * so we simply don't ask then.
			 */
			if (stat (filename, &st) == -1 || st.st_size == 0) {
				unlink (filename);
				g_free (filename);
				continue;
			}
			match = g_slist_prepend (match, filename);				
		}
	}
	
	closedir (dir);
	
	if (match != NULL)
		load = e_error_run(parent, "mail-composer:recover-autosave", NULL) == GTK_RESPONSE_YES;
	
	while (match != NULL) {
		GSList *next = match->next;
		char *filename = match->data;
		EMsgComposer *composer;
		
		if (load) { 
			composer = autosave_load_draft (filename);
		} else {
			unlink (filename);
		}
			
		g_free (filename);
		g_slist_free_1 (match);
		match = next;
	}			
}

static void
autosave_run_foreach_cb (gpointer key, gpointer value, gpointer data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (value);
	
	if (composer->enable_autosave)
		autosave_save_draft (composer);
}

static gint
autosave_run (gpointer data)
{
	AutosaveManager *am = data;
	
	g_hash_table_foreach (am->table, (GHFunc)autosave_run_foreach_cb, am);

	autosave_manager_stop (am);
	autosave_manager_start (am);
	
	return FALSE;
}

static gboolean
autosave_init_file (EMsgComposer *composer)
{
	if (composer->autosave_file == NULL) {
		composer->autosave_file = g_strdup_printf ("%s/%s", g_get_home_dir(), AUTOSAVE_SEED);
		composer->autosave_fd = mkstemp (composer->autosave_file);
		return TRUE;
	}
	return FALSE;
}

static void
autosave_manager_start (AutosaveManager *am)
{
	if (am->id == 0)
		am->id = g_timeout_add (AUTOSAVE_INTERVAL, autosave_run, am);
}

static void
autosave_manager_stop (AutosaveManager *am)
{
	if (am->id) {
		g_source_remove (am->id);
		am->id = 0;
	}
}

static AutosaveManager *
autosave_manager_new ()
{
	AutosaveManager *am;
	
	am = g_new (AutosaveManager, 1);
	am->table = g_hash_table_new (g_str_hash, g_str_equal);
	am->id = 0;
	am->ask = TRUE;
	
	return am;
}

static void
autosave_manager_register (AutosaveManager *am, EMsgComposer *composer) 
{
	char *key;
	
	g_return_if_fail (composer != NULL);
	
	if (autosave_init_file (composer)) {
		key = g_path_get_basename (composer->autosave_file);
		g_hash_table_insert (am->table, key, composer);
		if (am->ask) {
			/* keep recursion out of our bedrooms. */
			am->ask = FALSE;
			autosave_manager_query_load_orphans (am, (GtkWindow *)composer);
			am->ask = TRUE;
		}
	}
	autosave_manager_start (am);
}

static void
autosave_manager_unregister (AutosaveManager *am, EMsgComposer *composer) 
{
	char *key, *oldkey;
	void *olddata;

	if (!composer->autosave_file)
		return;

	key = g_path_get_basename(composer->autosave_file);
	if (g_hash_table_lookup_extended(am->table, key, (void **)&oldkey, &olddata)) {
		g_hash_table_remove(am->table, oldkey);
		g_free(oldkey);
		g_free(key);
	}
	
	/* only remove the file if we can successfully save it */
	/* FIXME this test could probably be more efficient */
	if (autosave_save_draft (composer))
		unlink (composer->autosave_file);
	
	close (composer->autosave_fd);
	g_free (composer->autosave_file);
	composer->autosave_file = NULL;
	
	if (g_hash_table_size (am->table) == 0)
		autosave_manager_stop (am);
}

static void
menu_file_save_draft_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	g_signal_emit (data, signals[SAVE_DRAFT], 0, FALSE);
	e_msg_composer_unset_changed (E_MSG_COMPOSER (data));
	e_msg_composer_unset_autosaved (E_MSG_COMPOSER (data));
}

/* Exit dialog.  (Displays a "Save composition to 'Drafts' before exiting?" warning before actually exiting.)  */

static void
do_exit (EMsgComposer *composer)
{
	const char *subject;
	int button;
	
	if (!e_msg_composer_is_dirty (composer) && !e_msg_composer_is_autosaved (composer)) {
		gtk_widget_destroy (GTK_WIDGET (composer));
		return;
	}
	
	gdk_window_raise (GTK_WIDGET (composer)->window);
	
	subject = e_msg_composer_hdrs_get_subject (E_MSG_COMPOSER_HDRS (composer->hdrs));

	button = e_error_run((GtkWindow *)composer, "mail-composer:exit-unsaved",
			     subject && subject[0] ? subject : _("Untitled Message"), NULL);

	switch (button) {
	case GTK_RESPONSE_YES:
		/* Save */
		g_signal_emit (GTK_OBJECT (composer), signals[SAVE_DRAFT], 0, TRUE);
		e_msg_composer_unset_changed (composer);
		e_msg_composer_unset_autosaved (composer);
		break;
	case GTK_RESPONSE_NO:
		/* Don't save */
		gtk_widget_destroy (GTK_WIDGET (composer));
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	}
}

/* Menu callbacks.  */

static void
menu_file_open_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	EMsgComposer *composer;
	char *file_name;
	
	composer = E_MSG_COMPOSER (data);
	
	file_name = e_msg_composer_select_file (composer, _("Open file"), FALSE);
	if (file_name == NULL)
		return;
	
	load (composer, file_name);
	
	g_free (file_name);
}

static void
menu_file_save_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	EMsgComposer *composer;
	CORBA_char *file_name;
	CORBA_Environment ev;
	
	composer = E_MSG_COMPOSER (data);
	
	CORBA_exception_init (&ev);
	
	file_name = Bonobo_PersistFile_getCurrentFile (composer->persist_file_interface, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		save (composer, NULL);
	} else {
		save (composer, file_name);
		CORBA_free (file_name);
	}
      	CORBA_exception_free (&ev);
}

static void
menu_file_save_as_cb (BonoboUIComponent *uic,
		      void *data,
		      const char *path)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	save (composer, NULL);
}

static void
menu_file_send_cb (BonoboUIComponent *uic,
		   void *data,
		   const char *path)
{
	g_signal_emit (GTK_OBJECT (data), signals[SEND], 0);
}

static void
menu_file_close_cb (BonoboUIComponent *uic,
		    void *data,
		    const char *path)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	do_exit (composer);
}
	
static void
menu_file_add_attachment_cb (BonoboUIComponent *uic,
			     void *data,
			     const char *path)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	e_msg_composer_attachment_bar_attach
		(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
		 NULL);
}

static void
menu_edit_cut_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	EMsgComposer *composer = data;
	
	g_return_if_fail (composer->focused_entry != NULL);
	
	if (GTK_IS_ENTRY (composer->focused_entry)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (composer->focused_entry));
	} else {
		/* happy happy joy joy, an EEntry. */
		g_assert_not_reached ();
	}
}

static void
menu_edit_copy_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	EMsgComposer *composer = data;
	
	g_return_if_fail (composer->focused_entry != NULL);
	
	if (GTK_IS_ENTRY (composer->focused_entry)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (composer->focused_entry));
	} else {
		/* happy happy joy joy, an EEntry. */
		g_assert_not_reached ();
	}
}

static void
menu_edit_paste_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	EMsgComposer *composer = data;
	
	g_return_if_fail (composer->focused_entry != NULL);
	
	if (GTK_IS_ENTRY (composer->focused_entry)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (composer->focused_entry));
	} else {
		/* happy happy joy joy, an EEntry. */
		g_assert_not_reached ();
	}
}

static void
menu_edit_select_all_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	EMsgComposer *composer = data;
	
	g_return_if_fail (composer->focused_entry != NULL);
	
	if (GTK_IS_ENTRY (composer->focused_entry)) {
		gtk_editable_set_position (GTK_EDITABLE (composer->focused_entry), -1);
		gtk_editable_select_region (GTK_EDITABLE (composer->focused_entry), 0, -1);
	} else {
		/* happy happy joy joy, an EEntry. */
		g_assert_not_reached ();
	}
}

static void
menu_edit_delete_all_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	CORBA_Environment ev;
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	CORBA_exception_init (&ev);
	
	GNOME_GtkHTML_Editor_Engine_undoBegin (composer->editor_engine, "Delete all but signature", "Undelete all", &ev);
	GNOME_GtkHTML_Editor_Engine_freeze (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "disable-selection", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "text-default-color", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "bold-off", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "italic-off", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "underline-off", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "strikeout-off", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "select-all", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "delete", &ev);
	GNOME_GtkHTML_Editor_Engine_setParagraphData (composer->editor_engine, "signature", "0", &ev);
	GNOME_GtkHTML_Editor_Engine_setParagraphData (composer->editor_engine, "orig", "0", &ev);
	e_msg_composer_show_sig_file (composer);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "style-normal", &ev);
	GNOME_GtkHTML_Editor_Engine_thaw (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_undoEnd (composer->editor_engine, &ev);
	
	CORBA_exception_free (&ev);
	/* printf ("delete all\n"); */
}

static void
menu_view_attachments_activate_cb (BonoboUIComponent           *component,
				   const char                  *path,
				   Bonobo_UIComponent_EventType type,
				   const char                  *state,
				   gpointer                     user_data)

{
	gboolean new_state;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	new_state = atoi (state);
	
	e_msg_composer_show_attachments (E_MSG_COMPOSER (user_data), new_state);
}

static void
menu_format_html_cb (BonoboUIComponent           *component,
		     const char                  *path,
		     Bonobo_UIComponent_EventType type,
		     const char                  *state,
		     gpointer                     user_data)

{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_send_html (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_security_pgp_sign_cb (BonoboUIComponent           *component,
			   const char                  *path,
			   Bonobo_UIComponent_EventType type,
			   const char                  *state,
			   gpointer                     composer)

{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_pgp_sign (E_MSG_COMPOSER (composer), atoi (state));
}

static void
menu_security_pgp_encrypt_cb (BonoboUIComponent           *component,
			      const char                  *path,
			      Bonobo_UIComponent_EventType type,
			      const char                  *state,
			      gpointer                     composer)

{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_pgp_encrypt (E_MSG_COMPOSER (composer), atoi (state));
}

static void
menu_security_smime_sign_cb (BonoboUIComponent           *component,
			     const char                  *path,
			     Bonobo_UIComponent_EventType type,
			     const char                  *state,
			     gpointer                     composer)

{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_smime_sign (E_MSG_COMPOSER (composer), atoi (state));
}

static void
menu_security_smime_encrypt_cb (BonoboUIComponent           *component,
				const char                  *path,
				Bonobo_UIComponent_EventType type,
				const char                  *state,
				gpointer                     composer)

{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_smime_encrypt (E_MSG_COMPOSER (composer), atoi (state));
}


static void
menu_view_from_cb (BonoboUIComponent           *component,
		   const char                  *path,
		   Bonobo_UIComponent_EventType type,
		   const char                  *state,
		   gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_view_from (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_view_replyto_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_view_replyto (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_view_to_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_view_to (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_view_postto_cb (BonoboUIComponent           *component,
		      const char                  *path,
		      Bonobo_UIComponent_EventType type,
		      const char                  *state,
		      gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_view_postto (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_view_cc_cb (BonoboUIComponent           *component,
		 const char                  *path,
		 Bonobo_UIComponent_EventType type,
		 const char                  *state,
		 gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_view_cc (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_view_bcc_cb (BonoboUIComponent           *component,
		  const char                  *path,
		  Bonobo_UIComponent_EventType type,
		  const char                  *state,
		  gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	e_msg_composer_set_view_bcc (E_MSG_COMPOSER (user_data), atoi (state));
}

static void
menu_changed_charset_cb (BonoboUIComponent           *component,
			 const char                  *path,
			 Bonobo_UIComponent_EventType type,
			 const char                  *state,
			 gpointer                     user_data)
{
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	if (atoi (state)) {
		/* Charset menu names are "Charset-%s" where %s is the charset name */
		g_free (E_MSG_COMPOSER (user_data)->charset);
		E_MSG_COMPOSER (user_data)->charset = g_strdup (path + strlen ("Charset-"));
	}
}


static BonoboUIVerb verbs [] = {

	BONOBO_UI_VERB ("FileOpen", menu_file_open_cb),
	BONOBO_UI_VERB ("FileSave", menu_file_save_cb),
	BONOBO_UI_VERB ("FileSaveAs", menu_file_save_as_cb),
	BONOBO_UI_VERB ("FileSaveDraft", menu_file_save_draft_cb),
	BONOBO_UI_VERB ("FileClose", menu_file_close_cb),
	
	BONOBO_UI_VERB ("FileAttach", menu_file_add_attachment_cb),
	
	BONOBO_UI_VERB ("FileSend", menu_file_send_cb),
	
	BONOBO_UI_VERB ("DeleteAll", menu_edit_delete_all_cb),
	
	BONOBO_UI_VERB_END
};

static EPixmap pixcache [] = {
	E_PIXMAP ("/Toolbar/FileAttach", "stock_attach", E_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/FileSend", "stock_mail-send", E_ICON_SIZE_LARGE_TOOLBAR),
	
/*	E_PIXMAP ("/menu/Insert/FileAttach", "stock_attach", E_ICON_SIZE_LARGE_TOOLBAR), */
	E_PIXMAP ("/commands/FileSend", "stock_mail-send", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FileSave", "stock_save", E_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FileSaveAs", "stock_save-as", E_ICON_SIZE_MENU),
	
	E_PIXMAP_END
};


static void
signature_activate_cb (GtkWidget *menu, EMsgComposer *composer)
{
	GtkWidget *active;
	ESignature *sig;
	
	active = gtk_menu_get_active (GTK_MENU (menu));
	sig = g_object_get_data ((GObject *) active, "sig");
	
	if (composer->signature != sig) {
		composer->signature = sig;
		e_msg_composer_show_sig_file (composer);
	}
}

static void
signature_added (ESignatureList *signatures, ESignature *sig, EMsgComposer *composer)
{
	GtkWidget *menu, *item;
	
	menu = gtk_option_menu_get_menu (composer->sig_menu);
	
	if (sig->autogen)
		item = gtk_menu_item_new_with_label (_("Autogenerated"));
	else
		item = gtk_menu_item_new_with_label (sig->name);
	g_object_set_data ((GObject *) item, "sig", sig);
	gtk_widget_show (item);
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
signature_removed (ESignatureList *signatures, ESignature *sig, EMsgComposer *composer)
{
	GtkWidget *menu;
	ESignature *cur;
	GList *items;
	
	if (composer->signature == sig) {
		composer->signature = NULL;
		e_msg_composer_show_sig_file (composer);
	}
	
	menu = gtk_option_menu_get_menu (composer->sig_menu);
	items = GTK_MENU_SHELL (menu)->children;
	while (items != NULL) {
		cur = g_object_get_data (items->data, "sig");
		if (cur == sig) {
			gtk_widget_destroy (items->data);
			break;
		}
		items = items->next;
	}
}

static void
menu_item_set_label (GtkMenuItem *item, const char *label)
{
	GtkWidget *widget;
	
	widget = gtk_bin_get_child ((GtkBin *) item);
	if (GTK_IS_LABEL (widget))
		gtk_label_set_text ((GtkLabel *) widget, label);
}

static void
signature_changed (ESignatureList *signatures, ESignature *sig, EMsgComposer *composer)
{
	GtkWidget *menu;
	ESignature *cur;
	GList *items;
	
	menu = gtk_option_menu_get_menu (composer->sig_menu);
	items = GTK_MENU_SHELL (menu)->children;
	while (items != NULL) {
		cur = g_object_get_data (items->data, "sig");
		if (cur == sig) {
			menu_item_set_label (items->data, sig->name);
			break;
		}
		items = items->next;
	}
}

static void
sig_select_item (EMsgComposer *composer)
{
	ESignature *cur;
	GtkWidget *menu;
	GList *items;
	int i = 0;
	
	if (!composer->signature) {
		gtk_option_menu_set_history (composer->sig_menu, 0);
		return;
	}
	
	menu = gtk_option_menu_get_menu (composer->sig_menu);
	items = GTK_MENU_SHELL (menu)->children;
	while (items != NULL) {
		cur = g_object_get_data ((GObject *) items->data, "sig");
		if (cur == composer->signature) {
			gtk_option_menu_set_history (composer->sig_menu, i);
			return;
		}
		items = items->next;
		i++;
	}
}

static void
setup_signatures_menu (EMsgComposer *composer)
{
	GtkWidget *hbox, *hspace, *label;
	ESignatureList *signatures;
	GtkWidget *menu, *item;
	ESignature *sig;
	EIterator *it;
	
	hbox = e_msg_composer_hdrs_get_from_hbox (E_MSG_COMPOSER_HDRS (composer->hdrs));
	
	label = gtk_label_new (_("Signature:"));
	gtk_widget_show (label);
	
	composer->sig_menu = (GtkOptionMenu *) gtk_option_menu_new ();
	
	gtk_box_pack_end_defaults (GTK_BOX (hbox), (GtkWidget *) composer->sig_menu);
	gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	hspace = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hspace);
	gtk_box_pack_start (GTK_BOX (hbox), hspace, FALSE, FALSE, 0);
	
	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	gtk_option_menu_set_menu (composer->sig_menu, menu);
	
	item = gtk_menu_item_new_with_label (_("None"));
	gtk_widget_show (item);
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	
	signatures = mail_config_get_signatures ();
	it = e_list_get_iterator ((EList *) signatures);
	
	while (e_iterator_is_valid (it)) {
		sig = (ESignature *) e_iterator_get (it);
		signature_added (signatures, sig, composer);
		e_iterator_next (it);
	}
	
	g_object_unref (it);
	
	g_signal_connect (menu, "selection-done", G_CALLBACK (signature_activate_cb), composer);

	gtk_widget_show ((GtkWidget *) composer->sig_menu);
	
	composer->sig_added_id = g_signal_connect (signatures, "signature-added", G_CALLBACK (signature_added), composer);
	composer->sig_removed_id = g_signal_connect (signatures, "signature-removed", G_CALLBACK (signature_removed), composer);
	composer->sig_changed_id = g_signal_connect (signatures, "signature-changed", G_CALLBACK (signature_changed), composer);
}

static void
setup_ui (EMsgComposer *composer)
{
	BonoboUIContainer *container;
	gboolean hide_smime;
	char *charset;
	
	container = bonobo_window_get_ui_container (BONOBO_WINDOW (composer));
	
	composer->uic = bonobo_ui_component_new_default ();
	/* FIXME: handle bonobo exceptions */
	bonobo_ui_component_set_container (composer->uic, bonobo_object_corba_objref (BONOBO_OBJECT (container)), NULL);
	
	bonobo_ui_component_add_verb_list_with_data (composer->uic, verbs, composer);
	
	bonobo_ui_component_freeze (composer->uic, NULL);
	
	bonobo_ui_util_set_ui (composer->uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-message-composer.xml",
			       "evolution-message-composer", NULL);
	
	e_pixmaps_update (composer->uic, pixcache);
	
	/* Populate the Charset Encoding menu and default it to whatever the user
	   chose as his default charset in the mailer */
	charset = composer_get_default_charset_setting ();
	e_charset_picker_bonobo_ui_populate (composer->uic, "/menu/Edit/EncodingPlaceholder",
					     charset,
					     menu_changed_charset_cb,
					     composer);
	g_free (charset);
	
	/* Format -> HTML */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/FormatHtml",
		"state", composer->send_html ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "FormatHtml",
		menu_format_html_cb, composer);
	
	/* View/From */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewFrom",
		"state", composer->view_from ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewFrom",
		menu_view_from_cb, composer);
	
	/* View/ReplyTo */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewReplyTo",
		"state", composer->view_replyto ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewReplyTo",
		menu_view_replyto_cb, composer);
	
	/* View/To */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewTo",
		"state", composer->view_to ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewTo",
		menu_view_to_cb, composer);
	
	/* View/PostTo */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewPostTo",
		"state", composer->view_postto ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewPostTo",
		menu_view_postto_cb, composer);
	
	/* View/CC */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewCC",
		"state", composer->view_cc ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewCC",
		menu_view_cc_cb, composer);
	
	/* View/BCC */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewBCC",
		"state", composer->view_bcc ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewBCC",
		menu_view_bcc_cb, composer);
	
	/* Security -> PGP Sign */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/SecurityPGPSign",
		"state", composer->pgp_sign ? "1" : "0", NULL);
	
	bonobo_ui_component_add_listener (
		composer->uic, "SecurityPGPSign",
		menu_security_pgp_sign_cb, composer);
	
	/* Security -> PGP Encrypt */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/SecurityPGPEncrypt",
		"state", composer->pgp_encrypt ? "1" : "0", NULL);
	
	bonobo_ui_component_add_listener (
		composer->uic, "SecurityPGPEncrypt",
		menu_security_pgp_encrypt_cb, composer);
	
#if defined(HAVE_NSS) && defined(SMIME_SUPPORTED)
	hide_smime = FALSE;
#else
	hide_smime = TRUE;
#endif
	
	/* Security -> S/MIME Sign */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/SecuritySMimeSign",
		"state", composer->smime_sign ? "1" : "0", NULL);
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/SecuritySMimeSign",
		"hidden", hide_smime ? "1" : "0", NULL);
	
	bonobo_ui_component_add_listener (
		composer->uic, "SecuritySMimeSign",
		menu_security_smime_sign_cb, composer);
	
	/* Security -> S/MIME Encrypt */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/SecuritySMimeEncrypt",
		"state", composer->smime_encrypt ? "1" : "0", NULL);
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/SecuritySMimeEncrypt",
		"hidden", hide_smime ? "1" : "0", NULL);
	
	bonobo_ui_component_add_listener (
		composer->uic, "SecuritySMimeEncrypt",
		menu_security_smime_encrypt_cb, composer);
	
	/* View -> Attachments */
	bonobo_ui_component_add_listener (
		composer->uic, "ViewAttach",
		menu_view_attachments_activate_cb, composer);
	
	bonobo_ui_component_thaw (composer->uic, NULL);

	/* Create the UIComponent for the non-control entries */

	composer->entry_uic = bonobo_ui_component_new_default ();
}


/* Miscellaneous callbacks.  */

static void
attachment_bar_changed_cb (EMsgComposerAttachmentBar *bar,
			   void *data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (data);

	guint attachment_num = e_msg_composer_attachment_bar_get_num_attachments (
		E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar));
	if (attachment_num) {
		gchar *num_text = g_strdup_printf (
			ngettext ("<b>%d</b> File Attached", "<b>%d</b> Files Attached", attachment_num),
			attachment_num);
		gtk_label_set_markup (GTK_LABEL (composer->attachment_expander_num),
				      num_text);
		g_free (num_text);

		gtk_widget_show (composer->attachment_expander_icon);
		
	} else {
		gtk_label_set_text (GTK_LABEL (composer->attachment_expander_num), "");
		gtk_widget_hide (composer->attachment_expander_icon);
	}
	
	
	/* Mark the composer as changed so it prompts about unsaved
           changes on close */
	e_msg_composer_set_changed (composer);
}

static void
attachment_expander_activate_cb (EExpander *expander,
				 void      *data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (data);
	gboolean show = e_expander_get_expanded (expander);
	
	/* Update the expander label */
	if (show)
		gtk_label_set_text_with_mnemonic (GTK_LABEL (composer->attachment_expander_label),
						  _("Hide _Attachment Bar (drop attachments here)"));
	else
		gtk_label_set_text_with_mnemonic (GTK_LABEL (composer->attachment_expander_label),
						  _("Show _Attachment Bar (drop attachments here)"));
	
	/* Update the GUI.  */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewAttach",
		"state", show ? "1" : "0", NULL);
}

static void
subject_changed_cb (EMsgComposerHdrs *hdrs,
		    gchar *subject,
		    void *data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	gtk_window_set_title (GTK_WINDOW (composer), subject[0] ? subject : _("Compose a message"));
}

static void
hdrs_changed_cb (EMsgComposerHdrs *hdrs,
		 void *data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	/* Mark the composer as changed so it prompts about unsaved changes on close */
	e_msg_composer_set_changed (composer);
}

enum {
	UPDATE_AUTO_CC,
	UPDATE_AUTO_BCC,
};

static void
update_auto_recipients (EMsgComposerHdrs *hdrs, int mode, const char *auto_addrs)
{
	EDestination *dest, **destv = NULL;
	CamelInternetAddress *iaddr;
	GList *list, *tail, *node;
	int i, n = 0;
	
	tail = list = NULL;
	
	if (auto_addrs) {
		iaddr = camel_internet_address_new ();
		if (camel_address_decode (CAMEL_ADDRESS (iaddr), auto_addrs) != -1) {
			for (i = 0; i < camel_address_length (CAMEL_ADDRESS (iaddr)); i++) {
				const char *name, *addr;
				
				if (!camel_internet_address_get (iaddr, i, &name, &addr))
					continue;
				
				dest = e_destination_new ();
				e_destination_set_auto_recipient (dest, TRUE);
				
				if (name)
					e_destination_set_name (dest, name);
				
				if (addr)
					e_destination_set_email (dest, addr);
				
				node = g_list_alloc ();
				node->data = dest;
				node->next = NULL;
				
				if (tail) {
					node->prev = tail;
					tail->next = node;
				} else {
					node->prev = NULL;
					list = node;
				}
				
				tail = node;
				n++;
			}
		}
		
		camel_object_unref (iaddr);
	}
	
	switch (mode) {
	case UPDATE_AUTO_CC:
		destv = e_msg_composer_hdrs_get_cc (hdrs);
		break;
	case UPDATE_AUTO_BCC:
		destv = e_msg_composer_hdrs_get_bcc (hdrs);
		break;
	default:
		g_assert_not_reached ();
	}
	
	if (destv) {
		for (i = 0; destv[i]; i++) {
			if (!e_destination_is_auto_recipient (destv[i])) {
				node = g_list_alloc ();
				node->data = e_destination_copy (destv[i]);
				node->next = NULL;
				
				if (tail) {
					node->prev = tail;
					tail->next = node;
				} else {
					node->prev = NULL;
					list = node;
				}
				
				tail = node;
				n++;
			}
		}
		
		e_destination_freev (destv);
	}
	
	destv = destination_list_to_vector_sized (list, n);
	g_list_free (list);
	
	switch (mode) {
	case UPDATE_AUTO_CC:
		e_msg_composer_hdrs_set_cc (hdrs, destv);
		break;
	case UPDATE_AUTO_BCC:
		e_msg_composer_hdrs_set_bcc (hdrs, destv);
		break;
	default:
		g_assert_not_reached ();
	}
	
	e_destination_freev (destv);
}

static void
from_changed_cb (EMsgComposerHdrs *hdrs, void *data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	if (hdrs->account) {
		EAccount *account = hdrs->account;
		
		e_msg_composer_set_pgp_sign (composer,
					     account->pgp_always_sign &&
					     (!account->pgp_no_imip_sign || !composer->mime_type ||
					      g_ascii_strncasecmp (composer->mime_type, "text/calendar", 13) != 0));
		e_msg_composer_set_smime_sign (composer, account->smime_sign_default);
		e_msg_composer_set_smime_encrypt (composer, account->smime_encrypt_default);
		update_auto_recipients (hdrs, UPDATE_AUTO_CC, account->always_cc ? account->cc_addrs : NULL);
		update_auto_recipients (hdrs, UPDATE_AUTO_BCC, account->always_bcc ? account->bcc_addrs : NULL);
	} else {
		update_auto_recipients (hdrs, UPDATE_AUTO_CC, NULL);
		update_auto_recipients (hdrs, UPDATE_AUTO_BCC, NULL);
	}
	
	set_editor_signature (composer);
	e_msg_composer_show_sig_file (composer);
}


/* GObject methods.  */

static void
composer_finalise (GObject *object)
{
	EMsgComposer *composer;

	composer = E_MSG_COMPOSER (object);
	
	if (composer->extra_hdr_names) {
		int i;
		
		for (i = 0; i < composer->extra_hdr_names->len; i++) {
			g_free (composer->extra_hdr_names->pdata[i]);
			g_free (composer->extra_hdr_values->pdata[i]);
		}
		g_ptr_array_free (composer->extra_hdr_names, TRUE);
		g_ptr_array_free (composer->extra_hdr_values, TRUE);
	}
	
	e_msg_composer_clear_inlined_table (composer);
	g_hash_table_destroy (composer->inline_images);
	g_hash_table_destroy (composer->inline_images_by_url);
	
	g_free (composer->charset);
	g_free (composer->mime_type);
	g_free (composer->mime_body);
	
	if (composer->redirect)
		camel_object_unref (composer->redirect);
	
	if (G_OBJECT_CLASS (parent_class)->finalize != NULL)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
composer_dispose(GObject *object)
{
	/* When destroy() is called, the contents of the window
	 * (including the remote editor control) will already have
	 * been destroyed, so we have to do this here.
	 */
	autosave_manager_unregister (am, E_MSG_COMPOSER (object));
	if (G_OBJECT_CLASS (parent_class)->dispose != NULL)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* GtkObject methods */
static void
destroy (GtkObject *object)
{
	EMsgComposer *composer;
	CORBA_Environment ev;
	ESignatureList *signatures;
	
	composer = E_MSG_COMPOSER (object);
	
	CORBA_exception_init (&ev);
	
	if (composer->uic) {
		bonobo_object_unref (BONOBO_OBJECT (composer->uic));
		composer->uic = NULL;
	}

	if (composer->entry_uic) {
		bonobo_object_unref (BONOBO_OBJECT (composer->entry_uic));
		composer->entry_uic = NULL;
	}
	
	/* FIXME?  I assume the Bonobo widget will get destroyed
           normally?  */
	if (composer->address_dialog != NULL) {
		gtk_widget_destroy (composer->address_dialog);
		composer->address_dialog = NULL;
	}
	if (composer->hdrs != NULL) {
		gtk_widget_destroy (composer->hdrs);
		composer->hdrs = NULL;
	}
	
	if (composer->notify_id) {
		GConfClient *gconf = gconf_client_get_default ();
		gconf_client_notify_remove (gconf, composer->notify_id);
		composer->notify_id = 0;
		g_object_unref (gconf);
	}		

	if (composer->persist_stream_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_stream_interface, &ev);
		CORBA_Object_release (composer->persist_stream_interface, &ev);
		composer->persist_stream_interface = CORBA_OBJECT_NIL;
	}
	
	if (composer->persist_file_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_file_interface, &ev);
		CORBA_Object_release (composer->persist_file_interface, &ev);
		composer->persist_file_interface = CORBA_OBJECT_NIL;
	}
	
	if (composer->editor_engine != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->editor_engine, &ev);
		CORBA_Object_release (composer->editor_engine, &ev);
		composer->editor_engine = CORBA_OBJECT_NIL;
	}
	
	CORBA_exception_free (&ev);
	
	if (composer->editor_listener) {
		bonobo_object_unref (composer->editor_listener);
		composer->editor_listener = NULL;
	}
	
	signatures = mail_config_get_signatures ();
	
	if (composer->sig_added_id != 0) {
		g_signal_handler_disconnect (signatures, composer->sig_added_id);
		composer->sig_added_id = 0;
	}
	
	if (composer->sig_removed_id != 0) {
		g_signal_handler_disconnect (signatures, composer->sig_removed_id);
		composer->sig_removed_id = 0;
	}
	
	if (composer->sig_changed_id != 0) {
		g_signal_handler_disconnect (signatures, composer->sig_changed_id);
		composer->sig_changed_id = 0;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static int
delete_event (GtkWidget *widget,
	      GdkEventAny *event)
{
	do_exit (E_MSG_COMPOSER (widget));
	
	return TRUE;
}

static void
attach_message(EMsgComposer *composer, CamelMimeMessage *msg)
{
	CamelMimePart *mime_part;
	const char *subject;

	mime_part = camel_mime_part_new();
	camel_mime_part_set_disposition(mime_part, "inline");
	subject = camel_mime_message_get_subject(msg);
	if (subject) {
		char *desc = g_strdup_printf(_("Attached message - %s"), subject);

		camel_mime_part_set_description(mime_part, desc);
		g_free(desc);
	} else
		camel_mime_part_set_description(mime_part, _("Attached message"));

	camel_medium_set_content_object((CamelMedium *)mime_part, (CamelDataWrapper *)msg);
	camel_mime_part_set_content_type(mime_part, "message/rfc822");
	e_msg_composer_attachment_bar_attach_mime_part(E_MSG_COMPOSER_ATTACHMENT_BAR(composer->attachment_bar), mime_part);
	camel_object_unref(mime_part);
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

static void
drop_action(EMsgComposer *composer, GdkDragContext *context, guint32 action, GtkSelectionData *selection, guint info, guint time)
{
	char *tmp, *str, **urls;
	CamelMimePart *mime_part;
	CamelStream *stream;
	CamelURL *url;
	CamelMimeMessage *msg;
	char *content_type;
	int i, success=FALSE, delete=FALSE;

	switch (info) {
	case DND_TYPE_MESSAGE_RFC822:
		d(printf ("dropping a message/rfc822\n"));
		/* write the message(s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, selection->data, selection->length);
		camel_stream_reset (stream);
		
		msg = camel_mime_message_new ();
		if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg, stream) != -1) {
			attach_message(composer, msg);
			success = TRUE;
			delete = action == GDK_ACTION_MOVE;
		}

		camel_object_unref(msg);
		camel_object_unref(stream);
		break;
	case DND_TYPE_TEXT_URI_LIST:
	case DND_TYPE_NETSCAPE_URL:
		d(printf ("dropping a text/uri-list\n"));
		tmp = g_strndup (selection->data, selection->length);
		urls = g_strsplit (tmp, "\n", 0);
		g_free (tmp);
		
		for (i = 0; urls[i] != NULL; i++) {
			str = g_strstrip (urls[i]);
			if (urls[i][0] == '#') {
				g_free(str);
				continue;
			}

			if (!g_ascii_strncasecmp (str, "mailto:", 7)) {
				handle_mailto (composer, str);
				g_free (str);
			} else {
				url = camel_url_new (str, NULL);
				g_free (str);

				if (url == NULL)
					continue;

				if (!g_ascii_strcasecmp (url->protocol, "file"))
					e_msg_composer_attachment_bar_attach
						(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
					 	url->path);

				camel_url_free (url);
			}
		}
		
		g_free (urls);
		success = TRUE;
		break;
	case DND_TYPE_TEXT_VCARD:
	case DND_TYPE_TEXT_CALENDAR:
		content_type = gdk_atom_name (selection->type);
		d(printf ("dropping a %s\n", content_type));
		
		mime_part = camel_mime_part_new ();
		camel_mime_part_set_content (mime_part, selection->data, selection->length, content_type);
		camel_mime_part_set_disposition (mime_part, "inline");
		
		e_msg_composer_attachment_bar_attach_mime_part
			(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
			 mime_part);
		
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

		uids = g_ptr_array_new();

		inptr = selection->data;
		inend = selection->data + selection->length;
		while (inptr < inend) {
			char *start = inptr;

			while (inptr < inend && *inptr)
				inptr++;

			if (start > (char *)selection->data)
				g_ptr_array_add(uids, g_strndup(start, inptr-start));

			inptr++;
		}

		if (uids->len > 0) {
			folder = mail_tool_uri_to_folder(selection->data, 0, &ex);
			if (folder) {
				if (uids->len == 1) {
					msg = camel_folder_get_message(folder, uids->pdata[0], &ex);
					if (msg == NULL)
						goto fail;
					attach_message(composer, msg);
				} else {
					CamelMultipart *mp = camel_multipart_new();
					char *desc;

					camel_data_wrapper_set_mime_type((CamelDataWrapper *)mp, "multipart/digest");
					camel_multipart_set_boundary(mp, NULL);
					for (i=0;i<uids->len;i++) {
						msg = camel_folder_get_message(folder, uids->pdata[i], &ex);
						if (msg) {
							mime_part = camel_mime_part_new();
							camel_mime_part_set_disposition(mime_part, "inline");
							camel_medium_set_content_object((CamelMedium *)mime_part, (CamelDataWrapper *)msg);
							camel_mime_part_set_content_type(mime_part, "message/rfc822");
							camel_multipart_add_part(mp, mime_part);
							camel_object_unref(mime_part);
							camel_object_unref(msg);
						} else {
							camel_object_unref(mp);
							goto fail;
						}
					}
					mime_part = camel_mime_part_new();
					camel_medium_set_content_object((CamelMedium *)mime_part, (CamelDataWrapper *)mp);
					/* translators, this count will always be >1 */
					desc = g_strdup_printf(ngettext("Attached message", "%d attached messages", uids->len), uids->len);
					camel_mime_part_set_description(mime_part, desc);
					g_free(desc);
					e_msg_composer_attachment_bar_attach_mime_part(E_MSG_COMPOSER_ATTACHMENT_BAR(composer->attachment_bar), mime_part);
					camel_object_unref(mime_part);
					camel_object_unref(mp);
				}
				success = TRUE;
				delete = action == GDK_ACTION_MOVE;
			fail:
				if (camel_exception_is_set(&ex)) {
					char *name;

					camel_object_get(folder, NULL, CAMEL_FOLDER_NAME, &name, NULL);
					e_error_run((GtkWindow *)composer, "mail-composer:attach-nomessages",
						    name?name:(char *)selection->data, camel_exception_get_description(&ex), NULL);
					camel_object_free(folder, CAMEL_FOLDER_NAME, name);
				}
				camel_object_unref(folder);
			} else {
				e_error_run((GtkWindow *)composer, "mail-composer:attach-nomessages",
					    selection->data, camel_exception_get_description(&ex), NULL);
			}

			camel_exception_clear(&ex);
		}

		g_ptr_array_free(uids, TRUE);

		break; }
	default:
		d(printf ("dropping an unknown\n"));
		break;
	}

	printf("Drag finished, success %d delete %d\n", success, delete);

	gtk_drag_finish(context, success, delete, time);
}

static void
drop_popup_copy(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_data *m = data;
	drop_action(m->composer, m->context, GDK_ACTION_COPY, m->selection, m->info, m->time);
}

static void
drop_popup_move(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_data *m = data;
	drop_action(m->composer, m->context, GDK_ACTION_MOVE, m->selection, m->info, m->time);
}

static void
drop_popup_cancel(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_data *m = data;
	gtk_drag_finish(m->context, FALSE, FALSE, m->time);
}

static EPopupItem drop_popup_menu[] = {
	{ E_POPUP_ITEM, "00.emc.02", N_("_Copy"), drop_popup_copy, NULL, "stock_mail-copy", 0 },
	{ E_POPUP_ITEM, "00.emc.03", N_("_Move"), drop_popup_move, NULL, "stock_mail-move", 0 },
	{ E_POPUP_BAR, "10.emc" },
	{ E_POPUP_ITEM, "99.emc.00", N_("Cancel _Drag"), drop_popup_cancel, NULL, NULL, 0 },
};

static void
drop_popup_free(EPopup *ep, GSList *items, void *data)
{
	struct _drop_data *m = data;

	g_slist_free(items);

	g_object_unref(m->context);
	g_object_unref(m->composer);
	g_free(m->selection->data);
	g_free(m->selection);
	g_free(m);
}

static void
drag_data_received (EMsgComposer *composer, GdkDragContext *context,
		    int x, int y, GtkSelectionData *selection,
		    guint info, guint time)
{
	if (selection->data == NULL || selection->length == -1)
		return;

	if (context->action == GDK_ACTION_ASK) {
		EMPopup *emp;
		GSList *menus = NULL;
		GtkMenu *menu;
		int i;
		struct _drop_data *m;

		m = g_malloc0(sizeof(*m));
		m->context = context;
		g_object_ref(context);
		m->composer = composer;
		g_object_ref(composer);
		m->action = context->action;
		m->info = info;
		m->time = time;
		m->selection = g_malloc0(sizeof(*m->selection));
		m->selection->data = g_malloc(selection->length);
		memcpy(m->selection->data, selection->data, selection->length);
		m->selection->length = selection->length;

		emp = em_popup_new("org.gnome.mail.composer.popup.drop");
		for (i=0;i<sizeof(drop_popup_menu)/sizeof(drop_popup_menu[0]);i++)
			menus = g_slist_append(menus, &drop_popup_menu[i]);

		e_popup_add_items((EPopup *)emp, menus, drop_popup_free, m);
		menu = e_popup_create_menu_once((EPopup *)emp, NULL, 0);
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, time);
	} else {
		drop_action(composer, context, context->action, selection, info, time);
	}
}

static gboolean
drag_motion(GObject *o, GdkDragContext *context, gint x, gint y, guint time, EMsgComposer *composer)
{
	GList *targets;
	GdkDragAction action, actions = 0;

	for (targets = context->targets; targets; targets = targets->next) {
		int i;

		for (i=0;i<sizeof(drag_info)/sizeof(drag_info[0]);i++)
			if (targets->data == (void *)drag_info[i].atom)
				actions |= drag_info[i].actions;
	}

	actions &= context->actions;
	action = context->suggested_action;
	/* we default to copy */
	if (action == GDK_ACTION_ASK && (actions & (GDK_ACTION_MOVE|GDK_ACTION_COPY)) != (GDK_ACTION_MOVE|GDK_ACTION_COPY))
		action = GDK_ACTION_COPY;

	gdk_drag_status(context, action, time);

	return action != 0;
}

static void
class_init (EMsgComposerClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GObjectClass *gobject_class;
	int i;

	for (i=0;i<sizeof(drag_info)/sizeof(drag_info[0]);i++)
		drag_info[i].atom = gdk_atom_intern(drag_info[i].target, FALSE);

	gobject_class = G_OBJECT_CLASS(klass);
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	
	gobject_class->finalize = composer_finalise;
	gobject_class->dispose = composer_dispose;
	object_class->destroy = destroy;
	widget_class->delete_event = delete_event;
	
	parent_class = g_type_class_ref(bonobo_window_get_type ());
	
	signals[SEND] =
		g_signal_new ("send",
			      E_TYPE_MSG_COMPOSER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMsgComposerClass, send),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[SAVE_DRAFT] =
		g_signal_new ("save-draft",
			      E_TYPE_MSG_COMPOSER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMsgComposerClass, save_draft),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);
}

static void
init (EMsgComposer *composer)
{
	composer->uic                      = NULL;
	
	composer->hdrs                     = NULL;
	composer->extra_hdr_names          = g_ptr_array_new ();
	composer->extra_hdr_values         = g_ptr_array_new ();
	
	composer->focused_entry            = NULL;
	
	composer->editor                   = NULL;
	
	composer->address_dialog           = NULL;
	
	composer->attachment_bar           = NULL;
	composer->attachment_scrolled_window = NULL;
	
	composer->persist_file_interface   = CORBA_OBJECT_NIL;
	composer->persist_stream_interface = CORBA_OBJECT_NIL;
	
	composer->editor_engine            = CORBA_OBJECT_NIL;
	composer->inline_images            = g_hash_table_new (g_str_hash, g_str_equal);
	composer->inline_images_by_url     = g_hash_table_new (g_str_hash, g_str_equal);
	composer->current_images           = NULL;
	
	composer->attachment_bar_visible   = FALSE;
	composer->send_html                = FALSE;
	composer->pgp_sign                 = FALSE;
	composer->pgp_encrypt              = FALSE;
	composer->smime_sign               = FALSE;
	composer->smime_encrypt            = FALSE;
	
	composer->has_changed              = FALSE;
	composer->autosaved                = FALSE;
	
	composer->redirect                 = FALSE;
	
	composer->charset                  = NULL;
	
	composer->enable_autosave          = TRUE;
	composer->autosave_file            = NULL;
	composer->autosave_fd              = -1;
}


GtkType
e_msg_composer_get_type (void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EMsgComposerClass),
			NULL, NULL,
			(GClassInitFunc) class_init,
			NULL, NULL,
			sizeof (EMsgComposer),
			0,
			(GInstanceInitFunc) init,
		};
		
		type = g_type_register_static (bonobo_window_get_type (), "EMsgComposer", &info, 0);
	}
	
	return type;
}

static void
e_msg_composer_load_config (EMsgComposer *composer, int visible_mask)
{
	GConfClient *gconf;
	
	gconf = gconf_client_get_default ();
	
	composer->view_from = gconf_client_get_bool (
		gconf, "/apps/evolution/mail/composer/view/From", NULL);
	composer->view_replyto = gconf_client_get_bool (
		gconf, "/apps/evolution/mail/composer/view/ReplyTo", NULL);
	composer->view_to = gconf_client_get_bool (
		gconf, "/apps/evolution/mail/composer/view/To", NULL);
	composer->view_postto = gconf_client_get_bool (
		gconf, "/apps/evolution/mail/composer/view/PostTo", NULL);
	composer->view_cc = gconf_client_get_bool ( 
		gconf, "/apps/evolution/mail/composer/view/Cc", NULL);
	composer->view_bcc = gconf_client_get_bool (
		gconf, "/apps/evolution/mail/composer/view/Bcc", NULL);
	composer->view_subject = gconf_client_get_bool (
		gconf, "/apps/evolution/mail/composer/view/Subject", NULL);
	
	/* if we're mailing, you cannot disable to so it should appear checked */
	if (visible_mask & E_MSG_COMPOSER_VISIBLE_TO)
		composer->view_to = TRUE;
	else
		composer->view_to = FALSE;
	
	/* ditto for post-to */
	if (visible_mask & E_MSG_COMPOSER_VISIBLE_POSTTO)
		composer->view_postto = TRUE;
	else
		composer->view_postto = FALSE;
	
	/* we set these to false initially if we're posting */
	if (!(visible_mask & E_MSG_COMPOSER_VISIBLE_CC))
		composer->view_cc = FALSE;
	
	if (!(visible_mask & E_MSG_COMPOSER_VISIBLE_BCC))
		composer->view_bcc = FALSE;
	
	g_object_unref (gconf);
}

static int
e_msg_composer_get_visible_flags (EMsgComposer *composer)
{
	int flags = 0;
	
	if (composer->view_from)
		flags |= E_MSG_COMPOSER_VISIBLE_FROM;
	if (composer->view_replyto)
		flags |= E_MSG_COMPOSER_VISIBLE_REPLYTO;
	if (composer->view_to)
		flags |= E_MSG_COMPOSER_VISIBLE_TO;
	if (composer->view_postto)
		flags |= E_MSG_COMPOSER_VISIBLE_POSTTO;
	if (composer->view_cc)
		flags |= E_MSG_COMPOSER_VISIBLE_CC;
	if (composer->view_bcc)
		flags |= E_MSG_COMPOSER_VISIBLE_BCC;
	if (composer->view_subject)
		flags |= E_MSG_COMPOSER_VISIBLE_SUBJECT;
	
	/*
	 * Until we have a GUI way, lets make sure that
	 * even if the user screws up, we will do the right
	 * thing (screws up == edit the config file manually
	 * and screw up).
	 */
	flags |= E_MSG_COMPOSER_VISIBLE_SUBJECT;	
	return flags;
}


static void
map_default_cb (EMsgComposer *composer, gpointer user_data)
{
	GtkWidget *widget;
	BonoboControlFrame *cf;
	Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;
	CORBA_Environment ev;
	const char *subject;
	char *text;

	/* If the 'To:' field is empty, focus it (This is ridiculously complicated) */
	
	widget = e_msg_composer_hdrs_get_to_entry (E_MSG_COMPOSER_HDRS (composer->hdrs));
	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));
	pb = bonobo_control_frame_get_control_property_bag (cf, NULL);
	text = bonobo_pbclient_get_string (pb, "text", NULL);
	bonobo_object_release_unref (pb, NULL);
	
	if (!text || text[0] == '\0') {
		bonobo_control_frame_control_activate (cf);
		
		g_free (text);
		return;
	}
	g_free (text);
	
	/* If not, check the subject field */
	
	subject = e_msg_composer_hdrs_get_subject (E_MSG_COMPOSER_HDRS (composer->hdrs));
	
	if (!subject || subject[0] == '\0') {
		widget = e_msg_composer_hdrs_get_subject_entry (E_MSG_COMPOSER_HDRS (composer->hdrs));
		gtk_widget_grab_focus (widget);
		return;
	}
	
	/* Jump to the editor as a last resort. */
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "grab-focus", &ev);
	CORBA_exception_free (&ev);
}

static void
msg_composer_destroy_notify (void *data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (data);
	
	all_composers = g_slist_remove (all_composers, composer);
}

/* Verbs for non-control entries */
static BonoboUIVerb entry_verbs [] = {
	BONOBO_UI_VERB ("EditCut", menu_edit_cut_cb),
	BONOBO_UI_VERB ("EditCopy", menu_edit_copy_cb),
	BONOBO_UI_VERB ("EditPaste", menu_edit_paste_cb),
	BONOBO_UI_VERB ("EditSelectAll", menu_edit_select_all_cb),
	BONOBO_UI_VERB_END
};

/* All this snot is so that Cut/Copy/Paste work. */
static gboolean
composer_entry_focus_in_event_cb (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	EMsgComposer *composer = user_data;
	BonoboUIContainer *container;

	composer->focused_entry = widget;

	container = bonobo_window_get_ui_container (BONOBO_WINDOW (composer));
	bonobo_ui_component_set_container (composer->entry_uic, bonobo_object_corba_objref (BONOBO_OBJECT (container)), NULL);

	bonobo_ui_component_add_verb_list_with_data (composer->entry_uic, entry_verbs, composer);

	bonobo_ui_component_freeze (composer->entry_uic, NULL);

	bonobo_ui_util_set_ui (composer->entry_uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-composer-entries.xml",
			       "evolution-composer-entries", NULL);
	
	bonobo_ui_component_thaw (composer->entry_uic, NULL);
	
	return FALSE;
}

static gboolean
composer_entry_focus_out_event_cb (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	EMsgComposer *composer = user_data;
	
	g_assert (composer->focused_entry == widget);
	composer->focused_entry = NULL;

	bonobo_ui_component_unset_container (composer->entry_uic, NULL);
	
	return FALSE;
}

static void
setup_cut_copy_paste (EMsgComposer *composer)
{
	EMsgComposerHdrs *hdrs;
	GtkWidget *entry;
	
	hdrs = (EMsgComposerHdrs *) composer->hdrs;
	
	entry = e_msg_composer_hdrs_get_subject_entry (hdrs);
	g_signal_connect (entry, "focus_in_event", G_CALLBACK (composer_entry_focus_in_event_cb), composer);
	g_signal_connect (entry, "focus_out_event", G_CALLBACK (composer_entry_focus_out_event_cb), composer);
	
	entry = e_msg_composer_hdrs_get_reply_to_entry (hdrs);
	g_signal_connect (entry, "focus_in_event", G_CALLBACK (composer_entry_focus_in_event_cb), composer);
	g_signal_connect (entry, "focus_out_event", G_CALLBACK (composer_entry_focus_out_event_cb), composer);
}

static void
composer_settings_update (GConfClient *gconf, guint cnxn_id, GConfEntry *entry, gpointer data)
{
	gboolean bool;
	EMsgComposer *composer = data;

	bool = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/magic_smileys", NULL);
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor),
				    "MagicSmileys", TC_CORBA_boolean, bool,
				    NULL);

	bool = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/magic_links", NULL);
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor),
				    "MagicLinks", TC_CORBA_boolean, bool,
				    NULL);

	bool = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/inline_spelling", NULL);
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor),
				   "InlineSpelling", TC_CORBA_boolean, bool,
				   NULL);
}

static void
e_msg_composer_unrealize (GtkWidget *widget, gpointer data)
{
	EMsgComposer *composer = E_MSG_COMPOSER (widget);
	GConfClient *gconf;
	int width, height;

	gtk_window_get_size (GTK_WINDOW (composer), &width, &height);

	gconf = gconf_client_get_default ();
	gconf_client_set_int (gconf, "/apps/evolution/mail/composer/width", width, NULL);
	gconf_client_set_int (gconf, "/apps/evolution/mail/composer/height", height, NULL);
	g_object_unref (gconf);
}

static EMsgComposer *
create_composer (int visible_mask)
{
	EMsgComposer *composer;
	GtkWidget *vbox, *expander_hbox;
	Bonobo_Unknown editor_server;
	CORBA_Environment ev;
	GConfClient *gconf;
	int vis;
	GList *icon_list;
	BonoboControlFrame *control_frame;
	GdkPixbuf *attachment_pixbuf;
	
	composer = g_object_new (E_TYPE_MSG_COMPOSER, "win_name", _("Compose a message"), NULL);
	gtk_window_set_title ((GtkWindow *) composer, _("Compose a message"));
	
	all_composers = g_slist_prepend (all_composers, composer);
	
	g_signal_connect (composer, "destroy",
			  G_CALLBACK (msg_composer_destroy_notify),
			  NULL);

	icon_list = e_icon_factory_get_icon_list ("stock_mail-compose");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (composer), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	/* DND support */
	gtk_drag_dest_set (GTK_WIDGET (composer), GTK_DEST_DEFAULT_ALL,  drop_types, num_drop_types, GDK_ACTION_COPY|GDK_ACTION_ASK|GDK_ACTION_MOVE);
	g_signal_connect(composer, "drag_data_received", G_CALLBACK (drag_data_received), NULL);
	g_signal_connect(composer, "drag-motion", G_CALLBACK(drag_motion), composer);
	e_msg_composer_load_config (composer, visible_mask);
	
	setup_ui (composer);
	
	vbox = gtk_vbox_new (FALSE, 0);
	
	vis = e_msg_composer_get_visible_flags (composer);
	composer->hdrs = e_msg_composer_hdrs_new (composer->uic, visible_mask, vis);
	if (!composer->hdrs) {
		e_error_run (GTK_WINDOW (composer), "mail-composer:no-address-control", NULL);
		gtk_object_destroy (GTK_OBJECT (composer));
		return NULL;
	}
	
	gtk_box_set_spacing (GTK_BOX (vbox), 6);
	gtk_box_pack_start (GTK_BOX (vbox), composer->hdrs, FALSE, FALSE, 0);
	g_signal_connect (composer->hdrs, "subject_changed",
			  G_CALLBACK (subject_changed_cb), composer);
	g_signal_connect (composer->hdrs, "hdrs_changed",
			  G_CALLBACK (hdrs_changed_cb), composer);
	g_signal_connect (composer->hdrs, "from_changed",
			  G_CALLBACK (from_changed_cb), composer);
	gtk_widget_show (composer->hdrs);
	
	setup_signatures_menu (composer);

	from_changed_cb((EMsgComposerHdrs *)composer->hdrs, composer);

	/* Editor component.  */
	composer->editor = bonobo_widget_new_control (
		GNOME_GTKHTML_EDITOR_CONTROL_ID,
		bonobo_ui_component_get_container (composer->uic));
	if (!composer->editor) {
		e_error_run (GTK_WINDOW (composer), "mail-composer:no-editor-control", NULL);
		gtk_object_destroy (GTK_OBJECT (composer));
		return NULL;
	}

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (composer->editor));
	bonobo_control_frame_set_autoactivate (control_frame, TRUE);
	
	/* let the editor know which mode we are in */
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor), 
				    "FormatHTML", TC_CORBA_boolean, composer->send_html,
				    NULL);
	
	gconf = gconf_client_get_default ();
	composer_settings_update (gconf, 0, NULL, composer);
	gconf_client_add_dir (gconf, "/apps/evolution/mail/composer", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	composer->notify_id = gconf_client_notify_add (gconf, "/apps/evolution/mail/composer",
						       composer_settings_update, composer, NULL, NULL);
	gtk_window_set_default_size (GTK_WINDOW (composer),
				     gconf_client_get_int (gconf, "/apps/evolution/mail/composer/width", NULL),
				     gconf_client_get_int (gconf, "/apps/evolution/mail/composer/height", NULL));
	g_signal_connect (composer, "unrealize", G_CALLBACK (e_msg_composer_unrealize), NULL);
	g_object_unref (gconf);
	
	editor_server = bonobo_widget_get_objref (BONOBO_WIDGET (composer->editor));
	
	/* FIXME: handle exceptions */
	CORBA_exception_init (&ev);
	composer->persist_file_interface
		= Bonobo_Unknown_queryInterface (editor_server, "IDL:Bonobo/PersistFile:1.0", &ev);
	composer->persist_stream_interface
		= Bonobo_Unknown_queryInterface (editor_server, "IDL:Bonobo/PersistStream:1.0", &ev);
	CORBA_exception_free (&ev);
	
	gtk_box_pack_start (GTK_BOX (vbox), composer->editor, TRUE, TRUE, 0);
	
	/* Attachment editor, wrapped into an EScrollFrame.  It's
           hidden in an EExpander. */
	
	composer->attachment_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (composer->attachment_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (composer->attachment_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	composer->attachment_bar = e_msg_composer_attachment_bar_new (NULL);
	GTK_WIDGET_SET_FLAGS (composer->attachment_bar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (composer->attachment_scrolled_window),
			   composer->attachment_bar);
	gtk_widget_show (composer->attachment_bar);
	g_signal_connect (composer->attachment_bar, "changed",
			  G_CALLBACK (attachment_bar_changed_cb), composer);

	composer->attachment_expander_label =
		gtk_label_new_with_mnemonic (_("Show _Attachment Bar (drop attachments here)"));
	composer->attachment_expander_num = gtk_label_new ("");
	gtk_label_set_use_markup (GTK_LABEL (composer->attachment_expander_num), TRUE);
	gtk_misc_set_alignment (GTK_MISC (composer->attachment_expander_label), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (composer->attachment_expander_num), 1.0, 0.5);
	expander_hbox = gtk_hbox_new (FALSE, 0);
	
	attachment_pixbuf = e_icon_factory_get_icon ("stock_attach", E_ICON_SIZE_MENU);
	composer->attachment_expander_icon = gtk_image_new_from_pixbuf (attachment_pixbuf);
	gtk_misc_set_alignment (GTK_MISC (composer->attachment_expander_icon), 1, 0.5);
	gtk_widget_set_size_request (composer->attachment_expander_icon, 100, -1);
	g_object_unref (attachment_pixbuf);	

	gtk_box_pack_start (GTK_BOX (expander_hbox), composer->attachment_expander_label,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (expander_hbox), composer->attachment_expander_icon,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (expander_hbox), composer->attachment_expander_num,
			    TRUE, TRUE, 0);
	gtk_widget_show_all (expander_hbox);
	gtk_widget_hide (composer->attachment_expander_icon);

	composer->attachment_expander = e_expander_new ("");	
	e_expander_set_label_widget (E_EXPANDER (composer->attachment_expander), expander_hbox);
	
	gtk_container_add (GTK_CONTAINER (composer->attachment_expander),
			   composer->attachment_scrolled_window);
	gtk_box_pack_start (GTK_BOX (vbox), composer->attachment_expander,
			    FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (composer->attachment_expander);
	g_signal_connect_after (composer->attachment_expander, "activate",
				G_CALLBACK (attachment_expander_activate_cb), composer);
	
	bonobo_window_set_contents (BONOBO_WINDOW (composer), vbox);
	gtk_widget_show (vbox);
	
	/* If we show this widget earlier, we lose network transparency. i.e. the
	   component appears on the machine evo is running on, ignoring any DISPLAY
	   variable. */
	gtk_widget_show (composer->editor);
	
	e_msg_composer_show_attachments (composer, FALSE);
	prepare_engine (composer);
	if (composer->editor_engine == CORBA_OBJECT_NIL) {
		e_error_run (GTK_WINDOW (composer), "mail-composer:no-editor-control", NULL);
		gtk_object_destroy (GTK_OBJECT (composer));
		return NULL;
	}
	
	setup_cut_copy_paste (composer);

	g_signal_connect (composer, "map", (GCallback) map_default_cb, NULL);
	
	if (am == NULL)
		am = autosave_manager_new ();
	
	autosave_manager_register (am, composer);

	composer->has_changed = FALSE;
	
	return composer;
}

static void
set_editor_signature (EMsgComposer *composer)
{
	EAccountIdentity *id;
	
	g_return_if_fail (E_MSG_COMPOSER_HDRS (composer->hdrs)->account != NULL);
	
	id = E_MSG_COMPOSER_HDRS (composer->hdrs)->account->id;
	if (id->sig_uid)
		composer->signature = mail_config_get_signature_by_uid (id->sig_uid);
	else
		composer->signature = NULL;
	
	sig_select_item (composer);
}

/**
 * e_msg_composer_new_with_type:
 *
 * Create a new message composer widget. The type can be
 * E_MSG_COMPOSER_MAIL, E_MSG_COMPOSER_POST or E_MSG_COMPOSER_MAIL_POST.
 *
 * Return value: A pointer to the newly created widget
 **/

EMsgComposer *
e_msg_composer_new_with_type (int type)
{
	gboolean send_html;
	GConfClient *gconf;
	EMsgComposer *new;

	gconf = gconf_client_get_default ();
	send_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/send_html", NULL);
	g_object_unref (gconf);

	switch (type) {
	case E_MSG_COMPOSER_MAIL:
		new = create_composer (E_MSG_COMPOSER_VISIBLE_MASK_MAIL);
		break;
	case E_MSG_COMPOSER_POST:
		new = create_composer (E_MSG_COMPOSER_VISIBLE_MASK_POST);
		break;
	default:
		new = create_composer (E_MSG_COMPOSER_VISIBLE_MASK_MAIL | E_MSG_COMPOSER_VISIBLE_MASK_POST);
	}

	if (new) {
		e_msg_composer_set_send_html (new, send_html);
		set_editor_signature (new);
		set_editor_text (new, "", 0, TRUE, TRUE);
	}

	return new;
}

/**
 * e_msg_composer_new:
 *
 * Create a new message composer widget.
 * 
 * Return value: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new (void)
{
	return e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL);
}

static gboolean
is_special_header (const char *hdr_name)
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
e_msg_composer_set_pending_body (EMsgComposer *composer, char *text, ssize_t len)
{
	char *old;
	
	old = g_object_get_data ((GObject *) composer, "body:text");
        g_free (old);
	g_object_set_data ((GObject *) composer, "body:text", text);
	g_object_set_data ((GObject *) composer, "body:len", GSIZE_TO_POINTER (len));
}

static void
e_msg_composer_flush_pending_body (EMsgComposer *composer, gboolean apply)
{
        char *body;
	ssize_t len;
	
	body = g_object_get_data ((GObject *) composer, "body:text");
	len = GPOINTER_TO_SIZE (g_object_get_data ((GObject *) composer, "body:len"));
	if (body) {
		if (apply) 
			set_editor_text (composer, body, len, FALSE, FALSE);
		
		g_object_set_data ((GObject *) composer, "body:text", NULL);
		g_free (body);
	}
}	

static void
add_attachments_handle_mime_part (EMsgComposer *composer, CamelMimePart *mime_part, 
				  gboolean just_inlines, gboolean related, int depth)
{
	CamelContentType *content_type;
	CamelDataWrapper *wrapper;
	
	content_type = camel_mime_part_get_content_type (mime_part);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	
	if (CAMEL_IS_MULTIPART (wrapper)) {
		/* another layer of multipartness... */
		add_attachments_from_multipart (composer, (CamelMultipart *) wrapper, just_inlines, depth + 1);
	} else if (just_inlines) {
		if (camel_mime_part_get_content_id (mime_part) ||
		    camel_mime_part_get_content_location (mime_part))
			e_msg_composer_add_inline_image_from_mime_part (composer, mime_part);
	} else if (CAMEL_IS_MIME_MESSAGE (wrapper)) {
		/* do nothing */
	} else if (related && camel_content_type_is (content_type, "image", "*")) {
		e_msg_composer_add_inline_image_from_mime_part (composer, mime_part);
	} else {
		if (camel_content_type_is (content_type, "text", "*")) {
			/* do nothing */
		} else {
			e_msg_composer_attach (composer, mime_part);
		}
	}
}

static void
add_attachments_from_multipart (EMsgComposer *composer, CamelMultipart *multipart,
				gboolean just_inlines, int depth)
{
	/* find appropriate message attachments to add to the composer */
	CamelMimePart *mime_part;
	gboolean related;
	int i, nparts;
	
	related = camel_content_type_is (CAMEL_DATA_WRAPPER (multipart)->mime_type, "multipart", "related");
		
	if (CAMEL_IS_MULTIPART_SIGNED (multipart)) {
		mime_part = camel_multipart_get_part (multipart, CAMEL_MULTIPART_SIGNED_CONTENT);
		add_attachments_handle_mime_part (composer, mime_part, just_inlines, related, depth);
	} else if (CAMEL_IS_MULTIPART_ENCRYPTED (multipart)) {
		/* what should we do in this case? */
	} else {
		nparts = camel_multipart_get_number (multipart);

		for (i = 0; i < nparts; i++) {
			mime_part = camel_multipart_get_part (multipart, i);
			add_attachments_handle_mime_part (composer, mime_part, just_inlines, related, depth);
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
e_msg_composer_add_message_attachments (EMsgComposer *composer, CamelMimeMessage *message,
					gboolean just_inlines)
{
	CamelDataWrapper *wrapper;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	if (!CAMEL_IS_MULTIPART (wrapper))
		return;
	
	/* there must be attachments... */
	add_attachments_from_multipart (composer, (CamelMultipart *) wrapper, just_inlines, 0);
}


static void
handle_multipart_signed (EMsgComposer *composer, CamelMultipart *multipart, int depth)
{
	CamelContentType *content_type;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	
	/* FIXME: make sure this isn't an s/mime signed part?? */
	e_msg_composer_set_pgp_sign (composer, TRUE);
	
	mime_part = camel_multipart_get_part (multipart, CAMEL_MULTIPART_SIGNED_CONTENT);
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
			handle_multipart_encrypted (composer, multipart, depth);
		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* this contains the text/plain and text/html versions of the message body */
			handle_multipart_alternative (composer, multipart, depth);
		} else {
			/* there must be attachments... */
			handle_multipart (composer, multipart, depth);
		}
	} else if (camel_content_type_is (content_type, "text", "*")) {
		ssize_t len;
		char *html;

		html = em_utils_part_to_html (mime_part, &len, NULL);
		e_msg_composer_set_pending_body (composer, html, len);
	} else {
		e_msg_composer_attach (composer, mime_part);
	}
}

static void
handle_multipart_encrypted (EMsgComposer *composer, CamelMultipart *multipart, int depth)
{
	CamelMultipartEncrypted *mpe = (CamelMultipartEncrypted *) multipart;
	CamelContentType *content_type;
	CamelCipherContext *cipher;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;
	CamelException ex;
	
	/* FIXME: make sure this is a PGP/MIME encrypted part?? */
	e_msg_composer_set_pgp_encrypt (composer, TRUE);
	
	camel_exception_init (&ex);
	cipher = mail_crypto_get_pgp_cipher_context (NULL);
	mime_part = camel_multipart_encrypted_decrypt (mpe, cipher, &ex);
	camel_object_unref (cipher);
	camel_exception_clear (&ex);
	
	if (!mime_part)
		return;
	
	content_type = camel_mime_part_get_content_type (mime_part);
	
	content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	
	if (CAMEL_IS_MULTIPART (content)) {
		multipart = CAMEL_MULTIPART (content);
		
		/* Note: depth is preserved here because we're not
                   counting multipart/encrypted as a multipart, instead
                   we want to treat the content part as our mime part
                   here. */
		
		if (CAMEL_IS_MULTIPART_SIGNED (content)) {
			/* handle the signed content and configure the composer to sign outgoing messages */
			handle_multipart_signed (composer, multipart, depth);
		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
			handle_multipart_encrypted (composer, multipart, depth);
		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* this contains the text/plain and text/html versions of the message body */
			handle_multipart_alternative (composer, multipart, depth);
		} else {
			/* there must be attachments... */
			handle_multipart (composer, multipart, depth);
		}
	} else if (camel_content_type_is (content_type, "text", "*")) {
		ssize_t len;
		char *html;

		html = em_utils_part_to_html (mime_part, &len, NULL);
		e_msg_composer_set_pending_body (composer, html, len);
	} else {
		e_msg_composer_attach (composer, mime_part);
	}
	
	camel_object_unref (mime_part);
}

static void
handle_multipart_alternative (EMsgComposer *composer, CamelMultipart *multipart, int depth)
{
	/* Find the text/html part and set the composer body to it's contents */
	CamelMimePart *text_part = NULL;
	int i, nparts;
	
	nparts = camel_multipart_get_number (multipart);
	
	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelDataWrapper *content;
		CamelMimePart *mime_part;
		
		mime_part = camel_multipart_get_part (multipart, i);
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
				handle_multipart_encrypted (composer, mp, depth + 1);
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
		ssize_t len;
		char *html;

		html = em_utils_part_to_html(text_part, &len, NULL);
		e_msg_composer_set_pending_body(composer, html, len);
	}
}

static void
handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, int depth)
{
	int i, nparts;
	
	nparts = camel_multipart_get_number (multipart);
	
	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelDataWrapper *content;
		CamelMimePart *mime_part;

		mime_part = camel_multipart_get_part (multipart, i);
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
				handle_multipart_encrypted (composer, mp, depth + 1);
			} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
				handle_multipart_alternative (composer, mp, depth + 1);
			} else {
				/* depth doesn't matter so long as we don't pass 0 */
				handle_multipart (composer, mp, depth + 1);
			}
		} else if (depth == 0 && i == 0) {
			ssize_t len;
			char *html;

			/* Since the first part is not multipart/alternative, then this must be the body */
			html = em_utils_part_to_html(mime_part, &len, NULL);
			e_msg_composer_set_pending_body(composer, html, len);
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
	CORBA_Environment ev;
	
	composer->signature = NULL;
	
	CORBA_exception_init (&ev);
	if (GNOME_GtkHTML_Editor_Engine_searchByData (composer->editor_engine, 1, "ClueFlow", "signature", "1", &ev)) {
		char *name, *str = NULL;
		
		str = GNOME_GtkHTML_Editor_Engine_getParagraphData (composer->editor_engine, "signature_name", &ev);
		if (ev._major == CORBA_NO_EXCEPTION && str) {
			if (!strncmp (str, "uid:", 4)) {
				name = decode_signature_name (str + 4);
				composer->signature = mail_config_get_signature_by_uid (name);
				g_free (name);
			} else if (!strncmp (str, "name:", 5)) {
				name = decode_signature_name (str + 4);
				composer->signature = mail_config_get_signature_by_name (name);
				g_free (name);
			}
		}
		
		sig_select_item (composer);
	}
	CORBA_exception_free (&ev);
}


static void
auto_recip_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
}

/**
 * e_msg_composer_new_with_message:
 * @message: The message to use as the source
 * 
 * Create a new message composer widget.
 *
 * Note: Designed to work only for messages constructed using Evolution.
 *
 * Return value: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_with_message (CamelMimeMessage *message)
{
	const CamelInternetAddress *to, *cc, *bcc;
	GList *To = NULL, *Cc = NULL, *Bcc = NULL;
	const char *format, *subject, *postto;
	EDestination **Tov, **Ccv, **Bccv;
	GHashTable *auto_cc, *auto_bcc;
	CamelContentType *content_type;
	struct _camel_header_raw *headers;
	CamelDataWrapper *content;
	EAccount *account = NULL;
	char *account_name;
	EMsgComposer *new;
	XEvolution *xev;
	int len, i;
	
	postto = camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-PostTo");
	
	new = create_composer (postto ? E_MSG_COMPOSER_VISIBLE_MASK_POST : E_MSG_COMPOSER_VISIBLE_MASK_MAIL);
	if (!new)
		return NULL;
	
	if (postto)
		e_msg_composer_hdrs_set_post_to (E_MSG_COMPOSER_HDRS (new->hdrs), postto);
	
	/* Restore the Account preference */
	account_name = (char *) camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Account");
	if (account_name) {
		account_name = g_strdup (account_name);
		g_strstrip (account_name);
		
		account = mail_config_get_account_by_name (account_name);
	}
	
	if (postto == NULL) {
		auto_cc = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
		auto_bcc = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
		
		if (account) {
			CamelInternetAddress *iaddr;
			
			/* hash our auto-recipients for this account */
			if (account->always_cc) {
				iaddr = camel_internet_address_new ();
				if (camel_address_decode (CAMEL_ADDRESS (iaddr), account->cc_addrs) != -1) {
					for (i = 0; i < camel_address_length (CAMEL_ADDRESS (iaddr)); i++) {
						const char *name, *addr;
						
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
						const char *name, *addr;
						
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
			const char *name, *addr;
			
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
			const char *name, *addr;
			
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
		g_hash_table_foreach (auto_cc, auto_recip_free, NULL);
		g_hash_table_destroy (auto_cc);
		g_list_free (Cc);
		
		len = CAMEL_ADDRESS (bcc)->addresses->len;
		for (i = 0; i < len; i++) {
			const char *name, *addr;
			
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
		g_hash_table_foreach (auto_bcc, auto_recip_free, NULL);
		g_hash_table_destroy (auto_bcc);
		g_list_free (Bcc);
	} else {
		Tov = NULL;
		Ccv = NULL;
		Bccv = NULL;
	}
	
	subject = camel_mime_message_get_subject (message);
	
	e_msg_composer_set_headers (new, account_name, Tov, Ccv, Bccv, subject);
	
	g_free (account_name);
	
	e_destination_freev (Tov);
	e_destination_freev (Ccv);
	e_destination_freev (Bccv);
	
	/* Restore the format editing preference */
	format = camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Format");
	if (format) {
		char **flags;

		while (*format && camel_mime_is_lwsp(*format))
			format++;

		flags = g_strsplit(format, ", ", 0);
		for (i=0;flags[i];i++) {
			printf("restoring draft flag '%s'\n", flags[i]);

			if (g_ascii_strcasecmp(flags[i], "text/html") == 0)
				e_msg_composer_set_send_html (new, TRUE);
			else if (g_ascii_strcasecmp(flags[i], "text/plain") == 0)
				e_msg_composer_set_send_html (new, FALSE);
			else if (g_ascii_strcasecmp(flags[i], "pgp-sign") == 0)
				e_msg_composer_set_pgp_sign(new, TRUE);
			else if (g_ascii_strcasecmp(flags[i], "pgp-encrypt") == 0)
				e_msg_composer_set_pgp_encrypt(new, TRUE);
			else if (g_ascii_strcasecmp(flags[i], "smime-sign") == 0)
				e_msg_composer_set_smime_sign(new, TRUE);
			else if (g_ascii_strcasecmp(flags[i], "smime-encrypt") == 0)
				e_msg_composer_set_smime_encrypt(new, TRUE);
		}
		g_strfreev(flags);
	}
	
	/* Remove any other X-Evolution-* headers that may have been set */
	xev = mail_tool_remove_xevolution_headers (message);
	mail_tool_destroy_xevolution (xev);
	
	/* set extra headers */
	headers = CAMEL_MIME_PART (message)->headers;
	while (headers) {
		if (!is_special_header (headers->name) ||
		    !g_ascii_strcasecmp (headers->name, "References") ||
		    !g_ascii_strcasecmp (headers->name, "In-Reply-To")) {
			g_ptr_array_add (new->extra_hdr_names, g_strdup (headers->name));
			g_ptr_array_add (new->extra_hdr_values, g_strdup (headers->value));
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
			handle_multipart_signed (new, multipart, 0);
		} else if (CAMEL_IS_MULTIPART_ENCRYPTED (content)) {
			/* decrypt the encrypted content and configure the composer to encrypt outgoing messages */
			handle_multipart_encrypted (new, multipart, 0);
		} else if (camel_content_type_is (content_type, "multipart", "alternative")) {
			/* this contains the text/plain and text/html versions of the message body */
			handle_multipart_alternative (new, multipart, 0);
		} else {
			/* there must be attachments... */
			handle_multipart (new, multipart, 0);
		}
	} else {
		ssize_t len;
		char *html;

		html = em_utils_part_to_html((CamelMimePart *)message, &len, NULL);
		e_msg_composer_set_pending_body(new, html, len);
	}
	
	/* We wait until now to set the body text because we need to ensure that
	 * the attachment bar has all the attachments, before we request them.
	 */	
	e_msg_composer_flush_pending_body (new, TRUE);
	
	set_signature_gui (new);
	
	return new;
}

static void
disable_editor (EMsgComposer *composer)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "editable-off", &ev);
	CORBA_exception_free (&ev);
	
	gtk_widget_set_sensitive (composer->attachment_bar, FALSE);
	
	bonobo_ui_component_set_prop (composer->uic, "/menu/Edit", "sensitive", "0", NULL);
	bonobo_ui_component_set_prop (composer->uic, "/menu/Format", "sensitive", "0", NULL);
	bonobo_ui_component_set_prop (composer->uic, "/menu/Insert", "sensitive", "0", NULL);
}

/**
 * e_msg_composer_new_redirect:
 * @message: The message to use as the source
 * 
 * Create a new message composer widget.
 *
 * Return value: A pointer to the newly created widget
 **/
EMsgComposer *
e_msg_composer_new_redirect (CamelMimeMessage *message, const char *resent_from)
{
	EMsgComposer *composer;
	const char *subject;
	
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	
	composer = e_msg_composer_new_with_message (message);
	subject = camel_mime_message_get_subject (message);
	
	composer->redirect = message;
	camel_object_ref (message);
	
	e_msg_composer_set_headers (composer, resent_from, NULL, NULL, NULL, subject);
	
	disable_editor (composer);
	
	return composer;
}


static GList *
add_recipients (GList *list, const char *recips)
{
	CamelInternetAddress *cia;
	const char *name, *addr;
	int num, i;
	
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
handle_mailto (EMsgComposer *composer, const char *mailto)
{
	EMsgComposerHdrs *hdrs;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	EDestination **tov, **ccv, **bccv;
	char *subject = NULL, *body = NULL;
	char *header, *content, *buf;
	size_t nread, nwritten;
	const char *p;
	int len, clen;
	CamelURL *url;
	
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
					e_msg_composer_attachment_bar_attach (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
									      url->path);
					camel_url_free (url);
				} else {
					e_msg_composer_attachment_bar_attach (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
									      content);
				}
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
	
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	
	e_msg_composer_hdrs_set_to (hdrs, tov);
	e_msg_composer_hdrs_set_cc (hdrs, ccv);
	e_msg_composer_hdrs_set_bcc (hdrs, bccv);
	
	e_destination_freev (tov);
	e_destination_freev (ccv);
	e_destination_freev (bccv);
	
	if (subject) {
		e_msg_composer_hdrs_set_subject (hdrs, subject);
		g_free (subject);
	}
	
	if (body) {
		char *htmlbody;
		
		htmlbody = camel_text_to_html (body, CAMEL_MIME_FILTER_TOHTML_PRE, 0);
		set_editor_text (composer, htmlbody, -1, FALSE, FALSE);
		g_free (htmlbody);
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
e_msg_composer_new_from_url (const char *url)
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
 * e_msg_composer_show_attachments:
 * @composer: A message composer widget
 * @show: A boolean specifying whether the attachment bar should be shown or
 * not
 * 
 * If @show is %FALSE, hide the attachment bar.  Otherwise, show it.
 **/
void
e_msg_composer_show_attachments (EMsgComposer *composer,
				 gboolean show)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	show_attachments (composer, show);
}


/**
 * e_msg_composer_set_headers:
 * @composer: a composer object
 * @from: the name of the account the user will send from,
 * or %NULL for the default account
 * @to: the values for the "To" header
 * @cc: the values for the "Cc" header
 * @bcc: the values for the "Bcc" header
 * @subject: the value for the "Subject" header
 *
 * Sets the headers in the composer to the given values.
 **/
void 
e_msg_composer_set_headers (EMsgComposer *composer,
			    const char *from,
			    EDestination **to,
			    EDestination **cc,
			    EDestination **bcc,
			    const char *subject)
{
	EMsgComposerHdrs *hdrs;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	
	e_msg_composer_hdrs_set_to (hdrs, to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	e_msg_composer_hdrs_set_subject (hdrs, subject);
	e_msg_composer_hdrs_set_from_account (hdrs, from);
}


/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer, const char *text, ssize_t len)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	set_editor_text (composer, text, len, TRUE, text == "");
}

/**
 * e_msg_composer_set_body:
 * @composer: a composer object
 * @body: the data to initialize the composer with
 * @mime_type: the MIME type of data
 *
 * Loads the given data into the composer as the message body.
 * This function should only be used by the CORBA composer factory.
 **/
void
e_msg_composer_set_body (EMsgComposer *composer, const char *body,
			 const char *mime_type)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	set_editor_text (composer, _("<b>(The composer contains a non-text message body, which cannot be edited.)<b>"), -1, FALSE, FALSE);
	e_msg_composer_set_send_html (composer, FALSE);
	disable_editor (composer);
	
	g_free (composer->mime_body);
	composer->mime_body = g_strdup (body);
	g_free (composer->mime_type);
	composer->mime_type = g_strdup (mime_type);

	if (g_ascii_strncasecmp (composer->mime_type, "text/calendar", 13) == 0) {
		EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
		if (hdrs->account && hdrs->account->pgp_no_imip_sign)
			e_msg_composer_set_pgp_sign (composer, FALSE);
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
e_msg_composer_add_header (EMsgComposer *composer, const char *name,
			   const char *value)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	
	g_ptr_array_add (composer->extra_hdr_names, g_strdup (name));
	g_ptr_array_add (composer->extra_hdr_values, g_strdup (value));
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
	EMsgComposerAttachmentBar *bar;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (attachment));
	
	bar = E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	e_msg_composer_attachment_bar_attach_mime_part (bar, attachment);
}


/**
 * e_msg_composer_add_inline_image_from_file:
 * @composer: a composer object
 * @file_name: the name of the file containing the image
 *
 * This reads in the image in @file_name and adds it to @composer
 * as an inline image, to be wrapped in a multipart/related.
 *
 * Return value: the newly-created CamelMimePart (which must be reffed
 * if the caller wants to keep its own reference), or %NULL on error.
 **/
CamelMimePart *
e_msg_composer_add_inline_image_from_file (EMsgComposer *composer,
					   const char *file_name)
{
	char *mime_type, *cid, *url, *name;
	CamelStream *stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *part;
	struct stat statbuf;
	
	/* check for regular file */
	if (stat (file_name, &statbuf) < 0 || !S_ISREG (statbuf.st_mode))
		return NULL;
	
	stream = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0);
	if (!stream)
		return NULL;
	
	wrapper = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	mime_type = e_msg_composer_guess_mime_type (file_name);
	camel_data_wrapper_set_mime_type (wrapper, mime_type ? mime_type : "application/octet-stream");
	g_free (mime_type);
	
	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);
	
	cid = camel_header_msgid_generate ();
	camel_mime_part_set_content_id (part, cid);
	name = g_path_get_basename(file_name);
	camel_mime_part_set_filename (part, name);
	g_free(name);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
	
	url = g_strdup_printf ("file:%s", file_name);
	g_hash_table_insert (composer->inline_images_by_url, url, part);
	
	url = g_strdup_printf ("cid:%s", cid);
	g_hash_table_insert (composer->inline_images, url, part);
	g_free (cid);
	
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
	char *url;
	const char *location, *cid;

	cid = camel_mime_part_get_content_id (part);
	if (!cid) {
		camel_mime_part_set_content_id (part, NULL);
		cid = camel_mime_part_get_content_id (part);
	}
	
	url = g_strdup_printf ("cid:%s", cid);
	g_hash_table_insert (composer->inline_images, url, part);
	camel_object_ref (part);
	
	location = camel_mime_part_get_content_location (part);
	if (location) {
		g_hash_table_insert (composer->inline_images_by_url,
				     g_strdup (location), part);
	}
}


/**
 * e_msg_composer_get_message:
 * @composer: A message composer widget
 * 
 * Retrieve the message edited by the user as a CamelMimeMessage.  The
 * CamelMimeMessage object is created on the fly; subsequent calls to this
 * function will always create new objects from scratch.
 * 
 * Return value: A pointer to the new CamelMimeMessage object
 **/
CamelMimeMessage *
e_msg_composer_get_message (EMsgComposer *composer, gboolean save_html_object_data)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return build_message (composer, save_html_object_data);
}

CamelMimeMessage *
e_msg_composer_get_message_draft (EMsgComposer *composer)
{
	CamelMimeMessage *msg;
	EAccount *account;
	gboolean old_flags[4];
	gboolean old_send_html;
	GString *flags;
	int i;

	/* always save drafts as HTML to preserve formatting */
	old_send_html = composer->send_html;
	composer->send_html = TRUE;
	old_flags[0] = composer->pgp_sign;
	composer->pgp_sign = FALSE;
	old_flags[1] = composer->pgp_encrypt;
	composer->pgp_encrypt = FALSE;
	old_flags[2] = composer->smime_sign;
	composer->smime_sign = FALSE;
	old_flags[3] = composer->smime_encrypt;
	composer->smime_encrypt = FALSE;
	
	msg = e_msg_composer_get_message (composer, TRUE);
	
	composer->send_html = old_send_html;
	composer->pgp_sign = old_flags[0];
	composer->pgp_encrypt = old_flags[1];
	composer->smime_sign = old_flags[2];
	composer->smime_encrypt = old_flags[3];
	
	/* Attach account info to the draft. */
	account = e_msg_composer_get_preferred_account (composer);
	if (account && account->name)
		camel_medium_set_header (CAMEL_MEDIUM (msg), "X-Evolution-Account", account->name);
	
	/* build_message() set this to text/html since we set composer->send_html to
	   TRUE before calling e_msg_composer_get_message() */
	if (!composer->send_html)
		flags = g_string_new("text/plain");
	else
		flags = g_string_new("text/html");

	/* This should probably only save the setting if it is
	 * different from the from-account default? */
	for (i=0;i<4;i++) {
		if (old_flags[i])
			g_string_append_printf(flags, ", %s", emc_draft_format_names[i]);
	}

	camel_medium_set_header (CAMEL_MEDIUM (msg), "X-Evolution-Format", flags->str);
	g_string_free(flags, TRUE);

	return msg;
}


static void
delete_old_signature (EMsgComposer *composer)
{
	CORBA_Environment ev;
	
	/* printf ("delete_old_signature\n"); */
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-bod", &ev);
	if (GNOME_GtkHTML_Editor_Engine_searchByData (composer->editor_engine, 1, "ClueFlow", "signature", "1", &ev)) {
		/* printf ("found\n"); */
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "select-paragraph", &ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "delete", &ev);
		/* selection-move-right doesn't succeed means that we are already on the end of document */
		/* if (!rv)
		   break; */
		GNOME_GtkHTML_Editor_Engine_setParagraphData (composer->editor_engine, "signature", "0", &ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "delete-back", &ev);
	}
	CORBA_exception_free (&ev);
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
	CORBA_Environment ev;
	char *html;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	/* printf ("e_msg_composer_show_sig_file\n"); */
	/* printf ("set sig '%s' '%s'\n", sig_file, composer->sig_file); */
	
	composer->in_signature_insert = TRUE;
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_freeze (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-position-save", &ev);
	GNOME_GtkHTML_Editor_Engine_undoBegin (composer->editor_engine, "Set signature", "Reset signature", &ev);
	
	delete_old_signature (composer);
	html = get_signature_html (composer);
	if (html) {
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "insert-paragraph", &ev);
		if (!GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-backward", &ev))
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "insert-paragraph", &ev);
		else
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-forward", &ev);
		/* printf ("insert %s\n", html); */
		GNOME_GtkHTML_Editor_Engine_setParagraphData (composer->editor_engine, "orig", "0", &ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "indent-zero", &ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "style-normal", &ev);
		GNOME_GtkHTML_Editor_Engine_insertHTML (composer->editor_engine, html, &ev);
		g_free (html);
	}
	
	GNOME_GtkHTML_Editor_Engine_undoEnd (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-position-restore", &ev);
	GNOME_GtkHTML_Editor_Engine_thaw (composer->editor_engine, &ev);
	CORBA_exception_free (&ev);
	composer->in_signature_insert = FALSE;
	
	/* printf ("e_msg_composer_show_sig_file end\n"); */
}


/**
 * e_msg_composer_set_send_html:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "Send HTML" flag set
 * 
 * Set the status of the "Send HTML" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_send_html (EMsgComposer *composer,
			      gboolean send_html)
{
	CORBA_Environment ev;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->send_html && send_html)
		return;
	
	if (!composer->send_html && !send_html)
		return;
	
	composer->send_html = send_html;
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "block-redraw", &ev);
	CORBA_exception_free (&ev);
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/FormatHtml",
				      "state", composer->send_html ? "1" : "0", NULL);
	
	/* let the editor know which mode we are in */
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor),
				    "FormatHTML", TC_CORBA_boolean,
				    composer->send_html, NULL);
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "unblock-redraw", &ev);
	CORBA_exception_free (&ev);
}


/**
 * e_msg_composer_get_send_html:
 * @composer: A message composer widget
 * 
 * Get the status of the "Send HTML mail" flag.
 * 
 * Return value: The status of the "Send HTML mail" flag.
 **/
gboolean
e_msg_composer_get_send_html (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->send_html;
}


/**
 * e_msg_composer_get_preferred_account:
 * @composer: composer
 *
 * Returns the user-specified account (from field).
 */
EAccount *
e_msg_composer_get_preferred_account (EMsgComposer *composer)
{
	EMsgComposerHdrs *hdrs;
	
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	
	return hdrs->account;
}


/**
 * e_msg_composer_set_pgp_sign:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "PGP Sign" flag set
 * 
 * Set the status of the "PGP Sign" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_pgp_sign (EMsgComposer *composer, gboolean pgp_sign)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->pgp_sign && pgp_sign)
		return;
	if (!composer->pgp_sign && !pgp_sign)
		return;
	
	composer->pgp_sign = pgp_sign;
	e_msg_composer_set_changed (composer);
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/SecurityPGPSign",
				      "state", composer->pgp_sign ? "1" : "0", NULL);
}


/**
 * e_msg_composer_get_pgp_sign:
 * @composer: A message composer widget
 * 
 * Get the status of the "PGP Sign" flag.
 * 
 * Return value: The status of the "PGP Sign" flag.
 **/
gboolean
e_msg_composer_get_pgp_sign (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->pgp_sign;
}


/**
 * e_msg_composer_set_pgp_encrypt:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "PGP Encrypt" flag set
 * 
 * Set the status of the "PGP Encrypt" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_pgp_encrypt (EMsgComposer *composer, gboolean pgp_encrypt)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->pgp_encrypt && pgp_encrypt)
		return;
	if (!composer->pgp_encrypt && !pgp_encrypt)
		return;
	
	composer->pgp_encrypt = pgp_encrypt;
	e_msg_composer_set_changed (composer);
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/SecurityPGPEncrypt",
				      "state", composer->pgp_encrypt ? "1" : "0", NULL);
}


/**
 * e_msg_composer_get_pgp_encrypt:
 * @composer: A message composer widget
 * 
 * Get the status of the "PGP Encrypt" flag.
 * 
 * Return value: The status of the "PGP Encrypt" flag.
 **/
gboolean
e_msg_composer_get_pgp_encrypt (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->pgp_encrypt;
}


/**
 * e_msg_composer_set_smime_sign:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "S/MIME Sign" flag set
 * 
 * Set the status of the "S/MIME Sign" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_smime_sign (EMsgComposer *composer, gboolean smime_sign)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->smime_sign && smime_sign)
		return;
	if (!composer->smime_sign && !smime_sign)
		return;
	
	composer->smime_sign = smime_sign;	
	e_msg_composer_set_changed (composer);

	bonobo_ui_component_set_prop (composer->uic, "/commands/SecuritySMimeSign",
				      "state", composer->smime_sign ? "1" : "0", NULL);
}


/**
 * e_msg_composer_get_smime_sign:
 * @composer: A message composer widget
 * 
 * Get the status of the "S/MIME Sign" flag.
 * 
 * Return value: The status of the "S/MIME Sign" flag.
 **/
gboolean
e_msg_composer_get_smime_sign (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->smime_sign;
}


/**
 * e_msg_composer_set_smime_encrypt:
 * @composer: A message composer widget
 * @send_html: Whether the composer should have the "S/MIME Encrypt" flag set
 * 
 * Set the status of the "S/MIME Encrypt" toggle item.  The user can override it.
 **/
void
e_msg_composer_set_smime_encrypt (EMsgComposer *composer, gboolean smime_encrypt)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->smime_encrypt && smime_encrypt)
		return;
	if (!composer->smime_encrypt && !smime_encrypt)
		return;
	
	composer->smime_encrypt = smime_encrypt;
	e_msg_composer_set_changed (composer);
	
	bonobo_ui_component_set_prop (composer->uic, "/commands/SecuritySMimeEncrypt",
				      "state", composer->smime_encrypt ? "1" : "0", NULL);
}


/**
 * e_msg_composer_get_smime_encrypt:
 * @composer: A message composer widget
 * 
 * Get the status of the "S/MIME Encrypt" flag.
 * 
 * Return value: The status of the "S/MIME Encrypt" flag.
 **/
gboolean
e_msg_composer_get_smime_encrypt (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->smime_encrypt;
}


/**
 * e_msg_composer_get_view_from:
 * @composer: A message composer widget
 * 
 * Get the status of the "View From header" flag.
 * 
 * Return value: The status of the "View From header" flag.
 **/
gboolean
e_msg_composer_get_view_from (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_from;
}


/**
 * e_msg_composer_set_view_from:
 * @composer: A message composer widget
 * @state: whether to show or hide the From selector
 *
 * Controls the state of the From selector
 */
void
e_msg_composer_set_view_from (EMsgComposer *composer, gboolean view_from)
{
	GConfClient *gconf;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_from && view_from) ||
	    (!composer->view_from && !view_from))
		return;
	
	composer->view_from = view_from;
	bonobo_ui_component_set_prop (composer->uic, "/commands/ViewFrom",
				      "state", composer->view_from ? "1" : "0", NULL);
	
	gconf = gconf_client_get_default ();
	gconf_client_set_bool (gconf, "/apps/evolution/mail/composer/view/From", view_from, NULL);
	g_object_unref (gconf);
	
	e_msg_composer_hdrs_set_visible (E_MSG_COMPOSER_HDRS (composer->hdrs),
					 e_msg_composer_get_visible_flags (composer));
}


/**
 * e_msg_composer_get_view_replyto:
 * @composer: A message composer widget
 *
 * Get the status of the "View Reply-To header" flag.
 *
 * Return value: The status of the "View Reply-To header" flag.
 **/
gboolean
e_msg_composer_get_view_replyto (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_replyto;
}


/**
 * e_msg_composer_set_view_replyto:
 * @composer: A message composer widget
 * @state: whether to show or hide the Reply-To selector
 *
 * Controls the state of the Reply-To selector
 */
void
e_msg_composer_set_view_replyto (EMsgComposer *composer, gboolean view_replyto)
{
	GConfClient *gconf;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_replyto && view_replyto) ||
	    (!composer->view_replyto && !view_replyto))
		return;
	
	composer->view_replyto = view_replyto;
	bonobo_ui_component_set_prop (composer->uic, "/commands/ViewReplyTo",
				      "state", composer->view_replyto ? "1" : "0", NULL);
	
	/* we do this /only/ if the fields is in the visible_mask */
	gconf = gconf_client_get_default ();
	gconf_client_set_bool (gconf, "/apps/evolution/mail/composer/view/ReplyTo", view_replyto, NULL);
	g_object_unref (gconf);
	
	e_msg_composer_hdrs_set_visible (E_MSG_COMPOSER_HDRS (composer->hdrs),
					 e_msg_composer_get_visible_flags (composer));
}


/**
 * e_msg_composer_get_view_to:
 * @composer: A message composer widget
 *
 * Get the status of the "View To header" flag.
 *
 * Return value: The status of the "View To header" flag.
 **/
gboolean
e_msg_composer_get_view_to (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_to;
}


/**
 * e_msg_composer_set_view_to:
 * @composer: A message composer widget
 * @state: whether to show or hide the To selector
 *
 * Controls the state of the To selector
 */
void
e_msg_composer_set_view_to (EMsgComposer *composer, gboolean view_to)
{
	GConfClient *gconf;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_to && view_to) ||
	    (!composer->view_to && !view_to))
		return;
	
	composer->view_to = view_to;
	bonobo_ui_component_set_prop (composer->uic, "/commands/ViewTo",
				      "state", composer->view_to ? "1" : "0", NULL);
	
	if ((E_MSG_COMPOSER_HDRS(composer->hdrs))->visible_mask & E_MSG_COMPOSER_VISIBLE_TO) {
		gconf = gconf_client_get_default ();
		gconf_client_set_bool (gconf, "/apps/evolution/mail/composer/view/To", view_to, NULL);
		g_object_unref (gconf);
	}
	
	e_msg_composer_hdrs_set_visible (E_MSG_COMPOSER_HDRS (composer->hdrs),
					 e_msg_composer_get_visible_flags (composer));
}



/**
 * e_msg_composer_get_view_postto:
 * @composer: A message composer widget
 *
 * Get the status of the "View PostTo header" flag.
 *
 * Return value: The status of the "View PostTo header" flag.
 **/
gboolean
e_msg_composer_get_view_postto (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_postto;
}


/**
 * e_msg_composer_set_view_postto:
 * @composer: A message composer widget
 * @state: whether to show or hide the PostTo selector
 *
 * Controls the state of the PostTo selector
 */
void
e_msg_composer_set_view_postto (EMsgComposer *composer, gboolean view_postto)
{
	GConfClient *gconf;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_postto && view_postto) ||
	    (!composer->view_postto && !view_postto))
		return;
	
	composer->view_postto = view_postto;
	bonobo_ui_component_set_prop (composer->uic, "/commands/ViewPostTo",
				      "state", composer->view_postto ? "1" : "0", NULL);
	
	if ((E_MSG_COMPOSER_HDRS(composer->hdrs))->visible_mask & E_MSG_COMPOSER_VISIBLE_POSTTO) {
		gconf = gconf_client_get_default ();
		gconf_client_set_bool (gconf, "/apps/evolution/mail/composer/view/PostTo", view_postto, NULL);
		g_object_unref (gconf);
	}
	
	e_msg_composer_hdrs_set_visible (E_MSG_COMPOSER_HDRS (composer->hdrs),
					 e_msg_composer_get_visible_flags (composer));
}



/**
 * e_msg_composer_get_view_cc:
 * @composer: A message composer widget
 * 
 * Get the status of the "View CC header" flag.
 * 
 * Return value: The status of the "View CC header" flag.
 **/
gboolean
e_msg_composer_get_view_cc (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_cc;
}


/**
 * e_msg_composer_set_view_cc:
 * @composer: A message composer widget
 * @state: whether to show or hide the cc view
 *
 * Controls the state of the CC display
 */
void
e_msg_composer_set_view_cc (EMsgComposer *composer, gboolean view_cc)
{
	GConfClient *gconf;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_cc && view_cc) ||
	    (!composer->view_cc && !view_cc))
		return;
	
	composer->view_cc = view_cc;
	bonobo_ui_component_set_prop (composer->uic, "/commands/ViewCC",
				      "state", composer->view_cc ? "1" : "0", NULL);
	
	if ((E_MSG_COMPOSER_HDRS (composer->hdrs))->visible_mask & E_MSG_COMPOSER_VISIBLE_CC) {
		gconf = gconf_client_get_default ();
		gconf_client_set_bool (gconf, "/apps/evolution/mail/composer/view/Cc", view_cc, NULL);
		g_object_unref (gconf);
	}
	
	e_msg_composer_hdrs_set_visible (E_MSG_COMPOSER_HDRS (composer->hdrs),
					 e_msg_composer_get_visible_flags (composer));
}


/**
 * e_msg_composer_get_view_bcc:
 * @composer: A message composer widget
 * 
 * Get the status of the "View BCC header" flag.
 * 
 * Return value: The status of the "View BCC header" flag.
 **/
gboolean
e_msg_composer_get_view_bcc (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_bcc;
}


/**
 * e_msg_composer_set_view_bcc:
 * @composer: A message composer widget
 * @state: whether to show or hide the bcc view
 *
 * Controls the state of the BCC display
 */
void
e_msg_composer_set_view_bcc (EMsgComposer *composer, gboolean view_bcc)
{
	GConfClient *gconf;
	
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_bcc && view_bcc) ||
	    (!composer->view_bcc && !view_bcc))
		return;
	
	composer->view_bcc = view_bcc;
	bonobo_ui_component_set_prop (composer->uic, "/commands/ViewBCC",
				      "state", composer->view_bcc ? "1" : "0", NULL);
	
	if ((E_MSG_COMPOSER_HDRS (composer->hdrs))->visible_mask & E_MSG_COMPOSER_VISIBLE_BCC) {
		gconf = gconf_client_get_default ();
		gconf_client_set_bool (gconf, "/apps/evolution/mail/composer/view/Bcc", view_bcc, NULL);
		g_object_unref (gconf);
	}
	
	e_msg_composer_hdrs_set_visible (E_MSG_COMPOSER_HDRS (composer->hdrs),
					 e_msg_composer_get_visible_flags (composer));
}


EDestination **
e_msg_composer_get_recipients (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return composer->hdrs ? e_msg_composer_hdrs_get_recipients (E_MSG_COMPOSER_HDRS (composer->hdrs)) : NULL;
}

EDestination **
e_msg_composer_get_to (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return composer->hdrs ? e_msg_composer_hdrs_get_to (E_MSG_COMPOSER_HDRS (composer->hdrs)) : NULL;
}

EDestination **
e_msg_composer_get_cc (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return composer->hdrs ? e_msg_composer_hdrs_get_cc (E_MSG_COMPOSER_HDRS (composer->hdrs)) : NULL;
}

EDestination **
e_msg_composer_get_bcc (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return composer->hdrs ? e_msg_composer_hdrs_get_bcc (E_MSG_COMPOSER_HDRS (composer->hdrs)) : NULL;
}

const char *
e_msg_composer_get_subject (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return composer->hdrs ? e_msg_composer_hdrs_get_subject (E_MSG_COMPOSER_HDRS (composer->hdrs)) : NULL;
}


/**
 * e_msg_composer_guess_mime_type:
 * @file_name: filename
 *
 * Returns the guessed mime type of the file given by #file_name.
 **/
char *
e_msg_composer_guess_mime_type (const char *file_name)
{
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	char *type = NULL;

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (file_name, info,
					  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result == GNOME_VFS_OK)
		type = g_strdup (gnome_vfs_file_info_get_mime_type (info));

	gnome_vfs_file_info_unref (info);

	return type;
}


/**
 * e_msg_composer_set_changed:
 * @composer: An EMsgComposer object.
 *
 * Mark the composer as changed, so before the composer gets destroyed
 * the user will be prompted about unsaved changes.
 **/
void
e_msg_composer_set_changed (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	composer->has_changed = TRUE;
}


/**
 * e_msg_composer_unset_changed:
 * @composer: An EMsgComposer object.
 *
 * Mark the composer as unchanged, so no prompt about unsaved changes
 * will appear before destroying the composer.
 **/
void
e_msg_composer_unset_changed (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	composer->has_changed = FALSE;
}

gboolean
e_msg_composer_is_dirty (EMsgComposer *composer)
{
	CORBA_Environment ev;
	gboolean rv;
	
	CORBA_exception_init (&ev);
	rv = composer->has_changed
		|| (GNOME_GtkHTML_Editor_Engine_hasUndo (composer->editor_engine, &ev) &&
		    !GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "is-saved", &ev));
	CORBA_exception_free (&ev);

	return rv;
}

/**
 * e_msg_composer_set_autosaved:
 * @composer: An EMsgComposer object.
 *
 * Mark the composer as autosaved, so before the composer gets destroyed
 * the user will be prompted about unsaved changes.
 **/
void
e_msg_composer_set_autosaved (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	composer->autosaved = TRUE;
}


/**
 * e_msg_composer_unset_autosaved:
 * @composer: An EMsgComposer object.
 *
 * Mark the composer as unautosaved, so no prompt about unsaved changes
 * will appear before destroying the composer.
 **/
void
e_msg_composer_unset_autosaved (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	composer->autosaved = FALSE;
}

gboolean
e_msg_composer_is_autosaved (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->autosaved;
}

void
e_msg_composer_set_enable_autosave  (EMsgComposer *composer, gboolean enabled)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	composer->enable_autosave = enabled;
}

static char *
next_word (const char *s, const char **sr)
{
	if (!s || !*s)
		return NULL;
	else {
		const char *begin;
		gunichar uc;
		gboolean cited;
		
		do {
			begin = s;
			cited = FALSE;
			uc = g_utf8_get_char (s);
			if (uc == 0)
				return NULL;
			s  = g_utf8_next_char (s);
		} while (!html_selection_spell_word (uc, &cited) && !cited && s);
		
		/* we are at beginning of word */
		if (s && *s) {
			gboolean cited_end;
			
			cited_end = FALSE;
			uc = g_utf8_get_char (s);
			
			/* go to end of word */
			while (html_selection_spell_word (uc, &cited_end) || (!cited && cited_end)) {
				cited_end = FALSE;
				s  = g_utf8_next_char (s);
				uc = g_utf8_get_char (s);
				if (uc == 0)
					break;
			}
			*sr = s;
			return s ? g_strndup (begin, s - begin) : g_strdup (begin);
		} else
			return NULL;
	}
}


void
e_msg_composer_ignore (EMsgComposer *composer, const char *str)
{
	CORBA_Environment ev;
	char *word;
	
	if (!str)
		return;
	
	CORBA_exception_init (&ev);
	while ((word = next_word (str, &str))) {
		/* printf ("ignore word %s\n", word); */
		GNOME_GtkHTML_Editor_Engine_ignoreWord (composer->editor_engine, word, &ev);
		g_free (word);
	}
	CORBA_exception_free (&ev);
}


void
e_msg_composer_drop_editor_undo (EMsgComposer *composer)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_dropUndo (composer->editor_engine, &ev);
	CORBA_exception_free (&ev);
}


gboolean
e_msg_composer_request_close_all (void)
{
	GSList *p, *pnext;

	for (p = all_composers; p != NULL; p = pnext) {
		pnext = p->next;
		do_exit (E_MSG_COMPOSER (p->data));
	}

	if (all_composers == NULL)
		return TRUE;
	else
		return FALSE;
}

void
e_msg_composer_check_autosave(GtkWindow *parent)
{
	if (am == NULL)
		am = autosave_manager_new();

	if (am->ask) {
		am->ask = FALSE;
		autosave_manager_query_load_orphans (am, parent);
		am->ask = TRUE;
	}
}
