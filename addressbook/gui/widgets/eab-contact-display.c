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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eab-contact-display.h"
#include "eab-popup.h"

#include "eab-gui-util.h"
#include "e-util/e-util.h"
#include "e-util/e-html-utils.h"
#include "e-util/e-icon-factory.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>

#define HANDLE_MAILTO_INTERNALLY 1

#define PARENT_TYPE (GTK_TYPE_HTML)

struct _EABContactDisplayPrivate {
	EContact *contact;

        GtkWidget *invisible;
	gchar *selection_uri;
};

static struct {
	const gchar *name;
	const gchar *pretty_name;
}
common_location [] =
{
	{ "WORK",  N_ ("Work")  },
	{ "HOME",  N_ ("Home")  },
	{ "OTHER", N_ ("Other") }
};

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n"  \
                    "<head>\n<meta name=\"generator\" content=\"Evolution Addressbook Component\">\n</head>\n"

#define HEADER_COLOR      "#7f7f7f"
#define IMAGE_COL_WIDTH   "20"
#define CONTACT_LIST_ICON "stock_contact-list"
#define AIM_ICON          "im-aim"
#define GROUPWISE_ICON    "im-nov"
#define ICQ_ICON          "im-icq"
#define JABBER_ICON       "im-jabber"
#define MSN_ICON          "im-msn"
#define YAHOO_ICON        "im-yahoo"
#define GADUGADU_ICON	    "im-gadugadu"
#define SKYPE_ICON	    "stock_people"
#define VIDEOCONF_ICON    "stock_video-conferencing"

#define MAX_COMPACT_IMAGE_DIMENSION 48

static void
eab_uri_popup_link_open(EPopup *ep, EPopupItem *item, gpointer data)
{
	EABPopupTargetURI *t = (EABPopupTargetURI *)ep->target;

	/* FIXME Pass a parent window. */
	e_show_uri (NULL, t->uri);
}

static void
eab_uri_popup_email_address_copy(EPopup *ep, EPopupItem *item, gpointer data)
{
	EABContactDisplay *display = data;
	struct _EABContactDisplayPrivate *p = display->priv;
        EABPopupTargetURI *t = (EABPopupTargetURI *)ep->target;
        const gchar *url = t->uri;
        gchar *html=NULL;
        gint i=0;
        GList *email_list, *l;
        gint email_num = atoi (url + strlen ("internal-mailto:"));

	email_list = e_contact_get (p->contact, E_CONTACT_EMAIL);
	for (l = email_list; l; l=l->next) {
		if (i==email_num)
			html = e_text_to_html (l->data, 0);
	i++;
        }

	g_free(p->selection_uri);
	p->selection_uri = g_strdup(html);
	g_free (html);

	gtk_selection_owner_set(p->invisible, GDK_SELECTION_PRIMARY, gtk_get_current_event_time());
	gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
}

static void
eab_uri_popup_link_copy(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EABContactDisplay *display = data;
	struct _EABContactDisplayPrivate *p = display->priv;

	g_free(p->selection_uri);
	p->selection_uri = g_strdup(pitem->user_data);

	gtk_selection_owner_set(p->invisible, GDK_SELECTION_PRIMARY, gtk_get_current_event_time());
	gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
}

static void
eab_uri_popup_address_send(EPopup *ep, EPopupItem *item, gpointer data)
{
         EABPopupTargetURI *t = (EABPopupTargetURI *)ep->target;
         const gchar *url = t->uri;
         EABContactDisplay *display = data;
	 struct _EABContactDisplayPrivate *p = display->priv;

	 gint mail_num = atoi (url + strlen ("internal-mailto:"));

         if (mail_num == -1)
         return;

         eab_send_contact (p->contact, mail_num, EAB_DISPOSITION_AS_TO);

}

static void
eab_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, EABContactDisplay *display)
{
	struct _EABContactDisplayPrivate *p = display->priv;

	if (p->selection_uri == NULL)
		return;

	gtk_selection_data_set(data, data->target, 8, (guchar *)p->selection_uri, strlen(p->selection_uri));
}

static void
eab_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, EABContactDisplay *display)
{
#if 0
	struct _EABContactDisplayPrivate *p = display->priv;

	g_free(p->selection_uri);
	p->selection_uri = NULL;
#endif
}

static EPopupItem eab_uri_popups[] = {
	{ E_POPUP_ITEM, (gchar *) "05.open", (gchar *) N_("_Open Link in Browser"), eab_uri_popup_link_open, NULL, NULL, EAB_POPUP_URI_NOT_MAILTO },
        { E_POPUP_ITEM, (gchar *) "10.copy", (gchar *) N_("_Copy Link Location"), eab_uri_popup_link_copy, NULL, (gchar *) "edit-copy", EAB_POPUP_URI_NOT_MAILTO },
        { E_POPUP_ITEM, (gchar *) "15.send", (gchar *) N_("_Send New Message To..."), eab_uri_popup_address_send, NULL, (gchar *) "mail-message-new", EAB_POPUP_URI_MAILTO},
	{ E_POPUP_ITEM, (gchar *) "20.copy", (gchar *) N_("Copy _Email Address"), eab_uri_popup_email_address_copy, NULL, (gchar *) "edit-copy", EAB_POPUP_URI_MAILTO},
        };

static void
eab_uri_popup_free(EPopup *ep, GSList *list, gpointer data)
{
	while (list) {
		GSList *n = list->next;
		struct _EPopupItem *item = list->data;

		g_free(item->user_data);
		item->user_data = NULL;
		g_slist_free_1(list);

		list = n;
		}
}

static gint
eab_uri_popup_event(EABContactDisplay *display, GdkEvent *event, const gchar *uri)
{
	EABPopup *emp;
	EABPopupTargetURI *t;
	GtkMenu *menu;
	GSList *menus = NULL;
	gint i;

	emp = eab_popup_new("org.gnome.evolution.addressbook.contactdisplay.popup");

	t = eab_popup_target_new_uri(emp, uri);
	t->target.widget = (GtkWidget *)display;

	for (i=0;i<sizeof(eab_uri_popups)/sizeof(eab_uri_popups[0]);i++) {
		eab_uri_popups[i].user_data = g_strdup(t->uri);
		menus = g_slist_prepend(menus, &eab_uri_popups[i]);
	}
	e_popup_add_items((EPopup *)emp, menus, NULL, eab_uri_popup_free, display);

        menu = e_popup_create_menu_once((EPopup *)emp,(EPopupTarget*)t, 0);

        if (event == NULL) {
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	} else {
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button.button, event->button.time);
	}

	return TRUE;
}

static void
on_url_requested (GtkHTML *html, const gchar *url, GtkHTMLStream *handle,
		  EABContactDisplay *display)
{
	if (!strcmp (url, "internal-contact-photo:")) {
		EContactPhoto *photo;

		photo = e_contact_get (display->priv->contact, E_CONTACT_PHOTO);
		if (!photo)
			photo = e_contact_get (display->priv->contact, E_CONTACT_LOGO);

		gtk_html_stream_write (handle, (gchar *)photo->data.inlined.data, photo->data.inlined.length);

		gtk_html_end (html, handle, GTK_HTML_STREAM_OK);

		e_contact_photo_free (photo);
	}
	else if (!strncmp (url, "evo-icon:", strlen ("evo-icon:"))) {
		gchar *data;
		gsize data_length;
		gchar *filename;

		filename = e_icon_factory_get_icon_filename (url + strlen ("evo-icon:"), GTK_ICON_SIZE_MENU);
		if (g_file_get_contents (filename, &data, &data_length, NULL)) {
			gtk_html_stream_write (handle, data, data_length);
			g_free (data);
		}

		gtk_html_stream_close (handle, GTK_HTML_STREAM_OK);

		g_free (filename);
	}
}

static void
on_link_clicked (GtkHTML *html, const gchar *uri, EABContactDisplay *display)
{
#ifdef HANDLE_MAILTO_INTERNALLY
	if (!strncmp (uri, "internal-mailto:", strlen ("internal-mailto:"))) {
		gint mail_num = atoi (uri + strlen ("internal-mailto:"));

		if (mail_num == -1)
			return;

		eab_send_contact (display->priv->contact, mail_num, EAB_DISPOSITION_AS_TO);

		return;
	}
#endif

	/* FIXME Pass a parent window. */
	e_show_uri (NULL, uri);
}

#if 0
static void
render_address (GtkHTMLStream *html_stream, EContact *contact, const gchar *html_label, EContactField adr_field, EContactField label_field)
{
	EContactAddress *adr;
	const gchar *label;

	label = e_contact_get_const (contact, label_field);
	if (label) {
		gchar *html = e_text_to_html (label, E_TEXT_TO_HTML_CONVERT_NL);

		gtk_html_stream_printf (html_stream, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td><td valign=\"top\" width=\"100\"><font color=" HEADER_COLOR ">%s:</font><br><a href=\"http://www.mapquest.com/\">%s</a></td><td valign=\"top\">%s</td></tr>", html_label, _("(map)"), html);

This shoul		g_free (html);
		return;
	}

	adr = e_contact_get (contact, adr_field);
	if (adr &&
	    (adr->po || adr->ext || adr->street || adr->locality || adr->region || adr->code || adr->country)) {

		gtk_html_stream_printf (html_stream, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td><td valign=\"top\" width=\"100\"><font color=" HEADER_COLOR ">%s:</font><br><a href=\"http://www.mapquest.com/\">%s</a></td><td valign=\"top\">", html_label, _("map"));

		if (adr->po && *adr->po) gtk_html_stream_printf (html_stream, "%s<br>", adr->po);
		if (adr->ext && *adr->ext) gtk_html_stream_printf (html_stream, "%s<br>", adr->ext);
		if (adr->street && *adr->street) gtk_html_stream_printf (html_stream, "%s<br>", adr->street);
		if (adr->locality && *adr->locality) gtk_html_stream_printf (html_stream, "%s<br>", adr->locality);
		if (adr->region && *adr->region) gtk_html_stream_printf (html_stream, "%s<br>", adr->region);
		if (adr->code && *adr->code) gtk_html_stream_printf (html_stream, "%s<br>", adr->code);
		if (adr->country && *adr->country) gtk_html_stream_printf (html_stream, "%s<br>", adr->country);

		gtk_html_stream_printf (html_stream, "</td></tr>");
	}
	if (adr)
		e_contact_address_free (adr);
}
#endif

static void
render_name_value (GtkHTMLStream *html_stream, const gchar *label, const gchar *str, const gchar *icon, guint html_flags)
{
	gchar *value = e_text_to_html (str, html_flags);

	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) {
		gtk_html_stream_printf (html_stream, "<tr><td align=\"right\" valign=\"top\">%s</td> <td align=\"right\" valign=\"top\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td>", value, label);
		gtk_html_stream_printf (html_stream, "<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">");
		if (icon)
			gtk_html_stream_printf (html_stream, "<img width=\"16\" height=\"16\" src=\"evo-icon:%s\"></td></tr>", icon);
		else
			gtk_html_stream_printf (html_stream, "</td></tr>");
	} else {
		gtk_html_stream_printf (html_stream, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">");
		if (icon)
			gtk_html_stream_printf (html_stream, "<img width=\"16\" height=\"16\" src=\"evo-icon:%s\">", icon);
		gtk_html_stream_printf (html_stream, "</td><td valign=\"top\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td> <td valign=\"top\">%s</td></tr>", label, value);
	}

	g_free (value);
}

static void
render_attribute (GtkHTMLStream *html_stream, EContact *contact, const gchar *html_label, EContactField field, const gchar *icon, guint html_flags)
{
	const gchar *str;

	str = e_contact_get_const (contact, field);

	if (str && *str) {
		render_name_value (html_stream, html_label, str, icon, html_flags);
	}
}

static void
accum_address (GString *gstr, EContact *contact, const gchar *html_label, EContactField adr_field, EContactField label_field)
{
	EContactAddress *adr;
	const gchar *label;
	gboolean is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

	label = e_contact_get_const (contact, label_field);
	if (label) {
		gchar *html = e_text_to_html (label, E_TEXT_TO_HTML_CONVERT_NL);

#ifdef mapping_works
		if (is_rtl)
			g_string_append_printf (gstr, "<tr><td align=\"right\" valign=\"top\">%s</td><td valign=\"top\" width=\"100\" align=\"right\"><font color=" HEADER_COLOR ">%s:</font><br><a href=\"http://www.mapquest.com/\">%s</a></td><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td></tr>", html, html_label, _("(map)"));
		else
			g_string_append_printf (gstr, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td><td valign=\"top\" width=\"100\"><font color=" HEADER_COLOR ">%s:</font><br><a href=\"http://www.mapquest.com/\">%s</a></td><td valign=\"top\">%s</td></tr>", html_label, _("(map)"), html);
#else
		if (is_rtl)
			g_string_append_printf (gstr, "<tr><td align=\"right\" valign=\"top\">%s</td><td valign=\"top\" width=\"100\" align=\"right\"><font color=" HEADER_COLOR ">%s:</font></td><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td></tr>", html, html_label);
		else
			g_string_append_printf (gstr, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td><td valign=\"top\" width=\"100\"><font color=" HEADER_COLOR ">%s:</font></td><td valign=\"top\">%s</td></tr>", html_label, html);
#endif

		g_free (html);
		return;
	}

	adr = e_contact_get (contact, adr_field);
	if (adr &&
	    (adr->po || adr->ext || adr->street || adr->locality || adr->region || adr->code || adr->country)) {
		if (is_rtl)
			g_string_append_printf (gstr, "<tr><td align=\"right\" valign=\"top\">");
		else
			g_string_append_printf (gstr, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td><td valign=\"top\" width=\"100\"><font color=" HEADER_COLOR ">%s:</font><br><a href=\"http://www.mapquest.com/\">%s</a></td><td valign=\"top\">", html_label, _("map"));

		if (adr->po && *adr->po) g_string_append_printf (gstr, "%s<br>", adr->po);
		if (adr->ext && *adr->ext) g_string_append_printf (gstr, "%s<br>", adr->ext);
		if (adr->street && *adr->street) g_string_append_printf (gstr, "%s<br>", adr->street);
		if (adr->locality && *adr->locality) g_string_append_printf (gstr, "%s<br>", adr->locality);
		if (adr->region && *adr->region) g_string_append_printf (gstr, "%s<br>", adr->region);
		if (adr->code && *adr->code) g_string_append_printf (gstr, "%s<br>", adr->code);
		if (adr->country && *adr->country) g_string_append_printf (gstr, "%s<br>", adr->country);

		if (is_rtl)
			g_string_append_printf (gstr, "</td><td valign=\"top\" width=\"100\" align=\"right\"><font color=" HEADER_COLOR ">%s:</font><br><a href=\"http://www.mapquest.com/\">%s</a></td><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td></tr>", html_label, _("map"));
		else
			g_string_append_printf (gstr, "</td></tr>");
	}
	if (adr)
		e_contact_address_free (adr);
}

static void
accum_name_value (GString *gstr, const gchar *label, const gchar *str, const gchar *icon, guint html_flags)
{
	gchar *value = e_text_to_html (str, html_flags);

	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) {
		g_string_append_printf (gstr, "<tr><td valign=\"top\" align=\"right\">%s</td> <td align=\"right\" valign=\"top\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td>", value, label);
		g_string_append_printf (gstr, "<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">");
		if (icon)
			g_string_append_printf (gstr, "<img width=\"16\" height=\"16\" src=\"evo-icon:%s\"></td></tr>", icon);
		else
			g_string_append_printf (gstr, "</td></tr>");
	} else {
		g_string_append_printf (gstr, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">");
		if (icon)
			g_string_append_printf (gstr, "<img width=\"16\" height=\"16\" src=\"evo-icon:%s\">", icon);
		g_string_append_printf (gstr, "</td><td valign=\"top\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td> <td valign=\"top\">%s</td></tr>", label, value);
	}

	g_free (value);
}

static void
accum_attribute (GString *gstr, EContact *contact, const gchar *html_label, EContactField field, const gchar *icon, guint html_flags)
{
	const gchar *str;

	str = e_contact_get_const (contact, field);

	if (str && *str) {
		accum_name_value (gstr, html_label, str, icon, html_flags);
	}
}

static void
accum_time_attribute (GString *gstr, EContact *contact, const gchar *html_label, EContactField field, const gchar *icon, guint html_flags)
{
	EContactDate *date;
	GDate *gdate = NULL;
	gchar sdate[100];

	date = e_contact_get (contact, field);
	if (date) {
		gdate = g_date_new_dmy ( date->day,
					 date->month,
					 date->year );
		g_date_strftime (sdate, 100, "%x", gdate);
		g_date_free (gdate);
		accum_name_value (gstr, html_label, sdate, icon, html_flags);
		e_contact_date_free (date);
	}
}

static void
accum_multival_attribute (GString *gstr, EContact *contact, const gchar *html_label, EContactField field, const gchar *icon, guint html_flags)
{
	GList *val_list, *l;

	val_list = e_contact_get (contact, field);
	for (l = val_list; l; l = l->next) {
		const gchar *str = (const gchar *) l->data;
		accum_name_value (gstr, html_label, str, icon, html_flags);
	}
	g_list_foreach (val_list, (GFunc) g_free, NULL);
	g_list_free (val_list);
}

static void
render_contact_list (GtkHTMLStream *html_stream, EContact *contact)
{
	GList *email_list;
	GList *l;

	gtk_html_stream_printf (html_stream, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr>");
	gtk_html_stream_printf (html_stream, "<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">");
	gtk_html_stream_printf (html_stream, "<img width=\"16\" height=\"16\" src=\"evo-icon:" CONTACT_LIST_ICON "\">");
	gtk_html_stream_printf (html_stream, "</td><td valign=\"top\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td> <td valign=\"top\">", _("List Members"));

	email_list = e_contact_get (contact, E_CONTACT_EMAIL);
	for (l = email_list; l; l = l->next) {
		gchar *value;

		value = eab_parse_qp_email_to_html (l->data);

		if (!value)
			value = e_text_to_html (l->data, E_TEXT_TO_HTML_CONVERT_ADDRESSES);

		gtk_html_stream_printf (html_stream, "%s<br>", value);

		g_free (value);
	}

	gtk_html_stream_printf (html_stream, "</td></tr></table>");
}

static void
start_block (GtkHTMLStream *html_stream, const gchar *label)
{
	gtk_html_stream_printf (html_stream, "<tr><td height=\"20\" colspan=\"3\"><font color=" HEADER_COLOR "><b>%s</b></font></td></tr>", label);
}

static void
end_block (GtkHTMLStream *html_stream)
{
	gtk_html_stream_printf (html_stream, "<tr><td height=\"20\">&nbsp;</td></tr>");
}

static const gchar *
get_email_location (EVCardAttribute *attr)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (common_location); i++) {
		if (e_vcard_attribute_has_type (attr, common_location [i].name))
			return _(common_location [i].pretty_name);
	}

	return _("Other");
}

static void
render_contact (GtkHTMLStream *html_stream, EContact *contact)
{
	GString *accum;
	GList *email_list, *l, *email_attr_list, *al;
	gboolean is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);
#ifdef HANDLE_MAILTO_INTERNALLY
	gint email_num = 0;
#endif
	const gchar *nl;
	gchar *nick=NULL;

	gtk_html_stream_printf (html_stream, "<table border=\"0\">");

	accum = g_string_new ("");
	nl = "";

	start_block (html_stream, "");

	email_list = e_contact_get (contact, E_CONTACT_EMAIL);
	email_attr_list = e_contact_get_attributes (contact, E_CONTACT_EMAIL);

	for (l = email_list, al=email_attr_list; l && al; l = l->next, al = al->next) {
		gchar *html = NULL, *name = NULL, *mail = NULL;
		gchar *attr_str = (gchar *)get_email_location ((EVCardAttribute *) al->data);

#ifdef HANDLE_MAILTO_INTERNALLY
		if (!eab_parse_qp_email (l->data, &name, &mail))
			mail = e_text_to_html (l->data, 0);

		g_string_append_printf (accum, "%s%s%s<a href=\"internal-mailto:%d\">%s</a>%s <font color=" HEADER_COLOR ">(%s)</font>",
						nl,
						name ? name : "",
						name ? " &lt;" : "",
						email_num,
						mail,
						name ? "&gt;" : "",
						attr_str ? attr_str : "");
		email_num ++;
#else
		html = eab_parse_qp_email_to_html (l->data);

		if (!html)
			html = e_text_to_html (l->data, E_TEXT_TO_HTML_CONVERT_ADDRESSES);

		g_string_append_printf (accum, "%s%s <font color=" HEADER_COLOR ">(%s)</font>", nl, html, attr_str ? attr_str : "");
#endif
		nl = "<br>";

		g_free (html);
		g_free (name);
		g_free (mail);
	}
	g_list_foreach (email_list, (GFunc)g_free, NULL);
	g_list_free (email_list);

	if (accum->len) {

#ifdef HANDLE_MAILTO_INTERNALLY
		if (is_rtl) {
			gtk_html_stream_printf (html_stream,
					"<tr><td valign=\"top\" align=\"right\">%s</td> <td valign=\"top\" align=\"right\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td></tr>",
					accum->str, _("Email"));
		} else {
			gtk_html_stream_printf (html_stream, "<tr><td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">");
			gtk_html_stream_printf (html_stream,
					"</td><td valign=\"top\" width=\"100\" nowrap><font color=" HEADER_COLOR ">%s:</font></td> <td valign=\"top\">%s</td></tr>",
					_("Email"), accum->str);
		}
#else
		render_name_value (html_stream, _("Email"), accum->str, NULL,
				   E_TEXT_TO_HTML_CONVERT_ADDRESSES | E_TEXT_TO_HTML_CONVERT_NL);
#endif
	}

	g_string_assign (accum, "");
	nick = e_contact_get (contact, E_CONTACT_NICKNAME);
	if (nick && *nick) {
		accum_name_value (accum, _("Nickname"), nick, NULL, 0);
		if (accum->len > 0)
			gtk_html_stream_printf (html_stream, "%s", accum->str);
	}

	g_string_assign (accum, "");
	accum_multival_attribute (accum, contact, _("AIM"), E_CONTACT_IM_AIM, AIM_ICON, 0);
	accum_multival_attribute (accum, contact, _("GroupWise"), E_CONTACT_IM_GROUPWISE, GROUPWISE_ICON, 0);
	accum_multival_attribute (accum, contact, _("ICQ"), E_CONTACT_IM_ICQ, ICQ_ICON, 0);
	accum_multival_attribute (accum, contact, _("Jabber"), E_CONTACT_IM_JABBER, JABBER_ICON, 0);
	accum_multival_attribute (accum, contact, _("MSN"), E_CONTACT_IM_MSN, MSN_ICON, 0);
	accum_multival_attribute (accum, contact, _("Yahoo"), E_CONTACT_IM_YAHOO, YAHOO_ICON, 0);
	accum_multival_attribute (accum, contact, _("Gadu-Gadu"), E_CONTACT_IM_GADUGADU, GADUGADU_ICON, 0);
	accum_multival_attribute (accum, contact, _("Skype"), E_CONTACT_IM_SKYPE, SKYPE_ICON, 0);

	if (accum->len > 0)
		gtk_html_stream_printf (html_stream, "%s", accum->str);

	end_block (html_stream);

	g_string_assign (accum, "");

	accum_attribute (accum, contact, _("Company"), E_CONTACT_ORG, NULL, 0);
	accum_attribute (accum, contact, _("Department"), E_CONTACT_ORG_UNIT, NULL, 0);
	accum_attribute (accum, contact, _("Profession"), E_CONTACT_ROLE, NULL, 0);
	accum_attribute (accum, contact, _("Position"), E_CONTACT_TITLE, NULL, 0);
	accum_attribute (accum, contact, _("Manager"), E_CONTACT_MANAGER, NULL, 0);
	accum_attribute (accum, contact, _("Assistant"), E_CONTACT_ASSISTANT, NULL, 0);
	accum_attribute (accum, contact, _("Video Chat"), E_CONTACT_VIDEO_URL, VIDEOCONF_ICON, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Calendar"), E_CONTACT_CALENDAR_URI, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Free/Busy"), E_CONTACT_FREEBUSY_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Phone"), E_CONTACT_PHONE_BUSINESS, NULL, 0);
	accum_attribute (accum, contact, _("Fax"), E_CONTACT_PHONE_BUSINESS_FAX, NULL, 0);
	accum_address   (accum, contact, _("Address"), E_CONTACT_ADDRESS_WORK, E_CONTACT_ADDRESS_LABEL_WORK);

	if (accum->len > 0) {
		start_block (html_stream, _("Work"));
		gtk_html_stream_printf (html_stream, "%s", accum->str);
		end_block (html_stream);
	}

	g_string_assign (accum, "");

	accum_attribute (accum, contact, _("Home Page"), E_CONTACT_HOMEPAGE_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Web Log"), E_CONTACT_BLOG_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);

	accum_attribute (accum, contact, _("Phone"), E_CONTACT_PHONE_HOME, NULL, 0);
	accum_attribute (accum, contact, _("Mobile Phone"), E_CONTACT_PHONE_MOBILE, NULL, 0);
	accum_address   (accum, contact, _("Address"), E_CONTACT_ADDRESS_HOME, E_CONTACT_ADDRESS_LABEL_HOME);
	accum_time_attribute (accum, contact, _("Birthday"), E_CONTACT_BIRTH_DATE, NULL, 0);
	accum_time_attribute (accum, contact, _("Anniversary"), E_CONTACT_ANNIVERSARY, NULL, 0);
	accum_attribute (accum, contact, _("Spouse"), E_CONTACT_SPOUSE, NULL, 0);
	if (accum->len > 0) {
		start_block (html_stream, _("Personal"));
		gtk_html_stream_printf (html_stream, "%s", accum->str);
		end_block (html_stream);
	}

	start_block (html_stream, "");

	render_attribute (html_stream, contact, _("Note"), E_CONTACT_NOTE, NULL,
			  E_TEXT_TO_HTML_CONVERT_ADDRESSES | E_TEXT_TO_HTML_CONVERT_URLS | E_TEXT_TO_HTML_CONVERT_NL);
	end_block (html_stream);

	gtk_html_stream_printf (html_stream, "</table>");
}

static void
eab_contact_display_render_normal (EABContactDisplay *display, EContact *contact)
{
	GtkHTMLStream *html_stream;
	gboolean is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

	if (display->priv->contact)
		g_object_unref (display->priv->contact);
	display->priv->contact = contact;
	if (display->priv->contact)
		g_object_ref (display->priv->contact);

	html_stream = gtk_html_begin (GTK_HTML (display));
	gtk_html_stream_write (html_stream, HTML_HEADER, sizeof (HTML_HEADER) - 1);
	gtk_html_stream_printf (html_stream, "<body><table width=\"100%%\"><tr><td %s>\n", is_rtl ? " align=\"right\" " : "");

	if (contact) {
		const gchar *str;
		gchar *html;
		EContactPhoto *photo;

		gtk_html_stream_printf (html_stream, "<table cellspacing=\"20\" border=\"0\"><td %s valign=\"top\">", is_rtl ? " align=\"right\" " : "");
		photo = e_contact_get (contact, E_CONTACT_PHOTO);
		if (!photo)
			photo = e_contact_get (contact, E_CONTACT_LOGO);
		/* Only handle inlined photos for now */
		if (photo && photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
			gtk_html_stream_printf (html_stream, "<img border=\"1\" src=\"internal-contact-photo:\">");
			e_contact_photo_free (photo);
		}

		gtk_html_stream_printf (html_stream, "</td><td %s valign=\"top\">\n", is_rtl ? " align=\"right\" " : "");

		str = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		if (!str)
			str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);

		if (str) {
			html = e_text_to_html (str, 0);
#ifdef HANDLE_MAILTO_INTERNALLY
			if (e_contact_get (contact, E_CONTACT_IS_LIST))
				gtk_html_stream_printf (html_stream, "<h2><a href=\"internal-mailto:0\">%s</a></h2>", html);
			else
#endif
			gtk_html_stream_printf (html_stream, "<h2>%s</h2>", html);
			g_free (html);
		}

		if (e_contact_get (contact, E_CONTACT_IS_LIST))
			render_contact_list (html_stream, contact);
		else
			render_contact (html_stream, contact);

		gtk_html_stream_printf (html_stream, "</td></tr></table>\n");
	}

	gtk_html_stream_printf (html_stream, "</td></tr></table></body></html>\n");
	gtk_html_end (GTK_HTML (display), html_stream, GTK_HTML_STREAM_OK);
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

	html_stream = gtk_html_begin (GTK_HTML (display));
	gtk_html_stream_write (html_stream, HTML_HEADER, sizeof (HTML_HEADER) - 1);
	gtk_html_stream_write (html_stream, "<body>\n", 7);

	if (contact) {
		const gchar *str;
		gchar *html;
		EContactPhoto *photo;
		guint bg_frame = 0x000000, bg_body = 0xEEEEEE;
		GtkStyle *style;

		style = gtk_widget_get_style (GTK_WIDGET (display));
		if (style) {
			gushort r, g, b;

			r = style->black.red >> 8;
			g = style->black.green >> 8;
			b = style->black.blue >> 8;
			bg_frame = ((r << 16) | (g << 8) | b) & 0xffffff;

			#define DARKER(a) (((a) >= 0x22) ? ((a) - 0x22) : 0)
			r = DARKER (style->bg[GTK_STATE_NORMAL].red >> 8);
			g = DARKER (style->bg[GTK_STATE_NORMAL].green >> 8);
			b = DARKER (style->bg[GTK_STATE_NORMAL].blue >> 8);
			bg_body = ((r << 16) | (g << 8) | b) & 0xffffff;
			#undef DARKER
		}

		gtk_html_stream_printf (html_stream,
					"<table width=\"100%%\" cellpadding=1 cellspacing=0 bgcolor=\"#%06X\">"
					"<tr><td valign=\"top\">"
					"<table width=\"100%%\" cellpadding=0 cellspacing=0 bgcolor=\"#%06X\">"
					"<tr><td valign=\"top\">"
					"<table>"
					"<tr><td valign=\"top\">", bg_frame, bg_body);

		photo = e_contact_get (contact, E_CONTACT_PHOTO);
		if (!photo)
			photo = e_contact_get (contact, E_CONTACT_LOGO);
		if (photo) {
			gint calced_width = MAX_COMPACT_IMAGE_DIMENSION, calced_height = MAX_COMPACT_IMAGE_DIMENSION;
			GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
			GdkPixbuf *pixbuf;

			/* figure out if we need to downscale the
			   image here.  we don't scale the pixbuf
			   itself, just insert width/height tags in
			   the html */
			gdk_pixbuf_loader_write (loader, photo->data.inlined.data, photo->data.inlined.length, NULL);
			gdk_pixbuf_loader_close (loader, NULL);
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			if (pixbuf)
				g_object_ref (pixbuf);
			g_object_unref (loader);
			if (pixbuf) {
				gint max_dimension;

				calced_width = gdk_pixbuf_get_width (pixbuf);
				calced_height = gdk_pixbuf_get_height (pixbuf);

				max_dimension = calced_width;
				if (max_dimension < calced_height)
					max_dimension = calced_height;

				if (max_dimension > MAX_COMPACT_IMAGE_DIMENSION) {
					calced_width *= ((gfloat)MAX_COMPACT_IMAGE_DIMENSION / max_dimension);
					calced_height *= ((gfloat)MAX_COMPACT_IMAGE_DIMENSION / max_dimension);
				}
			}

			g_object_unref (pixbuf);
			gtk_html_stream_printf (html_stream, "<img width=\"%d\" height=\"%d\" src=\"internal-contact-photo:\">",
						calced_width, calced_height);
			e_contact_photo_free (photo);
		}

		gtk_html_stream_printf (html_stream, "</td><td valign=\"top\">\n");

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

			gtk_html_stream_printf (html_stream, "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\"><tr><td valign=\"top\">");
			gtk_html_stream_printf (html_stream, "<b>%s:</b>&nbsp;<td>", _("List Members"));

			email_list = e_contact_get (contact, E_CONTACT_EMAIL);

			for (l = email_list; l; l = l->next) {
				if (l->data) {
					html = e_text_to_html (l->data, 0);
					gtk_html_stream_printf (html_stream, "%s, ", html);
					g_free (html);
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

			#define print_email() {								\
				html = eab_parse_qp_email_to_html (str);				\
													\
				if (!html)								\
					html = e_text_to_html (str, 0);				\
													\
				gtk_html_stream_printf (html_stream, "%s%s", comma ? ", " : "", html);	\
				g_free (html);								\
				comma = TRUE;								\
			}

			gtk_html_stream_printf (html_stream, "<b>%s:</b> ", _("Email"));
			str = e_contact_get_const (contact, E_CONTACT_EMAIL_1);
			if (str)
				print_email ();

			str = e_contact_get_const (contact, E_CONTACT_EMAIL_2);
			if (str)
				print_email ();

			str = e_contact_get_const (contact, E_CONTACT_EMAIL_3);
			if (str)
				print_email ();

			gtk_html_stream_write (html_stream, "<br>", 4);

			#undef print_email

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
	gtk_html_end (GTK_HTML (display), html_stream, GTK_HTML_STREAM_OK);
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

static gint
eab_html_press_event (GtkWidget *widget, GdkEvent *event,EABContactDisplay *display)
{
	gchar *uri;
	gboolean res = FALSE;

	if (event->button.button!= 3 )
		return FALSE;

	uri = gtk_html_get_url_at (GTK_HTML (widget), event->button.x, event->button.y);
	if (uri) {
		eab_uri_popup_event(display,event,uri);
		}

         g_free(uri);

	return res;
}

GtkWidget*
eab_contact_display_new (void)
{
	EABContactDisplay *display;

	struct _EABContactDisplayPrivate *p;

	display = g_object_new (EAB_TYPE_CONTACT_DISPLAY, NULL);
	p=display->priv = g_new0 (EABContactDisplayPrivate, 1);

	gtk_html_set_default_content_type (GTK_HTML (display), "text/html; charset=utf-8");

	gtk_html_set_editable (GTK_HTML (display), FALSE);

	g_signal_connect (display, "url_requested",
			  G_CALLBACK (on_url_requested),
			  display);
	g_signal_connect (display, "link_clicked",
			  G_CALLBACK (on_link_clicked),
			  display);
        g_signal_connect(display, "button_press_event",
                          G_CALLBACK(eab_html_press_event),
                           display);
        p->invisible = gtk_invisible_new();
	g_signal_connect(p->invisible, "selection_get", G_CALLBACK(eab_selection_get), display);
	g_signal_connect(p->invisible, "selection_clear_event", G_CALLBACK(eab_selection_clear_event), display);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 0);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 1);

#if 0
	g_signal_connect (display, "object_requested",
			  G_CALLBACK (on_object_requested),
			  mail_display);
	g_signal_connect (display, "button_press_event",
			  G_CALLBACK (html_button_press_event), mail_display);
	g_signal_connect (display, "motion_notify_event",
			  G_CALLBACK (html_motion_notify_event), mail_display);
	g_signal_connect (display, "enter_notify_event",
			  G_CALLBACK (html_enter_notify_event), mail_display);
	g_signal_connect (display, "iframe_created",
			  G_CALLBACK (html_iframe_created), mail_display);
	g_signal_connect (display, "on_url",
			  G_CALLBACK (html_on_url), mail_display);
#endif

	return GTK_WIDGET (display);
}

static void
eab_contact_display_init (GObject *object)
{
	gtk_html_construct ((GtkHTML *)object);
}

static void
eab_contact_display_class_init (GtkObjectClass *object_class)
{
	/*	object_class->destroy = mail_display_destroy;*/
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
