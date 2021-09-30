/*
 * e-mail-formatter-utils.h
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

#include "e-mail-formatter-utils.h"
#include "e-mail-part-headers.h"

#include <string.h>
#include <glib/gi18n.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

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
	GtkTextDirection direction;
	const gchar *fmt, *html;
	const gchar *display;
	gchar *mhtml = NULL, *mfmt = NULL;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (label != NULL);

	if (value == NULL)
		return;

	while (*value == ' ')
		value++;

	if (!(flags & E_MAIL_FORMATTER_HEADER_FLAG_HTML)) {
		CamelMimeFilterToHTMLFlags text_format_flags;

		text_format_flags =
			e_mail_formatter_get_text_format_flags (formatter);
		html = mhtml = camel_text_to_html (
			value, text_format_flags, 0);
	} else {
		html = value;
	}

	direction = gtk_widget_get_default_direction ();

	if (flags & E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS) {
		if ((flags & E_MAIL_FORMATTER_HEADER_FLAG_BOLD) &&
		    !(flags & E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING)) {
			fmt = "<tr style=\"display: %s\">"
				"<td><b>%s:</b> %s</td></tr>";
		} else {
			fmt = "<tr style=\"display: %s\">"
				"<td>%s: %s</td></tr>";
		}
	} else {
		const gchar *decstr;
		const gchar *dirstr;
		const gchar *extra_style;

		if ((flags & E_MAIL_FORMATTER_HEADER_FLAG_NODEC) != 0)
			decstr = "";
		else
			decstr = ":";

		if ((flags & E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING) != 0)
			extra_style = " style=\"font-weight: normal;\"";
		else
			extra_style = "";

		if (direction == GTK_TEXT_DIR_RTL)
			dirstr = "rtl";
		else
			dirstr = "ltr";

		mfmt = g_strdup_printf (
			"<tr class=\"header\" style=\"display: %%s;\">"
			"<th class=\"header %s\"%s>%%s%s</th>"
			"<td class=\"header %s\">%%s</td>"
			"</tr>",
			dirstr, extra_style, decstr, dirstr);
		fmt = mfmt;
	}

	if (flags & E_MAIL_FORMATTER_HEADER_FLAG_HIDDEN)
		display = "none";
	else
		display = "table-row";

	g_string_append_printf (buffer, fmt, display, label, html);

	g_free (mhtml);
	g_free (mfmt);
}

gchar *
e_mail_formatter_format_address (EMailFormatter *formatter,
                                 GString *out,
                                 struct _camel_header_address *a,
                                 const gchar *field,
                                 gboolean no_links,
                                 gboolean elipsize)
{
	CamelMimeFilterToHTMLFlags flags;
	gchar *name, *mailto, *addr, *sanitized_addr;
	gint i = 0;
	gchar *str = NULL;
	gint limit = mail_config_get_address_count ();
	gboolean show_mails = mail_config_get_show_mails_in_preview ();

	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), NULL);
	g_return_val_if_fail (out != NULL, NULL);
	g_return_val_if_fail (field != NULL, NULL);

	flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;

	while (a != NULL) {
		if (a->name)
			name = camel_text_to_html (a->name, flags, 0);
		else
			name = NULL;

		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			sanitized_addr = camel_utils_sanitize_ascii_domain_in_address (a->v.addr, TRUE);
			if (name != NULL && *name != '\0') {
				gchar *real, *mailaddr;

				if (show_mails || no_links) {
					if (strchr (a->name, ',') || strchr (a->name, ';') || strchr (a->name, '\"') || strchr (a->name, '<') || strchr (a->name, '>'))
						g_string_append_printf (out, "&quot;%s&quot;", name);
					else
						g_string_append (out, name);

					g_string_append (out, " &lt;");
				}

				/* rfc2368 for mailto syntax and url encoding extras */
				if ((real = camel_header_encode_phrase ((guchar *) a->name))) {
					mailaddr = g_strdup_printf ("%s <%s>", real, sanitized_addr ? sanitized_addr : a->v.addr);
					g_free (real);
					mailto = camel_url_encode (mailaddr, "?=&()");
					g_free (mailaddr);
				} else {
					mailto = camel_url_encode (sanitized_addr ? sanitized_addr : a->v.addr, "?=&()");
				}
			} else {
				mailto = camel_url_encode (sanitized_addr ? sanitized_addr : a->v.addr, "?=&()");
			}
			addr = camel_text_to_html (sanitized_addr ? sanitized_addr : a->v.addr, flags, 0);
			if (no_links)
				g_string_append_printf (out, "%s", addr);
			else if (!show_mails && name && *name)
				g_string_append_printf (out, "<a href=\"mailto:%s\">%s</a>", mailto, name);
			else
				g_string_append_printf (out, "<a href=\"mailto:%s\">%s</a>", mailto, addr);
			g_free (sanitized_addr);
			g_free (mailto);
			g_free (addr);

			if (name != NULL && *name != '\0' && (show_mails || no_links))
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
		if (a != NULL)
			g_string_append (out, ", ");

		if (!elipsize)
			continue;

		/* Let us add a '...' if we have more addresses */
		if (limit > 0 && i == limit && a != NULL) {
			if (strcmp (field, _("To")) == 0 ||
			    strcmp (field, _("Cc")) == 0 ||
			    strcmp (field, _("Bcc")) == 0) {
				gint icon_width, icon_height;

				if (!gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &icon_width, &icon_height)) {
					icon_width = 16;
					icon_height = 16;
				}

				g_string_append (
					out,
					"<span id=\"__evo-moreaddr\" "
					"style=\"display: none;\">");

				str = g_strdup_printf (
					"<button type=\"button\" id=\"__evo-moreaddr-button\" class=\"header-collapse\" style=\"display: inline-block;\">"
					"<img src=\"gtk-stock://pan-end-symbolic?size=%d\" width=\"%dpx\" height=\"%dpx\"/>"
					"</button>",
					GTK_ICON_SIZE_BUTTON, icon_width, icon_height);
			}
		}
	}

	if (elipsize && str) {
		if (strcmp (field, _("To")) == 0 ||
		    strcmp (field, _("Cc")) == 0 ||
		    strcmp (field, _("Bcc")) == 0) {

			g_string_append (
				out,
				"</span>"
				"<span class=\"navigable\" "
					"id=\"__evo-moreaddr-ellipsis\" "
					"style=\"display: inline;\">...</span>");
		}
	}

	return str;
}

void
e_mail_formatter_canon_header_name (gchar *name)
{
	gchar *inptr = name;

	g_return_if_fail (name != NULL);

	/* canonicalise the header name... first letter is
	 * capitalised and any letter following a '-' also gets
	 * capitalised */

	if (*inptr >= 'a' && *inptr <= 'z')
		*inptr -= 0x20;

	inptr++;

	while (*inptr) {
		if (inptr[-1] == '-' && *inptr >= 'a' && *inptr <= 'z')
			*inptr -= 0x20;
		else if (inptr[-1] != '-' && *inptr >= 'A' && *inptr <= 'Z')
			*inptr += 0x20;

		inptr++;
	}
}

void
e_mail_formatter_format_header (EMailFormatter *formatter,
                                GString *buffer,
                                const gchar *header_name,
                                const gchar *header_value,
                                guint32 flags,
                                const gchar *charset)
{
	gchar *canon_name, *buf, *value = NULL;
	const gchar *label, *txt;
	gboolean addrspec = FALSE;
	gchar *str_field = NULL;
	gint i;

	g_return_if_fail (E_IS_MAIL_FORMATTER (formatter));
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (header_name != NULL);
	g_return_if_fail (header_value != NULL);

	canon_name = g_alloca (strlen (header_name) + 1);
	strcpy (canon_name, header_name);
	e_mail_formatter_canon_header_name (canon_name);

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (g_ascii_strcasecmp (canon_name, addrspec_hdrs[i]) == 0) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(canon_name);

	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;
		gchar *img;
		gchar *fmt_charset;

		fmt_charset = e_mail_formatter_dup_charset (formatter);
		if (fmt_charset == NULL)
			fmt_charset = e_mail_formatter_dup_default_charset (formatter);

		buf = camel_header_unfold (header_value);
		addrs = camel_header_address_decode (buf, fmt_charset);
		if (addrs == NULL) {
			g_free (fmt_charset);
			g_free (buf);
			return;
		}

		g_free (fmt_charset);
		g_free (buf);

		html = g_string_new ("");
		img = e_mail_formatter_format_address (
			formatter, html, addrs, label,
			(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS),
			!(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

		if (img != NULL) {
			str_field = g_strdup_printf ("%s: %s", label, img);
			label = str_field;
			flags |= E_MAIL_FORMATTER_HEADER_FLAG_NODEC;
			g_free (img);
		}

		camel_header_address_list_clear (&addrs);
		txt = value = g_string_free (html, FALSE);

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_HTML;
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "Subject")) {
		buf = camel_header_unfold (header_value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "X-Evolution-Mailer")) {
		/* pseudo-header */
		label = _("Mailer");
		buf = camel_header_unfold (header_value);
		txt = value = camel_header_format_ctext (buf, charset);
		g_free (buf);
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "Date") ||
		   g_str_equal (canon_name, "Resent-Date")) {
		CamelMimeFilterToHTMLFlags text_format_flags;
		gint msg_offset, local_tz;
		time_t msg_date;
		struct tm local;
		gchar *html;
		gboolean hide_real_date;

		hide_real_date = !e_mail_formatter_get_show_real_date (formatter);

		txt = header_value;
		while (*txt == ' ' || *txt == '\t')
			txt++;

		text_format_flags =
			e_mail_formatter_get_text_format_flags (formatter);

		html = camel_text_to_html (txt, text_format_flags, 0);

		msg_date = camel_header_decode_date (txt, &msg_offset);
		e_localtime_with_offset (msg_date, &local, &local_tz);

		/* Convert message offset to minutes (e.g. -0400 --> -240) */
		msg_offset = ((msg_offset / 100) * 60) + (msg_offset % 100);
		/* Turn into offset from localtime, not UTC */
		msg_offset -= local_tz / 60;

		/* value will be freed at the end */
		if (!hide_real_date && !msg_offset) {
			/* No timezone difference; just
			 * show the real Date: header. */
			txt = value = html;
		} else {
			gchar *date_str;

			date_str = e_datetime_format_format (
				"mail", "header",
				DTFormatKindDateTime, msg_date);

			if (hide_real_date) {
				/* Show only the local-formatted date, losing
				 * all timezone information like Outlook does.
				 * Should we attempt to show it somehow? */
				txt = value = date_str;
			} else {
				txt = value = g_strdup_printf (
					"%s (<I>%s</I>)", html, date_str);
				g_free (date_str);
			}
			g_free (html);
		}

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_HTML;
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "Newsgroups")) {
		GSList *ng, *scan;
		GString *html;

		buf = camel_header_unfold (header_value);

		if (!(ng = camel_header_newsgroups_decode (buf))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new ("");
		scan = ng;
		while (scan) {
			const gchar *newsgroup = scan->data;

			if (flags & E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS)
				g_string_append_printf (
					html, "%s", newsgroup);
			else
				g_string_append_printf (
					html, "<a href=\"news:%s\">%s</a>",
					newsgroup, newsgroup);
			scan = g_slist_next (scan);
			if (scan)
				g_string_append_printf (html, ", ");
		}

		g_slist_free_full (ng, g_free);

		txt = value = g_string_free (html, FALSE);

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_HTML;
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "Received") ||
		   g_str_has_prefix (canon_name, "X-") ||
		   g_str_has_prefix (canon_name, "Dkim-") ||
		   g_str_has_prefix (canon_name, "Arc-")) {
		/* don't unfold Received nor extension headers */
		txt = value = camel_header_decode_string (header_value, charset);
	} else {
		buf = camel_header_unfold (header_value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);
	}

	e_mail_formatter_format_text_header (
		formatter, buffer, label, txt, flags);

	g_free (value);
	g_free (str_field);
}

GList *
e_mail_formatter_find_rfc822_end_iter (GList *rfc822_start_iter)
{
	GList *link = rfc822_start_iter;
	EMailPart *part;
	const gchar *part_id;
	gchar *end;

	g_return_val_if_fail (rfc822_start_iter != NULL, NULL);

	part = E_MAIL_PART (link->data);

	part_id = e_mail_part_get_id (part);
	g_return_val_if_fail (part_id != NULL, NULL);

	end = g_strconcat (part_id, ".end", NULL);

	while (link != NULL) {
		part = E_MAIL_PART (link->data);

		part_id = e_mail_part_get_id (part);
		g_return_val_if_fail (part_id != NULL, NULL);

		if (g_strcmp0 (part_id, end) == 0)
			break;

		link = g_list_next (link);
	}

	g_free (end);

	return link;
}

gchar *
e_mail_formatter_parse_html_mnemonics (const gchar *label,
                                       gchar **out_access_key)
{
	const gchar *pos = NULL;
	GString *html_label = NULL;

	g_return_val_if_fail (label != NULL, NULL);

	if (out_access_key != NULL)
		*out_access_key = NULL;

	if (!g_utf8_validate (label, -1, NULL)) {
		gchar *res = g_strdup (label);

		g_return_val_if_fail (g_utf8_validate (label, -1, NULL), res);

		return res;
	}

	pos = strstr (label, "_");
	if (pos != NULL) {
		gunichar uk;

		html_label = g_string_new ("");
		g_string_append_len (html_label, label, pos - label);

		pos++;
		uk = g_utf8_get_char (pos);

		pos = g_utf8_next_char (pos);

		g_string_append (html_label, "<u>");
		g_string_append_unichar (html_label, uk);
		g_string_append (html_label, "</u>");
		g_string_append (html_label, pos);

		if (out_access_key != NULL && uk != 0) {
			gchar ukstr[10];
			gint len;

			len = g_unichar_to_utf8 (g_unichar_toupper (uk), ukstr);
			if (len > 0)
				*out_access_key = g_strndup (ukstr, len);
		}

	} else {
		html_label = g_string_new (label);
	}

	return g_string_free (html_label, FALSE);
}

void
e_mail_formatter_format_security_header (EMailFormatter *formatter,
                                         EMailFormatterContext *context,
                                         GString *buffer,
                                         EMailPart *part,
                                         guint32 flags)
{
	struct _validity_flags {
		guint32 flags;
		const gchar *description_complete;
		const gchar *description_partial;
	} validity_flags[] = {
		{ E_MAIL_PART_VALIDITY_PGP | E_MAIL_PART_VALIDITY_SIGNED, N_("GPG signed"), N_("partially GPG signed") },
		{ E_MAIL_PART_VALIDITY_PGP | E_MAIL_PART_VALIDITY_ENCRYPTED, N_("GPG encrypted"), N_("partially GPG encrypted") },
		{ E_MAIL_PART_VALIDITY_SMIME | E_MAIL_PART_VALIDITY_SIGNED, N_("S/MIME signed"), N_("partially S/MIME signed") },
		{ E_MAIL_PART_VALIDITY_SMIME | E_MAIL_PART_VALIDITY_ENCRYPTED, N_("S/MIME encrypted"), N_("partially S/MIME encrypted") }
	};
	const gchar *part_id;
	gchar *part_id_prefix;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	guint32 check_valid_flags = 0;
	gint part_id_prefix_len;
	gboolean is_partial = FALSE;
	guint ii;

	g_return_if_fail (E_IS_MAIL_PART_HEADERS (part));

	/* Get prefix of this PURI */
	part_id = e_mail_part_get_id (part);
	part_id_prefix = g_strndup (part_id, g_strrstr (part_id, ".") - part_id);
	part_id_prefix_len = strlen (part_id_prefix);

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	/* Ignore the main message, the headers and the end parts */
	#define should_skip_part(_id) \
		(g_strcmp0 (_id, part_id_prefix) == 0 || \
		(_id && g_str_has_suffix (_id, ".rfc822.end")) || \
		(_id && strlen (_id) == part_id_prefix_len + 8 /* strlen (".headers") */ && \
		g_strcmp0 (_id + part_id_prefix_len, ".headers") == 0))

	/* Check parts for this ID. */
	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = link->data;
		const gchar *id = e_mail_part_get_id (mail_part);

		if (!e_mail_part_id_has_prefix (mail_part, part_id_prefix))
			continue;

		if (should_skip_part (id))
			continue;

		if (!e_mail_part_has_validity (mail_part)) {
			/* A part without validity, thus it's partially signed/encrypted */
			is_partial = TRUE;
		} else {
			guint32 validies = 0;
			for (ii = 0; ii < G_N_ELEMENTS (validity_flags); ii++) {
				if (e_mail_part_get_validity (mail_part, validity_flags[ii].flags))
					validies |= validity_flags[ii].flags;
			}
			check_valid_flags |= validies;
		}

		/* Do not traverse sub-messages */
		if (g_str_has_suffix (e_mail_part_get_id (mail_part), ".rfc822") &&
		    !g_str_equal (e_mail_part_get_id (mail_part), part_id_prefix))
			link = e_mail_formatter_find_rfc822_end_iter (link);
	}

	if (check_valid_flags) {
		GString *tmp;

		if (!is_partial) {
			for (link = head; link != NULL && !is_partial; link = g_list_next (link)) {
				EMailPart *mail_part = link->data;
				const gchar *id = e_mail_part_get_id (mail_part);

				if (!e_mail_part_id_has_prefix (mail_part, part_id_prefix))
					continue;

				if (should_skip_part (id))
					continue;

				if (!e_mail_part_has_validity (mail_part)) {
					/* A part without validity, thus it's partially signed/encrypted */
					is_partial = TRUE;
					break;
				}

				is_partial = !e_mail_part_get_validity (mail_part, check_valid_flags);

				/* Do not traverse sub-messages */
				if (g_str_has_suffix (e_mail_part_get_id (mail_part), ".rfc822") &&
				    !g_str_equal (e_mail_part_get_id (mail_part), part_id_prefix))
					link = e_mail_formatter_find_rfc822_end_iter (link);
			}
		}

		/* Add encryption/signature header */
		tmp = g_string_new ("");

		for (link = head; link; link = g_list_next (link)) {
			EMailPart *mail_part = link->data;
			const gchar *id = e_mail_part_get_id (mail_part);

			if (!e_mail_part_has_validity (mail_part) ||
			    !e_mail_part_id_has_prefix (mail_part, part_id_prefix))
				continue;

			if (should_skip_part (id))
				continue;

			for (ii = 0; ii < G_N_ELEMENTS (validity_flags); ii++) {
				if (e_mail_part_get_validity (mail_part, validity_flags[ii].flags)) {
					if (tmp->len > 0)
						g_string_append (tmp, ", ");
					g_string_append (tmp, is_partial ? _(validity_flags[ii].description_partial) : _(validity_flags[ii].description_complete));
				}
			}

			break;
		}

		if (tmp->len > 0)
			e_mail_formatter_format_header (formatter, buffer, _("Security"), tmp->str, flags, "UTF-8");

		g_string_free (tmp, TRUE);
	}

	#undef should_skip_part

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	g_free (part_id_prefix);
}
