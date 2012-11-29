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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eab-contact-formatter.h"

#include "eab-gui-util.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/e-html-utils.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-plugin-ui.h"

#ifdef WITH_CONTACT_MAPS
#include "widgets/misc/e-contact-map.h"
#endif

#include <string.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (
        EABContactFormatter,
        eab_contact_formatter,
        G_TYPE_OBJECT);

#define EAB_CONTACT_FORMATTER_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE \
        ((obj), EAB_TYPE_CONTACT_FORMATTER, EABContactFormatterPrivate))

#define TEXT_IS_RIGHT_TO_LEFT \
        (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)

enum {
	PROP_0,
        PROP_DISPLAY_MODE,
        PROP_RENDER_MAPS,
	PROP_STYLE,
	PROP_STATE
};

struct _EABContactFormatterPrivate {

        EContact *contact;

        EABContactDisplayMode mode;
        gboolean render_maps;

	GtkStyle *style;
	GtkStateType state;
};

static struct {
	const gchar *name;
	const gchar *pretty_name;
}
common_location[] =
{
	{ "WORK",  N_ ("Work")  },
	{ "HOME",  N_ ("Home")  },
	{ "OTHER", N_ ("Other") }
};

#define IMAGE_COL_WIDTH   "20"
#define CONTACT_LIST_ICON "stock_contact-list"
#define AIM_ICON          "im-aim"
#define GROUPWISE_ICON    "im-nov"
#define ICQ_ICON          "im-icq"
#define JABBER_ICON       "im-jabber"
#define MSN_ICON          "im-msn"
#define YAHOO_ICON        "im-yahoo"
#define GADUGADU_ICON	  "im-gadugadu"
#define SKYPE_ICON	  "stock_people"
#define TWITTER_ICON	  "im-twitter"
#define VIDEOCONF_ICON    "stock_video-conferencing"

#define MAX_COMPACT_IMAGE_DIMENSION 48

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n"  \
"<head>\n<meta name=\"generator\" content=\"Evolution Addressbook Component\">\n" \
"<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview.css\">" \
"<style type=\"text/css\">\n" \
"  div#header { width:100%; clear: both; }\n" \
"  div#columns { width: 100%; clear: both; }\n" \
"  div#footer { width: 100%; clear: both; }\n" \
"  div.column { width: auto; float: left; margin-right: 15px; }\n" \
"  img#contact-photo { float: left; }\n" \
"  div#contact-name { float: left; margin-left: 20px; }\n" \
"</style>\n" \
"</head>\n"

static gboolean
icon_available (const gchar *icon)
{
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;

	if (!icon)
		return FALSE;

	icon_theme = gtk_icon_theme_get_default ();
	icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon, 16, 0);
	if (icon_info != NULL)
		gtk_icon_info_free (icon_info);

	return icon_info != NULL;
}

static void
render_address_link (GString *buffer,
                     EContact *contact,
                     gint map_type)
{
	EContactAddress *adr;
	GString *link = g_string_new ("");

	adr = e_contact_get (contact, map_type);
	if (adr &&
	    (adr->street || adr->locality || adr->region || adr->country)) {
		gchar *escaped;

		if (adr->street && *adr->street)
			g_string_append_printf (link, "%s, ", adr->street);

		if (adr->locality && *adr->locality)
			g_string_append_printf (link, "%s, ", adr->locality);

		if (adr->region && *adr->region)
			g_string_append_printf (link, "%s, ", adr->region);

		if (adr->country && *adr->country)
			g_string_append_printf (link, "%s", adr->country);

		escaped = g_uri_escape_string (link->str, NULL, TRUE);
		g_string_assign (link, escaped);
		g_free (escaped);

		g_string_prepend (link, "<a href=\"http://maps.google.com?q=");
		g_string_append_printf (link, "\">%s</a>", _("Open map"));
	}

	if (adr)
		e_contact_address_free (adr);

	g_string_append (buffer, link->str);
	g_string_free (link, TRUE);
}

static void
accum_address (GString *buffer,
               EContact *contact,
               const gchar *html_label,
               EContactField adr_field,
               EContactField label_field)
{
	EContactAddress *adr;
	const gchar *label;
	GString *map_link = g_string_new ("<br>");

	render_address_link (map_link, contact, adr_field);

	label = e_contact_get_const (contact, label_field);
	if (label) {
		gchar *html = e_text_to_html (label, E_TEXT_TO_HTML_CONVERT_NL);

		if (TEXT_IS_RIGHT_TO_LEFT) {
			g_string_append_printf (
				buffer,
				"<tr>"
				"<td align=\"right\" valign=\"top\" nowrap>%s</td>"
				"<th>%s:<br>%s</th>"
				"<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td>"
				"</tr>",
				html, html_label, map_link->str);
		} else {
			g_string_append_printf (
				buffer,
				"<tr>"
				"<td width=\"" IMAGE_COL_WIDTH "\"></td>"
				"<th>%s:<br>%s</th>"
				"<td valign=\"top\" nowrap>%s</td>"
				"</tr>",
				html_label, map_link->str, html);
		}

		g_free (html);
		g_string_free (map_link, TRUE);
		return;
	}

	adr = e_contact_get (contact, adr_field);
	if (adr &&
	   (adr->po || adr->ext || adr->street || adr->locality ||
	    adr->region || adr->code || adr->country)) {

		if (TEXT_IS_RIGHT_TO_LEFT) {
			g_string_append_printf (
				buffer, "<tr><td align=\"right\" valign=\"top\" nowrap>");
		} else {
			g_string_append_printf (
				buffer,
				"<tr>"
				"<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\"></td>"
				"<th>%s:<br>%s</th>"
				"<td valign=\"top\" nowrap>",
				html_label, map_link->str);
		}

		if (adr->po && *adr->po)
			g_string_append_printf (buffer, "%s<br>", adr->po);

		if (adr->ext && *adr->ext)
			g_string_append_printf (buffer, "%s<br>", adr->ext);

		if (adr->street && *adr->street)
			g_string_append_printf (buffer, "%s<br>", adr->street);

		if (adr->locality && *adr->locality)
			g_string_append_printf (buffer, "%s<br>", adr->locality);

		if (adr->region && *adr->region)
			g_string_append_printf (buffer, "%s<br>", adr->region);

		if (adr->code && *adr->code)
			g_string_append_printf (buffer, "%s<br>", adr->code);

		if (adr->country && *adr->country)
			g_string_append_printf (buffer, "%s<br>", adr->country);

		if (TEXT_IS_RIGHT_TO_LEFT) {
			g_string_append_printf (
				buffer,
				"</td><th%s:<br>%s</th>"
				"<td width=\"" IMAGE_COL_WIDTH "\"></td>"
				"</tr>", html_label, map_link->str);
		} else {
			g_string_append_printf (buffer, "</td></tr>");
		}

	}

	if (adr)
		e_contact_address_free (adr);

	g_string_free (map_link, TRUE);
}

static void
render_table_row (GString *buffer,
                  const gchar *label,
                  const gchar *str,
                  const gchar *icon,
                  guint html_flags)
{
	const gchar *icon_html;
	gchar *value;

	if (html_flags)
		value = e_text_to_html (str, html_flags);
	else
		value = (gchar *) str;

	if (icon && icon_available (icon)) {
		icon_html = g_strdup_printf ("<img src=\"gtk-stock://%s\" width=\"16\" height=\"16\" />", icon);
	} else {
		icon_html = "";
	}

	if (TEXT_IS_RIGHT_TO_LEFT) {
		g_string_append_printf (
			buffer, "<tr>"
			"<td valign=\"top\" align=\"right\">%s</td>"
			"<th align=\"right\" valign=\"top\" width=\"100\" nowrap>:%s</th>"
			"<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">%s</td>"
			"</tr>",
			value, label, icon_html);
	} else {
		g_string_append_printf (
			buffer, "<tr>"
			"<td valign=\"top\" width=\"" IMAGE_COL_WIDTH "\">%s</td>"
			"<th valign=\"top\" width=\"100\" nowrap>%s:</th>"
			"<td valign=\"top\">%s</td>"
			"</tr>",
			icon_html, label, value);
	}

	if (html_flags)
		g_free (value);
}

static void
accum_attribute (GString *buffer,
                 EContact *contact,
                 const gchar *html_label,
                 EContactField field,
                 const gchar *icon,
                 guint html_flags)
{
	const gchar *str;

	str = e_contact_get_const (contact, field);

	if (str != NULL && *str != '\0')
		render_table_row (buffer, html_label, str, icon, html_flags);
}

static void
accum_time_attribute (GString *buffer,
                      EContact *contact,
                      const gchar *html_label,
                      EContactField field,
                      const gchar *icon,
                      guint html_flags)
{
	EContactDate *date;
	GDate *gdate = NULL;
	gchar sdate[100];

	date = e_contact_get (contact, field);
	if (date) {
		gdate = g_date_new_dmy (
			date->day,
			date->month,
			date->year);
		g_date_strftime (sdate, 100, "%x", gdate);
		g_date_free (gdate);
		render_table_row (buffer, html_label, sdate, icon, html_flags);
		e_contact_date_free (date);
	}
}

static void
accum_attribute_multival (GString *buffer,
                          EContact *contact,
                          const gchar *html_label,
                          EContactField field,
                          const gchar *icon,
                          guint html_flags)
{
	GList *val_list, *l;
	GString *val = g_string_new ("");

	val_list = e_contact_get (contact, field);

	for (l = val_list; l; l = l->next) {
		if (l != val_list)
			g_string_append (val, "<br>");

		g_string_append (val, l->data);
	}

	if (val->str && *val->str)
		render_table_row (buffer, html_label, val->str, icon, html_flags);

	g_string_free (val, TRUE);
	g_list_foreach (val_list, (GFunc) g_free, NULL);
	g_list_free (val_list);
}

static const gchar *
get_email_location (EVCardAttribute *attr)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (common_location); i++) {
		if (e_vcard_attribute_has_type (attr, common_location[i].name))
			return _(common_location[i].pretty_name);
	}

	return _("Other");
}

static void
render_title_block (EABContactFormatter *formatter,
                    GString *buffer)
{
	const gchar *str;
	gchar *html;
	EContactPhoto *photo;
	EContact *contact;

	contact = formatter->priv->contact;

	g_string_append_printf (
		buffer,
		"<table border=\"0\"><tr>"
		"<td %s valign=\"middle\">", TEXT_IS_RIGHT_TO_LEFT ?
		"align=\"right\"" : "");

	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	if (!photo)
		photo = e_contact_get (contact, E_CONTACT_LOGO);

	if (photo && photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
		gchar *photo_data;
		photo_data = g_base64_encode (
				photo->data.inlined.data,
				photo->data.inlined.length);
		g_string_append_printf (
			buffer, "<img border=\"1\" src=\"data:%s;base64,%s\">",
			photo->data.inlined.mime_type,
			photo_data);
	} else if (photo && photo->type == E_CONTACT_PHOTO_TYPE_URI && photo->data.uri && *photo->data.uri) {
		gboolean is_local = g_str_has_prefix (photo->data.uri, "file://");
		gchar *unescaped = g_uri_unescape_string (photo->data.uri, NULL);
		g_string_append_printf (
			buffer, "<img border=\"1\" src=\"%s%s\">",
			is_local ? "evo-" : "", unescaped);
		g_free (unescaped);
	}

	if (photo)
		e_contact_photo_free (photo);

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		g_string_append_printf (buffer, "<img src=\"gtk-stock://%s\">", CONTACT_LIST_ICON);
	}

	g_string_append_printf (
		buffer,
		"</td><td width=\"20\"></td><td %s valign=\"top\">\n",
		TEXT_IS_RIGHT_TO_LEFT ? "align=\"right\"" : "");

	str = e_contact_get_const (contact, E_CONTACT_FILE_AS);
	if (!str)
		str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);

	if (str) {
		html = e_text_to_html (str, 0);
		if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
			g_string_append_printf (
				buffer,
				"<h2><a href=\"internal-mailto:0\">%s</a></h2>",
				html);
		} else {
			g_string_append_printf (buffer, "<h2>%s</h2>", html);
		}
		g_free (html);
	}

	g_string_append (buffer, "</td></tr></table>");
}

static void
render_contact_list_row (EABContactFormatter *formatter,
                         EDestination *destination,
                         GString *buffer)
{
	gchar *evolution_imagesdir;
	gboolean list_collapsed = FALSE;
	const gchar *textrep;
	gchar *name = NULL, *email_addr = NULL;

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);

	textrep = e_destination_get_textrep (destination, TRUE);
	if (!eab_parse_qp_email (textrep, &name, &email_addr))
		email_addr = g_strdup (textrep);

	g_string_append (buffer, "<tr>");
	if (e_destination_is_evolution_list (destination)) {
		g_string_append_printf (
			buffer,
			"<td width=" IMAGE_COL_WIDTH " valign=\"top\" align=\"left\">"
			"<img src=\"evo-file://%s/minus.png\" "
			"id=\"%s\" "
			"class=\"navigable _evo_collapse_button\">"
			"</td><td width=\"100%%\" align=\"left\">%s",
			evolution_imagesdir,
			e_destination_get_contact_uid (destination),
			name ? name : email_addr);

		if (!list_collapsed) {
			const GList *dest, *dests;
			g_string_append_printf (
				buffer,
				"<br><table cellspacing=\"1\" id=\"list-%s\">",
				e_destination_get_contact_uid (destination));

			dests = e_destination_list_get_root_dests (destination);
			for (dest = dests; dest; dest = dest->next) {
				render_contact_list_row (
					formatter, dest->data, buffer);
			}

			g_string_append (buffer, "</table>");
		}

		g_string_append (buffer, "</td>");

	} else {
		if (name && *name) {
			g_string_append_printf (
				buffer,
				"<td colspan=\"2\">%s &lt"
				"<a href=\"mailto:%s\">%s</a>&gt;"
				"</td>",
				name, email_addr, email_addr);
		} else {
			g_string_append_printf (
				buffer,
				"<td colspan=\"2\">"
				"<a href=\"mailto:%s\">%s</a>"
				"</td>",
				email_addr, email_addr);
		}
	}

	g_string_append (buffer, "</tr>");

	g_free (evolution_imagesdir);
	g_free (name);
	g_free (email_addr);
}

static void
render_contact_list (EABContactFormatter *formatter,
                     GString *buffer)
{
	EContact *contact;
	EDestination *destination;
	const GList *dest, *dests;

	contact = formatter->priv->contact;

	destination = e_destination_new ();
	e_destination_set_contact (destination, contact, 0);
	dests = e_destination_list_get_root_dests (destination);

	render_title_block (formatter, buffer);

	g_string_append_printf (
		buffer,
		"<table border=\"0\"><tr><th colspan=\"2\">%s</th></tr>"
		"<tr><td with=" IMAGE_COL_WIDTH "></td><td>", _("List Members:"));

	g_string_append (buffer, "<table border=\"0\" cellspacing=\"1\">");

	for (dest = dests; dest; dest = dest->next)
		render_contact_list_row (formatter, dest->data, buffer);

	g_string_append (buffer, "</table>");
	g_string_append (buffer, "</td></tr></table>");

	g_object_unref (destination);
}

static void
render_contact_column (EABContactFormatter *formatter,
                       GString *buffer)
{
	EContact *contact;
	GString *accum, *email;
	GList *email_list, *l, *email_attr_list, *al;
	gint email_num = 0;
	const gchar *nl;

	contact = formatter->priv->contact;
	email = g_string_new ("");
	nl = "";

	email_list = e_contact_get (contact, E_CONTACT_EMAIL);
	email_attr_list = e_contact_get_attributes (contact, E_CONTACT_EMAIL);

	for (l = email_list, al = email_attr_list; l && al; l = l->next, al = al->next) {
		gchar *name = NULL, *mail = NULL;
		gchar *attr_str = (gchar *) get_email_location ((EVCardAttribute *) al->data);

		if (!eab_parse_qp_email (l->data, &name, &mail))
			mail = e_text_to_html (l->data, 0);

		g_string_append_printf (
			email,
			"%s%s%s<a href=\"internal-mailto:%d\">%s</a>%s "
			"<span class=\"header\">(%s)</span>",
			nl,
			name ? name : "",
			name ? " &lt;" : "",
			email_num,
			mail,
			name ? "&gt;" : "",
			attr_str ? attr_str : "");
		email_num++;
		nl = "<br>";

		g_free (name);
		g_free (mail);
	}
	g_list_foreach (email_list, (GFunc) g_free, NULL);
	g_list_foreach (email_attr_list, (GFunc) e_vcard_attribute_free, NULL);
	g_list_free (email_list);
	g_list_free (email_attr_list);

	accum = g_string_new ("");

	if (email->len)
		render_table_row (accum, _("Email"), email->str, NULL, 0);

	accum_attribute (accum, contact, _("Nickname"), E_CONTACT_NICKNAME, NULL, 0);
	accum_attribute_multival (accum, contact, _("AIM"), E_CONTACT_IM_AIM, AIM_ICON, 0);
	accum_attribute_multival (accum, contact, _("GroupWise"), E_CONTACT_IM_GROUPWISE, GROUPWISE_ICON, 0);
	accum_attribute_multival (accum, contact, _("ICQ"), E_CONTACT_IM_ICQ, ICQ_ICON, 0);
	accum_attribute_multival (accum, contact, _("Jabber"), E_CONTACT_IM_JABBER, JABBER_ICON, 0);
	accum_attribute_multival (accum, contact, _("MSN"), E_CONTACT_IM_MSN, MSN_ICON, 0);
	accum_attribute_multival (accum, contact, _("Yahoo"), E_CONTACT_IM_YAHOO, YAHOO_ICON, 0);
	accum_attribute_multival (accum, contact, _("Gadu-Gadu"), E_CONTACT_IM_GADUGADU, GADUGADU_ICON, 0);
	accum_attribute_multival (accum, contact, _("Skype"), E_CONTACT_IM_SKYPE, SKYPE_ICON, 0);
	accum_attribute_multival (accum, contact, _("Twitter"), E_CONTACT_IM_TWITTER, TWITTER_ICON, 0);

	if (accum->len)
		g_string_append_printf (
			buffer,
			"<div class=\"column\" id=\"contact-internet\">"
			"<table border=\"0\" cellspacing=\"5\">%s</table>"
			"</div>", accum->str);

	g_string_free (accum, TRUE);
	g_string_free (email, TRUE);
}

static void
accum_address_map (GString *buffer,
                   EContact *contact,
                   gint map_type)
{
        #ifdef WITH_CONTACT_MAPS

	g_string_append (buffer, "<tr><td colspan=\"3\">");

	if (map_type == E_CONTACT_ADDRESS_WORK) {
		g_string_append (
			buffer,
			"<object type=\"application/x-work-map-widget\" "
			"width=\"250\" height=\"250\"></object>");
	} else {
		g_string_append (
			buffer,
			"<object type=\"application/x-home-map-widget\" "
			"width=\"250\" height=\"250\"></object>");
	}

	g_string_append (buffer, "</td></tr>");

        #endif
}

static void
render_work_column (EABContactFormatter *formatter,
                    GString *buffer)
{
	EContact *contact = formatter->priv->contact;
	GString *accum = g_string_new ("");

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
	if (formatter->priv->render_maps)
		accum_address_map (accum, contact, E_CONTACT_ADDRESS_WORK);

	if (accum->len > 0) {
		g_string_append_printf (
			buffer,
			"<div class=\"column\" id=\"contact-work\">"
			"<h3>%s</h3>"
			"<table border=\"0\" cellspacing=\"5\">%s</table>"
			"</div>", _("Work"), accum->str);
	}

	g_string_free (accum, TRUE);
}

static void
render_personal_column (EABContactFormatter *formatter,
                        GString *buffer)
{
	EContact *contact = formatter->priv->contact;
	GString *accum = g_string_new ("");

	accum_attribute (accum, contact, _("Home Page"), E_CONTACT_HOMEPAGE_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Web Log"), E_CONTACT_BLOG_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Phone"), E_CONTACT_PHONE_HOME, NULL, 0);
	accum_attribute (accum, contact, _("Mobile Phone"), E_CONTACT_PHONE_MOBILE, NULL, 0);
	accum_address   (accum, contact, _("Address"), E_CONTACT_ADDRESS_HOME, E_CONTACT_ADDRESS_LABEL_HOME);
	accum_time_attribute (accum, contact, _("Birthday"), E_CONTACT_BIRTH_DATE, NULL, 0);
	accum_time_attribute (accum, contact, _("Anniversary"), E_CONTACT_ANNIVERSARY, NULL, 0);
	accum_attribute (accum, contact, _("Spouse"), E_CONTACT_SPOUSE, NULL, 0);
	if (formatter->priv->render_maps)
		accum_address_map (accum, contact, E_CONTACT_ADDRESS_HOME);

	if (accum->len > 0) {
		g_string_append_printf (
			buffer,
			"<div class=\"column\" id=\"contact-personal\">"
			"<h3>%s</h3>"
			"<table border=\"0\" cellspacing=\"5\">%s</table>"
			"</div>", _("Personal"), accum->str);
	}

	g_string_free (accum, TRUE);
}

static void
render_footer (EABContactFormatter *formatter,
               GString *buffer)
{
	EContact *contact;
	const gchar *str;

	contact = formatter->priv->contact;

	str = e_contact_get_const (contact, E_CONTACT_NOTE);
	if (!str || !*str)
		return;

	g_string_append (
		buffer,
		"<div id=\"footer\"><table border=\"0\" cellspacing=\"5\">");

	render_table_row (
		buffer, _("Note"),
		e_contact_get_const (contact, E_CONTACT_NOTE),
		NULL,
		E_TEXT_TO_HTML_CONVERT_ADDRESSES |
		E_TEXT_TO_HTML_CONVERT_URLS |
		E_TEXT_TO_HTML_CONVERT_NL);

	g_string_append (buffer, "</table></div>");
}

static void
render_contact (EABContactFormatter *formatter,
                GString *buffer)
{
	render_title_block (formatter, buffer);

	g_string_append (buffer, "<div id=\"columns\">");
	render_contact_column (formatter, buffer);
	render_work_column (formatter, buffer);
	render_personal_column (formatter, buffer);
	g_string_append (buffer, "</div>");

	render_footer (formatter, buffer);
}

static void
render_normal (EABContactFormatter *formatter,
               GString *buffer)
{
	g_string_append (buffer, HTML_HEADER);
	g_string_append_printf (
		buffer, "<body bgcolor=\"#%06x\" text=\"#%06x\">",
		e_color_to_value (
			&formatter->priv->style->base[formatter->priv->state]),
		e_color_to_value (
			&formatter->priv->style->text[formatter->priv->state]));

	if (formatter->priv->contact) {

		if (e_contact_get (formatter->priv->contact, E_CONTACT_IS_LIST))

			render_contact_list (
				formatter,
				buffer);
		else
			render_contact (
				formatter,
				buffer);

	}

	g_string_append (buffer, "</body></html>\n");
}

static void
render_compact (EABContactFormatter *formatter,
                GString *buffer)
{
	EContact *contact = formatter->priv->contact;
	const gchar *str;
	gchar *html;
	EContactPhoto *photo;

	g_string_append (buffer, HTML_HEADER);
	g_string_append (buffer, "<body>\n");

	if (!contact) {
		g_string_append (buffer, "</body></html>");
		return;
	}

	g_string_append_printf (
		buffer,
		"<table><tr><td valign=\"top\">");

	photo = e_contact_get (contact, E_CONTACT_PHOTO);

	if (!photo)
		photo = e_contact_get (contact, E_CONTACT_LOGO);

	if (photo) {
		gint calced_width = MAX_COMPACT_IMAGE_DIMENSION, calced_height = MAX_COMPACT_IMAGE_DIMENSION;
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
		GdkPixbuf *pixbuf;

		/* figure out if we need to downscale the
		 * image here.  we don't scale the pixbuf
		 * itself, just insert width/height tags in
		 * the html */
		if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
			gdk_pixbuf_loader_write (
				loader, photo->data.inlined.data,
				photo->data.inlined.length, NULL);
		} else if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
				photo->data.uri &&
				g_ascii_strncasecmp (photo->data.uri, "file://", 7) == 0) {
			gchar *filename, *contents = NULL;
			gsize length;

			filename = g_filename_from_uri (photo->data.uri, NULL, NULL);

			if (filename) {
				if (g_file_get_contents (filename, &contents, &length, NULL)) {
					gdk_pixbuf_loader_write (loader, (const guchar *) contents, length, NULL);
					g_free (contents);
				}

				g_free (filename);
			}
		}

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
				calced_width *= ((gfloat) MAX_COMPACT_IMAGE_DIMENSION / max_dimension);
				calced_height *= ((gfloat) MAX_COMPACT_IMAGE_DIMENSION / max_dimension);
			}

			g_object_unref (pixbuf);
		}

		if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
			photo->data.uri && *photo->data.uri) {
			gboolean is_local = g_str_has_prefix (photo->data.uri, "file://");
			gchar *unescaped = g_uri_unescape_string (photo->data.uri, NULL);
			g_string_append_printf (
				buffer,
				"<img width=\"%d\" height=\"%d\" src=\"%s%s\">",
				calced_width, calced_height,
				is_local ? "evo-" : "", unescaped);
			g_free (unescaped);
		} else {
			gchar *photo_data;

			photo_data = g_base64_encode (
					photo->data.inlined.data,
					photo->data.inlined.length);
			g_string_append_printf (
				buffer,
				"<img border=\"1\" src=\"data:%s;base64,%s\" "
					"width=\"%d\" height=\"%d\">",
				photo->data.inlined.mime_type,
				photo_data,
				calced_width, calced_height);
				g_free (photo_data);
		}

		e_contact_photo_free (photo);
	}

	g_string_append (buffer, "</td><td width=\"5\"></td><td valign=\"top\">\n");

	str = e_contact_get_const (contact, E_CONTACT_FILE_AS);

	if (str) {
		html = e_text_to_html (str, 0);
		g_string_append_printf (buffer, "<b>%s</b>", html);
		g_free (html);
	} else {
		str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);

		if (str) {
			html = e_text_to_html (str, 0);
			g_string_append_printf (buffer, "<b>%s</b>", html);
			g_free (html);
		}
	}

	g_string_append (buffer, "<hr>");

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		GList *email_list;
		GList *l;

		g_string_append (
			buffer,
			"<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\">"
			"<tr><td valign=\"top\">");
		g_string_append_printf (
			buffer,
			"<b>%s:</b>&nbsp;<td>", _ ("List Members"));

		email_list = e_contact_get (contact, E_CONTACT_EMAIL);

		for (l = email_list; l; l = l->next) {
			if (l->data) {
				html = e_text_to_html (l->data, 0);
				g_string_append_printf (buffer, "%s, ", html);
				g_free (html);
			}
		}

		g_string_append (buffer, "</td></tr></table>");

	} else {

		gboolean comma = FALSE;
		str = e_contact_get_const (contact, E_CONTACT_TITLE);

		if (str) {
			html = e_text_to_html (str, 0);
			g_string_append_printf (buffer, "<b>%s:</b> %s<br>", _ ("Job Title"), str);
			g_free (html);
		}

		#define print_email() {								\
			html = eab_parse_qp_email_to_html (str);				\
			\
			if (!html)                                                              \
				html = e_text_to_html (str, 0);					\
			\
			g_string_append_printf (buffer, "%s%s", comma ? ", " : "", html);	\
			g_free (html);								\
			comma = TRUE;								\
		}

		g_string_append_printf (buffer, "<b>%s:</b> ", _ ("Email"));
		str = e_contact_get_const (contact, E_CONTACT_EMAIL_1);

		if (str)
			print_email ();

		str = e_contact_get_const (contact, E_CONTACT_EMAIL_2);

		if (str)
			print_email ();

		str = e_contact_get_const (contact, E_CONTACT_EMAIL_3);

		if (str)
			print_email ();

		g_string_append (buffer, "<br>");

		#undef print_email

		str = e_contact_get_const (contact, E_CONTACT_HOMEPAGE_URL);

		if (str) {
			html = e_text_to_html (str, E_TEXT_TO_HTML_CONVERT_URLS);
			g_string_append_printf (
				buffer, "<b>%s:</b> %s<br>",
				_ ("Home page"), html);
			g_free (html);
		}

		str = e_contact_get_const (contact, E_CONTACT_BLOG_URL);

		if (str) {
			html = e_text_to_html (str, E_TEXT_TO_HTML_CONVERT_URLS);
			g_string_append_printf (
				buffer, "<b>%s:</b> %s<br>",
				_ ("Blog"), html);
		}
	}

	g_string_append (buffer, "</td></tr></table>\n");

	g_string_append (buffer, "</body></html>\n");
}

static CamelStream *
format_contact (EABContactFormatter *formatter,
                GCancellable *cancellable)
{
	GString *buffer;
	CamelStream *stream;

	buffer = g_string_new ("");

	if (formatter->priv->mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL) {
		render_normal (formatter, buffer);
	} else {
		render_compact (formatter, buffer);
	}

	stream = camel_stream_mem_new ();
	camel_stream_write_string (stream, buffer->str, cancellable, NULL);

	g_string_free (buffer, TRUE);

	return stream;
}

static void
do_start_async_formatter (GSimpleAsyncResult *result,
                          GObject *object,
                          GCancellable *cancellable)
{
	EABContactFormatter *formatter;
	CamelStream *stream;

	formatter = EAB_CONTACT_FORMATTER (object);

	stream = format_contact (formatter, cancellable);

	g_simple_async_result_set_op_res_gpointer (result, stream, NULL);
}

static void
eab_contact_formatter_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	EABContactFormatter *formatter = EAB_CONTACT_FORMATTER (object);

	switch (property_id) {
		case PROP_DISPLAY_MODE:
			eab_contact_formatter_set_display_mode (
				formatter, g_value_get_int (value));
			return;
		case PROP_RENDER_MAPS:
			eab_contact_formatter_set_render_maps (
				formatter, g_value_get_boolean (value));
			return;
		case PROP_STYLE:
			eab_contact_formatter_set_style (
				formatter, g_value_get_object (value));
			return;
		case PROP_STATE:
			eab_contact_formatter_set_state (
				formatter, g_value_get_uint (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
eab_contact_formatter_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	EABContactFormatter *formatter = EAB_CONTACT_FORMATTER (object);

	switch (property_id) {
		case PROP_DISPLAY_MODE:
			g_value_set_int (
				value,
				eab_contact_formatter_get_display_mode (
					formatter));
			return;
		case PROP_RENDER_MAPS:
			g_value_set_boolean (
				value,
				eab_contact_formatter_get_render_maps (
					formatter));
			return;
		case PROP_STYLE:
			g_value_set_object (
				value,
				eab_contact_formatter_get_style (
					formatter));
			return;
		case PROP_STATE:
			g_value_set_uint (
				value,
				eab_contact_formatter_get_state (
					formatter));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
eab_contact_formatter_dispose (GObject *object)
{
	EABContactFormatter *formatter;

	formatter = EAB_CONTACT_FORMATTER (object);

	if (formatter->priv->contact) {
		g_object_unref (formatter->priv->contact);
		formatter->priv->contact = NULL;
	}

	if (formatter->priv->style) {
		g_object_unref (formatter->priv->style);
		formatter->priv->style = NULL;
	}

	G_OBJECT_CLASS (eab_contact_formatter_parent_class)->dispose (object);
}

static void
eab_contact_formatter_class_init (EABContactFormatterClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EABContactFormatterClass));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = eab_contact_formatter_dispose;
	object_class->set_property = eab_contact_formatter_set_property;
	object_class->get_property = eab_contact_formatter_get_property;

	g_object_class_install_property (
		object_class,
		PROP_DISPLAY_MODE,
		g_param_spec_int (
			"display-mode",
			"",
			"",
			EAB_CONTACT_DISPLAY_RENDER_NORMAL,
			EAB_CONTACT_DISPLAY_RENDER_COMPACT,
			EAB_CONTACT_DISPLAY_RENDER_NORMAL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_RENDER_MAPS,
		g_param_spec_boolean (
			"render-maps",
			"",
			"",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_STYLE,
		g_param_spec_object (
			"style",
			NULL,
			NULL,
			GTK_TYPE_STYLE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_STATE,
		g_param_spec_uint (
			"state",
			NULL,
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
eab_contact_formatter_init (EABContactFormatter *formatter)
{
	formatter->priv = EAB_CONTACT_FORMATTER_GET_PRIVATE (formatter);

	formatter->priv->contact = NULL;
	formatter->priv->mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
	formatter->priv->render_maps = FALSE;
}

EABContactFormatter *
eab_contact_formatter_new (EABContactDisplayMode mode,
                           gboolean render_maps)
{
	return g_object_new (
		EAB_TYPE_CONTACT_FORMATTER,
		"display-mode", mode,
		"render-maps", render_maps,
		NULL);
}

void
eab_contact_formatter_set_display_mode (EABContactFormatter *formatter,
                                        EABContactDisplayMode mode)
{
	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));

	if (formatter->priv->mode == mode)
		return;

	formatter->priv->mode = mode;

	g_object_notify (G_OBJECT (formatter), "display-mode");
}

EABContactDisplayMode
eab_contact_formatter_get_display_mode (EABContactFormatter *formatter)
{
	g_return_val_if_fail (EAB_IS_CONTACT_FORMATTER (formatter),
			      EAB_CONTACT_DISPLAY_RENDER_NORMAL);

	return formatter->priv->mode;
}

void
eab_contact_formatter_set_render_maps (EABContactFormatter *formatter,
                                       gboolean render_maps)
{
	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));

	if (formatter->priv->render_maps == render_maps)
		return;

	formatter->priv->render_maps = render_maps;

	g_object_notify (G_OBJECT (formatter), "render-maps");
}

gboolean
eab_contact_formatter_get_render_maps (EABContactFormatter *formatter)
{
	g_return_val_if_fail (EAB_IS_CONTACT_FORMATTER (formatter), FALSE);

	return formatter->priv->render_maps;
}

void
eab_contact_formatter_set_style (EABContactFormatter *formatter,
                                 GtkStyle *style)
{
	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));

	if (formatter->priv->style == style) {
		return;
	}

	g_clear_object (&formatter->priv->style);

	if (style != NULL) {
		formatter->priv->style = g_object_ref (style);
	}

	g_object_notify (G_OBJECT (formatter), "style");
}

GtkStyle *
eab_contact_formatter_get_style (EABContactFormatter *formatter)
{
	g_return_val_if_fail (EAB_IS_CONTACT_FORMATTER (formatter), NULL);

	return formatter->priv->style;
}

void
eab_contact_formatter_set_state (EABContactFormatter *formatter,
                                 GtkStateType state)
{
	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));

	if (formatter->priv->state == state)
		return;

	formatter->priv->state = state;

	g_object_notify (G_OBJECT (formatter), "state");
}

GtkStateType
eab_contact_formatter_get_state (EABContactFormatter *formatter)
{
	g_return_val_if_fail (EAB_IS_CONTACT_FORMATTER (formatter), 0);

	return formatter->priv->state;
}

void
eab_contact_formatter_format_contact_sync (EABContactFormatter *formatter,
                                           EContact *contact,
                                           CamelStream *stream,
                                           GCancellable *cancellable)
{
	CamelStream *out;

	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_object_ref (contact);

	if (formatter->priv->contact)
		g_object_unref (formatter->priv->contact);

	formatter->priv->contact = contact;

	out = format_contact (formatter, cancellable);

	g_seekable_seek (G_SEEKABLE (out), 0, G_SEEK_SET, cancellable, NULL);
	camel_stream_write_to_stream (out, stream, cancellable, NULL);

	g_object_unref (out);
}

void
eab_contact_formatter_format_contact_async (EABContactFormatter *formatter,
                                            EContact *contact,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));
	g_return_if_fail (E_IS_CONTACT (contact));
	g_return_if_fail (callback != NULL);

	g_object_ref (contact);
	if (formatter->priv->contact)
		g_object_unref (formatter->priv->contact);

	formatter->priv->contact = contact;

	simple = g_simple_async_result_new (
		G_OBJECT (formatter), callback, user_data,
		eab_contact_formatter_format_contact_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, do_start_async_formatter,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

static void
collapse_contacts_list (WebKitDOMEventTarget *event_target,
                        WebKitDOMEvent *event,
                        gpointer user_data)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *list;
	gchar *id, *list_id;
	gchar *imagesdir, *src;
	gboolean hidden;

	document = user_data;
	id = webkit_dom_html_element_get_id (WEBKIT_DOM_HTML_ELEMENT (event_target));

	list_id = g_strconcat ("list-", id, NULL);
	list = webkit_dom_document_get_element_by_id (document, list_id);
	g_free (id);
	g_free (list_id);

	if (!list)
		return;

	imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);
	hidden = webkit_dom_html_element_get_hidden (WEBKIT_DOM_HTML_ELEMENT (list));

	if (hidden) {
		src = g_strdup_printf ("evo-file://%s/minus.png", imagesdir);
	} else {
		src = g_strdup_printf ("evo-file://%s/plus.png", imagesdir);
	}

	webkit_dom_html_element_set_hidden (
		WEBKIT_DOM_HTML_ELEMENT (list), !hidden);
	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (event_target), src);

	g_free (src);
	g_free (imagesdir);
}

void
eab_contact_formatter_bind_dom (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *nodes;
	gulong i, length;

	nodes = webkit_dom_document_get_elements_by_class_name (
			document, "_evo_collapse_button");

	length = webkit_dom_node_list_get_length (nodes);
	for (i = 0; i < length; i++) {

		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (nodes, i);
		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (node), "click",
			G_CALLBACK (collapse_contacts_list), FALSE, document);
	}
}
