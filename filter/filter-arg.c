/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  Abstract filter argument class.
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

#include "filter-arg.h"


static void filter_arg_class_init (FilterArgClass *class);
static void filter_arg_init       (FilterArg      *gspaper);

static GtkObjectClass *parent_class;

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_arg_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterArg",
			sizeof (FilterArg),
			sizeof (FilterArgClass),
			(GtkClassInitFunc) filter_arg_class_init,
			(GtkObjectInitFunc) filter_arg_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
write_html_nothing(FilterArg *arg, GtkHTML *html, GtkHTMLStreamHandle *stream)
{
	/* empty */
}

static void
write_text_nothing(FilterArg *arg, GString *string)
{
	/* empty */
}

static void
edit_values_nothing(FilterArg *arg)
{
	/* empty */
}

static void
free_value_nothing(FilterArg *arg, void *v)
{
	/* empty */
}

static gint
compare_pointers(gpointer a, gpointer b)
{
	return a==b;
}

static void
filter_arg_class_init (FilterArgClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gtk_object_get_type ());

	class->write_html = write_html_nothing;
	class->write_text = write_text_nothing;
	class->edit_values = edit_values_nothing;
	class->free_value = free_value_nothing;
	
	signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FilterArgClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_arg_init (FilterArg *arg)
{
	arg->values = NULL;
}

/**
 * filter_arg_new:
 *
 * Create a new FilterArg widget.
 * 
 * Return value: A new FilterArg widget.
 **/
FilterArg *
filter_arg_new (char *name)
{
	FilterArg *a = FILTER_ARG ( gtk_type_new (filter_arg_get_type ()));
	if (name)
		a->name = g_strdup(name);
	return a;
}

void
filter_arg_add(FilterArg *arg, void *v)
{
	g_return_if_fail(v != NULL);

	arg->values = g_list_append(arg->values, v);
	gtk_signal_emit(GTK_OBJECT(arg), signals[CHANGED]);
}

void
filter_arg_remove(FilterArg *arg, void *v)
{
	arg->values = g_list_remove(arg->values, v);
	((FilterArgClass *)(arg->object.klass))->free_value(arg, v);
	gtk_signal_emit(GTK_OBJECT(arg), signals[CHANGED]);
}

void
filter_arg_write_html(FilterArg *arg, GtkHTML *html, GtkHTMLStreamHandle *stream)
{
	((FilterArgClass *)(arg->object.klass))->write_html(arg, html, stream);
}
void
filter_arg_write_text(FilterArg *arg, GString *string)
{
	((FilterArgClass *)(arg->object.klass))->write_text(arg, string);
}
void
filter_arg_edit_values(FilterArg *arg)
{
	g_return_if_fail(arg != NULL);

	if (((FilterArgClass *)(arg->object.klass))->edit_values)
		((FilterArgClass *)(arg->object.klass))->edit_values(arg);
	else
		g_warning("No implementation of virtual method edit_values");
}

xmlNodePtr
filter_arg_values_get_xml(FilterArg *arg)
{
	return ((FilterArgClass *)(arg->object.klass))->values_get_xml(arg);
}
void
filter_arg_values_add_xml(FilterArg *arg, xmlNodePtr node)
{
	((FilterArgClass *)(arg->object.klass))->values_add_xml(arg, node);
}

/* returns the number of args in the arg list */
int
filter_arg_get_count(FilterArg *arg)
{
	int count=0;
	GList *l;

	for (l = arg->values;l;l=g_list_next(l))
		count++;
	return count;
}

void *
filter_arg_get_value(FilterArg *arg, int index)
{
	int count=0;
	GList *l;

	for (l = arg->values;l && count<index;l=g_list_next(l))
		count++;
	if (l)
		return l->data;
	return NULL;
}

char *
filter_arg_get_value_as_string(FilterArg *arg, int index)
{
	int count=0;
	GList *l;
	void *data;

	data = filter_arg_get_value(arg, index);
	if (data) {
		return ((FilterArgClass *)(arg->object.klass))->get_value_as_string(arg, data);
	} else {
		return "";
	}
}


