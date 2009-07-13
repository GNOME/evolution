/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "libedataserver/e-sexp.h"
#include "filter-colour.h"

#define d(x)

static gint colour_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static gint xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_colour_class_init (FilterColourClass *klass);
static void filter_colour_init (FilterColour *fc);
static void filter_colour_finalise (GObject *obj);

static FilterElementClass *parent_class;

GType
filter_colour_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterColourClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_colour_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterColour),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_colour_init,
		};

		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterColour", &info, 0);
	}

	return type;
}

static void
filter_colour_class_init (FilterColourClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);

	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);

	object_class->finalize = filter_colour_finalise;

	/* override methods */
	fe_class->eq = colour_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_colour_init (FilterColour *fc)
{
	;
}

static void
filter_colour_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_colour_new:
 *
 * Create a new FilterColour object.
 *
 * Return value: A new #FilterColour object.
 **/
FilterColour *
filter_colour_new (void)
{
	return (FilterColour *) g_object_new (FILTER_TYPE_COLOUR, NULL, NULL);
}

static gint
colour_eq (FilterElement *fe, FilterElement *cm)
{
	FilterColour *fc = (FilterColour *) fe;
	FilterColour *cc = (FilterColour *) cm;

	return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& gdk_color_equal (&fc->color, &cc->color);
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	FilterColour *fc = (FilterColour *)fe;
	xmlNodePtr value;
	gchar spec[16];

	g_snprintf (spec, sizeof (spec), "#%04x%04x%04x",
		fc->color.red, fc->color.green, fc->color.blue);

	value = xmlNewNode(NULL, (const guchar *)"value");
	xmlSetProp(value, (const guchar *)"type", (const guchar *)"colour");
	xmlSetProp(value, (const guchar *)"name", (guchar *)fe->name);
	xmlSetProp(value, (const guchar *)"spec", (guchar *)spec);

	return value;
}

static gint
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterColour *fc = (FilterColour *)fe;
	xmlChar *prop;

	xmlFree (fe->name);
	fe->name = (gchar *)xmlGetProp(node, (const guchar *)"name");

	prop = xmlGetProp(node, (const guchar *)"spec");
	if (prop != NULL) {
		gdk_color_parse((gchar *)prop, &fc->color);
		xmlFree (prop);
	} else {
		/* try reading the old RGB properties */
		prop = xmlGetProp(node, (const guchar *)"red");
		sscanf((gchar *)prop, "%" G_GINT16_MODIFIER "x", &fc->color.red);
		xmlFree (prop);
		prop = xmlGetProp(node, (const guchar *)"green");
		sscanf((gchar *)prop, "%" G_GINT16_MODIFIER "x", &fc->color.green);
		xmlFree (prop);
		prop = xmlGetProp(node, (const guchar *)"blue");
		sscanf((gchar *)prop, "%" G_GINT16_MODIFIER "x", &fc->color.blue);
		xmlFree (prop);
	}

	return 0;
}

static void
set_color (GtkColorButton *color_button, FilterColour *fc)
{
	gtk_color_button_get_color (color_button, &fc->color);
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterColour *fc = (FilterColour *) fe;
	GtkWidget *color_button;

	color_button = gtk_color_button_new_with_color (&fc->color);
	gtk_widget_show (color_button);

	g_signal_connect (
		G_OBJECT (color_button), "color_set",
		G_CALLBACK (set_color), fe);

	return color_button;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterColour *fc = (FilterColour *)fe;
	gchar spec[16];

	g_snprintf (spec, sizeof (spec), "#%04x%04x%04x",
		fc->color.red, fc->color.green, fc->color.blue);
	e_sexp_encode_string (out, spec);
}
