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

#include "filter-element.h"
#include "filter-input.h"
#include "filter-option.h"
#include "filter-code.h"
#include "filter-colour.h"
#include "filter-datespec.h"
#include "filter-score.h"
#include "filter-folder.h"

static void xml_create(FilterElement *fe, xmlNodePtr node);
static FilterElement *clone(FilterElement *fe);

static void filter_element_class_init	(FilterElementClass *class);
static void filter_element_init	(FilterElement *gspaper);
static void filter_element_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterElement *)(x))->priv)
struct _FilterElementPrivate {
};

static GtkObjectClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_element_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterElement",
			sizeof(FilterElement),
			sizeof(FilterElementClass),
			(GtkClassInitFunc)filter_element_class_init,
			(GtkObjectInitFunc)filter_element_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_element_class_init (FilterElementClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gtk_object_get_type ());

	object_class->finalize = filter_element_finalise;

	/* override methods */
	class->xml_create = xml_create;
	class->clone = clone;

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_element_init (FilterElement *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
filter_element_finalise(GtkObject *obj)
{
	FilterElement *o = (FilterElement *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_element_new:
 *
 * Create a new FilterElement object.
 * 
 * Return value: A new #FilterElement object.
 **/
FilterElement *
filter_element_new(void)
{
	FilterElement *o = (FilterElement *)gtk_type_new(filter_element_get_type ());
	return o;
}

/**
 * filter_element_xml_create:
 * @fe: 
 * @node: 
 * 
 * Create a new filter element based on an xml definition of
 * that element.
 **/
void		filter_element_xml_create	(FilterElement *fe, xmlNodePtr node)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->xml_create(fe, node);
}

/**
 * filter_element_xml_encode:
 * @fe: 
 * 
 * Encode the values of a filter element into xml format.
 * 
 * Return value: 
 **/
xmlNodePtr	filter_element_xml_encode	(FilterElement *fe)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->xml_encode(fe);
}

/**
 * filter_element_xml_decode:
 * @fe: 
 * @node: 
 * 
 * Decode the values of a fitler element from xml format.
 * 
 * Return value: 
 **/
int		filter_element_xml_decode	(FilterElement *fe, xmlNodePtr node)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->xml_decode(fe, node);
}

/**
 * filter_element_clone:
 * @fe: 
 * 
 * Clones the FilterElement @fe.
 * 
 * Return value: 
 **/
FilterElement	*filter_element_clone		(FilterElement *fe)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->clone(fe);
}

/**
 * filter_element_get_widget:
 * @fe: 
 * @node: 
 * 
 * Create a widget to represent this element.
 * 
 * Return value: 
 **/
GtkWidget	*filter_element_get_widget	(FilterElement *fe)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->get_widget(fe);
}

/**
 * filter_element_build_code:
 * @fe: 
 * @out: 
 * @ff: 
 * 
 * Add the code representing this element to the output string @out.
 **/
void		filter_element_build_code	(FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->build_code(fe, out, ff);
}

/**
 * filter_element_format_sexp:
 * @fe: 
 * @out: 
 * 
 * Format the value(s) of this element in a method suitable for the context of
 * sexp where it is used.  Usually as space separated, double-quoted strings.
 **/
void		filter_element_format_sexp	(FilterElement *fe, GString *out)
{
	return ((FilterElementClass *)((GtkObject *)fe)->klass)->format_sexp(fe, out);
}

/**
 * filter_element_new_type_name:
 * @type: 
 * 
 * Create a new filter element based on its type name.
 * 
 * Return value: 
 **/
FilterElement	*filter_element_new_type_name	(const char *type)
{
	if (type == NULL)
		return NULL;

	if (!strcmp (type, "string")) {
		return (FilterElement *)filter_input_new ();
	} else if (!strcmp (type, "folder")) {
		return (FilterElement *)filter_folder_new ();
	} else if (!strcmp (type, "address")) {
		/* FIXME: temporary ... need real address type */
		return (FilterElement *)filter_input_new_type_name (type);
	} else if (!strcmp (type, "code")) {
		return (FilterElement *)filter_code_new ();
	} else if (!strcmp (type, "colour")) {
		return (FilterElement *)filter_colour_new ();
	} else if (!strcmp (type, "optionlist")) {
		return (FilterElement *)filter_option_new ();
	} else if (!strcmp (type, "datespec")) {
		return (FilterElement *)filter_datespec_new ();
	} else if (!strcmp (type, "score")) {
		return (FilterElement *)filter_score_new ();
	} else {
		g_warning("Unknown filter type '%s'", type);
		return 0;
	}
}

/* default implementations */
static void xml_create(FilterElement *fe, xmlNodePtr node)
{
	fe->name = xmlGetProp(node, "name");
}

static FilterElement *clone(FilterElement *fe)
{
	xmlNodePtr node;
	FilterElement *new;

	new = (FilterElement *)gtk_type_new( ((GtkObject *)fe)->klass->type );
	node = filter_element_xml_encode(fe);
	filter_element_xml_decode(new, node);
	xmlFreeNodeList(node);
	return new;
}


