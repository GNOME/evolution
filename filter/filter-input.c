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

#include "filter-input.h"
#include "e-util/e-sexp.h"

#define d(x)

static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

static void filter_input_class_init	(FilterInputClass *class);
static void filter_input_init	(FilterInput *gspaper);
static void filter_input_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterInput *)(x))->priv)

struct _FilterInputPrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_input_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterInput",
			sizeof(FilterInput),
			sizeof(FilterInputClass),
			(GtkClassInitFunc)filter_input_class_init,
			(GtkObjectInitFunc)filter_input_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_input_class_init (FilterInputClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;

	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_element_get_type ());

	object_class->finalize = filter_input_finalise;

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
filter_input_init (FilterInput *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
filter_input_finalise(GtkObject *obj)
{
	FilterInput *o = (FilterInput *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_input_new:
 *
 * Create a new FilterInput object.
 * 
 * Return value: A new #FilterInput object.
 **/
FilterInput *
filter_input_new(void)
{
	FilterInput *o = (FilterInput *)gtk_type_new(filter_input_get_type ());
	return o;
}

FilterInput	*filter_input_new_type_name	(const char *type)
{
	FilterInput *o = filter_input_new();
	o->type = g_strdup(type);

	d(printf("new type %s = %p\n", type, o));
	return o;
}

static void xml_create(FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
	
}

static xmlNodePtr xml_encode(FilterElement *fe)
{
	xmlNodePtr value;
	GList *l;
	FilterInput *fi = (FilterInput *)fe;
	char *type;

	type = fi->type?fi->type:"string";

	d(printf("Encoding %s as xml\n", type));

	value = xmlNewNode(NULL, "value");
	xmlSetProp(value, "name", fe->name);
	xmlSetProp(value, "type", type);
	l = fi->values;
	while (l) {
                xmlNodePtr cur;
		char *str = l->data;

                cur = xmlNewChild(value, NULL, type, NULL);
		xmlNodeSetContent(cur, str);
                l = g_list_next(l);
	}
	return value;
}

static int xml_decode(FilterElement *fe, xmlNodePtr node)
{
	FilterInput *fi = (FilterInput *)fe;
	char *name, *str, *type;
	xmlNodePtr n;

	type = fi->type?fi->type:"string";

	d(printf("Decoding %s from xml %p\n", type, fe));

	name = xmlGetProp(node, "name");
	d(printf("Name = %s\n", name));
	fe->name = name;
	fi->type = xmlGetProp(node, "type");
	n = node->childs;
	while (n) {
		if (!strcmp(n->name, type)) {
			str = xmlNodeGetContent(n);
			d(printf("  '%s'\n", str));
			fi->values = g_list_append(fi->values, str);
		} else {
			g_warning("Unknown node type '%s' encountered decoding a %s\n", n->name, type);
		}
		n = n->next;
	}
	return 0;
}

static void entry_changed(GtkEntry *entry, FilterElement *fe)
{
	char *new;
	FilterInput *fi = (FilterInput *)fe;
	GList *l;

	new = gtk_entry_get_text(entry);

	/* NOTE: entry only supports a single value ... */
	l = fi->values;
	while (l) {
		g_free(l->data);
		l = g_list_next(l);
	}
	g_list_free(fi->values);

	fi->values = g_list_append(NULL, g_strdup(new));
}

static GtkWidget *get_widget(FilterElement *fe)
{
	GtkEntry *entry;
	FilterInput *fi = (FilterInput *)fe;

	entry = (GtkEntry *)gtk_entry_new();
	if (fi->values && fi->values->data) {
		gtk_entry_set_text(entry, fi->values->data);
	}
	gtk_signal_connect((GtkObject *)entry, "changed", entry_changed, fe);
	return (GtkWidget *)entry;
}

static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void format_sexp(FilterElement *fe, GString *out)
{
	GList *l;
	FilterInput *fi = (FilterInput *)fe;

	l = fi->values;
	while (l) {
		e_sexp_encode_string(out, l->data);
		l = g_list_next(l);
	}
}
