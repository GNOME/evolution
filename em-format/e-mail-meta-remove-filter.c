/*
 * e-mail-meta-remove-filter.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "e-mail-meta-remove-filter.h"

G_DEFINE_TYPE (EMailMetaRemoveFilter, e_mail_meta_remove_filter, CAMEL_TYPE_MIME_FILTER)

static void
remove_meta_tag (CamelMimeFilter *filter,
                 const gchar *in,
                 gsize len,
                 gsize prespace,
                 gchar **out,
                 gsize *outlen)
{
	EMailMetaRemoveFilter *meta_remove = (EMailMetaRemoveFilter *) filter;
	register const gchar *inptr = in;
	const gchar *inend = in + len;
	const gchar *start = NULL;
	const gchar *end_of_prev_meta = NULL;
	gboolean in_meta = meta_remove->in_meta;
	gboolean previously_in_meta = meta_remove->in_meta;
	gboolean charset_meta = FALSE;
	gboolean backup = FALSE;
	GString *new_out = NULL;
	gsize offset = 0;

	new_out = g_string_new ("");

	if (meta_remove->after_head)
		goto copy_input;

	while (inptr < inend) {
		/* Start of meta */
		if (g_ascii_strncasecmp (inptr, "<meta ", 6) == 0) {
			/* If there was previous meta tag */
			if (end_of_prev_meta) {
				/* And there were some tags between these two meta tags */
				if (inptr - 1 != end_of_prev_meta) {
					/* Save them */
					gchar *tags;

					tags = g_strndup (
						end_of_prev_meta + 1,
						inptr - end_of_prev_meta - 2);

					g_string_append (new_out, tags);
					g_free (tags);
				}
			}

			in_meta = TRUE;
			start = inptr;
			inptr += 6;
		}

		/* Meta tags are valid just in head element */
		if (!in_meta && g_ascii_strncasecmp (inptr, "</head>", 7) == 0) {
			meta_remove->after_head = TRUE;
			if (end_of_prev_meta)
				break;
			else
				goto copy_input;
		}

		if (!in_meta && g_ascii_strncasecmp (inptr, "<body", 5) == 0) {
			meta_remove->after_head = TRUE;
			if (end_of_prev_meta)
				break;
			else
				goto copy_input;
		}

		/* Charset meta */
		if (in_meta && !meta_remove->remove_all_meta) {
			if (g_ascii_strncasecmp (inptr, "charset", 7) == 0)
				charset_meta = TRUE;
		}

		/* End of meta tag */
		if (in_meta && g_ascii_strncasecmp (inptr, ">", 1) == 0) {
			end_of_prev_meta = inptr;
			in_meta = FALSE;
			/* Strip meta tag from input */
			if (meta_remove->remove_all_meta || charset_meta) {
				if (new_out->len == 0 && !previously_in_meta) {
					if (start) {
						/* Copy tags before meta tag */
						if (start - in > 0) {
							gchar *beginning;

							beginning = g_strndup (in, start - in);
							g_string_append (new_out, beginning);
							g_free (beginning);
						}
					} else {
						/* If meta tag continues from previous buffer
						 * just adjust the offset */
						offset = end_of_prev_meta + 1 - in;
					}
				}

				/* If we wanted to remove just charset meta and we
				 * removed it, quit */
				if (!meta_remove->remove_all_meta) {
					meta_remove->after_head = TRUE;
					break;
				}
			}
			start = NULL;
			charset_meta = FALSE;
		}

		inptr++;
	}

	if (in_meta) {
		/* Meta tag doesn't end in this buffer */
		gchar *tags = NULL;

		if (end_of_prev_meta && start) {
			/* No tags between two meta tags */
			if (end_of_prev_meta + 1 == start)
				goto save_output;
			tags = g_strndup (
				end_of_prev_meta + 1,
				start - end_of_prev_meta - 2);
		} else if (!end_of_prev_meta && start) {
			tags = g_strndup (in + offset , start - in - offset);
		}

		if (tags) {
			g_string_append (new_out, tags);
			g_free (tags);
		}
	} else if (end_of_prev_meta) {
		gchar *end;

		/* Copy tags after last meta to output */
		end = g_strndup (end_of_prev_meta + 1, inend - end_of_prev_meta - 1);
		g_string_append (new_out, end);
		g_free (end);
	} else if (!end_of_prev_meta) {
		/* Meta was not found in this buffer */
		camel_mime_filter_backup (filter, inend - 6, 6);
		backup = TRUE;
		goto copy_input;
	}

 save_output:
	*out = (gchar *) new_out->str;
	*outlen = new_out->len;
	g_string_free (new_out, FALSE);

	meta_remove->in_meta = in_meta;

	return;

 copy_input:
	if (backup) {
		gchar *out_backup = g_strndup (in, inend - in - 6);
		*out = out_backup;
		*outlen = inend - in - 6;
		g_free (out_backup);
	} else {
		*out = (gchar *) in;
		*outlen = inend - in;
	}

	meta_remove->in_meta = in_meta;

	g_string_free (new_out, TRUE);
}

static void
filter_filter (CamelMimeFilter *filter,
               const gchar *in,
               gsize len,
               gsize prespace,
               gchar **out,
               gsize *outlen,
               gsize *outprespace)
{
	remove_meta_tag (filter, in, len, prespace, out, outlen);

	*outprespace = prespace;
}

static void
filter_complete (CamelMimeFilter *filter,
                 const gchar *in,
                 gsize len,
                 gsize prespace,
                 gchar **out,
                 gsize *outlen,
                 gsize *outprespace)
{
	*out = (gchar *) in;
	*outlen = len;
	*outprespace = prespace;
}

static void
filter_reset (CamelMimeFilter *filter)
{
	EMailMetaRemoveFilter *meta_remove = (EMailMetaRemoveFilter *) filter;

	meta_remove->in_meta = FALSE;
	meta_remove->after_head = FALSE;
}

static void
e_mail_meta_remove_filter_class_init (EMailMetaRemoveFilterClass *class)
{
	CamelMimeFilterClass *mime_filter_class;

	mime_filter_class = CAMEL_MIME_FILTER_CLASS (class);
	mime_filter_class->filter = filter_filter;
	mime_filter_class->complete = filter_complete;
	mime_filter_class->reset = filter_reset;
}

static void
e_mail_meta_remove_filter_init (EMailMetaRemoveFilter *filter)
{
}

/**
 * e_mail_meta_remove_filter_new:
 * @remove_all_meta: Whether remove all meta tags from message or just meta
 * tag with charset attribute
 *
 * Creates a new meta_remove filter.
 *
 * Returns a new meta_remove filter.
 **/
CamelMimeFilter *
e_mail_meta_remove_filter_new (gboolean remove_all_meta)
{
	EMailMetaRemoveFilter *filter = g_object_new (E_TYPE_MAIL_META_REMOVE_FILTER, NULL);

	filter->remove_all_meta = remove_all_meta;
	filter->in_meta = FALSE;
	filter->after_head = FALSE;

	return CAMEL_MIME_FILTER (filter);
}
