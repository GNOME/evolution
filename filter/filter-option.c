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

#include "filter-option.h"
#include "filter-part.h"
#include "e-util/e-sexp.h"
#include "e-util/e-unicode.h"

#define d(x)

static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static FilterElement *clone(FilterElement *fe);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

static void filter_option_class_init	(FilterOptionClass *class);
static void filter_option_init	(FilterOption *gspaper);
static void filter_option_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterOption *)(x))->priv)

struct _FilterOptionPrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_option_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterOption",
			sizeof(FilterOption),
			sizeof(FilterOptionClass),
			(GtkClassInitFunc)filter_option_class_init,
			(GtkObjectInitFunc)filter_option_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_option_class_init (FilterOptionClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;

	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_element_get_type ());

	object_class->finalize = filter_option_finalise;

	/* override methods */
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->clone = clone;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_option_init (FilterOption *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
filter_option_finalise(GtkObject *obj)
{
	FilterOption *o = (FilterOption *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_option_new:
 *
 * Create a new FilterOption object.
 * 
 * Return value: A new #FilterOption object.
 **/
FilterOption *
filter_option_new(void)
{
	FilterOption *o = (FilterOption *)gtk_type_new(filter_option_get_type ());
	return o;
}

static struct _filter_option *
find_option(FilterOption *fo, const char *name)
{
	GList *l = fo->options;
	struct _filter_option *op;

	while (l) {
		op = l->data;
		if (!strcmp(name, op->value)) {
			return op;
		}
		l = g_list_next(l);
	}
	return NULL;
}

void		filter_option_set_current(FilterOption *option, const char *name)
{
	g_assert(IS_FILTER_OPTION(option));

	option->current = find_option(option, name);
}

static void xml_create(FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	xmlNodePtr n, work;
	struct _filter_option *op;

	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);

	n = node->childs;
	while (n) {
		if (!strcmp(n->name, "option")) {
			op = g_malloc0(sizeof(*op));
			op->value = xmlGetProp(n, "value");
			work = n->childs;
			while (work) {
				if (!strcmp(work->name, "title")) {
					if (!op->title) {
						op->title = xmlNodeGetContent(work);
					}
				} else if (!strcmp(work->name, "code")) {
					if (!op->code) {
						op->code = xmlNodeGetContent(work);
					}
				}
				work = work->next;
			}
			d(printf("creating new option:\n title %s\n value %s\n code %s\n", op->title, op->value, op->code));
			fo->options = g_list_append(fo->options, op);
			if (fo->current == NULL)
				fo->current = op;
		} else {
			g_warning("Unknown xml node within optionlist: %s\n", n->name);
		}
		n = n->next;
	}
}

static xmlNodePtr xml_encode(FilterElement *fe)
{
	xmlNodePtr value;
	FilterOption *fo = (FilterOption *)fe;

	d(printf("Encoding option as xml\n"));
	value = xmlNewNode(NULL, "value");
	xmlSetProp(value, "name", fe->name);
	xmlSetProp(value, "type", "option");
	if (fo->current) {
		xmlSetProp(value, "value", fo->current->value);
	}
	return value;
}

static int xml_decode(FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	char *value;

	d(printf("Decoding option from xml\n"));
	fe->name = xmlGetProp(node, "name");
	value = xmlGetProp(node, "value");
	if (value) {
		fo->current = find_option(fo, value);
		xmlFree(value);
	} else {
		fo->current = NULL;
	}
	return 0;
}

static void option_activate(GtkMenuItem *item, FilterOption *fo)
{
	fo->current = gtk_object_get_data((GtkObject *)item, "option");
	d(printf("option changed to %s\n", fo->current->title));
}

static GtkWidget *get_widget(FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe;
	GtkMenu *menu;
	GtkOptionMenu *omenu;
	GtkMenuItem *item;
	GList *l = fo->options;
	struct _filter_option *op;
	int index = 0, current=0;
	gchar *s;

	menu = (GtkMenu *)gtk_menu_new();
	while (l) {
		op = l->data;
		s = e_utf8_to_gtk_string ((GtkWidget *) menu, op->title);
		item = (GtkMenuItem *)gtk_menu_item_new_with_label(s);
		g_free (s);
		gtk_object_set_data((GtkObject *)item, "option", op);
		gtk_signal_connect((GtkObject *)item, "activate", option_activate, fo);
		gtk_menu_append(menu, (GtkWidget *)item);
		gtk_widget_show((GtkWidget *)item);
		if (op == fo->current) {
			current = index;
		}
		l = g_list_next(l);
		index++;
	}

	omenu = (GtkOptionMenu *)gtk_option_menu_new();
	gtk_option_menu_set_menu(omenu, (GtkWidget *)menu);
	gtk_option_menu_set_history(omenu, current);

	return (GtkWidget *)omenu;
}

static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	FilterOption *fo = (FilterOption *)fe;

	d(printf("building option code %p, current = %p\n", fo, fo->current));

	if (fo->current) {
		filter_part_expand_code(ff, fo->current->code, out);
	}
}

static void format_sexp(FilterElement *fe, GString *out)
{
	FilterOption *fo = (FilterOption *)fe;

	if (fo->current) {
		e_sexp_encode_string(out, fo->current->value);
	}
}

static FilterElement *clone(FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe, *new;
	GList *l;
	struct _filter_option *fn, *op;

	d(printf("cloning option\n"));

        new = FILTER_OPTION(((FilterElementClass *)(parent_class))->clone(fe));
	l = fo->options;
	while (l) {
		op = l->data;
		fn = g_malloc(sizeof(*fn));
		d(printf("  option %s\n", op->title));
		fn->title = g_strdup(op->title);
		fn->value = g_strdup(op->value);
		fn->code = g_strdup(op->code);
		new->options = g_list_append(new->options, fn);
		l = g_list_next(l);

		if (new->current == NULL)
			new->current = fn;
	}

	d(printf("cloning option code %p, current = %p\n", new, new->current));

	return (FilterElement *)new;
}
