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

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>

#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

#include "e-util/e-datetime-format.h"
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#ifdef HAVE_NSS
#include "certificate-viewer.h"
#include "e-cert-db.h"
#endif

#include "mail-config.h"

#include "e-mail-display.h"
#include "e-mail-attachment-bar.h"
#include "em-format-html-display.h"
#include "em-utils.h"
#include "widgets/misc/e-attachment-button.h"
#include "widgets/misc/e-attachment-view.h"

#define d(x)

#define EM_FORMAT_HTML_DISPLAY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FORMAT_HTML_DISPLAY, EMFormatHTMLDisplayPrivate))

struct _EMFormatHTMLDisplayPrivate {
	GtkWidget *attachment_view;  /* weak reference */
};

struct _smime_pobject {
	EMFormatHTMLPObject object;

	gint signature;
	CamelCipherValidity *valid;
	GtkWidget *widget;
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

static const gchar *smime_sign_colour[5] = {
	"", " bgcolor=\"#88bb88\"", " bgcolor=\"#bb8888\"", " bgcolor=\"#e8d122\"",""
};

static void efhd_attachment_frame(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri);
static void efhd_message_add_bar(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info);
static gboolean efhd_attachment_button (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject);
static gboolean efhd_attachment_optional (EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *object);

struct _attach_puri {
	EMFormatPURI puri;

	const EMFormatHandler *handle;

	const gchar *snoop_mime_type;

	/* for the > and V buttons */
	GtkWidget *forward, *down;
	/* currently no way to correlate this data to the frame :( */
	GtkHTML *frame;
	guint shown:1;

	/* Embedded Frame */
	GtkHTMLEmbedded *html;

	/* Attachment */
	EAttachment *attachment;

	/* image stuff */
	gint fit_width;
	gint fit_height;
	GtkImage *image;
	GtkWidget *event_box;

	/* Optional Text Mem Stream */
	CamelStreamMem *mstream;

	/* Signed / Encrypted */
        camel_cipher_validity_sign_t sign;
        camel_cipher_validity_encrypt_t encrypt;
};

static void efhd_message_prefix(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info);

static void efhd_builtin_init(EMFormatHTMLDisplayClass *efhc);

static gpointer parent_class;

static void
efhd_xpkcs7mime_free (EMFormatHTMLPObject *o)
{
	struct _smime_pobject *po = (struct _smime_pobject *)o;

	if (po->widget)
		gtk_widget_destroy(po->widget);
	camel_cipher_validity_free(po->valid);
}

static void
efhd_xpkcs7mime_info_response (GtkWidget *widget,
                               guint button,
                               struct _smime_pobject *po)
{
	gtk_widget_destroy (widget);
	po->widget = NULL;
}

#ifdef HAVE_NSS
static void
efhd_xpkcs7mime_viewcert_clicked (GtkWidget *button,
                                  struct _smime_pobject *po)
{
	CamelCipherCertInfo *info = g_object_get_data((GObject *)button, "e-cert-info");
	ECert *ec = NULL;

	if (info->cert_data)
		ec = e_cert_new (CERT_DupCertificate (info->cert_data));

	if (ec != NULL) {
		GtkWidget *w = certificate_viewer_show(ec);

		/* oddly enough certificate_viewer_show doesn't ... */
		gtk_widget_show(w);
		g_signal_connect(w, "response", G_CALLBACK(gtk_widget_destroy), NULL);

		if (w && po->widget)
			gtk_window_set_transient_for ((GtkWindow *)w, (GtkWindow *)po->widget);

		g_object_unref(ec);
	} else {
		g_warning("can't find certificate for %s <%s>", info->name?info->name:"", info->email?info->email:"");
	}
}
#endif

static void
efhd_xpkcs7mime_add_cert_table (GtkWidget *vbox,
                                CamelDList *certlist,
                                struct _smime_pobject *po)
{
	CamelCipherCertInfo *info = (CamelCipherCertInfo *)certlist->head;
	GtkTable *table = (GtkTable *)gtk_table_new(camel_dlist_length(certlist), 2, FALSE);
	gint n = 0;

	while (info->next) {
		gchar *la = NULL;
		const gchar *l = NULL;

		if (info->name) {
			if (info->email && strcmp(info->name, info->email) != 0)
				l = la = g_strdup_printf("%s <%s>", info->name, info->email);
			else
				l = info->name;
		} else {
			if (info->email)
				l = info->email;
		}

		if (l) {
			GtkWidget *w;
#if defined(HAVE_NSS)
			ECert *ec = NULL;
#endif
			w = gtk_label_new(l);
			gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.5);
			g_free(la);
			gtk_table_attach(table, w, 0, 1, n, n+1, GTK_FILL, GTK_FILL, 3, 3);
#if defined(HAVE_NSS)
			w = gtk_button_new_with_mnemonic(_("_View Certificate"));
			gtk_table_attach(table, w, 1, 2, n, n+1, 0, 0, 3, 3);
			g_object_set_data((GObject *)w, "e-cert-info", info);
			g_signal_connect(w, "clicked", G_CALLBACK(efhd_xpkcs7mime_viewcert_clicked), po);

			if (info->cert_data)
				ec = e_cert_new (CERT_DupCertificate (info->cert_data));

			if (ec == NULL)
				gtk_widget_set_sensitive(w, FALSE);
			else
				g_object_unref(ec);
#else
			w = gtk_label_new (_("This certificate is not viewable"));
			gtk_table_attach(table, w, 1, 2, n, n+1, 0, 0, 3, 3);
#endif
			n++;
		}

		info = info->next;
	}

	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)table, TRUE, TRUE, 6);
}

static void
efhd_xpkcs7mime_validity_clicked (GtkWidget *button,
                                  EMFormatHTMLPObject *pobject)
{
	struct _smime_pobject *po = (struct _smime_pobject *)pobject;
	GtkBuilder *builder;
	GtkWidget *vbox, *w;

	if (po->widget)
		/* FIXME: window raise? */
		return;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	po->widget = e_builder_get_widget(builder, "message_security_dialog");

	vbox = e_builder_get_widget(builder, "signature_vbox");
	w = gtk_label_new (_(smime_sign_table[po->valid->sign.status].description));
	gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.5);
	gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
	gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	if (po->valid->sign.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new(NULL);
		gtk_text_buffer_set_text(buffer, po->valid->sign.description, strlen(po->valid->sign.description));
		w = g_object_new(gtk_scrolled_window_get_type(),
				 "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "shadow_type", GTK_SHADOW_IN,
				 "child", g_object_new(gtk_text_view_get_type(),
						       "buffer", buffer,
						       "cursor_visible", FALSE,
						       "editable", FALSE,
						       "width_request", 500,
						       "height_request", 160,
						       NULL),
				 NULL);
		g_object_unref(buffer);

		gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	}

	if (!camel_dlist_empty(&po->valid->sign.signers))
		efhd_xpkcs7mime_add_cert_table(vbox, &po->valid->sign.signers, po);

	gtk_widget_show_all(vbox);

	vbox = e_builder_get_widget(builder, "encryption_vbox");
	w = gtk_label_new(_(smime_encrypt_table[po->valid->encrypt.status].description));
	gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.5);
	gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
	gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	if (po->valid->encrypt.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new(NULL);
		gtk_text_buffer_set_text(buffer, po->valid->encrypt.description, strlen(po->valid->encrypt.description));
		w = g_object_new(gtk_scrolled_window_get_type(),
				 "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				 "shadow_type", GTK_SHADOW_IN,
				 "child", g_object_new(gtk_text_view_get_type(),
						       "buffer", buffer,
						       "cursor_visible", FALSE,
						       "editable", FALSE,
						       "width_request", 500,
						       "height_request", 160,
						       NULL),
				 NULL);
		g_object_unref(buffer);

		gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	}

	if (!camel_dlist_empty(&po->valid->encrypt.encrypters))
		efhd_xpkcs7mime_add_cert_table(vbox, &po->valid->encrypt.encrypters, po);

	gtk_widget_show_all(vbox);

	g_object_unref(builder);

	g_signal_connect(po->widget, "response", G_CALLBACK(efhd_xpkcs7mime_info_response), po);
	gtk_widget_show(po->widget);
}

static gboolean
efhd_xpkcs7mime_button (EMFormatHTML *efh,
                        GtkHTMLEmbedded *eb,
                        EMFormatHTMLPObject *pobject)
{
	GtkWidget *container;
	GtkWidget *widget;
	struct _smime_pobject *po = (struct _smime_pobject *)pobject;
	const gchar *icon_name;

	/* FIXME: need to have it based on encryption and signing too */
	if (po->valid->sign.status != 0)
		icon_name = smime_sign_table[po->valid->sign.status].icon;
	else
		icon_name = smime_encrypt_table[po->valid->encrypt.status].icon;

	container = GTK_WIDGET (eb);

	widget = gtk_button_new ();
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (efhd_xpkcs7mime_validity_clicked), pobject);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	return TRUE;
}

static void
efhd_format_clone (EMFormat *emf,
                   CamelFolder *folder,
                   const gchar *uid,
                   CamelMimeMessage *msg,
                   EMFormat *src)
{
	if (emf != src)
		EM_FORMAT_HTML (emf)->header_wrap_flags = 0;

	/* Chain up to parent's format_clone() method. */
	EM_FORMAT_CLASS (parent_class)->
		format_clone (emf, folder, uid, msg, src);
}

static void
efhd_format_attachment (EMFormat *emf,
                        CamelStream *stream,
                        CamelMimePart *part,
                        const gchar *mime_type,
                        const EMFormatHandler *handle)
{
	gchar *classid, *text, *html;
	struct _attach_puri *info;

	classid = g_strdup_printf ("attachment%s", emf->part_id->str);
	info = (struct _attach_puri *) em_format_add_puri (
		emf, sizeof (*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject (
		EM_FORMAT_HTML (emf), sizeof (EMFormatHTMLPObject),
		classid, part, efhd_attachment_button);
	info->handle = handle;
	info->shown = em_format_is_inline (
		emf, info->puri.part_id, info->puri.part, handle);
	info->snoop_mime_type = emf->snoop_mime_type;
	info->attachment = e_attachment_new ();
	e_attachment_set_mime_part (info->attachment, info->puri.part);

	if (emf->valid) {
		info->sign = emf->valid->sign.status;
		info->encrypt = emf->valid->encrypt.status;
	}

	camel_stream_write_string (
		stream, EM_FORMAT_HTML_VPAD
		"<table cellspacing=0 cellpadding=0><tr><td>"
		"<table width=10 cellspacing=0 cellpadding=0>"
		"<tr><td></td></tr></table></td>", NULL);

	camel_stream_printf (
		stream, "<td><object classid=\"%s\"></object></td>", classid);

	camel_stream_write_string (
		stream, "<td><table width=3 cellspacing=0 cellpadding=0>"
		"<tr><td></td></tr></table></td><td><font size=-1>", NULL);

	/* output some info about it */
	/* FIXME: should we look up mime_type from object again? */
	text = em_format_describe_part (part, mime_type);
	html = camel_text_to_html (
		text, EM_FORMAT_HTML (emf)->text_html_flags &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string (stream, html, NULL);
	g_free (html);
	g_free (text);

	camel_stream_write_string (
		stream, "</font></td></tr><tr></table>\n"
		EM_FORMAT_HTML_VPAD, NULL);

	if (handle && info->shown)
		handle->handler (emf, stream, part, handle, FALSE);

	g_free (classid);
}

static void
efhd_format_optional (EMFormat *emf,
                      CamelStream *fstream,
                      CamelMimePart *part,
                      CamelStream *mstream)
{
	gchar *classid, *html;
	struct _attach_puri *info;
	CamelStream *stream = NULL;

	if (CAMEL_IS_STREAM_FILTER (fstream))
		stream = camel_stream_filter_get_source (
			CAMEL_STREAM_FILTER (fstream));
	if (stream == NULL)
		stream = fstream;

	classid = g_strdup_printf ("optional%s", emf->part_id->str);
	info = (struct _attach_puri *) em_format_add_puri (
		emf, sizeof (*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject (
		EM_FORMAT_HTML (emf), sizeof (EMFormatHTMLPObject),
		classid, part, efhd_attachment_optional);
	info->handle = em_format_find_handler (emf, "text/plain");
	info->shown = FALSE;
	info->snoop_mime_type = "text/plain";
	info->attachment = e_attachment_new ();
	e_attachment_set_mime_part (info->attachment, info->puri.part);
	info->mstream = (CamelStreamMem *) g_object_ref (mstream);
	if (emf->valid) {
		info->sign = emf->valid->sign.status;
		info->encrypt = emf->valid->encrypt.status;
	}

	camel_stream_write_string (
		stream, EM_FORMAT_HTML_VPAD
		"<table cellspacing=0 cellpadding=0><tr><td>"
		"<h3><font size=-1 color=red>", NULL);

	html = camel_text_to_html (
		_("Evolution cannot render this email as it is too "
		  "large to process. You can view it unformatted or "
		  "with an external text editor."),
		EM_FORMAT_HTML (emf)->text_html_flags &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string (stream, html, NULL);
	camel_stream_write_string (
		stream, "</font></h3></td></tr></table>\n", NULL);
	camel_stream_write_string (
		stream, "<table cellspacing=0 cellpadding=0><tr>", NULL);
	camel_stream_printf (
		stream, "<td><object classid=\"%s\"></object>"
		"</td></tr></table>", classid);

	g_free(html);

	camel_stream_write_string (stream, EM_FORMAT_HTML_VPAD, NULL);

	g_free (classid);
}

static void
efhd_format_secure (EMFormat *emf,
                    CamelStream *stream,
                    CamelMimePart *part,
                    CamelCipherValidity *valid)
{
	EMFormatClass *format_class;

	format_class = g_type_class_peek (EM_TYPE_FORMAT);
	format_class->format_secure (emf, stream, part, valid);

	if (emf->valid == valid
	    && (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE
		|| valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)) {
		gchar *classid;
		struct _smime_pobject *pobj;

		camel_stream_printf (
			stream, "<table border=0 width=\"100%%\" "
			"cellpadding=3 cellspacing=0%s><tr>",
			smime_sign_colour[valid->sign.status]);

		classid = g_strdup_printf (
			"smime:///em-format-html/%s/icon/signed",
			emf->part_id->str);
		pobj = (struct _smime_pobject *) em_format_html_add_pobject (
			EM_FORMAT_HTML (emf), sizeof (*pobj),
			classid, part, efhd_xpkcs7mime_button);
		pobj->valid = camel_cipher_validity_clone(valid);
		pobj->object.free = efhd_xpkcs7mime_free;
		camel_stream_printf (
			stream, "<td valign=center><object classid=\"%s\">"
			"</object></td><td width=100%% valign=center>",
			classid);
		g_free (classid);

		if (valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
			gchar *signers;
			const gchar *desc;
			gint status;

			status = valid->sign.status;
			desc = smime_sign_table[status].shortdesc;

			camel_stream_printf (stream, "%s", gettext (desc));

			signers = em_format_html_format_cert_infos ((CamelCipherCertInfo *)valid->sign.signers.head);
			if (signers && *signers) {
				camel_stream_printf (stream, " (%s)", signers);
			}
			g_free (signers);
		}

		if (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
			const gchar *desc;
			gint status;

			if (valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
				camel_stream_printf (stream, "<br>");
			}

			status = valid->encrypt.status;
			desc = smime_encrypt_table[status].shortdesc;
			camel_stream_printf (stream, "%s", gettext (desc));
		}

		camel_stream_printf(stream, "</td></tr></table>");
	}
}

static void
efhd_class_init (EMFormatHTMLDisplayClass *class)
{
	EMFormatClass *format_class;
	EMFormatHTMLClass *format_html_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMFormatHTMLDisplayPrivate));

	format_class = EM_FORMAT_CLASS (class);
	format_class->format_clone = efhd_format_clone;
	format_class->format_attachment = efhd_format_attachment;
	format_class->format_optional = efhd_format_optional;
	format_class->format_secure = efhd_format_secure;

	format_html_class = EM_FORMAT_HTML_CLASS (class);
	format_html_class->html_widget_type = E_TYPE_MAIL_DISPLAY;

	efhd_builtin_init (class);
}

static void
efhd_init (EMFormatHTMLDisplay *efhd)
{
	EWebView *web_view;

	web_view = em_format_html_get_web_view (EM_FORMAT_HTML (efhd));

	efhd->priv = EM_FORMAT_HTML_DISPLAY_GET_PRIVATE (efhd);

	e_mail_display_set_formatter (
		E_MAIL_DISPLAY (web_view), EM_FORMAT_HTML (efhd));

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
em_format_html_display_new (void)
{
	return g_object_new (EM_TYPE_FORMAT_HTML_DISPLAY, NULL);
}

/* ********************************************************************** */

static EMFormatHandler type_builtin_table[] = {
	{ (gchar *) "x-evolution/message/prefix", (EMFormatFunc)efhd_message_prefix },
	{ (gchar *) "x-evolution/message/post-header", (EMFormatFunc)efhd_message_add_bar }
};

static void
efhd_builtin_init(EMFormatHTMLDisplayClass *efhc)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (type_builtin_table); i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}

static void
efhd_write_image(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	CamelDataWrapper *dw = camel_medium_get_content ((CamelMedium *)puri->part);

	/* TODO: identical to efh_write_image */
	d(printf("writing image '%s'\n", puri->cid));
	camel_data_wrapper_decode_to_stream(dw, stream, NULL);
	camel_stream_close(stream, NULL);
}

static void
efhd_message_prefix(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	const gchar *flag, *comp, *due;
	time_t date;
	gchar *iconpath, *due_date_str;

	if (emf->folder == NULL || emf->uid == NULL
	    || (flag = camel_folder_get_message_user_tag(emf->folder, emf->uid, "follow-up")) == NULL
	    || flag[0] == 0)
		return;

	/* header displayed for message-flags in mail display */
	camel_stream_printf(stream, "<table border=1 width=\"100%%\" cellspacing=2 cellpadding=2><tr>");

	comp = camel_folder_get_message_user_tag(emf->folder, emf->uid, "completed-on");
	iconpath = e_icon_factory_get_icon_filename (comp && comp[0] ? "stock_mail-flag-for-followup-done" : "stock_mail-flag-for-followup", GTK_ICON_SIZE_MENU);
	if (iconpath) {
		CamelMimePart *iconpart;

		iconpart = em_format_html_file_part((EMFormatHTML *)emf, "image/png", iconpath);
		g_free (iconpath);
		if (iconpart) {
			gchar *classid;

			classid = g_strdup_printf("icon:///em-format-html-display/%s/%s", emf->part_id->str, comp&&comp[0]?"comp":"uncomp");
			camel_stream_printf(stream, "<td align=\"left\"><img src=\"%s\"></td>", classid);
			(void)em_format_add_puri(emf, sizeof(EMFormatPURI), classid, iconpart, efhd_write_image);
			g_free(classid);
			g_object_unref (iconpart);
		}
	}

	camel_stream_printf(stream, "<td align=\"left\" width=\"100%%\">");

	if (comp && comp[0]) {
		date = camel_header_decode_date (comp, NULL);
		due_date_str = e_datetime_format_format ("mail", "header", DTFormatKindDateTime, date);
		camel_stream_printf (stream, "%s, %s %s", flag, _("Completed on"), due_date_str ? due_date_str : "???");
		g_free (due_date_str);
	} else if ((due = camel_folder_get_message_user_tag(emf->folder, emf->uid, "due-by")) != NULL && due[0]) {
		time_t now;

		date = camel_header_decode_date(due, NULL);
		now = time(NULL);
		if (now > date)
			camel_stream_printf(stream, "<b>%s</b>&nbsp;", _("Overdue:"));

		due_date_str = e_datetime_format_format ("mail", "header", DTFormatKindDateTime, date);
		/* To Translators: the "by" is part of the string, like "Follow-up by Tuesday, January 13, 2009" */
		camel_stream_printf (stream, "%s %s %s", flag, _("by"), due_date_str ? due_date_str : "???");
	} else {
		camel_stream_printf(stream, "%s", flag);
	}

	camel_stream_printf(stream, "</td></tr></table>");
}

/* ********************************************************************** */

static void
efhd_attachment_button_expanded (EAttachmentButton *button,
                                 GParamSpec *pspec,
                                 struct _attach_puri *info)
{
	EMFormatHTML *efh;

	/* FIXME The PURI struct seems to have some lifecycle issues,
	 *       because casting info->puri.format to an EMFormatHTML
	 *       can lead to crashes.  So we hack around it. */
	efh = g_object_get_data (G_OBJECT (button), "efh");
	g_return_if_fail (EM_IS_FORMAT_HTML (efh));

	if (efh->state == EM_FORMAT_HTML_STATE_RENDERING)
		return;

	info->shown = e_attachment_button_get_expanded (button);

	em_format_set_inline (
		info->puri.format, info->puri.part_id, info->shown);
}

/* ********************************************************************** */

/* attachment button callback */
static gboolean
efhd_attachment_button(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)efh;
	struct _attach_puri *info;
	EAttachmentView *view;
	EAttachmentStore *store;
	EAttachment *attachment;
	EWebView *web_view;
	GtkWidget *widget;
	gpointer parent;
	EMFormat *emf = (EMFormat *) efh;
	guint32 size = 0;

	/* FIXME: handle default shown case */
	d(printf("adding attachment button/content\n"));

	if (emf->folder && emf->folder->summary && emf->uid) {
		CamelMessageInfo *mi;

		mi = camel_folder_summary_uid (emf->folder->summary, emf->uid);
		if (mi) {
			const CamelMessageContentInfo *ci;

			ci = camel_folder_summary_guess_content_info (mi, camel_mime_part_get_content_type (pobject->part));
			if (ci) {
				size = ci->size;
				/* what if its not encoded in base64 ? is it a case to consider? */
				if (ci->encoding && !g_ascii_strcasecmp (ci->encoding, "base64"))
					size = size/1.37;
			}
			camel_message_info_free (mi);
		}
	}

	info = (struct _attach_puri *)em_format_find_puri((EMFormat *)efh, pobject->classid);

	if (!info || info->forward) {
		g_warning ("unable to expand the attachment\n");
		return TRUE;
	}

	attachment = info->attachment;
	e_attachment_set_shown (attachment, info->shown);
	e_attachment_set_signed (attachment, info->sign);
	e_attachment_set_encrypted (attachment, info->encrypt);
	e_attachment_set_can_show (attachment, info->handle != NULL);

	web_view = em_format_html_get_web_view (efh);
	parent = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	view = em_format_html_display_get_attachment_view (efhd);
	gtk_widget_show (GTK_WIDGET (view));

	store = e_attachment_view_get_store (view);
	e_attachment_store_add_attachment (store, info->attachment);

	e_attachment_load_async (
		info->attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);
	if (size != 0) {
		GFileInfo *fileinfo;

		fileinfo = e_attachment_get_file_info (info->attachment);
		g_file_info_set_size (fileinfo, size);
		e_attachment_set_file_info (info->attachment, fileinfo);
	}

	widget = e_attachment_button_new (view);
	e_attachment_button_set_attachment (
		E_ATTACHMENT_BUTTON (widget), attachment);
	gtk_container_add (GTK_CONTAINER (eb), widget);
	gtk_widget_show (widget);

	/* FIXME Not sure why the expanded callback can't just use
	 *       info->puri.format, but there seems to be lifecycle
	 *       issues with the PURI struct.  Maybe it should have
	 *       a reference count? */
	g_object_set_data (G_OBJECT (widget), "efh", efh);

	g_signal_connect (
		widget, "notify::expanded",
		G_CALLBACK (efhd_attachment_button_expanded), info);

	return TRUE;
}

static void
efhd_attachment_frame (EMFormat *emf,
                       CamelStream *stream,
                       EMFormatPURI *puri)
{
	struct _attach_puri *info = (struct _attach_puri *)puri;

	if (info->shown)
		info->handle->handler (
			emf, stream, info->puri.part, info->handle, FALSE);

	camel_stream_close (stream, NULL);
}

static void
efhd_bar_resize (EMFormatHTML *efh,
                 GtkAllocation *event)
{
	EMFormatHTMLDisplayPrivate *priv;
	GtkAllocation allocation;
	EWebView *web_view;
	GtkWidget *widget;
	gint width;

	priv = EM_FORMAT_HTML_DISPLAY_GET_PRIVATE (efh);

	web_view = em_format_html_get_web_view (efh);

	widget = GTK_WIDGET (web_view);
	gtk_widget_get_allocation (widget, &allocation);
	width = allocation.width - 12;

	if (width > 0) {
		widget = priv->attachment_view;
		gtk_widget_set_size_request (widget, width, -1);
	}
}

static gboolean
efhd_add_bar (EMFormatHTML *efh,
              GtkHTMLEmbedded *eb,
              EMFormatHTMLPObject *pobject)
{
	EMFormatHTMLDisplayPrivate *priv;
	GtkWidget *widget;

	/* XXX See note in efhd_message_add_bar(). */
	if (!EM_IS_FORMAT_HTML_DISPLAY (efh))
		return FALSE;

	priv = EM_FORMAT_HTML_DISPLAY_GET_PRIVATE (efh);

	widget = e_mail_attachment_bar_new ();
	gtk_container_add (GTK_CONTAINER (eb), widget);
	priv->attachment_view = widget;
	gtk_widget_hide (widget);

	g_signal_connect_swapped (
		eb, "size-allocate",
		G_CALLBACK (efhd_bar_resize), efh);

	return TRUE;
}

static void
efhd_message_add_bar (EMFormat *emf,
                      CamelStream *stream,
                      CamelMimePart *part,
                      const EMFormatHandler *info)
{
	const gchar *classid = "attachment-bar";

	/* XXX Apparently this installs the callback for -all-
	 *     EMFormatHTML subclasses, not just this subclass.
	 *     Bad idea.  So we have to filter out other types
	 *     in the callback. */
	em_format_html_add_pobject (
		EM_FORMAT_HTML (emf),
		sizeof (EMFormatHTMLPObject),
		classid, part, efhd_add_bar);

	camel_stream_printf (
		stream, "<td><object classid=\"%s\"></object></td>", classid);
}

static void
efhd_optional_button_show (GtkWidget *widget, GtkWidget *w)
{
	GtkWidget *label = g_object_get_data (G_OBJECT (widget), "text-label");

	if (gtk_widget_get_visible (w)) {
		gtk_widget_hide (w);
		gtk_label_set_text_with_mnemonic (GTK_LABEL (label), _("View _Unformatted"));
	} else {
		gtk_label_set_text_with_mnemonic (GTK_LABEL (label), _("Hide _Unformatted"));
		gtk_widget_show (w);
	}
}

static void
efhd_resize (GtkWidget *w, GtkAllocation *event, EMFormatHTML *efh)
{
	EWebView *web_view;
	GtkAllocation allocation;

	web_view = em_format_html_get_web_view (efh);
	gtk_widget_get_allocation (GTK_WIDGET (web_view), &allocation);
	gtk_widget_set_size_request (w, allocation.width - 48, 250);
}

/* optional render attachment button callback */
static gboolean
efhd_attachment_optional(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	struct _attach_puri *info;
	GtkWidget *hbox, *vbox, *button, *mainbox, *scroll, *label, *img;
	AtkObject *a11y;
	GtkWidget *view;
	GtkAllocation allocation;
	GtkTextBuffer *buffer;
	GByteArray *byte_array;
	EWebView *web_view;

	/* FIXME: handle default shown case */
	d(printf("adding attachment button/content for optional rendering\n"));

	info = (struct _attach_puri *)em_format_find_puri((EMFormat *)efh, pobject->classid);
	if (!info || info->forward) {
		g_warning ("unable to expand the attachment\n");
		return TRUE;
	}

	scroll = gtk_scrolled_window_new (NULL, NULL);
	mainbox = gtk_hbox_new(FALSE, 0);

	button = gtk_button_new();
	hbox = gtk_hbox_new (FALSE, 0);
	img = gtk_image_new_from_icon_name (
		"stock_show-all", GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new_with_mnemonic(_("View _Unformatted"));
	g_object_set_data (G_OBJECT (button), "text-label", (gpointer)label);
	gtk_box_pack_start (GTK_BOX (hbox), img, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 2);
	gtk_widget_show_all (hbox);
	gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (hbox));
	if (info->handle)
		g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK(efhd_optional_button_show), scroll);
	else {
		gtk_widget_set_sensitive(button, FALSE);
		gtk_widget_set_can_focus (button, FALSE);
	}

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX (mainbox), button, FALSE, FALSE, 6);

	button = gtk_button_new();
	hbox = gtk_hbox_new (FALSE, 0);
	img = gtk_image_new_from_stock (
		GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new_with_mnemonic(_("O_pen With"));
	gtk_box_pack_start (GTK_BOX (hbox), img, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE), TRUE, TRUE, 2);
	gtk_widget_show_all (hbox);
	gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (hbox));

	a11y = gtk_widget_get_accessible (button);
	atk_object_set_name (a11y, _("Attachment"));

	gtk_box_pack_start(GTK_BOX (mainbox), button, FALSE, FALSE, 6);

	gtk_widget_show_all(mainbox);

	gtk_box_pack_start(GTK_BOX (vbox), mainbox, FALSE, FALSE, 6);

	view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (view));
	byte_array = camel_stream_mem_get_byte_array (info->mstream);
	gtk_text_buffer_set_text (
		buffer, (gchar *) byte_array->data, byte_array->len);
	g_object_unref (info->mstream);
	info->mstream = NULL;
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (view));
	gtk_box_pack_start(GTK_BOX (vbox), scroll, TRUE, TRUE, 6);
	gtk_widget_show (GTK_WIDGET(view));

	web_view = em_format_html_get_web_view (efh);
	gtk_widget_get_allocation (GTK_WIDGET (web_view), &allocation);
	gtk_widget_set_size_request (scroll, allocation.width - 48, 250);
	g_signal_connect (scroll, "size_allocate", G_CALLBACK(efhd_resize), efh);
	gtk_widget_show (scroll);

	if (!info->shown)
		gtk_widget_hide (scroll);

	gtk_widget_show (vbox);
	gtk_container_add(GTK_CONTAINER (eb), vbox);
	info->handle = NULL;

	return TRUE;
}

EAttachmentView *
em_format_html_display_get_attachment_view (EMFormatHTMLDisplay *html_display)
{
	g_return_val_if_fail (EM_IS_FORMAT_HTML_DISPLAY (html_display), NULL);

	return E_ATTACHMENT_VIEW (html_display->priv->attachment_view);
}
