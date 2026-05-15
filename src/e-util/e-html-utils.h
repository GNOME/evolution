/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Dan Winship <danw@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_HTML_UTILS__
#define __E_HTML_UTILS__

#include <glib.h>

#define E_TEXT_TO_HTML_PRE                (1 << 0)
#define E_TEXT_TO_HTML_CONVERT_NL         (1 << 1)
#define E_TEXT_TO_HTML_CONVERT_SPACES     (1 << 2)
#define E_TEXT_TO_HTML_CONVERT_URLS       (1 << 3)
#define E_TEXT_TO_HTML_MARK_CITATION      (1 << 4)
#define E_TEXT_TO_HTML_CONVERT_ADDRESSES  (1 << 5)
#define E_TEXT_TO_HTML_ESCAPE_8BIT        (1 << 6)
#define E_TEXT_TO_HTML_CITE               (1 << 7)
#define E_TEXT_TO_HTML_HIDE_URL_SCHEME    (1 << 8)
#define E_TEXT_TO_HTML_URL_IS_WHOLE_TEXT  (1 << 9)
#define E_TEXT_TO_HTML_CONVERT_ALL_SPACES (1 << 10)
#define E_TEXT_TO_HTML_LAST_FLAG          (1 << 11)

gchar *e_text_to_html_full (const gchar *input, guint flags, guint32 color);
gchar *e_text_to_html      (const gchar *input, guint flags);

#endif /* __E_HTML_UTILS__ */
