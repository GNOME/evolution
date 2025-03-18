/*
 * e-unicode.c - utf-8 support functions for gal
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

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <iconv.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libxml/xmlmemory.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n.h>
#include "e-unicode.h"

#define d(x)

#define FONT_TESTING
#define MAX_DECOMP 8

/* FIXME: this has not been ported fully yet - non ASCII people beware. */

gchar *
e_utf8_from_gtk_event_key (GtkWidget *widget,
                           guint keyval,
                           const gchar *string)
{
	gint unival;
	gchar *utf;
	gint unilen;

	if (keyval == GDK_KEY_VoidSymbol) {
		utf = e_utf8_from_locale_string_sized (string, strlen (string));
	} else {
		unival = gdk_keyval_to_unicode (keyval);

		if (unival < ' ') return NULL;

		utf = g_new (gchar, 7);

		unilen = e_unichar_to_utf8 (unival, utf);

		utf[unilen] = '\0';
	}

	return utf;
}

gchar *
e_utf8_from_iconv_string_sized (iconv_t ic,
                                const gchar *string,
                                gint bytes)
{
	gchar *new, *ob;
	const gchar *ib;
	gsize ibl, obl;

	if (!string) return NULL;

	if (ic == (iconv_t) -1) {
		gint i;
		/* iso-8859-1 */
		ib = (gchar *) string;
		new = ob = (gchar *) g_new (guchar, bytes * 2 + 1);
		for (i = 0; i < (bytes); i++) {
			ob += e_unichar_to_utf8 (ib[i], ob);
		}
		if (ob)
			*ob = '\0';
		return new;
	}

	ib = string;
	ibl = bytes;
	new = ob = g_new (gchar, ibl * 6 + 1);
	obl = ibl * 6;

	while (ibl > 0) {
		camel_iconv (ic, &ib, &ibl, &ob, &obl);
		if (ibl > 0) {
			gint len;
			if ((*ib & 0x80) == 0x00) len = 1;
			else if ((*ib &0xe0) == 0xc0) len = 2;
			else if ((*ib &0xf0) == 0xe0) len = 3;
			else if ((*ib &0xf8) == 0xf0) len = 4;
			else {
				g_warning ("Invalid UTF-8 sequence");
				break;
			}
			ib += len;
			ibl = bytes - (ib - string);
			if (ibl > bytes) ibl = 0;
			*ob++ = '_';
			obl--;
		}
	}

	*ob = '\0';

	return new;
}

gchar *
e_utf8_to_iconv_string_sized (iconv_t ic,
                              const gchar *string,
                              gint bytes)
{
	gchar *new, *ob;
	const gchar *ib;
	gsize ibl, obl;

	if (!string) return NULL;

	if (ic == (iconv_t) -1) {
		gint len;
		const gchar *u;
		gunichar uc;

		new = (gchar *) g_new (guchar, bytes * 4 + 1);
		u = string;
		len = 0;

		while ((u) && (u - string < bytes)) {
			u = e_util_unicode_get_utf8 (u, &uc);
			new[len++] = uc & 0xff;
		}
		new[len] = '\0';
		return new;
	}

	ib = string;
	ibl = bytes;
	new = ob = g_new (char, ibl * 4 + 4);
	obl = ibl * 4;

	while (ibl > 0) {
		camel_iconv (ic, &ib, &ibl, &ob, &obl);
		if (ibl > 0) {
			gint len;
			if ((*ib & 0x80) == 0x00) len = 1;
			else if ((*ib &0xe0) == 0xc0) len = 2;
			else if ((*ib &0xf0) == 0xe0) len = 3;
			else if ((*ib &0xf8) == 0xf0) len = 4;
			else {
				g_warning ("Invalid UTF-8 sequence");
				break;
			}
			ib += len;
			ibl = bytes - (ib - string);
			if (ibl > bytes) ibl = 0;

			/* FIXME This is wrong.  What if the destination
			 *       charset is 16 or 32 bit? */
			*ob++ = '_';
			obl--;
		}
	}

	/* Make sure to terminate with plenty of padding */
	memset (ob, 0, 4);

	return new;
}

gchar *
e_utf8_from_locale_string_sized (const gchar *string,
                                 gint bytes)
{
	iconv_t ic;
	gchar *ret;

	if (!string) return NULL;

	ic = camel_iconv_open ("utf-8", camel_iconv_locale_charset ());
	ret = e_utf8_from_iconv_string_sized (ic, string, bytes);
	camel_iconv_close (ic);

	return ret;
}

/**
 * e_utf8_ensure_valid:
 * @string: string to make valid UTF-8
 *
 * Ensures the returned string will be valid UTF-8 string, thus GTK+
 * functions expecting only valid UTF-8 text will not crash.
 *
 * Returned pointer should be freed with g_free().
 *
 * Returns: a newly-allocated UTF-8 string
 **/
gchar *
e_utf8_ensure_valid (const gchar *string)
{
	gchar *res = g_strdup (string), *p;

	if (!res)
		return res;

	p = res;
	while (!g_utf8_validate (p, -1, (const gchar **) &p)) {
		/* make all invalid characters appear as question marks */
		*p = '?';
	}

	return res;
}

/**
 * e_unichar_to_utf8:
 * @c: a ISO10646 character code
 * @outbuf: output buffer, must have at least 6 bytes of space.
 *          If %NULL, the length will be computed and returned
 *          and nothing will be written to @out.
 *
 * Convert a single character to utf8
 *
 * Return value: number of bytes written
 **/

gint
e_unichar_to_utf8 (gint c,
                   gchar *outbuf)
{
  gsize len = 0;
  gint first;
  gint i;

  if (c < 0x80)
    {
      first = 0;
      len = 1;
    }
  else if (c < 0x800)
    {
      first = 0xc0;
      len = 2;
    }
  else if (c < 0x10000)
    {
      first = 0xe0;
      len = 3;
    }
   else if (c < 0x200000)
    {
      first = 0xf0;
      len = 4;
    }
  else if (c < 0x4000000)
    {
      first = 0xf8;
      len = 5;
    }
  else
    {
      first = 0xfc;
      len = 6;
    }

  if (outbuf)
    {
      for (i = len - 1; i > 0; --i)
	{
	  outbuf[i] = (c & 0x3f) | 0x80;
	  c >>= 6;
	}
      outbuf[0] = c | first;
    }

  return len;
}

gchar *
e_xml_get_translated_utf8_string_prop_by_name (const xmlNode *parent,
                                               const xmlChar *prop_name)
{
	xmlChar *prop;
	gchar *ret_val = NULL;
	gchar *combined_name;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (prop_name != NULL, NULL);

	prop = xmlGetProp ((xmlNode *) parent, prop_name);
	if (prop != NULL) {
		ret_val = g_strdup ((gchar *) prop);
		xmlFree (prop);
		return ret_val;
	}

	combined_name = g_strdup_printf ("_%s", prop_name);
	prop = xmlGetProp ((xmlNode *) parent, (guchar *) combined_name);
	if (prop != NULL) {
		ret_val = g_strdup (gettext ((gchar *) prop));
		xmlFree (prop);
	}
	g_free (combined_name);

	return ret_val;
}
