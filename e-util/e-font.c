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

struct _EFont {
	GdkFont font;
};

EFont *
e_font_from_gdk_name (const gchar *name)
{
	GdkFont *font;

	font = gdk_fontset_load (name);

	return (EFont *) font;
}

EFont *
e_font_from_gdk_font (GdkFont *font)
{
	gdk_font_ref (font);

	return (EFont *) font;
}

void
e_font_ref (EFont *font)
{
	gdk_font_ref (&font->font);
}

void
e_font_unref (EFont *font)
{
	gdk_font_unref (&font->font);
}

gint
e_font_ascent (EFont * font)
{
	return font->font.ascent;
}

gint
e_font_descent (EFont * font)
{
	return font->font.descent;
}

void
e_font_draw_utf8_text (GdkDrawable *drawable, EFont *font, EFontStyle style, GdkGC *gc, gint x, gint y, gchar *text, gint numbytes)
{
	guchar *iso;
	gchar *p;
	gint uni, len;

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (font != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (text != NULL);

	if (numbytes < 1) return;

	iso = alloca (numbytes);

	for (len = 0, p = text; p != NULL && p < (text + numbytes); len++, p = unicode_next_utf8 (p)) {
		unicode_get_utf8 (p, &uni);
		if ((uni < ' ') || (uni > 255)) uni = ' ';
		iso[len] = uni;
	}

	gdk_draw_text (drawable, &font->font, gc, x, y, iso, len);

	if (style & E_FONT_BOLD)
		gdk_draw_text (drawable, &font->font, gc, x + 1, y, iso, len);
}

gint
e_font_utf8_text_width (EFont *font, EFontStyle style, char *text, int numbytes)
{
	guchar *iso;
	gchar *p;
	gint uni, len;

	g_return_val_if_fail (font != NULL, 0);
	g_return_val_if_fail (text != NULL, 0);

	iso = alloca (numbytes);

	for (len = 0, p = text; p != NULL && p < (text + numbytes); len++, p = unicode_next_utf8 (p)) {
		unicode_get_utf8 (p, &uni);
		if ((uni < ' ') || (uni > 255)) uni = ' ';
		iso[len] = uni;
	}

	return gdk_text_width (&font->font, iso, len);
}

gint
e_font_utf8_char_width (EFont *font, EFontStyle style, char *text)
{
	unicode_char_t uni;
	guchar iso;

	g_return_val_if_fail (font != NULL, 0);
	g_return_val_if_fail (text != NULL, 0);

	if (!unicode_get_utf8 (text, &uni)) return 0;

	if ((uni < ' ') || (uni > 255)) uni = ' ';

	iso = uni;

	return gdk_text_width (&font->font, &iso, 1);
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
		g_hash_table_insert (eh, "iso10646-1", "UCS2");
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

	name = gdk_atom_name (atom);
	p = strchr (name, '-'); /* Foundry */
	p = strchr (p + 1, '-'); /* Family */
	p = strchr (p + 1, '-'); /* Weight */
	p = strchr (p + 1, '-'); /* Slant */
	p = strchr (p + 1, '-'); /* Set Width */
	p = strchr (p + 1, '-'); /* Add Style */
	p = strchr (p + 1, '-'); /* Pixel Size */
	p = strchr (p + 1, '-'); /* Point Size */
	p = strchr (p + 1, '-'); /* Resolution X */
	p = strchr (p + 1, '-'); /* Resolution Y */
	p = strchr (p + 1, '-'); /* Spacing */
	p = strchr (p + 1, '-'); /* Average Width */
	p = strchr (p + 1, '-'); /* Charset */

	encoding = translate_encoding (p + 1);

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

	if (!uh) {
		uh = g_hash_table_new (g_str_hash, g_str_equal);
	}

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

	if (!uh) {
		uh = g_hash_table_new (g_str_hash, g_str_equal);
	}

	uiconv = g_hash_table_lookup (uh, enc);

	if (!uiconv) {
		uiconv = unicode_iconv_open (enc, "UTF-8");
		if (uiconv == (unicode_iconv_t) -1) return uiconv;
		g_hash_table_insert (uh, (gpointer) enc, uiconv);
	}

	return uiconv;
}







