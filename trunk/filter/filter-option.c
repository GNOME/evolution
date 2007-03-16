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

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>

#include "filter-option.h"
#include "filter-part.h"
#include <libedataserver/e-sexp.h>

#define d(x)

static int option_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static FilterElement *clone (FilterElement *fe);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_option_class_init (FilterOptionClass *klass);
static void filter_option_init (FilterOption *fo);
static void filter_option_finalise (GObject *obj);


static FilterElementClass *parent_class;


GType
filter_option_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterOptionClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_option_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterOption),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_option_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterOption", &info, 0);
	}
	
	return type;
}

static void
filter_option_class_init (FilterOptionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);
	
	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);
	
	object_class->finalize = filter_option_finalise;
	
	/* override methods */
	fe_class->eq = option_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->clone = clone;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_option_init (FilterOption *fo)
{
	fo->type = "option";
}

static void
free_option (struct _filter_option *o, void *data)
{
	g_free (o->title);
	g_free (o->value);
	g_free (o->code);
	g_free (o);
}

static void
filter_option_finalise (GObject *obj)
{
	FilterOption *fo = (FilterOption *) obj;
	
	g_list_foreach (fo->options, (GFunc)free_option, NULL);
	g_list_free (fo->options);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
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
	return (FilterOption *) g_object_new (FILTER_TYPE_OPTION, NULL, NULL);
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
struct _filter_option *
filter_option_add(FilterOption *fo, const char *value, const char *title, const char *code)
{
	struct _filter_option *op;
	
	g_assert(IS_FILTER_OPTION(fo));
	g_return_val_if_fail(find_option(fo, value) == NULL, NULL);
	
	op = g_malloc(sizeof(*op));
	op->title = g_strdup(title);
	op->value = g_strdup(value);
	op->code = g_strdup(code);
	
	fo->options = g_list_append(fo->options, op);
	if (fo->current == NULL)
		fo->current = op;

	return op;
}

static int
option_eq(FilterElement *fe, FilterElement *cm)
{
	FilterOption *fo = (FilterOption *)fe, *co = (FilterOption *)cm;
	
	return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& ((fo->current && co->current && strcmp(fo->current->value, co->current->value) == 0)
		    || (fo->current == NULL && co->current == NULL));
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	xmlNodePtr n, work;
	
	/* parent implementation */
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
	
	n = node->children;
	while (n) {
		if (!strcmp (n->name, "option")) {
			char *tmp, *value, *title = NULL, *code = NULL;
			
			value = xmlGetProp (n, "value");
			work = n->children;
			while (work) {
				if (!strcmp (work->name, "title")) {
					if (!title) {
						if (!(tmp = xmlNodeGetContent (work)))
							tmp = xmlStrdup ("");
						
						title = g_strdup (tmp);
						xmlFree (tmp);
					}
				} else if (!strcmp (work->name, "code")) {
					if (!code) {
						if (!(tmp = xmlNodeGetContent (work)))
							tmp = xmlStrdup ("");
						
						code = g_strdup (tmp);
						xmlFree (tmp);
					}
				}
				work = work->next;
			}
			
			filter_option_add (fo, value, title, code);
			xmlFree (value);
			g_free (title);
			g_free (code);
		} else if (n->type == XML_ELEMENT_NODE) {
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
	
	fo->current = g_object_get_data ((GObject *) widget, "option");
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
		g_object_set_data ((GObject *) item, "option", op);
		g_signal_connect (item, "activate", G_CALLBACK (option_changed), fe);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
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
		g_signal_emit_by_name (first, "activate", fe);
	
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
	struct _filter_option *op, *newop;
	
	d(printf ("cloning option\n"));
	
        new = FILTER_OPTION (FILTER_ELEMENT_CLASS (parent_class)->clone (fe));
	l = fo->options;
	while (l) {
		op = l->data;
		newop = filter_option_add (new, op->value, op->title, op->code);
		if (fo->current == op)
			new->current = newop;
		l = l->next;
	}
	
	d(printf ("cloning option code %p, current = %p\n", new, new->current));
	
	return (FilterElement *) new;
}
