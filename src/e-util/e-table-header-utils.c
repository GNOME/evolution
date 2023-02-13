/*
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
 *		Chris Lahey <clahey@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-table-header-utils.h"

#include <string.h> /* strlen() */

#include <gtk/gtk.h>

#include "e-table-defines.h"
#include "e-unicode.h"

static void
get_button_padding (GtkWidget *widget,
                    GtkBorder *padding)
{
	GtkStyleContext *context;
	GtkStateFlags state_flags;

	context = gtk_widget_get_style_context (widget);
	state_flags = gtk_widget_get_state_flags (widget);

	gtk_style_context_save (context);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);
	gtk_style_context_set_state (context, state_flags);
	gtk_style_context_get_padding (context, state_flags, padding);

	gtk_style_context_restore (context);
}

/**
 * e_table_header_compute_height:
 * @ecol: Table column description.
 * @widget: The widget from which to build the PangoLayout.
 *
 * Computes the minimum height required for a table header button.
 *
 * Return value: The height of the button, in pixels.
 **/
gdouble
e_table_header_compute_height (ETableCol *ecol,
                               GtkWidget *widget)
{
	gint height;
	PangoLayout *layout;
	GtkBorder padding;

	g_return_val_if_fail (ecol != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_COL (ecol), -1);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), -1);

	get_button_padding (widget, &padding);

	layout = gtk_widget_create_pango_layout (widget, ecol->text);

	pango_layout_get_pixel_size (layout, NULL, &height);

	if (ecol->icon_name != NULL) {
		e_table_col_ensure_surface (ecol, widget);
		g_return_val_if_fail (ecol->surface != NULL, -1);
		height = MAX (height, ecol->surface_height);
	}

	height = MAX (height, MIN_ARROW_SIZE);
	height += padding.top + padding.bottom + 2 * HEADER_PADDING;

	g_object_unref (layout);

	return height;
}

gdouble
e_table_header_width_extras (GtkWidget *widget)
{
	GtkBorder padding;

	get_button_padding (widget, &padding);
	return padding.left + padding.right + 2 * HEADER_PADDING;
}

/**
 * e_table_header_draw_button:
 * @cr: a cairo context
 * @ecol: Table column for the header information.
 * @widget: The table widget.
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
e_table_header_draw_button (cairo_t *cr,
                            ETableCol *ecol,
                            GtkWidget *widget,
                            gint x,
                            gint y,
                            gint width,
                            gint height,
                            gint button_width,
                            gint button_height,
                            ETableColArrow arrow)
{
	gint inner_x, inner_y;
	gint inner_width, inner_height;
	gint arrow_width = 0, arrow_height = 0, text_height = 0;
	PangoContext *pango_context;
	PangoLayout *layout;
	GtkStyleContext *context;
	GtkBorder padding;
	GtkStateFlags state_flags;

	g_return_if_fail (cr != NULL);
	g_return_if_fail (ecol != NULL);
	g_return_if_fail (E_IS_TABLE_COL (ecol));
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (button_width > 0 && button_height > 0);

	/* Button bevel */
	context = gtk_widget_get_style_context (widget);
	state_flags = gtk_widget_get_state_flags (widget);

	gtk_style_context_save (context);
	gtk_style_context_set_state (context, state_flags);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

	gtk_style_context_get_padding (context, state_flags, &padding);

	gtk_render_background (
		context, cr, x, y,
		button_width, button_height);
	gtk_render_frame (
		context, cr, x, y,
		button_width, button_height);

	/* Inside area */

	inner_width =
		button_width -
		(padding.left + padding.right + 2 * HEADER_PADDING);
	inner_height =
		button_height -
		(padding.top + padding.bottom + 2 * HEADER_PADDING);

	if (inner_width < 1 || inner_height < 1) {
		gtk_style_context_restore (context);
		return; /* nothing fits */
	}

	inner_x = x + padding.left + HEADER_PADDING;
	inner_y = y + padding.top + HEADER_PADDING;

	/* Arrow space */

	switch (arrow) {
	case E_TABLE_COL_ARROW_NONE:
		break;

	case E_TABLE_COL_ARROW_UP:
	case E_TABLE_COL_ARROW_DOWN:
		arrow_width = MIN (MIN_ARROW_SIZE, inner_width);
		arrow_height = MIN (MIN_ARROW_SIZE, inner_height);

		if (ecol->icon_name == NULL)
			inner_width -= arrow_width + HEADER_PADDING;
		break;
	default:
		gtk_style_context_restore (context);
		g_warn_if_reached ();
		return;
	}

	if (inner_width < 1) {
		gtk_style_context_restore (context);
		return; /* nothing else fits */
	}

	layout = gtk_widget_create_pango_layout (widget, ecol->text);
	pango_layout_get_pixel_size (layout, NULL, &text_height);
	g_object_unref (layout);

	pango_context = gtk_widget_create_pango_context (widget);
	layout = pango_layout_new (pango_context);
	g_object_unref (pango_context);

	pango_layout_set_text (layout, ecol->text, -1);
	pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

	/* Pixbuf or label */
	if (ecol->icon_name != NULL) {
		gint pwidth, pheight;
		gint clip_height;
		gint xpos;

		e_table_col_ensure_surface (ecol, widget);

		g_return_if_fail (ecol->surface != NULL);

		pwidth = ecol->surface_width;
		pheight = ecol->surface_height;

		clip_height = MIN (pheight, inner_height);

		xpos = inner_x;

		if (inner_width - pwidth > 11) {
			gint ypos;

			pango_layout_get_pixel_size (layout, &width, NULL);

			if (width < inner_width - (pwidth + 1)) {
				xpos = inner_x + (inner_width - width - (pwidth + 1)) / 2;
			}

			ypos = inner_y + MAX (0, (inner_height - text_height) / 2);

			pango_layout_set_width (
				layout, (inner_width - (xpos - inner_x)) *
				PANGO_SCALE);

			gtk_render_layout (
				context, cr, xpos + pwidth + 1,
				ypos, layout);
		}

		gtk_render_icon_surface (
			context, cr, ecol->surface, xpos + 1,
			inner_y + (inner_height - clip_height) / 2);

	} else {
		pango_layout_set_width (layout, inner_width * PANGO_SCALE);

		gtk_render_layout (context, cr, inner_x, inner_y + MAX (0, (inner_height - text_height) / 2), layout);
	}

	switch (arrow) {
	case E_TABLE_COL_ARROW_NONE:
		break;

	case E_TABLE_COL_ARROW_UP:
	case E_TABLE_COL_ARROW_DOWN: {
		if (ecol->icon_name == NULL)
			inner_width += arrow_width + HEADER_PADDING;

		gtk_render_arrow (
			context, cr,
			(arrow == E_TABLE_COL_ARROW_UP) ? 0 : G_PI,
			inner_x + inner_width - arrow_width,
			inner_y + (inner_height - arrow_height) / 2,
			MAX (arrow_width, arrow_height));

		break;
	}

	default:
		g_warn_if_reached ();
		break;
	}

	g_object_unref (layout);
	gtk_style_context_restore (context);
}
