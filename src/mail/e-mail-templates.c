/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2008 - Diego Escalante Urrelo
 * Copyright (C) 2018 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *		Diego Escalante Urrelo <diegoe@gnome.org>
 *		Bharath Acharya <abharath@novell.com>
 */

#include "evolution-config.h"

#include <string.h>

#include "e-util/e-util.h"
#include "em-format/e-mail-stripsig-filter.h"

#include "em-composer-utils.h"
#include "em-utils.h"

#include "e-mail-templates.h"

static void
replace_in_string (GString *text,
		   const gchar *find,
		   const gchar *replacement)
{
	const gchar *p, *next;
	GString *str;
	gint find_len;

	g_return_if_fail (text != NULL);
	g_return_if_fail (find != NULL);

	find_len = strlen (find);
	str = g_string_new ("");
	p = text->str;
	while (next = e_util_strstrcase (p, find), next) {
		if (p < next)
			g_string_append_len (str, p, next - p);
		if (replacement && *replacement)
			g_string_append (str, replacement);
		p = next + find_len;
	}

	/* Avoid unnecessary allocation when the 'text' doesn't contain the variable */
	if (p != text->str) {
		g_string_append (str, p);
		g_string_assign (text, str->str);
	}

	g_string_free (str, TRUE);
}

/* Replaces $ORIG[variable] in given template by given replacement from the original message */
static void
replace_template_variable (GString *text,
                           const gchar *variable,
                           const gchar *replacement)
{
	gchar *find;

	g_return_if_fail (text != NULL);
	g_return_if_fail (variable != NULL);
	g_return_if_fail (*variable);

	find = g_strconcat ("$ORIG[", variable, "]", NULL);

	replace_in_string (text, find, replacement);

	g_free (find);
}

static void
replace_user_variables (GString *text,
			CamelMimeMessage *source_message)
{
	CamelInternetAddress *to;
	const gchar *name, *addr;
	GSettings *settings;
	gchar **strv;
	gint ii;

	g_return_if_fail (text);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (source_message));

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.templates");
	strv = g_settings_get_strv (settings, "template-placeholders");
	g_object_unref (settings);

	for (ii = 0; strv && strv[ii]; ii++) {
		gchar *equal_sign, *find, *var_name = strv[ii];
		const gchar *var_value;

		equal_sign = strchr (var_name, '=');
		if (!equal_sign)
			continue;

		*equal_sign = '\0';
		var_value = equal_sign + 1;

		find = g_strconcat ("$", var_name, NULL);
		replace_in_string (text, find, var_value);
		g_free (find);

		*equal_sign = '=';
	}

	g_strfreev (strv);

	to = camel_mime_message_get_recipients (source_message, CAMEL_RECIPIENT_TYPE_TO);
	if (to && camel_internet_address_get (to, 0, &name, &addr)) {
		replace_in_string (text, "$sender_name", name);
		replace_in_string (text, "$sender_email", addr);
	}
}

static void
replace_email_addresses (GString *template,
                         CamelInternetAddress *internet_address,
                         const gchar *variable)
{
	gint address_index = 0;
	GString *emails = g_string_new ("");
	const gchar *address_name, *address_email;

	g_return_if_fail (template);
	g_return_if_fail (internet_address);
	g_return_if_fail (variable);

	while (camel_internet_address_get (internet_address, address_index, &address_name, &address_email)) {
		gchar *address = camel_internet_address_format_address (address_name, address_email);

		if (address_index > 0)
			g_string_append_printf (emails, ", %s", address);
		else
			g_string_append_printf (emails, "%s", address);

		address_index++;
		g_free (address);
	}
	replace_template_variable (template, variable, emails->str);
	g_string_free (emails, TRUE);
}

static ESource *
ref_identity_source_from_message_and_folder (CamelMimeMessage *message,
					     CamelFolder *folder,
					     const gchar *message_uid)
{
	EShell *shell;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	shell = e_shell_get_default ();
	if (!shell)
		return NULL;

	return em_composer_utils_guess_identity_source (shell, message, folder, message_uid, NULL, NULL);
}

static CamelMimePart *
fill_template (CamelMimeMessage *message,
	       CamelFolder *source_folder,
	       const gchar *source_message_uid,
	       CamelFolder *templates_folder,
               CamelMimePart *template)
{
	const CamelNameValueArray *headers;
	CamelContentType *ct;
	CamelStream *stream;
	CamelMimePart *return_part;
	CamelMimePart *message_part = NULL;
	CamelDataWrapper *dw;
	CamelInternetAddress *internet_address;
	GString *template_body;
	GByteArray *byte_array;
	gint i;
	guint jj, len;
	gboolean message_html, template_html, template_markdown;
	gboolean has_quoted_body;

	ct = camel_mime_part_get_content_type (template);
	template_html = ct && camel_content_type_is (ct, "text", "html");
	template_markdown = ct && camel_content_type_is (ct, "text", "markdown");

	message_html = FALSE;
	/* When template is html, then prefer HTML part of the original message. Otherwise go for plaintext */
	dw = camel_medium_get_content (CAMEL_MEDIUM (message));
	if (CAMEL_IS_MULTIPART (dw)) {
		CamelMultipart *multipart = CAMEL_MULTIPART (dw);

		for (i = 0; i < camel_multipart_get_number (multipart); i++) {
			CamelMimePart *part = camel_multipart_get_part (multipart, i);

			ct = camel_mime_part_get_content_type (part);
			if (!ct)
				continue;

			if (camel_content_type_is (ct, "text", "html") && template_html) {
				message_part = camel_multipart_get_part (multipart, i);
				message_html = TRUE;
				break;
			} else if (!message_html && (camel_content_type_is (ct, "text", "plain") || camel_content_type_is (ct, "text", "markdown"))) {
				message_part = camel_multipart_get_part (multipart, i);
			}
		}
	} else {
		CamelContentType *mpct;

		message_part = CAMEL_MIME_PART (message);

		mpct = camel_mime_part_get_content_type (message_part);
		message_html = mpct && camel_content_type_is (mpct, "text", "html");
	}

	/* Get content of the template */
	stream = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (template)), stream, NULL, NULL);
	camel_stream_flush (stream, NULL, NULL);
	byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (stream));
	template_body = g_string_new_len ((gchar *) byte_array->data, byte_array->len);
	g_object_unref (stream);

	/* Replace all $ORIG[header_name] by respective values */
	headers = camel_medium_get_headers (CAMEL_MEDIUM (message));
	len = camel_name_value_array_get_length (headers);
	for (jj = 0; jj < len; jj++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (headers, jj, &header_name, &header_value) ||
		    !header_name)
			continue;

		if (g_ascii_strncasecmp (header_name, "content-", 8) != 0 &&
		    g_ascii_strcasecmp (header_name, "to") != 0 &&
		    g_ascii_strcasecmp (header_name, "cc") != 0 &&
		    g_ascii_strcasecmp (header_name, "bcc") != 0 &&
		    g_ascii_strcasecmp (header_name, "from") != 0 &&
		    g_ascii_strcasecmp (header_name, "subject") != 0)
			replace_template_variable (template_body, header_name, header_value);
	}

	/* Now manually replace the *subject* header. The header->value for subject header could be
	 * base64 encoded, so let camel_mime_message to decode it for us if needed */
	replace_template_variable (template_body, "subject", camel_mime_message_get_subject (message));

	/* Replace TO and FROM modifiers. */
	internet_address = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	replace_email_addresses (template_body, internet_address, "to");

	internet_address = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	replace_email_addresses (template_body, internet_address, "cc");

	internet_address = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);
	replace_email_addresses (template_body, internet_address, "bcc");

	internet_address = camel_mime_message_get_from (message);
	replace_email_addresses (template_body, internet_address, "from");

	has_quoted_body = e_util_strstrcase (template_body->str, "$ORIG[quoted-body]") != NULL;
	if (has_quoted_body && !template_html) {
		gchar *html;

		template_html = TRUE;

		html = camel_text_to_html (
			template_body->str,
			CAMEL_MIME_FILTER_TOHTML_DIV |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
			CAMEL_MIME_FILTER_TOHTML_MARK_CITATION |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
		g_string_assign (template_body, html);
		g_free (html);

		g_string_append (template_body, "<!-- disable-format-prompt -->");
	}

	if (!template_markdown)
		g_string_append (template_body, "<span id=\"x-evo-template-fix-paragraphs\"></span>");

	/* Now extract body of the original message and replace the $ORIG[body] modifier in template */
	if (message_part && (has_quoted_body || e_util_strstrcase (template_body->str, "$ORIG[body]"))) {
		GString *message_body, *message_body_nosig = NULL;
		CamelStream *mem_stream;

		stream = camel_stream_mem_new ();
		mem_stream = stream;

		ct = camel_mime_part_get_content_type (message_part);
		if (ct) {
			const gchar *charset = camel_content_type_param (ct, "charset");
			if (charset && *charset) {
				CamelMimeFilter *filter = camel_mime_filter_charset_new (charset, "UTF-8");
				if (filter) {
					CamelStream *filtered = camel_stream_filter_new (stream);

					if (filtered) {
						camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered), filter);
						g_object_unref (stream);
						stream = filtered;
					}

					g_object_unref (filter);
				}
			}
		}

		camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (message_part)), stream, NULL, NULL);
		camel_stream_flush (stream, NULL, NULL);
		byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (mem_stream));
		message_body = g_string_new_len ((gchar *) byte_array->data, byte_array->len);
		g_object_unref (stream);

		if (has_quoted_body) {
			CamelMimeFilter *filter;

			stream = camel_stream_mem_new ();
			mem_stream = stream;

			ct = camel_mime_part_get_content_type (message_part);
			if (ct) {
				const gchar *charset = camel_content_type_param (ct, "charset");
				if (charset && *charset) {
					filter = camel_mime_filter_charset_new (charset, "UTF-8");
					if (filter) {
						CamelStream *filtered = camel_stream_filter_new (stream);

						if (filtered) {
							camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered), filter);
							g_object_unref (stream);
							stream = filtered;
						}

						g_object_unref (filter);
					}
				}
			}

			filter = e_mail_stripsig_filter_new (!message_html);
			if (filter) {
				CamelStream *filtered = camel_stream_filter_new (stream);

				if (filtered) {
					camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered), filter);
					g_object_unref (stream);
					stream = filtered;
				}

				g_object_unref (filter);
			}

			camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (message_part)), stream, NULL, NULL);
			camel_stream_flush (stream, NULL, NULL);
			byte_array = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (mem_stream));
			message_body_nosig = g_string_new_len ((gchar *) byte_array->data, byte_array->len);
			g_object_unref (stream);
		}

		if (template_html && !message_html) {
			gchar *html = camel_text_to_html (
				message_body->str,
				CAMEL_MIME_FILTER_TOHTML_PRE |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
			replace_template_variable (template_body, "body", html);
			g_free (html);
		} else if (!template_html && message_html) {
			gchar *html;

			g_string_prepend (message_body, "<pre>");
			g_string_append (message_body, "</pre>");

			template_html = TRUE;

			html = camel_text_to_html (
				template_body->str,
				CAMEL_MIME_FILTER_TOHTML_DIV |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
			g_string_assign (template_body, html);
			g_free (html);

			replace_template_variable (template_body, "body", message_body->str);
		} else { /* Other cases should not occur. And even if they happen to do, there's nothing we can really do about it */
			replace_template_variable (template_body, "body", message_body->str);
		}

		if (has_quoted_body) {
			if (!message_html) {
				gchar *html = camel_text_to_html (
					message_body_nosig->str,
					CAMEL_MIME_FILTER_TOHTML_PRE |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
					CAMEL_MIME_FILTER_TOHTML_QUOTE_CITATION |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
				g_string_assign (message_body_nosig, html);
				g_free (html);
			}

			g_string_prepend (message_body_nosig, "<blockquote type=\"cite\">");
			g_string_append (message_body_nosig, "</blockquote>");

			replace_template_variable (template_body, "quoted-body", message_body_nosig->str);
		}

		if (message_body_nosig)
			g_string_free (message_body_nosig, TRUE);

		g_string_free (message_body, TRUE);
	} else {
		replace_template_variable (template_body, "body", "");
		replace_template_variable (template_body, "quoted-body", "");
	}

	if (e_util_strstrcase (template_body->str, "$ORIG[reply-credits]")) {
		ESource *identity_source;
		gchar *reply_credits;

		identity_source = ref_identity_source_from_message_and_folder (message, source_folder, source_message_uid);

		reply_credits = em_composer_utils_get_reply_credits (identity_source, message);

		if (reply_credits && template_html) {
			gchar *html = camel_text_to_html (
				reply_credits,
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
			g_free (reply_credits);
			reply_credits = html;
		}

		replace_template_variable (template_body, "reply-credits", reply_credits ? reply_credits : "");

		g_clear_object (&identity_source);
		g_free (reply_credits);
	}

	replace_user_variables (template_body, message);

	return_part = camel_mime_part_new ();

	if (template_html)
		camel_mime_part_set_content (return_part, template_body->str, template_body->len, "text/html");
	else if (template_markdown)
		camel_mime_part_set_content (return_part, template_body->str, template_body->len, "text/markdown");
	else
		camel_mime_part_set_content (return_part, template_body->str, template_body->len, "text/plain");

	g_string_free (template_body, TRUE);

	return return_part;
}

static CamelMimePart *
find_template_part_in_multipart (CamelMultipart *multipart,
				 CamelMultipart *new_multipart)
{
	CamelMimePart *template_part = NULL;
	gint ii;

	for (ii = 0; ii < camel_multipart_get_number (multipart); ii++) {
		CamelMimePart *part = camel_multipart_get_part (multipart, ii);
		CamelContentType *ct = camel_mime_part_get_content_type (part);

		if (!template_part && ct && camel_content_type_is (ct, "multipart", "*")) {
			CamelDataWrapper *dw;

			dw = camel_medium_get_content (CAMEL_MEDIUM (part));
			template_part = (dw && CAMEL_IS_MULTIPART (dw)) ?
				find_template_part_in_multipart (CAMEL_MULTIPART (dw), new_multipart) : NULL;

			if (!template_part) {
				/* Copy any other parts (attachments...) to the output message */
				camel_mime_part_set_disposition (part, "attachment");
				camel_multipart_add_part (new_multipart, part);
			}
		} else if (ct && camel_content_type_is (ct, "text", "html")) {
			template_part = part;
		} else if (!template_part && ct && (camel_content_type_is (ct, "text", "plain") || camel_content_type_is (ct, "text", "markdown"))) {
			template_part = part;
		} else {
			/* Copy any other parts (attachments...) to the output message */
			camel_mime_part_set_disposition (part, "attachment");
			camel_multipart_add_part (new_multipart, part);
		}
	}

	return template_part;
}

CamelMimeMessage *
e_mail_templates_apply_sync (CamelMimeMessage *source_message,
			     CamelFolder *source_folder,
			     const gchar *source_message_uid,
			     CamelFolder *templates_folder,
			     const gchar *templates_message_uid,
			     GCancellable *cancellable,
			     GError **error)
{
	CamelMimeMessage *template_message, *result_message = NULL;
	CamelMultipart *new_multipart;
	CamelDataWrapper *dw;
	const CamelNameValueArray *headers;
	CamelMimePart *template_part = NULL;
	const gchar *tmp_value;
	gchar *references, *message_id;
	guint ii, len;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (source_message), NULL);
	g_return_val_if_fail (CAMEL_IS_FOLDER (templates_folder), NULL);
	g_return_val_if_fail (templates_message_uid != NULL, NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return NULL;

	template_message = camel_folder_get_message_sync (templates_folder, templates_message_uid, cancellable, error);
	if (!template_message)
		return NULL;

	result_message = camel_mime_message_new ();
	new_multipart = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (new_multipart), "multipart/alternative");
	camel_multipart_set_boundary (new_multipart, NULL);

	dw = camel_medium_get_content (CAMEL_MEDIUM (template_message));
	/* If template is a multipart, then try to use HTML. When no HTML part is available, use plaintext. Every other
	 * add as an attachment */
	if (CAMEL_IS_MULTIPART (dw)) {
		template_part = find_template_part_in_multipart (CAMEL_MULTIPART (dw), new_multipart);
	} else {
		CamelContentType *ct = camel_mime_part_get_content_type (CAMEL_MIME_PART (template_message));

		if (ct && (camel_content_type_is (ct, "text", "html") ||
		    camel_content_type_is (ct, "text", "plain") ||
		    camel_content_type_is (ct, "text", "markdown"))) {
			template_part = CAMEL_MIME_PART (template_message);
		}
	}

	g_warn_if_fail (template_part != NULL);

	if (template_part) {
		CamelMimePart *out_part = NULL;

		/* Here replace all the modifiers in template body by values
		   from message and return the newly created part */
		out_part = fill_template (source_message, source_folder, source_message_uid, templates_folder, template_part);

		/* Assigning part directly to mime_message causes problem with
		   "Content-type" header displaying in the HTML message (camel parsing bug?) */
		camel_multipart_add_part (new_multipart, out_part);
		g_object_unref (out_part);
	}

	camel_medium_set_content (CAMEL_MEDIUM (result_message), CAMEL_DATA_WRAPPER (new_multipart));

	/* Add the headers from the message we are replying to, so CC and that
	 * stuff is preserved. Also replace any $ORIG[header-name] modifiers ignoring
	 * 'content-*' headers */
	headers = camel_medium_get_headers (CAMEL_MEDIUM (source_message));
	len = camel_name_value_array_get_length (headers);
	for (ii = 0; ii < len; ii++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (headers, ii, &header_name, &header_value) ||
		    !header_name)
			continue;

		if (g_ascii_strncasecmp (header_name, "content-", 8) != 0 &&
		    g_ascii_strcasecmp (header_name, "from") != 0 &&
		    g_ascii_strcasecmp (header_name, "Message-ID") != 0 &&
		    g_ascii_strcasecmp (header_name, "In-Reply-To") != 0 &&
		    g_ascii_strcasecmp (header_name, "References") != 0) {
			gchar *new_header_value = NULL;

			/* Some special handling of the 'subject' header */
			if (g_ascii_strncasecmp (header_name, "subject", 7) == 0) {
				GString *subject = g_string_new (camel_mime_message_get_subject (template_message));
				guint jj;

				/* Now replace all possible $ORIG[]s in the subject line by values from original message */
				for (jj = 0; jj < len; jj++) {
					const gchar *m_header_name = NULL, *m_header_value = NULL;

					if (camel_name_value_array_get (headers, jj, &m_header_name, &m_header_value) &&
					    m_header_name &&
					    g_ascii_strncasecmp (m_header_name, "content-", 8) != 0 &&
					    g_ascii_strcasecmp (m_header_name, "subject") != 0)
						replace_template_variable (subject, m_header_name, m_header_value);
				}

				/* Now replace $ORIG[subject] variable, handling possible base64 encryption */
				replace_template_variable (
					subject, "subject",
					camel_mime_message_get_subject (source_message));

				replace_user_variables (subject, source_message);

				new_header_value = g_string_free (subject, FALSE);
			}

			camel_medium_add_header (CAMEL_MEDIUM (result_message), header_name, new_header_value ? new_header_value : header_value);

			g_free (new_header_value);
		}
	}

	/* Set the To: field to the same To: field of the message we are replying to. */
	camel_mime_message_set_recipients (
		result_message, CAMEL_RECIPIENT_TYPE_TO,
		camel_mime_message_get_reply_to (source_message) ? camel_mime_message_get_reply_to (source_message) :
		camel_mime_message_get_from (source_message));

	/* Copy the recipients from the template. */
	camel_mime_message_set_recipients (result_message, CAMEL_RECIPIENT_TYPE_CC,
		camel_mime_message_get_recipients (template_message, CAMEL_RECIPIENT_TYPE_CC));

	camel_mime_message_set_recipients (result_message, CAMEL_RECIPIENT_TYPE_BCC,
		camel_mime_message_get_recipients (template_message, CAMEL_RECIPIENT_TYPE_BCC));

	if (camel_mime_message_get_reply_to (template_message))
		camel_mime_message_set_reply_to (result_message, camel_mime_message_get_reply_to (template_message));

	/* Add In-Reply-To and References. */

	message_id = camel_header_unfold (camel_medium_get_header (CAMEL_MEDIUM (source_message), "Message-ID"));
	references = camel_header_unfold (camel_medium_get_header (CAMEL_MEDIUM (source_message), "References"));

	if (message_id && *message_id) {
		gchar *reply_refs;

		camel_medium_add_header (CAMEL_MEDIUM (result_message), "In-Reply-To", message_id);

		if (references)
			reply_refs = g_strdup_printf ("%s %s", references, message_id);
		else
			reply_refs = NULL;

		camel_medium_add_header (CAMEL_MEDIUM (result_message), "References", reply_refs ? reply_refs : message_id);

		g_free (reply_refs);

	} else if (references && *references) {
		camel_medium_add_header (CAMEL_MEDIUM (result_message), "References", references);
	}

	/* inherit composer mode from the template */
	tmp_value = camel_medium_get_header (CAMEL_MEDIUM (template_message), "X-Evolution-Composer-Mode");
	if (tmp_value && *tmp_value)
		camel_medium_set_header (CAMEL_MEDIUM (result_message), "X-Evolution-Composer-Mode", tmp_value);

	g_free (message_id);
	g_free (references);

	g_clear_object (&template_message);
	g_clear_object (&new_multipart);

	return result_message;
}

typedef struct _AsyncContext {
	CamelMimeMessage *source_message;
	CamelFolder *source_folder;
	CamelFolder *templates_folder;
	gchar *source_message_uid;
	gchar *templates_message_uid;
	CamelMimeMessage *result_message;
} AsyncContext;

static void
async_context_free (gpointer ptr)
{
	AsyncContext *context = ptr;

	if (context) {
		g_clear_object (&context->source_message);
		g_clear_object (&context->source_folder);
		g_clear_object (&context->templates_folder);
		g_clear_object (&context->result_message);
		g_free (context->source_message_uid);
		g_free (context->templates_message_uid);
		g_slice_free (AsyncContext, context);
	}
}

static void
e_mail_templates_apply_thread (ESimpleAsyncResult *simple,
			       gpointer source_object,
			       GCancellable *cancellable)
{
	AsyncContext *context;
	GError *local_error = NULL;

	context = e_simple_async_result_get_op_pointer (simple);
	g_return_if_fail (context != NULL);

	context->result_message = e_mail_templates_apply_sync (
		context->source_message, context->source_folder, context->source_message_uid,
		context->templates_folder, context->templates_message_uid,
		cancellable, &local_error);

	if (local_error)
		e_simple_async_result_take_error (simple, local_error);
}

void
e_mail_templates_apply (CamelMimeMessage *source_message,
			CamelFolder *source_folder,
			const gchar *source_message_uid,
			CamelFolder *templates_folder,
			const gchar *templates_message_uid,
			GCancellable *cancellable,
			GAsyncReadyCallback callback,
			gpointer user_data)
{
	ESimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (source_message));
	g_return_if_fail (CAMEL_IS_FOLDER (templates_folder));
	g_return_if_fail (templates_message_uid != NULL);
	g_return_if_fail (callback != NULL);

	context = g_slice_new0 (AsyncContext);
	context->source_message = g_object_ref (source_message);
	context->source_folder = source_folder ? g_object_ref (source_folder) : NULL;
	context->source_message_uid = g_strdup (source_message_uid);
	context->templates_folder = g_object_ref (templates_folder);
	context->templates_message_uid = g_strdup (templates_message_uid);
	context->result_message = NULL;

	simple = e_simple_async_result_new (G_OBJECT (source_message), callback,
		user_data, e_mail_templates_apply);

	e_simple_async_result_set_op_pointer (simple, context, (GDestroyNotify) async_context_free);

	e_simple_async_result_run_in_thread (simple, G_PRIORITY_DEFAULT, e_mail_templates_apply_thread, cancellable);

	g_object_unref (simple);
}

CamelMimeMessage *
e_mail_templates_apply_finish (GObject *source_object,
			       GAsyncResult *result,
			       GError **error)
{
	ESimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (e_simple_async_result_is_valid (result, source_object, e_mail_templates_apply), NULL);

	simple = E_SIMPLE_ASYNC_RESULT (result);
	context = e_simple_async_result_get_op_pointer (simple);

	if (e_simple_async_result_propagate_error (simple, error))
		return NULL;

	return context->result_message ? g_object_ref (context->result_message) : NULL;
}
