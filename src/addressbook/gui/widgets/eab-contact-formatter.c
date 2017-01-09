/*
 * eab-contact-formatter.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "eab-book-util.h"
#include "eab-gui-util.h"

#include "eab-contact-formatter.h"

#define EAB_CONTACT_FORMATTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EAB_TYPE_CONTACT_FORMATTER, EABContactFormatterPrivate))

#define TEXT_IS_RIGHT_TO_LEFT \
	(gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)

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

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n" \
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

struct _EABContactFormatterPrivate {
	EABContactDisplayMode mode;
	gboolean render_maps;
	gboolean supports_tel;
	gboolean supports_sip;
};

enum {
	PROP_0,
	PROP_DISPLAY_MODE,
	PROP_RENDER_MAPS
};

static struct {
	const gchar *name;
	const gchar *pretty_name;
} common_location[] = {
	{ "WORK",  N_ ("Work")  },
	{ "HOME",  N_ ("Home")  },
	{ "OTHER", N_ ("Other") }
};

G_DEFINE_TYPE (
	EABContactFormatter,
	eab_contact_formatter,
	G_TYPE_OBJECT);

#define E_CREATE_TEL_URL	(E_TEXT_TO_HTML_LAST_FLAG << 0)
#define E_CREATE_SIP_URL	(E_TEXT_TO_HTML_LAST_FLAG << 1)

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
		icon_html = g_strdup_printf ("<img src=\"gtk-stock://%s\" width=\"16px\" height=\"16px\" />", icon);
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

/* Returns NULL if no replace had been done (and
   original 'str' should be used instead). Otherwise
   free the returned pointer with g_free().
*/
static gchar *
maybe_create_url (const gchar *str,
		  guint html_flags)
{
	gchar *tmp = NULL;
	const gchar *url = NULL;

	g_return_val_if_fail (str != NULL, NULL);

	if ((html_flags & E_CREATE_TEL_URL) != 0) {
		/* RFC 3966 requires either the phone number begins with '+',
		   or the URL contains a 'phone-context' parameter, but that
		   also requires changing phone number for some countries, thus
		   rather mandate the '+' at the beginning. */
		if (*str == '+')
			url = "tel:";
	} else if ((html_flags & E_CREATE_SIP_URL) != 0) {
		url = "sip:";
	}

	if (url && g_ascii_strncasecmp (str, url, strlen (url)) != 0)
		tmp = g_strconcat (url, str, NULL);

	return tmp;
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

	if (str != NULL && *str != '\0') {
		gchar *tmp = NULL;

		tmp = maybe_create_url (str, html_flags);
		if (tmp)
			str = tmp;

		render_table_row (buffer, html_label, str, icon, html_flags);

		g_free (tmp);
	}
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
	const gchar *str;
	gchar *tmp;

	val_list = e_contact_get (contact, field);

	for (l = val_list; l; l = l->next) {
		str = l->data;

		if (l != val_list)
			g_string_append (val, "<br>");

		tmp = maybe_create_url (str, html_flags);
		if (tmp)
			str = tmp;

		if ((html_flags & E_TEXT_TO_HTML_CONVERT_URLS) != 0) {
			gchar *value = e_text_to_html (str, html_flags);

			if (value && *value)
				g_string_append (val, value);

			g_free (value);
		} else {
			g_string_append (val, str);
		}

		g_free (tmp);
	}

	if (val->str && *val->str) {
		if ((html_flags & E_TEXT_TO_HTML_CONVERT_URLS) != 0)
			html_flags = 0;

		render_table_row (buffer, html_label, val->str, icon, html_flags);
	}

	g_string_free (val, TRUE);
	g_list_foreach (val_list, (GFunc) g_free, NULL);
	g_list_free (val_list);
}

typedef enum {
	EAB_CONTACT_FORMATTER_SIP_TYPE_HOME,
	EAB_CONTACT_FORMATTER_SIP_TYPE_WORK,
	EAB_CONTACT_FORMATTER_SIP_TYPE_OTHER
} EABContactFormatterSIPType;

static void
accum_sip (GString *buffer,
	   EContact *contact,
	   EABContactFormatterSIPType use_sip_type,
	   const gchar *icon,
	   guint html_flags)
{
	const gchar *html_label = _("SIP");
	GList *sip_attr_list, *l;
	GString *val = g_string_new ("");
	gchar *tmp;

	sip_attr_list = e_contact_get_attributes (contact, E_CONTACT_SIP);
	for (l = sip_attr_list; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		gchar *sip;
		const gchar *str;
		EABContactFormatterSIPType sip_type;

		if (e_vcard_attribute_has_type (attr, "HOME"))
			sip_type = EAB_CONTACT_FORMATTER_SIP_TYPE_HOME;
		else if (e_vcard_attribute_has_type (attr, "WORK"))
			sip_type = EAB_CONTACT_FORMATTER_SIP_TYPE_WORK;
		else
			sip_type = EAB_CONTACT_FORMATTER_SIP_TYPE_OTHER;

		if (sip_type != use_sip_type)
			continue;

		sip = e_vcard_attribute_get_value (attr);
		if (!sip || !*sip) {
			g_free (sip);
			continue;
		}

		tmp = maybe_create_url (sip, html_flags);
		if (tmp)
			str = tmp;
		else
			str = sip;

		if ((html_flags & E_TEXT_TO_HTML_CONVERT_URLS) != 0) {
			gchar *value = e_text_to_html (str, html_flags);

			if (value && *value) {
				if (val->len)
					g_string_append (val, "<br>");
				g_string_append (val, value);
			}

			g_free (value);
		} else {
			if (val->len)
				g_string_append (val, "<br>");
			g_string_append (val, str);
		}

		g_free (tmp);
		g_free (sip);
	}

	if (val->str && *val->str) {
		if ((html_flags & E_TEXT_TO_HTML_CONVERT_URLS) != 0)
			html_flags = 0;

		render_table_row (buffer, html_label, val->str, icon, html_flags);
	}

	g_string_free (val, TRUE);
	g_list_free_full (sip_attr_list, (GDestroyNotify) e_vcard_attribute_free);
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
                    EContact *contact,
                    GString *buffer)
{
	const gchar *str;
	gchar *html;
	EContactPhoto *photo;

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
			buffer,
			"<img id=\"__evo-contact-photo\" border=\"1\" src=\"data:%s;base64,%s\">",
			photo->data.inlined.mime_type,
			photo_data);
	} else if (photo && photo->type == E_CONTACT_PHOTO_TYPE_URI && photo->data.uri && *photo->data.uri) {
		gboolean is_local = g_str_has_prefix (photo->data.uri, "file://");
		const gchar *uri = photo->data.uri;
		/* WebKit 2.2.x doesn't re-escape URIs, thus do this for versions before and after this */
		#if !(WEBKIT_MAJOR_VERSION == 2 && WEBKIT_MINOR_VERSION == 2)
		gchar *unescaped = g_uri_unescape_string (uri, NULL);
		uri = unescaped;
		#endif
		g_string_append_printf (
			buffer, "<img id=\"__evo-contact-photo\" border=\"1\" src=\"%s%s\">",
			is_local ? "evo-" : "", uri);
		#if !(WEBKIT_MAJOR_VERSION == 2 && WEBKIT_MINOR_VERSION == 2)
		g_free (unescaped);
		#endif
	}

	if (photo)
		e_contact_photo_free (photo);

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		g_string_append_printf (buffer, "<img src=\"gtk-stock://%s\" width=\"16px\" height=\"16px\">", CONTACT_LIST_ICON);
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
			"<img src=\"evo-file://%s/minus.png\" width=\"16px\" height=\"16px\" "
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
                     EContact *contact,
                     GString *buffer)
{
	EDestination *destination;
	const GList *dest, *dests;

	destination = e_destination_new ();
	e_destination_set_contact (destination, contact, 0);
	dests = e_destination_list_get_root_dests (destination);

	render_title_block (formatter, contact, buffer);

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
                       EContact *contact,
                       GString *buffer)
{
	GString *accum, *email;
	GList *email_list, *l, *email_attr_list, *al;
	gint email_num = 0;
	const gchar *nl;
	guint32 sip_flags = 0;

	if (formatter->priv->supports_sip)
		sip_flags = E_TEXT_TO_HTML_CONVERT_URLS |
			    E_TEXT_TO_HTML_HIDE_URL_SCHEME |
			    E_TEXT_TO_HTML_URL_IS_WHOLE_TEXT |
			    E_CREATE_SIP_URL;

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

	accum_sip (accum, contact, EAB_CONTACT_FORMATTER_SIP_TYPE_OTHER, NULL, sip_flags);

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
/* Disabled, due to the code depending on WebKit1 functionality (gtk+ widgets inside webview).
   Re-enable once there is a good replacement. See also ACTION (CONTACT_PREVIEW_SHOW_MAPS) usage.
*/
#if 0
#ifdef ENABLE_CONTACT_MAPS
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
#endif /* ENABLE_CONTACT_MAPS */
#endif /* 0 */
}

static void
render_work_column (EABContactFormatter *formatter,
                    EContact *contact,
                    GString *buffer)
{
	GString *accum = g_string_new ("");
	guint32 phone_flags = 0, sip_flags = 0;

	if (formatter->priv->supports_tel)
		phone_flags = E_TEXT_TO_HTML_CONVERT_URLS |
			      E_TEXT_TO_HTML_HIDE_URL_SCHEME |
			      E_TEXT_TO_HTML_URL_IS_WHOLE_TEXT |
			      E_CREATE_TEL_URL;

	if (formatter->priv->supports_sip)
		sip_flags = E_TEXT_TO_HTML_CONVERT_URLS |
			    E_TEXT_TO_HTML_HIDE_URL_SCHEME |
			    E_TEXT_TO_HTML_URL_IS_WHOLE_TEXT |
			    E_CREATE_SIP_URL;

	accum_attribute (accum, contact, _("Company"), E_CONTACT_ORG, NULL, 0);
	accum_attribute (accum, contact, _("Department"), E_CONTACT_ORG_UNIT, NULL, 0);
	accum_attribute (accum, contact, _("Office"), E_CONTACT_OFFICE, NULL, 0);
	accum_attribute (accum, contact, _("Profession"), E_CONTACT_ROLE, NULL, 0);
	accum_attribute (accum, contact, _("Position"), E_CONTACT_TITLE, NULL, 0);
	accum_attribute (accum, contact, _("Manager"), E_CONTACT_MANAGER, NULL, 0);
	accum_attribute (accum, contact, _("Assistant"), E_CONTACT_ASSISTANT, NULL, 0);
	accum_attribute (accum, contact, _("Video Chat"), E_CONTACT_VIDEO_URL, VIDEOCONF_ICON, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Calendar"), E_CONTACT_CALENDAR_URI, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Free/Busy"), E_CONTACT_FREEBUSY_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Phone"), E_CONTACT_PHONE_BUSINESS, NULL, phone_flags);
	accum_attribute (accum, contact, _("Fax"), E_CONTACT_PHONE_BUSINESS_FAX, NULL, 0);
	accum_sip       (accum, contact, EAB_CONTACT_FORMATTER_SIP_TYPE_WORK, NULL, sip_flags);
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
                        EContact *contact,
                        GString *buffer)
{
	GString *accum = g_string_new ("");
	guint32 phone_flags = 0, sip_flags = 0;

	if (formatter->priv->supports_tel)
		phone_flags = E_TEXT_TO_HTML_CONVERT_URLS |
			      E_TEXT_TO_HTML_HIDE_URL_SCHEME |
			      E_TEXT_TO_HTML_URL_IS_WHOLE_TEXT |
			      E_CREATE_TEL_URL;

	if (formatter->priv->supports_sip)
		sip_flags = E_TEXT_TO_HTML_CONVERT_URLS |
			    E_TEXT_TO_HTML_HIDE_URL_SCHEME |
			    E_TEXT_TO_HTML_URL_IS_WHOLE_TEXT |
			    E_CREATE_SIP_URL;

	accum_attribute (accum, contact, _("Home Page"), E_CONTACT_HOMEPAGE_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Web Log"), E_CONTACT_BLOG_URL, NULL, E_TEXT_TO_HTML_CONVERT_URLS);
	accum_attribute (accum, contact, _("Phone"), E_CONTACT_PHONE_HOME, NULL, phone_flags);
	accum_attribute (accum, contact, _("Mobile Phone"), E_CONTACT_PHONE_MOBILE, NULL, phone_flags);
	accum_sip       (accum, contact, EAB_CONTACT_FORMATTER_SIP_TYPE_HOME, NULL, sip_flags);
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
render_other_column (EABContactFormatter *formatter,
                     EContact *contact,
                     GString *buffer)
{
	GString *accum = g_string_new ("");

	accum_address (accum, contact, _("Address"), E_CONTACT_ADDRESS_OTHER, E_CONTACT_ADDRESS_LABEL_OTHER);
	if (formatter->priv->render_maps)
		accum_address_map (accum, contact, E_CONTACT_ADDRESS_OTHER);

	if (accum->len > 0) {
		g_string_append_printf (
			buffer,
			"<div class=\"column\" id=\"contact-other\">"
			"<h3>%s</h3>"
			"<table border=\"0\" cellspacing=\"5\">%s</table>"
			"</div>", _("Other"), accum->str);
	}

	g_string_free (accum, TRUE);
}

static void
render_footer (EABContactFormatter *formatter,
               EContact *contact,
               GString *buffer)
{
	const gchar *str;

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
                EContact *contact,
                GString *buffer)
{
	render_title_block (formatter, contact, buffer);

	g_string_append (buffer, "<div id=\"columns\">");
	render_contact_column (formatter, contact, buffer);
	render_work_column (formatter, contact, buffer);
	render_personal_column (formatter, contact, buffer);
	render_other_column (formatter, contact, buffer);
	g_string_append (buffer, "</div>");

	render_footer (formatter, contact, buffer);
}

static void
render_normal (EABContactFormatter *formatter,
               EContact *contact,
               GString *buffer)
{
	g_string_append (buffer, HTML_HEADER);
	g_string_append (buffer,"<body class=\"-e-web-view-background-color -e-web-view-text-color\">");

	if (contact != NULL) {
		if (e_contact_get (contact, E_CONTACT_IS_LIST))
			render_contact_list (formatter, contact, buffer);
		else
			render_contact (formatter, contact, buffer);
	}

	g_string_append (buffer, "</body></html>\n");
}

static void
render_compact (EABContactFormatter *formatter,
                EContact *contact,
                GString *buffer)
{
	const gchar *str;
	gchar *html;
	EContactPhoto *photo;

	g_string_append (buffer, HTML_HEADER);
	g_string_append (buffer,"<body class=\"-e-web-view-background-color -e-web-view-text-color\">");

	if (contact == NULL) {
		g_string_append (buffer, "</body></html>");
		return;
	}

	g_string_append_printf (
		buffer,
		"<table><tr><td valign=\"top\">");

	photo = e_contact_get (contact, E_CONTACT_PHOTO);

	if (photo == NULL)
		photo = e_contact_get (contact, E_CONTACT_LOGO);

	if (photo != NULL) {
		gint calced_width = MAX_COMPACT_IMAGE_DIMENSION;
		gint calced_height = MAX_COMPACT_IMAGE_DIMENSION;
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
			const gchar *uri = photo->data.uri;
			/* WebKit 2.2.x doesn't re-escape URIs, thus do this for versions before and after this */
			#if !(WEBKIT_MAJOR_VERSION == 2 && WEBKIT_MINOR_VERSION == 2)
			gchar *unescaped = g_uri_unescape_string (uri, NULL);
			uri = unescaped;
			#endif
			g_string_append_printf (
				buffer,
				"<img id=\"__evo-contact-photo\" width=\"%dpx\" height=\"%dpx\" src=\"%s%s\">",
				calced_width, calced_height,
				is_local ? "evo-" : "", uri);
			#if !(WEBKIT_MAJOR_VERSION == 2 && WEBKIT_MINOR_VERSION == 2)
			g_free (unescaped);
			#endif
		} else {
			gchar *photo_data;

			photo_data = g_base64_encode (
					photo->data.inlined.data,
					photo->data.inlined.length);
			g_string_append_printf (
				buffer,
				"<img id=\"__evo-contact-photo\" border=\"1\" src=\"data:%s;base64,%s\" "
					"width=\"%dpx\" height=\"%dpx\">",
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

		#define print_email() { \
			html = eab_parse_qp_email_to_html (str); \
 \
			if (!html) \
				html = e_text_to_html (str, 0); \
 \
			g_string_append_printf (buffer, "%s%s", comma ? ", " : "", html); \
			g_free (html); \
			comma = TRUE; \
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

static gboolean
eab_contact_formatter_scheme_supported (const gchar *scheme)
{
	GAppInfo *app_info;
	gboolean supported;

	app_info = g_app_info_get_default_for_uri_scheme (scheme);
	supported = app_info != NULL;

	if (app_info && g_app_info_can_delete (app_info))
		g_app_info_delete (app_info);

	return supported;
}

static void
eab_contact_formatter_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DISPLAY_MODE:
			eab_contact_formatter_set_display_mode (
				EAB_CONTACT_FORMATTER (object),
				g_value_get_int (value));
			return;

		case PROP_RENDER_MAPS:
			eab_contact_formatter_set_render_maps (
				EAB_CONTACT_FORMATTER (object),
				g_value_get_boolean (value));
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
	switch (property_id) {
		case PROP_DISPLAY_MODE:
			g_value_set_int (
				value,
				eab_contact_formatter_get_display_mode (
				EAB_CONTACT_FORMATTER (object)));
			return;

		case PROP_RENDER_MAPS:
			g_value_set_boolean (
				value,
				eab_contact_formatter_get_render_maps (
				EAB_CONTACT_FORMATTER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
eab_contact_formatter_class_init (EABContactFormatterClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EABContactFormatterClass));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = eab_contact_formatter_set_property;
	object_class->get_property = eab_contact_formatter_get_property;

	g_object_class_install_property (
		object_class,
		PROP_DISPLAY_MODE,
		g_param_spec_int (
			"display-mode",
			"Display Mode",
			NULL,
			EAB_CONTACT_DISPLAY_RENDER_NORMAL,
			EAB_CONTACT_DISPLAY_RENDER_COMPACT,
			EAB_CONTACT_DISPLAY_RENDER_NORMAL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_RENDER_MAPS,
		g_param_spec_boolean (
			"render-maps",
			"Render Maps",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
eab_contact_formatter_init (EABContactFormatter *formatter)
{
	formatter->priv = EAB_CONTACT_FORMATTER_GET_PRIVATE (formatter);

	formatter->priv->mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
	formatter->priv->render_maps = FALSE;
	formatter->priv->supports_tel = eab_contact_formatter_scheme_supported ("tel");
	formatter->priv->supports_sip = eab_contact_formatter_scheme_supported ("sip");
}

EABContactFormatter *
eab_contact_formatter_new (void)
{
	return g_object_new (EAB_TYPE_CONTACT_FORMATTER, NULL);
}

gboolean
eab_contact_formatter_get_render_maps (EABContactFormatter *formatter)
{
	g_return_val_if_fail (EAB_IS_CONTACT_FORMATTER (formatter), FALSE);

	return formatter->priv->render_maps;
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

EABContactDisplayMode
eab_contact_formatter_get_display_mode (EABContactFormatter *formatter)
{
	g_return_val_if_fail (
		EAB_IS_CONTACT_FORMATTER (formatter),
		EAB_CONTACT_DISPLAY_RENDER_NORMAL);

	return formatter->priv->mode;
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

void
eab_contact_formatter_format_contact (EABContactFormatter *formatter,
                                      EContact *contact,
                                      GString *output_buffer)
{
	g_return_if_fail (EAB_IS_CONTACT_FORMATTER (formatter));
	g_return_if_fail (E_IS_CONTACT (contact));
	g_return_if_fail (output_buffer != NULL);

	if (formatter->priv->mode == EAB_CONTACT_DISPLAY_RENDER_NORMAL)
		render_normal (formatter, contact, output_buffer);
	else
		render_compact (formatter, contact, output_buffer);
}
