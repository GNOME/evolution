/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id$
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 */
/*
  @NOTATION@
 */
/* Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 * Port to Pango co-done by Gergõ Érdi <cactus@cactus.rulez.org>
 */

#include "evolution-config.h"

#include <math.h>
#include <string.h>
#include "gnome-canvas-text.h"

#include "gnome-canvas-util.h"
#include "gnome-canvas-i18n.h"

/* Object argument IDs */
enum {
	PROP_0,

	/* Text contents */
	PROP_TEXT,
	PROP_MARKUP,

	/* Position */
	PROP_X,
	PROP_Y,

	/* Font */
	PROP_FONT,
	PROP_FONT_DESC,
	PROP_FAMILY, PROP_FAMILY_SET,

	/* Style */
	PROP_ATTRIBUTES,
	PROP_STYLE,         PROP_STYLE_SET,
	PROP_VARIANT,       PROP_VARIANT_SET,
	PROP_WEIGHT,        PROP_WEIGHT_SET,
	PROP_STRETCH,	    PROP_STRETCH_SET,
	PROP_SIZE,          PROP_SIZE_SET,
	PROP_SIZE_POINTS,
	PROP_STRIKETHROUGH, PROP_STRIKETHROUGH_SET,
	PROP_UNDERLINE,     PROP_UNDERLINE_SET,
	PROP_RISE,          PROP_RISE_SET,
	PROP_SCALE,         PROP_SCALE_SET,

	/* Clipping */
	PROP_JUSTIFICATION,
	PROP_CLIP_WIDTH,
	PROP_CLIP_HEIGHT,
	PROP_CLIP,
	PROP_X_OFFSET,
	PROP_Y_OFFSET,

	/* Coloring */
	PROP_FILL_COLOR,
	PROP_FILL_COLOR_GDK,
	PROP_FILL_COLOR_RGBA,

	/* Rendered size accessors */
	PROP_TEXT_WIDTH,
	PROP_TEXT_HEIGHT
};

static void gnome_canvas_text_dispose (GnomeCanvasItem *object);
static void gnome_canvas_text_set_property (GObject            *object,
					    guint               param_id,
					    const GValue       *value,
					    GParamSpec         *pspec);
static void gnome_canvas_text_get_property (GObject            *object,
					    guint               param_id,
					    GValue             *value,
					    GParamSpec         *pspec);

static void gnome_canvas_text_update (GnomeCanvasItem *item,
				      const cairo_matrix_t *matrix,
				      gint flags);
static void gnome_canvas_text_draw (GnomeCanvasItem *item, cairo_t *cr,
				    gint x, gint y, gint width, gint height);
static GnomeCanvasItem *gnome_canvas_text_point (GnomeCanvasItem *item,
                                                 gdouble x,
                                                 gdouble y,
                                                 gint cx,
                                                 gint cy);
static void gnome_canvas_text_bounds (GnomeCanvasItem *item,
				      gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);

static void gnome_canvas_text_set_markup (GnomeCanvasText *textitem,
					  const gchar     *markup);

static void gnome_canvas_text_set_font_desc    (GnomeCanvasText *textitem,
						PangoFontDescription *font_desc);

static void gnome_canvas_text_apply_font_desc  (GnomeCanvasText *textitem);
static void gnome_canvas_text_apply_attributes (GnomeCanvasText *textitem);

static void add_attr (PangoAttrList  *attr_list,
		      PangoAttribute *attr);

G_DEFINE_TYPE (
	GnomeCanvasText,
	gnome_canvas_text,
	GNOME_TYPE_CANVAS_ITEM)

/* Class initialization function for the text item */
static void
gnome_canvas_text_class_init (GnomeCanvasTextClass *class)
{
	GObjectClass *gobject_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gobject_class->set_property = gnome_canvas_text_set_property;
	gobject_class->get_property = gnome_canvas_text_get_property;

	/* Text */
	g_object_class_install_property (
		gobject_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
			"Text",
			"Text to render",
			NULL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_MARKUP,
		g_param_spec_string (
			"markup",
			"Markup",
			"Marked up text to render",
			NULL,
			G_PARAM_WRITABLE));

	/* Position */
	g_object_class_install_property (
		gobject_class,
		PROP_X,
		g_param_spec_double (
			"x",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_Y,
		g_param_spec_double (
			"y",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	/* Font */
	g_object_class_install_property (
		gobject_class,
		PROP_FONT,
		g_param_spec_string (
			"font",
			"Font",
			"Font description as a string",
			NULL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_FONT_DESC,
		g_param_spec_boxed (
			"font_desc",
			"Font description",
			"Font description as a PangoFontDescription struct",
			PANGO_TYPE_FONT_DESCRIPTION,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_FAMILY,
		g_param_spec_string (
			"family",
			"Font family",
			"Name of the font family, e.g. "
			"Sans, Helvetica, Times, Monospace",
			NULL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	/* Style */
	g_object_class_install_property (
		gobject_class,
		PROP_ATTRIBUTES,
		g_param_spec_boxed (
			"attributes",
			NULL,
			NULL,
			PANGO_TYPE_ATTR_LIST,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_STYLE,
		g_param_spec_enum (
			"style",
			"Font style",
			"Font style",
			PANGO_TYPE_STYLE,
			PANGO_STYLE_NORMAL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_VARIANT,
		g_param_spec_enum (
			"variant",
			"Font variant",
			"Font variant",
			PANGO_TYPE_VARIANT,
			PANGO_VARIANT_NORMAL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_WEIGHT,
		g_param_spec_int (
			"weight",
			"Font weight",
			"Font weight",
			0,
			G_MAXINT,
			PANGO_WEIGHT_NORMAL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_STRETCH,
		g_param_spec_enum (
			"stretch",
			"Font stretch",
			"Font stretch",
			PANGO_TYPE_STRETCH,
			PANGO_STRETCH_NORMAL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_SIZE,
		g_param_spec_int (
			"size",
			"Font size",
			"Font size (as a multiple of PANGO_SCALE, "
			"eg. 12*PANGO_SCALE for a 12pt font size)",
			0,
			G_MAXINT,
			0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_SIZE_POINTS,
		g_param_spec_double (
			"size_points",
			"Font points",
			"Font size in points (eg. 12 for a 12pt font size)",
			0.0,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_RISE,
		g_param_spec_int (
			"rise",
			"Rise",
			"Offset of text above the baseline "
			"(below the baseline if rise is negative)",
			-G_MAXINT,
			G_MAXINT,
			0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_STRIKETHROUGH,
		g_param_spec_boolean (
			"strikethrough",
			"Strikethrough",
			"Whether to strike through the text",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_UNDERLINE,
		g_param_spec_enum (
			"underline",
			"Underline",
			"Style of underline for this text",
			PANGO_TYPE_UNDERLINE,
			PANGO_UNDERLINE_NONE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_SCALE,
		g_param_spec_double (
			"scale",
			"Scale",
			"Size of font, relative to default size",
			0.0,
			G_MAXDOUBLE,
			1.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_JUSTIFICATION,
		g_param_spec_enum (
			"justification",
			NULL,
			NULL,
			GTK_TYPE_JUSTIFICATION,
			GTK_JUSTIFY_LEFT,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_CLIP_WIDTH,
		g_param_spec_double (
			"clip_width",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_CLIP_HEIGHT,
		g_param_spec_double (
			"clip_height",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_CLIP,
		g_param_spec_boolean (
			"clip",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_X_OFFSET,
		g_param_spec_double (
			"x_offset",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_Y_OFFSET,
		g_param_spec_double (
			"y_offset",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_FILL_COLOR,
		g_param_spec_string (
			"fill_color",
			"Color",
			"Text color, as string",
			NULL,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_FILL_COLOR_GDK,
		g_param_spec_boxed (
			"fill_color_gdk",
			"Color",
			"Text color, as a GdkColor",
			GDK_TYPE_COLOR,
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_FILL_COLOR_RGBA,
		g_param_spec_uint (
			"fill_color_rgba",
			"Color",
			"Text color, as an R/G/B/A combined integer",
			0, G_MAXUINT, 0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_TEXT_WIDTH,
		g_param_spec_double (
			"text_width",
			"Text width",
			"Width of the rendered text",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		gobject_class,
		PROP_TEXT_HEIGHT,
		g_param_spec_double (
			"text_height",
			"Text height",
			"Height of the rendered text",
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READABLE));

	/* Style props are set (explicitly applied) or not */
#define ADD_SET_PROP(propname, propval, nick, blurb) \
	g_object_class_install_property ( \
		gobject_class, propval, \
		g_param_spec_boolean ( \
			propname, nick, blurb, FALSE, \
			G_PARAM_READABLE | G_PARAM_WRITABLE))

	ADD_SET_PROP (
		"family_set",
		PROP_FAMILY_SET,
		"Font family set",
		"Whether this tag affects the font family");

	ADD_SET_PROP (
		"style_set",
		PROP_STYLE_SET,
		"Font style set",
		"Whether this tag affects the font style");

	ADD_SET_PROP (
		"variant_set",
		PROP_VARIANT_SET,
		"Font variant set",
		"Whether this tag affects the font variant");

	ADD_SET_PROP (
		"weight_set",
		PROP_WEIGHT_SET,
		"Font weight set",
		"Whether this tag affects the font weight");

	ADD_SET_PROP (
		"stretch_set",
		PROP_STRETCH_SET,
		"Font stretch set",
		"Whether this tag affects the font stretch");

	ADD_SET_PROP (
		"size_set",
		PROP_SIZE_SET,
		"Font size set",
		"Whether this tag affects the font size");

	ADD_SET_PROP (
		"rise_set",
		PROP_RISE_SET,
		"Rise set",
		"Whether this tag affects the rise");

	ADD_SET_PROP (
		"strikethrough_set",
		PROP_STRIKETHROUGH_SET,
		"Strikethrough set",
		"Whether this tag affects strikethrough");

	ADD_SET_PROP (
		"underline_set",
		PROP_UNDERLINE_SET,
		"Underline set",
		"Whether this tag affects underlining");

	ADD_SET_PROP (
		"scale_set",
		PROP_SCALE_SET,
		"Scale set",
		"Whether this tag affects font scaling");
#undef ADD_SET_PROP

	item_class->dispose = gnome_canvas_text_dispose;
	item_class->update = gnome_canvas_text_update;
	item_class->draw = gnome_canvas_text_draw;
	item_class->point = gnome_canvas_text_point;
	item_class->bounds = gnome_canvas_text_bounds;
}

/* Object initialization function for the text item */
static void
gnome_canvas_text_init (GnomeCanvasText *text)
{
	text->x = 0.0;
	text->y = 0.0;
	text->justification = GTK_JUSTIFY_LEFT;
	text->clip_width = 0.0;
	text->clip_height = 0.0;
	text->xofs = 0.0;
	text->yofs = 0.0;
	text->layout = NULL;

	text->font_desc = NULL;

	text->underline = PANGO_UNDERLINE_NONE;
	text->strikethrough = FALSE;
	text->rise = 0;

	text->underline_set = FALSE;
	text->strike_set = FALSE;
	text->rise_set = FALSE;
}

/* Dispose handler for the text item */
static void
gnome_canvas_text_dispose (GnomeCanvasItem *object)
{
	GnomeCanvasText *text;

	g_return_if_fail (GNOME_IS_CANVAS_TEXT (object));

	text = GNOME_CANVAS_TEXT (object);

	g_free (text->text);
	text->text = NULL;

	if (text->layout != NULL) {
		g_object_unref (text->layout);
		text->layout = NULL;
	}

	if (text->font_desc != NULL) {
		pango_font_description_free (text->font_desc);
		text->font_desc = NULL;
	}

	if (text->attr_list != NULL) {
		pango_attr_list_unref (text->attr_list);
		text->attr_list = NULL;
	}

	GNOME_CANVAS_ITEM_CLASS (gnome_canvas_text_parent_class)->
		dispose (object);
}

static void
get_bounds (GnomeCanvasText *text,
            gdouble *px1,
            gdouble *py1,
            gdouble *px2,
            gdouble *py2)
{
	GnomeCanvasItem *item;
	gdouble wx, wy;

	item = GNOME_CANVAS_ITEM (text);

	/* Get canvas pixel coordinates for text position */

	wx = text->x;
	wy = text->y;
	gnome_canvas_item_i2w (item, &wx, &wy);
	gnome_canvas_w2c (
		item->canvas, wx + text->xofs, wy + text->yofs,
		&text->cx, &text->cy);

	/* Get canvas pixel coordinates for clip rectangle position */

	gnome_canvas_w2c (item->canvas, wx, wy, &text->clip_cx, &text->clip_cy);
	text->clip_cwidth = text->clip_width;
	text->clip_cheight = text->clip_height;

	/* Bounds */

	if (text->clip) {
		*px1 = text->clip_cx;
		*py1 = text->clip_cy;
		*px2 = text->clip_cx + text->clip_cwidth;
		*py2 = text->clip_cy + text->clip_cheight;
	} else {
		*px1 = text->cx;
		*py1 = text->cy;
		*px2 = text->cx + text->max_width;
		*py2 = text->cy + text->height;
	}
}

static PangoFontMask
get_property_font_set_mask (guint property_id)
{
	switch (property_id) {
		case PROP_FAMILY_SET:
			return PANGO_FONT_MASK_FAMILY;
		case PROP_STYLE_SET:
			return PANGO_FONT_MASK_STYLE;
		case PROP_VARIANT_SET:
			return PANGO_FONT_MASK_VARIANT;
		case PROP_WEIGHT_SET:
			return PANGO_FONT_MASK_WEIGHT;
		case PROP_STRETCH_SET:
			return PANGO_FONT_MASK_STRETCH;
		case PROP_SIZE_SET:
			return PANGO_FONT_MASK_SIZE;
	}

	return 0;
}

static void
ensure_font (GnomeCanvasText *text)
{
	if (!text->font_desc)
		text->font_desc = pango_font_description_new ();
}

/* Set_arg handler for the text item */
static void
gnome_canvas_text_set_property (GObject *object,
                                guint param_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasText *text;
	GdkColor *pcolor;
	PangoAlignment align;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_TEXT (object));

	item = GNOME_CANVAS_ITEM (object);
	text = GNOME_CANVAS_TEXT (object);

	if (!text->layout)
		text->layout = pango_layout_new (
			gtk_widget_get_pango_context (
			GTK_WIDGET (item->canvas)));

	switch (param_id) {
	case PROP_TEXT:
		g_free (text->text);

		text->text = g_value_dup_string (value);
		pango_layout_set_text (text->layout, text->text, -1);

		break;

	case PROP_MARKUP:
		gnome_canvas_text_set_markup (
			text, g_value_get_string (value));
		break;

	case PROP_X:
		text->x = g_value_get_double (value);
		break;

	case PROP_Y:
		text->y = g_value_get_double (value);
		break;

	case PROP_FONT: {
		const gchar *font_name;
		PangoFontDescription *font_desc;

		font_name = g_value_get_string (value);
		if (font_name)
			font_desc = pango_font_description_from_string (font_name);
		else
			font_desc = NULL;

		gnome_canvas_text_set_font_desc (text, font_desc);
		if (font_desc)
			pango_font_description_free (font_desc);

		break;
	}

	case PROP_FONT_DESC:
		gnome_canvas_text_set_font_desc (text, g_value_peek_pointer (value));
		break;

	case PROP_FAMILY:
	case PROP_STYLE:
	case PROP_VARIANT:
	case PROP_WEIGHT:
	case PROP_STRETCH:
	case PROP_SIZE:
	case PROP_SIZE_POINTS:
		ensure_font (text);

		switch (param_id) {
		case PROP_FAMILY:
			pango_font_description_set_family (
				text->font_desc,
				g_value_get_string (value));
			break;
		case PROP_STYLE:
			pango_font_description_set_style (
				text->font_desc,
				g_value_get_enum (value));
			break;
		case PROP_VARIANT:
			pango_font_description_set_variant (
				text->font_desc,
				g_value_get_enum (value));
			break;
		case PROP_WEIGHT:
			pango_font_description_set_weight (
				text->font_desc,
				g_value_get_int (value));
			break;
		case PROP_STRETCH:
			pango_font_description_set_stretch (
				text->font_desc,
				g_value_get_enum (value));
			break;
		case PROP_SIZE:
			/* FIXME: This is bogus! It should be pixels, not points/PANGO_SCALE! */
			pango_font_description_set_size (
				text->font_desc,
				g_value_get_int (value));
			break;
		case PROP_SIZE_POINTS:
			pango_font_description_set_size (
				text->font_desc,
				g_value_get_double (value) * PANGO_SCALE);
			break;
		}

		gnome_canvas_text_apply_font_desc (text);
		break;

	case PROP_FAMILY_SET:
	case PROP_STYLE_SET:
	case PROP_VARIANT_SET:
	case PROP_WEIGHT_SET:
	case PROP_STRETCH_SET:
	case PROP_SIZE_SET:
		if (!g_value_get_boolean (value) && text->font_desc)
			pango_font_description_unset_fields (
				text->font_desc,
				get_property_font_set_mask (param_id));
		break;

	case PROP_SCALE:
		text->scale = g_value_get_double (value);
		text->scale_set = TRUE;

		gnome_canvas_text_apply_font_desc (text);
		break;

	case PROP_SCALE_SET:
		text->scale_set = g_value_get_boolean (value);

		gnome_canvas_text_apply_font_desc (text);
		break;

	case PROP_UNDERLINE:
		text->underline = g_value_get_enum (value);
		text->underline_set = TRUE;

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_UNDERLINE_SET:
		text->underline_set = g_value_get_boolean (value);

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_STRIKETHROUGH:
		text->strikethrough = g_value_get_boolean (value);
		text->strike_set = TRUE;

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_STRIKETHROUGH_SET:
		text->strike_set = g_value_get_boolean (value);

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_RISE:
		text->rise = g_value_get_int (value);
		text->rise_set = TRUE;

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_RISE_SET:
		text->rise_set = TRUE;

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_ATTRIBUTES:
		if (text->attr_list)
			pango_attr_list_unref (text->attr_list);

		text->attr_list = g_value_peek_pointer (value);
		pango_attr_list_ref (text->attr_list);

		gnome_canvas_text_apply_attributes (text);
		break;

	case PROP_JUSTIFICATION:
		text->justification = g_value_get_enum (value);

		switch (text->justification) {
		case GTK_JUSTIFY_LEFT:
			align = PANGO_ALIGN_LEFT;
			break;
		case GTK_JUSTIFY_CENTER:
			align = PANGO_ALIGN_CENTER;
			break;
		case GTK_JUSTIFY_RIGHT:
			align = PANGO_ALIGN_RIGHT;
			break;
		default:
			/* GTK_JUSTIFY_FILL isn't supported yet. */
			align = PANGO_ALIGN_LEFT;
			break;
		}
		pango_layout_set_alignment (text->layout, align);
		break;

	case PROP_CLIP_WIDTH:
		text->clip_width = fabs (g_value_get_double (value));
		break;

	case PROP_CLIP_HEIGHT:
		text->clip_height = fabs (g_value_get_double (value));
		break;

	case PROP_CLIP:
		text->clip = g_value_get_boolean (value);
		break;

	case PROP_X_OFFSET:
		text->xofs = g_value_get_double (value);
		break;

	case PROP_Y_OFFSET:
		text->yofs = g_value_get_double (value);
		break;

	case PROP_FILL_COLOR: {
		const gchar *color_name;

		color_name = g_value_get_string (value);
		if (color_name) {
			GdkColor color;
			if (gdk_color_parse (color_name, &color)) {
				text->rgba = ((color.red & 0xff00) << 16 |
					      (color.green & 0xff00) << 8 |
					      (color.blue & 0xff00) |
					      0xff);
			} else {
				g_warning (
					"%s: Failed to parse color form string '%s'",
					G_STRFUNC, color_name);
			}
		}
		break;
	}

	case PROP_FILL_COLOR_GDK:
		pcolor = g_value_get_boxed (value);
		if (pcolor) {
			text->rgba = ((pcolor->red & 0xff00) << 16 |
				      (pcolor->green & 0xff00) << 8|
				      (pcolor->blue & 0xff00) |
				      0xff);
		} else {
			text->rgba = 0;
		}
		break;

	case PROP_FILL_COLOR_RGBA:
		text->rgba = g_value_get_uint (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

	/* Calculate text dimensions */

	if (text->layout)
		pango_layout_get_pixel_size (
			text->layout,
			&text->max_width,
			&text->height);
	else {
		text->max_width = 0;
		text->height = 0;
	}

	gnome_canvas_item_request_update (item);
}

/* Get_arg handler for the text item */
static void
gnome_canvas_text_get_property (GObject *object,
                                guint param_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	GnomeCanvasText *text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_TEXT (object));

	text = GNOME_CANVAS_TEXT (object);

	switch (param_id) {
	case PROP_TEXT:
		g_value_set_string (value, text->text);
		break;

	case PROP_X:
		g_value_set_double (value, text->x);
		break;

	case PROP_Y:
		g_value_set_double (value, text->y);
		break;

	case PROP_FONT:
	case PROP_FONT_DESC:
	case PROP_FAMILY:
	case PROP_STYLE:
	case PROP_VARIANT:
	case PROP_WEIGHT:
	case PROP_STRETCH:
	case PROP_SIZE:
	case PROP_SIZE_POINTS:
		ensure_font (text);

		switch (param_id) {
		case PROP_FONT:
		{
			/* FIXME GValue imposes a totally gratuitous string
			 *       copy here, we could just hand off string
			 *       ownership. */
			gchar *str;

			str = pango_font_description_to_string (text->font_desc);
			g_value_set_string (value, str);
			g_free (str);

			break;
		}

		case PROP_FONT_DESC:
			g_value_set_boxed (value, text->font_desc);
			break;

		case PROP_FAMILY:
			g_value_set_string (
				value,
				pango_font_description_get_family (
				text->font_desc));
			break;

		case PROP_STYLE:
			g_value_set_enum (
				value,
				pango_font_description_get_style (
				text->font_desc));
			break;

		case PROP_VARIANT:
			g_value_set_enum (
				value,
				pango_font_description_get_variant (
				text->font_desc));
			break;

		case PROP_WEIGHT:
			g_value_set_int (
				value,
				pango_font_description_get_weight (
				text->font_desc));
			break;

		case PROP_STRETCH:
			g_value_set_enum (
				value,
				pango_font_description_get_stretch (
				text->font_desc));
			break;

		case PROP_SIZE:
			g_value_set_int (
				value,
				pango_font_description_get_size (
				text->font_desc));
			break;

		case PROP_SIZE_POINTS:
			g_value_set_double (
				value, ((gdouble)
				pango_font_description_get_size (
				text->font_desc)) / (gdouble) PANGO_SCALE);
			break;
		}
		break;

	case PROP_FAMILY_SET:
	case PROP_STYLE_SET:
	case PROP_VARIANT_SET:
	case PROP_WEIGHT_SET:
	case PROP_STRETCH_SET:
	case PROP_SIZE_SET:
	{
		PangoFontMask set_mask = text->font_desc ?
			pango_font_description_get_set_fields (text->font_desc) : 0;
		PangoFontMask test_mask = get_property_font_set_mask (param_id);
		g_value_set_boolean (value, (set_mask & test_mask) != 0);

		break;
	}

	case PROP_SCALE:
		g_value_set_double (value, text->scale);
		break;
	case PROP_SCALE_SET:
		g_value_set_boolean (value, text->scale_set);
		break;

	case PROP_UNDERLINE:
		g_value_set_enum (value, text->underline);
		break;
	case PROP_UNDERLINE_SET:
		g_value_set_boolean (value, text->underline_set);
		break;

	case PROP_STRIKETHROUGH:
		g_value_set_boolean (value, text->strikethrough);
		break;
	case PROP_STRIKETHROUGH_SET:
		g_value_set_boolean (value, text->strike_set);
		break;

	case PROP_RISE:
		g_value_set_int (value, text->rise);
		break;
	case PROP_RISE_SET:
		g_value_set_boolean (value, text->rise_set);
		break;

	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, text->attr_list);
		break;

	case PROP_JUSTIFICATION:
		g_value_set_enum (value, text->justification);
		break;

	case PROP_CLIP_WIDTH:
		g_value_set_double (value, text->clip_width);
		break;

	case PROP_CLIP_HEIGHT:
		g_value_set_double (value, text->clip_height);
		break;

	case PROP_CLIP:
		g_value_set_boolean (value, text->clip);
		break;

	case PROP_X_OFFSET:
		g_value_set_double (value, text->xofs);
		break;

	case PROP_Y_OFFSET:
		g_value_set_double (value, text->yofs);
		break;

	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, text->rgba);
		break;

	case PROP_TEXT_WIDTH:
		g_value_set_double (value, text->max_width);
		break;

	case PROP_TEXT_HEIGHT:
		g_value_set_double (value, text->height);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* */
static void
gnome_canvas_text_apply_font_desc (GnomeCanvasText *text)
{
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	GtkWidget *widget;

	widget = GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas);
	pango_context = gtk_widget_create_pango_context (widget);
	font_desc = pango_font_description_copy (pango_context_get_font_description (pango_context));
	g_object_unref (pango_context);

	if (text->font_desc)
		pango_font_description_merge (font_desc, text->font_desc, TRUE);

	pango_layout_set_font_description (text->layout, font_desc);
	pango_font_description_free (font_desc);
}

static void
add_attr (PangoAttrList *attr_list,
          PangoAttribute *attr)
{
	attr->start_index = 0;
	attr->end_index = G_MAXINT;

	pango_attr_list_insert (attr_list, attr);
}

/* */
static void
gnome_canvas_text_apply_attributes (GnomeCanvasText *text)
{
	PangoAttrList *attr_list;

	if (text->attr_list)
		attr_list = pango_attr_list_copy (text->attr_list);
	else
		attr_list = pango_attr_list_new ();

	if (text->underline_set)
		add_attr (attr_list, pango_attr_underline_new (text->underline));
	if (text->strike_set)
		add_attr (attr_list, pango_attr_strikethrough_new (text->strikethrough));
	if (text->rise_set)
		add_attr (attr_list, pango_attr_rise_new (text->rise));

	pango_layout_set_attributes (text->layout, attr_list);
	pango_attr_list_unref (attr_list);
}

static void
gnome_canvas_text_set_font_desc (GnomeCanvasText *text,
                                 PangoFontDescription *font_desc)
{
	if (text->font_desc)
		pango_font_description_free (text->font_desc);

	if (font_desc)
		text->font_desc = pango_font_description_copy (font_desc);
	else
		text->font_desc = NULL;

	gnome_canvas_text_apply_font_desc (text);
}

/* Setting the text from a Pango markup string */
static void
gnome_canvas_text_set_markup (GnomeCanvasText *textitem,
                              const gchar *markup)
{
	PangoAttrList *attr_list = NULL;
	gchar         *text = NULL;
	GError        *error = NULL;

	if (markup && !pango_parse_markup (markup, -1,
					   0,
					   &attr_list, &text, NULL,
					   &error))
	{
		g_warning (
			"Failed to set cell text from markup due to "
			"error parsing markup: %s", error->message);
		g_error_free (error);
		return;
	}

	g_free (textitem->text);
	if (textitem->attr_list)
		pango_attr_list_unref (textitem->attr_list);

	textitem->text = text;
	textitem->attr_list = attr_list;

	pango_layout_set_text (textitem->layout, text, -1);

	gnome_canvas_text_apply_attributes (textitem);
}

/* Update handler for the text item */
static void
gnome_canvas_text_update (GnomeCanvasItem *item,
                          const cairo_matrix_t *matrix,
                          gint flags)
{
	GnomeCanvasText *text;
	gdouble x1, y1, x2, y2;

	text = GNOME_CANVAS_TEXT (item);

	GNOME_CANVAS_ITEM_CLASS (gnome_canvas_text_parent_class)->
		update (item, matrix, flags);

	get_bounds (text, &x1, &y1, &x2, &y2);

	gnome_canvas_update_bbox (
		item,
		floor (x1), floor (y1),
		ceil (x2), ceil (y2));
}

/* Draw handler for the text item */
static void
gnome_canvas_text_draw (GnomeCanvasItem *item,
                        cairo_t *cr,
                        gint x,
                        gint y,
                        gint width,
                        gint height)
{
	GnomeCanvasText *text = GNOME_CANVAS_TEXT (item);

	if (!text->text)
		return;

	cairo_save (cr);

	if (text->clip) {
		cairo_rectangle (
			cr,
			text->clip_cx - x,
			text->clip_cy - y,
			text->clip_cwidth,
			text->clip_cheight);
		cairo_clip (cr);
	}

	cairo_set_source_rgba (
		cr,
		((text->rgba >> 24) & 0xff) / 255.0,
		((text->rgba >> 16) & 0xff) / 255.0,
		((text->rgba >> 8) & 0xff) / 255.0,
		( text->rgba & 0xff) / 255.0);

	cairo_move_to (cr, text->cx - x, text->cy - y);
	pango_cairo_show_layout (cr, text->layout);

	cairo_restore (cr);
}

/* Point handler for the text item */
static GnomeCanvasItem *
gnome_canvas_text_point (GnomeCanvasItem *item,
                         gdouble x,
                         gdouble y,
                         gint cx,
                         gint cy)
{
	GnomeCanvasText *text;
	PangoLayoutIter *iter;
	gint x1, y1, x2, y2;

	text = GNOME_CANVAS_TEXT (item);

	/* The idea is to build bounding rectangles for each of the lines of
	 * text (clipped by the clipping rectangle, if it is activated) and see
	 * whether the point is inside any of these.  If it is, we are done.
	 * Otherwise, calculate the distance to the nearest rectangle.
	 */

	iter = pango_layout_get_iter (text->layout);
	do {
		PangoRectangle log_rect;

		pango_layout_iter_get_line_extents (iter, NULL, &log_rect);

		x1 = text->cx + PANGO_PIXELS (log_rect.x);
		y1 = text->cy + PANGO_PIXELS (log_rect.y);
		x2 = x1 + PANGO_PIXELS (log_rect.width);
		y2 = y1 + PANGO_PIXELS (log_rect.height);

		if (text->clip) {
			if (x1 < text->clip_cx)
				x1 = text->clip_cx;

			if (y1 < text->clip_cy)
				y1 = text->clip_cy;

			if (x2 > (text->clip_cx + text->clip_width))
				x2 = text->clip_cx + text->clip_width;

			if (y2 > (text->clip_cy + text->clip_height))
				y2 = text->clip_cy + text->clip_height;

			if ((x1 >= x2) || (y1 >= y2))
				continue;
		}

		/* Calculate distance from point to rectangle */

		if (cx >= x1 && cx < x2 && cy >= y1 && cy < y2) {
			pango_layout_iter_free (iter);
			return item;
		}

	} while (pango_layout_iter_next_line (iter));

	pango_layout_iter_free (iter);

	return NULL;
}

/* Bounds handler for the text item */
static void
gnome_canvas_text_bounds (GnomeCanvasItem *item,
                          gdouble *x1,
                          gdouble *y1,
                          gdouble *x2,
                          gdouble *y2)
{
	GnomeCanvasText *text;
	gdouble width, height;

	text = GNOME_CANVAS_TEXT (item);

	*x1 = text->x;
	*y1 = text->y;

	if (text->clip) {
		width = text->clip_width;
		height = text->clip_height;
	} else {
		width = text->max_width;
		height = text->height;
	}

	*x2 = *x1 + width;
	*y2 = *y1 + height;
}
