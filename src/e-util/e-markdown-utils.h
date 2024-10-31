/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MARKDOWN_UTILS_H
#define E_MARKDOWN_UTILS_H

#include <glib.h>

#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

gchar *		e_markdown_utils_text_to_html		(const gchar *plain_text,
							 gssize length);
gchar *		e_markdown_utils_text_to_html_full	(const gchar *plain_text,
							 gssize length,
							 EMarkdownTextToHTMLFlags flags);
gchar *		e_markdown_utils_html_to_text		(const gchar *html,
							 gssize length,
							 EMarkdownHTMLToTextFlags flags);
EMarkdownHTMLToTextFlags
		e_markdown_utils_link_to_text_to_flags	(EHTMLLinkToText link_to_text);

G_END_DECLS

#endif /* E_MARKDOWN_UTILS_H */
