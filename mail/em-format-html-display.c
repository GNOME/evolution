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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

#include "e-util/e-datetime-format.h"
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#include <shell/e-shell.h>
#include <shell/e-shell-utils.h>

#include <libedataserver/e-flag.h>

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
#include "certificate-viewer.h"
#include "e-cert-db.h"
#endif

#include "e-mail-display.h"
#include "e-mail-attachment-bar.h"
#include "em-format-html-display.h"
#include "em-utils.h"
#include "widgets/misc/e-attachment.h"
#include "widgets/misc/e-attachment-button.h"
#include "widgets/misc/e-attachment-view.h"
#include "shell/e-shell.h"
#include "shell/e-shell-window.h"

#define EM_FORMAT_HTML_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FORMAT_HTML_DISPLAY, EMFormatHTMLDisplayPrivate))

#define d(x)

#define EM_FORMAT_HTML_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FORMAT_HTML_DISPLAY, EMFormatHTMLDisplayPrivate))

struct _EMFormatHTMLDisplayPrivate {

        EAttachmentView *last_view;

};

/* TODO: move the dialogue elsehwere */
/* FIXME: also in em-format-html.c */
static const struct {
	const gchar *icon, *shortdesc, *description;
} smime_sign_table[5] = {
	{ "stock_signature-bad", N_("Unsigned"), N_("This message is not signed. There is no guarantee that this message is authentic.") },
	{ "stock_signature-ok", N_("Valid signature"), N_("This message is signed and is valid meaning that it is very likely that this message is authentic.") },
	{ "stock_signature-bad", N_("Invalid signature"), N_("The signature of this message cannot be verified, it may have been altered in transit.") },
	{ "stock_signature", N_("Valid signature, but cannot verify sender"), N_("This message is signed with a valid signature, but the sender of the message cannot be verified.") },
	{ "stock_signature-bad", N_("Signature exists, but need public key"), N_("This message is signed with a signature, but there is no corresponding public key.") },

};

static const struct {
	const gchar *icon, *shortdesc, *description;
} smime_encrypt_table[4] = {
	{ "stock_lock-broken", N_("Unencrypted"), N_("This message is not encrypted. Its content may be viewed in transit across the Internet.") },
	{ "stock_lock-ok", N_("Encrypted, weak"), N_("This message is encrypted, but with a weak encryption algorithm. It would be difficult, but not impossible for an outsider to view the content of this message in a practical amount of time.") },
	{ "stock_lock-ok", N_("Encrypted"), N_("This message is encrypted.  It would be difficult for an outsider to view the content of this message.") },
	{ "stock_lock-ok", N_("Encrypted, strong"), N_("This message is encrypted, with a strong encryption algorithm. It would be very difficult for an outsider to view the content of this message in a practical amount of time.") },
};

static const GdkRGBA smime_sign_colour[5] = {
	{ 0 }, { 0.53, 0.73, 0.53, 1 }, { 0.73, 0.53, 0.53, 1 }, { 0.91, 0.82, 0.13, 1 }, { 0 },
};

static void efhd_message_prefix	(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efhd_message_add_bar	(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efhd_parse_attachment	(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void efhd_parse_secure		(EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);

static GtkWidget * efhd_attachment_bar		(EMFormat *emf, EMFormatPURI *puri, GCancellable *cancellable);
static GtkWidget * efhd_attachment_button	(EMFormat *emf, EMFormatPURI *puri, GCancellable *cancellable);

static void efhd_write_attachment_bar   (EMFormat *emf, EMFormatPURI *emp, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efhd_write_attachment       (EMFormat *emf, EMFormatPURI *emp, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efhd_write_secure_button    (EMFormat *emf, EMFormatPURI *emp, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);

static void efhd_free_attach_puri_data (EMFormatPURI *puri);

static void efhd_builtin_init (EMFormatHTMLDisplayClass *efhc);

static gpointer parent_class;

static EAttachmentStore *
find_parent_attachment_store (EMFormatHTMLDisplay *efhd,
                              const gchar *part_id)
{
	EMFormat *emf = (EMFormat *) efhd;
	EMFormatAttachmentBarPURI *abp;
	gchar *tmp, *pos;
	GList *item;

	tmp = g_strdup (part_id);

	do {
		gchar *id;

		pos = g_strrstr (tmp, ".");
		if (!pos)
			break;

		g_free (tmp);
		tmp = g_strndup (part_id, pos - tmp);
		id = g_strdup_printf ("%s.attachment-bar", tmp);

		item = g_hash_table_lookup (emf->mail_part_table, id);

		g_free (id);

	} while (pos && !item);

	g_free (tmp);

	abp = (EMFormatAttachmentBarPURI *) item->data;

	if (abp)
		return abp->store;
	else
		return NULL;
}

static void
efhd_attachment_bar_puri_free (EMFormatPURI *puri)
{
	EMFormatAttachmentBarPURI *abp;

	abp = (EMFormatAttachmentBarPURI *) puri;

	if (abp->store) {
		g_object_unref (abp->store);
		abp->store = NULL;
	}
}

static void
efhd_xpkcs7mime_free (EMFormatPURI *puri)
{
	EMFormatSMIMEPURI *sp = (EMFormatSMIMEPURI *) puri;

	if (sp->widget)
		gtk_widget_destroy (sp->widget);

	if (sp->description)
		g_free (sp->description);

	if (sp->valid)
		camel_cipher_validity_free (sp->valid);
}

static void
efhd_xpkcs7mime_info_response (GtkWidget *widget,
                               guint button,
                               EMFormatSMIMEPURI *po)
{
	gtk_widget_destroy (widget);
	po->widget = NULL;
}

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
static void
efhd_xpkcs7mime_viewcert_clicked (GtkWidget *button,
                                  EMFormatSMIMEPURI *po)
{
	CamelCipherCertInfo *info = g_object_get_data((GObject *)button, "e-cert-info");
	ECert *ec = NULL;

	if (info->cert_data)
		ec = e_cert_new (CERT_DupCertificate (info->cert_data));

	if (ec != NULL) {
		GtkWidget *w = certificate_viewer_show (ec);

		/* oddly enough certificate_viewer_show doesn't ... */
		gtk_widget_show (w);
		g_signal_connect (
			w, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

		if (w && po->widget)
			gtk_window_set_transient_for ((GtkWindow *) w, (GtkWindow *) po->widget);

		g_object_unref (ec);
	} else {
		g_warning("can't find certificate for %s <%s>", info->name?info->name:"", info->email?info->email:"");
	}
}
#endif

static void
efhd_xpkcs7mime_add_cert_table (GtkWidget *grid,
                                GQueue *certlist,
                                EMFormatSMIMEPURI *po)
{
	GList *head, *link;
	GtkTable *table;
	gint n = 0;

	table = (GtkTable *) gtk_table_new (certlist->length, 2, FALSE);

	head = g_queue_peek_head_link (certlist);

	for (link = head; link != NULL; link = g_list_next (link)) {
		CamelCipherCertInfo *info = link->data;
		gchar *la = NULL;
		const gchar *l = NULL;

		if (info->name) {
			if (info->email && strcmp (info->name, info->email) != 0)
				l = la = g_strdup_printf("%s <%s>", info->name, info->email);
			else
				l = info->name;
		} else {
			if (info->email)
				l = info->email;
		}

		if (l) {
			GtkWidget *w;
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
			ECert *ec = NULL;
#endif
			w = gtk_label_new (l);
			gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
			g_free (la);
			gtk_table_attach (table, w, 0, 1, n, n + 1, GTK_FILL, GTK_FILL, 3, 3);
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
			w = gtk_button_new_with_mnemonic(_("_View Certificate"));
			gtk_table_attach (table, w, 1, 2, n, n + 1, 0, 0, 3, 3);
			g_object_set_data((GObject *)w, "e-cert-info", info);
			g_signal_connect (
				w, "clicked",
				G_CALLBACK (efhd_xpkcs7mime_viewcert_clicked), po);

			if (info->cert_data)
				ec = e_cert_new (CERT_DupCertificate (info->cert_data));

			if (ec == NULL)
				gtk_widget_set_sensitive (w, FALSE);
			else
				g_object_unref (ec);
#else
			w = gtk_label_new (_("This certificate is not viewable"));
			gtk_table_attach (table, w, 1, 2, n, n + 1, 0, 0, 3, 3);
#endif
			n++;
		}
	}

	gtk_container_add (GTK_CONTAINER (grid), GTK_WIDGET (table));
}

static void
efhd_xpkcs7mime_validity_clicked (GtkWidget *button,
                                  EMFormatPURI *puri)
{
	EMFormatSMIMEPURI *po = (EMFormatSMIMEPURI *) puri;
	GtkBuilder *builder;
	GtkWidget *grid, *w;

	if (po->widget)
		/* FIXME: window raise? */
		return;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	po->widget = e_builder_get_widget(builder, "message_security_dialog");

	grid = e_builder_get_widget(builder, "signature_grid");
	w = gtk_label_new (_(smime_sign_table[po->valid->sign.status].description));
	gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_container_add (GTK_CONTAINER (grid), w);
	if (po->valid->sign.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (buffer, po->valid->sign.description, strlen (po->valid->sign.description));
		w = g_object_new (gtk_scrolled_window_get_type (),
				 "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "shadow_type", GTK_SHADOW_IN,
				 "expand", TRUE,
				 "child", g_object_new(gtk_text_view_get_type(),
						       "buffer", buffer,
						       "cursor_visible", FALSE,
						       "editable", FALSE,
						       "width_request", 500,
						       "height_request", 160,
						       NULL),
				 NULL);
		g_object_unref (buffer);

		gtk_container_add (GTK_CONTAINER (grid), w);
	}

	if (!g_queue_is_empty (&po->valid->sign.signers))
		efhd_xpkcs7mime_add_cert_table (grid, &po->valid->sign.signers, po);

	gtk_widget_show_all (grid);

	grid = e_builder_get_widget(builder, "encryption_grid");
	w = gtk_label_new (_(smime_encrypt_table[po->valid->encrypt.status].description));
	gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_container_add (GTK_CONTAINER (grid), w);
	if (po->valid->encrypt.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (buffer, po->valid->encrypt.description, strlen (po->valid->encrypt.description));
		w = g_object_new (gtk_scrolled_window_get_type (),
				 "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "shadow_type", GTK_SHADOW_IN,
				 "expand", TRUE,
				 "child", g_object_new(gtk_text_view_get_type(),
						       "buffer", buffer,
						       "cursor_visible", FALSE,
						       "editable", FALSE,
						       "width_request", 500,
						       "height_request", 160,
						       NULL),
				 NULL);
		g_object_unref (buffer);

		gtk_container_add (GTK_CONTAINER (grid), w);
	}

	if (!g_queue_is_empty (&po->valid->encrypt.encrypters))
		efhd_xpkcs7mime_add_cert_table (grid, &po->valid->encrypt.encrypters, po);

	gtk_widget_show_all (grid);

	g_object_unref (builder);

	g_signal_connect (
		po->widget, "response",
		G_CALLBACK (efhd_xpkcs7mime_info_response), po);

	gtk_widget_show (po->widget);
}

static GtkWidget *
efhd_xpkcs7mime_button (EMFormat *emf,
                        EMFormatPURI *puri,
                        GCancellable *cancellable)
{
	GtkWidget *box, *button, *layout, *widget;
	EMFormatSMIMEPURI *po = (EMFormatSMIMEPURI *) puri;
	const gchar *icon_name;

	/* FIXME: need to have it based on encryption and signing too */
	if (po->valid->sign.status != 0)
		icon_name = smime_sign_table[po->valid->sign.status].icon;
	else
		icon_name = smime_encrypt_table[po->valid->encrypt.status].icon;

	box = gtk_event_box_new ();
	if (po->valid->sign.status != 0)
		gtk_widget_override_background_color (box, GTK_STATE_FLAG_NORMAL,
			&smime_sign_colour[po->valid->sign.status]);

	layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add (GTK_CONTAINER (box), layout);

	button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (layout), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
		G_CALLBACK (efhd_xpkcs7mime_validity_clicked), puri);

	widget = gtk_image_new_from_icon_name (
			icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (button), widget);

	widget = gtk_label_new (po->description);
	gtk_box_pack_start (GTK_BOX (layout), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	return box;
}

struct attachment_load_data {
	EAttachment *attachment;
	EFlag *flag;
};

static void
attachment_loaded (EAttachment *attachment,
                   GAsyncResult *res,
                   gpointer user_data)
{
	struct attachment_load_data *data = user_data;
	EShell *shell;
	GtkWindow *window;

	shell = e_shell_get_default ();
	window = e_shell_get_active_window (shell);

	e_attachment_load_handle_error (data->attachment, res, window);

	e_flag_set (data->flag);
}

/* Idle callback */
static gboolean
load_attachment_idle (struct attachment_load_data *data)
{
	e_attachment_load_async (data->attachment,
		(GAsyncReadyCallback) attachment_loaded, data);

	return FALSE;
}

static void
efhd_parse_attachment (EMFormat *emf,
                       CamelMimePart *part,
                       GString *part_id,
                       EMFormatParserInfo *info,
                       GCancellable *cancellable)
{
	gchar *text, *html;
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) emf;
	EMFormatAttachmentPURI *puri;
	EAttachmentStore *store;
	const EMFormatHandler *handler;
	CamelContentType *ct;
	gchar *mime_type;
	gint len;
	const gchar *cid;
	guint32 size;
	struct attachment_load_data *load_data;
	gboolean can_show = FALSE;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	len = part_id->len;
	g_string_append (part_id, ".attachment");

        /* Try to find handler for the mime part */
	ct = camel_mime_part_get_content_type (part);
	if (ct) {
		mime_type = camel_content_type_simple (ct);
		handler = em_format_find_handler (emf, mime_type);
	}

	/* FIXME: should we look up mime_type from object again? */
	text = em_format_describe_part (part, mime_type);
	html = camel_text_to_html (
		text, EM_FORMAT_HTML (emf)->text_html_flags &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	g_free (text);
	g_free (mime_type);

	puri = (EMFormatAttachmentPURI *) em_format_puri_new (
			emf, sizeof (EMFormatAttachmentPURI), part, part_id->str);
	puri->puri.free = efhd_free_attach_puri_data;
	puri->puri.write_func = efhd_write_attachment;
	puri->puri.widget_func = efhd_attachment_button;
	puri->shown = (handler && em_format_is_inline (emf, part_id->str, part, handler));
	puri->snoop_mime_type = em_format_snoop_type (part);
	puri->attachment = e_attachment_new ();
	puri->attachment_view_part_id = NULL;
	puri->description = html;
	puri->handle = handler;
	if (info->validity)
		puri->puri.validity = camel_cipher_validity_clone (info->validity);

	cid = camel_mime_part_get_content_id (part);
	if (cid)
		puri->puri.cid = g_strdup_printf ("cid:%s", cid);

	if (handler) {
		CamelContentType *ct;

                /* This mime_type is important for WebKit to determine content type.
                 * We have converted text/ * to text/html, other (binary) formats remained
                 * untouched. */
		ct = camel_content_type_decode (handler->mime_type);
		if (g_strcmp0 (ct->type, "text") == 0)
			puri->puri.mime_type = g_strdup ("text/html");
		else
			puri->puri.mime_type = camel_content_type_simple (ct);
		camel_content_type_unref (ct);
	}

	em_format_add_puri (emf, (EMFormatPURI *) puri);

        /* Though it is an attachment, we still might be able to parse it and
         * so discover some parts that we might be even able to display. */
	if (handler && handler->parse_func && (handler->parse_func != efhd_parse_attachment) &&
	    ((handler->flags & EM_FORMAT_HANDLER_COMPOUND_TYPE) ||
	     (handler->flags & EM_FORMAT_HANDLER_INLINE_DISPOSITION))) {
		GList *i;
		EMFormatParserInfo attachment_info = { .handler = handler,
						       .is_attachment = TRUE };
		handler->parse_func (emf, puri->puri.part, part_id, &attachment_info, cancellable);

		i = g_hash_table_lookup (emf->mail_part_table, part_id->str);
		if (i->next && i->next->data) {
			EMFormatPURI *p = i->next->data;
			puri->attachment_view_part_id = g_strdup (p->uri);
			can_show = TRUE;
		}
	}

	e_attachment_set_mime_part (puri->attachment, part);
	e_attachment_set_shown (puri->attachment, puri->shown);
	if (puri->puri.validity) {
		e_attachment_set_signed (puri->attachment, puri->puri.validity->sign.status);
		e_attachment_set_encrypted (puri->attachment, puri->puri.validity->encrypt.status);
	}
	e_attachment_set_can_show (puri->attachment,
		can_show || (puri->handle && puri->handle->write_func));

	store = find_parent_attachment_store (efhd, part_id->str);
	e_attachment_store_add_attachment (store, puri->attachment);

	if (emf->folder && emf->folder->summary && emf->message_uid) {
		CamelDataWrapper *dw = camel_medium_get_content (CAMEL_MEDIUM (puri->puri.part));
		GByteArray *ba;
		ba = camel_data_wrapper_get_byte_array (dw);
		if (ba) {
			size = ba->len;

			if (camel_mime_part_get_encoding (puri->puri.part) == CAMEL_TRANSFER_ENCODING_BASE64)
				size = size / 1.37;
		}
	}

	load_data = g_new0 (struct attachment_load_data, 1);
	load_data->attachment = g_object_ref (puri->attachment);
	load_data->flag = e_flag_new ();

	e_flag_clear (load_data->flag);

	/* e_attachment_load_async must be called from main thread */
	g_idle_add ((GSourceFunc) load_attachment_idle, load_data);

	e_flag_wait (load_data->flag);

	e_flag_free (load_data->flag);
	g_object_unref (load_data->attachment);
	g_free (load_data);

	if (size != 0) {
		GFileInfo *fileinfo;

		fileinfo = e_attachment_get_file_info (puri->attachment);
		g_file_info_set_size (fileinfo, size);
		e_attachment_set_file_info (puri->attachment, fileinfo);
	}

	g_string_truncate (part_id, len);
}

static void
efhd_parse_secure (EMFormat *emf,
                   CamelMimePart *part,
                   GString *part_id,
                   EMFormatParserInfo *info,
                   GCancellable *cancellable)
{
	if (info->validity
	    && (info->validity->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE
		|| info->validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)) {
		GString *buffer;
		EMFormatSMIMEPURI *pobj;

		pobj = (EMFormatSMIMEPURI *) em_format_puri_new (
				emf, sizeof (EMFormatSMIMEPURI), part, part_id->str);
		pobj->puri.free = efhd_xpkcs7mime_free;
		pobj->valid = camel_cipher_validity_clone (info->validity);
		pobj->puri.widget_func = efhd_xpkcs7mime_button;
		pobj->puri.write_func = efhd_write_secure_button;

		em_format_add_puri (emf, (EMFormatPURI *) pobj);

		buffer = g_string_new ("");

		if (info->validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
			const gchar *desc;
			gint status;

			status = info->validity->sign.status;
			desc = smime_sign_table[status].shortdesc;

			g_string_append (buffer, gettext (desc));

			em_format_html_format_cert_infos (
				&info->validity->sign.signers, buffer);
		}

		if (info->validity->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
			const gchar *desc;
			gint status;

			if (info->validity->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)
				g_string_append (buffer, "\n");

			status = info->validity->encrypt.status;
			desc = smime_encrypt_table[status].shortdesc;
			g_string_append (buffer, gettext (desc));
		}

		pobj->description = g_string_free (buffer, FALSE);
	}
}

/******************************************************************************/
static void
efhd_write_attachment_bar (EMFormat *emf,
                           EMFormatPURI *puri,
                           CamelStream *stream,
                           EMFormatWriterInfo *info,
                           GCancellable *cancellable)
{
	EMFormatAttachmentBarPURI *efab = (EMFormatAttachmentBarPURI *) puri;
	gchar *str;

	if (info->mode == EM_FORMAT_WRITE_MODE_PRINTING)
		return;

	if (e_attachment_store_get_num_attachments (efab->store) == 0)
		return;

	str = g_strdup_printf (
		"<object type=\"application/x-attachment-bar\" "
			"height=\"20\" width=\"100%%\" "
			"id=\"%s\"data=\"%s\"></object>", puri->uri, puri->uri);

	camel_stream_write_string (stream, str, cancellable, NULL);

	g_free (str);
}

static void
efhd_write_attachment (EMFormat *emf,
                       EMFormatPURI *puri,
                       CamelStream *stream,
                       EMFormatWriterInfo *info,
                       GCancellable *cancellable)
{
	gchar *str, *desc;
	const gchar *mime_type;
	gchar *button_id;

	EMFormatAttachmentPURI *efa = (EMFormatAttachmentPURI *) puri;

        /* If the attachment is requested as RAW, then call the handler directly
         * and do not append any other code. */
	if ((info->mode == EM_FORMAT_WRITE_MODE_RAW) &&
	    efa->handle && efa->handle->write_func) {

		efa->handle->write_func (emf, puri, stream, info, cancellable);
		return;
	}

	if (info->mode == EM_FORMAT_WRITE_MODE_PRINTING) {

		if (efa->handle && efa->handle->write_func)
			efa->handle->write_func (emf, puri, stream, info, cancellable);

		return;
	}

	if (efa->handle)
		mime_type = efa->handle->mime_type;
	else
		mime_type = efa->snoop_mime_type;

	button_id = g_strconcat (puri->uri, ".attachment_button", NULL);

	desc = em_format_describe_part (puri->part, mime_type);
	str = g_strdup_printf (
		"<div class=\"attachment\">"
		"<table width=\"100%%\" border=\"0\">"
		"<tr valign=\"middle\">"
		"<td align=\"left\" width=\"100\">"
		"<object type=\"application/x-attachment-button\" "
		"height=\"20\" width=\"100\" data=\"%s\" id=\"%s\"></object>"
		"</td>"
		"<td align=\"left\">%s</td>"
		"</tr>", puri->uri, button_id, desc);

	camel_stream_write_string (stream, str, cancellable, NULL);
	g_free (desc);
	g_free (button_id);
	g_free (str);

        /* If we know how to write the attachment, then do it */
	if ((efa->handle && efa->handle->write_func) ||
	    (efa->attachment_view_part_id)) {

		str = g_strdup_printf (
			"<tr><td colspan=\"2\">"
			"<div class=\"attachment-wrapper\" id=\"%s\">",
			puri->uri);

		camel_stream_write_string (stream, str, cancellable, NULL);
		g_free (str);

		if (efa->handle->write_func) {
			efa->handle->write_func (
				emf, puri, stream, info, cancellable);
		} else if (efa->attachment_view_part_id) {
			EMFormatPURI *p;

			p = em_format_find_puri (
				emf, efa->attachment_view_part_id);
			if (p && p->write_func)
				p->write_func (emf, p, stream, info, cancellable);
		}

		camel_stream_write_string (stream, "</div></td></tr>", cancellable, NULL);
	}

	camel_stream_write_string (stream, "</table></div>", cancellable, NULL);
}

static void
efhd_write_secure_button (EMFormat *emf,
                          EMFormatPURI *puri,
                          CamelStream *stream,
                          EMFormatWriterInfo *info,
                          GCancellable *cancellable)
{
	gchar *str;

	if ((info->mode != EM_FORMAT_WRITE_MODE_NORMAL) &&
	    (info->mode != EM_FORMAT_WRITE_MODE_RAW))
		return;

	str = g_strdup_printf (
		"<object type=\"application/x-secure-button\" "
		"height=\"20\" width=\"100%%\" "
		"data=\"%s\" id=\"%s\"></object>", puri->uri, puri->uri);

	camel_stream_write_string (stream, str, cancellable, NULL);

	g_free (str);
}

static void
efhd_finalize (GObject *object)
{
	EMFormatHTMLDisplay *efhd;

	efhd = EM_FORMAT_HTML_DISPLAY (object);
	g_return_if_fail (efhd != NULL);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
efhd_preparse (EMFormat *emf)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) emf;

	efhd->priv->last_view = NULL;
}

static void
efhd_class_init (EMFormatHTMLDisplayClass *class)
{
	GObjectClass *object_class;
	EMFormatHTMLClass *format_html_class;
	EMFormatClass *format_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMFormatHTMLDisplayPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = efhd_finalize;

	format_html_class = EM_FORMAT_HTML_CLASS (class);
	format_html_class->html_widget_type = E_TYPE_MAIL_DISPLAY;

	format_class = EM_FORMAT_CLASS (class);
	format_class->preparse = efhd_preparse;

	efhd_builtin_init (class);
}

static void
efhd_init (EMFormatHTMLDisplay *efhd)
{
	efhd->priv = EM_FORMAT_HTML_DISPLAY_GET_PRIVATE (efhd);

        /* we want to convert url's etc */
	EM_FORMAT_HTML (efhd)->text_html_flags |=
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;

}

GType
em_format_html_display_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFormatHTMLDisplayClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) efhd_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFormatHTMLDisplay),
			0,     /* n_preallocs */
			(GInstanceInitFunc) efhd_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			EM_TYPE_FORMAT_HTML, "EMFormatHTMLDisplay",
			&type_info, 0);
	}

	return type;
}

EMFormatHTMLDisplay *
em_format_html_display_new (CamelSession *session)
{
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_FORMAT_HTML_DISPLAY,
		"session", session, NULL);
}

/* ********************************************************************** */

static EMFormatHandler type_builtin_table[] = {
	{ (gchar *) "x-evolution/message/prefix", efhd_message_prefix, },
	{ (gchar *) "x-evolution/message/attachment-bar", (EMFormatParseFunc) efhd_message_add_bar, efhd_write_attachment_bar, },
	{ (gchar *) "x-evolution/message/attachment", efhd_parse_attachment, efhd_write_attachment, },
	{ (gchar *) "x-evolution/message/x-secure-button", efhd_parse_secure, efhd_write_secure_button, },
};

static void
efhd_builtin_init (EMFormatHTMLDisplayClass *efhc)
{
	gint i;

	EMFormatClass *emfc = (EMFormatClass *) efhc;

	for (i = 0; i < G_N_ELEMENTS (type_builtin_table); i++)
		em_format_class_add_handler (emfc, &type_builtin_table[i]);
}

static void
efhd_message_prefix (EMFormat *emf,
                     CamelMimePart *part,
                     GString *part_id,
                     EMFormatParserInfo *info,
                     GCancellable *cancellable)
{
	const gchar *flag, *comp, *due;
	time_t date;
	gchar *iconpath, *due_date_str;
	GString *buffer;
	EMFormatAttachmentPURI *puri;

	if (emf->folder == NULL || emf->message_uid == NULL
	    || (flag = camel_folder_get_message_user_tag(emf->folder, emf->message_uid, "follow-up")) == NULL
	    || flag[0] == 0)
		return;

	puri = (EMFormatAttachmentPURI *) em_format_puri_new (
			emf, sizeof (EMFormatAttachmentPURI), part, ".message_prefix");

	puri->attachment_view_part_id = g_strdup (part_id->str);

	comp = camel_folder_get_message_user_tag(emf->folder, emf->message_uid, "completed-on");
	iconpath = e_icon_factory_get_icon_filename (comp && comp[0] ? "stock_mail-flag-for-followup-done" : "stock_mail-flag-for-followup", GTK_ICON_SIZE_MENU);
	if (iconpath) {
		gchar *classid;

		classid = g_strdup_printf (
			"icon:///em-format-html-display/%s/%s",
			part_id->str,
			comp && comp[0] ? "comp" : "uncomp");

		puri->puri.uri = classid;

		g_free (classid);
	}

	buffer = g_string_new ("");

	if (comp && comp[0]) {
		date = camel_header_decode_date (comp, NULL);
		due_date_str = e_datetime_format_format (
			"mail", "header", DTFormatKindDateTime, date);
		g_string_append_printf (
			buffer, "%s, %s %s",
			flag, _("Completed on"),
			due_date_str ? due_date_str : "???");
		g_free (due_date_str);
	} else if ((due = camel_folder_get_message_user_tag(emf->folder, emf->message_uid, "due-by")) != NULL && due[0]) {
		time_t now;

		date = camel_header_decode_date (due, NULL);
		now = time (NULL);
		if (now > date)
			g_string_append_printf (
				buffer,
				"<b>%s</b> ",
				_("Overdue:"));

		due_date_str = e_datetime_format_format (
			"mail", "header", DTFormatKindDateTime, date);
		/* Translators: the "by" is part of the string,
		 * like "Follow-up by Tuesday, January 13, 2009" */
		g_string_append_printf (
			buffer, "%s %s %s",
			flag, _("by"),
			due_date_str ? due_date_str : "???");
		g_free (due_date_str);
	} else {
		g_string_append (buffer, flag);
	}

	puri->description = g_string_free (buffer, FALSE);
}

/* ********************************************************************** */

/* attachment button callback */
static GtkWidget *
efhd_attachment_button (EMFormat *emf,
                        EMFormatPURI *puri,
                        GCancellable *cancellable)
{
	EMFormatAttachmentPURI *info = (EMFormatAttachmentPURI *) puri;
	GtkWidget *widget;

	/* FIXME: handle default shown case */
	d(printf("adding attachment button/content\n"));

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	if (!info || info->forward) {
		g_warning ("unable to expand the attachment\n");
		return NULL;
	}

	widget = e_attachment_button_new ();
	g_object_set_data (G_OBJECT (widget), "uri", puri->uri);
	e_attachment_button_set_attachment (
		E_ATTACHMENT_BUTTON (widget), info->attachment);
	e_attachment_button_set_view (
		E_ATTACHMENT_BUTTON (widget),
		EM_FORMAT_HTML_DISPLAY (emf)->priv->last_view);

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_show (widget);

	return widget;
}

static GtkWidget *
efhd_attachment_bar (EMFormat *emf,
                     EMFormatPURI *puri,
                     GCancellable *cancellable)
{
	EMFormatAttachmentBarPURI *abp = (EMFormatAttachmentBarPURI *) puri;
	GtkWidget *widget;

	widget = e_mail_attachment_bar_new (abp->store);
	EM_FORMAT_HTML_DISPLAY (emf)->priv->last_view = (EAttachmentView *) widget;

	return widget;
}

static void
efhd_message_add_bar (EMFormat *emf,
                      CamelMimePart *part,
                      GString *part_id,
                      EMFormatParserInfo *info,
                      GCancellable *cancellable)
{
	EMFormatAttachmentBarPURI *puri;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	len = part_id->len;
	g_string_append (part_id, ".attachment-bar");
	puri = (EMFormatAttachmentBarPURI *) em_format_puri_new (
			emf, sizeof (EMFormatAttachmentBarPURI), part, part_id->str);
	puri->puri.write_func = efhd_write_attachment_bar;
	puri->puri.widget_func = efhd_attachment_bar;
	puri->puri.free = efhd_attachment_bar_puri_free;
	puri->store = E_ATTACHMENT_STORE (e_attachment_store_new ());

	em_format_add_puri (emf, (EMFormatPURI *) puri);

	g_string_truncate (part_id, len);
}

static void
efhd_free_attach_puri_data (EMFormatPURI *puri)
{
	EMFormatAttachmentPURI *info = (EMFormatAttachmentPURI *) puri;

	g_return_if_fail (puri != NULL);

	if (info->attachment) {
		g_object_unref (info->attachment);
		info->attachment = NULL;
	}

	if (info->description) {
		g_free (info->description);
		info->description = NULL;
	}

	if (info->attachment_view_part_id) {
		g_free (info->attachment_view_part_id);
		info->attachment_view_part_id = NULL;
	}

	if (info->mstream) {
		g_object_unref (info->mstream);
		info->mstream = NULL;
	}
}
