/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#include <config.h>

#include <string.h>
#include <glib.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gal/widgets/e-unicode.h>

#include "filter-option.h"
#include "filter-part.h"
#include "e-util/e-sexp.h"

#define d(x)

static int option_eq(FilterElement *fe, FilterElement *cm);
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
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
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
	filter_element->eq = option_eq;
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
	o->type = "option";

	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
free_option(struct _filter_option *o, void *data)
{
	g_free(o->title);
	g_free(o->value);
	g_free(o->code);
	g_free(o);
}

static void
filter_option_finalise (GtkObject *obj)
{
	FilterOption *o = (FilterOption *)obj;

	g_list_foreach(o->options, (GFunc)free_option, NULL);
	g_list_free(o->options);
	
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
filter_option_new (void)
{
	FilterOption *o = (FilterOption *)gtk_type_new (filter_option_get_type ());
	return o;
}

static struct _filter_option *
find_option (FilterOption *fo, const char *name)
{
	GList *l = fo->options;
	struct _filter_option *op;
	
	while (l) {
		op = l->data;
		if (!strcmp (name, op->value)) {
			return op;
		}
		l = g_list_next (l);
	}
	
	return NULL;
}

void
filter_option_set_current (FilterOption *option, const char *name)
{
	g_assert(IS_FILTER_OPTION(option));
	
	option->current = find_option (option, name);
}

/* used by implementers to add additional options */
void
filter_option_add(FilterOption *fo, const char *value, const char *title, const char *code)
{
	struct _filter_option *op;

	g_assert(IS_FILTER_OPTION(fo));
	g_return_if_fail(find_option(fo, value) == NULL);

	op = g_malloc(sizeof(*op));
	op->title = g_strdup(title);
	op->value = g_strdup(value);
	op->code = g_strdup(code);

	fo->options = g_list_append(fo->options, op);
	if (fo->current == NULL)
		fo->current = op;
}

static int
option_eq(FilterElement *fe, FilterElement *cm)
{
	FilterOption *fo = (FilterOption *)fe, *co = (FilterOption *)cm;

	return ((FilterElementClass *)(parent_class))->eq(fe, cm)
		&& ((fo->current && co->current && strcmp(fo->current->value, co->current->value) == 0)
		    || (fo->current == NULL && co->current == NULL));
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	xmlNodePtr n, work;

	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
	
	n = node->childs;
	while (n) {
		if (!strcmp (n->name, "option")) {
			char *tmp, *value, *title = NULL, *code = NULL;

			value = xmlGetProp (n, "value");
			work = n->childs;
			while (work) {
				if (!strcmp (work->name, "title")) {
					if (!title) {
						tmp = xmlNodeGetContent(work);
						title = e_utf8_xml1_decode(tmp);
						if (tmp)
							xmlFree(tmp);
					}
				} else if (!strcmp (work->name, "code")) {
					if (!code) {
						tmp = xmlNodeGetContent(work);
						code = e_utf8_xml1_decode(tmp);
						if (tmp)
							xmlFree(tmp);
					}
				}
				work = work->next;
			}
			filter_option_add(fo, value, title, code);
			xmlFree(value);
			g_free(title);
			g_free(code);
		} else {
			g_warning ("Unknown xml node within optionlist: %s\n", n->name);
		}
		n = n->next;
	}
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	FilterOption *fo = (FilterOption *)fe;
	
	d(printf ("Encoding option as xml\n"));
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", fo->type);
	if (fo->current)
		xmlSetProp (value, "value", fo->current->value);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	char *value;
	
	d(printf ("Decoding option from xml\n"));
	xmlFree (fe->name);
	fe->name = xmlGetProp (node, "name");
	value = xmlGetProp (node, "value");
	if (value) {
		fo->current = find_option (fo, value);
		xmlFree (value);
	} else {
		fo->current = NULL;
	}
	return 0;
}

static void
option_changed (GtkWidget *widget, FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe;
	
	fo->current = gtk_object_get_data (GTK_OBJECT (widget), "option");
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe;
	GtkWidget *menu;
	GtkWidget *omenu;
	GtkWidget *item;
	GtkWidget *first = NULL;
	GList *l = fo->options;
	struct _filter_option *op;
	int index = 0, current = 0;
	
	menu = gtk_menu_new ();
	while (l) {
		op = l->data;
		item = gtk_menu_item_new_with_label (_(op->title));
		gtk_object_set_data (GTK_OBJECT (item), "option", op);
		gtk_signal_connect (GTK_OBJECT (item), "activate", option_changed, fe);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
		if (op == fo->current) {
			current = index;
			first = item;
		} else if (!first) {
			first = item;
		}
		
		l = g_list_next (l);
		index++;
	}
	
	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	
	if (first)
		gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", fe);
	
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), current);
	
	return omenu;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	FilterOption *fo = (FilterOption *)fe;
	
	d(printf ("building option code %p, current = %p\n", fo, fo->current));
	
	if (fo->current && fo->current->code)
		filter_part_expand_code (ff, fo->current->code, out);
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterOption *fo = (FilterOption *)fe;
	
	if (fo->current)
		e_sexp_encode_string (out, fo->current->value);
}

static FilterElement *
clone (FilterElement *fe)
{
	FilterOption *fo = (FilterOption *)fe, *new;
	GList *l;
	struct _filter_option *op;
	
	d(printf ("cloning option\n"));
	
        new = FILTER_OPTION (((FilterElementClass *)(parent_class))->clone(fe));
	l = fo->options;
	while (l) {
		op = l->data;
		filter_option_add(new, op->value, op->title, op->code);
		l = g_list_next (l);
	}
	
	d(printf ("cloning option code %p, current = %p\n", new, new->current));
	
	return (FilterElement *)new;
}
