/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.c
 *
 * Copyright (C) 1999  Ximian, Inc.
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

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <gal/unicode/gunicode.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-exec.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-window-icon.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-object-client.h>
#include <bonobo/bonobo-stream-memory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>
#include <libgnomevfs/gnome-vfs.h>

#include <glade/glade.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-scroll-frame.h>
#include <gal/e-text/e-entry.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/htmlselection.h>

/*#include <addressbook/backend/ebook/e-card.h>*/

#include "widgets/misc/e-charset-picker.h"

#include "camel/camel.h"
#include "camel/camel-charset-map.h"
#include "camel/camel-session.h"

#include "mail/mail.h"
#include "mail/mail-crypto.h"
#include "mail/mail-tools.h"
#include "mail/mail-ops.h"
#include "mail/mail-mt.h"
#include "mail/mail-session.h"

#include "e-util/e-html-utils.h"

#include "e-msg-composer.h"
#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"
#include "e-msg-composer-select-file.h"

#include "evolution-shell-component-utils.h"

#include "Editor.h"
#include "listener.h"

#define GNOME_GTKHTML_EDITOR_CONTROL_ID "OAFIID:GNOME_GtkHTML_Editor"


#define DEFAULT_WIDTH 600
#define DEFAULT_HEIGHT 500

enum {
	SEND,
	POSTPONE,
	SAVE_DRAFT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
	DND_TYPE_MESSAGE_RFC822,
	DND_TYPE_TEXT_URI_LIST,
	DND_TYPE_TEXT_VCARD,
};

static GtkTargetEntry drop_types[] = {
	{ "message/rfc822", 0, DND_TYPE_MESSAGE_RFC822 },
	{ "text/uri-list", 0, DND_TYPE_TEXT_URI_LIST },
	{ "text/x-vcard", 0, DND_TYPE_TEXT_VCARD },
};

static const int num_drop_types = sizeof (drop_types) / sizeof (drop_types[0]);

static GnomeAppClass *parent_class = NULL;

/* local prototypes */
static GList *add_recipients   (GList *list, const char *recips, gboolean decode);

static void message_rfc822_dnd (EMsgComposer *composer, CamelStream *stream);

/* used by e_msg_composer_add_message_attachments() */
static void add_attachments_from_multipart (EMsgComposer *composer, CamelMultipart *multipart,
					    gboolean just_inlines, int depth);

/* used by e_msg_composer_new_with_message() */
static void handle_multipart_alternative (EMsgComposer *composer, CamelMultipart *multipart, int depth);

static void handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, int depth);

static void set_editor_signature (EMsgComposer *composer);



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
	bonobo_object_unref (BONOBO_OBJECT(stream));
	
	return text;
}

#define LINE_LEN 72

static CamelMimePartEncodingType
best_encoding (GByteArray *buf, const char *charset)
{
	char *in, *out, outbuf[256], *ch;
	size_t inlen, outlen;
	int status, count = 0;
	iconv_t cd;
	
	if (!charset)
		return -1;
	
	cd = iconv_open (charset, "utf-8");
	if (cd == (iconv_t) -1)
		return -1;
	
	in = buf->data;
	inlen = buf->len;
	do {
		out = outbuf;
		outlen = sizeof (outbuf);
		status = iconv (cd, &in, &inlen, &out, &outlen);
		for (ch = out - 1; ch >= outbuf; ch--) {
			if ((unsigned char)*ch > 127)
				count++;
		}
	} while (status == -1 && errno == E2BIG);
	iconv_close (cd);
	
	if (status == -1)
		return -1;
	
	if (count == 0)
		return CAMEL_MIME_PART_ENCODING_7BIT;
	else if (count <= buf->len * 0.17)
		return CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		return CAMEL_MIME_PART_ENCODING_BASE64;
}

static const char *
best_charset (GByteArray *buf, const char *default_charset, CamelMimePartEncodingType *encoding)
{
	const char *charset;
	
	/* First try US-ASCII */
	*encoding = best_encoding (buf, "US-ASCII");
	if (*encoding == CAMEL_MIME_PART_ENCODING_7BIT)
		return NULL;
	
	/* Next try the user-specified charset for this message */
	charset = default_charset;
	*encoding = best_encoding (buf, charset);
	if (*encoding != -1)
		return charset;
	
	/* Now try the user's default charset from the mail config */
	charset = mail_config_get_default_charset ();
	*encoding = best_encoding (buf, charset);
	if (*encoding != -1)
		return charset;
	
	/* Try to find something that will work */
	charset = camel_charset_best (buf->data, buf->len);
	if (!charset)
		*encoding = CAMEL_MIME_PART_ENCODING_7BIT;
	else
		*encoding = best_encoding (buf, charset);
	return charset;
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
build_message (EMsgComposer *composer)
{
	EMsgComposerAttachmentBar *attachment_bar =
		E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar);
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (composer->hdrs);
	CamelMimeMessage *new;
	GByteArray *data;
	CamelDataWrapper *plain, *html, *current;
	CamelMimePartEncodingType plain_encoding;
	const char *charset;
	CamelContentType *type;
	CamelStream *stream;
	CamelMultipart *body = NULL;
	CamelMimePart *part;
	CamelException ex;
	int i;
	
	if (composer->persist_stream_interface == CORBA_OBJECT_NIL)
		return NULL;
	
	/* evil kludgy hack for Redirect */
	if (composer->redirect) {
		e_msg_composer_hdrs_to_redirect (hdrs, composer->redirect);
		camel_object_ref (CAMEL_OBJECT (composer->redirect));
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
		plain_encoding = CAMEL_MIME_PART_ENCODING_7BIT;
		for (i = 0; composer->mime_body[i]; i++) {
			if ((unsigned char)composer->mime_body[i] > 127) {
				plain_encoding = CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE;
				break;
			}
		}
		data = g_byte_array_new ();
		g_byte_array_append (data, composer->mime_body, i);
		type = header_content_type_decode (composer->mime_type);
	} else {
		data = get_text (composer->persist_stream_interface, "text/plain");
		if (!data) {
			/* The component has probably died */
			camel_object_unref (CAMEL_OBJECT (new));
			return NULL;
		}
		
		/* FIXME: we may want to do better than this... */
		charset = best_charset (data, composer->charset, &plain_encoding);
		type = header_content_type_new ("text", "plain");
		if (charset)
			header_content_type_set_param (type, "charset", charset);
	}
	
	plain = camel_data_wrapper_new ();
	stream = camel_stream_mem_new_with_byte_array (data);
	camel_data_wrapper_construct_from_stream (plain, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	camel_data_wrapper_set_mime_type_field (plain, type);
	header_content_type_unref (type);
	
	if (composer->send_html) {
		clear_current_images (composer);
		
		data = get_text (composer->persist_stream_interface, "text/html");		
		if (!data) {
			/* The component has probably died */
			camel_object_unref (CAMEL_OBJECT (new));
			camel_object_unref (CAMEL_OBJECT (plain));
			return NULL;
		}
		html = camel_data_wrapper_new ();
		stream = camel_stream_mem_new_with_byte_array (data);
		camel_data_wrapper_construct_from_stream (html, stream);
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_data_wrapper_set_mime_type (html, "text/html; charset=utf-8");
		
		/* Build the multipart/alternative */
		body = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
						  "multipart/alternative");
		camel_multipart_set_boundary (body, NULL);
		
		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), plain);
		camel_object_unref (CAMEL_OBJECT (plain));
		camel_mime_part_set_encoding (part, plain_encoding);
		camel_multipart_add_part (body, part);
		camel_object_unref (CAMEL_OBJECT (part));
		
		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), html);
		camel_object_unref (CAMEL_OBJECT (html));
		camel_multipart_add_part (body, part);
		camel_object_unref (CAMEL_OBJECT (part));
		
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
			camel_object_unref (CAMEL_OBJECT (body));
			camel_multipart_add_part (html_with_images, part);
			camel_object_unref (CAMEL_OBJECT (part));
			
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
		camel_object_unref (CAMEL_OBJECT (current));
		camel_multipart_add_part (multipart, part);
		camel_object_unref (CAMEL_OBJECT (part));
		
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
	
	if (composer->pgp_sign || composer->pgp_encrypt) {
		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), current);
		if (current == plain)
			camel_mime_part_set_encoding (part, plain_encoding);
		camel_object_unref (CAMEL_OBJECT (current));
		
		if (composer->pgp_sign) {
			CamelInternetAddress *from = NULL;
			const char *pgpid;
			
			camel_exception_init (&ex);
			
			if (hdrs->account && hdrs->account->pgp_key) {
				pgpid = hdrs->account->pgp_key;
			} else {
				/* time for plan b */
				from = e_msg_composer_hdrs_get_from (hdrs);
				camel_internet_address_get (from, 0, NULL, &pgpid);
			}
			
			printf ("build_message(): pgpid = '%s'\n", pgpid);
			
			mail_crypto_pgp_mime_part_sign (&part, pgpid, CAMEL_CIPHER_HASH_SHA1, &ex);
			
			if (from)
				camel_object_unref (CAMEL_OBJECT (from));
			
			if (camel_exception_is_set (&ex))
				goto exception;
		}
		
		if (composer->pgp_encrypt) {
			/* FIXME: recipients should be an array of key ids rather than email addresses */
			const CamelInternetAddress *addr;
			const char *address;
			GPtrArray *recipients;
			int i, len;
			
			camel_exception_init (&ex);
			recipients = g_ptr_array_new ();
			
			/* check to see if we should encrypt to self */
			if (hdrs->account && hdrs->account->pgp_encrypt_to_self) {
				CamelInternetAddress *from = NULL;
				
				if (hdrs->account->pgp_key) {
					address = hdrs->account->pgp_key;
				} else {
					/* time for plan b */
					from = e_msg_composer_hdrs_get_from (hdrs);
					camel_internet_address_get (from, 0, NULL, &address);
				}
				
				g_ptr_array_add (recipients, g_strdup (address));
				
				if (from)
					camel_object_unref (CAMEL_OBJECT (from));
			}
			
			addr = camel_mime_message_get_recipients (new, CAMEL_RECIPIENT_TYPE_TO);
			len = camel_address_length (CAMEL_ADDRESS (addr));
			for (i = 0; i < len; i++) {
				camel_internet_address_get (addr, i, NULL, &address);
				g_ptr_array_add (recipients, g_strdup (address));
			}
			
			addr = camel_mime_message_get_recipients (new, CAMEL_RECIPIENT_TYPE_CC);
			len = camel_address_length (CAMEL_ADDRESS (addr));
			for (i = 0; i < len; i++) {
				camel_internet_address_get (addr, i, NULL, &address);
				g_ptr_array_add (recipients, g_strdup (address));
			}
			
			addr = camel_mime_message_get_recipients (new, CAMEL_RECIPIENT_TYPE_BCC);
			len = camel_address_length (CAMEL_ADDRESS (addr));
			for (i = 0; i < len; i++) {
				camel_internet_address_get (addr, i, NULL, &address);
				g_ptr_array_add (recipients, g_strdup (address));
			}
			
			mail_crypto_pgp_mime_part_encrypt (&part, recipients, &ex);
			for (i = 0; i < recipients->len; i++)
				g_free (recipients->pdata[i]);
			g_ptr_array_free (recipients, TRUE);
			if (camel_exception_is_set (&ex))
				goto exception;
		}
		
		current = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		camel_object_ref (CAMEL_OBJECT (current));
		camel_object_unref (CAMEL_OBJECT (part));
	}
	
	camel_medium_set_content_object (CAMEL_MEDIUM (new), current);
	if (current == plain)
		camel_mime_part_set_encoding (CAMEL_MIME_PART (new), plain_encoding);
	camel_object_unref (CAMEL_OBJECT (current));
	
#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
	if (composer->smime_sign) {
		CamelInternetAddress *from = NULL;
		CamelMimeMessage *smime_mesg;
		const char *certname;
		
		camel_exception_init (&ex);
		
		if (hdrs->account && hdrs->account->smime_key) {
			certname = hdrs->account->smime_key;
		} else {
			/* time for plan b */
			from = e_msg_composer_hdrs_get_from (hdrs);
			camel_internet_address_get (from, 0, NULL, &certname);
		}
		
		smime_mesg = mail_crypto_smime_sign (new, certname, TRUE, TRUE, &ex);
		
		if (from)
			camel_object_unref (CAMEL_OBJECT (from));
		
		if (camel_exception_is_set (&ex))
			goto exception;
		
		camel_object_unref (CAMEL_OBJECT (new));
		new = smime_mesg;
	}
	
	if (composer->smime_encrypt) {
		/* FIXME: we should try to get the preferred cert "nickname" for each recipient */
		const CamelInternetAddress *addr = NULL;
		CamelInternetAddress *from = NULL;
		CamelMimeMessage *smime_mesg;
		const char *address;
		GPtrArray *recipients;
		int i, len;
		
		camel_exception_init (&ex);
		recipients = g_ptr_array_new ();
		
		/* check to see if we should encrypt to self */
		if (hdrs->account && hdrs->account->smime_encrypt_to_self) {
			if (hdrs->account->smime_key) {
				address = hdrs->account->smime_key;
			} else {
				/* time for plan b */
				from = e_msg_composer_hdrs_get_from (hdrs);
				camel_internet_address_get (from, 0, NULL, &address);
			}
			
			g_ptr_array_add (recipients, g_strdup (address));
			
			if (from)
				camel_object_unref (CAMEL_OBJECT (addr));
		}
		
		addr = camel_mime_message_get_recipients (new, CAMEL_RECIPIENT_TYPE_TO);
		len = camel_address_length (CAMEL_ADDRESS (addr));
		for (i = 0; i < len; i++) {
			camel_internet_address_get (addr, i, NULL, &address);
			g_ptr_array_add (recipients, g_strdup (address));
		}
		
		addr = camel_mime_message_get_recipients (new, CAMEL_RECIPIENT_TYPE_CC);
		len = camel_address_length (CAMEL_ADDRESS (addr));
		for (i = 0; i < len; i++) {
			camel_internet_address_get (addr, i, NULL, &address);
			g_ptr_array_add (recipients, g_strdup (address));
		}
		
		addr = camel_mime_message_get_recipients (new, CAMEL_RECIPIENT_TYPE_BCC);
		len = camel_address_length (CAMEL_ADDRESS (addr));
		for (i = 0; i < len; i++) {
			camel_internet_address_get (addr, i, NULL, &address);
			g_ptr_array_add (recipients, g_strdup (address));
		}
		
		from = e_msg_composer_hdrs_get_from (E_MSG_COMPOSER_HDRS (composer->hdrs));
		camel_internet_address_get (from, 0, NULL, &address);
		
		smime_mesg = mail_crypto_smime_encrypt (new, address, recipients, &ex);
		
		camel_object_unref (CAMEL_OBJECT (from));
		
		for (i = 0; i < recipients->len; i++)
			g_free (recipients->pdata[i]);
		g_ptr_array_free (recipients, TRUE);
		
		if (camel_exception_is_set (&ex))
			goto exception;
		
		camel_object_unref (CAMEL_OBJECT (new));
		new = smime_mesg;
	}
	
	/* FIXME: what about mail_crypto_smime_certsonly()?? */
	
	/* FIXME: what about mail_crypto_smime_envelope()?? */
	
#endif /* HAVE_NSS */
	
	/* Attach whether this message was written in HTML */
	camel_medium_set_header (CAMEL_MEDIUM (new), "X-Evolution-Format",
				 composer->send_html ? "text/html" : "text/plain");
	
	return new;
	
 exception:
	
	if (part != CAMEL_MIME_PART (new))
		camel_object_unref (CAMEL_OBJECT (part));
	
	camel_object_unref (CAMEL_OBJECT (new));
	
	if (camel_exception_is_set (&ex)) {
		GtkWidget *dialog;
		
		dialog = gnome_error_dialog_parented (camel_exception_get_description (&ex),
						      GTK_WINDOW (composer));
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		camel_exception_clear (&ex);
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
	const char *charset;
	char *content;
	int fd;
	
	fd = open (file_name, O_RDONLY | O_CREAT, 0644);
	if (fd == -1) {
		char *msg;

		if (warn) {
			msg = g_strdup_printf (_("Error while reading file %s:\n%s"),
					       file_name, g_strerror (errno));
			gnome_error_dialog (msg);
			g_free (msg);
		}
		return g_strdup ("");
	}
	
	stream = camel_stream_fs_new_with_fd (fd);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	charset = composer ? composer->charset : mail_config_get_default_charset ();
	charenc = (CamelMimeFilter *) camel_mime_filter_charset_new_convert (charset, "utf-8");
	camel_stream_filter_add (filtered_stream, charenc);
	camel_object_unref (CAMEL_OBJECT (charenc));
	
	if (want_html) {
		html = camel_mime_filter_tohtml_new (flags, 0);
		camel_stream_filter_add (filtered_stream, html);
		camel_object_unref (CAMEL_OBJECT (html));
	}
	
	memstream = (CamelStreamMem *) camel_stream_mem_new ();
	buffer = g_byte_array_new ();
	camel_stream_mem_set_byte_array (memstream, buffer);
	
	camel_stream_write_to_stream (CAMEL_STREAM (filtered_stream), CAMEL_STREAM (memstream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (memstream));
	
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
	
	return get_file_content (NULL, sigfile, !in_html, 0, FALSE);
}

static void
prepare_engine (EMsgComposer *composer)
{
	CORBA_Environment ev;
	
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	/* printf ("prepare_engine\n"); */
	
	CORBA_exception_init (&ev);
	composer->editor_engine = (GNOME_GtkHTML_Editor_Engine) bonobo_object_client_query_interface
		(bonobo_widget_get_server (BONOBO_WIDGET (composer->editor)), "IDL:GNOME/GtkHTML/Editor/Engine:1.0", &ev);
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

static gchar *
get_signature_html (EMsgComposer *composer)
{
	gboolean format_html = FALSE;
	char *text, *html = NULL, *sig_file = NULL, *script = NULL;
	static gboolean random_initialized = FALSE;
	
	if (composer->signature) {
		sig_file = composer->signature->filename;
		format_html = composer->signature->html;
		script = composer->signature->script;
	} else if (composer->random_signature) {
		GList *l;
		gint pos;

		if (!random_initialized) {
			printf ("initialize random generator\n");
			srand (time (NULL));
			random_initialized = TRUE;
		}
		pos = (int) (((gdouble) mail_config_get_signatures_random ())*rand()/(RAND_MAX+1.0));
		printf ("using %d sig\n", pos);

		for (l = mail_config_get_signature_list (); l; l = l->next) {
			MailConfigSignature *sig = (MailConfigSignature *) l->data;

			if (sig->random) {
				if (pos == 0) {
					printf ("using %s\n", sig->name);
					sig_file = sig->filename;
					script = sig->script;
					format_html = sig->html;
					break;
				}
				pos --;
			}
		}
	}
	if (!sig_file)
		return NULL;
	printf ("sig file: %s\n", sig_file);

	mail_config_signature_run_script (script);
	text = e_msg_composer_get_sig_file_content (sig_file, format_html);
	/* printf ("text: %s\n", text); */
	if (text) {
		/* The signature dash convention ("-- \n") is specified in the
		 * "Son of RFC 1036": http://www.chemie.fu-berlin.de/outerspace/netnews/son-of-1036.html,
		 * section 4.3.2.
		 */
		html = g_strdup_printf ("<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"signature\" value=\"1\">-->"
					"<TABLE WIDTH=\"100%%\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>"
					"%s%s%s%s"
					"</TD></TR></TABLE>",
					format_html ? "" : "<PRE>\n",
					format_html || !strncmp ("-- \n", text, 4) ? "" : "-- \n",
					text,
					format_html ? "" : "</PRE>\n");
		g_free (text);
		text = html;
	}
	
	return text;
}

static void
set_editor_text (EMsgComposer *composer, const char *text)
{
	Bonobo_PersistStream persist;
	BonoboStream *stream;
	BonoboWidget *editor;
	CORBA_Environment ev;
	
	editor = BONOBO_WIDGET (composer->editor);
	
	CORBA_exception_init (&ev);
	persist = (Bonobo_PersistStream) bonobo_object_client_query_interface (
		bonobo_widget_get_server (editor), "IDL:Bonobo/PersistStream:1.0", &ev);
	
	g_return_if_fail (persist != CORBA_OBJECT_NIL);
	
	stream = bonobo_stream_mem_create (text, strlen (text),
					   TRUE, FALSE);
	Bonobo_PersistStream_load (persist, (Bonobo_Stream)bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
				   "text/html", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* FIXME. Some error message. */
		return;
	}
	if (ev._major != CORBA_SYSTEM_EXCEPTION)
		CORBA_Object_release (persist, &ev);
	
	Bonobo_Unknown_unref (persist, &ev);
	CORBA_exception_free (&ev);
	bonobo_object_unref (BONOBO_OBJECT(stream));
}

static void
set_config (EMsgComposer *composer, char *key, int val)
{
	char *full_key;
	
	if (composer->config_db == CORBA_OBJECT_NIL)
		return;
	
	full_key = g_strconcat ("/Mail/Composer/", key, NULL);
	
	bonobo_config_set_long (composer->config_db, full_key, val, NULL);
	
	g_free (full_key);
}


/* Commands.  */

static void
show_attachments (EMsgComposer *composer,
		  gboolean show)
{
	if (show) {
		gtk_widget_show (composer->attachment_scroll_frame);
		gtk_widget_show (composer->attachment_bar);
	} else {
		gtk_widget_hide (composer->attachment_scroll_frame);
		gtk_widget_hide (composer->attachment_bar);
	}
	
	composer->attachment_bar_visible = show;
	
	/* Update the GUI.  */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewAttach",
		"state", show ? "1" : "0", NULL);
}

static void
save (EMsgComposer *composer,
      const char *file_name)
{
	CORBA_Environment ev;
	char *my_file_name;
	int fd;
	
	if (file_name != NULL)
		my_file_name = g_strdup (file_name);
	else
		my_file_name = e_msg_composer_select_file (composer, _("Save as..."));
	
	if (my_file_name == NULL)
		return;
	
	/* check to see if we already have the file */
	if ((fd = open (my_file_name, O_RDONLY | O_CREAT | O_EXCL, 0777)) == -1) {
		GtkWidget *dialog, *label;
		
		dialog = gnome_dialog_new (_("Warning!"),
					   GNOME_STOCK_BUTTON_YES,
					   GNOME_STOCK_BUTTON_NO,
					   NULL);
		label = gtk_label_new (_("File exists, overwrite?"));
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 0);
		
		switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog))) {
		case -1:
			gtk_widget_destroy (dialog);
			return;
		case 1:
			return;
		default:
			/* ie, the user hit "Yes" so just continue as normal */
		}
	} else
		close (fd);
	
	CORBA_exception_init (&ev);
	
	Bonobo_PersistFile_save (composer->persist_file_interface, my_file_name, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error saving file: %s"), g_basename (my_file_name));
	}
	
	CORBA_exception_free (&ev);
	
	g_free (my_file_name);
}

static void
load (EMsgComposer *composer, const char *file_name)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	Bonobo_PersistFile_load (composer->persist_file_interface, file_name, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION)
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error loading file: %s"), g_basename (file_name));
	
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

static gboolean
autosave_save_draft (EMsgComposer *composer)
{
	CamelMimeMessage *message;
	CamelStream *stream;
	char *file;
	int fd;
	gboolean success = TRUE;
	
	fd = composer->autosave_fd;
	file = composer->autosave_file;
	
	if (fd == -1) {
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error accessing file: %s"), file);
		return FALSE;
	}
	
	message = e_msg_composer_get_message_draft (composer);
	
	if (message == NULL) {
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Unable to retrieve message from editor"));
		return FALSE;
	}
	
	if (lseek (fd, (off_t)0, SEEK_SET) == -1) {
		camel_object_unref (CAMEL_OBJECT (message));
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Unable to seek on file: %s\n%s"), file, g_strerror (errno));
		return FALSE;
	}
	
	if (ftruncate (fd, (off_t)0) == -1) {
		camel_object_unref (CAMEL_OBJECT (message));
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Unable to truncate file: %s\n%s"), file, g_strerror (errno));
		return FALSE;
	}
	
	/* this does an lseek so we don't have to */
	stream = camel_stream_fs_new_with_fd (fd);
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream) == -1
	    || camel_stream_flush (CAMEL_STREAM (stream)) == -1) {
		e_notice (GTK_WINDOW (composer), GNOME_MESSAGE_BOX_ERROR,
			  _("Error autosaving message: %s\n %s"), file, strerror(errno));
		
		success = FALSE;
	}
	
	/* set the fd to -1 in the stream so camel doesn't close it we want to keep it open */
	CAMEL_STREAM_FS (stream)->fd = -1;
	camel_object_unref (CAMEL_OBJECT (stream));
	
	camel_object_unref (CAMEL_OBJECT (message));
	
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
        
	stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0);
	
	if (stream == NULL) 
		return NULL;
		
	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), stream);
	unlink (filename);
	
	composer = e_msg_composer_new_with_message (msg);
	if (composer) {
		autosave_save_draft (composer);
		
		gtk_signal_connect (GTK_OBJECT (composer), "send",
				    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (composer), "postpone",
				    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
		
		gtk_widget_show (GTK_WIDGET (composer));
	}
	
	camel_object_unref ((CamelObject *)stream);
	return composer;
}

static gboolean
autosave_is_owned (AutosaveManager *am, const char *file)
{
        return g_hash_table_lookup (am->table, file) != NULL;
}

static void
autosave_query_cb (gint reply, gpointer data)
{
	int *yes = data;
	
	*yes = !reply;
}

static void
autosave_manager_query_load_orphans (AutosaveManager *am, EMsgComposer *composer)
{
	GtkWidget *dialog;
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
			if (stat (filename, &st) < 0 || st.st_size == 0) {
				unlink (filename);
				g_free (filename);
				continue;
			}
			match = g_slist_prepend (match, filename);				
		}
	}
	
	closedir (dir);
	
	if (match != NULL) {
		dialog = gnome_question_dialog_parented (_("Ximian Evolution has found unsaved files from a previous session.\n"
							   "Would you like to try to recover them?"),
							 autosave_query_cb, &load, GTK_WINDOW (composer));
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
	
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
	
	return TRUE;
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
		am->id = gtk_timeout_add (AUTOSAVE_INTERVAL, autosave_run, am);
}

static void
autosave_manager_stop (AutosaveManager *am)
{
	if (am->id) {
		gtk_timeout_remove (am->id);
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
		key = g_basename (composer->autosave_file);
		g_hash_table_insert (am->table, key, composer);
		if (am->ask) {
			/* keep recursion out of our bedrooms. */
			am->ask = FALSE;
			autosave_manager_query_load_orphans (am, composer);
			am->ask = TRUE;
		}
	}
	autosave_manager_start (am);
}

static void
autosave_manager_unregister (AutosaveManager *am, EMsgComposer *composer) 
{
	if (!composer->autosave_file)
		return;

	g_hash_table_remove (am->table, g_basename (composer->autosave_file));
	
	/* only remove the file if we can successfully save it */
	/* FIXME this test could probably be more efficient */
	if (autosave_save_draft (composer)) {
		unlink (composer->autosave_file);
	}
	close (composer->autosave_fd);
	g_free (composer->autosave_file);
	composer->autosave_file = NULL;
	
	if (g_hash_table_size (am->table) == 0)
		autosave_manager_stop (am);
}

static void
menu_file_save_draft_cb (BonoboUIComponent *uic, void *data, const char *path)
{
	gtk_signal_emit (GTK_OBJECT (data), signals[SAVE_DRAFT], FALSE);
	e_msg_composer_unset_changed (E_MSG_COMPOSER (data));
}

/* Exit dialog.  (Displays a "Save composition to 'Drafts' before exiting?" warning before actually exiting.)  */

enum { REPLY_YES = 0, REPLY_NO, REPLY_CANCEL };

static void
exit_dialog_cb (int reply, EMsgComposer *composer)
{
	switch (reply) {
	case REPLY_YES:
		gtk_signal_emit (GTK_OBJECT (composer), signals[SAVE_DRAFT], TRUE);
		e_msg_composer_unset_changed (composer);
		break;
	case REPLY_NO:
		gtk_widget_destroy (GTK_WIDGET (composer));
		break;
	case REPLY_CANCEL:
	default:
	}
}

static void
do_exit (EMsgComposer *composer)
{
	GtkWidget *dialog;
	gint button;
	
	if (e_msg_composer_is_dirty (composer)) {
		dialog = gnome_message_box_new (_("This message has not been sent.\n\nDo you wish to save your changes?"),
						GNOME_MESSAGE_BOX_QUESTION,
						GNOME_STOCK_BUTTON_YES,      /* Save */
						GNOME_STOCK_BUTTON_NO,       /* Don't save */
						GNOME_STOCK_BUTTON_CANCEL,   /* Cancel */
						NULL);
	
		gtk_window_set_title (GTK_WINDOW (dialog), _("Warning: Modified Message"));
		gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (composer));
		gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
		button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		
		exit_dialog_cb (button, composer);
	} else {
		gtk_widget_destroy (GTK_WIDGET (composer));
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
	
	file_name = e_msg_composer_select_file (composer, _("Open file"));
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
	if (session && camel_session_is_online (session))
		gtk_signal_emit (GTK_OBJECT (data), signals[SEND]);
	else
		gtk_signal_emit (GTK_OBJECT (data), signals[POSTPONE]);
}

static void
menu_file_send_later_cb (BonoboUIComponent *uic,
			 void *data,
			 const char *path)
{
	gtk_signal_emit (GTK_OBJECT (data), signals[POSTPONE]);
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
	printf ("delete all\n");
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
menu_file_insert_file_cb (BonoboUIComponent *uic,
			  void *data,
			  const char *path)
{
	EMsgComposer *composer;
	char *file_name;
	char *html;
	CORBA_Environment ev;
	
	composer = E_MSG_COMPOSER (data);
	
	file_name = e_msg_composer_select_file (composer, _("Insert File"));
	if (file_name == NULL)
		return;
	
	html = get_file_content (composer, file_name, TRUE, E_TEXT_TO_HTML_PRE, TRUE);
	if (html == NULL)
		return;
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_freeze (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-position-save", &ev);
	GNOME_GtkHTML_Editor_Engine_undoBegin (composer->editor_engine, "Insert file", "Uninsert file", &ev);
	if (!GNOME_GtkHTML_Editor_Engine_isParagraphEmpty (composer->editor_engine, &ev))
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "insert-paragraph", &ev);
	GNOME_GtkHTML_Editor_Engine_insertHTML (composer->editor_engine, html, &ev);
	GNOME_GtkHTML_Editor_Engine_undoEnd (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-position-restore", &ev);
	GNOME_GtkHTML_Editor_Engine_thaw (composer->editor_engine, &ev);
	CORBA_exception_free (&ev);
	
	g_free (html);
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

	BONOBO_UI_VERB ("FileOpen",   menu_file_open_cb),
	BONOBO_UI_VERB ("FileSave",   menu_file_save_cb),
	BONOBO_UI_VERB ("FileSaveAs", menu_file_save_as_cb),
	BONOBO_UI_VERB ("FileSaveDraft", menu_file_save_draft_cb),
	BONOBO_UI_VERB ("FileClose",  menu_file_close_cb),
	
	BONOBO_UI_VERB ("FileInsertFile", menu_file_insert_file_cb),
	BONOBO_UI_VERB ("FileAttach",     menu_file_add_attachment_cb),
	
	BONOBO_UI_VERB ("FileSend",       menu_file_send_cb),
	BONOBO_UI_VERB ("FileSendLater",  menu_file_send_later_cb),
	
	BONOBO_UI_VERB ("DeleteAll",  menu_edit_delete_all_cb),
	
	BONOBO_UI_VERB_END
};

static EPixmap pixcache [] = {
	E_PIXMAP ("/Toolbar/FileAttach", "buttons/add-attachment.png"),
	E_PIXMAP ("/Toolbar/FileSend", "buttons/send-24.png"),
	
/*	E_PIXMAP ("/menu/Insert/FileAttach", "buttons/add-attachment.png"), */
	E_PIXMAP ("/commands/FileSend", "send-16.png"),
	E_PIXMAP ("/commands/FileSendLater", "send-later-16.png"),
	E_PIXMAP ("/commands/FileSave", "save-16.png"),
	E_PIXMAP ("/commands/FileSaveAs", "save-as-16.png"),
	
	E_PIXMAP_END
};

static void
signature_regenerate_cb (BonoboUIComponent *uic, gpointer user_data, const char *path)
{
	printf ("signature_regenerate_cb: %s\n", path);

	e_msg_composer_show_sig_file (E_MSG_COMPOSER (user_data));
}

static void
signature_cb (BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type,
	      const char *state, gpointer user_data)
{
	EMsgComposer *composer = (EMsgComposer *) user_data;

	printf ("signature_cb: %s (%s)\n", path, state);

	if (state && *state == '1') {
		if (path && !strncmp (path, "Signature", 9)) {
			MailConfigSignature *old_sig;
			gboolean old_random;

			old_sig = composer->signature;
			old_random = composer->random_signature;

			printf ("I'm going to set signature (%d)\n", atoi (path + 9));
			if (path [9] == 'N') {
				composer->signature = NULL;
				composer->random_signature = FALSE;
			} else if (path [9] == 'R') {
				composer->signature = NULL;
				composer->random_signature = TRUE;
			} else {
				composer->signature = g_list_nth_data (mail_config_get_signature_list (), atoi (path + 9));
				composer->random_signature = FALSE;
			}
			if (old_sig != composer->signature || old_random != composer->random_signature)
				e_msg_composer_show_sig_file (composer);
		}
	}

	printf ("signature_cb end\n");
}

static void setup_signatures_menu (EMsgComposer *composer);

static void
remove_signature_list (EMsgComposer *composer)
{
	gchar path [64];
	gint len = g_list_length (mail_config_get_signature_list ());

	bonobo_ui_component_rm (composer->uic, "/menu/Edit/EditMisc/EditSignaturesSubmenu/SeparatorList", NULL);
	bonobo_ui_component_rm (composer->uic, "/menu/Edit/EditMisc/EditSignaturesSubmenu/SeparatorRegenerate", NULL);
	bonobo_ui_component_rm (composer->uic, "/menu/Edit/EditMisc/EditSignaturesSubmenu/SignatureRegenerate", NULL);
	for (; len; len --) {
		g_snprintf (path, 64, "/menu/Edit/EditMisc/EditSignaturesSubmenu/Signature%d", len - 1);
		bonobo_ui_component_rm (composer->uic, path, NULL);
	}
}

static void
sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, EMsgComposer *composer)
{
	gchar *path;

	bonobo_ui_component_freeze (composer->uic, NULL);
	switch (event) {
	case MAIL_CONFIG_SIG_EVENT_DELETED:
		if (sig == composer->signature)
			composer->signature = NULL;
		path = g_strdup_printf ("/menu/Edit/EditMisc/EditSignaturesSubmenu/Signature%d",
					g_list_length (mail_config_get_signature_list ()));
		bonobo_ui_component_rm (composer->uic, path, NULL);
		g_free (path);
		setup_signatures_menu (composer);
		break;
	case MAIL_CONFIG_SIG_EVENT_RANDOM_OFF:
		composer->random_signature = FALSE;
		bonobo_ui_component_rm (composer->uic, "/menu/Edit/EditMisc/EditSignaturesSubmenu/SignatureRandom", NULL);
		bonobo_ui_component_rm (composer->uic, "/menu/Edit/EditMisc/EditSignaturesSubmenu/SeparatorRandom", NULL);
		setup_signatures_menu (composer);
		break;
	case MAIL_CONFIG_SIG_EVENT_RANDOM_ON:
		remove_signature_list (composer);
		setup_signatures_menu (composer);
		break;
	case MAIL_CONFIG_SIG_EVENT_ADDED:
	case MAIL_CONFIG_SIG_EVENT_NAME_CHANGED:
		setup_signatures_menu (composer);
	}
	bonobo_ui_component_thaw (composer->uic, NULL);
}

static void
setup_signatures_menu (EMsgComposer *composer)
{
	GList *l, *list;
	GString *str;
	gchar *line;
	gint i, len = 0;

	str = g_string_new ("<submenu name=\"EditSignaturesSubmenu\" _label=\"Signatures\">\n"
			    "<menuitem name=\"SignatureNone\" _label=\"None\" verb=\"SignatureNone\""
			    " type=\"radio\" group=\"signatures_group\"/>\n");
	if (mail_config_get_signatures_random ()) {
		g_string_append (str,
				 "<separator name=\"SeparatorRandom\"/>\n"
				 "<menuitem name=\"SignatureRandom\" _label=\"Random\" verb=\"SignatureRandom\""
				 " type=\"radio\" group=\"signatures_group\"/>\n");
	}

	list = mail_config_get_signature_list ();
	if (list) {

		g_string_append (str, "<separator name=\"SeparatorList\"/>");

		for (l = list; l; len ++, l = l->next) {
			line = g_strdup_printf ("<menuitem name=\"Signature%d\" _label=\"%s\""
						" verb=\"Signature%d\" type=\"radio\" group=\"signatures_group\"/>\n",
						len, ((MailConfigSignature *)l->data)->name, len);
			g_string_append (str, line);
			g_free (line);
		}
	}

	g_string_append (str,
			 "<separator name=\"SeparatorRegenerate\"/>\n"
			 "<menuitem name=\"SignatureRegenerate\" _label=\"_Regenerate\""
			 " verb=\"SignatureRegenerate\" accel=\"*Ctrl**Shift*G\"/>");
	g_string_append (str, "</submenu>\n");

	bonobo_ui_component_set_translate (composer->uic, "/menu/Edit/EditMisc/", str->str, NULL);
	bonobo_ui_component_set (composer->uic, "/menu/Edit/EditMisc/", "<separator/>", NULL);

	bonobo_ui_component_add_listener (composer->uic, "SignatureNone", signature_cb, composer);
	bonobo_ui_component_add_listener (composer->uic, "SignatureRandom", signature_cb, composer);
	bonobo_ui_component_add_verb (composer->uic, "SignatureRegenerate", signature_regenerate_cb, composer);

	for (i = 0; i < len; i ++) {
		g_string_sprintf (str, "Signature%d", i + 1);
		bonobo_ui_component_add_listener (composer->uic, str->str, signature_cb, composer);
	}
	g_string_free (str, TRUE);
}

static void
setup_ui (EMsgComposer *composer)
{
	BonoboUIContainer *container;
	char *default_charset;
	gboolean hide_smime;
	
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (composer));
	
	composer->uic = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (composer->uic, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	
	bonobo_ui_component_add_verb_list_with_data (composer->uic, verbs, composer);
	
        /* Customize Toolbar thingie */
	bonobo_ui_engine_config_set_path (bonobo_window_get_ui_engine (BONOBO_WINDOW (composer)),
                                          "/evolution/UIConf/composer");
	
	bonobo_ui_component_freeze (composer->uic, NULL);
	
	bonobo_ui_util_set_ui (composer->uic, EVOLUTION_DATADIR,
			       "evolution-message-composer.xml",
			       "evolution-message-composer");
	
	e_pixmaps_update (composer->uic, pixcache);
	
	/* Populate the Charset Encoding menu and default it to whatever the user
	   chose as his default charset in the mailer */
	default_charset = bonobo_config_get_string (composer->config_db,
						    "/Mail/Format/default_charset",
						    NULL);
	e_charset_picker_bonobo_ui_populate (composer->uic, "/menu/Edit/EncodingPlaceholder",
					     default_charset,
					     menu_changed_charset_cb,
					     composer);
	g_free (default_charset);
	
	if (!session || !camel_session_is_online (session)) {
		char *tooltip;
		
		/* Move the accelerator from Send to Send Later */
		bonobo_ui_component_set_prop (
			composer->uic, "/commands/FileSend",
			"accel", NULL, NULL);
		bonobo_ui_component_set_prop (
			composer->uic, "/commands/FileSendLater",
			"accel", "*Ctrl*Return", NULL);
		
		/* Update the FileSend tooltip to be the same as the FileSendLater tooltip... */
		tooltip = bonobo_ui_component_get_prop (
			composer->uic, "/commands/FileSendLater",
			"tip", NULL);
		bonobo_ui_component_set_prop (
			composer->uic, "/commands/FileSend",
			"tip", tooltip, NULL);
		g_free (tooltip);
	}
	
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
	
	/* View/BCC */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewBCC",
		"state", composer->view_bcc ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewBCC",
		menu_view_bcc_cb, composer);
	
	/* View/CC */
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewCC",
		"state", composer->view_cc ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (
		composer->uic, "ViewCC",
		menu_view_cc_cb, composer);
	
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

	setup_signatures_menu (composer);
	mail_config_signature_register_client ((MailConfigSignatureClient) sig_event_client, composer);
	
	bonobo_ui_component_thaw (composer->uic, NULL);
}


/* Miscellaneous callbacks.  */

static void
attachment_bar_changed_cb (EMsgComposerAttachmentBar *bar,
			   void *data)
{
	EMsgComposer *composer;
	gboolean show = FALSE;
	
	composer = E_MSG_COMPOSER (data);
	
	if (e_msg_composer_attachment_bar_get_num_attachments (bar) > 0)
		show = TRUE;
	
	e_msg_composer_show_attachments (composer, show);
	
	/* Mark the composer as changed so it prompts about unsaved
           changes on close */
	e_msg_composer_set_changed (composer);
}

static void
subject_changed_cb (EMsgComposerHdrs *hdrs,
		    gchar *subject,
		    void *data)
{
	EMsgComposer *composer;
	
	composer = E_MSG_COMPOSER (data);
	
	if (strlen (subject))
		gtk_window_set_title (GTK_WINDOW (composer), subject);
	else
		gtk_window_set_title (GTK_WINDOW (composer),
				      _("Compose a message"));
	g_free (subject);
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
		
		camel_object_unref (CAMEL_OBJECT (iaddr));
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
	
	destv = e_destination_list_to_vector_sized (list, n);
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
		const MailConfigAccount *account = hdrs->account;
		
		e_msg_composer_set_pgp_sign (composer, account->pgp_always_sign);
		e_msg_composer_set_smime_sign (composer, account->smime_always_sign);
		update_auto_recipients (hdrs, UPDATE_AUTO_CC, account->always_cc ? account->cc_addrs : NULL);
		update_auto_recipients (hdrs, UPDATE_AUTO_BCC, account->always_bcc ? account->bcc_addrs : NULL);
	} else {
		update_auto_recipients (hdrs, UPDATE_AUTO_CC, NULL);
		update_auto_recipients (hdrs, UPDATE_AUTO_BCC, NULL);
	}

	set_editor_signature (composer);
	e_msg_composer_show_sig_file (composer);
}


/* GtkObject methods.  */

static void
composer_shutdown (GtkObject *object)
{
	/* When destroy() is called, the contents of the window
	 * (including the remote editor control) will already have
	 * been destroyed, so we have to do this here.
	 */
	autosave_manager_unregister (am, E_MSG_COMPOSER (object));
	if (GTK_OBJECT_CLASS (parent_class)->shutdown != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->shutdown) (object);
}

static void
destroy (GtkObject *object)
{
	EMsgComposer *composer;
	CORBA_Environment ev;
	
	composer = E_MSG_COMPOSER (object);

	mail_config_signature_register_client ((MailConfigSignatureClient) sig_event_client, composer);

	CORBA_exception_init (&ev);

	if (composer->config_db) {
		Bonobo_ConfigDatabase_sync (composer->config_db, &ev);
		bonobo_object_release_unref (composer->config_db, NULL);
	}
	composer->config_db = NULL;

	if (composer->uic)
		bonobo_object_unref (BONOBO_OBJECT (composer->uic));
	composer->uic = NULL;
	
	/* FIXME?  I assume the Bonobo widget will get destroyed
           normally?  */
	
	if (composer->address_dialog != NULL)
		gtk_widget_destroy (composer->address_dialog);
	if (composer->hdrs != NULL)
		gtk_widget_destroy (composer->hdrs);
	
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
	
	CORBA_exception_init (&ev);
	
	if (composer->persist_stream_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_stream_interface, &ev);
		CORBA_Object_release (composer->persist_stream_interface, &ev);
	}
	
	if (composer->persist_file_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->persist_file_interface, &ev);
		CORBA_Object_release (composer->persist_file_interface, &ev);
	}
	
	if (composer->editor_engine != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (composer->editor_engine, &ev);
		CORBA_Object_release (composer->editor_engine, &ev);
	}
	
	CORBA_exception_free (&ev);
	
	if (composer->redirect)
		camel_object_unref (CAMEL_OBJECT (composer->redirect));
	
	if (composer->editor_listener)
		bonobo_object_unref (composer->editor_listener);
	
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
message_rfc822_dnd (EMsgComposer *composer, CamelStream *stream)
{
	CamelMimeParser *mp;
	CamelException *ex;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_stream (mp, stream);
	
	ex = camel_exception_new ();
	
	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		CamelMimeMessage *message;
		CamelMimePart *part;
		
		message = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (message), mp) == -1) {
			camel_object_unref (CAMEL_OBJECT (message));
			break;
		}
		
		part = camel_mime_part_new ();
		camel_mime_part_set_disposition (part, "inline");
		camel_medium_set_content_object (CAMEL_MEDIUM (part),
						 CAMEL_DATA_WRAPPER (message));
		camel_mime_part_set_content_type (part, "message/rfc822");
		e_msg_composer_attachment_bar_attach_mime_part (E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
								part);
		camel_object_unref (CAMEL_OBJECT (message));
		camel_object_unref (CAMEL_OBJECT (part));
		camel_exception_clear (ex);
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (CAMEL_OBJECT (mp));
	camel_exception_free (ex);
}

static void
drag_data_received (EMsgComposer *composer, GdkDragContext *context,
		    int x, int y, GtkSelectionData *selection,
		    guint info, guint time)
{
	char *tmp, *filename, **filenames;
	CamelMimePart *mime_part;
	CamelStream *stream;
	CamelURL *url;
	int i;
	
	switch (info) {
	case DND_TYPE_MESSAGE_RFC822:
		/* write the message(s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, selection->data, selection->length);
		camel_stream_reset (stream);
		
		message_rfc822_dnd (composer, stream);
		camel_object_unref (CAMEL_OBJECT (stream));
		break;
	case DND_TYPE_TEXT_URI_LIST:
		tmp = g_strndup (selection->data, selection->length);
		filenames = g_strsplit (tmp, "\n", 0);
		g_free (tmp);
		
		for (i = 0; filenames[i] != NULL; i++) {
			filename = g_strstrip (filenames[i]);
			
			url = camel_url_new (filename, NULL);
			g_free (filename);
			filename = url->path;
			url->path = NULL;
			camel_url_free (url);
			
			e_msg_composer_attachment_bar_attach
				(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
				 filename);
			
			g_free (filename);
		}
		
		g_free (filenames);
		break;
	case DND_TYPE_TEXT_VCARD:
		printf ("dropping a text/x-vcard\n");
		mime_part = camel_mime_part_new ();
		camel_mime_part_set_content (mime_part, selection->data,
					     selection->length, "text/x-vcard");
		camel_mime_part_set_disposition (mime_part, "inline");
		
		e_msg_composer_attachment_bar_attach_mime_part
			(E_MSG_COMPOSER_ATTACHMENT_BAR (composer->attachment_bar),
			 mime_part);
		
		camel_object_unref (CAMEL_OBJECT (mime_part));
	default:
		printf ("dropping an unknown\n");
		break;
	}
}

typedef void (*GtkSignal_NONE__NONE_INT) (GtkObject *, int, gpointer);

static void marshal_NONE__NONE_INT (GtkObject *object, GtkSignalFunc func,
				    gpointer func_data, GtkArg *args)
{
	GtkSignal_NONE__NONE_INT rfunc;
	
	rfunc = (GtkSignal_NONE__NONE_INT) func;
	(*rfunc)(object, GTK_VALUE_INT (args[0]), func_data);
}


static void
class_init (EMsgComposerClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	
	object_class->shutdown = composer_shutdown;
	object_class->destroy = destroy;
	
	widget_class->delete_event = delete_event;
	
	parent_class = gtk_type_class (bonobo_window_get_type ());
	
	signals[SEND] =
		gtk_signal_new ("send",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, send),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	signals[POSTPONE] =
		gtk_signal_new ("postpone",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, postpone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	signals[SAVE_DRAFT] =
		gtk_signal_new ("save-draft",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerClass, save_draft),
				marshal_NONE__NONE_INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposer *composer)
{
	composer->uic                      = NULL;
	
	composer->hdrs                     = NULL;
	composer->extra_hdr_names          = g_ptr_array_new ();
	composer->extra_hdr_values         = g_ptr_array_new ();
	
	composer->editor                   = NULL;
	
	composer->address_dialog           = NULL;
	
	composer->attachment_bar           = NULL;
	composer->attachment_scroll_frame  = NULL;
	
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
	
	composer->redirect                 = FALSE;
	
	composer->charset                  = NULL;
	
	composer->enable_autosave          = TRUE;
	composer->autosave_file            = NULL;
	composer->autosave_fd              = -1;
}


GtkType
e_msg_composer_get_type (void)
{
	static GtkType type = 0;
	
	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposer",
			sizeof (EMsgComposer),
			sizeof (EMsgComposerClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (bonobo_window_get_type (), &info);
	}
	
	return type;
}

static void
load_from_config_db (EMsgComposer *composer)
{
	Bonobo_ConfigDatabase db = composer->config_db;
	
	composer->view_from = bonobo_config_get_long_with_default (db, 
		"Mail/Composer/ViewFrom", 1, NULL);
	composer->view_replyto = bonobo_config_get_long_with_default (db, 
                "Mail/Composer/ViewReplyTo", 0, NULL);
	composer->view_bcc = bonobo_config_get_long_with_default (db, 
                "Mail/Composer/ViewBCC", 0, NULL);
	composer->view_cc = bonobo_config_get_long_with_default (db, 
                "Mail/Composer/ViewCC", 1, NULL);
	composer->view_subject = bonobo_config_get_long_with_default (db, 
                "Mail/Composer/ViewSubject", 1, NULL);
}

static void
e_msg_composer_load_config (EMsgComposer *composer)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	db = bonobo_get_object ( "wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (ev._major == CORBA_NO_EXCEPTION && db != CORBA_OBJECT_NIL){
		composer->config_db = db;
		load_from_config_db (composer);
	} else
		composer->config_db = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);
}

static gint
e_msg_composer_get_visible_flags (EMsgComposer *composer)
{
	int flags = 0;
	
	if (composer->view_from)
		flags |= E_MSG_COMPOSER_VISIBLE_FROM;
	if (composer->view_replyto)
		flags |= E_MSG_COMPOSER_VISIBLE_REPLYTO;
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
	GtkWidget *to;
        BonoboControlFrame *cf;
        Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;
	char *text;
	CORBA_Environment ev;

	/* If the 'To:' field is empty, focus it (This is ridiculously complicated) */

	to = e_msg_composer_hdrs_get_to_entry (E_MSG_COMPOSER_HDRS (composer->hdrs));
        cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (to));
        pb = bonobo_control_frame_get_control_property_bag (cf, NULL);
	text = bonobo_property_bag_client_get_value_string (pb, "text", NULL);
	bonobo_object_release_unref (pb, NULL);

	if (!text || text[0] == '\0') {
		bonobo_control_frame_focus_child (cf, GTK_DIR_TAB_FORWARD);
		g_free (text);
		return;
	}
	g_free (text);

	/* If not, check the subject field */

	text = e_msg_composer_hdrs_get_subject (E_MSG_COMPOSER_HDRS (composer->hdrs));

	if (!text || text[0] == '\0') {
		GtkWidget *widget;
		
		widget = e_msg_composer_hdrs_get_subject_entry (E_MSG_COMPOSER_HDRS (composer->hdrs));
		gtk_widget_grab_focus (GTK_WIDGET (E_ENTRY (widget)->canvas));
		g_free (text);
		return;
	}
	g_free (text);

	/* Jump to the editor as a last resort. */

	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "grab-focus", &ev);
	CORBA_exception_free (&ev);
}

static EMsgComposer *
create_composer (void)
{
	EMsgComposer *composer;
	GtkWidget *vbox;
	BonoboObject *editor_server;
	gint vis;
	
	g_return_val_if_fail (gtk_main_level () > 0, NULL);

	composer = gtk_type_new (E_TYPE_MSG_COMPOSER);
	
	gtk_window_set_default_size (GTK_WINDOW (composer),
				     DEFAULT_WIDTH, DEFAULT_HEIGHT);
	gnome_window_icon_set_from_file (GTK_WINDOW (composer), EVOLUTION_DATADIR
					 "/images/evolution/compose-message.png");
	bonobo_window_construct (BONOBO_WINDOW (composer), "e-msg-composer",
				 _("Compose a message"));
	
	/* DND support */
	gtk_drag_dest_set (GTK_WIDGET (composer), GTK_DEST_DEFAULT_ALL,
			   drop_types, num_drop_types, GDK_ACTION_COPY);
	gtk_signal_connect (GTK_OBJECT (composer), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received), NULL);
	e_msg_composer_load_config (composer);
	
	setup_ui (composer);
	
	vbox = gtk_vbox_new (FALSE, 0);
	
	vis = e_msg_composer_get_visible_flags (composer);
	composer->hdrs = e_msg_composer_hdrs_new (vis);
	if (!composer->hdrs) {
		e_activation_failure_dialog (GTK_WINDOW (composer),
					     _("Could not create composer window:\n"
					       "Unable to activate address selector control."),
					     SELECT_NAMES_OAFIID,
					     "IDL:Bonobo/Control:1.0");
		gtk_object_destroy (GTK_OBJECT (composer));
		return NULL;
	}
	
	gtk_box_pack_start (GTK_BOX (vbox), composer->hdrs, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (composer->hdrs), "subject_changed",
			    GTK_SIGNAL_FUNC (subject_changed_cb), composer);
	gtk_signal_connect (GTK_OBJECT (composer->hdrs), "hdrs_changed",
			    GTK_SIGNAL_FUNC (hdrs_changed_cb), composer);
	gtk_signal_connect (GTK_OBJECT (composer->hdrs), "from_changed",
			    GTK_SIGNAL_FUNC (from_changed_cb), composer);
	gtk_widget_show (composer->hdrs);
	
	/* Editor component.  */
	composer->editor = bonobo_widget_new_control (
		GNOME_GTKHTML_EDITOR_CONTROL_ID,
		bonobo_ui_component_get_container (composer->uic));
	if (!composer->editor) {
		e_activation_failure_dialog (GTK_WINDOW (composer),
					     _("Could not create composer window:\n"
					       "Unable to activate HTML editor component."),
					     GNOME_GTKHTML_EDITOR_CONTROL_ID,
					     "IDL:Bonobo/Control:1.0");
		gtk_object_destroy (GTK_OBJECT (composer));
		return NULL;
	}
	
	/* let the editor know which mode we are in */
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor), 
				    "FormatHTML", composer->send_html,
				    NULL);
	
	editor_server = BONOBO_OBJECT (bonobo_widget_get_server (BONOBO_WIDGET (composer->editor)));
	
	composer->persist_file_interface
		= bonobo_object_query_interface (editor_server, "IDL:Bonobo/PersistFile:1.0");
	composer->persist_stream_interface
		= bonobo_object_query_interface (editor_server, "IDL:Bonobo/PersistStream:1.0");
	
	gtk_box_pack_start (GTK_BOX (vbox), composer->editor, TRUE, TRUE, 0);
	
	/* Attachment editor, wrapped into an EScrollFrame.  We don't
           show it for now.  */
	
	composer->attachment_scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (composer->attachment_scroll_frame),
					GTK_SHADOW_IN);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (composer->attachment_scroll_frame),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	composer->attachment_bar = e_msg_composer_attachment_bar_new (NULL);
	GTK_WIDGET_SET_FLAGS (composer->attachment_bar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (composer->attachment_scroll_frame),
			   composer->attachment_bar);
	gtk_box_pack_start (GTK_BOX (vbox),
			    composer->attachment_scroll_frame,
			    FALSE, FALSE, GNOME_PAD_SMALL);
	
	gtk_signal_connect (GTK_OBJECT (composer->attachment_bar), "changed",
			    GTK_SIGNAL_FUNC (attachment_bar_changed_cb), composer);
	
	bonobo_window_set_contents (BONOBO_WINDOW (composer), vbox);
	gtk_widget_show (vbox);
	
	/* If we show this widget earlier, we lose network transparency. i.e. the
	   component appears on the machine evo is running on, ignoring any DISPLAY
	   variable. */
	gtk_widget_show (composer->editor);
	
	e_msg_composer_show_attachments (composer, FALSE);

	prepare_engine (composer);
	if (composer->editor_engine == CORBA_OBJECT_NIL) {
		e_activation_failure_dialog (GTK_WINDOW (composer),
					     _("Could not create composer window:\n"
					       "Unable to activate HTML editor component."),
					     GNOME_GTKHTML_EDITOR_CONTROL_ID,
					     "IDL:GNOME/GtkHTML/Editor/Engine:1.0");
		gtk_object_destroy (GTK_OBJECT (composer));
		return NULL;
	}
		
	gtk_signal_connect (GTK_OBJECT (composer), "map", map_default_cb, NULL);

	if (am == NULL) {
		am = autosave_manager_new ();
	}
	autosave_manager_register (am, composer);

	return composer;
}

static void
set_editor_signature (EMsgComposer *composer)
{
	printf ("set_editor_signature\n");
	if (E_MSG_COMPOSER_HDRS (composer->hdrs)->account->id) {
		MailConfigIdentity *id;
		gchar *verb, *name;
		
		id = E_MSG_COMPOSER_HDRS (composer->hdrs)->account->id;

		composer->random_signature = composer->send_html ? id->html_random : id->text_random;
		if (composer->random_signature)
			composer->signature = NULL;
		else
			composer->signature = composer->send_html ? id->html_signature : id->text_signature;

		if (composer->random_signature) {
			verb = g_strdup ("/commands/SignatureRandom");
			name = g_strdup ("SignatureRandom");
		} else if (composer->signature == NULL) {
			verb = g_strdup ("/commands/SignatureNone");
			name = g_strdup ("SignatureNone");
		} else {
			verb = g_strdup_printf ("/commands/Signature%d", composer->signature->id);
			name = g_strdup_printf ("Signature%d", composer->signature->id);
		}
		bonobo_ui_component_set_prop (composer->uic, verb, "state", "1", NULL);
		g_free (verb);
	}
	printf ("set_editor_signature end\n");
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
	EMsgComposer *new;
	
	new = create_composer ();
	if (new) {
		e_msg_composer_set_send_html (new, mail_config_get_send_html ());
		set_editor_text (new, "");
		set_editor_signature (new);
	}
	
	return new;
}

static gboolean
is_special_header (const char *hdr_name)
{
	/* Note: a header is a "special header" if it has any meaning:
	   1. it's not a X-* header or
	   2. it's an X-Evolution* header
	*/
	if (g_strncasecmp (hdr_name, "X-", 2))
		return TRUE;
	
	if (!g_strncasecmp (hdr_name, "X-Evolution", 11))
		return TRUE;
	
	/* we can keep all other X-* headers */
	
	return FALSE;
}

static void
e_msg_composer_set_pending_body (EMsgComposer *composer, char *text)
{
	char *old;
	
	old = gtk_object_get_data (GTK_OBJECT (composer), "body:text");
        g_free (old);
	gtk_object_set_data (GTK_OBJECT (composer), "body:text", text);
}

static void
e_msg_composer_flush_pending_body (EMsgComposer *composer, gboolean apply)
{
        char *body;
	
	body = gtk_object_get_data (GTK_OBJECT (composer), "body:text");
	if (body) {
		if (apply) 
			set_editor_text (composer, body);
		
		gtk_object_set_data (GTK_OBJECT (composer), "body:text", NULL);
		g_free (body);
	}
}	

static void
add_attachments_from_multipart (EMsgComposer *composer, CamelMultipart *multipart,
				gboolean just_inlines, int depth)
{
	/* find appropriate message attachments to add to the composer */
	int i, nparts;
	
	nparts = camel_multipart_get_number (multipart);
	
	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelMimePart *mime_part;
		
		mime_part = camel_multipart_get_part (multipart, i);
		content_type = camel_mime_part_get_content_type (mime_part);
		
		if (header_content_type_is (content_type, "multipart", "*")) {
			/* another layer of multipartness... */
			CamelDataWrapper *wrapper;
			CamelMultipart *mpart;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mpart = CAMEL_MULTIPART (wrapper);
			
			add_attachments_from_multipart (composer, mpart, just_inlines, depth + 1);
		} else if (header_content_type_is (content_type, "text", "*")) {
			/* do nothing */
		} else if (header_content_type_is (content_type, "message", "*")) {
			/* do nothing */
		} else if (just_inlines) {
			if (camel_mime_part_get_content_id (mime_part) ||
			    camel_mime_part_get_content_location (mime_part))
				e_msg_composer_add_inline_image_from_mime_part (composer, mime_part);
		} else {
			e_msg_composer_attach (composer, mime_part);
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
	CamelContentType *content_type;
	
	content_type = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));
	if (header_content_type_is (content_type, "multipart", "*")) {
		/* there must be attachments... */
		CamelDataWrapper *wrapper;
		CamelMultipart *multipart;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (CAMEL_MIME_PART (message)));
		multipart = CAMEL_MULTIPART (wrapper);
		
		add_attachments_from_multipart (composer, multipart, just_inlines, 0);
	} else {
		/* do nothing... */
	}
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
		CamelMimePart *mime_part;
		
		mime_part = camel_multipart_get_part (multipart, i);
		content_type = camel_mime_part_get_content_type (mime_part);
		
		if (header_content_type_is (content_type, "multipart", "*")) {
			/* another layer of multipartness... */
			CamelDataWrapper *wrapper;
			CamelMultipart *mpart;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mpart = CAMEL_MULTIPART (wrapper);
			
			/* depth doesn't matter so long as we don't pass 0 */
			handle_multipart (composer, mpart, depth + 1);
		} else if (header_content_type_is (content_type, "text", "html")) {
			/* text/html is preferable, so once we find it we're done... */
			text_part = mime_part;
			break;
		} else if (header_content_type_is (content_type, "text", "*")) {
			/* anyt text part not text/html is second rate so the first
			   text part we find isn't necessarily the one we'll use. */
			if (!text_part)
				text_part = mime_part;
		} else {
			e_msg_composer_attach (composer, mime_part);
		}
	}
	
	if (text_part) {
		CamelDataWrapper *contents;
		char *text;
		
		contents = camel_medium_get_content_object (CAMEL_MEDIUM (text_part));
		text = mail_get_message_body (contents, FALSE, FALSE);
		
		if (text)
			e_msg_composer_set_pending_body (composer, text);
	}
}

static void
handle_multipart (EMsgComposer *composer, CamelMultipart *multipart, int depth)
{
	int i, nparts;
	
	nparts = camel_multipart_get_number (multipart);
	
	for (i = 0; i < nparts; i++) {
		CamelContentType *content_type;
		CamelMimePart *mime_part;
		
		mime_part = camel_multipart_get_part (multipart, i);
		content_type = camel_mime_part_get_content_type (mime_part);
		
		if (header_content_type_is (content_type, "multipart", "alternative")) {
			/* this structure contains the body */
			CamelDataWrapper *wrapper;
			CamelMultipart *mpart;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mpart = CAMEL_MULTIPART (wrapper);
			
			handle_multipart_alternative (composer, mpart, depth + 1);
		} else if (header_content_type_is (content_type, "multipart", "*")) {
			/* another layer of multipartness... */
			CamelDataWrapper *wrapper;
			CamelMultipart *mpart;
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mpart = CAMEL_MULTIPART (wrapper);
			
			handle_multipart (composer, mpart, depth + 1);
		} else if (depth == 0 && i == 0) {
			/* Since the first part is not multipart/alternative, then this must be the body */
			CamelDataWrapper *contents;
			char *text;
			
			contents = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			text = mail_get_message_body (contents, FALSE, FALSE);
			
			if (text)
				e_msg_composer_set_pending_body (composer, text);
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
	EDestination **Tov, **Ccv, **Bccv;
	const char *format, *subject, *account_name;
	CamelContentType *content_type;
	struct _header_raw *headers;
	EMsgComposer *new;
	XEvolution *xev;
	guint len, i;
	
	g_return_val_if_fail (gtk_main_level () > 0, NULL);
	
	new = create_composer ();
	if (!new)
		return NULL;
	
	subject = camel_mime_message_get_subject (message);
	
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
	Tov = e_destination_list_to_vector (To);
	g_list_free (To);
	
	len = CAMEL_ADDRESS (cc)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *name, *addr;
		
		if (camel_internet_address_get (cc, i, &name, &addr)) {
			EDestination *dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);
			Cc = g_list_append (Cc, dest);
		}
	}
	Ccv = e_destination_list_to_vector (Cc);
	g_list_free (Cc);
	
	len = CAMEL_ADDRESS (bcc)->addresses->len;
	for (i = 0; i < len; i++) {
		const char *name, *addr;
		
		if (camel_internet_address_get (bcc, i, &name, &addr)) {
			EDestination *dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);
			Bcc = g_list_append (Bcc, dest);
		}
	}
	
	Bccv = e_destination_list_to_vector (Bcc);
	g_list_free (Bcc);
	
	/* Restore the Account preference */
	account_name = camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Account");
	if (account_name) {
		while (*account_name && isspace ((unsigned) *account_name))
			account_name++;
	}
	if (account_name == NULL) {
		account_name = camel_medium_get_header (CAMEL_MEDIUM (message), "From");
	}
	
	e_msg_composer_set_headers (new, account_name, Tov, Ccv, Bccv, subject);
	
	e_destination_freev (Tov);
	e_destination_freev (Ccv);
	e_destination_freev (Bccv);
	
	/* Restore the format editing preference */
	format = camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Format");
	if (format) {
		while (*format && isspace ((unsigned) *format))
			format++;
		
		if (!g_strcasecmp (format, "text/html"))
			e_msg_composer_set_send_html (new, TRUE);
		else
			e_msg_composer_set_send_html (new, FALSE);
	}
	
	/* Remove any other X-Evolution-* headers that may have been set */
	xev = mail_tool_remove_xevolution_headers (message);
	mail_tool_destroy_xevolution (xev);
	
	/* set extra headers */
	headers = CAMEL_MIME_PART (message)->headers;
	while (headers) {
		if (!is_special_header (headers->name)) {
			g_ptr_array_add (new->extra_hdr_names, g_strdup (headers->name));
			g_ptr_array_add (new->extra_hdr_values, g_strdup (headers->value));
		}
		
		headers = headers->next;
	}
	
	/* Restore the attachments and body text */
	content_type = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));
	if (header_content_type_is (content_type, "multipart", "alternative")) {
		/* this contains the text/plain and text/html versions of the message body */
		CamelDataWrapper *wrapper;
		CamelMultipart *multipart;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (CAMEL_MIME_PART (message)));
		multipart = CAMEL_MULTIPART (wrapper);
		
		handle_multipart_alternative (new, multipart, 0);
	} else if (header_content_type_is (content_type, "multipart", "*")) {
		/* there must be attachments... */
		CamelDataWrapper *wrapper;
		CamelMultipart *multipart;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (CAMEL_MIME_PART (message)));
		multipart = CAMEL_MULTIPART (wrapper);
		
		handle_multipart (new, multipart, 0);
	} else {
		/* We either have a text/plain or a text/html part */
		CamelDataWrapper *contents;
		char *text;
		
		contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
		text = mail_get_message_body (contents, FALSE, FALSE);
		
		if (text)
			e_msg_composer_set_pending_body (new, text);
	}
	
	/* We wait until now to set the body text because we need to ensure that
	 * the attachment bar has all the attachments, before we request them.
	 */	
	e_msg_composer_flush_pending_body (new, TRUE);

	set_editor_signature (new);
	
	return new;
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
	
	g_return_val_if_fail (message != NULL, NULL);
	
	composer = e_msg_composer_new_with_message (message);
	subject = camel_mime_message_get_subject (message);
	
	composer->redirect = message;
	camel_object_ref (CAMEL_OBJECT (message));
	
	e_msg_composer_set_headers (composer, resent_from, NULL, NULL, NULL, subject);
	
	gtk_widget_set_sensitive (composer->editor, FALSE);
	gtk_widget_set_sensitive (composer->attachment_bar, FALSE);
	
	return composer;
}


static GList *
add_recipients (GList *list, const char *recips, gboolean decode)
{
	CamelInternetAddress *cia;
	const char *name, *addr;
	int num, i;
	
	cia = camel_internet_address_new ();
	if (decode)
		num = camel_address_decode (CAMEL_ADDRESS (cia), recips);
	else
		num = camel_address_unformat (CAMEL_ADDRESS (cia), recips);
	
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

/**
 * e_msg_composer_new_from_url:
 * @url: a mailto URL
 *
 * Create a new message composer widget, and fill in fields as
 * defined by the provided URL.
 **/
EMsgComposer *
e_msg_composer_new_from_url (const char *url_in)
{
	EMsgComposer *composer;
	EMsgComposerHdrs *hdrs;
	GList *to = NULL, *cc = NULL, *bcc = NULL;
	EDestination **tov, **ccv, **bccv;
	char *subject = NULL, *body = NULL;
	const char *p, *header;
	char *content;
	int len, clen;
	
	g_return_val_if_fail (g_strncasecmp (url_in, "mailto:", 7) == 0, NULL);
	
	composer = e_msg_composer_new ();
	if (!composer)
		return NULL;
	
	/* Parse recipients (everything after ':' until '?' or eos). */
	p = url_in + 7;
	len = strcspn (p, "?");
	if (len) {
		content = g_strndup (p, len);
		camel_url_decode (content);
		to = add_recipients (to, content, FALSE);
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
			
			header = p;
			p += len + 1;
			
			clen = strcspn (p, "&");
			
			content = g_strndup (p, clen);
			camel_url_decode (content);
			
			if (!g_strncasecmp (header, "to", len)) {
				to = add_recipients (to, content, FALSE);
			} else if (!g_strncasecmp (header, "cc", len)) {
				cc = add_recipients (cc, content, FALSE);
			} else if (!g_strncasecmp (header, "bcc", len)) {
				bcc = add_recipients (bcc, content, FALSE);
			} else if (!g_strncasecmp (header, "subject", len)) {
				g_free (subject);
				subject = g_strdup (content);
			} else if (!g_strncasecmp (header, "body", len)) {
				g_free (body);
				body = g_strdup (content);
			} else {
				/* add an arbitrary header */
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
	
	tov  = e_destination_list_to_vector (to);
	ccv  = e_destination_list_to_vector (cc);
	bccv = e_destination_list_to_vector (bcc);
	
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
		
		htmlbody = e_text_to_html (body, E_TEXT_TO_HTML_PRE);
		set_editor_text (composer, htmlbody);
		g_free (htmlbody);
	}
	
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
	g_return_if_fail (composer != NULL);
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
	
	e_msg_composer_hdrs_set_from_account (hdrs, from);
	e_msg_composer_hdrs_set_to (hdrs, to);
	e_msg_composer_hdrs_set_cc (hdrs, cc);
	e_msg_composer_hdrs_set_bcc (hdrs, bcc);
	e_msg_composer_hdrs_set_subject (hdrs, subject);
}


/**
 * e_msg_composer_set_body_text:
 * @composer: a composer object
 * @text: the HTML text to initialize the editor with
 *
 * Loads the given HTML text into the editor.
 **/
void
e_msg_composer_set_body_text (EMsgComposer *composer, const char *text)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	set_editor_text (composer, text);
	
	/* set editor text unfortunately kills the signature so we
           have to re-show it */
	e_msg_composer_show_sig_file (composer);
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
	
	g_free (composer->mime_body);
	composer->mime_body = g_strdup (body);
	g_free (composer->mime_type);
	composer->mime_type = g_strdup (mime_type);
	composer->send_html = FALSE;
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
	char *mime_type, *cid, *url;
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
	camel_object_unref (CAMEL_OBJECT (wrapper));

	cid = header_msgid_generate ();
	camel_mime_part_set_content_id (part, cid);
	camel_mime_part_set_filename (part, g_basename (file_name));
	camel_mime_part_set_encoding (part, CAMEL_MIME_PART_ENCODING_BASE64);

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
	char *cid, *url;
	const char *location;

	cid = (char *)camel_mime_part_get_content_id (part);
	if (!cid) {
		camel_mime_part_set_content_id (part, NULL);
		cid = (char *)camel_mime_part_get_content_id (part);
	}

	url = g_strdup_printf ("cid:%s", cid);
	g_hash_table_insert (composer->inline_images, url, part);
	camel_object_ref (CAMEL_OBJECT (part));

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
e_msg_composer_get_message (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);
	
	return build_message (composer);
}

CamelMimeMessage *
e_msg_composer_get_message_draft (EMsgComposer *composer)
{
	CamelMimeMessage *msg;
	const MailConfigAccount *account;
	gboolean old_send_html;
	gboolean old_pgp_sign;
	gboolean old_pgp_encrypt;
	gboolean old_smime_sign;
	gboolean old_smime_encrypt;
	
	/* always save drafts as HTML to preserve formatting */
	old_send_html = composer->send_html;
	composer->send_html = TRUE;
	old_pgp_sign = composer->pgp_sign;
	composer->pgp_sign = FALSE;
	old_pgp_encrypt = composer->pgp_encrypt;
	composer->pgp_encrypt = FALSE;
	old_smime_sign = composer->smime_sign;
	composer->smime_sign = FALSE;
	old_smime_encrypt = composer->smime_encrypt;
	composer->smime_encrypt = FALSE;
	
	msg = e_msg_composer_get_message (composer);
	
	composer->send_html = old_send_html;
	composer->pgp_sign = old_pgp_sign;
	composer->pgp_encrypt = old_pgp_encrypt;
	composer->smime_sign = old_smime_sign;
	composer->smime_encrypt = old_smime_encrypt;
	
	/* Attach account info to the draft. */
	account = e_msg_composer_get_preferred_account (composer);
	if (account && account->name)
		camel_medium_set_header (CAMEL_MEDIUM (msg), "X-Evolution-Account", account->name);
	
	/* build_message() set this to text/html since we set composer->send_html to
	   TRUE before calling e_msg_composer_get_message() */
	if (!composer->send_html)
		camel_medium_set_header (CAMEL_MEDIUM (msg), "X-Evolution-Format", "text/plain");
	
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
	gchar *html;

	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	printf ("e_msg_composer_show_sig_file\n");
	/* printf ("set sig '%s' '%s'\n", sig_file, composer->sig_file); */

	composer->in_signature_insert = TRUE;
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_freeze (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-position-save", &ev);
	GNOME_GtkHTML_Editor_Engine_undoBegin (composer->editor_engine, "Set signature", "Reset signature", &ev);

	delete_old_signature (composer);
	html = get_signature_html (composer);
	if (html) {
		if (!GNOME_GtkHTML_Editor_Engine_isParagraphEmpty (composer->editor_engine, &ev))
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "insert-paragraph", &ev);
		if (!GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-backward", &ev))
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "insert-paragraph", &ev);
		else
			GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-forward", &ev);
		/* printf ("insert %s\n", html); */
		GNOME_GtkHTML_Editor_Engine_setParagraphData (composer->editor_engine, "orig", "0", &ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "indent-zero", &ev);
		GNOME_GtkHTML_Editor_Engine_insertHTML (composer->editor_engine, html, &ev);
		g_free (html);
	}

	GNOME_GtkHTML_Editor_Engine_undoEnd (composer->editor_engine, &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "cursor-position-restore", &ev);
	GNOME_GtkHTML_Editor_Engine_thaw (composer->editor_engine, &ev);
	CORBA_exception_free (&ev);
	composer->in_signature_insert = FALSE;

	printf ("e_msg_composer_show_sig_file end\n");
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

	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (composer->send_html && send_html)
		return;
	if (! composer->send_html && ! send_html)
		return;

	composer->send_html = send_html;

	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (composer->editor_engine, "block-redraw", &ev);
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/FormatHtml",
		"state", composer->send_html ? "1" : "0", NULL);

	/* let the editor know which mode we are in */
	bonobo_widget_set_property (BONOBO_WIDGET (composer->editor), "FormatHTML",
				    composer->send_html, NULL);

	set_config (composer, "FormatHTML", composer->send_html);
	set_editor_signature (composer);
	e_msg_composer_show_sig_file (composer);
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
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	return composer->send_html;
}


/**
 * e_msg_composer_get_preferred_account:
 * @composer: composer
 *
 * Returns the user-specified account (from field).
 */
const MailConfigAccount *
e_msg_composer_get_preferred_account (EMsgComposer *composer)
{
	EMsgComposerHdrs *hdrs;
	
	g_return_val_if_fail (composer != NULL, NULL);
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->pgp_sign && pgp_sign)
		return;
	if (!composer->pgp_sign && !pgp_sign)
		return;
	
	composer->pgp_sign = pgp_sign;
	
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
	g_return_val_if_fail (composer != NULL, FALSE);
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->pgp_encrypt && pgp_encrypt)
		return;
	if (!composer->pgp_encrypt && !pgp_encrypt)
		return;
	
	composer->pgp_encrypt = pgp_encrypt;
	
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
	g_return_val_if_fail (composer != NULL, FALSE);
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->smime_sign && smime_sign)
		return;
	if (!composer->smime_sign && !smime_sign)
		return;
	
	composer->smime_sign = smime_sign;
	
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
	g_return_val_if_fail (composer != NULL, FALSE);
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if (composer->smime_encrypt && smime_encrypt)
		return;
	if (!composer->smime_encrypt && !smime_encrypt)
		return;
	
	composer->smime_encrypt = smime_encrypt;
	
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
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->smime_encrypt;
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if ((composer->view_bcc && view_bcc) ||
	    (!composer->view_bcc && !view_bcc))
		return;

	composer->view_bcc = view_bcc;
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewBCC",
		"state", composer->view_bcc ? "1" : "0", NULL);
	set_config (composer, "ViewBCC", composer->view_bcc);
	e_msg_composer_set_hdrs_visible
		(E_MSG_COMPOSER_HDRS (composer->hdrs),
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
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_bcc;
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if ((composer->view_cc && view_cc) ||
	    (!composer->view_cc && !view_cc))
		return;

	composer->view_cc = view_cc;
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewCC",
		"state", composer->view_cc ? "1" : "0", NULL);
	set_config (composer, "ViewCC", composer->view_cc);
	e_msg_composer_set_hdrs_visible
		(E_MSG_COMPOSER_HDRS (composer->hdrs),
		 e_msg_composer_get_visible_flags (composer));
}

EDestination **
e_msg_composer_get_recipients (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return composer->hdrs ? e_msg_composer_hdrs_get_recipients (E_MSG_COMPOSER_HDRS (composer->hdrs)) : NULL;
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
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_cc;
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if ((composer->view_from && view_from) ||
	    (!composer->view_from && !view_from))
		return;

	composer->view_from = view_from;
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewFrom",
		"state", composer->view_from ? "1" : "0", NULL);
	set_config (composer, "ViewFrom", composer->view_from);
	e_msg_composer_set_hdrs_visible
		(E_MSG_COMPOSER_HDRS (composer->hdrs),
		 e_msg_composer_get_visible_flags (composer));
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
	g_return_val_if_fail (composer != NULL, FALSE);
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
e_msg_composer_set_view_replyto (EMsgComposer *composer, gboolean view_replyto)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	
	if ((composer->view_replyto && view_replyto) ||
	    (!composer->view_replyto && !view_replyto))
		return;
	
	composer->view_replyto = view_replyto;
	bonobo_ui_component_set_prop (
		composer->uic, "/commands/ViewReplyTo",
		"state", composer->view_replyto ? "1" : "0", NULL);
	set_config (composer, "ViewReplyTo", composer->view_replyto);
	e_msg_composer_set_hdrs_visible
		(E_MSG_COMPOSER_HDRS (composer->hdrs),
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
	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	
	return composer->view_replyto;
}

/**
 * e_msg_composer_guess_mime_type:
 * @file_name: filename
 *
 * Returns the guessed mime type of the file given by #file_name.
 **/
gchar *
e_msg_composer_guess_mime_type (const gchar *file_name)
{
	GnomeVFSFileInfo info;
	GnomeVFSResult result;

	result = gnome_vfs_get_file_info (file_name, &info,
					  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result == GNOME_VFS_OK) {
		gchar *type;

		type = g_strdup (gnome_vfs_file_info_get_mime_type (&info));
		gnome_vfs_file_info_unref (&info);
		return type;
	} else
		return NULL;
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
	g_return_if_fail (composer != NULL);
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
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	composer->has_changed = FALSE;
}

gboolean
e_msg_composer_is_dirty (EMsgComposer *composer)
{
	CORBA_Environment ev;
	gboolean rv;
	
	CORBA_exception_init (&ev);
	rv = composer->has_changed || GNOME_GtkHTML_Editor_Engine_hasUndo (composer->editor_engine, &ev);
	CORBA_exception_free (&ev);

	return rv;
}

void
e_msg_composer_set_enable_autosave  (EMsgComposer *composer, gboolean enabled)
{
	g_return_if_fail (composer != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	composer->enable_autosave = enabled;
}

static gchar *
next_word (const gchar *s, const gchar **sr)
{
	if (!s || !*s)
		return NULL;
	else {
		const gchar *begin;
		gunichar uc;
		gboolean cited;

		do {
			begin = s;
			cited = FALSE;
			uc = g_utf8_get_char (s);
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
				if (!s)
					break;
				uc = g_utf8_get_char (s);
			}
			*sr = s;
			return s ? g_strndup (begin, s - begin) : g_strdup (begin);
		} else
			return NULL;
	}
}

void
e_msg_composer_ignore (EMsgComposer *composer, const gchar *str)
{
	CORBA_Environment ev;
	gchar *word;

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
