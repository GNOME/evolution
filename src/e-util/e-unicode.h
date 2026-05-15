/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Lauris Kaplinski <lauris@ximian.com>
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

