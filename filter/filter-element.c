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

#include <string.h>
#include <stdlib.h>

#include "filter-element.h"

struct _element_type {
	char *name;

	FilterElementFunc create;
	void *data;
};

static gboolean validate (FilterElement *fe);
static int element_eq(FilterElement *fe, FilterElement *cm);
static void xml_create(FilterElement *fe, xmlNodePtr node);
static FilterElement *clone(FilterElement *fe);
static void copy_value(FilterElement *de, FilterElement *se);

static void filter_element_class_init (FilterElementClass *klass);
static void filter_element_init	(FilterElement *fe);
static void filter_element_finalise (GObject *obj);

static GObjectClass *parent_class = NULL;
static GHashTable *fe_table;

GType
filter_element_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterElementClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_element_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterElement),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_element_init,
		};
		fe_table = g_hash_table_new(g_str_hash, g_str_equal);
		type = g_type_register_static (G_TYPE_OBJECT, "FilterElement", &info, 0);
	}
	
	return type;
}

static void
filter_element_class_init (FilterElementClass *klass)
{
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	((GObjectClass *)klass)->finalize = filter_element_finalise;
	
	/* override methods */
	klass->validate = validate;
	klass->eq = element_eq;
	klass->xml_create = xml_create;
	klass->clone = clone;
	klass->copy_value = copy_value;
}

static void
filter_element_init (FilterElement *fe)
{
	;
}

static void
filter_element_finalise (GObject *obj)
{
	FilterElement *o = (FilterElement *)obj;
	
	xmlFree (o->name);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_element_new:
 *
 * Create a new FilterElement object.
 * 
 * Return value: A new #FilterElement object.
 **/
FilterElement *
filter_element_new (void)
{
	return (FilterElement *) g_object_new (FILTER_TYPE_ELEMENT, NULL, NULL);
}

gboolean
filter_element_validate (FilterElement *fe)
{
	return FILTER_ELEMENT_GET_CLASS (fe)->validate (fe);
}

int
filter_element_eq (FilterElement *fe, FilterElement *cm)
{
	FilterElementClass *klass;
	
	klass = FILTER_ELEMENT_GET_CLASS (fe);
	return (klass == FILTER_ELEMENT_GET_CLASS (cm)) && klass->eq (fe, cm);
}

/**
 * filter_element_xml_create:
 * @fe: filter element
 * @node: xml node
 * 
 * Create a new filter element based on an xml definition of
 * that element.
 **/
void
filter_element_xml_create (FilterElement *fe, xmlNodePtr node)
{
	FILTER_ELEMENT_GET_CLASS (fe)->xml_create (fe, node);
}

/**
 * filter_element_xml_encode:
 * @fe: filter element
 * 
 * Encode the values of a filter element into xml format.
 * 
 * Return value: 
 **/
xmlNodePtr
filter_element_xml_encode (FilterElement *fe)
{
	return FILTER_ELEMENT_GET_CLASS (fe)->xml_encode (fe);
}

/**
 * filter_element_xml_decode:
 * @fe: filter element
 * @node: xml node
 * 
 * Decode the values of a fitler element from xml format.
 * 
 * Return value: 
 **/
int
filter_element_xml_decode (FilterElement *fe, xmlNodePtr node)
{
	return FILTER_ELEMENT_GET_CLASS (fe)->xml_decode (fe, node);
}

/**
 * filter_element_clone:
 * @fe: filter element
 * 
 * Clones the FilterElement @fe.
 * 
 * Return value: 
 **/
FilterElement *
filter_element_clone (FilterElement *fe)
{
	return FILTER_ELEMENT_GET_CLASS (fe)->clone (fe);
}

/**
 * filter_element_get_widget:
 * @fe: filter element
 * @node: xml node
 * 
 * Create a widget to represent this element.
 * 
 * Return value: 
 **/
GtkWidget *
filter_element_get_widget (FilterElement *fe)
{
	return FILTER_ELEMENT_GET_CLASS (fe)->get_widget (fe);
}

/**
 * filter_element_build_code:
 * @fe: filter element
 * @out: output buffer
 * @ff: 
 * 
 * Add the code representing this element to the output string @out.
 **/
void
filter_element_build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	FILTER_ELEMENT_GET_CLASS (fe)->build_code (fe, out, ff);
}

/**
 * filter_element_format_sexp:
 * @fe: filter element
 * @out: output buffer
 * 
 * Format the value(s) of this element in a method suitable for the context of
 * sexp where it is used.  Usually as space separated, double-quoted strings.
 **/
void
filter_element_format_sexp (FilterElement *fe, GString *out)
{
	FILTER_ELEMENT_GET_CLASS (fe)->format_sexp (fe, out);
}

void
filter_element_set_data (FilterElement *fe, gpointer data)
{
	fe->data = data;
}

/* default implementations */
static gboolean
validate (FilterElement *fe)
{
	return TRUE;
}

static int
element_eq (FilterElement *fe, FilterElement *cm)
{
	return ((fe->name && cm->name && strcmp (fe->name, cm->name) == 0)
		|| (fe->name == NULL && cm->name == NULL));
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	fe->name = xmlGetProp (node, "name");
}

static FilterElement *
clone (FilterElement *fe)
{
	xmlNodePtr node;
	FilterElement *new;
	
	new = (FilterElement *) g_object_new (G_OBJECT_TYPE (fe), NULL, NULL);
	node = filter_element_xml_encode (fe);
	filter_element_xml_decode (new, node);
	xmlFreeNodeList (node);
	
	return new;
}

/* This is somewhat hackish, implement all the base cases in here */
#include "filter-input.h"
#include "filter-option.h"
#include "filter-code.h"
#include "filter-colour.h"
#include "filter-datespec.h"
#include "filter-int.h"
#include "filter-file.h"
#include "filter-label.h"

static void
copy_value(FilterElement *de, FilterElement *se)
{
	if (IS_FILTER_INPUT(se)) {
		if (IS_FILTER_INPUT(de)) {
			if (((FilterInput *)se)->values)
				filter_input_set_value((FilterInput*)de, ((FilterInput *)se)->values->data);
		} else if (IS_FILTER_INT(de)) {
			((FilterInt *)de)->val = atoi((char *) ((FilterInput *)se)->values->data);
		}
	} else if (IS_FILTER_COLOUR(se)) {
		if (IS_FILTER_COLOUR(de)) {
			FilterColour *s = (FilterColour *)se, *d = (FilterColour *)de;

			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			d->a = s->a;
		}
	} else if (IS_FILTER_DATESPEC(se)) {
		if (IS_FILTER_DATESPEC(de)) {
			FilterDatespec *s = (FilterDatespec *)se, *d = (FilterDatespec *)de;

			d->type = s->type;
			d->value = s->value;
		}
	} else if (IS_FILTER_INT(se)) {
		if (IS_FILTER_INT(de)) {
			FilterInt *s = (FilterInt *)se, *d = (FilterInt *)de;

			d->val = s->val;
		} else if (IS_FILTER_INPUT(de)) {
			FilterInt *s = (FilterInt *)se;
			FilterInput *d = (FilterInput *)de;
			char *v;

			v = g_strdup_printf("%d", s->val);
			filter_input_set_value(d, v);
			g_free(v);
		}
	} else if (IS_FILTER_OPTION(se)) {
		if (IS_FILTER_OPTION(de)) {
			FilterOption *s = (FilterOption *)se, *d = (FilterOption *)de;

			if (s->current)
				filter_option_set_current(d, s->current->value);
		}
	}
}

/* only copies the value, not the name/type */
void
filter_element_copy_value (FilterElement *de, FilterElement *se)
{
	FILTER_ELEMENT_GET_CLASS (de)->copy_value(de, se);
}
