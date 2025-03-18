/*
 * e-unicode.h - utf-8 support functions for gal
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
 *
 * Authors:
 *		Lauris Kaplinski <lauris@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UNICODE_H
#define E_UNICODE_H

#include <sys/types.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <iconv.h>

G_BEGIN_DECLS

gchar *		e_utf8_from_gtk_event_key	(GtkWidget *widget,
						 guint keyval,
						 const gchar *string);
gchar *		e_utf8_from_iconv_string_sized	(iconv_t ic,
						 const gchar *string,
						 gint bytes);
gchar *		e_utf8_to_iconv_string_sized	(iconv_t ic,
						 const gchar *string,
						 gint bytes);
gchar *		e_utf8_from_locale_string_sized	(const gchar *string,
						 gint bytes);
gchar *		e_utf8_ensure_valid		 (const gchar *string);
/*
 * These are simple wrappers that save us some typing
 */

/* NB! This return newly allocated string, not const as gtk+ one */
gint		e_unichar_to_utf8		(gint c,
						 gchar *outbuf);
gchar *		e_xml_get_translated_utf8_string_prop_by_name
						(const xmlNode *parent,
						 const xmlChar *prop_name);

G_END_DECLS

#endif /* E_UNICODE_H */

