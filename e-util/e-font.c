#define _E_FONT_C_

/*
 * e-font
 *
 * Temporary wrappers around GdkFonts to get unicode displaying
 *
 * Author: Lauris Kaplinski <lauris@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 */

#include <string.h>
#include <gdk/gdkx.h>
#include <unicode.h>
#include "e-font.h"

#define FONT_TESTING

struct _EFont {
	gint refcount;
	GdkFont *font;
	gboolean bytes;
	unicode_iconv_t to;
	unicode_iconv_t from;
};

EFont *
e_font_from_gdk_name (const gchar *name)
{
	EFont * font;
	GdkFont *gdkfont;

	gdkfont = gdk_fontset_load (name);
	font = e_font_from_gdk_font (gdkfont);
	gdk_font_unref (gdkfont);

	return font;
}

EFont *
e_font_from_gdk_font (GdkFont *gdkfont)
{
	EFont *font;
	XFontStruct *xfs;
	Atom font_atom, atom;
	Bool status;

	gdk_font_ref (gdkfont);

	/* Try to find iso-10646-1 encoded font with same name */

	font_atom = gdk_atom_intern ("FONT", FALSE);
	if (gdkfont->type == GDK_FONT_FONTSET) {
		XFontStruct **font_structs;
		gint num_fonts;
		gchar **font_names;
		num_fonts = XFontsOfFontSet (GDK_FONT_XFONT (gdkfont),
					     &font_structs,
					     &font_names);
		status = XGetFontProperty (font_structs[0],
					   font_atom,
					   &atom);
	} else {
		status = XGetFontProperty (GDK_FONT_XFONT (gdkfont),
					   font_atom,
					   &atom);
	}
	if (status) {
		gchar *name, *p;
		gchar *newname;
		GdkFont *newfont;
		gint i;

		name = gdk_atom_name (atom);
		newname = alloca (strlen (name) + 12);
		strcpy (newname, name);
		p = newname;
		for (i = 0; i < 13; i++) {
			/* Skip hyphen */
			while (*p && (*p != '-')) p++;
			if (*p) p++;
		}
		g_snprintf (p, 12, "ISO10646-1");
		newfont = gdk_font_load (newname);
		if (newfont) {
			gdk_font_unref (gdkfont);
			gdkfont = newfont;
		}
		g_free (name);
	}

	font = g_new (EFont, 1);

	xfs = GDK_FONT_XFONT (gdkfont);

	font->refcount = 1;
	font->font = gdkfont;
	font->bytes = ((xfs->min_byte1 != 0) || (xfs->max_byte1 != 0)) ? 2 : 1;
	font->to = e_uiconv_to_gdk_font (font->font);
	font->from = e_uiconv_from_gdk_font (font->font);

	return font;

}

void
e_font_ref (EFont *font)
{
	font->refcount++;
}

void
e_font_unref (EFont *font)
{
	font->refcount--;

	if (font->refcount < 1) {
		gdk_font_unref (font->font);
		g_free (font);
	}
}

gint
e_font_ascent (EFont * font)
{
	return font->font->ascent;
}

gint
e_font_descent (EFont * font)
{
	return font->font->descent;
}

static gint
e_font_to_native (EFont *font, gchar *native, gchar *utf, gint bytes)
{
	char *ib, *ob;
	size_t ibl, obl;

	ib = utf;
	ibl = bytes;
	ob = native;
	obl = bytes * 4;

	while (ibl > 0) {
		unicode_iconv (font->to, (const char **) &ib, &ibl, &ob, &obl);
		if (ibl > 0) {
			gint len;
			if ((*ib & 0x80) == 0x00) len = 1;
			else if ((*ib &0xe0) == 0xc0) len = 2;
			else if ((*ib &0xf0) == 0xe0) len = 3;
			else if ((*ib &0xf80) == 0xf0) len = 4;
			else {
				g_warning ("Invalid UTF-8 sequence");
				return ob - native;
			}
			ib += len;
			ibl = bytes - (ib - utf);
			if (ibl > bytes) ibl = 0;
			if (font->bytes == 1) {
				*ob++ = '_';
				obl--;
			} else {
				*((guint16 *) ob) = '_';
				ob += 2;
				obl -= 2;
			}
		}
	}

	return ob - native;
}

void
e_font_draw_utf8_text (GdkDrawable *drawable, EFont *font, EFontStyle style, GdkGC *gc, gint x, gint y, gchar *text, gint numbytes)
{
	gchar *native;
	gint native_bytes;

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (font != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (text != NULL);

	if (numbytes < 1) return;

	native = alloca (numbytes * 4);

	native_bytes = e_font_to_native (font, native, text, numbytes);

	gdk_draw_text (drawable, font->font, gc, x, y, native, native_bytes);

	if (style & E_FONT_BOLD)
		gdk_draw_text (drawable, font->font, gc, x + 1, y, native, native_bytes);
}

gint
e_font_utf8_text_width (EFont *font, EFontStyle style, char *text, int numbytes)
{
	gchar *native;
	gint native_bytes;
	gint width;

	g_return_val_if_fail (font != NULL, 0);
	g_return_val_if_fail (text != NULL, 0);

	if (numbytes < 1) return 0;

	native = alloca (numbytes * 4);

	native_bytes = e_font_to_native (font, native, text, numbytes);

	width = gdk_text_width (font->font, native, native_bytes);

	return width;
}

gint
e_font_utf8_char_width (EFont *font, EFontStyle style, char *text)
{
	gint len;

	g_return_val_if_fail (font != NULL, 0);
	g_return_val_if_fail (text != NULL, 0);

	if ((*text & 0x80) == 0x00) len = 1;
	else if ((*text &0xe0) == 0xc0) len = 2;
	else if ((*text &0xf0) == 0xe0) len = 3;
	else if ((*text &0xf80) == 0xf0) len = 4;
	else {
		g_warning ("Invalid UTF-8 sequence");
		return 0;
	}

	return e_font_utf8_text_width (font, style, text, len);
}

static const gchar *
translate_encoding (const gchar *encoding)
{
	static GHashTable *eh = NULL;
	gchar e[64];

	if (!eh) {
		eh = g_hash_table_new (g_str_hash, g_str_equal);

		g_hash_table_insert (eh, "iso8859-1", "iso-8859-1");
		g_hash_table_insert (eh, "iso8859-2", "iso-8859-2");
		g_hash_table_insert (eh, "iso8859-3", "iso-8859-3");
		g_hash_table_insert (eh, "iso8859-4", "iso-8859-4");
		g_hash_table_insert (eh, "iso8859-5", "iso-8859-5");
		g_hash_table_insert (eh, "iso8859-6", "iso-8859-6");
		g_hash_table_insert (eh, "iso8859-7", "iso-8859-7");
		g_hash_table_insert (eh, "iso8859-8", "iso-8859-8");
		g_hash_table_insert (eh, "iso8859-9", "iso-8859-9");
		g_hash_table_insert (eh, "iso8859-10", "iso-8859-10");
		g_hash_table_insert (eh, "iso8859-13", "iso-8859-13");
		g_hash_table_insert (eh, "iso8859-14", "iso-8859-14");
		g_hash_table_insert (eh, "iso8859-15", "iso-8859-15");
		g_hash_table_insert (eh, "iso10646-1", "UTF-16");
		g_hash_table_insert (eh, "koi8-r", "koi8-r");
	}

	strncpy (e, encoding, 64);
	g_strdown (e);

	return g_hash_table_lookup (eh, e);
}

const gchar *
e_gdk_font_encoding (GdkFont *font)
{
	Atom font_atom, atom;
	Bool status;
	char *name, *p;
	const gchar *encoding;
	gint i;

	if (!font) return NULL;

	font_atom = gdk_atom_intern ("FONT", FALSE);

	if (font->type == GDK_FONT_FONTSET) {
		XFontStruct **font_structs;
		gint num_fonts;
		gchar **font_names;

		num_fonts = XFontsOfFontSet (GDK_FONT_XFONT (font),
					     &font_structs,
					     &font_names);
		status = XGetFontProperty (font_structs[0],
					   font_atom,
					   &atom);
	} else {
		status = XGetFontProperty (GDK_FONT_XFONT (font),
					   font_atom,
					   &atom);
	}

	if (!status) return NULL;

	name = p = gdk_atom_name (atom);

	for (i = 0; i < 13; i++) {
		/* Skip hyphen */
		while (*p && (*p != '-')) p++;
		if (*p) p++;
	}

	if (!*p) return NULL;

	encoding = translate_encoding (p);

	g_free (name);

	return encoding;
}

unicode_iconv_t
e_uiconv_from_gdk_font (GdkFont *font)
{
	static GHashTable *uh = NULL;
	const gchar *enc;
	unicode_iconv_t uiconv;

	if (!font) return (unicode_iconv_t) -1;

	enc = e_gdk_font_encoding (font);

	if (!enc) return (unicode_iconv_t) -1;

	if (!uh) uh = g_hash_table_new (g_str_hash, g_str_equal);

	uiconv = g_hash_table_lookup (uh, enc);

	if (!uiconv) {
		uiconv = unicode_iconv_open ("UTF-8", enc);
		if (uiconv == (unicode_iconv_t) -1) return uiconv;
		g_hash_table_insert (uh, (gpointer) enc, uiconv);
	}

	return uiconv;
}

unicode_iconv_t
e_uiconv_to_gdk_font (GdkFont *font)
{
	static GHashTable *uh = NULL;
	const gchar *enc;
	unicode_iconv_t uiconv;

	if (!font) return (unicode_iconv_t) -1;

	enc = e_gdk_font_encoding (font);

	if (!enc) return (unicode_iconv_t) -1;

	if (!uh) uh = g_hash_table_new (g_str_hash, g_str_equal);

	uiconv = g_hash_table_lookup (uh, enc);

	if (!uiconv) {
		uiconv = unicode_iconv_open (enc, "UTF-8");
		if (uiconv == (unicode_iconv_t) -1) return uiconv;
		g_hash_table_insert (uh, (gpointer) enc, uiconv);
	}

	return uiconv;
}








