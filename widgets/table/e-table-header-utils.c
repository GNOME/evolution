/* ETable widget - utilities for drawing table header buttons
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Chris Lahey <clahey@helixcode.com>
 *          Miguel de Icaza <miguel@helixcode.com>
 *          Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "e-table-defines.h"
#include "e-table-header-utils.h"



/**
 * e_table_header_compute_height:
 * @ecol: Table column description.
 * @style: Style for the button's bevel.
 * @font: Font for the button's text, or NULL if no font is available.
 *
 * Computes the minimum height required for a table header button.
 *
 * Return value: The height of the button, in pixels.
 **/
double
e_table_header_compute_height (ETableCol *ecol, GtkStyle *style, GdkFont *font)
{
	int ythick;
	int height;

	g_return_val_if_fail (ecol != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_COL (ecol), -1);
	g_return_val_if_fail (style != NULL, -1);

	ythick = style->klass->ythickness;

	if (font)
		height = font->ascent + font->descent;
	else
		height = 16; /* FIXME: default? */

	if (ecol->is_pixbuf) {
		g_assert (ecol->pixbuf != NULL);
		height = MAX (height, gdk_pixbuf_get_height (ecol->pixbuf));
	}

	height = MAX (height, MIN_ARROW_SIZE);

	height += 2 * (ythick + HEADER_PADDING);

	return height;
}

double
e_table_header_width_extras (GtkStyle *style)
{
	g_return_val_if_fail (style != NULL, -1);

	return 2 * (style->klass->xthickness + HEADER_PADDING);
}

/* Creates a pixmap that is a composite of a background color and the upper-left
 * corner rectangle of a pixbuf.
 */
static GdkPixmap *
make_composite_pixmap (GdkDrawable *drawable, GdkGC *gc,
		       GdkPixbuf *pixbuf, GdkColor *bg, int width, int height,
		       int dither_xofs, int dither_yofs)
{
	int pwidth, pheight;
	GdkPixmap *pixmap;
	GdkPixbuf *tmp;
	int color;

	pwidth = gdk_pixbuf_get_width (pixbuf);
	pheight = gdk_pixbuf_get_height (pixbuf);
	g_assert (width <= pwidth && height <= pheight);

	color = ((bg->red & 0xff00) << 8) | (bg->green & 0xff00) | ((bg->blue & 0xff00) >> 8);

	if (width >= pwidth && height >= pheight) {
		tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
		if (!tmp)
			return NULL;

		gdk_pixbuf_composite_color (pixbuf, tmp,
					    0, 0,
					    width, height,
					    0, 0,
					    1.0, 1.0,
					    GDK_INTERP_NEAREST,
					    255,
					    0, 0,
					    16,
					    color, color);
	} else {
		int x, y, rowstride;
		GdkPixbuf *fade;
		guchar *pixels;

		/* Do a nice fade of the pixbuf down and to the right */

		fade = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
		if (!fade)
			return NULL;

		gdk_pixbuf_copy_area (pixbuf,
				      0, 0,
				      width, height,
				      fade,
				      0, 0);

		rowstride = gdk_pixbuf_get_rowstride (fade);
		pixels = gdk_pixbuf_get_pixels (fade);

		for (y = 0; y < height; y++) {
			guchar *p;
			int yfactor;

			p = pixels + y * rowstride;

			if (height < pheight)
				yfactor = height - y;
			else
				yfactor = height;

			for (x = 0; x < width; x++) {
				int xfactor;

				if (width < pwidth)
					xfactor = width - x;
				else
					xfactor = width;

				p[3] = ((int) p[3] * xfactor * yfactor / (width * height));
				p += 4;
			}
		}

		tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
		if (!tmp) {
			gdk_pixbuf_unref (fade);
			return NULL;
		}

		gdk_pixbuf_composite_color (fade, tmp,
					    0, 0,
					    width, height,
					    0, 0,
					    1.0, 1.0,
					    GDK_INTERP_NEAREST,
					    255,
					    0, 0,
					    16,
					    color, color);

		gdk_pixbuf_unref (fade);
	}

	pixmap = gdk_pixmap_new (drawable, width, height, gdk_rgb_get_visual ()->depth);
	gdk_draw_rgb_image_dithalign (pixmap, gc,
				      0, 0,
				      width, height,
				      GDK_RGB_DITHER_NORMAL,
				      gdk_pixbuf_get_pixels (tmp),
				      gdk_pixbuf_get_rowstride (tmp),
				      dither_xofs, dither_yofs);
	gdk_pixbuf_unref (tmp);

	return pixmap;
}

/**
 * e_table_header_draw_button:
 * @drawable: Destination drawable.
 * @ecol: Table column for the header information.
 * @style: Style to use for drawing the button.
 * @font: Font for the button's text.
 * @state: State of the table widget.
 * @widget: The table widget.
 * @gc: GC to use for drawing.
 * @x: Leftmost coordinate of the button.
 * @y: Topmost coordinate of the button.
 * @width: Width of the region to draw.
 * @height: Height of the region to draw.
 * @button_width: Width for the complete button.
 * @button_height: Height for the complete button.
 * @arrow: Arrow type to use as a sort indicator.
 * 
 * Draws a button suitable for a table header.
 **/
void
e_table_header_draw_button (GdkDrawable *drawable, ETableCol *ecol,
			    GtkStyle *style, GdkFont *font, GtkStateType state,
			    GtkWidget *widget, GdkGC *gc,
			    int x, int y, int width, int height,
			    int button_width, int button_height,
			    ETableColArrow arrow)
{
	int xthick, ythick;
	int inner_x, inner_y;
	int inner_width, inner_height;

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (ecol != NULL);
	g_return_if_fail (E_IS_TABLE_COL (ecol));
	g_return_if_fail (style != NULL);
	g_return_if_fail (font != NULL);
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (button_width > 0 && button_height > 0);

	xthick = style->klass->xthickness;
	ythick = style->klass->ythickness;

	/* Button bevel */

	gtk_paint_box (style, drawable, state, GTK_SHADOW_OUT,
		       NULL, widget, "button",
		       x, y, button_width, button_height);

	/* Inside area */

	inner_width = button_width - 2 * (xthick + HEADER_PADDING);
	inner_height = button_height - 2 * (ythick + HEADER_PADDING);

	if (inner_width < 1 || inner_height < 1)
		return; /* nothing fits */

	inner_x = x + xthick + HEADER_PADDING;
	inner_y = y + ythick + HEADER_PADDING;

	/* Arrow */

	switch (arrow) {
	case E_TABLE_COL_ARROW_NONE:
		break;

	case E_TABLE_COL_ARROW_UP:
	case E_TABLE_COL_ARROW_DOWN: {
		int arrow_width, arrow_height;

		arrow_width = MIN (MIN_ARROW_SIZE, inner_width);
		arrow_height = MIN (MIN_ARROW_SIZE, inner_height);

		gtk_paint_arrow (style, drawable, state,
				 GTK_SHADOW_IN, NULL, widget, "header",
				 (arrow == E_TABLE_COL_ARROW_UP) ? GTK_ARROW_UP : GTK_ARROW_DOWN,
				 TRUE,
				 inner_x + inner_width - arrow_width,
				 inner_y + (inner_height - arrow_height) / 2,
				 arrow_width, arrow_height);

		inner_width -= arrow_width + HEADER_PADDING;
		break;
	}

	default:
		g_assert_not_reached ();
		return;
	}

	if (inner_width < 1)
		return; /* nothing else fits */

	/* Pixbuf or label */

	if (ecol->is_pixbuf) {
		int pwidth, pheight;
		int clip_width, clip_height;
		GdkPixmap *pixmap;

		g_assert (ecol->pixbuf != NULL);

		pwidth = gdk_pixbuf_get_width (ecol->pixbuf);
		pheight = gdk_pixbuf_get_height (ecol->pixbuf);

		clip_width = MIN (pwidth, inner_width);
		clip_height = MIN (pheight, inner_height);

		pixmap = make_composite_pixmap (drawable, gc,
						ecol->pixbuf, &style->bg[state],
						clip_width, clip_height,
						inner_x,
						inner_y + (inner_height - clip_height) / 2);
		if (pixmap) {
			gdk_draw_pixmap (drawable, gc, pixmap,
					 0, 0,
					 inner_x,
					 inner_y + (inner_height - clip_height) / 2,
					 clip_width, clip_height);
			gdk_pixmap_unref (pixmap);
		}
	} else {
		int ypos;

		ypos = inner_y + (inner_height - font->ascent - font->descent) / 2 + font->ascent;

		e_table_draw_elided_string (drawable, font, gc,
					    inner_x, ypos,
					    ecol->text, inner_width, TRUE);
	}
}

/* Computes the length of a string that needs to be trimmed for elision */
static int
compute_elision_length (GdkFont *font, const char *str, int max_width)
{
	int len;
	int l = 0, left, right;
	int rbearing;

	len = strlen (str);

	if (len <= 0)
	  return 0;

	left = 0;
	right = len;

	while (left < right) {
		l = (left + right) / 2;
		gdk_text_extents (font, str, l, NULL, &rbearing, NULL, NULL, NULL);

		if (rbearing < max_width)
			left = l + 1;
		else if (rbearing > max_width)
			right = l;
		else
			return l;
	}

	if (rbearing > max_width)
		return MAX (0, l - 1);
	else
		return l;
}

/* Default width of the elision arrow in pixels */
#define ARROW_WIDTH 4

/**
 * e_table_draw_elided_string:
 * @drawable: Destination drawable.
 * @font: Font for the text.
 * @gc: GC to use for drawing.
 * @x: X insertion point for the string.
 * @y: Y insertion point for the string's baseline.
 * @str: String to draw.
 * @max_width: Maximum width in which the string must fit.
 * @center: Whether to center the string in the available area if it does fit.
 * 
 * Draws a string, possibly trimming it so that it fits inside the specified
 * maximum width.  If it does not fit, an elision indicator is drawn after the
 * last character that does fit.
 **/
void
e_table_draw_elided_string (GdkDrawable *drawable, GdkFont *font, GdkGC *gc,
			    int x, int y, const char *str, int max_width, gboolean center)
{
	int rbearing;
	int width;

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (font != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (str != NULL);
	g_return_if_fail (max_width >= 0);

	gdk_string_extents (font, str, NULL, &rbearing, &width, NULL, NULL);

	if (rbearing <= max_width) {
		int xpos;

		if (center)
			xpos = x + (max_width - width) / 2;
		else
			xpos = x;

		gdk_draw_string (drawable, font, gc, xpos, y, str);
	} else {
		int arrow_width;
		int len;
		int i;

		if (max_width < ARROW_WIDTH + 1)
			arrow_width = max_width - 1;
		else
			arrow_width = ARROW_WIDTH;

		len = compute_elision_length (font, str, max_width - arrow_width - 1);
		gdk_draw_text (drawable, font, gc, x, y, str, len);

		gdk_text_extents (font, str, len, NULL, &rbearing, NULL, NULL, NULL);

		y -= font->ascent;

		for (i = 0; i < arrow_width; i++) {
			int h;

			h = 2 * i + 1;

			gdk_draw_line (drawable, gc,
				       x + rbearing + arrow_width - i,
				       y + (font->ascent + font->descent - h) / 2,
				       x + rbearing + arrow_width - i,
				       y + (font->ascent + font->descent - h) / 2 + h - 1);
		}
	}
}
