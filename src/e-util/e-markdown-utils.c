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
#include <glib/gi18n-lib.h>

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
	return e_markdown_utils_text_to_html_full (plain_text, length, E_MARKDOWN_TEXT_TO_HTML_FLAG_NONE);
}

G_LOCK_DEFINE_STATIC (custom_command_lock);

static void
markdown_utils_update_value_cb (GSettings *settings,
				const gchar *key,
				gpointer user_data)
{
	gchar **pstr = user_data;
	gchar *value;

	value = g_settings_get_string (settings, key);

	G_LOCK (custom_command_lock);

	if (g_strcmp0 (*pstr, value) != 0) {
		g_free (*pstr);
		*pstr = g_steal_pointer (&value);
	}

	G_UNLOCK (custom_command_lock);

	g_free (value);
}

static void
markdown_utils_handler_destroyed_cb (gpointer data,
				     GClosure *closure)
{
	gchar **pstr = data;

	G_LOCK (custom_command_lock);

	g_free (*pstr);
	*pstr = NULL;

	G_UNLOCK (custom_command_lock);
}

/**
 * e_markdown_utils_text_to_html_full:
 * @plain_text: plain text with markdown to convert to HTML
 * @length: length of the @plain_text, or -1 when it's nul-terminated
 * @flags: a bit-or of %EMarkdownTextToHTMLFlags flags
 *
 * Convert @plain_text, possibly with markdown, into the HTML, influencing
 * the result HTML code with the @flags.
 *
 * Note: The function can return %NULL when was not built
 *    with the markdown support.
 *
 * Returns: (transfer full) (nullable): text converted into HTML,
 *    or %NULL, when was not built with the markdown support.
 *    Free the string with g_free(), when no longer needed.
 *
 * Since: 3.48
 **/
gchar *
e_markdown_utils_text_to_html_full (const gchar *plain_text,
				    gssize length,
				    EMarkdownTextToHTMLFlags flags)
{
	static gchar *command = NULL;
	static gchar *command_sourcepos_arg = NULL;
	GString *html = NULL;
	gchar *converted = NULL;
	gboolean has_sourcepos = FALSE;

	if (length == -1)
		length = plain_text ? strlen (plain_text) : 0;

	if (!length)
		return NULL;

	G_LOCK (custom_command_lock);

	if (!command) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");

		g_signal_connect_data (settings, "changed::markdown-to-html-command",
			G_CALLBACK (markdown_utils_update_value_cb), &command,
			markdown_utils_handler_destroyed_cb, 0);
		g_signal_connect_data (settings, "changed::markdown-to-html-command-sourcepos-arg",
			G_CALLBACK (markdown_utils_update_value_cb), &command_sourcepos_arg,
			markdown_utils_handler_destroyed_cb, 0);

		command = g_settings_get_string (settings, "markdown-to-html-command");
		command_sourcepos_arg = g_settings_get_string (settings, "markdown-to-html-command-sourcepos-arg");

		g_clear_object (&settings);
	}

	if (command && *command) {
		gchar **argv = NULL;
		gint argc = 0;
		GError *local_error = NULL;

		if (g_shell_parse_argv (command, &argc, &argv, &local_error)) {
			GSubprocess *subprocess;

			if ((flags & E_MARKDOWN_TEXT_TO_HTML_FLAG_INCLUDE_SOURCEPOS) != 0) {
				if (command_sourcepos_arg && *command_sourcepos_arg) {
					gchar **new_argv;
					guint ii;

					new_argv = g_new0 (gchar *, g_strv_length (argv) + 2);
					for (ii = 0; argv[ii]; ii++) {
						new_argv[ii] = argv[ii];
						argv[ii] = NULL;
					}
					new_argv[ii] = g_strdup (command_sourcepos_arg);

					g_free (argv);
					argv = new_argv;
					has_sourcepos = TRUE;
				}
			}

			subprocess = g_subprocess_newv ((const gchar * const *) argv,
				G_SUBPROCESS_FLAGS_STDIN_PIPE |
				G_SUBPROCESS_FLAGS_STDOUT_PIPE |
				G_SUBPROCESS_FLAGS_STDERR_PIPE,
				&local_error);
			if (subprocess) {
				gchar *str_stdout = NULL, *str_stderr = NULL;
				if (g_subprocess_communicate_utf8 (subprocess, plain_text, NULL, &str_stdout, &str_stderr, &local_error)) {
					if (str_stdout && *str_stdout) {
						converted = g_steal_pointer (&str_stdout);
					} else if (str_stderr && *str_stderr) {
						gchar *tmp;

						tmp = g_strdup_printf (_("Command “%s” returned error: %s"), command, str_stderr);
						html = g_string_sized_new (strlen (tmp) + 15);
						e_util_markup_append_escaped (html, "<div>%s</div>", tmp);
						g_free (tmp);
					}

					g_free (str_stdout);
					g_free (str_stderr);
				} else {
					gchar *tmp;

					tmp = g_strdup_printf (_("Failed to read data from command “%s”: %s"), command, local_error ? local_error->message : _("Unknown error"));
					html = g_string_sized_new (strlen (tmp) + 15);
					e_util_markup_append_escaped (html, "<div>%s</div>", tmp);
					g_free (tmp);
				}

				g_clear_object (&subprocess);
			} else {
				gchar *tmp;

				tmp = g_strdup_printf (_("Failed to run process from command line “%s”: %s"), command, local_error ? local_error->message : _("Unknown error"));
				html = g_string_sized_new (strlen (tmp) + 15);
				e_util_markup_append_escaped (html, "<div>%s</div>", tmp);
				g_free (tmp);
			}

			g_strfreev (argv);
		} else {
			gchar *tmp;

			tmp = g_strdup_printf (_("Failed to parse command line “%s”: %s"), command, local_error ? local_error->message : _("Unknown error"));
			html = g_string_sized_new (strlen (tmp) + 15);
			e_util_markup_append_escaped (html, "<div>%s</div>", tmp);
			g_free (tmp);
		}

		g_clear_error (&local_error);
	}

	G_UNLOCK (custom_command_lock);

	#ifdef HAVE_MARKDOWN
	if (!converted && !html) {
		has_sourcepos = TRUE;
		converted = cmark_markdown_to_html (plain_text ? plain_text : "", length,
			CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_UNSAFE |
			((flags & E_MARKDOWN_TEXT_TO_HTML_FLAG_INCLUDE_SOURCEPOS) != 0 ? CMARK_OPT_SOURCEPOS : 0));
	}
	#endif

	if (converted) {
		g_warn_if_fail (html == NULL);

		if (has_sourcepos && (flags & E_MARKDOWN_TEXT_TO_HTML_FLAG_INCLUDE_SOURCEPOS) != 0)
			html = e_str_replace_string (converted, "<blockquote data-sourcepos=", "<blockquote type=\"cite\" data-sourcepos=");
		else
			html = e_str_replace_string (converted, "<blockquote>", "<blockquote type=\"cite\">");

		g_free (converted);
	}

	return html ? g_string_free (html, FALSE) : NULL;
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
		/* For Inline/Outlook style replies */
		if (quirks->cite_body)
			g_string_insert (buffer, 0, "\n");
		else
			g_string_insert (buffer, 0, "  \n");

		g_string_insert (buffer, 0, quirks->to_body_credits);
	}
}

typedef struct _HTMLToTextData {
	GString *buffer;
	gboolean in_body;
	gint in_code;
	gint in_pre;
	gint in_paragraph;
	guint paragraph_index;
	guint pending_nl_paragraph_index;
	gboolean in_li;
	gboolean in_ulol_start;
	gboolean line_start;
	GString *quote_prefix;
	gchar *href;
	GString *link_text;
	GSList *list_index; /* gint; -1 for unordered list */
	GPtrArray *link_references; /* gchar * */
	gboolean plain_text;
	gboolean significant_nl;
	EHTMLLinkToText link_to_text;
	struct _ComposerQuirks composer_quirks;
} HTMLToTextData;

#define markdown_utils_append_tag(_dt, _txt) markdown_utils_append_text (_dt, _txt, -1, FALSE)

static void
markdown_utils_append_text (HTMLToTextData *data,
			    const gchar *text,
			    gssize text_len,
			    gboolean can_convert_nl)
{
	if (data->pending_nl_paragraph_index) {
		if (!data->in_pre && !data->in_li && !data->quote_prefix->len) {
			/* Trim trailing spaces before new line */
			while (data->buffer->len > 0 && data->buffer->str[data->buffer->len - 1] == ' ') {
				g_string_truncate (data->buffer, data->buffer->len - 1);
			}
		}

		if (data->plain_text)
			g_string_append_c (data->buffer, '\n');
		else if (!data->in_pre && !data->list_index && data->pending_nl_paragraph_index == data->paragraph_index)
			g_string_append (data->buffer, "  \n");
		else
			g_string_append_c (data->buffer, '\n');

		if (data->quote_prefix->len)
			g_string_append (data->buffer, data->quote_prefix->str);

		data->line_start = !data->quote_prefix->len;
		data->pending_nl_paragraph_index = 0;
	}

	if (text && (text_len == -1 || text_len > 0)) {
		if (data->line_start && !data->in_pre && !data->in_li && !data->quote_prefix->len) {
			if (*text == '\n' && !data->significant_nl) {
				text++;
				if (text_len > 0)
					text_len--;
			} else {
				while (*text == ' ' && (text_len == -1 || text_len > 0)) {
					text++;
					if (text_len > 0)
						text_len--;
				}
			}
		}

		if ((text_len == -1 || text_len > 0) && *text) {
			gint ii, from_index = data->buffer->len;

			if (data->line_start && !data->in_pre && !data->in_li && !data->plain_text && data->buffer->len > 1 &&
			    data->buffer->str[data->buffer->len - 1] == '\n' &&
			    data->buffer->str[data->buffer->len - 2] != '\n' && (data->buffer->len < 3 ||
			    (data->buffer->str[data->buffer->len - 2] != ' ' || data->buffer->str[data->buffer->len - 3] != ' '))) {
				g_string_insert (data->buffer, data->buffer->len - 1, "  ");
				from_index = data->buffer->len;
			}

			if (data->line_start && data->quote_prefix->len && !data->in_li)
				g_string_append (data->buffer, data->quote_prefix->str);

			data->line_start = FALSE;

			g_string_append_len (data->buffer, text, text_len);

			if (can_convert_nl && !data->in_pre && !data->in_li && !data->plain_text) {
				for (ii = from_index; ii < data->buffer->len; ii++) {
					if (data->buffer->str[ii] == '\n') {
						if (data->significant_nl) {
							gint jj;

							for (jj = ii; jj > 0 && data->buffer->str[jj - 1] == ' '; jj--) {
								/* Count trailing spaces before the end of line */
							}

							/* Trim the spaces */
							if (jj != ii) {
								g_string_erase (data->buffer, jj, ii - jj);
								ii = jj;
							}

							g_string_insert (data->buffer, ii, "  ");
							ii += 2;

							for (jj = ii + 1; jj < data->buffer->len && data->buffer->str[jj] == ' '; jj++) {
								/* Count trailing spaces after the end of line */
							}

							/* Trim the spaces */
							if (jj != ii + 1)
								g_string_erase (data->buffer, ii + 1, jj - ii - 1);

							if (ii + 1 < data->buffer->len && data->quote_prefix->len && !data->in_li) {
								g_string_insert (data->buffer, ii + 1, data->quote_prefix->str);
								ii += data->quote_prefix->len;
							}
						} else {
							data->buffer->str[ii] = ' ';
						}
					}
				}
			} else if (data->quote_prefix->len && !data->in_li) {
				for (ii = from_index; ii < data->buffer->len - 1; ii++) {
					if (data->buffer->str[ii] == '\n') {
						g_string_insert (data->buffer, ii + 1, data->quote_prefix->str);
						ii += data->quote_prefix->len;
					}
				}
			}
		} else if (data->line_start) {
			if (data->quote_prefix->len && !data->in_li)
				g_string_append (data->buffer, data->quote_prefix->str);

			data->line_start = FALSE;
		}
	}
}

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
		if (!data->href) {
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
		markdown_utils_append_tag (data, NULL);
		data->pending_nl_paragraph_index = data->paragraph_index - 1;

		if (data->quote_prefix->len) {
			g_string_append (data->quote_prefix, "> ");
			markdown_utils_append_tag (data, NULL);
		} else {
			markdown_utils_append_tag (data, NULL);
			g_string_append (data->quote_prefix, "> ");
		}

		return;
	}

	if (g_ascii_strcasecmp (name, "br") == 0) {
		if (data->pending_nl_paragraph_index)
			markdown_utils_append_tag (data, NULL);

		if (data->link_text)
			g_string_append_c (data->link_text, '\n');
		else
			data->pending_nl_paragraph_index = data->paragraph_index;

		return;
	}

	if (g_ascii_strcasecmp (name, "b") == 0 ||
	    g_ascii_strcasecmp (name, "strong") == 0) {
		if (!data->plain_text)
			markdown_utils_append_tag (data, "**");
		return;
	}

	if (g_ascii_strcasecmp (name, "i") == 0 ||
	    g_ascii_strcasecmp (name, "em") == 0) {
		if (!data->plain_text)
			markdown_utils_append_tag (data, "*");
		return;
	}

	if (g_ascii_strcasecmp (name, "pre") == 0) {
		if (!data->in_paragraph)
			data->paragraph_index++;

		data->in_paragraph++;
		data->in_pre++;
		if (data->in_pre == 1) {
			if (data->plain_text) {
				markdown_utils_append_tag (data, NULL);
			} else if (g_str_has_suffix (data->buffer->str, "```\n")) {
				/* Merge consecutive <pre></pre> into one code block */
				g_string_truncate (data->buffer, data->buffer->len - 4);
				if (data->in_paragraph == 1)
					data->paragraph_index--;
			} else {
				markdown_utils_append_tag (data, "```\n");
			}
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "code") == 0) {
		data->in_code++;
		if (data->in_code == 1 && !data->in_pre && !data->plain_text)
			markdown_utils_append_tag (data, "`");
		else
			markdown_utils_append_tag (data, NULL);
		return;
	}

	if (g_ascii_strcasecmp (name, "h1") == 0 ||
	    g_ascii_strcasecmp (name, "h2") == 0 ||
	    g_ascii_strcasecmp (name, "h3") == 0 ||
	    g_ascii_strcasecmp (name, "h4") == 0 ||
	    g_ascii_strcasecmp (name, "h5") == 0 ||
	    g_ascii_strcasecmp (name, "h6") == 0) {
		markdown_utils_append_tag (data, NULL);

		if (!data->in_paragraph)
			data->paragraph_index++;
		data->in_paragraph++;

		if (!data->plain_text) {
			switch (name[1]) {
			case '1':
				markdown_utils_append_tag (data, "# ");
				break;
			case '2':
				markdown_utils_append_tag (data, "## ");
				break;
			case '3':
				markdown_utils_append_tag (data, "### ");
				break;
			case '4':
				markdown_utils_append_tag (data, "#### ");
				break;
			case '5':
				markdown_utils_append_tag (data, "##### ");
				break;
			case '6':
				markdown_utils_append_tag (data, "###### ");
				break;
			}
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "p") == 0 ||
	    g_ascii_strcasecmp (name, "div") == 0) {
		markdown_utils_append_tag (data, NULL);

		if (!data->in_paragraph)
			data->paragraph_index++;
		data->in_paragraph++;
		return;
	}

	if (g_ascii_strcasecmp (name, "ul") == 0 ||
	    g_ascii_strcasecmp (name, "ol") == 0) {
		if (!data->in_ulol_start && !data->pending_nl_paragraph_index)
			data->pending_nl_paragraph_index = data->paragraph_index - 1;

		markdown_utils_append_tag (data, NULL);

		if (g_ascii_strcasecmp (name, "ul") == 0)
			data->list_index = g_slist_prepend (data->list_index, GINT_TO_POINTER (-1));
		else
			data->list_index = g_slist_prepend (data->list_index, GINT_TO_POINTER (1));

		data->in_ulol_start = TRUE;
		data->in_li = FALSE;
		data->pending_nl_paragraph_index = data->paragraph_index;
		data->paragraph_index++;
		return;
	}

	if (g_ascii_strcasecmp (name, "li") == 0) {
		data->in_li = TRUE;
		data->in_ulol_start = FALSE;

		if (data->list_index) {
			gint index = GPOINTER_TO_INT (data->list_index->data);
			gint level = g_slist_length (data->list_index) - 1;

			markdown_utils_append_tag (data, NULL);

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
		if (data->href && data->link_text) {
			if (data->plain_text) {
				gboolean added = FALSE;

				if (data->link_to_text != E_HTML_LINK_TO_TEXT_NONE && data->link_text->len > 0 &&
				    e_util_link_requires_reference (data->href, data->link_text->str)) {
					if (data->link_to_text == E_HTML_LINK_TO_TEXT_INLINE) {
						gchar *tag;

						tag = g_strdup_printf ("%s <%s>", data->link_text->str, data->href);
						markdown_utils_append_tag (data, tag);
						g_free (tag);
						added = TRUE;
					} else if (data->link_to_text == E_HTML_LINK_TO_TEXT_REFERENCE ||
						   data->link_to_text == E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL) {
						gchar *tag;
						guint index;

						if (!data->link_references)
							data->link_references = g_ptr_array_new_with_free_func (g_free);

						/* group together the same href-s, even they can be with different text */
						for (index = 0; index < data->link_references->len; index++) {
							const gchar *ref = g_ptr_array_index (data->link_references, index);

							if (ref) {
								if ((data->link_to_text == E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL &&
								     g_ascii_strcasecmp (ref, data->href) == 0) ||
								    (data->link_to_text == E_HTML_LINK_TO_TEXT_REFERENCE &&
								     g_str_has_suffix (ref, data->href) && strlen (ref) > strlen (data->href) &&
								     ref[strlen (ref) - strlen (data->href) - 1] == ' ')) {
									break;
								}
							}
						}

						tag = g_strdup_printf ("%s [%u]", data->link_text->str, index + 1);
						markdown_utils_append_tag (data, tag);
						g_free (tag);
						added = TRUE;

						if (index == data->link_references->len) {
							if (data->link_to_text == E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL) {
								g_ptr_array_add (data->link_references, g_strdup (data->href));
							} else {
								guint ii;

								/* remove new lines from the text */
								for (ii = 0; ii < data->link_text->len; ii++) {
									if (data->link_text->str[ii] == '\r') {
										g_string_erase (data->link_text, ii, 1);
										ii--;
									} else if (data->link_text->str[ii] == '\n') {
										data->link_text->str[ii] = ' ';
									}
								}

								g_ptr_array_add (data->link_references, g_strconcat (data->link_text->str, " ", data->href, NULL));
							}
						}
					}
				}
				if (!added)
					markdown_utils_append_text (data, data->link_text->str, data->link_text->len, TRUE);
			} else {
				gchar *tag;

				tag = g_strdup_printf ("[%s](%s)", data->link_text->str, data->href);
				markdown_utils_append_tag (data, tag);
				g_free (tag);
			}

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

		data->paragraph_index++;

		if (data->pending_nl_paragraph_index != data->paragraph_index - 1) {
			markdown_utils_append_tag (data, NULL);

			if (!data->pending_nl_paragraph_index)
				data->pending_nl_paragraph_index = data->paragraph_index - 1;
		}

		return;
	}

	if (g_ascii_strcasecmp (name, "b") == 0 ||
	    g_ascii_strcasecmp (name, "strong") == 0) {
		if (!data->plain_text)
			markdown_utils_append_tag (data, "**");
		return;
	}

	if (g_ascii_strcasecmp (name, "i") == 0 ||
	    g_ascii_strcasecmp (name, "em") == 0) {
		if (!data->plain_text)
			markdown_utils_append_tag (data, "*");
		return;
	}

	if (g_ascii_strcasecmp (name, "pre") == 0) {
		data->paragraph_index++;

		if (data->in_paragraph > 0)
			data->in_paragraph--;

		if (data->in_pre > 0) {
			/* The composer creates the signature delimiter with a forced <BR>,
			   aka "<pre>-- <br></pre>", which causes unnecessary doubled "\n\n"
			   in the markdown, which this tries to avoid. */
			if (!data->composer_quirks.enabled || !g_str_has_suffix (data->buffer->str, "```\n-- "))
				markdown_utils_append_tag (data, "\n");
			else
				markdown_utils_append_tag (data, "");
			data->in_pre--;
			if (data->in_pre == 0 && !data->plain_text)
				markdown_utils_append_tag (data, "```\n");
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "code") == 0) {
		if (data->in_code > 0) {
			data->in_code--;
			if (data->in_code == 0 && !data->in_pre && !data->plain_text)
				markdown_utils_append_tag (data, "`");
		}
		return;
	}

	if (g_ascii_strcasecmp (name, "p") == 0 ||
	    g_ascii_strcasecmp (name, "div") == 0 ||
	    g_ascii_strcasecmp (name, "tr") == 0 ||
	    g_ascii_strcasecmp (name, "h1") == 0 ||
	    g_ascii_strcasecmp (name, "h2") == 0 ||
	    g_ascii_strcasecmp (name, "h3") == 0 ||
	    g_ascii_strcasecmp (name, "h4") == 0 ||
	    g_ascii_strcasecmp (name, "h5") == 0 ||
	    g_ascii_strcasecmp (name, "h6") == 0) {
		if (g_ascii_strcasecmp (name, "tr") != 0) {
			data->pending_nl_paragraph_index = data->paragraph_index;
			data->paragraph_index++;

			if (data->in_paragraph > 0)
				data->in_paragraph--;
		}

		return;
	}

	if (g_ascii_strcasecmp (name, "ul") == 0 ||
	    g_ascii_strcasecmp (name, "ol") == 0) {
		/* end the <li> */
		markdown_utils_append_tag (data, NULL);

		data->paragraph_index++;

		if (data->pending_nl_paragraph_index != data->paragraph_index - 1) {
			/* add extra line to split the list from the next paragraph */
			if (!data->pending_nl_paragraph_index)
				data->pending_nl_paragraph_index = data->paragraph_index - 1;

			markdown_utils_append_tag (data, NULL);
		}

		if (data->list_index)
			data->list_index = g_slist_remove (data->list_index, data->list_index->data);
		data->in_ulol_start = FALSE;

		return;
	}

	if (g_ascii_strcasecmp (name, "li") == 0) {
		markdown_utils_append_tag (data, NULL);

		data->pending_nl_paragraph_index = data->paragraph_index;
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
		if (data->link_text)
			g_string_append_len (data->link_text, text, len);
		else
			markdown_utils_append_text (data, text, len, TRUE);
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

	dd (printf ("%s: flags:%s%s%s%s%s%s%shtml:'%.*s'\n", G_STRFUNC,
		flags == E_MARKDOWN_HTML_TO_TEXT_FLAG_NONE ? "none " : "",
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT) != 0 ? "plain-text " : "",
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS) != 0 ? "composer-quirks " : "",
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL) != 0 ? "significant-nl " : "",
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE) != 0 ? "link-inline " : "",
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE) != 0 ? "link-reference " : "",
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL) != 0 ? "link-reference-without-label " : "",
		(int) length, html);)

	memset (&data, 0, sizeof (HTMLToTextData));

	data.buffer = g_string_new (NULL);
	data.quote_prefix = g_string_new (NULL);
	data.plain_text = (flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT) != 0;
	data.significant_nl = (flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL) != 0;
	data.link_to_text = (flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE) != 0 ? E_HTML_LINK_TO_TEXT_INLINE :
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE) != 0 ? E_HTML_LINK_TO_TEXT_REFERENCE :
		(flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL)  != 0 ? E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL :
		E_HTML_LINK_TO_TEXT_NONE;
	data.composer_quirks.enabled = (flags & E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS) != 0;

	memset (&sax, 0, sizeof (htmlSAXHandler));

	sax.startElement = markdown_utils_sax_start_element_cb;
	sax.endElement = markdown_utils_sax_end_element_cb;
	sax.characters = markdown_utils_sax_characters_cb;
	sax.warning = markdown_utils_sax_warning_cb;
	sax.error = markdown_utils_sax_error_cb;

	ctxt = htmlCreatePushParserCtxt (&sax, &data, "", 0, "", XML_CHAR_ENCODING_UTF8);
	htmlCtxtUseOptions (ctxt, HTML_PARSE_RECOVER | HTML_PARSE_NONET | HTML_PARSE_IGNORE_ENC);
	htmlParseChunk (ctxt, html ? html : "", length, 1);

	/* The libxml doesn't read elements after </html>, but the quirks can be stored after them,
	   thus retry after that element end, if it exists. */
	if (data.composer_quirks.enabled && html && ctxt->input && ctxt->input->cur) {
		guint html_end_length = ctxt->input->end - ctxt->input->cur;

		if (html_end_length > 1) {
			htmlParserCtxtPtr ctxt2;

			data.composer_quirks.reading_html_end = TRUE;

			ctxt2 = htmlCreatePushParserCtxt (&sax, &data, "", 0, "", XML_CHAR_ENCODING_UTF8);
			htmlCtxtUseOptions (ctxt2, HTML_PARSE_RECOVER | HTML_PARSE_NONET | HTML_PARSE_IGNORE_ENC);
			htmlParseChunk (ctxt2, (const gchar *) ctxt->input->cur, html_end_length, 1);
			htmlFreeParserCtxt (ctxt2);
		}
	}

	htmlFreeParserCtxt (ctxt);

	/* To add ending new-lines, if any */
	markdown_utils_append_tag (&data, NULL);

	markdown_utils_apply_composer_quirks (data.buffer, &data.composer_quirks);

	if (data.link_references) {
		guint ii;

		g_string_append_c (data.buffer, '\n');

		for (ii = 0; ii < data.link_references->len; ii++) {
			const gchar *ref = g_ptr_array_index (data.link_references, ii);

			g_string_append_printf (data.buffer, "[%u] %s\n", ii + 1, ref);
		}
	}

	g_free (data.href);

	if (data.link_text)
		g_string_free (data.link_text, TRUE);

	g_string_free (data.quote_prefix, TRUE);
	g_slist_free (data.list_index);
	g_clear_pointer (&data.link_references, g_ptr_array_unref);
	g_free (data.composer_quirks.to_body_credits);

	return g_string_free (data.buffer, FALSE);
}

/**
 * e_markdown_utils_link_to_text_to_flags:
 * @link_to_text: an #EHTMLLinkToText
 *
 * Converts @link_to_text enum into corresponding #EMarkdownHTMLToTextFlags flag.
 *
 * Returns: @link_to_text as ##EMarkdownHTMLToTextFlags
 *
 * Since: 3.52
 **/
EMarkdownHTMLToTextFlags
e_markdown_utils_link_to_text_to_flags (EHTMLLinkToText link_to_text)
{
	switch (link_to_text) {
	case E_HTML_LINK_TO_TEXT_NONE:
		break;
	case E_HTML_LINK_TO_TEXT_INLINE:
		return E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE;
	case E_HTML_LINK_TO_TEXT_REFERENCE:
		return E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE;
	case E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL:
		return E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL;
	}

	return E_MARKDOWN_HTML_TO_TEXT_FLAG_NONE;
}
