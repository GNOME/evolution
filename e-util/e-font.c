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
e_font_draw_utf8_text (GdkDrawable *drawable, EFont *font, EFontStyle style, GdkGC *gc, gint x, gint y, gchar *text, gint length)
{
	guchar *iso_text;
	gchar *p;
	gint uni, i;

	iso_text = alloca (length);

	for (p = text, i = 0; i < length; i++, p = unicode_next_utf8 (p)) {
		unicode_get_utf8 (p, &uni);
		if ((uni < ' ') || (uni > 255)) uni = ' ';
		iso_text[i] = uni;
	}

	gdk_draw_text (drawable, &font->font, gc, x, y, iso_text, length);

	if (style & E_FONT_BOLD)
		gdk_draw_text (drawable, &font->font, gc, x + 1, y, iso_text, length);
}

gint
e_font_utf8_text_width (EFont *font, EFontStyle style, char *text, int length)
{
	guchar *iso_text;
	gchar *p;
	gint uni, i;

	iso_text = alloca (length);

	for (p = text, i = 0; i < length; i++, p = unicode_next_utf8 (p)) {
		unicode_get_utf8 (p, &uni);
		if ((uni < ' ') || (uni > 255)) uni = ' ';
		iso_text[i] = uni;
	}

	return gdk_text_width (&font->font, iso_text, length);
}







