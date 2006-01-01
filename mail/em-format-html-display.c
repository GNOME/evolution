/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/gtkhtml-search.h>

#include <gtk/gtkeventbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktable.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtktoolbutton.h>
#include <gtk/gtkframe.h>

#include <glade/glade.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnome/gnome-i18n.h>

#include <bonobo/bonobo-control-frame.h>
#include <bonobo/bonobo-stream-memory.h>
#include <bonobo/bonobo-widget.h>

#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-multipart.h>
#include <camel/camel-internet-address.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-cipher-context.h>
#include <camel/camel-folder.h>
#include <camel/camel-string-utils.h>

#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

#include <libedataserver/e-msgport.h>
#include <e-util/e-gui-utils.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#ifdef HAVE_NSS
#include "certificate-viewer.h"
#include "e-cert-db.h"
#endif

#include "mail-config.h"

#include "em-format-html-display.h"
#include "em-marshal.h"
#include "e-searching-tokenizer.h"
#include "em-icon-stream.h"
#include "em-utils.h"
#include "em-popup.h"
#include "e-attachment.h"
#include "e-attachment-bar.h"

#ifdef G_OS_WIN32
/* Undefine the similar macro from <pthread.h>,it doesn't check if
 * localtime() returns NULL.
 */
#undef localtime_r

/* The localtime() in Microsoft's C library is MT-safe */
#define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

#define d(x)

#define EFHD_TABLE_OPEN "<table>"

struct _EMFormatHTMLDisplayPrivate {
	/* For the interactive search dialogue */
	/* TODO: Should this be more subtle, like the mozilla one? */
	GtkDialog *search_dialog;
	GtkWidget *search_entry;
	GtkWidget *search_matches_label;
	GtkWidget *search_case_check;
	char *search_text;
	int search_wrap;	/* are we doing a wrap search */

	/* for Attachment bar */
	GtkWidget *attachment_bar;
	GtkWidget *attachment_box;
	GtkWidget *label;
	GtkWidget *arrow;
	GtkWidget *forward;
	GtkWidget *down;
	GtkWidget *attachment_area;
	gboolean  show_bar;
	GHashTable *files;
};

static int efhd_html_button_press_event (GtkWidget *widget, GdkEventButton *event, EMFormatHTMLDisplay *efh);
static void efhd_html_link_clicked (GtkHTML *html, const char *url, EMFormatHTMLDisplay *efhd);
static void efhd_html_on_url (GtkHTML *html, const char *url, EMFormatHTMLDisplay *efhd);

static void efhd_attachment_frame(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri);
static gboolean efhd_attachment_image(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject);
static void efhd_message_add_bar(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info);
static void efhd_attachment_bar_refresh (EMFormatHTMLDisplay *efhd);

struct _attach_puri {
	EMFormatPURI puri;

	const EMFormatHandler *handle;

	const char *snoop_mime_type;

	/* for the > and V buttons */
	GtkWidget *forward, *down;
	/* currently no way to correlate this data to the frame :( */
	GtkHTML *frame;
	CamelStream *output;
	unsigned int shown:1;

	/* image stuff */
	int fit_width;
	int fit_height;
	GtkImage *image;

	/* Signed / Encrypted */
        camel_cipher_validity_sign_t sign;
        camel_cipher_validity_encrypt_t encrypt;
};


static void efhd_iframe_created(GtkHTML *html, GtkHTML *iframe, EMFormatHTMLDisplay *efh);
/*static void efhd_url_requested(GtkHTML *html, const char *url, GtkHTMLStream *handle, EMFormatHTMLDisplay *efh);
  static gboolean efhd_object_requested(GtkHTML *html, GtkHTMLEmbedded *eb, EMFormatHTMLDisplay *efh);*/

static void efhd_message_prefix(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info);

static const EMFormatHandler *efhd_find_handler(EMFormat *emf, const char *mime_type);
static void efhd_format_clone(EMFormat *, CamelFolder *folder, const char *, CamelMimeMessage *msg, EMFormat *);
static void efhd_format_error(EMFormat *emf, CamelStream *stream, const char *txt);
static void efhd_format_source(EMFormat *, CamelStream *, CamelMimePart *);
static void efhd_format_attachment(EMFormat *, CamelStream *, CamelMimePart *, const char *, const EMFormatHandler *);
static void efhd_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid);
static void efhd_complete(EMFormat *);

static gboolean efhd_bonobo_object(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject);
static gboolean efhd_use_component(const char *mime_type);

static void efhd_builtin_init(EMFormatHTMLDisplayClass *efhc);

enum {
	EFHD_LINK_CLICKED,
	EFHD_POPUP_EVENT,
	EFHD_ON_URL,
	EFHD_LAST_SIGNAL,
};

static guint efhd_signals[EFHD_LAST_SIGNAL] = { 0 };

/* EMFormatHandler's for bonobo objects */
static GHashTable *efhd_bonobo_handlers;
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
		int state = GTK_WIDGET_STATE(html);
		gushort r, g, b;
#define SCALE (238)

		/* choose a suitably darker or lighter colour */
		r = style->base[state].red >> 8;
		g = style->base[state].green >> 8;
		b = style->base[state].blue >> 8;

		if (r+b+g > 128*3) {
			r = (r*SCALE) >> 8;
			g = (g*SCALE) >> 8;
			b = (b*SCALE) >> 8;
		} else {
			r = 128 - ((SCALE * r) >> 9);
			g = 128 - ((SCALE * g) >> 9);
			b = 128 - ((SCALE * b) >> 9);
		}

		efhd->formathtml.body_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;
		
#undef SCALE
#define SCALE (174)
		/* choose a suitably darker or lighter colour */
		r = style->base[state].red >> 8;
		g = style->base[state].green >> 8;
		b = style->base[state].blue >> 8;

		if (r+b+g > 128*3) {
			r = (r*SCALE) >> 8;
			g = (g*SCALE) >> 8;
			b = (b*SCALE) >> 8;
		} else {
			r = 128 - ((SCALE * r) >> 9);
			g = 128 - ((SCALE * g) >> 9);
			b = 128 - ((SCALE * b) >> 9);
		}

		efhd->formathtml.frame_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;

		r = style->base[GTK_STATE_NORMAL].red >> 8;
		g = style->base[GTK_STATE_NORMAL].green >> 8;
		b = style->base[GTK_STATE_NORMAL].blue >> 8;

		efhd->formathtml.content_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;

		r = style->text[state].red >> 8;
		g = style->text[state].green >> 8;
		b = style->text[state].blue >> 8;

		efhd->formathtml.text_colour = ((r<<16) | (g<< 8) | b) & 0xffffff;
	}
#undef SCALE
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

	efhd->search_tok = (ESearchingTokenizer *)e_searching_tokenizer_new();
	gtk_html_set_tokenizer (efh->html, (HTMLTokenizer *)efhd->search_tok);

	g_signal_connect(efh->html, "realize", G_CALLBACK(efhd_gtkhtml_realise), o);
	g_signal_connect(efh->html, "style-set", G_CALLBACK(efhd_gtkhtml_style_set), o);
	/* we want to convert url's etc */
	efh->text_html_flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
#undef efh

	efhd->nobar = getenv("EVOLUTION_NO_BAR") != NULL;

	efhd->priv->show_bar = FALSE;
	efhd->priv->files = NULL;
}

static void
efhd_finalise(GObject *o)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)o;

	/* check pending stuff */

	if (efhd->priv->files)
		g_hash_table_destroy(efhd->priv->files);

	g_free(efhd->priv->search_text);
	g_free(efhd->priv);

	((GObjectClass *)efhd_parent)->finalize(o);
}

static gboolean
efhd_bool_accumulator(GSignalInvocationHint *ihint, GValue *out, const GValue *in, void *data)
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
	((EMFormatClass *)klass)->format_secure = efhd_format_secure;
	((EMFormatClass *)klass)->complete = efhd_complete;

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
			     em_marshal_BOOLEAN__BOXED_POINTER_POINTER,
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
em_format_html_display_get_type(void)
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

		efhd_bonobo_handlers = g_hash_table_new(g_str_hash, g_str_equal);
	}

	return type;
}

EMFormatHTMLDisplay *em_format_html_display_new(void)
{
	EMFormatHTMLDisplay *efhd;

	efhd = g_object_new(em_format_html_display_get_type(), 0);

	g_signal_connect(efhd->formathtml.html, "iframe_created", G_CALLBACK(efhd_iframe_created), efhd);
	g_signal_connect(efhd->formathtml.html, "link_clicked", G_CALLBACK(efhd_html_link_clicked), efhd);
	g_signal_connect(efhd->formathtml.html, "on_url", G_CALLBACK(efhd_html_on_url), efhd);
	g_signal_connect(efhd->formathtml.html, "button_press_event", G_CALLBACK(efhd_html_button_press_event), efhd);

	return efhd;
}

void em_format_html_display_goto_anchor(EMFormatHTMLDisplay *efhd, const char *name)
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

EAttachmentBar *
em_format_html_display_get_bar (EMFormatHTMLDisplay *efhd)
{
	return efhd->priv->attachment_bar;
}

void
em_format_html_display_set_search(EMFormatHTMLDisplay *efhd, int type, GSList *strings)
{
	switch(type&3) {
	case EM_FORMAT_HTML_DISPLAY_SEARCH_PRIMARY:
		e_searching_tokenizer_set_primary_case_sensitivity(efhd->search_tok, (type&EM_FORMAT_HTML_DISPLAY_SEARCH_ICASE) == 0);
		e_searching_tokenizer_set_primary_search_string(efhd->search_tok, NULL);
		while (strings) {
			e_searching_tokenizer_add_primary_search_string(efhd->search_tok, strings->data);
			strings = strings->next;
		}
		break;
	case EM_FORMAT_HTML_DISPLAY_SEARCH_SECONDARY:
	default:
		e_searching_tokenizer_set_secondary_case_sensitivity(efhd->search_tok, (type&EM_FORMAT_HTML_DISPLAY_SEARCH_ICASE) == 0);
		e_searching_tokenizer_set_secondary_search_string(efhd->search_tok, NULL);
		while (strings) {
			e_searching_tokenizer_add_secondary_search_string(efhd->search_tok, strings->data);
			strings = strings->next;
		}
		break;
	}

	d(printf("redrawing with search\n"));
	em_format_redraw((EMFormat *)efhd);
}

static void
efhd_update_matches(EMFormatHTMLDisplay *efhd)
{
	struct _EMFormatHTMLDisplayPrivate *p = efhd->priv;
	char *str;
	/* message-search popup match count string */
	char *fmt = _("Matches: %d");

	if (p->search_dialog) {
		str = alloca(strlen(fmt)+32);
		sprintf(str, fmt, e_searching_tokenizer_match_count(efhd->search_tok));
		gtk_label_set_text((GtkLabel *)p->search_matches_label, str);
	}
}

static void
efhd_update_search(EMFormatHTMLDisplay *efhd)
{
	struct _EMFormatHTMLDisplayPrivate *p = efhd->priv;
	GSList *words = NULL;
	int flags = 0;

	if (!gtk_toggle_button_get_active((GtkToggleButton *)p->search_case_check))
		flags = EM_FORMAT_HTML_DISPLAY_SEARCH_ICASE | EM_FORMAT_HTML_DISPLAY_SEARCH_PRIMARY;
	else
		flags = EM_FORMAT_HTML_DISPLAY_SEARCH_PRIMARY;

	if (p->search_text)
		words = g_slist_append(words, p->search_text);

	em_format_html_display_set_search(efhd, flags, words);
	g_slist_free(words);
}

static void
efhd_search_response(GtkWidget *w, int button, EMFormatHTMLDisplay *efhd)
{
	struct _EMFormatHTMLDisplayPrivate *p = efhd->priv;

	if (button == GTK_RESPONSE_ACCEPT) {
		char *txt = g_strdup(gtk_entry_get_text((GtkEntry *)p->search_entry));

		g_strstrip(txt);
		if (p->search_text && strcmp(p->search_text, txt) == 0 && !p->search_wrap) {
			if (!gtk_html_engine_search_next(((EMFormatHTML *)efhd)->html))
				p->search_wrap = TRUE;
			g_free(txt);
		} else {
			g_free(p->search_text);
			p->search_text = txt;
			if (!p->search_wrap)
				efhd_update_search(efhd);
			p->search_wrap = FALSE;
			gtk_html_engine_search(((EMFormatHTML *)efhd)->html, txt,
					       gtk_toggle_button_get_active((GtkToggleButton *)p->search_case_check),
					       TRUE, FALSE);
		}
	} else {
		g_free(p->search_text);
		p->search_text = NULL;
		gtk_widget_destroy((GtkWidget *)p->search_dialog);
		p->search_dialog = NULL;
		em_format_html_display_set_search(efhd, EM_FORMAT_HTML_DISPLAY_SEARCH_PRIMARY, NULL);
	}
}

static void
efhd_search_case_toggled(GtkWidget *w, EMFormatHTMLDisplay *efhd)
{
	struct _EMFormatHTMLDisplayPrivate *p = efhd->priv;

	g_free(p->search_text);
	p->search_text = NULL;
	efhd_search_response(w, GTK_RESPONSE_ACCEPT, efhd);
}

static void
efhd_search_entry_activate(GtkWidget *w, EMFormatHTMLDisplay *efhd)
{
	efhd_search_response(w, GTK_RESPONSE_ACCEPT, efhd);
}

/**
 * em_format_html_display_search:
 * @efhd: 
 * 
 * Run an interactive search dialogue.
 **/
void
em_format_html_display_search(EMFormatHTMLDisplay *efhd)
{
	struct _EMFormatHTMLDisplayPrivate *p = efhd->priv;
	GladeXML *xml;
	char *gladefile;

	if (p->search_dialog) {
		gdk_window_raise(((GtkWidget *)p->search_dialog)->window);
		return;
	}

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-dialogs.glade",
				      NULL);
	xml = glade_xml_new (gladefile, "search_message_dialog", NULL);
	g_free (gladefile);

	if (xml == NULL) {
		g_warning("Cannot open search dialog glade file");
		/* ?? */
		return;
	}

	/* TODO: The original put the subject in the frame, but it had some
	   ugly arbitrary string-cutting code to make sure it fit. */

	p->search_dialog = (GtkDialog *)glade_xml_get_widget(xml, "search_message_dialog");
	p->search_entry = glade_xml_get_widget(xml, "search_entry");
	p->search_matches_label = glade_xml_get_widget(xml, "search_matches_label");
	p->search_case_check = glade_xml_get_widget(xml, "search_case_check");
	p->search_wrap = FALSE;

	gtk_dialog_set_default_response((GtkDialog *)p->search_dialog, GTK_RESPONSE_ACCEPT);
	e_dialog_set_transient_for ((GtkWindow *) p->search_dialog, (GtkWidget *) ((EMFormatHTML *) efhd)->html);
	gtk_window_set_destroy_with_parent ((GtkWindow *) p->search_dialog, TRUE);
	efhd_update_matches(efhd);

	g_signal_connect(p->search_entry, "activate", G_CALLBACK(efhd_search_entry_activate), efhd);
	g_signal_connect(p->search_case_check, "toggled", G_CALLBACK(efhd_search_case_toggled), efhd);
	g_signal_connect(p->search_dialog, "response", G_CALLBACK(efhd_search_response), efhd);
	gtk_widget_show((GtkWidget *)p->search_dialog);
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

static int
efhd_html_button_press_event (GtkWidget *widget, GdkEventButton *event, EMFormatHTMLDisplay *efhd)
{
	char *uri;
	gboolean res = FALSE;
	EMFormatPURI *puri = NULL;

	if (event->button != 3)
		return FALSE;

	uri = gtk_html_get_url_at (GTK_HTML (widget), event->x, event->y);

	d(printf("popup button pressed\n"));

	if (uri) {
		puri = em_format_find_puri((EMFormat *)efhd, uri);
		d(printf("poup event, uri = '%s' part = '%p'\n", uri, puri?puri->part:NULL));
	}

	g_signal_emit((GtkObject *)efhd, efhd_signals[EFHD_POPUP_EVENT], 0, event, uri, puri?puri->part:NULL, &res);

	g_free(uri);

	return res;
}

gboolean 
em_format_html_display_popup_menu (EMFormatHTMLDisplay *efhd)
{
	GtkHTML *html;
	char *url;
	gboolean res = FALSE;
	EMFormatPURI *puri = NULL;

	html = efhd->formathtml.html;

	url = gtk_html_get_cursor_url (html);

	if (url)
		puri = em_format_find_puri((EMFormat *)efhd, url);

	g_signal_emit((GtkObject *)efhd, efhd_signals[EFHD_POPUP_EVENT], 0, NULL, url, puri?puri->part:NULL, &res);

	g_free(url);

	return res;
}

static void
efhd_html_link_clicked (GtkHTML *html, const char *url, EMFormatHTMLDisplay *efhd)
{
	d(printf("link clicked event '%s'\n", url));
	g_signal_emit((GObject *)efhd, efhd_signals[EFHD_LINK_CLICKED], 0, url);
}

static void
efhd_html_on_url (GtkHTML *html, const char *url, EMFormatHTMLDisplay *efhd)
{
	d(printf("on_url event '%s'\n", url));
	g_signal_emit((GObject *)efhd, efhd_signals[EFHD_ON_URL], 0, url);
}

static void
efhd_complete(EMFormat *emf)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)emf;

	if (efhd->priv->search_dialog)
		efhd_update_matches(efhd);

	if (efhd->priv->files) {
		g_hash_table_destroy (efhd->priv->files);
		efhd->priv->files = NULL;
	}
}

/* ********************************************************************** */

/* TODO: move the dialogue elsehwere */
/* FIXME: also in em-format-html.c */
static const struct {
	const char *icon, *shortdesc, *description;
} smime_sign_table[4] = {
	{ "stock_signature-bad", N_("Unsigned"), N_("This message is not signed. There is no guarantee that this message is authentic.") },
	{ "stock_signature-ok", N_("Valid signature"), N_("This message is signed and is valid meaning that it is very likely that this message is authentic.") },
	{ "stock_signature-bad", N_("Invalid signature"), N_("The signature of this message cannot be verified, it may have been altered in transit.") },
	{ "stock_signature", N_("Valid signature, cannot verify sender"), N_("This message is signed with a valid signature, but the sender of the message cannot be verified.") },
};

static const struct {
	const char *icon, *shortdesc, *description;
} smime_encrypt_table[4] = {
	{ "stock_lock-broken", N_("Unencrypted"), N_("This message is not encrypted. Its content may be viewed in transit across the Internet.") },
	{ "stock_lock-ok", N_("Encrypted, weak"), N_("This message is encrypted, but with a weak encryption algorithm. It would be difficult, but not impossible for an outsider to view the content of this message in a practical amount of time.") },
	{ "stock_lock-ok", N_("Encrypted"), N_("This message is encrypted.  It would be difficult for an outsider to view the content of this message.") },
	{ "stock_lock-ok", N_("Encrypted, strong"), N_("This message is encrypted, with a strong encryption algorithm. It would be very difficult for an outsider to view the content of this message in a practical amount of time.") },
};

static const char *smime_sign_colour[4] = {
	"", " bgcolor=\"#88bb88\"", " bgcolor=\"#bb8888\"", " bgcolor=\"#e8d122\""
};

struct _smime_pobject {
	EMFormatHTMLPObject object;

	int signature;
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
			gtk_window_set_transient_for((GtkWindow *)w, (GtkWindow *)po->widget);

		g_object_unref(ec);
	} else {
		g_warning("can't find certificate for %s <%s>", info->name?info->name:"", info->email?info->email:"");
	}
}
#endif

static void
efhd_xpkcs7mime_add_cert_table(GtkWidget *vbox, EDList *certlist, struct _smime_pobject *po)
{
	CamelCipherCertInfo *info = (CamelCipherCertInfo *)certlist->head;
	GtkTable *table = (GtkTable *)gtk_table_new(e_dlist_length(certlist), 2, FALSE);
	int n = 0;

	while (info->next) {
		char *la = NULL;
		const char *l = NULL;

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
	char *gladefile;

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
		w = gtk_label_new(po->valid->sign.description);
		gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.5);
		gtk_label_set_line_wrap((GtkLabel *)w, FALSE);
		gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	}

	if (!e_dlist_empty(&po->valid->sign.signers))
		efhd_xpkcs7mime_add_cert_table(vbox, &po->valid->sign.signers, po);

	gtk_widget_show_all(vbox);

	vbox = glade_xml_get_widget(xml, "encryption_vbox");
	w = gtk_label_new(_(smime_encrypt_table[po->valid->encrypt.status].description));
	gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.5);
	gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
	gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	if (po->valid->encrypt.description) {
		w = gtk_label_new(po->valid->encrypt.description);
		gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.5);
		gtk_label_set_line_wrap((GtkLabel *)w, FALSE);
		gtk_box_pack_start((GtkBox *)vbox, w, TRUE, TRUE, 6);
	}

	if (!e_dlist_empty(&po->valid->encrypt.encrypters))
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
	const char *name;

	/* FIXME: need to have it based on encryption and signing too */
	if (po->valid->sign.status != 0)
		name = smime_sign_table[po->valid->sign.status].icon;
	else
		name = smime_encrypt_table[po->valid->encrypt.status].icon;

	icon = e_icon_factory_get_image (name, E_ICON_SIZE_LARGE_TOOLBAR);
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
		char *classid;
		struct _smime_pobject *pobj;
		
		camel_stream_printf (stream, "<table border=0 width=\"100%%\" cellpadding=3 cellspacing=0%s><tr>",
				     smime_sign_colour[valid->sign.status]);
		
		classid = g_strdup_printf("smime:///em-format-html/%s/icon/signed", emf->part_id->str);
		pobj = (struct _smime_pobject *)em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(*pobj), classid, part, efhd_xpkcs7mime_button);
		pobj->valid = camel_cipher_validity_clone(valid);
		pobj->object.free = efhd_xpkcs7mime_free;
		camel_stream_printf(stream, "<td valign=top><object classid=\"%s\"></object></td><td width=100%% valign=top>", classid);
		g_free(classid);
		if (valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
			camel_stream_printf(stream, "%s<br>", _(smime_sign_table[valid->sign.status].shortdesc));
		}

		if (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
			camel_stream_printf(stream, "%s<br>", _(smime_encrypt_table[valid->encrypt.status].shortdesc));
		}

		camel_stream_printf(stream, "</td></tr></table>");
	}
}

static void
efhd_image(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *handle)
{
	char *classid;
	struct _attach_puri *info;
	
	classid = g_strdup_printf("image%s", ((EMFormat *)efh)->part_id->str);
	info = (struct _attach_puri *)em_format_add_puri((EMFormat *)efh, sizeof(*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject(efh, sizeof(EMFormatHTMLPObject), classid, part, efhd_attachment_image);

	info->handle = handle;
	info->shown = TRUE;
	info->snoop_mime_type = ((EMFormat *) efh)->snoop_mime_type;
	info->fit_width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;

	camel_stream_printf(stream, "<td><object classid=\"%s\"></object></td>", classid);
	g_free(classid);
}

/* ********************************************************************** */

static EMFormatHandler type_builtin_table[] = {
	{ "image/gif", (EMFormatFunc)efhd_image },
	{ "image/jpeg", (EMFormatFunc)efhd_image },
	{ "image/png", (EMFormatFunc)efhd_image },
	{ "image/x-png", (EMFormatFunc)efhd_image },
	{ "image/tiff", (EMFormatFunc)efhd_image },
	{ "image/x-bmp", (EMFormatFunc)efhd_image },
	{ "image/bmp", (EMFormatFunc)efhd_image },
	{ "image/svg", (EMFormatFunc)efhd_image },
	{ "image/x-cmu-raster", (EMFormatFunc)efhd_image },
	{ "image/x-ico", (EMFormatFunc)efhd_image },
	{ "image/x-portable-anymap", (EMFormatFunc)efhd_image },
	{ "image/x-portable-bitmap", (EMFormatFunc)efhd_image },
	{ "image/x-portable-graymap", (EMFormatFunc)efhd_image },
	{ "image/x-portable-pixmap", (EMFormatFunc)efhd_image },
	{ "image/x-xpixmap", (EMFormatFunc)efhd_image },

	/* This is where one adds those busted, non-registered types,
	   that some idiot mailer writers out there decide to pull out
	   of their proverbials at random. */

	{ "image/jpg", (EMFormatFunc)efhd_image },
	{ "image/pjpeg", (EMFormatFunc)efhd_image },

	{ "x-evolution/message/prefix", (EMFormatFunc)efhd_message_prefix },
	{ "x-evolution/message/post-header", (EMFormatFunc)efhd_message_add_bar },
};

static void
efhd_builtin_init(EMFormatHTMLDisplayClass *efhc)
{
	int i;

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}

/* ********************************************************************** */

static void
efhd_bonobo_unknown(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	char *classid;

	classid = g_strdup_printf("bonobo-unknown:///em-format-html-display/%s", emf->part_id->str);
	em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(EMFormatHTMLPObject), classid, part, efhd_bonobo_object);
	camel_stream_printf(stream, "<object classid=\"%s\" type=\"%s\"></object><br>\n", classid, info->mime_type);
	g_free(classid);
}

/* ********************************************************************** */

static const EMFormatHandler *efhd_find_handler(EMFormat *emf, const char *mime_type)
{
	const EMFormatHandler *handle;

	if ( (handle = ((EMFormatClass *)efhd_parent)->find_handler(emf, mime_type)) == NULL
	     && efhd_use_component(mime_type)
	     && (handle = g_hash_table_lookup(efhd_bonobo_handlers, mime_type)) == NULL) {
		EMFormatHandler *h = g_malloc0(sizeof(*h));

		h->mime_type = g_strdup(mime_type);
		h->handler = efhd_bonobo_unknown;
		h->flags = EM_FORMAT_HANDLER_INLINE_DISPOSITION;
		g_hash_table_insert(efhd_bonobo_handlers, h->mime_type, h);

		handle = h;
	}

	return handle;
}

static void efhd_format_clone(EMFormat *emf, CamelFolder *folder, const char *uid, CamelMimeMessage *msg, EMFormat *src)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) emf;

	if (emf != src) {
		if (src)
			efhd->priv->show_bar = ((EMFormatHTMLDisplay *)src)->priv->show_bar;
		else
			efhd->priv->show_bar = FALSE;
	}

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
	const char *flag, *comp, *due;
	time_t date;
	char due_date[128];
	struct tm due_tm;
	char *iconpath;

	if (emf->folder == NULL || emf->uid == NULL
	    || (flag = camel_folder_get_message_user_tag(emf->folder, emf->uid, "follow-up")) == NULL
	    || flag[0] == 0)
		return;

	/* header displayed for message-flags in mail display */
	camel_stream_printf(stream, "<table border=1 width=\"100%%\" cellspacing=2 cellpadding=2><tr>");

	comp = camel_folder_get_message_user_tag(emf->folder, emf->uid, "completed-on");
	iconpath = e_icon_factory_get_icon_filename (comp && comp[0] ? "stock_flag-for-followup-done" : "stock_flag-for-followup", E_ICON_SIZE_MENU);
	if (iconpath) {
		CamelMimePart *iconpart;

		iconpart = em_format_html_file_part((EMFormatHTML *)emf, "image/png", iconpath);
		g_free (iconpath);
		if (iconpart) {
			char *classid;

			classid = g_strdup_printf("icon:///em-format-html-display/%s/%s", emf->part_id->str, comp&&comp[0]?"comp":"uncomp");
			camel_stream_printf(stream, "<td align=\"left\"><img src=\"%s\"></td>", classid);
			(void)em_format_add_puri(emf, sizeof(EMFormatPURI), classid, iconpart, efhd_write_image);
			g_free(classid);
			camel_object_unref(iconpart);
		}
	}

	camel_stream_printf(stream, "<td align=\"left\" width=\"100%%\">");

	if (comp && comp[0]) {
		date = camel_header_decode_date(comp, NULL);
		localtime_r(&date, &due_tm);
		e_utf8_strftime_fix_am_pm(due_date, sizeof (due_date), _("Completed on %B %d, %Y, %l:%M %p"), &due_tm);
		camel_stream_printf(stream, "%s, %s", flag, due_date);
	} else if ((due = camel_folder_get_message_user_tag(emf->folder, emf->uid, "due-by")) != NULL && due[0]) {
		time_t now;

		date = camel_header_decode_date(due, NULL);
		now = time(NULL);
		if (now > date)
			camel_stream_printf(stream, "<b>%s</b>&nbsp;", _("Overdue:"));
		
		localtime_r(&date, &due_tm);
		e_utf8_strftime_fix_am_pm(due_date, sizeof (due_date), _("by %B %d, %Y, %l:%M %p"), &due_tm);
		camel_stream_printf(stream, "%s %s", flag, due_date);
	} else {
		camel_stream_printf(stream, "%s", flag);
	}

	camel_stream_printf(stream, "</td></tr></table>");
}

/* TODO: if these aren't going to do anything should remove */
static void efhd_format_error(EMFormat *emf, CamelStream *stream, const char *txt)
{
	((EMFormatClass *)efhd_parent)->format_error(emf, stream, txt);
}

static void efhd_format_source(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	((EMFormatClass *)efhd_parent)->format_source(emf, stream, part);
}

/* ********************************************************************** */

/* if it hasn't been processed yet, format the attachment */
static void
efhd_attachment_show(EPopup *ep, EPopupItem *item, void *data)
{
	struct _attach_puri *info = data;

	d(printf("show attachment button called\n"));

	info->shown = ~info->shown;
	em_format_set_inline(info->puri.format, info->puri.part_id, info->shown);
}

static void
efhd_attachment_button_show(GtkWidget *w, void *data)
{
	efhd_attachment_show(NULL, NULL, data);
}

static void
efhd_image_fit(EPopup *ep, EPopupItem *item, void *data)
{
	struct _attach_puri *info = data;

	info->fit_width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;
	gtk_image_set_from_pixbuf(info->image, em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height));
}

static void
efhd_image_unfit(EPopup *ep, EPopupItem *item, void *data)
{
	struct _attach_puri *info = data;

	info->fit_width = 0;
	gtk_image_set_from_pixbuf((GtkImage *)info->image, em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height));
}

static EPopupItem efhd_menu_items[] = {
	{ E_POPUP_BAR, "05.display", },
	{ E_POPUP_ITEM, "05.display.00", N_("_View Inline"), efhd_attachment_show },
	{ E_POPUP_ITEM, "05.display.00", N_("_Hide"), efhd_attachment_show },
	{ E_POPUP_ITEM, "05.display.01", N_("_Fit to Width"), efhd_image_fit, NULL, NULL, EM_POPUP_PART_IMAGE },
	{ E_POPUP_ITEM, "05.display.01", N_("Show _Original Size"), efhd_image_unfit, NULL, NULL, EM_POPUP_PART_IMAGE },
};

static void
efhd_menu_items_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static void
efhd_popup_place_widget(GtkMenu *menu, int *x, int *y, gboolean *push_in, gpointer user_data)
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
	char *uri, *uri_crlf, *path;
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
			gtk_selection_data_set(data, data->target, 8, uri, strlen(uri));
			return;
		}

		path = em_utils_temp_save_part(w, part);
		if (path == NULL)
			return;

		uri = g_filename_to_uri(path, NULL, NULL);
		g_free(path);
		uri_crlf = g_strconcat(uri, "\r\n", NULL);
		g_free(uri);
		gtk_selection_data_set(data, data->target, 8, uri_crlf, strlen(uri_crlf));
		g_object_set_data_full((GObject *)w, "e-drag-uri", uri_crlf, g_free);
		break;
	default:
		abort();
	}
}

static void
efhd_drag_data_delete(GtkWidget *w, GdkDragContext *drag, EMFormatHTMLPObject *pobject)
{
	char *uri;
	
	uri = g_object_get_data((GObject *)w, "e-drag-uri");
	if (uri) {
		/* NB: this doesn't kill the dnd directory */
		/* NB: is this ever called? */
		/* NB even more: doesn't the e-drag-uri have \r\n
		 * appended? (see efhd_drag_data_get())
		 */
		char *filename = g_filename_from_uri (uri, NULL, NULL);
		g_unlink(filename);
		g_free(filename);
		g_object_set_data((GObject *)w, "e-drag-uri", NULL);
	}
}

static void
efhd_write_icon_job(struct _EMFormatHTMLJob *job, int cancelled)
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
	int width;

	if (info->fit_width == 0)
		return;

	width = ((GtkWidget *)((EMFormatHTML *)info->puri.format)->html)->allocation.width - 12;
	if (info->fit_width == width)
		return;
	info->fit_width = width;
	pb = em_icon_stream_get_image(info->puri.cid, info->fit_width, info->fit_height);
	gtk_image_set_from_pixbuf(info->image, pb);
	g_object_unref(pb);
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
		{ "text/uri-list", 0, 1 },
	};
	char *simple_type;

	info = (struct _attach_puri *)em_format_find_puri((EMFormat *)efh, pobject->classid);

	info->image = (GtkImage *)gtk_image_new();
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
	g_signal_connect(box, "popup_menu", G_CALLBACK(efhd_attachment_popup_menu), info);

	return TRUE;
}

/* attachment button callback */
static gboolean
efhd_attachment_button(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)efh;
	EAttachment *new;
	struct _attach_puri *info;
	GtkWidget *hbox, *w, *button, *mainbox;
	char *simple_type, *tmp, *new_file = NULL;
	const char *file;
	GtkTargetEntry drag_types[] = {
		{ NULL, 0, 0 },
		{ "text/uri-list", 0, 1 },
	};
	AtkObject *a11y;

	/* FIXME: handle default shown case */
	d(printf("adding attachment button/content\n"));

	info = (struct _attach_puri *)em_format_find_puri((EMFormat *)efh, pobject->classid);
	g_assert(info != NULL);
	g_assert(info->forward == NULL);

	if (efhd->priv->attachment_bar) {
		file = camel_mime_part_get_filename(info->puri.part);

		new = e_attachment_new_from_mime_part (info->puri.part);

		if (!file) {
			file = "attachment.dat";
			new->file_name = g_strdup(file);
		}

		tmp = g_hash_table_lookup (efhd->priv->files, file);
		if (tmp) {
			guint count = GPOINTER_TO_UINT(tmp);
			char *ext;
			char *tmp_file = g_strdup (file);
		
			if ((ext = strrchr(tmp_file, '.'))) {
				ext[0] = 0;
				new_file = g_strdup_printf("%s(%d).%s", tmp_file, count++, ext+1);
			} else {
				new_file = g_strdup_printf("%s(%d)", tmp_file, count++);
			}

			g_free (tmp_file);
			g_hash_table_insert (efhd->priv->files, g_strdup(file), GUINT_TO_POINTER(count));
			g_free (new->file_name);
			new->file_name = new_file;
		} else {
			g_hash_table_insert (efhd->priv->files, g_strdup(file), GUINT_TO_POINTER(1));
		}

		/* Store the status of encryption / signature on the attachment for emblem display 
		 * FIXME: May not work well always
		 */
		new->sign = info->sign;
		new->encrypt = info->encrypt;
	
		/* Add the attachment to the bar.*/
		e_attachment_bar_add_attachment(E_ATTACHMENT_BAR(efhd->priv->attachment_bar), new);
		efhd_attachment_bar_refresh(efhd);
	}
	
	mainbox = gtk_hbox_new(FALSE, 0);

	button = gtk_button_new();

	if (info->handle)
		g_signal_connect(button, "clicked", G_CALLBACK(efhd_attachment_button_show), info);
	else {
		gtk_widget_set_sensitive(button, FALSE);
		GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
	}

	hbox = gtk_hbox_new(FALSE, 2);
	info->forward = gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start((GtkBox *)hbox, info->forward, TRUE, TRUE, 0);
	if (info->handle) {
		info->down = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start((GtkBox *)hbox, info->down, TRUE, TRUE, 0);
	}

	w = gtk_image_new();
	gtk_widget_set_size_request(w, 24, 24);
	gtk_box_pack_start((GtkBox *)hbox, w, TRUE, TRUE, 0);
	gtk_container_add((GtkContainer *)button, hbox);
	gtk_box_pack_start((GtkBox *)mainbox, button, TRUE, TRUE, 0);

	/* Check for snooped type to get the right icon/processing */
	if (info->snoop_mime_type)
		simple_type = g_strdup(info->snoop_mime_type);
	else
		simple_type = camel_content_type_simple (((CamelDataWrapper *)pobject->part)->mime_type);
	camel_strdown(simple_type);

	/* FIXME: offline parts, just get icon */
	if (camel_content_type_is(((CamelDataWrapper *)pobject->part)->mime_type, "image", "*")) {
		EMFormatHTMLJob *job;
		GdkPixbuf *mini;
		char *key;

		key = pobject->classid;
		mini = em_icon_stream_get_image(key, 24, 24);
		if (mini) {
			d(printf("got image from cache '%s'\n", key));
			gtk_image_set_from_pixbuf((GtkImage *)w, mini);
			g_object_unref(mini);
		} else {
			d(printf("need to create icon image '%s'\n", key));
			job = em_format_html_job_new(efh, efhd_write_icon_job, pobject);
			job->stream = (CamelStream *)em_icon_stream_new((GtkImage *)w, key, 24, 24, FALSE);
			em_format_html_job_queue(efh, job);
		}
	} else {
		GdkPixbuf *pixbuf, *mini;
		
		if ((pixbuf = e_icon_for_mime_type (simple_type, 24))) {
			if ((mini = gdk_pixbuf_scale_simple (pixbuf, 24, 24, GDK_INTERP_BILINEAR))) {
				gtk_image_set_from_pixbuf ((GtkImage *) w, mini);
				g_object_unref (mini);
			}
			g_object_unref (pixbuf);
		}
	}

	drag_types[0].target = simple_type;
	gtk_drag_source_set(button, GDK_BUTTON1_MASK, drag_types, sizeof(drag_types)/sizeof(drag_types[0]), GDK_ACTION_COPY);
	g_signal_connect(button, "drag-data-get", G_CALLBACK(efhd_drag_data_get), pobject);
	g_signal_connect (button, "drag-data-delete", G_CALLBACK(efhd_drag_data_delete), pobject);
	g_free(simple_type);

	button = gtk_button_new();
	/*GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);*/
	gtk_container_add((GtkContainer *)button, gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE));

	a11y = gtk_widget_get_accessible (button);
	atk_object_set_name (a11y, _("Attachment Button"));

	g_signal_connect(button, "button_press_event", G_CALLBACK(efhd_attachment_popup), info);
	g_signal_connect(button, "popup_menu", G_CALLBACK(efhd_attachment_popup_menu), info);
	g_signal_connect(button, "clicked", G_CALLBACK(efhd_attachment_popup_menu), info);
	gtk_box_pack_start((GtkBox *)mainbox, button, TRUE, TRUE, 0);

	gtk_widget_show_all(mainbox);

	if (info->shown)
		gtk_widget_hide(info->forward);
	else if (info->down)
		gtk_widget_hide(info->down);

	gtk_container_add((GtkContainer *)eb, mainbox);

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

static gboolean
efhd_bonobo_object(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	CamelDataWrapper *wrapper;
	Bonobo_ServerInfo *component;
	GtkWidget *embedded;
	Bonobo_PersistStream persist;	
	CORBA_Environment ev;
	CamelStreamMem *cstream;
	BonoboStream *bstream;
	BonoboControlFrame *control_frame;
	Bonobo_PropertyBag prop_bag;

	component = gnome_vfs_mime_get_default_component(eb->type);
	if (component == NULL)
		return FALSE;

	embedded = bonobo_widget_new_control(component->iid, NULL);
	CORBA_free(component);
	if (embedded == NULL)
		return FALSE;
	
	CORBA_exception_init(&ev);

	control_frame = bonobo_widget_get_control_frame((BonoboWidget *)embedded);
	prop_bag = bonobo_control_frame_get_control_property_bag(control_frame, NULL);
	if (prop_bag != CORBA_OBJECT_NIL) {
		/*
		 * Now we can take care of business. Currently, the only control
		 * that needs something passed to it through a property bag is
		 * the iTip control, and it needs only the From email address,
		 * but perhaps in the future we can generalize this section of code
		 * to pass a bunch of useful things to all embedded controls.
		 */
		const CamelInternetAddress *from;
		char *from_address;
		
		from = camel_mime_message_get_from((CamelMimeMessage *)((EMFormat *)efh)->message);
		from_address = camel_address_encode((CamelAddress *)from);
		bonobo_property_bag_client_set_value_string(prop_bag, "from_address", from_address, &ev);
		g_free(from_address);
		
		Bonobo_Unknown_unref(prop_bag, &ev);
	}
	
	persist = (Bonobo_PersistStream)Bonobo_Unknown_queryInterface(bonobo_widget_get_objref((BonoboWidget *)embedded),
								      "IDL:Bonobo/PersistStream:1.0", &ev);
	if (persist == CORBA_OBJECT_NIL) {
		gtk_object_sink((GtkObject *)embedded);
		CORBA_exception_free(&ev);				
		return FALSE;
	}
	
	/* Write the data to a CamelStreamMem... */
	cstream = (CamelStreamMem *)camel_stream_mem_new();
	wrapper = camel_medium_get_content_object((CamelMedium *)pobject->part);
	if (FALSE && !g_ascii_strncasecmp (eb->type, "text/", 5)) {
		/* do charset conversion, etc */
		d(printf("performing charset conversion for %s component\n", eb->type));
		em_format_format_text((EMFormat *)efh, (CamelStream *)cstream, wrapper);
	} else {
		camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) cstream);
	}
	
	/* ...convert the CamelStreamMem to a BonoboStreamMem... */
	bstream = bonobo_stream_mem_create(cstream->buffer->data, cstream->buffer->len, TRUE, FALSE);
	camel_object_unref(cstream);
	
	/* ...and hydrate the PersistStream from the BonoboStream. */
	Bonobo_PersistStream_load(persist,
				  bonobo_object_corba_objref(BONOBO_OBJECT (bstream)),
				  eb->type, &ev);
	bonobo_object_unref(BONOBO_OBJECT (bstream));
	Bonobo_Unknown_unref(persist, &ev);
	CORBA_Object_release(persist, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		gtk_object_sink((GtkObject *)embedded);
		CORBA_exception_free(&ev);				
		return FALSE;
	}
	CORBA_exception_free(&ev);
	
	gtk_widget_show(embedded);
	gtk_container_add(GTK_CONTAINER (eb), embedded);
	
	return TRUE;
}

static gboolean
efhd_check_server_prop(Bonobo_ServerInfo *component, const char *propname, const char *value)
{
	CORBA_sequence_CORBA_string stringv;
	Bonobo_ActivationProperty *prop;
	int i;

	prop = bonobo_server_info_prop_find(component, propname);
	if (!prop || prop->v._d != Bonobo_ACTIVATION_P_STRINGV)
		return FALSE;

	stringv = prop->v._u.value_stringv;
	for (i = 0; i < stringv._length; i++) {
		if (!g_ascii_strcasecmp(value, stringv._buffer[i]))
			return TRUE;
	}

	return FALSE;
}

static gboolean
efhd_use_component(const char *mime_type)
{
	GList *components, *iter;
	Bonobo_ServerInfo *component = NULL;

	/* should this cache it? */

	if (g_ascii_strcasecmp(mime_type, "text/x-vcard") != 0
	    && g_ascii_strcasecmp(mime_type, "text/calendar") != 0) {
		const char **mime_types;
		int i;

		mime_types = mail_config_get_allowable_mime_types();
		for (i = 0; mime_types[i]; i++) {
			if (!g_ascii_strcasecmp(mime_types[i], mime_type))
				goto type_ok;
		}
		return FALSE;
	}
type_ok:
	components = gnome_vfs_mime_get_all_components (mime_type);
	for (iter = components; iter; iter = iter->next) {
		Bonobo_ServerInfo *comp = iter->data;

		comp = iter->data;
		if (efhd_check_server_prop(comp, "repo_ids", "IDL:Bonobo/PersistStream:1.0")
		    && efhd_check_server_prop(comp, "bonobo:supported_mime_types", mime_type)) {
			component = comp;
			break;
		}
	}
	gnome_vfs_mime_component_list_free (components);

	return component != NULL;
}

static void
attachment_bar_arrow_clicked(GtkWidget *w, EMFormatHTMLDisplay *efhd)
{

	efhd->priv->show_bar = !efhd->priv->show_bar;

	if (efhd->priv->show_bar) {
		gtk_widget_show(efhd->priv->attachment_box);
		gtk_widget_show(efhd->priv->down);
		gtk_widget_hide(efhd->priv->forward);
	} else {
		gtk_widget_hide(efhd->priv->attachment_box);
		gtk_widget_show(efhd->priv->forward);
		gtk_widget_hide(efhd->priv->down);		
	}
}

static void
attachments_save_all_clicked(GtkWidget *w, EMFormatHTMLDisplay *efhd)
{
	GSList *attachment_parts;

	attachment_parts = e_attachment_bar_get_parts(E_ATTACHMENT_BAR(efhd->priv->attachment_bar));
	em_utils_save_parts(w, _("Select folder to save all attachments..."), attachment_parts);
}

static void
efhd_bar_popup_position(GtkMenu *menu, int *x, int *y, gboolean *push_in, gpointer user_data)
{
	EAttachmentBar *bar = user_data;
	GnomeIconList *icon_list = user_data;
	GList *selection;
	GnomeCanvasPixbuf *image;
	
	gdk_window_get_origin (((GtkWidget*) bar)->window, x, y);
	
	selection = gnome_icon_list_get_selection (icon_list);
	if (selection == NULL)
		return;
	
	image = gnome_icon_list_get_icon_pixbuf_item (icon_list, GPOINTER_TO_INT(selection->data));
	if (image == NULL)
		return;
	
	/* Put menu to the center of icon. */
	*x += (int)(image->item.x1 + image->item.x2) / 2;
	*y += (int)(image->item.y1 + image->item.y2) / 2;
}

static void
efhd_bar_save_selected(EPopup *ep, EPopupItem *item, void *data)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)data;
	GSList *attachment_parts, *tmp;
	GSList *parts = NULL;

	attachment_parts = e_attachment_bar_get_selected(E_ATTACHMENT_BAR(efhd->priv->attachment_bar));
	
	for (tmp = attachment_parts; tmp; tmp=tmp->next)
		parts = g_slist_prepend(parts, ((EAttachment *)tmp->data)->body);

	parts = g_slist_reverse(parts);
	em_utils_save_parts(efhd->priv->attachment_bar, _("Select folder to save selected attachments..."), parts);

	g_slist_foreach(attachment_parts, (GFunc)g_object_unref, NULL);
	g_slist_free (attachment_parts);
}

static EPopupItem efhd_bar_menu_items[] = {
	{ E_POPUP_BAR, "05.display", },
	{ E_POPUP_ITEM, "05.display.01", N_("Save Selected..."), efhd_bar_save_selected, NULL, NULL, EM_POPUP_ATTACHMENTS_MULTIPLE},
};

static gboolean
efhd_bar_button_press_event(EAttachmentBar *bar, GdkEventButton *event, EMFormat *emf)
{
	GtkMenu *menu;
	GSList *list=NULL;
	EPopupTarget *target;
	EMPopup *emp;
	GSList *menus = NULL;
	int i;

	if (event && event->button != 3)
		return FALSE;

	/** @HookPoint-EMPopup: Attachment Bar  Context Menu
	 * @Id: org.gnome.evolution.mail.attachments.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: EMPopupTargetPart
	 *
	 * This is the drop-down menu shown when a user clicks on the attachment bar
	 * when attachments are selected.
	 */
	emp = em_popup_new("org.gnome.evolution.mail.attachments.popup");

	/* Add something like save-selected, foward selected attachments in a mail etc....*/
	list = e_attachment_bar_get_selected(bar);
	
	/* Lets not propagate any more the r-click which is intended to us*/
	if ( g_slist_length (list) == 0)
		return TRUE;
	
	target = (EPopupTarget *)em_popup_target_new_attachments(emp, list);
	for (i=0; i<2; i++)
		menus = g_slist_prepend(menus, &efhd_bar_menu_items[i]);
	e_popup_add_items((EPopup *)emp, menus, NULL, efhd_menu_items_free, emf);

	((EMPopupTargetPart *)target)->target.widget = (GtkWidget *)bar;
	menu = e_popup_create_menu_once((EPopup *)emp, (EPopupTarget *)target, 0);
	if (event)
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
	else
		gtk_menu_popup(menu, NULL, NULL, (GtkMenuPositionFunc)efhd_bar_popup_position, bar, 0, gtk_get_current_event_time());

	return TRUE;
}

static gboolean
efhd_bar_popup_menu_event (EAttachmentBar *bar, EMFormat *emf) 
{
	return efhd_bar_button_press_event(bar, NULL, emf);
}

static void
efhd_attachment_bar_refresh (EMFormatHTMLDisplay *efhd)
{
	int nattachments;

	if (!efhd->priv->attachment_bar)
		return;

	nattachments = e_attachment_bar_get_num_attachments (E_ATTACHMENT_BAR(efhd->priv->attachment_bar));
	if (nattachments) {
		char *txt;

		/* Cant i put in the number of attachments here ?*/
		txt = g_strdup_printf(ngettext("%d attachment", "%d attachments", nattachments), nattachments);
		gtk_label_set_text ((GtkLabel *)efhd->priv->label, txt);
		g_free (txt);
	
		/* Show the bar even when the first attachment is added */
		if (nattachments == 1) {
			gtk_widget_show_all (efhd->priv->attachment_area);
			
			if (efhd->priv->show_bar) {
				gtk_widget_show(efhd->priv->down);
				gtk_widget_hide(efhd->priv->forward);
			} else {
				gtk_widget_show(efhd->priv->forward);
				gtk_widget_hide(efhd->priv->down);
				gtk_widget_hide(efhd->priv->attachment_box);
			}
		}
	}
}

static void
efhd_bar_resize(GtkWidget *w, GtkAllocation *event, EMFormatHTML *efh)
{
	int width;
	GtkRequisition req;
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) efh;

	gtk_widget_size_request (efhd->priv->attachment_bar, &req);
	width = ((GtkWidget *) efh->html)->allocation.width - 16;
	/* Update the width of the bar */
	e_attachment_bar_set_width(E_ATTACHMENT_BAR(efhd->priv->attachment_bar), width);
}

static gboolean
efhd_bar_scroll_event(GtkWidget *w, GdkEventScroll *event, EMFormatHTMLDisplay *efhd)
{
	gboolean ret;

	/* Emulate the scroll over the attachment bar, as if it is scrolled in the window.
	*  It doesnt go automatically since the GnomeIconList is a layout by itself
	*/
	g_signal_emit_by_name (gtk_widget_get_parent((GtkWidget *)efhd->formathtml.html), "scroll_event", event, &ret);

	return TRUE;
}

static gboolean
efhd_add_bar(EMFormatHTML *efh, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *)efh;
	struct _EMFormatHTMLDisplayPrivate *priv = efhd->priv;
	GtkWidget *hbox1, *hbox2, *hbox3, *vbox, *txt, *image, *save;
	int width, height;

	priv->attachment_bar = e_attachment_bar_new(NULL);
	((EAttachmentBar *)priv->attachment_bar)->expand = TRUE;
	
	priv->forward = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	priv->down = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	hbox3 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox3, priv->forward, FALSE, FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox3, priv->down, FALSE, FALSE, 0);
	priv->arrow = (GtkWidget *)gtk_tool_button_new(hbox3, NULL);
	atk_object_set_name (gtk_widget_get_accessible (priv->arrow), _("Toggle Attachment Bar"));

	priv->label = gtk_label_new(_("No Attachment"));
	save = gtk_button_new();
	image = gtk_image_new_from_stock ("gtk-save", GTK_ICON_SIZE_BUTTON);
	txt = gtk_label_new(_("Save All"));
	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start((GtkBox *)hbox1, image, FALSE, FALSE, 2);
	gtk_box_pack_start((GtkBox *)hbox1, txt, FALSE, FALSE, 0);

	gtk_container_add((GtkContainer *)save, hbox1);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox2, priv->arrow, FALSE, FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox2, priv->label, FALSE, FALSE, 2);
	gtk_box_pack_start ((GtkBox *)hbox2, save, FALSE, FALSE, 2);

	priv->attachment_box = gtk_frame_new (NULL);
	gtk_container_add ((GtkContainer *)priv->attachment_box, priv->attachment_bar);

	gtk_widget_get_size_request(priv->attachment_bar, &width, &height);

	/* FIXME: What if the text is more?. Should we reduce the text with appending ...?
	 * or resize the bar? How to figure out that, it needs more space? */
	gtk_widget_set_size_request (priv->attachment_bar, 
					((GtkWidget *)efh->html)->parent->allocation.width - /* FIXME */16,
					84 /* FIXME: Default show only one row, Dont hardcode size*/);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)vbox, hbox2, FALSE, FALSE, 2);
	gtk_box_pack_start ((GtkBox *)vbox, priv->attachment_box, TRUE, TRUE, 2);

	gtk_container_add ((GtkContainer *)eb, vbox);
	gtk_widget_show ((GtkWidget *)eb);

	/* Lets hide it by default and show only when there are attachments */
	priv->attachment_area = vbox;
	gtk_widget_hide_all (priv->attachment_area);
	
	g_signal_connect (priv->arrow, "clicked", G_CALLBACK(attachment_bar_arrow_clicked), efh);
	g_signal_connect (priv->attachment_bar, "button_press_event", G_CALLBACK(efhd_bar_button_press_event), efhd);
	g_signal_connect (priv->attachment_bar, "popup-menu", G_CALLBACK(efhd_bar_popup_menu_event), efhd);
	g_signal_connect (save, "clicked", G_CALLBACK(attachments_save_all_clicked), efh);
	g_signal_connect (eb, "size_allocate", G_CALLBACK (efhd_bar_resize), efh);
	g_signal_connect (priv->attachment_bar, "scroll_event", G_CALLBACK(efhd_bar_scroll_event), efhd);

	return TRUE;
}

static void
efhd_message_add_bar(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) emf;
	const char *classid = "attachment-bar";

	if (efhd->nobar || efhd->priv->files)
		return;

	efhd->priv->files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(EMFormatHTMLPObject), classid, part, efhd_add_bar);
	camel_stream_printf(stream, "<td><object classid=\"%s\"></object></td>", classid);
}

static void
efhd_format_attachment(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const char *mime_type, const EMFormatHandler *handle)
{
	char *classid, *text, *html;
	struct _attach_puri *info;

	classid = g_strdup_printf("attachment%s", emf->part_id->str);
	info = (struct _attach_puri *)em_format_add_puri(emf, sizeof(*info), classid, part, efhd_attachment_frame);
	em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(EMFormatHTMLPObject), classid, part, efhd_attachment_button);
	info->handle = handle;
	info->shown = em_format_is_inline(emf, info->puri.part_id, info->puri.part, handle);
	info->snoop_mime_type = emf->snoop_mime_type;

	if (emf->valid) {
		info->sign = emf->valid->sign.status;
		info->encrypt = emf->valid->encrypt.status;
	}

	camel_stream_write_string(stream,
				  EM_FORMAT_HTML_VPAD
				  "<table cellspacing=0 cellpadding=0><tr><td>"
				  "<table width=10 cellspacing=0 cellpadding=0>"
				  "<tr><td></td></tr></table></td>");

	camel_stream_printf(stream, "<td><object classid=\"%s\"></object></td>", classid);

	camel_stream_write_string(stream,
				  "<td><table width=3 cellspacing=0 cellpadding=0>"
				  "<tr><td></td></tr></table></td><td><font size=-1>");

	/* output some info about it */
	/* FIXME: should we look up mime_type from object again? */
	text = em_format_describe_part(part, mime_type);
	html = camel_text_to_html(text, ((EMFormatHTML *)emf)->text_html_flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string(stream, html);
	g_free(html);
	g_free(text);

	camel_stream_write_string(stream,
				  "</font></td></tr><tr></table>\n"
				  EM_FORMAT_HTML_VPAD);

	if (handle) {
		if (info->shown)
			handle->handler(emf, stream, part, handle);
		/*camel_stream_printf(stream, "<iframe src=\"%s\" marginheight=0 marginwidth=0>%s</iframe>\n", classid, _("Attachment content could not be loaded"));*/
	} else if (efhd_use_component(mime_type)) {
		g_free(classid); /* messy */

		classid = g_strdup_printf("bonobo-unknown:///em-format-html-display/%s", emf->part_id->str);
		em_format_html_add_pobject((EMFormatHTML *)emf, sizeof(EMFormatHTMLPObject), classid, part, efhd_bonobo_object);
		camel_stream_printf(stream, "<object classid=\"%s\" type=\"%s\"></object><br>>\n", classid, mime_type);
	}

	g_free(classid);
}
