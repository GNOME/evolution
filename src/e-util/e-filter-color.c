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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-filter-color.h"

G_DEFINE_TYPE (
	EFilterColor,
	e_filter_color,
	E_TYPE_FILTER_ELEMENT)

static void
set_color (GtkColorButton *color_button,
           EFilterColor *fc)
{
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (color_button), &fc->color);
}

static gint
filter_color_eq (EFilterElement *element_a,
                 EFilterElement *element_b)
{
	EFilterColor *color_a = E_FILTER_COLOR (element_a);
	EFilterColor *color_b = E_FILTER_COLOR (element_b);

	return E_FILTER_ELEMENT_CLASS (e_filter_color_parent_class)->
		eq (element_a, element_b) &&
		gdk_rgba_equal (&color_a->color, &color_b->color);
}

static xmlNodePtr
filter_color_xml_encode (EFilterElement *element)
{
	EFilterColor *fc = E_FILTER_COLOR (element);
	xmlNodePtr value;
	gchar spec[16];

	#define cnvrt(x) (((guint16) (65535 * (x))) & 0xFFFF)
	g_snprintf (
		spec, sizeof (spec), "#%04x%04x%04x",
		cnvrt (fc->color.red), cnvrt (fc->color.green), cnvrt (fc->color.blue));
	#undef cnvrt

	value = xmlNewNode (NULL, (xmlChar *)"value");
	xmlSetProp (value, (xmlChar *)"type", (xmlChar *)"colour");
	xmlSetProp (value, (xmlChar *)"name", (xmlChar *) element->name);
	xmlSetProp (value, (xmlChar *)"spec", (xmlChar *) spec);

	return value;
}

static gint
filter_color_xml_decode (EFilterElement *element,
                         xmlNodePtr node)
{
	EFilterColor *fc = E_FILTER_COLOR (element);
	xmlChar *prop;

	xmlFree (element->name);
	element->name = (gchar *) xmlGetProp (node, (xmlChar *)"name");

	prop = xmlGetProp (node, (xmlChar *)"spec");
	if (prop != NULL) {
		if (!gdk_rgba_parse (&fc->color, (gchar *) prop))
			g_warning ("%s: Failed to parse color from string '%s'", G_STRFUNC, prop);
		xmlFree (prop);
	} else {
		guint16 rr, gg, bb;

		/* try reading the old RGB properties */
		prop = xmlGetProp (node, (xmlChar *)"red");

		sscanf ((gchar *) prop, "%" G_GINT16_MODIFIER "x", &rr);
		fc->color.red = rr / 65535.0;
		xmlFree (prop);
		prop = xmlGetProp (node, (xmlChar *)"green");
		sscanf ((gchar *) prop, "%" G_GINT16_MODIFIER "x", &gg);
		fc->color.green = gg / 65535.0;
		xmlFree (prop);
		prop = xmlGetProp (node, (xmlChar *)"blue");
		sscanf ((gchar *) prop, "%" G_GINT16_MODIFIER "x", &bb);
		fc->color.blue = bb / 65535.0;
		xmlFree (prop);
	}

	return 0;
}

static GtkWidget *
filter_color_get_widget (EFilterElement *element)
{
	EFilterColor *fc = E_FILTER_COLOR (element);
	GtkWidget *color_button;

	color_button = gtk_color_button_new_with_rgba (&fc->color);
	gtk_widget_show (color_button);

	g_signal_connect (
		color_button, "color_set",
		G_CALLBACK (set_color), element);

	return color_button;
}

static void
filter_color_format_sexp (EFilterElement *element,
                          GString *out)
{
	EFilterColor *fc = E_FILTER_COLOR (element);
	gchar spec[16];

	#define cnvrt(x) (((guint16) (65535 * (x))) & 0xFFFF)
	g_snprintf (
		spec, sizeof (spec), "#%04x%04x%04x",
		cnvrt (fc->color.red), cnvrt (fc->color.green), cnvrt (fc->color.blue));
	#undef cnvrt
	camel_sexp_encode_string (out, spec);
}

static void
filter_color_describe (EFilterElement *element,
		       GString *out)
{
	EFilterColor *fc = E_FILTER_COLOR (element);
	gchar spec[16];

	#define cnvrt(x) (((guint16) (255 * (x))) & 0xFF)
	g_snprintf (
		spec, sizeof (spec), "#%02x%02x%02x",
		cnvrt (fc->color.red), cnvrt (fc->color.green), cnvrt (fc->color.blue));
	#undef cnvrt

	g_string_append_c (out, '[');
	g_string_append (out, spec);
	g_string_append (out, "] ");
	g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_COLOR_START);
	g_string_append (out, spec);
	g_string_append_c (out, E_FILTER_ELEMENT_DESCRIPTION_COLOR_END);
}

static void
e_filter_color_class_init (EFilterColorClass *class)
{
	EFilterElementClass *filter_element_class;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->eq = filter_color_eq;
	filter_element_class->xml_encode = filter_color_xml_encode;
	filter_element_class->xml_decode = filter_color_xml_decode;
	filter_element_class->get_widget = filter_color_get_widget;
	filter_element_class->format_sexp = filter_color_format_sexp;
	filter_element_class->describe = filter_color_describe;
}

static void
e_filter_color_init (EFilterColor *filter)
{
}

/**
 * filter_color_new:
 *
 * Create a new EFilterColor object.
 *
 * Return value: A new #EFilterColor object.
 **/
EFilterColor *
e_filter_color_new (void)
{
	return g_object_new (E_TYPE_FILTER_COLOR, NULL);
}
