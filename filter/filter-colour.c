/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include <gnome.h>
#include <gnome-xml/xmlmemory.h>

#include "e-util/e-sexp.h"
#include "filter-colour.h"

#define d(x)

static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

static void filter_colour_class_init	(FilterColourClass *class);
static void filter_colour_init	(FilterColour *gspaper);
static void filter_colour_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterColour *)(x))->priv)

struct _FilterColourPrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_colour_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterColour",
			sizeof(FilterColour),
			sizeof(FilterColourClass),
			(GtkClassInitFunc)filter_colour_class_init,
			(GtkObjectInitFunc)filter_colour_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_colour_class_init (FilterColourClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_element_get_type ());

	object_class->finalize = filter_colour_finalise;

	/* override methods */
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_colour_init (FilterColour *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
filter_colour_finalise(GtkObject *obj)
{
	FilterColour *o = (FilterColour *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_colour_new:
 *
 * Create a new FilterColour object.
 * 
 * Return value: A new #FilterColour object.
 **/
FilterColour *
filter_colour_new(void)
{
	FilterColour *o = (FilterColour *)gtk_type_new(filter_colour_get_type ());
	return o;
}

static void xml_create(FilterElement *fe, xmlNodePtr node)
{
	/*FilterColour *fc = (FilterColour *)fe;*/

	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
}

static xmlNodePtr xml_encode(FilterElement *fe)
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
get_value(xmlNodePtr node, char *name)
{
	unsigned int ret;
	char *value;

	value = xmlGetProp(node, name);
	sscanf(value, "%04x", &ret);
	xmlFree(value);
	return ret;
}


static int xml_decode(FilterElement *fe, xmlNodePtr node)
{
	FilterColour *fc = (FilterColour *)fe;

	fe->name = xmlGetProp(node, "name");
	fc->r = get_value(node, "red");
	fc->g = get_value(node, "green");
	fc->b = get_value(node, "blue");
	fc->a = get_value(node, "alpha");

	return 0;
}

static void set_colour(GnomeColorPicker *cp, guint r, guint g, guint b, guint a, FilterColour *fc)
{
	fc->r = r;
	fc->g = g;
	fc->b = b;
	fc->a = a;
}

static GtkWidget *get_widget(FilterElement *fe)
{
	FilterColour *fc = (FilterColour *)fe;
	GnomeColorPicker *cp;

	cp = (GnomeColorPicker *)gnome_color_picker_new();
	gnome_color_picker_set_i16(cp, fc->r, fc->g, fc->b, fc->a);
	gtk_widget_show((GtkWidget *)cp);
	gtk_signal_connect((GtkObject *)cp, "color_set", set_colour, fe);
	return (GtkWidget *)cp;
}

static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void format_sexp(FilterElement *fe, GString *out)
{
	char *str;
	FilterColour *fc = (FilterColour *)fe;

	str =g_strdup_printf("rgb:%04x/%04x/%04x", fc->r, fc->g, fc->b);
	e_sexp_encode_string(out, str);
	g_free(str);
}
