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

#include <glade/glade.h>

#include <glib/gi18n.h>

#include <camel/camel-stream.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-multipart.h>
#include <camel/camel-internet-address.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-cipher-context.h>
#include <camel/camel-folder.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-operation.h>

#include <misc/e-cursors.h>
#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

#include <libedataserver/e-msgport.h>
#include "e-util/e-datetime-format.h"
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#ifdef HAVE_NSS
#include "certificate-viewer.h"
#include "e-cert-db.h"
#endif

#include "mail-config.h"

#include "e-mail-attachment-bar.h"
#include "em-format-html-display.h"
#include "em-icon-stream.h"
#include "em-utils.h"
#include "em-popup.h"
#include "e-icon-entry.h"
#include "widgets/misc/e-attachment-button.h"
#include "widgets/misc/e-attachment-view.h"

#define d(x)

struct _EMFormatHTMLDisplayPrivate {
	GtkWidget *attachment_view;  /* weak reference */
};

static gint efhd_html_button_press_event (GtkWidget *widget, GdkEventButton *event, EMFormatHTMLDisplay *efh);
static void efhd_html_link_clicked (GtkHTML *html, const gchar *url, EMFormatHTMLDisplay *efhd);
static void efhd_html_on_url (GtkHTML *html, const gchar *url, EMFormatHTMLDisplay *efhd);

static void efhd_attachment_frame(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri);
static gboolean efhd_attachment_image(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject);
static void efhd_message_add_bar(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info);

struct _attach_puri {
	EMFormatPURI puri;

	const EMFormatHandler *handle;

	const gchar *snoop_mime_type;

	/* for the > and V buttons */
	GtkWidget *forward, *down;
	/* currently no way to correlate this data to the frame :( */
	GtkHTML *frame;
	CamelStream *output;
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

static void efhd_iframe_created(GtkHTML *html, GtkHTML *iframe, EMFormatHTMLDisplay *efh);
/*static void efhd_url_requested(GtkHTML *html, const gchar *url, GtkHTMLStream *handle, EMFormatHTMLDisplay *efh);
  static gboolean efhd_object_requested(GtkHTML *html, GtkHTMLEmbedded *eb, EMFormatHTMLDisplay *efh);*/

static void efhd_message_prefix(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info);

static const EMFormatHandler *efhd_find_handler(EMFormat *emf, const gchar *mime_type);
static void efhd_format_clone(EMFormat *, CamelFolder *folder, const gchar *, CamelMimeMessage *msg, EMFormat *);
static void efhd_format_error(EMFormat *emf, CamelStream *stream, const gchar *txt);
static void efhd_format_source(EMFormat *, CamelStream *, CamelMimePart *);
static void efhd_format_attachment(EMFormat *, CamelStream *, CamelMimePart *, const gchar *, const EMFormatHandler *);
static void efhd_format_optional(EMFormat *, CamelStream *, CamelMimePart *, CamelStream *);
static void efhd_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid);

static void efhd_builtin_init(EMFormatHTMLDisplayClass *efhc);

enum {
	EFHD_LINK_CLICKED,
	EFHD_POPUP_EVENT,
	EFHD_ON_URL,
	EFHD_LAST_SIGNAL
};

static guint efhd_signals[EFHD_LAST_SIGNAL] = { 0 };

static EMFormatHTMLClass *efhd_parent;
static EMFormatClass *efhd_format_class;

static void
efhd_gtkhtml_realise(GtkHTML *html, EMFormatHTMLDisplay *efhd)
{
	GtkStyle *style;

	/* FIXME: does this have to be re-done every time we draw? */

	/* My favorite thing to do... muck around with colors so we respect people's stupid themes.
	   However, we only do this if we are rendering to the screen -- we ignore the theme
	   when we are printing. */
	style = gtk_widget_get_style((GtkWidget *)html);
	if (style) {
		gint state = GTK_WIDGET_STATE(html);
		gushort r, g, b;

		r = style->fg[state].red >> 8;
		g = style->fg[state].green >> 8;
		b = style->fg[state].blue >> 8;

		efhd->formathtml.header_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;

		r = style->bg[state].red >> 8;
		g = style->bg[state].green >> 8;
		b = style->bg[state].blue >> 8;

		efhd->formathtml.body_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;

		r = style->dark[state].red >> 8;
		g = style->dark[state].green >> 8;
		b = style->dark[state].blue >> 8;

		efhd->formathtml.frame_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;

		r = style->base[GTK_STATE_NORMAL].red >> 8;
		g = style->base[GTK_STATE_NORMAL].green >> 8;
		b = style->base[GTK_STATE_NORMAL].blue >> 8;

		efhd->formathtml.content_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;

		r = style->text[state].red >> 8;
		g = style->text[state].green >> 8;
		b = style->text[state].blue >> 8;

		efhd->formathtml.text_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;
#undef DARKER
	}
}

static void
efhd_gtkhtml_style_set(GtkHTML *html, GtkStyle *old, EMFormatHTMLDisplay *efhd)
{
	efhd_gtkhtml_realise(html, efhd);
	em_format_redraw((EMFormat *)efhd);
}

static void
efhd_init(GObject *o)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)o;
#define efh ((EMFormatHTML *)efhd)

	efhd->priv = g_malloc0(sizeof(*efhd->priv));

	g_signal_connect(efh->html, "realize", G_CALLBACK(efhd_gtkhtml_realise), o);
	g_signal_connect(efh->html, "style-set", G_CALLBACK(efhd_gtkhtml_style_set), o);
	/* we want to convert url's etc */
	efh->text_html_flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
#undef efh

	efhd->nobar = getenv("EVOLUTION_NO_BAR") != NULL;
}

static void
efhd_finalise(GObject *o)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)o;

	/* check pending stuff */

	g_free(efhd->priv);

	((GObjectClass *)efhd_parent)->finalize(o);
}

static gboolean
efhd_bool_accumulator(GSignalInvocationHint *ihint, GValue *out, const GValue *in, gpointer data)
{
	gboolean val = g_value_get_boolean(in);

	g_value_set_boolean(out, val);

	return !val;
}

static void
efhd_class_init(GObjectClass *klass)
{
	((EMFormatClass *)klass)->find_handler = efhd_find_handler;
	((EMFormatClass *)klass)->format_clone = efhd_format_clone;
	((EMFormatClass *)klass)->format_error = efhd_format_error;
	((EMFormatClass *)klass)->format_source = efhd_format_source;
	((EMFormatClass *)klass)->format_attachment = efhd_format_attachment;
	((EMFormatClass *)klass)->format_optional = efhd_format_optional;
	((EMFormatClass *)klass)->format_secure = efhd_format_secure;

	klass->finalize = efhd_finalise;

	efhd_signals[EFHD_LINK_CLICKED] =
		g_signal_new("link_clicked",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(EMFormatHTMLDisplayClass, link_clicked),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__POINTER,
			     G_TYPE_NONE, 1, G_TYPE_POINTER);

	efhd_signals[EFHD_POPUP_EVENT] =
		g_signal_new("popup_event",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(EMFormatHTMLDisplayClass, popup_event),
			     efhd_bool_accumulator, NULL,
			     e_marshal_BOOLEAN__BOXED_POINTER_POINTER,
			     G_TYPE_BOOLEAN, 3,
			     GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
			     G_TYPE_POINTER, G_TYPE_POINTER);

	efhd_signals[EFHD_ON_URL] =
		g_signal_new("on_url",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(EMFormatHTMLDisplayClass, on_url),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__STRING,
			     G_TYPE_NONE, 1,
			     G_TYPE_STRING);

	efhd_builtin_init((EMFormatHTMLDisplayClass *)klass);
}

GType
em_format_html_display_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFormatHTMLDisplayClass),
			NULL, NULL,
			(GClassInitFunc)efhd_class_init,
			NULL, NULL,
			sizeof(EMFormatHTMLDisplay), 0,
			(GInstanceInitFunc)efhd_init
		};
		efhd_parent = g_type_class_ref(em_format_html_get_type());
		efhd_format_class = g_type_class_ref(em_format_get_type());
		type = g_type_register_static(em_format_html_get_type(), "EMFormatHTMLDisplay", &info, 0);
	}

	return type;
}

static gboolean
efhd_scroll_event(GtkWidget *w, GdkEventScroll *event, EMFormatHTMLDisplay *efhd)
{
	if (event->state & GDK_CONTROL_MASK)
	{
		if (event->direction == GDK_SCROLL_UP)
		{
			gtk_html_zoom_in (efhd->formathtml.html);
		}
		else if (event->direction == GDK_SCROLL_DOWN)
		{
			gtk_html_zoom_out (efhd->formathtml.html);
		}
		return TRUE;
	}
	return FALSE;
}

EMFormatHTMLDisplay *em_format_html_display_new(void)
{
	EMFormatHTMLDisplay *efhd;

	efhd = g_object_new(em_format_html_display_get_type(), 0);

	g_signal_connect(efhd->formathtml.html, "iframe_created", G_CALLBACK(efhd_iframe_created), efhd);
	g_signal_connect(efhd->formathtml.html, "link_clicked", G_CALLBACK(efhd_html_link_clicked), efhd);
	g_signal_connect(efhd->formathtml.html, "on_url", G_CALLBACK(efhd_html_on_url), efhd);
	g_signal_connect(efhd->formathtml.html, "button_press_event", G_CALLBACK(efhd_html_button_press_event), efhd);
	g_signal_connect(efhd->formathtml.html,"scroll_event", G_CALLBACK(efhd_scroll_event), efhd);

	return efhd;
}

void em_format_html_display_goto_anchor(EMFormatHTMLDisplay *efhd, const gchar *name)
{
	printf("FIXME: go to anchor '%s'\n", name);
}

void em_format_html_display_set_animate(EMFormatHTMLDisplay *efhd, gboolean state)
{
	efhd->animate = state;
	gtk_html_set_animate(((EMFormatHTML *)efhd)->html, state);
}

void em_format_html_display_set_caret_mode(EMFormatHTMLDisplay *efhd, gboolean state)
{
	efhd->caret_mode = state;
	gtk_html_set_caret_mode(((EMFormatHTML *)efhd)->html, state);
}

void
em_format_html_display_cut (EMFormatHTMLDisplay *efhd)
{
	gtk_html_cut (((EMFormatHTML *) efhd)->html);
}

void
em_format_html_display_copy (EMFormatHTMLDisplay *efhd)
{
	gtk_html_copy (((EMFormatHTML *) efhd)->html);
}

void
em_format_html_display_paste (EMFormatHTMLDisplay *efhd)
{
	gtk_html_paste (((EMFormatHTML *) efhd)->html, FALSE);
}

void
em_format_html_display_zoom_in (EMFormatHTMLDisplay *efhd)
{
	gtk_html_zoom_in (((EMFormatHTML *) efhd)->html);
}

void
em_format_html_display_zoom_out (EMFormatHTMLDisplay *efhd)
{
	gtk_html_zoom_out (((EMFormatHTML *) efhd)->html);
}

void
em_format_html_display_zoom_reset (EMFormatHTMLDisplay *efhd)
{
	gtk_html_zoom_reset (((EMFormatHTML *) efhd)->html);
}

/* ********************************************************************** */

static void
efhd_iframe_created(GtkHTML *html, GtkHTML *iframe, EMFormatHTMLDisplay *efh)
{
	d(printf("Iframe created %p ... \n", iframe));

	g_signal_connect(iframe, "button_press_event", G_CALLBACK (efhd_html_button_press_event), efh);

	return;
}

static void
efhd_get_uri_puri (GtkWidget *html, GdkEventButton *event, EMFormatHTMLDisplay *efhd, gchar **uri, EMFormatPURI **puri)
{
	gchar *url, *img_url;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));
	g_return_if_fail (efhd != NULL);

	if (event) {
		url = gtk_html_get_url_at (GTK_HTML (html), event->x, event->y);
		img_url = gtk_html_get_image_src_at (GTK_HTML (html), event->x, event->y);
	} else {
		url = gtk_html_get_cursor_url (GTK_HTML (html));
		img_url = gtk_html_get_cursor_image_src (GTK_HTML (html));
	}

	if (img_url) {
		if (!(strstr (img_url, "://") || g_ascii_strncasecmp (img_url, "cid:", 4) == 0)) {
			gchar *u = g_filename_to_uri (img_url, NULL, NULL);
			g_free (img_url);
			img_url = u;
		}
	}

	if (puri) {
		if (url)
			*puri = em_format_find_puri ((EMFormat *)efhd, url);

		if (!*puri && img_url)
			*puri = em_format_find_puri ((EMFormat *)efhd, img_url);
	}

	if (uri) {
		*uri = NULL;
		if (img_url && g_ascii_strncasecmp (img_url, "cid:", 4) != 0) {
			if (url)
				*uri = g_strdup_printf ("%s\n%s", url, img_url);
			else {
				*uri = img_url;
				img_url = NULL;
			}
		} else {
			*uri = url;
			url = NULL;
		}
	}

	g_free (url);
	g_free (img_url);
}

static gint
efhd_html_button_press_event (GtkWidget *widget, GdkEventButton *event, EMFormatHTMLDisplay *efhd)
{
	gchar *uri = NULL;
	EMFormatPURI *puri = NULL;
	gboolean res = FALSE;

	if (event->button != 3)
		return FALSE;

	d(printf("popup button pressed\n"));

	efhd_get_uri_puri (widget, event, efhd, &uri, &puri);

	if (uri && !strncmp (uri, "##", 2)) {
		g_free (uri);
		return TRUE;
	}

	g_signal_emit((GtkObject *)efhd, efhd_signals[EFHD_POPUP_EVENT], 0, event, uri, puri?puri->part:NULL, &res);

	g_free (uri);

	return res;
}

gboolean
em_format_html_display_popup_menu (EMFormatHTMLDisplay *efhd)
{
	GtkHTML *html;
	gchar *uri = NULL;
	EMFormatPURI *puri = NULL;
	gboolean res = FALSE;

	html = efhd->formathtml.html;

	efhd_get_uri_puri (GTK_WIDGET (html), NULL, efhd, &uri, &puri);

	g_signal_emit ((GtkObject *)efhd, efhd_signals[EFHD_POPUP_EVENT], 0, NULL, uri, puri?puri->part:NULL, &res);

	g_free (uri);

	return res;
}

static void
efhd_html_link_clicked (GtkHTML *html, const gchar *url, EMFormatHTMLDisplay *efhd)
{
	d(printf("link clicked event '%s'\n", url));
	if (url && !strncmp(url, "##", 2)) {
		if (!strcmp (url, "##TO##"))
			if (!(((EMFormatHTML *) efhd)->header_wrap_flags & EM_FORMAT_HTML_HEADER_TO))
				((EMFormatHTML *) efhd)->header_wrap_flags |= EM_FORMAT_HTML_HEADER_TO;
			else
				((EMFormatHTML *) efhd)->header_wrap_flags &= ~EM_FORMAT_HTML_HEADER_TO;
		else if (!strcmp (url, "##CC##"))
			if (!(((EMFormatHTML *) efhd)->header_wrap_flags & EM_FORMAT_HTML_HEADER_CC))
				((EMFormatHTML *) efhd)->header_wrap_flags |= EM_FORMAT_HTML_HEADER_CC;
			else
				((EMFormatHTML *) efhd)->header_wrap_flags &= ~EM_FORMAT_HTML_HEADER_CC;
		else if (!strcmp (url, "##BCC##")) {
			if (!(((EMFormatHTML *) efhd)->header_wrap_flags & EM_FORMAT_HTML_HEADER_BCC))
				((EMFormatHTML *) efhd)->header_wrap_flags |= EM_FORMAT_HTML_HEADER_BCC;
			else
				((EMFormatHTML *) efhd)->header_wrap_flags &= ~EM_FORMAT_HTML_HEADER_BCC;
		}
		em_format_redraw((EMFormat *)efhd);
	} else
	    g_signal_emit((GObject *)efhd, efhd_signals[EFHD_LINK_CLICKED], 0, url);
}

static void
efhd_html_on_url (GtkHTML *html, const gchar *url, EMFormatHTMLDisplay *efhd)
{
	d(printf("on_url event '%s'\n", url));

	g_signal_emit((GObject *)efhd, efhd_signals[EFHD_ON_URL], 0, url);
}

/* ********************************************************************** */

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

struct _smime_pobject {
	EMFormatHTMLPObject object;

	gint signature;
	CamelCipherValidity *valid;
	GtkWidget *widget;
};

static void
efhd_xpkcs7mime_free(EMFormatHTMLPObject *o)
{
	struct _smime_pobject *po = (struct _smime_pobject *)o;

	if (po->widget)
		gtk_widget_destroy(po->widget);
	camel_cipher_validity_free(po->valid);
}

static void
efhd_xpkcs7mime_info_response(GtkWidget *w, guint button, struct _smime_pobject *po)
{
	gtk_widget_destroy(w);
	po->widget = NULL;
}

#ifdef HAVE_NSS
static void
efhd_xpkcs7mime_viewcert_foad(GtkWidget *w, guint button, struct _smime_pobject *po)
{
	gtk_widget_destroy(w);
}

static void
efhd_xpkcs7mime_viewcert_clicked(GtkWidget *button, struct _smime_pobject *po)
{
	CamelCipherCertInfo *info = g_object_get_data((GObject *)button, "e-cert-info");
	ECertDB *db = e_cert_db_peek();
	ECert *ec = NULL;

	if (info->email)
		ec = e_cert_db_find_cert_by_email_address(db, info->email, NULL);

	if (ec == NULL && info->name)
		ec = e_cert_db_find_cert_by_nickname(db, info->name, NULL);

	if (ec != NULL) {
		GtkWidget *w = certificate_viewer_show(ec);

		/* oddly enough certificate_viewer_show doesn't ... */
		gtk_widget_show(w);
		g_signal_connect(w, "response", G_CALLBACK(efhd_xpkcs7mime_viewcert_foad), po);

		if (w && po->widget)
			gtk_window_set_transient_for ((GtkWindow *)w, (GtkWindow *)po->widget);

		g_object_unref(ec);
	} else {
		g_warning("can't find certificate for %s <%s>", info->name?info->name:"", info->email?info->email:"");
	}
}
#endif

static void
efhd_xpkcs7mime_add_cert_table(GtkWidget *vbox, CamelDList *certlist, struct _smime_pobject *po)
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
			ECertDB *db = e_cert_db_peek();
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

			if (info->email)
				ec = e_cert_db_find_cert_by_email_address(db, info->email, NULL);
			if (ec == NULL && info->name)
				ec = e_cert_db_find_cert_by_nickname(db, info->name, NULL);

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
efhd_xpkcs7mime_validity_clicked(GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	struct _smime_pobject *po = (struct _smime_pobject *)pobject;
	GladeXML *xml;
	GtkWidget *vbox, *w;
	gchar *gladefile;

	if (po->widget)
		/* FIXME: window raise? */
		return;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-dialogs.glade",
				      NULL);
	xml = glade_xml_new(gladefile, "message_security_dialog", NULL);
	g_free (gladefile);

	po->widget = glade_xml_get_widget(xml, "message_security_dialog");

	vbox = glade_xml_get_widget(xml, "signature_vbox");
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

	vbox = glade_xml_get_widget(xml, "encryption_vbox");
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

	g_object_unref(xml);

	g_signal_connect(po->widget, "response", G_CALLBACK(efhd_xpkcs7mime_info_response), po);
	gtk_widget_show(po->widget);
}

static gboolean
efhd_xpkcs7mime_button(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	GtkWidget *icon, *button;
	struct _smime_pobject *po = (struct _smime_pobject *)pobject;
	const gchar *icon_name;

	/* FIXME: need to have it based on encryption and signing too */
	if (po->valid->sign.status != 0)
		icon_name = smime_sign_table[po->valid->sign.status].icon;
	else
		icon_name = smime_encrypt_table[po->valid->encrypt.status].icon;

	icon = gtk_image_new_from_icon_name (
		icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show(icon);

	button = gtk_button_new();
	g_signal_connect(button, "clicked", G_CALLBACK(efhd_xpkcs7mime_validity_clicked), pobject);

	gtk_container_add((GtkContainer *)button, icon);
	gtk_widget_show(button);
	gtk_container_add((GtkContainer *)eb, button);

	return TRUE;
}

static void
efhd_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid)
{
	/* Note: We call EMFormatClass directly, not EMFormatHTML, our parent */
	efhd_format_class->format_secure(emf, stream, part, valid);

	if (emf->valid == valid
	    && (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE
		|| valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)) {
		gchar *classid;
		struct _smime_pobject *pobj;

		camel_stream_printf (stream, "<table border=0 width=\"100%%\" cellpadding=3 cellspacing=0%s><tr>",
				     smime_sign_colour[valid->sign.status]);

		classid = g_strdup_printf("smime:///em-format-html/%s/icon/signed", emf->part_id->str);
		pobj = (struct _smime_pobject *)em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(*pobj), classid, part, efhd_xpkcs7mime_button);
		pobj->valid = camel_cipher_validity_clone(valid);
		pobj->object.free = efhd_xpkcs7mime_free;
		camel_stream_printf(stream, "<td valign=center><object classid=\"%s\"></object></td><td width=100%% valign=center>", classid);
		g_free(classid);
		if (valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
			camel_stream_printf (stream, "%s", _(smime_sign_table[valid->sign.status].shortdesc));
		}

		if (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
			if (valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
				camel_stream_printf (stream, "<br>");
			}
			camel_stream_printf (stream, "%s", _(smime_encrypt_table[valid->encrypt.status].shortdesc));
		}

		camel_stream_printf(stream, "</td></tr></table>");
	}
}

static void
efhd_image(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *handle)
{
	gchar *classid;
	struct _attach_puri *info;

	classid = g_strdup_printf("image%s", ((EMFormat *)efh)->part_id->str);
	info = (struct _attach_puri *)em_format_add_puri((EMFormat *)efh, sizeof(*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject(efh, sizeof(EMFormatHTMLPObject), classid, part, efhd_attachment_image);

	info->handle = handle;
	info->shown = TRUE;
	info->snoop_mime_type = ((EMFormat *) efh)->snoop_mime_type;
	if (camel_operation_cancel_check (NULL) || !info->puri.format || !((EMFormatHTML *)info->puri.format)->html) {
		/* some fake value, we are cancelled anyway, thus doesn't matter */
		info->fit_width = 256;
	} else {
		info->fit_width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;
	}

	camel_stream_printf(stream, "<td><object classid=\"%s\"></object></td>", classid);
	g_free(classid);
}

/* ********************************************************************** */

static EMFormatHandler type_builtin_table[] = {
	{ (gchar *) "image/gif", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/jpeg", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/png", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-png", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/tiff", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-bmp", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/bmp", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/svg", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-cmu-raster", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-ico", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-portable-anymap", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-portable-bitmap", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-portable-graymap", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-portable-pixmap", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/x-xpixmap", (EMFormatFunc)efhd_image },

	/* This is where one adds those busted, non-registered types,
	   that some idiot mailer writers out there decide to pull out
	   of their proverbials at random. */

	{ (gchar *) "image/jpg", (EMFormatFunc)efhd_image },
	{ (gchar *) "image/pjpeg", (EMFormatFunc)efhd_image },

	{ (gchar *) "x-evolution/message/prefix", (EMFormatFunc)efhd_message_prefix },
	{ (gchar *) "x-evolution/message/post-header", (EMFormatFunc)efhd_message_add_bar }
};

static void
efhd_builtin_init(EMFormatHTMLDisplayClass *efhc)
{
	gint i;

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}

static const EMFormatHandler *
efhd_find_handler(EMFormat *emf, const gchar *mime_type)
{
	return ((EMFormatClass *) efhd_parent)->find_handler (emf, mime_type);
}

static void efhd_format_clone(EMFormat *emf, CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, EMFormat *src)
{
	if (emf != src)
		((EMFormatHTML *) emf)->header_wrap_flags = 0;

	((EMFormatClass *)efhd_parent)->format_clone(emf, folder, uid, msg, src);
}

static void
efhd_write_image(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)puri->part);

	/* TODO: identical to efh_write_image */
	d(printf("writing image '%s'\n", puri->cid));
	camel_data_wrapper_decode_to_stream(dw, stream);
	camel_stream_close(stream);
}

static void efhd_message_prefix(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
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
	iconpath = e_icon_factory_get_icon_filename (comp && comp[0] ? "stock_flag-for-followup-done" : "stock_flag-for-followup", GTK_ICON_SIZE_MENU);
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
			camel_object_unref(iconpart);
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

/* TODO: if these aren't going to do anything should remove */
static void efhd_format_error(EMFormat *emf, CamelStream *stream, const gchar *txt)
{
	((EMFormatClass *)efhd_parent)->format_error(emf, stream, txt);
}

static void efhd_format_source(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	((EMFormatClass *)efhd_parent)->format_source(emf, stream, part);
}

/* ********************************************************************** */

/* Checks on the widget whether it can be processed, based on the state of EMFormatHTML.
   The widget should have set "efh" data as the EMFormatHTML instance. */
static gboolean
efhd_can_process_attachment (GtkWidget *button)
{
	EMFormatHTML *efh;

	if (!button)
		return FALSE;

	efh = g_object_get_data (G_OBJECT (button), "efh");

	return efh && efh->state != EM_FORMAT_HTML_STATE_RENDERING;
}

/* if it hasn't been processed yet, format the attachment */
static void
efhd_attachment_show(EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _attach_puri *info = data;

	d(printf("show attachment button called %p\n", info));

	info->shown = ~info->shown;
	em_format_set_inline(info->puri.format, info->puri.part_id, info->shown);
}

static void
efhd_attachment_button_expanded (GtkWidget *widget,
                                 GParamSpec *pspec,
                                 struct _attach_puri *info)
{
	if (!efhd_can_process_attachment (widget))
		return;

	efhd_attachment_show (NULL, NULL, info);
}

static void
efhd_image_fit(EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _attach_puri *info = data;

	info->fit_width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;
	gtk_image_set_from_pixbuf(info->image, em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height));
}

static void
efhd_image_unfit(EPopup *ep, EPopupItem *item, gpointer data)
{
	struct _attach_puri *info = data;

	info->fit_width = 0;
	gtk_image_set_from_pixbuf((GtkImage *)info->image, em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height));
}

static EPopupItem efhd_menu_items[] = {
	{ E_POPUP_BAR, (gchar *) "05.display" },
	{ E_POPUP_ITEM, (gchar *) "05.display.00", (gchar *) N_("_View Inline"), efhd_attachment_show },
	{ E_POPUP_ITEM, (gchar *) "05.display.00", (gchar *) N_("_Hide"), efhd_attachment_show },
	{ E_POPUP_ITEM, (gchar *) "05.display.01", (gchar *) N_("_Fit to Width"), efhd_image_fit, NULL, NULL, EM_POPUP_PART_IMAGE },
	{ E_POPUP_ITEM, (gchar *) "05.display.01", (gchar *) N_("Show _Original Size"), efhd_image_unfit, NULL, NULL, EM_POPUP_PART_IMAGE },
};

static void
efhd_menu_items_free(EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free(items);
}

static void
efhd_popup_place_widget(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
	GtkWidget *w = user_data;

	gdk_window_get_origin(gtk_widget_get_parent_window(w), x, y);
	*x += w->allocation.x + w->allocation.width;
	*y += w->allocation.y;
}

static gboolean
efhd_attachment_popup(GtkWidget *w, GdkEventButton *event, struct _attach_puri *info)
{
	GtkMenu *menu;
	GSList *menus = NULL;
	EMPopup *emp;
	EMPopupTargetPart *target;

	d(printf("attachment popup, button %d\n", event->button));

	if (event && event->button != 1 && event->button != 3) {
		/* ?? gtk_propagate_event(GTK_WIDGET (user_data), (GdkEvent *)event);*/
		return FALSE;
	}

	if (!efhd_can_process_attachment (w))
		return FALSE;

	/** @HookPoint-EMPopup: Attachment Button Context Menu
	 * @Id: org.gnome.evolution.mail.formathtmldisplay.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: EMPopupTargetPart
	 *
	 * This is the drop-down menu shown when a user clicks on the down arrow
	 * of the attachment button in inline mail content.
	 */
	emp = em_popup_new("org.gnome.evolution.mail.formathtmldisplay.popup");
	target = em_popup_target_new_part(emp, info->puri.part, info->handle?info->handle->mime_type:NULL);
	target->target.widget = w;

	/* add our local menus */
	if (info->handle) {
		/* show/hide menus, only if we have an inline handler */
		menus = g_slist_prepend(menus, &efhd_menu_items[0]);
		menus = g_slist_prepend(menus, &efhd_menu_items[info->shown?2:1]);
		if (info->shown && info->image) {
			if (info->fit_width != 0) {
				if (em_icon_stream_is_resized(info->puri.cid, info->fit_width, info->fit_height))
				    menus = g_slist_prepend(menus, &efhd_menu_items[4]);
			} else
				menus = g_slist_prepend(menus, &efhd_menu_items[3]);
		}
	}

	e_popup_add_items((EPopup *)emp, menus, NULL, efhd_menu_items_free, info);

	menu = e_popup_create_menu_once((EPopup *)emp, (EPopupTarget *)target, 0);
	if (event)
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
	else
		gtk_menu_popup(menu, NULL, NULL, (GtkMenuPositionFunc)efhd_popup_place_widget, w, 0, gtk_get_current_event_time());

	return TRUE;
}

static gboolean
efhd_image_popup(GtkWidget *w, GdkEventButton *event, struct _attach_puri *info)
{
	if (event && event->button != 3)
		return FALSE;

	return efhd_attachment_popup(w, event, info);
}

static gboolean
efhd_attachment_popup_menu(GtkWidget *w, struct _attach_puri *info)
{
	return efhd_attachment_popup(w, NULL, info);
}

/* ********************************************************************** */

static void
efhd_drag_data_get(GtkWidget *w, GdkDragContext *drag, GtkSelectionData *data, guint info, guint time, EMFormatHTMLPObject *pobject)
{
	CamelMimePart *part = pobject->part;
	gchar *uri, *uri_crlf, *path;
	CamelStream *stream;

	switch (info) {
	case 0: /* mime/type request */
		stream = camel_stream_mem_new();
		/* TODO: shoudl format_format_text run on the content-object? */
		/* TODO: should we just do format_content? */
		if (camel_content_type_is (((CamelDataWrapper *)part)->mime_type, "text", "*")) {
			/* FIXME: this should be an em_utils method, it only needs a default charset param */
			em_format_format_text((EMFormat *)pobject->format, stream, (CamelDataWrapper *)part);
		} else {
			CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);

			camel_data_wrapper_decode_to_stream(dw, stream);
		}

		gtk_selection_data_set(data, data->target, 8,
				       ((CamelStreamMem *)stream)->buffer->data,
				       ((CamelStreamMem *)stream)->buffer->len);
		camel_object_unref(stream);
		break;
	case 1: /* text-uri-list request */
		/* Kludge around Nautilus requesting the same data many times */
		uri = g_object_get_data((GObject *)w, "e-drag-uri");
		if (uri) {
			gtk_selection_data_set(data, data->target, 8, (guchar *)uri, strlen(uri));
			return;
		}

		path = em_utils_temp_save_part(w, part, FALSE);
		if (path == NULL)
			return;

		uri = g_filename_to_uri(path, NULL, NULL);
		g_free(path);
		uri_crlf = g_strconcat(uri, "\r\n", NULL);
		g_free(uri);
		gtk_selection_data_set(data, data->target, 8, (guchar *)uri_crlf, strlen(uri_crlf));
		g_object_set_data_full((GObject *)w, "e-drag-uri", uri_crlf, g_free);
		break;
	default:
		abort();
	}
}

static void
efhd_drag_data_delete(GtkWidget *w, GdkDragContext *drag, EMFormatHTMLPObject *pobject)
{
	gchar *uri;

	uri = g_object_get_data((GObject *)w, "e-drag-uri");
	if (uri) {
		/* NB: this doesn't kill the dnd directory */
		/* NB: is this ever called? */
		/* NB even more: doesn't the e-drag-uri have \r\n
		 * appended? (see efhd_drag_data_get())
		 */
		gchar *filename = g_filename_from_uri (uri, NULL, NULL);
		g_unlink(filename);
		g_free(filename);
		g_object_set_data((GObject *)w, "e-drag-uri", NULL);
	}
}

static void
efhd_write_icon_job(struct _EMFormatHTMLJob *job, gint cancelled)
{
	EMFormatHTMLPObject *pobject;
	CamelDataWrapper *dw;

	if (cancelled)
		return;

	pobject = job->u.data;
	dw = camel_medium_get_content_object((CamelMedium *)pobject->part);
	camel_data_wrapper_decode_to_stream(dw, job->stream);
	camel_stream_close(job->stream);
}

static void
efhd_image_resized(GtkWidget *w, GtkAllocation *event, struct _attach_puri *info)
{
	GdkPixbuf *pb;
	gint width;

	if (info->fit_width == 0)
		return;

	width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;
	if (info->fit_width == width)
		return;
	info->fit_width = width;

	pb = em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height);
	if (pb) {
		gtk_image_set_from_pixbuf(info->image, pb);
		g_object_unref(pb);
	}
}

static void
efhd_change_cursor(GtkWidget *w, GdkEventCrossing *event, struct _attach_puri *info)
{
	if (info->shown && info->image) {
		if (info->fit_width != 0) {
			if (em_icon_stream_is_resized(info->puri.cid, info->fit_width, info->fit_height))
				e_cursor_set(w->window, E_CURSOR_ZOOM_IN);

		}
	}
}

static void
efhd_image_fit_width(GtkWidget *widget, GdkEventButton *event, struct _attach_puri *info)
{
	gint width;

	width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;

	if (info->shown && info->image) {
		if (info->fit_width != 0) {
			if (em_icon_stream_is_resized(info->puri.cid, info->fit_width, info->fit_height)) {
				if (info->fit_width != width) {
					info->fit_width = width;
					e_cursor_set (widget->window, E_CURSOR_ZOOM_IN);
				} else {
					info->fit_width = 0;
					e_cursor_set(widget->window, E_CURSOR_ZOOM_OUT);
				}
			}
		} else {
			info->fit_width = width;
			e_cursor_set (widget->window, E_CURSOR_ZOOM_IN);
		}
	}

	gtk_image_set_from_pixbuf(info->image, em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height));
}

/* When the puri gets freed in the formatter thread and if the image is resized, crash will happen
   See bug #333864 So while freeing the puri, we disconnect the image attach resize attached with
   the puri */

static void
efhd_image_unallocate (struct _EMFormatPURI * puri)
{
	struct _attach_puri *info = (struct _attach_puri *) puri;
	g_signal_handlers_disconnect_by_func(info->html, efhd_image_resized, info);

	g_signal_handlers_disconnect_by_func(info->event_box, efhd_image_popup, info);
	g_signal_handlers_disconnect_by_func(info->event_box, efhd_change_cursor, info);
	g_signal_handlers_disconnect_by_func(info->event_box, efhd_attachment_popup_menu, info);
	g_signal_handlers_disconnect_by_func(info->event_box, efhd_image_fit_width, info);
}

static gboolean
efhd_attachment_image(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	GtkWidget *box;
	EMFormatHTMLJob *job;
	struct _attach_puri *info;
	GdkPixbuf *pixbuf;
	GtkTargetEntry drag_types[] = {
		{ NULL, 0, 0 },
		{ (gchar *) "text/uri-list", 0, 1 },
	};
	gchar *simple_type;

	info = (struct _attach_puri *)em_format_find_puri((EMFormat *)efh, pobject->classid);

	info->image = (GtkImage *)gtk_image_new();
	info->html = eb;
	info->puri.free = efhd_image_unallocate;

	pixbuf = em_icon_stream_get_image(pobject->classid, info->fit_width, info->fit_height);
	if (pixbuf) {
		gtk_image_set_from_pixbuf(info->image, pixbuf);
		g_object_unref(pixbuf);
	} else {
		job = em_format_html_job_new(efh, efhd_write_icon_job, pobject);
		job->stream = (CamelStream *)em_icon_stream_new((GtkImage *)info->image, pobject->classid, info->fit_width, info->fit_height, TRUE);
		em_format_html_job_queue(efh, job);
	}

	box = gtk_event_box_new();
	info->event_box = box;
	gtk_container_add((GtkContainer *)box, (GtkWidget *)info->image);
	gtk_widget_show_all(box);
	gtk_container_add((GtkContainer *)eb, box);

	g_signal_connect(eb, "size_allocate", G_CALLBACK(efhd_image_resized), info);

	simple_type = camel_content_type_simple(((CamelDataWrapper *)pobject->part)->mime_type);
	camel_strdown(simple_type);

	drag_types[0].target = simple_type;
	gtk_drag_source_set(box, GDK_BUTTON1_MASK, drag_types, sizeof(drag_types)/sizeof(drag_types[0]), GDK_ACTION_COPY);
	g_free(simple_type);

	g_signal_connect(box, "drag-data-get", G_CALLBACK(efhd_drag_data_get), pobject);
	g_signal_connect (box, "drag-data-delete", G_CALLBACK(efhd_drag_data_delete), pobject);

	g_signal_connect(box, "button_press_event", G_CALLBACK(efhd_image_popup), info);
	g_signal_connect(box, "enter-notify-event", G_CALLBACK(efhd_change_cursor), info);
	g_signal_connect(box, "popup_menu", G_CALLBACK(efhd_attachment_popup_menu), info);
	g_signal_connect(box, "button-press-event", G_CALLBACK(efhd_image_fit_width), info);

	g_object_set_data (G_OBJECT (box), "efh", efh);

	return TRUE;
}

/* attachment button callback */
static gboolean
efhd_attachment_button(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)efh;
	struct _attach_puri *info;
	EAttachmentView *view;
	EAttachmentStore *store;
	EAttachment *attachment;
	GtkWidget *widget;
	gpointer parent;

	/* FIXME: handle default shown case */
	d(printf("adding attachment button/content\n"));

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

	parent = gtk_widget_get_toplevel (GTK_WIDGET (efh->html));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	view = E_ATTACHMENT_VIEW (efhd->priv->attachment_view);
	gtk_widget_show (efhd->priv->attachment_view);

	store = e_attachment_view_get_store (view);
	e_attachment_store_add_attachment (store, info->attachment);

	e_attachment_load_async (
		info->attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, parent);

	widget = e_attachment_button_new (view);
	e_attachment_button_set_attachment (
		E_ATTACHMENT_BUTTON (widget), attachment);
	gtk_container_add (GTK_CONTAINER (eb), widget);
	gtk_widget_show (widget);

	g_object_set_data (G_OBJECT (widget), "efh", efh);

	g_signal_connect (
		widget, "notify::expanded",
		G_CALLBACK (efhd_attachment_button_expanded), info);

	return TRUE;
}

/* not used currently */
/* frame source callback */
static void
efhd_attachment_frame(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	struct _attach_puri *info = (struct _attach_puri *)puri;

	if (info->shown) {
		d(printf("writing to frame content, handler is '%s'\n", info->handle->mime_type));
		info->handle->handler(emf, stream, info->puri.part, info->handle);
		camel_stream_close(stream);
	} else {
		/* FIXME: this is leaked if the object is closed without showing it
		   NB: need a virtual puri_free method? */
		info->output = stream;
		camel_object_ref(stream);
	}
}

static void
efhd_bar_resize (EMFormatHTML *efh,
                 GtkAllocation *event)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) efh;
	GtkWidget *widget;
	gint width;

	widget = GTK_WIDGET (efh->html);
	width = widget->allocation.width - 12;

	if (width > 0) {
		widget = efhd->priv->attachment_view;
		gtk_widget_set_size_request (widget, width, -1);
	}
}

static gboolean
efhd_add_bar (EMFormatHTML *efh,
              GtkHTMLEmbedded *eb,
              EMFormatHTMLPObject *pobject)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) efh;
	GtkWidget *widget;

	widget = e_mail_attachment_bar_new ();
	gtk_container_add (GTK_CONTAINER (eb), widget);
	efhd->priv->attachment_view = widget;
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

	em_format_html_add_pobject (
		(EMFormatHTML *) emf,
		sizeof (EMFormatHTMLPObject),
		classid, part, efhd_add_bar);

	camel_stream_printf (
		stream, "<td><object classid=\"%s\"></object></td>", classid);
}

static void
efhd_format_attachment(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const gchar *mime_type, const EMFormatHandler *handle)
{
	gchar *classid, *text, *html;
	struct _attach_puri *info;

	classid = g_strdup_printf ("attachment%s", emf->part_id->str);
	info = (struct _attach_puri *)em_format_add_puri (
		emf, sizeof (*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject (
		(EMFormatHTML *) emf, sizeof (EMFormatHTMLPObject),
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
		"<tr><td></td></tr></table></td>");

	camel_stream_printf (
		stream, "<td><object classid=\"%s\"></object></td>", classid);

	camel_stream_write_string (
		stream, "<td><table width=3 cellspacing=0 cellpadding=0>"
		"<tr><td></td></tr></table></td><td><font size=-1>");

	/* output some info about it */
	/* FIXME: should we look up mime_type from object again? */
	text = em_format_describe_part (part, mime_type);
	html = camel_text_to_html (
		text, ((EMFormatHTML *)emf)->text_html_flags &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string (stream, html);
	g_free (html);
	g_free (text);

	camel_stream_write_string (
		stream, "</font></td></tr><tr></table>\n"
		EM_FORMAT_HTML_VPAD);

	if (handle && info->shown)
		handle->handler(emf, stream, part, handle);

	g_free(classid);
}

static void
efhd_optional_button_show (GtkWidget *widget, GtkWidget *w)
{
	GtkWidget *label = g_object_get_data (G_OBJECT (widget), "text-label");

	if (GTK_WIDGET_VISIBLE (w)) {
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
	gtk_widget_set_size_request (w, ((GtkWidget *)efh->html)->allocation.width-48, 250);
}

/* optional render attachment button callback */
static gboolean
efhd_attachment_optional(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	struct _attach_puri *info;
	GtkWidget *hbox, *vbox, *button, *mainbox, *scroll, *label, *img;
	AtkObject *a11y;
	GtkWidget *view;
	GtkTextBuffer *buffer;

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
		GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
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

	g_signal_connect(button, "button_press_event", G_CALLBACK(efhd_attachment_popup), info);
	g_signal_connect(button, "popup_menu", G_CALLBACK(efhd_attachment_popup_menu), info);
	g_signal_connect(button, "clicked", G_CALLBACK(efhd_attachment_popup_menu), info);
	gtk_box_pack_start(GTK_BOX (mainbox), button, FALSE, FALSE, 6);

	gtk_widget_show_all(mainbox);

	gtk_box_pack_start(GTK_BOX (vbox), mainbox, FALSE, FALSE, 6);

	view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (view));
	gtk_text_buffer_set_text (buffer, (gchar *)info->mstream->buffer->data, info->mstream->buffer->len);
	camel_object_unref(info->mstream);
	info->mstream = NULL;
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (view));
	gtk_box_pack_start(GTK_BOX (vbox), scroll, TRUE, TRUE, 6);
	gtk_widget_show (GTK_WIDGET(view));

	gtk_widget_set_size_request (scroll, (GTK_WIDGET (efh->html))->allocation.width - 48, 250);
	g_signal_connect (scroll, "size_allocate", G_CALLBACK(efhd_resize), efh);
	gtk_widget_show (scroll);

	if (!info->shown)
		gtk_widget_hide (scroll);

	gtk_widget_show (vbox);
	gtk_container_add(GTK_CONTAINER (eb), vbox);
	info->handle = NULL;

	return TRUE;
}

static void
efhd_format_optional(EMFormat *emf, CamelStream *fstream, CamelMimePart *part, CamelStream *mstream)
{
	gchar *classid, *html;
	struct _attach_puri *info;
	CamelStream *stream;

	if (CAMEL_IS_STREAM_FILTER (fstream) && ((CamelStreamFilter *) fstream)->source)
		stream = ((CamelStreamFilter *) fstream)->source;
	else
		stream = fstream;

	classid = g_strdup_printf("optional%s", emf->part_id->str);
	info = (struct _attach_puri *)em_format_add_puri(emf, sizeof(*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(EMFormatHTMLPObject), classid, part, efhd_attachment_optional);
	info->handle = em_format_find_handler(emf, "text/plain");
	info->shown = FALSE;
	info->snoop_mime_type = "text/plain";
	info->attachment = e_attachment_new ();
	e_attachment_set_mime_part (info->attachment, info->puri.part);
	info->mstream = (CamelStreamMem *)mstream;
	if (emf->valid) {
		info->sign = emf->valid->sign.status;
		info->encrypt = emf->valid->encrypt.status;
	}

	camel_stream_write_string(stream,
				  EM_FORMAT_HTML_VPAD
				  "<table cellspacing=0 cellpadding=0><tr><td><h3><font size=-1 color=red>");

	html = camel_text_to_html(_("Evolution cannot render this email as it is too large to process. You can view it unformatted or with an external text editor."), ((EMFormatHTML *)emf)->text_html_flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string(stream, html);
	camel_stream_write_string(stream,
				  "</font></h3></td></tr></table>\n");
	camel_stream_write_string(stream,
				  "<table cellspacing=0 cellpadding=0>"
				  "<tr>");
	camel_stream_printf(stream, "<td><object classid=\"%s\"></object></td></tr></table>", classid);

	g_free(html);

	camel_stream_write_string(stream,
/*				  "</font></h2></td></tr></table>\n" */
				  EM_FORMAT_HTML_VPAD);

	g_free(classid);
}
