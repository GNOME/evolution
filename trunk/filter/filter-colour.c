/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-color-picker.h>

#include "libedataserver/e-sexp.h"
#include "filter-colour.h"

#define d(x)

static int colour_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
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

static int
colour_eq (FilterElement *fe, FilterElement *cm)
{
	FilterColour *fc = (FilterColour *)fe, *cc = (FilterColour *)cm;
	
        return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& fc->r == cc->r
		&& fc->g == cc->g
		&& fc->b == cc->b
		&& fc->a == cc->a;
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
	xmlNodePtr value;
	FilterColour *fc = (FilterColour *)fe;
	char hex[16];
	
	d(printf("Encoding colour as xml\n"));
	value = xmlNewNode(NULL, "value");
	xmlSetProp(value, "name", fe->name);
	xmlSetProp(value, "type", "colour");
	
	sprintf(hex, "%04x", fc->r);
	xmlSetProp(value, "red", hex);
	sprintf(hex, "%04x", fc->g);
	xmlSetProp(value, "green", hex);
	sprintf(hex, "%04x", fc->b);
	xmlSetProp(value, "blue", hex);
	sprintf(hex, "%04x", fc->a);
	xmlSetProp(value, "alpha", hex);
	
	return value;
}

static guint16
get_value (xmlNodePtr node, char *name)
{
	unsigned int ret;
	char *value;
	
	value = xmlGetProp(node, name);
	sscanf(value, "%04x", &ret);
	xmlFree(value);
	return ret;
}


static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterColour *fc = (FilterColour *)fe;
	
	xmlFree (fe->name);
	fe->name = xmlGetProp(node, "name");
	fc->r = get_value(node, "red");
	fc->g = get_value(node, "green");
	fc->b = get_value(node, "blue");
	fc->a = get_value(node, "alpha");
	
	return 0;
}

static void
set_colour (GnomeColorPicker *cp, guint r, guint g, guint b, guint a, FilterColour *fc)
{
	fc->r = r;
	fc->g = g;
	fc->b = b;
	fc->a = a;
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterColour *fc = (FilterColour *) fe;
	GnomeColorPicker *cp;
	
	cp = (GnomeColorPicker *) gnome_color_picker_new ();
	gnome_color_picker_set_i16 (cp, fc->r, fc->g, fc->b, fc->a);
	gtk_widget_show ((GtkWidget *) cp);
	g_signal_connect (cp, "color_set", G_CALLBACK (set_colour), fe);
	
	return (GtkWidget *) cp;
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
	char *str;
	
	str = g_strdup_printf ("#%02x%02x%02x", (fc->r >> 8) & 0xff, (fc->g >> 8) & 0xff, (fc->b >> 8) & 0xff);
	e_sexp_encode_string (out, str);
	g_free (str);
}
