/*
 * e-unicode.h - utf-8 support functions for gal
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
 *
 * Authors:
 *		Lauris Kaplinski <lauris@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_UNICODE_H_
#define _E_UNICODE_H_

#include <sys/types.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <iconv.h>

G_BEGIN_DECLS

gchar       *e_utf8_from_gtk_event_key                      (GtkWidget     *widget,
							     guint          keyval,
							     const gchar   *string);
gchar       *e_utf8_from_iconv_string_sized                 (iconv_t        ic,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_to_iconv_string_sized                   (iconv_t        ic,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_to_charset_string_sized                 (const gchar   *charset,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_from_locale_string_sized                (const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_ensure_valid			    (const gchar   *string);
/*
 * These are simple wrappers that save us some typing
 */

/* NB! This return newly allocated string, not const as gtk+ one */
gint         e_unichar_to_utf8                              (gint           c,
							     gchar         *outbuf);
gchar       *e_unicode_get_utf8                             (const gchar   *text,
							     gunichar      *out);
gchar       *e_xml_get_translated_utf8_string_prop_by_name  (const xmlNode *parent,
							     const xmlChar *prop_name);

G_END_DECLS

#endif

