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

#include "filter-code.h"

static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

static void filter_code_class_init	(FilterCodeClass *class);
static void filter_code_init	(FilterCode *gspaper);
static void filter_code_finalise	(GtkObject *obj);

static FilterInputClass *parent_class;

guint
filter_code_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterCode",
			sizeof(FilterCode),
			sizeof(FilterCodeClass),
			(GtkClassInitFunc)filter_code_class_init,
			(GtkObjectInitFunc)filter_code_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_input_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_code_class_init (FilterCodeClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_input_get_type ());

	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;

	object_class->finalize = filter_code_finalise;
	/* override methods */

}

static void
filter_code_init (FilterCode *o)
{
	((FilterInput *)o)->type = g_strdup("code");
}

static void
filter_code_finalise(GtkObject *obj)
{
	FilterCode *o = (FilterCode *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_code_new:
 *
 * Create a new FilterCode object.
 * 
 * Return value: A new #FilterCode object.
 **/
FilterCode *
filter_code_new(void)
{
	FilterCode *o = (FilterCode *)gtk_type_new(filter_code_get_type ());
	return o;
}

/* here, the string IS the code */
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	GList *l;
	FilterInput *fi = (FilterInput *)fe;

	l = fi->values;
	while (l) {
		g_string_append(out, (char *)l->data);
		l = g_list_next(l);
	}
}

/* and we have no value */
static void format_sexp(FilterElement *fe, GString *out)
{
	return;
}
