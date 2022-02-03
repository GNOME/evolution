/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#ifdef HAVE_MARKDOWN
#include <cmark.h>
#endif

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

#include "e-misc-utils.h"

#include "e-markdown-utils.h"

#define dd(x)

/**
 * e_markdown_utils_text_to_html:
 * @plain_text: plain text with markdown to convert to HTML
 * @length: length of the @plain_text, or -1 when it's nul-terminated
 *
 * Convert @plain_text, possibly with markdown, into the HTML.
 *
 * Note: The function can return %NULL when was not built
 *    with the markdown support.
 *
 * Returns: (transfer full) (nullable): text converted into HTML,
 *    or %NULL, when was not built with the markdown support.
 *    Free the string with g_free(), when no longer needed.
 *
 * Since: 3.44
 **/
gchar *
e_markdown_utils_text_to_html (const gchar *plain_text,
			       gssize length)
{
	#ifdef HAVE_MARKDOWN
	GString *html;
	gchar *converted;

	if (length == -1)
		length = plain_text ? strlen (plain_text) : 0;

	converted = cmark_markdown_to_html (plain_text ? plain_text : "", length,
		CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE);

	html = e_str_replace_string (converted, "<blockquote>", "<blockquote type=\"cite\">");

	g_free (converted);

	return g_string_free (html, FALSE);
	#else
	return NULL;
	#endif
}

static const gchar *
markdown_utils_get_attribute_value (const xmlChar **xcattrs,
				    const gchar *name)
{
	gint ii;

	if (!xcattrs)
		return NULL;

	for (ii = 0; xcattrs[ii] && xcattrs[ii + 1]; ii += 2) {
		if (g_ascii_strcasecmp (name, (const gchar *) xcattrs[ii]) == 0)
			return (const gchar *) xcattrs[ii + 1];
	}

	return NULL;
}

struct _ComposerQuirks {
	gboolean enabled;
	gboolean reading_html_end;
	gchar *to_body_credits;
	gboolean cite_body;
};

static void
markdown_utils_apply_composer_quirks (GString *buffer,
				      struct _ComposerQuirks *quirks)
{
	if (!quirks || !quirks->enabled)
		return;

	if (quirks->cite_body) {
		gint ii;

		g_string_insert (buffer, 0, "> ");

		for (ii = 0; ii < buffer->len; ii++) {
			if (buffer->str[ii] == '\n' && ii + 1 < buffer->len) {
				g_string_insert (buffer, ii + 1, "> ");
				ii += 2;
			}
		}
	}

	if (quirks->to_body_credits) {
		g_string_insert (buffer, 0, "\n");
		g_string_insert (buffer, 0, quirks->to_body_credits);
	}
}

typedef struct _HTMLToTextData {
	GString *buffer;
	gboolean in_body;
	gint in_code;
	gint in_pre;
	gint in_paragraph;
	gboolean in_paragraph_end;
	gboolean in_li;
	GString *quote_prefix;
	gchar *href;
	GString *link_text;
	GSList *list_index; /* gint; -1 for unordered list */
	gboolean plain_text;
	struct _ComposerQuirks composer_quirks;
} HTMLToTextData;

static void
markdown_utils_sax_start_element_cb (gpointer ctx,
				     const xmlChar *xcname,
				     const xmlChar **xcattrs)
{
	HTMLToTextData *data = ctx;
	const gchar *name = (const gchar *) xcname;
	#if dd(1)+0
	{
		gint ii;

		printf ("%s: '%s'\n", G_STRFUNC, name);
		for (ii = 0; xcattrs && xcattrs[ii]; ii++) {
			printf ("   attr[%d]: '%s'\n", ii, xcattrs[ii]);
		}
	}
	#endif

	if (data->composer_quirks.enabled && g_ascii_strcasecmp (name, "span") == 0) {
		const gchar *value;

		value = markdown_utils_get_attribute_value (xcattrs, "class");

		if (value && g_ascii_strcasecmp (value, "-x-evo-cite-body") == 0) {
			data->composer_quirks.cite_body = TRUE;
			return;
		} else if (value && g_ascii_strcasecmp (value, "-x-evo-to-body") == 0) {
			value = markdown_utils_get_attribute_value (xcattrs, "data-credits");

			if (value && *value) {
				g_free (data->composer_quirks.to_body_credits);
				data->composer_quirks.to_body_credits = g_strdup (value);
				return;
			}
		}
	}

	if (data->composer_quirks.reading_html_end)
		return;

	if (g_ascii_strcasecmp (name, "body") == 0) {
		data->in_body = TRUE;
		return;
	}

	if (!data->in_body)
		return;

	if (g_ascii_strcasecmp (name, "a") == 0) {
		if (!data->plain_text && !data->href) {
			const gchar *href;

			href = markdown_utils_get_attribute_value (xcattrs, "href");

			if (href && *href) {
				data->href = g_strdup (href);
				data->link_text = g_string_new (NULL);
			}
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "blockquote") == 0) {
		if (data->in_paragraph_end) {
			if (data->quote_prefix->len)
				g_string_append (data->buffer, data->quote_prefix->str);

			g_string_append_c (data->buffer, '\n');

			data->in_paragraph_end = FALSE;
		}

		g_string_append (data->quote_prefix, "> ");
		return;
	}

	if (g_ascii_strcasecmp (name, "br") == 0) {
		if (data->plain_text) {
			g_string_append (data->buffer, "\n");

			if (data->quote_prefix->len)
				g_string_append (data->buffer, data->quote_prefix->str);
		} else if (!data->composer_quirks.enabled) {
			g_string_append (data->buffer, "<br>");
		}

		return;
	}

	if (g_ascii_strcasecmp (name, "b") == 0 ||
	    g_ascii_strcasecmp (name, "strong") == 0) {
		if (!data->plain_text)
			g_string_append (data->buffer, "**");
		return;
	}

	if (g_ascii_strcasecmp (name, "i") == 0 ||
	    g_ascii_strcasecmp (name, "em") == 0) {
		if (!data->plain_text)
			g_string_append (data->buffer, "*");
		return;
	}

	if (g_ascii_strcasecmp (name, "pre") == 0) {
		data->in_paragraph++;
		data->in_pre++;
		if (data->in_pre == 1) {
			if (!data->plain_text)
				g_string_append (data->buffer, "```\n");
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "code") == 0) {
		data->in_code++;
		if (data->in_code == 1 && !data->in_pre && !data->plain_text)
			g_string_append (data->buffer, "`");
		return;
	}

	if (g_ascii_strcasecmp (name, "h1") == 0 ||
	    g_ascii_strcasecmp (name, "h2") == 0 ||
	    g_ascii_strcasecmp (name, "h3") == 0 ||
	    g_ascii_strcasecmp (name, "h4") == 0 ||
	    g_ascii_strcasecmp (name, "h5") == 0 ||
	    g_ascii_strcasecmp (name, "h6") == 0) {
		if (data->in_paragraph_end) {
			g_string_append_c (data->buffer, '\n');
			data->in_paragraph_end = FALSE;
		}

		data->in_paragraph++;
		if (data->quote_prefix->len)
			g_string_append (data->buffer, data->quote_prefix->str);

		if (!data->plain_text) {
			switch (name[1]) {
			case '1':
				g_string_append (data->buffer, "# ");
				break;
			case '2':
				g_string_append (data->buffer, "## ");
				break;
			case '3':
				g_string_append (data->buffer, "### ");
				break;
			case '4':
				g_string_append (data->buffer, "#### ");
				break;
			case '5':
				g_string_append (data->buffer, "##### ");
				break;
			case '6':
				g_string_append (data->buffer, "###### ");
				break;
			}
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "p") == 0 ||
	    g_ascii_strcasecmp (name, "div") == 0) {
		if (data->in_paragraph_end) {
			data->in_paragraph_end = FALSE;

			if (data->quote_prefix->len)
				g_string_append (data->buffer, data->quote_prefix->str);

			g_string_append_c (data->buffer, '\n');
		}

		data->in_paragraph++;
		if (data->quote_prefix->len)
			g_string_append (data->buffer, data->quote_prefix->str);
		return;
	}

	if (g_ascii_strcasecmp (name, "ul") == 0) {
		if (data->in_paragraph_end) {
			g_string_append_c (data->buffer, '\n');
			data->in_paragraph_end = FALSE;
		}
		data->list_index = g_slist_prepend (data->list_index, GINT_TO_POINTER (-1));
		data->in_li = FALSE;
		return;
	}

	if (g_ascii_strcasecmp (name, "ol") == 0) {
		if (data->in_paragraph_end) {
			g_string_append_c (data->buffer, '\n');
			data->in_paragraph_end = FALSE;
		}
		data->list_index = g_slist_prepend (data->list_index, GINT_TO_POINTER (1));
		data->in_li = FALSE;
		return;
	}

	if (g_ascii_strcasecmp (name, "li") == 0) {
		data->in_paragraph_end = FALSE;
		data->in_li = TRUE;

		if (data->list_index) {
			gint index = GPOINTER_TO_INT (data->list_index->data);
			gint level = g_slist_length (data->list_index) - 1;

			if (data->quote_prefix->len)
				g_string_append (data->buffer, data->quote_prefix->str);

			if (level > 0)
				g_string_append_printf (data->buffer, "%*s", level * 3, "");

			if (index == -1) {
				g_string_append (data->buffer, "- ");
			} else {
				g_string_append_printf (data->buffer, "%d. ", index);
				data->list_index->data = GINT_TO_POINTER (index + 1);
			}
		}
		return;
	}
}

static void
markdown_utils_sax_end_element_cb (gpointer ctx,
				   const xmlChar *xcname)
{
	HTMLToTextData *data = ctx;
	const gchar *name = (const gchar *) xcname;

	dd (printf ("%s: '%s'\n", G_STRFUNC, name);)

	if (g_ascii_strcasecmp (name, "body") == 0) {
		data->in_body = FALSE;
		return;
	}

	if (!data->in_body)
		return;

	if (g_ascii_strcasecmp (name, "a") == 0) {
		if (!data->plain_text && data->href && data->link_text) {
			g_string_append_printf (data->buffer, "[%s](%s)", data->link_text->str, data->href);

			g_free (data->href);
			data->href = NULL;

			g_string_free (data->link_text, TRUE);
			data->link_text = NULL;
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "blockquote") == 0) {
		if (data->quote_prefix->len > 1)
			g_string_truncate (data->quote_prefix, data->quote_prefix->len - 2);

		data->in_paragraph_end = data->quote_prefix->len > 1;

		if (!data->in_paragraph_end)
			g_string_append_c (data->buffer, '\n');

		return;
	}

	if (g_ascii_strcasecmp (name, "b") == 0 ||
	    g_ascii_strcasecmp (name, "strong") == 0) {
		if (!data->plain_text)
			g_string_append (data->buffer, "**");
		return;
	}

	if (g_ascii_strcasecmp (name, "i") == 0 ||
	    g_ascii_strcasecmp (name, "em") == 0) {
		if (!data->plain_text)
			g_string_append (data->buffer, "*");
		return;
	}

	if (g_ascii_strcasecmp (name, "pre") == 0) {
		if (data->in_paragraph > 0)
			data->in_paragraph--;

		if (data->in_pre > 0) {
			data->in_pre--;
			if (data->in_pre == 0 && !data->plain_text)
				g_string_append (data->buffer, "```");
			g_string_append_c (data->buffer, '\n');
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "code") == 0) {
		if (data->in_code > 0) {
			data->in_code--;
			if (data->in_code == 0 && !data->in_pre && !data->plain_text)
				g_string_append (data->buffer, "`");
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "p") == 0 ||
	    g_ascii_strcasecmp (name, "div") == 0 ||
	    g_ascii_strcasecmp (name, "h1") == 0 ||
	    g_ascii_strcasecmp (name, "h2") == 0 ||
	    g_ascii_strcasecmp (name, "h3") == 0 ||
	    g_ascii_strcasecmp (name, "h4") == 0 ||
	    g_ascii_strcasecmp (name, "h5") == 0 ||
	    g_ascii_strcasecmp (name, "h6") == 0) {
		/* To avoid double-line ends when parsing composer HTML */
		if (data->composer_quirks.enabled && !(
		    g_ascii_strcasecmp (name, "p") == 0 ||
		    g_ascii_strcasecmp (name, "div") == 0))
			g_string_append_c (data->buffer, '\n');

		data->in_paragraph_end = TRUE;

		if (data->in_paragraph > 0)
			data->in_paragraph--;
		return;
	}

	if (g_ascii_strcasecmp (name, "ul") == 0 ||
	    g_ascii_strcasecmp (name, "ol") == 0) {
		if (data->list_index)
			data->list_index = g_slist_remove (data->list_index, data->list_index->data);
		data->in_paragraph_end = data->list_index == NULL;

		if (!data->in_paragraph_end && data->buffer->len && data->buffer->str[data->buffer->len - 1] == '\n')
			g_string_truncate (data->buffer, data->buffer->len - 1);

		return;
	}

	if (g_ascii_strcasecmp (name, "li") == 0) {
		g_string_append_c (data->buffer, '\n');

		data->in_paragraph_end = FALSE;
		data->in_li = FALSE;

		return;
	}
}

static gboolean
markdown_utils_only_whitespace (const gchar *text,
				gint len)
{
	gint ii;

	for (ii = 0; ii < len && text[ii]; ii++) {
		if (!g_ascii_isspace (text[ii]))
			return FALSE;
	}

	return TRUE;
}

static void
markdown_utils_sax_characters_cb (gpointer ctx,
				  const xmlChar *xctext,
				  gint len)
{
	HTMLToTextData *data = ctx;
	const gchar *text = (const gchar *) xctext;

	dd (printf ("%s: text:'%.*s' in_body:%d in_paragraph:%d in_li:%d\n", G_STRFUNC, len, text, data->in_body, data->in_paragraph, data->in_li);)

	if (data->in_body && (data->in_paragraph || data->in_li || !markdown_utils_only_whitespace (text, len))) {
		if (data->link_text) {
			g_string_append_len (data->link_text, text, len);
		} else {
			gsize from_index = data->buffer->len;

			g_string_append_len (data->buffer, text, len);

			if (data->quote_prefix->len && !data->in_li && strchr (data->buffer->str + from_index, '\n')) {
				gint ii;

				for (ii = from_index; ii < data->buffer->len; ii++) {
					if (data->buffer->str[ii] == '\n') {
						g_string_insert (data->buffer, ii + 1, data->quote_prefix->str);
						ii += data->quote_prefix->len + 1;
					}
				}
			}
		}
	}
}

static void
markdown_utils_sax_warning_cb (gpointer ctx,
			       const gchar *msg,
			       ...)
{
	/* Ignore these */
}

static void
markdown_utils_sax_error_cb (gpointer ctx,
			     const gchar *msg,
			     ...)
{
	/* Ignore these */
}

/**
 * e_markdown_utils_html_to_text:
 * @html: a text in HTML
 * @length: length of the @html, or -1 when it's nul-terminated
 * @flags: a bit-or of %EMarkdownHTMLToTextFlags
 *
 * Convert @html into the markdown text. The @flags influence
 * what can be preserved from the @html.
 *
 * Returns: (transfer full) (nullable): HTML converted into markdown text.
 *    Free the string with g_free(), when no longer needed.
 *
 * Since: 3.44
 **/
gchar *
e_markdown_utils_html_to_text (const gchar *html,
			       gssize length,
			       EMarkdownHTMLToTextFlags flags)
{
	htmlParserCtxtPtr ctxt;
	htmlSAXHandler sax;
	HTMLToTextData data;

	if (length < 0)
		length = html ? strlen (html) : 0;

	memset (&data, 0, sizeof (HTMLToTextData));

	data.buffer = g_string_new (NULL);
	data.quote_prefix = g_string_new (NULL);
	data.plain_text = (flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT) != 0;
	data.composer_quirks.enabled = (flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS) != 0;

	memset (&sax, 0, sizeof (htmlSAXHandler));

	sax.startElement = markdown_utils_sax_start_element_cb;
	sax.endElement = markdown_utils_sax_end_element_cb;
	sax.characters = markdown_utils_sax_characters_cb;
	sax.warning = markdown_utils_sax_warning_cb;
	sax.error = markdown_utils_sax_error_cb;

	ctxt = htmlCreatePushParserCtxt (&sax, &data, html ? html : "", length, "", XML_CHAR_ENCODING_UTF8);

	htmlParseChunk (ctxt, "", 0, 1);

	/* The libxml doesn't read elements after </html>, but the quirks can be stored after them,
	   thus retry after that element end, if it exists. */
	if (data.composer_quirks.enabled && html && ctxt->input && ctxt->input->cur) {
		guint html_end_length = ctxt->input->end - ctxt->input->cur;

		if (html_end_length > 1) {
			htmlParserCtxtPtr ctxt2;

			data.composer_quirks.reading_html_end = TRUE;

			ctxt2 = htmlCreatePushParserCtxt (&sax, &data, (const gchar *) ctxt->input->cur, html_end_length, "", XML_CHAR_ENCODING_UTF8);
			htmlParseChunk (ctxt2, "", 0, 1);
			htmlFreeParserCtxt (ctxt2);
		}
	}

	htmlFreeParserCtxt (ctxt);

	markdown_utils_apply_composer_quirks (data.buffer, &data.composer_quirks);

	g_free (data.href);

	if (data.link_text)
		g_string_free (data.link_text, TRUE);

	g_string_free (data.quote_prefix, TRUE);
	g_slist_free (data.list_index);
	g_free (data.composer_quirks.to_body_credits);

	return g_string_free (data.buffer, FALSE);
}
