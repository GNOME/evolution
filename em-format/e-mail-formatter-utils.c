/*
 * e-mail-formatter-utils.h
 *
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

#ifdef HAVE_CONFIG
#include <config.h>
#endif

#include "e-mail-formatter-utils.h"

#include <camel/camel.h>

#include <libemail-engine/e-mail-utils.h>
#include <libemail-engine/mail-config.h>
#include <e-util/e-util.h>
#include <e-util/e-datetime-format.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n.h>

#include <string.h>

static const gchar *addrspec_hdrs[] = {
	"Sender", "From", "Reply-To", "To", "Cc", "Bcc",
	"Resent-Sender", "Resent-From", "Resent-Reply-To",
	"Resent-To", "Resent-Cc", "Resent-Bcc", NULL
};

void
e_mail_formatter_format_text_header (EMailFormatter *formatter,
                                     GString *buffer,
                                     const gchar *label,
                                     const gchar *value,
                                     guint32 flags)
{
	const gchar *fmt, *html;
	gchar *mhtml = NULL;
	gboolean is_rtl;

	if (value == NULL)
		return;

	while (*value == ' ')
		value++;

	if (!(flags & E_MAIL_FORMATTER_HEADER_FLAG_HTML))
		html = mhtml = camel_text_to_html (value,
			e_mail_formatter_get_text_format_flags (formatter), 0);
	else
		html = value;

	is_rtl = gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL;

	if (flags & E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS) {
		if (flags & E_MAIL_FORMATTER_HEADER_FLAG_BOLD) {
			fmt = "<tr class=\"header-item\" style=\"display: %s\"><td><b>%s:</b> %s</td></tr>";
		} else {
			fmt = "<tr class=\"header-item\" style=\"display: %s\"><td>%s: %s</td></tr>";
		}
	} else if (flags & E_MAIL_FORMATTER_HEADER_FLAG_NODEC) {
		if (is_rtl)
			fmt = "<tr class=\"header-item rtl\" style=\"display: %s\"><td align=\"right\" valign=\"top\" width=\"100%%\">%2$s</td><th valign=top align=\"left\" nowrap>%1$s<b>&nbsp;</b></th></tr>";
		else
			fmt = "<tr class=\"header-item\" style=\"display: %s\"><th align=\"right\" valign=\"top\" nowrap>%s<b>&nbsp;</b></th><td valign=top>%s</td></tr>";
	} else {
		if (flags & E_MAIL_FORMATTER_HEADER_FLAG_BOLD) {
			if (is_rtl)
				fmt = "<tr class=\"header-item rtl\" style=\"display: %s\"><td align=\"right\" valign=\"top\" width=\"100%%\">%2$s</td><th align=\"left\" nowrap>%1$s:<b>&nbsp;</b></th></tr>";
			else
				fmt = "<tr class=\"header-item\" style=\"display: %s\"><th align=\"right\" valign=\"top\" nowrap>%s:<b>&nbsp;</b></th><td>%s</td></tr>";
		} else {
			if (is_rtl)
				fmt = "<tr class=\"header-item rtl\" style=\"display: %s\"><td align=\"right\" valign=\"top\" width=\"100%\">%2$s</td><td align=\"left\" nowrap>%1$s:<b>&nbsp;</b></td></tr>";
			else
				fmt = "<tr class=\"header-item\" style=\"display: %s\"><td align=\"right\" valign=\"top\" nowrap>%s:<b>&nbsp;</b></td><td>%s</td></tr>";
		}
	}

	g_string_append_printf (buffer, fmt,
		(flags & E_MAIL_FORMATTER_HEADER_FLAG_HIDDEN ? "none" : "table-row"), label, html);

	g_free (mhtml);
}

gchar *
e_mail_formatter_format_address (EMailFormatter *formatter,
                                 GString *out,
                                 struct _camel_header_address *a,
                                 gchar *field,
                                 gboolean no_links,
                                 gboolean elipsize)
{
	guint32 flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	gchar *name, *mailto, *addr;
	gint i = 0;
	gchar *str = NULL;
	gint limit = mail_config_get_address_count ();

	while (a) {
		if (a->name)
			name = camel_text_to_html (a->name, flags, 0);
		else
			name = NULL;

		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			if (name && *name) {
				gchar *real, *mailaddr;

				if (strchr (a->name, ',') || strchr (a->name, ';'))
					g_string_append_printf (out, "&quot;%s&quot;", name);
				else
					g_string_append (out, name);

				g_string_append (out, " &lt;");

				/* rfc2368 for mailto syntax and url encoding extras */
				if ((real = camel_header_encode_phrase ((guchar *) a->name))) {
					mailaddr = g_strdup_printf("%s <%s>", real, a->v.addr);
					g_free (real);
					mailto = camel_url_encode (mailaddr, "?=&()");
					g_free (mailaddr);
				} else {
					mailto = camel_url_encode (a->v.addr, "?=&()");
				}
			} else {
				mailto = camel_url_encode (a->v.addr, "?=&()");
			}
			addr = camel_text_to_html (a->v.addr, flags, 0);
			if (no_links)
				g_string_append_printf (out, "%s", addr);
			else
				g_string_append_printf (out, "<a href=\"mailto:%s\">%s</a>", mailto, addr);
			g_free (mailto);
			g_free (addr);

			if (name && *name)
				g_string_append (out, "&gt;");
			break;
		case CAMEL_HEADER_ADDRESS_GROUP:
			g_string_append_printf (out, "%s: ", name);
			e_mail_formatter_format_address (
				formatter, out, a->v.members, field,
				no_links, elipsize);
			g_string_append_printf (out, ";");
			break;
		default:
			g_warning ("Invalid address type");
			break;
		}

		g_free (name);

		i++;
		a = a->next;
		if (a)
			g_string_append (out, ", ");

		if (!elipsize)
			continue;

		/* Let us add a '...' if we have more addresses */
		if (limit > 0 && i == limit && a != NULL) {
			const gchar *id = NULL;

			if (strcmp (field, _("To")) == 0) {
				id = "to";
			} else if (strcmp (field, _("Cc")) == 0) {
				id = "cc";
			} else if (strcmp (field, _("Bcc")) == 0) {
				id = "bcc";
			}

			if (id) {
				g_string_append_printf (out,
					"<span id=\"__evo-moreaddr-%s\" "
					      "style=\"display: none;\">", id);
				str = g_strdup_printf (
					"<img src=\"evo-file://%s/plus.png\" "
					     "id=\"__evo-moreaddr-img-%s\" class=\"navigable\">",
					EVOLUTION_IMAGESDIR, id);
			}
		}
	}

	if (elipsize && str) {
		const gchar *id = NULL;

		if (strcmp (field, _("To")) == 0) {
			id = "to";
		} else if (strcmp (field, _("Cc")) == 0) {
			id = "cc";
		} else if (strcmp (field, _("Bcc")) == 0) {
			id = "bcc";
		}

		if (id) {
			g_string_append_printf (out,
				"</span>"
				"<span class=\"navigable\" "
					"id=\"__evo-moreaddr-ellipsis-%s\" "
					"style=\"display: inline;\">...</span>",
				id);
		}
	}

	return str;
}

void
e_mail_formatter_canon_header_name (gchar *name)
{
	gchar *inptr = name;

	/* canonicalise the header name... first letter is
	 * capitalised and any letter following a '-' also gets
	 * capitalised */

	if (*inptr >= 'a' && *inptr <= 'z')
		*inptr -= 0x20;

	inptr++;

	while (*inptr) {
		if (inptr[-1] == '-' && *inptr >= 'a' && *inptr <= 'z')
			*inptr -= 0x20;
		else if (*inptr >= 'A' && *inptr <= 'Z')
			*inptr += 0x20;

		inptr++;
	}
}

void
e_mail_formatter_format_header (EMailFormatter *formatter,
                                GString *buffer,
                                CamelMedium *part,
                                struct _camel_header_raw *header,
                                guint32 flags,
                                const gchar *charset)
{
	gchar *name, *buf, *value = NULL;
	const gchar *label, *txt;
	gboolean addrspec = FALSE;
	gchar *str_field = NULL;
	gint i;

	name = g_alloca (strlen (header->name) + 1);
	strcpy (name, header->name);
	e_mail_formatter_canon_header_name (name);

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (!strcmp (name, addrspec_hdrs[i])) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(name);

	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;
		gchar *img;
		const gchar *charset = e_mail_formatter_get_charset (formatter) ?
						e_mail_formatter_get_charset (formatter) :
						e_mail_formatter_get_default_charset (formatter);

		buf = camel_header_unfold (header->value);
		if (!(addrs = camel_header_address_decode (buf, charset))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new("");
		img = e_mail_formatter_format_address (formatter, html, addrs, (gchar *) label,
			(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS),
			!(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

		if (img) {
			str_field = g_strdup_printf ("%s: %s", label, img);
			label = str_field;
			flags |= E_MAIL_FORMATTER_HEADER_FLAG_NODEC;
			g_free (img);
		}

		camel_header_address_list_clear (&addrs);
		txt = value = html->str;
		g_string_free (html, FALSE);

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_HTML | E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp (name, "Subject")) {
		buf = camel_header_unfold (header->value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp(name, "X-evolution-mailer")) {
		/* pseudo-header */
		label = _("Mailer");
		txt = value = camel_header_format_ctext (header->value, charset);
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp (name, "Date") || !strcmp (name, "Resent-Date")) {
		gint msg_offset, local_tz;
		time_t msg_date;
		struct tm local;
		gchar *html;
		gboolean hide_real_date;

		hide_real_date = !e_mail_formatter_get_show_real_date (formatter);

		txt = header->value;
		while (*txt == ' ' || *txt == '\t')
			txt++;

		html = camel_text_to_html (txt,
			e_mail_formatter_get_text_format_flags (formatter), 0);

		msg_date = camel_header_decode_date (txt, &msg_offset);
		e_localtime_with_offset (msg_date, &local, &local_tz);

		/* Convert message offset to minutes (e.g. -0400 --> -240) */
		msg_offset = ((msg_offset / 100) * 60) + (msg_offset % 100);
		/* Turn into offset from localtime, not UTC */
		msg_offset -= local_tz / 60;

		/* value will be freed at the end */
		if (!hide_real_date && !msg_offset) {
			/* No timezone difference; just show the real Date: header */
			txt = value = html;
		} else {
			gchar *date_str;

			date_str = e_datetime_format_format ("mail", "header",
							     DTFormatKindDateTime, msg_date);

			if (hide_real_date) {
				/* Show only the local-formatted date, losing all timezone
				 * information like Outlook does. Should we attempt to show
				 * it somehow? */
				txt = value = date_str;
			} else {
				txt = value = g_strdup_printf ("%s (<I>%s</I>)", html, date_str);
				g_free (date_str);
			}
			g_free (html);
		}
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_HTML |
			 E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp(name, "Newsgroups")) {
		struct _camel_header_newsgroup *ng, *scan;
		GString *html;

		buf = camel_header_unfold (header->value);

		if (!(ng = camel_header_newsgroups_decode (buf))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new("");
		scan = ng;
		while (scan) {
			if (flags & E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS)
				g_string_append_printf (html, "%s", scan->newsgroup);
			else
				g_string_append_printf(html, "<a href=\"news:%s\">%s</a>",
					scan->newsgroup, scan->newsgroup);
			scan = scan->next;
			if (scan)
				g_string_append_printf(html, ", ");
		}

		camel_header_newsgroups_free (ng);

		txt = html->str;
		g_string_free (html, FALSE);
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_HTML |
			 E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp (name, "Received") || !strncmp (name, "X-", 2)) {
		/* don't unfold Received nor extension headers */
		txt = value = camel_header_decode_string (header->value, charset);
	} else {
		/* don't unfold Received nor extension headers */
		buf = camel_header_unfold (header->value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);
	}

	e_mail_formatter_format_text_header (formatter, buffer, label, txt, flags);

	g_free (value);
	g_free (str_field);
}

GSList *
e_mail_formatter_find_rfc822_end_iter (GSList *iter)
{
	EMailPart *part;
	gchar *end;

	part = iter->data;
	end = g_strconcat (part->id, ".end", NULL);
	for (; iter != NULL; iter = g_slist_next (iter)) {
		part = iter->data;
		if (!part)
			continue;

		if (g_strcmp0 (part->id, end) == 0) {
			g_free (end);
			return iter;
		}
	}
	g_free (end);
	return iter;
}

gchar *
e_mail_formatter_parse_html_mnemonics (const gchar *label,
                                       gchar **access_key)
{
	const gchar *pos = NULL;
	gchar ak = 0;
	GString *html_label = NULL;

	pos = strstr (label, "_");
	if (pos != NULL) {
		ak = pos[1];

		/* Convert to uppercase */
		if (ak >= 'a')
			ak = ak - 32;

		html_label = g_string_new ("");
		g_string_append_len (html_label, label, pos - label);
		g_string_append_printf (html_label, "<u>%c</u>", pos[1]);
		g_string_append (html_label, &pos[2]);

		if (access_key) {
			if (ak) {
				*access_key = g_strdup_printf ("%c", ak);
			} else {
				*access_key = NULL;
			}
		}

	} else {
		html_label = g_string_new (label);

		if (access_key) {
			*access_key = NULL;
		}
	}

	return g_string_free (html_label, FALSE);
}
