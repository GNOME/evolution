/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-unicode.h - utf-8 support functions for gal
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Lauris Kaplinski <lauris@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_UNICODE_H_
#define _E_UNICODE_H_

#include <sys/types.h>
#include <glib.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkwidget.h>
#include <libxml/tree.h>
#include <iconv.h>

G_BEGIN_DECLS

#define G_UTF8_IN_GAL

/*
 * UTF-8 searching implementations
 *
 * e_utf8_strstrcase - case insensitive search
 * e_utf8_strstrcasedecomp - case insensitive and decompositing search (i.e. accented
 *   letters are treated equal to their base letters, explicit accent marks (unicode
 *   not ascii/iso ones) are ignored).
 */

const gchar *e_utf8_strstrcase                              (const gchar   *haystack,
							     const gchar   *needle);
const gchar *e_utf8_strstrcasedecomp                        (const gchar   *haystack,
							     const gchar   *needle);
gchar       *e_utf8_from_gtk_event_key                      (GtkWidget     *widget,
							     guint          keyval,
							     const gchar   *string);
gchar       *e_utf8_from_iconv_string                       (iconv_t        ic,
							     const gchar   *string);
gchar       *e_utf8_from_iconv_string_sized                 (iconv_t        ic,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_to_iconv_string                         (iconv_t        ic,
							     const gchar   *string);
gchar       *e_utf8_to_iconv_string_sized                   (iconv_t        ic,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_from_charset_string                     (const gchar   *charset,
							     const gchar   *string);
gchar       *e_utf8_from_charset_string_sized               (const gchar   *charset,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_to_charset_string                       (const gchar   *charset,
							     const gchar   *string);
gchar       *e_utf8_to_charset_string_sized                 (const gchar   *charset,
							     const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_from_locale_string                      (const gchar   *string);
gchar       *e_utf8_from_locale_string_sized                (const gchar   *string,
							     gint           bytes);
gchar       *e_utf8_to_locale_string                        (const gchar   *string);
gchar       *e_utf8_to_locale_string_sized                  (const gchar   *string,
							     gint           bytes);
gboolean     e_utf8_is_ascii                                (const gchar   *string);
/*
 * These are simple wrappers that save us some typing
 */

/* NB! This return newly allocated string, not const as gtk+ one */
gchar       *e_utf8_gtk_entry_get_text                      (GtkEntry      *entry);
void         e_utf8_gtk_entry_set_text                      (GtkEntry      *entry,
							     const gchar   *text);
gchar       *e_utf8_gtk_editable_get_text                   (GtkEditable   *editable);
void         e_utf8_gtk_editable_set_text                   (GtkEditable   *editable,
							     const gchar   *text);
gchar       *e_utf8_gtk_editable_get_chars                  (GtkEditable   *editable,
							     gint           start,
							     gint           end);
void         e_utf8_gtk_editable_insert_text                (GtkEditable   *editable,
							     const gchar   *text,
							     gint           length,
							     gint          *position);
gchar       *e_utf8_xml1_decode                             (const gchar   *text);
gchar       *e_utf8_xml1_encode                             (const gchar   *text);
gint         e_unichar_to_utf8                              (gint           c,
							     gchar         *outbuf);
gchar       *e_unicode_get_utf8                             (const gchar   *text,
							     gunichar      *out);
guint32      gdk_keyval_to_unicode                          (guint          keysym);
gchar       *e_xml_get_translated_utf8_string_prop_by_name  (const xmlNode *parent,
							     const xmlChar *prop_name);

G_END_DECLS

#endif


