/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2003 Ximian, Inc. (www.ximian.com)
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

#include "eab-contact-display.h"

#include "e-util/e-html-utils.h"
#include "util/eab-destination.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

struct _EABContactDisplayPrivate {
	GtkHTML *html;
	EContact *contact;
};


#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n"  \
                    "<head>\n<meta name=\"generator\" content=\"Evolution Addressbook Component\">\n</head>\n"

#define MAX_COMPACT_IMAGE_DIMENSION 48

static void
on_url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle,
		  EABContactDisplay *display)
{
	printf ("on_url_requested (%s)\n", url);
	if (!strcmp (url, "internal-contact-photo:")) {
		EContactPhoto *photo;

		photo = e_contact_get (display->priv->contact, E_CONTACT_PHOTO);
		if (!photo)
			photo = e_contact_get (display->priv->contact, E_CONTACT_LOGO);

		printf ("writing a photo of length %d\n", photo->length);

		gtk_html_stream_write (handle, photo->data, photo->length);

		gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
	}
}

static void
on_link_clicked (GtkHTML *html, const char *url, EABContactDisplay *display)
{
	GError *err = NULL;
		
	gnome_url_show (url, &err);
		
	if (err) {
		g_warning ("gnome_url_show: %s", err->message);
		g_error_free (err);
	}
}

static void
render_address (GtkHTMLStream *html_stream, EContact *contact, const char *html_label, EContactField adr_field, EContactField label_field)
{
	EContactAddress *adr;
	const char *label;

	label = e_contact_get_const (contact, label_field);
	if (label) {
		char *html = e_text_to_html (label, E_TEXT_TO_HTML_CONVERT_NL);

		gtk_html_stream_printf (html_stream, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr valign=\"top\"><td>");
		gtk_html_stream_printf (html_stream, "<b>%s:</b>&nbsp;<td>%s<br>", html_label, html);

		gtk_html_stream_printf (html_stream, "<a href=\"http://www.mapquest.com/\">%s</a>", _("Map It"));
		gtk_html_stream_printf (html_stream, "</td></tr></table>");
		g_free (html);
		return;
	}

	adr = e_contact_get (contact, adr_field);
	if (adr &&
	    (adr->po || adr->ext || adr->street || adr->locality || adr->region || adr->code || adr->country)) {

		gtk_html_stream_printf (html_stream, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr valign=\"top\"><td>");
		gtk_html_stream_printf (html_stream, "<b>%s:</b>&nbsp;<td>", html_label);

		if (adr->po && *adr->po) gtk_html_stream_printf (html_stream, "%s<br>", adr->po);
		if (adr->ext && *adr->ext) gtk_html_stream_printf (html_stream, "%s<br>", adr->ext);
		if (adr->street && *adr->street) gtk_html_stream_printf (html_stream, "%s<br>", adr->street);
		if (adr->locality && *adr->locality) gtk_html_stream_printf (html_stream, "%s<br>", adr->locality);
		if (adr->region && *adr->region) gtk_html_stream_printf (html_stream, "%s<br>", adr->region);
		if (adr->code && *adr->code) gtk_html_stream_printf (html_stream, "%s<br>", adr->code);
		if (adr->country && *adr->country) gtk_html_stream_printf (html_stream, "%s<br>", adr->country);

		gtk_html_stream_printf (html_stream, "<a href=\"http://www.mapquest.com/\">%s</a>", _("Map It"));
		gtk_html_stream_printf (html_stream, "</td></tr></table>");
	}
	if (adr)
		e_contact_address_free (adr);
}

static void
render_string (GtkHTMLStream *html_stream, EContact *contact, const char *html_label, EContactField field)
{
	const char *str;

	str = e_contact_get_const (contact, field);

	if (str && *str) {
		char *html = e_text_to_html (str, 0);
		gtk_html_stream_printf (html_stream, "<b>%s:</b> %s<br>", html_label, str);
		g_free (html);
	}
}

static void
render_url (GtkHTMLStream *html_stream, EContact *contact, const char *html_label, EContactField field)
{
	const char *str;
	str = e_contact_get_const (contact, field);
	if (str && *str) {
		char *html = e_text_to_html (str, E_TEXT_TO_HTML_CONVERT_URLS);
		gtk_html_stream_printf (html_stream, "<b>%s:</b> %s<br>",
					html_label, html);
		g_free (html);
	}
}

static void
eab_contact_display_render_normal (EABContactDisplay *display, EContact *contact)
{
	GtkHTMLStream *html_stream;

	if (display->priv->contact)
		g_object_unref (display->priv->contact);
	display->priv->contact = contact;
	if (display->priv->contact)
		g_object_ref (display->priv->contact);

	html_stream = gtk_html_begin (display->priv->html);
	gtk_html_stream_write (html_stream, HTML_HEADER, sizeof (HTML_HEADER) - 1);
	gtk_html_stream_write (html_stream, "<body>\n", 7);

	if (contact) {
		char *str, *html;
		EContactPhoto *photo;

		gtk_html_stream_printf (html_stream, "<table border=\"0\" valign=\"top\"><tr valign=\"top\"><td>");
		photo = e_contact_get (contact, E_CONTACT_PHOTO);
		if (!photo)
			photo = e_contact_get (contact, E_CONTACT_LOGO);
		if (photo) {
			gtk_html_stream_printf (html_stream, "<img src=\"internal-contact-photo:\">");
			e_contact_photo_free (photo);
		}
		
		gtk_html_stream_printf (html_stream, "</td><td>\n");

		str = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		if (str) {
			html = e_text_to_html (str, 0);
			gtk_html_stream_printf (html_stream, "<h2>%s</h2>", html);
			g_free (html);
		}
		else {
			str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
			if (str) {
				html = e_text_to_html (str, 0);
				gtk_html_stream_printf (html_stream, "<h2>%s</h2>", html);
				g_free (html);
			}
		}

		if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
			GList *email_list;
			GList *l;

			gtk_html_stream_printf (html_stream, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr valign=\"top\"><td>");
			gtk_html_stream_printf (html_stream, "<b>%s:</b>&nbsp;<td>", _("List Members"));

			email_list = e_contact_get (contact, E_CONTACT_EMAIL);
			for (l = email_list; l; l = l->next) {
				EABDestination *dest = eab_destination_import (l->data);
				if (dest) {
					const char *textrep = eab_destination_get_textrep (dest, TRUE);
					char *html = e_text_to_html (textrep, 0);
					gtk_html_stream_printf (html_stream, "%s<br>", html);
					g_free (html);
					g_object_unref (dest);
				}
			}
			gtk_html_stream_printf (html_stream, "</td></tr></table>");
		}
		else {
			render_string (html_stream, contact, _("Job Title"), E_CONTACT_TITLE);

			render_string (html_stream, contact, _("Email"), E_CONTACT_EMAIL_1);
			render_string (html_stream, contact, _("Email"), E_CONTACT_EMAIL_2);
			render_string (html_stream, contact, _("Email"), E_CONTACT_EMAIL_3);


			render_address (html_stream, contact, _("Home Address"),  E_CONTACT_ADDRESS_HOME,  E_CONTACT_ADDRESS_LABEL_HOME);
			render_address (html_stream, contact, _("Work Address"),  E_CONTACT_ADDRESS_WORK,  E_CONTACT_ADDRESS_LABEL_WORK);
			render_address (html_stream, contact, _("Other Address"), E_CONTACT_ADDRESS_OTHER, E_CONTACT_ADDRESS_LABEL_OTHER);

			gtk_html_stream_printf (html_stream, "<hr>");

			render_url (html_stream, contact, _("Home page"), E_CONTACT_HOMEPAGE_URL);
			render_url (html_stream, contact, _("Blog"), E_CONTACT_BLOG_URL);

		}

		gtk_html_stream_printf (html_stream, "</td></tr></table>\n");
	}

	gtk_html_stream_write (html_stream, "</body></html>\n", 15);
	gtk_html_end (display->priv->html, html_stream, GTK_HTML_STREAM_OK);
}

static void
eab_contact_display_render_compact (EABContactDisplay *display, EContact *contact)
{
	GtkHTMLStream *html_stream;

	if (display->priv->contact)
		g_object_unref (display->priv->contact);
	display->priv->contact = contact;
	if (display->priv->contact)
		g_object_ref (display->priv->contact);

	html_stream = gtk_html_begin (display->priv->html);
	gtk_html_stream_write (html_stream, HTML_HEADER, sizeof (HTML_HEADER) - 1);
	gtk_html_stream_write (html_stream, "<body>\n", 7);

	if (contact) {
		char *str, *html;
		EContactPhoto *photo;

		gtk_html_stream_printf (html_stream,
					"<table width=\"100%%\" cellpadding=1 cellspacing=0 bgcolor=\"#000000\">"
					"<tr><td>"
					"<table width=\"100%%\" cellpadding=0 cellspacing=0 bgcolor=\"#eeeeee\">"
					"<tr><td>"
					"<table>"
					"<tr valign=\"top\"><td>");

		photo = e_contact_get (contact, E_CONTACT_PHOTO);
		if (!photo)
			photo = e_contact_get (contact, E_CONTACT_LOGO);
		if (photo) {
			int calced_width = MAX_COMPACT_IMAGE_DIMENSION, calced_height = MAX_COMPACT_IMAGE_DIMENSION;
			GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
			GdkPixbuf *pixbuf;

			/* figure out if we need to downscale the
			   image here.  we don't scale the pixbuf
			   itself, just insert width/height tags in
			   the html */
			gdk_pixbuf_loader_write (loader, photo->data, photo->length, NULL);
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			gdk_pixbuf_loader_close (loader, NULL);
			g_object_unref (loader);
			if (pixbuf) {
				int max_dimension = gdk_pixbuf_get_height (pixbuf);
				if (max_dimension < gdk_pixbuf_get_width (pixbuf))
					max_dimension = gdk_pixbuf_get_width (pixbuf);

				calced_width = (float)gdk_pixbuf_get_width (pixbuf) / max_dimension * MAX_COMPACT_IMAGE_DIMENSION;
				calced_height = (float)gdk_pixbuf_get_height (pixbuf) / max_dimension * MAX_COMPACT_IMAGE_DIMENSION;

				calced_width = MIN (calced_width, MAX_COMPACT_IMAGE_DIMENSION);
				calced_height = MIN (calced_height, MAX_COMPACT_IMAGE_DIMENSION);
			}

			gdk_pixbuf_unref (pixbuf);
			gtk_html_stream_printf (html_stream, "<img width=\"%d\" height=\"%d\" src=\"internal-contact-photo:\">",
						calced_width, calced_height);
			e_contact_photo_free (photo);
		}
		
		gtk_html_stream_printf (html_stream, "</td><td>\n");

		str = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		if (str) {
			html = e_text_to_html (str, 0);
			gtk_html_stream_printf (html_stream, "<b>%s</b>", html);
			g_free (html);
		}
		else {
			str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
			if (str) {
				html = e_text_to_html (str, 0);
				gtk_html_stream_printf (html_stream, "<b>%s</b>", html);
				g_free (html);
			}
		}

		gtk_html_stream_write (html_stream, "<hr>", 4);
		
		if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
			GList *email_list;
			GList *l;

			gtk_html_stream_printf (html_stream, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr valign=\"top\"><td>");
			gtk_html_stream_printf (html_stream, "<b>%s:</b>&nbsp;<td>", _("List Members"));

			email_list = e_contact_get (contact, E_CONTACT_EMAIL);
			for (l = email_list; l; l = l->next) {
				EABDestination *dest = eab_destination_import (l->data);
				if (dest) {
					const char *textrep = eab_destination_get_textrep (dest, TRUE);
					char *html = e_text_to_html (textrep, 0);
					gtk_html_stream_printf (html_stream, "%s, ", html);
					g_free (html);
					g_object_unref (dest);
				}
			}
			gtk_html_stream_printf (html_stream, "</td></tr></table>");
		}
		else {
			gboolean comma = FALSE;
			str = e_contact_get_const (contact, E_CONTACT_TITLE);
			if (str) {
				html = e_text_to_html (str, 0);
				gtk_html_stream_printf (html_stream, "<b>%s:</b> %s<br>", _("Job Title"), str);
				g_free (html);
			}

			gtk_html_stream_printf (html_stream, "<b>%s:</b> ", _("Email"));
			str = e_contact_get_const (contact, E_CONTACT_EMAIL_1);
			if (str) {
				html = e_text_to_html (str, 0);
				gtk_html_stream_printf (html_stream, "%s", str);
				g_free (html);
				comma = TRUE;
			}
			str = e_contact_get_const (contact, E_CONTACT_EMAIL_2);
			if (str) {
				html = e_text_to_html (str, 0);
				gtk_html_stream_printf (html_stream, "%s%s", comma ? ", " : "", str);
				g_free (html);
				comma = TRUE;
			}
			str = e_contact_get_const (contact, E_CONTACT_EMAIL_3);
			if (str) {
				html = e_text_to_html (str, 0);
				gtk_html_stream_printf (html_stream, "%s%s", comma ? ", " : "", str);
				g_free (html);
			}
			gtk_html_stream_write (html_stream, "<br>", 4);
			
			str = e_contact_get_const (contact, E_CONTACT_HOMEPAGE_URL);
			if (str) {
				html = e_text_to_html (str, E_TEXT_TO_HTML_CONVERT_URLS);
				gtk_html_stream_printf (html_stream, "<b>%s:</b> %s<br>",
							_("Home page"), html);
				g_free (html);
			}

			str = e_contact_get_const (contact, E_CONTACT_BLOG_URL);
			if (str) {
				html = e_text_to_html (str, E_TEXT_TO_HTML_CONVERT_URLS);
				gtk_html_stream_printf (html_stream, "<b>%s:</b> %s<br>",
							_("Blog"), html);
			}
		}

		gtk_html_stream_printf (html_stream, "</td></tr></table></td></tr></table></td></tr></table>\n");
	}

	gtk_html_stream_write (html_stream, "</body></html>\n", 15);
	gtk_html_end (display->priv->html, html_stream, GTK_HTML_STREAM_OK);
}

void
eab_contact_display_render (EABContactDisplay *display, EContact *contact,
			    EABContactDisplayRenderMode mode)
{
	switch (mode) {
	case EAB_CONTACT_DISPLAY_RENDER_NORMAL:
		eab_contact_display_render_normal (display, contact);
		break;
	case EAB_CONTACT_DISPLAY_RENDER_COMPACT:
		eab_contact_display_render_compact (display, contact);
		break;
	}
}

GtkWidget*
eab_contact_display_new (void)
{
	EABContactDisplay *display;

	display = g_object_new (EAB_TYPE_CONTACT_DISPLAY, NULL);
	
	display->priv = g_new0 (EABContactDisplayPrivate, 1);

	display->priv->html = GTK_HTML (gtk_html_new ());
	
	gtk_html_set_default_content_type (display->priv->html, "text/html; charset=utf-8");
	
	gtk_html_set_editable (display->priv->html, FALSE);


	g_signal_connect (display->priv->html, "url_requested",
			  G_CALLBACK (on_url_requested),
			  display);
	g_signal_connect (display->priv->html, "link_clicked",
			  G_CALLBACK (on_link_clicked),
			  display);
#if 0
	g_signal_connect (display->priv->html, "object_requested",
			  G_CALLBACK (on_object_requested),
			  mail_display);
	g_signal_connect (display->priv->html, "button_press_event",
			  G_CALLBACK (html_button_press_event), mail_display);
	g_signal_connect (display->priv->html, "motion_notify_event",
			  G_CALLBACK (html_motion_notify_event), mail_display);
	g_signal_connect (display->priv->html, "enter_notify_event",
			  G_CALLBACK (html_enter_notify_event), mail_display);
	g_signal_connect (display->priv->html, "iframe_created",
			  G_CALLBACK (html_iframe_created), mail_display);
	g_signal_connect (display->priv->html, "on_url",
			  G_CALLBACK (html_on_url), mail_display);
#endif

	gtk_box_pack_start_defaults (GTK_BOX (display), GTK_WIDGET (display->priv->html));
	gtk_widget_show (GTK_WIDGET (display->priv->html));

	return GTK_WIDGET (display);
}


static void
eab_contact_display_init (GObject *object)
{
}

static void
eab_contact_display_class_init (GtkObjectClass *object_class)
{
	//	object_class->destroy = mail_display_destroy;
}

GType
eab_contact_display_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EABContactDisplayClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_contact_display_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABContactDisplay),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_contact_display_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EABContactDisplay", &info, 0);
	}

	return type;
}
