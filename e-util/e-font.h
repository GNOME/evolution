#ifndef _E_FONT_H_
#define _E_FONT_H_

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

#include <glib.h>
#include <gdk/gdk.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

typedef struct _EFont EFont;

/*
 * We use very primitive styling here, enough for marking read/unread lines
 */

typedef enum {
	E_FONT_PLAIN = 0,
	E_FONT_BOLD = (1 << 0),
	E_FONT_ITALIC = (1 << 4)
} EFontStyle;

EFont * e_font_from_gdk_name (const gchar *name);
EFont * e_font_from_gdk_font (GdkFont *font);

void e_font_ref (EFont *font);
void e_font_unref (EFont *font);

gint e_font_ascent (EFont * font);
gint e_font_descent (EFont * font);

#define e_font_height(f) (e_font_ascent (f) + e_font_descent (f))

/*
 * NB! UTF-8 text widths are given in chars, not bytes
 */

void e_font_draw_utf8_text (GdkDrawable *drawable,
			    EFont *font, EFontStyle style,
			    GdkGC *gc,
			    gint x, gint y,
			    gchar *text,
			    gint numbytes);

int e_font_utf8_text_width (EFont *font, EFontStyle style,
			    char *text,
			    int numbytes);

int e_font_utf8_char_width (EFont *font, EFontStyle style,
			    char *text);

END_GNOME_DECLS

#endif
