/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#include <gtk/gtk.h>
#include <gnome.h>
#include <gnome-xml/xmlmemory.h>

#include "e-util/e-sexp.h"
#include "filter-url.h"

#define d(x)

static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_url_class_init (FilterUrlClass *class);
static void filter_url_init (FilterUrl *gspaper);
static void filter_url_finalise (GtkObject *obj);

#define _PRIVATE(x) (((FilterUrl *)(x))->priv)

struct _FilterUrlPrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_url_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterUrl",
			sizeof (FilterUrl),
			sizeof (FilterUrlClass),
			(GtkClassInitFunc) filter_url_class_init,
			(GtkObjectInitFunc) filter_url_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_url_class_init (FilterUrlClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (filter_element_get_type ());
	
	object_class->finalize = filter_url_finalise;
	
	/* override methods */
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;
	
	/* signals */
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_url_init (FilterUrl *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_url_finalise (GtkObject *obj)
{
	FilterUrl *o = (FilterUrl *)obj;
	
	o = o;
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

/**
 * filter_url_new:
 *
 * Create a new FilterUrl object.
 * 
 * Return value: A new #FilterUrl object.
 **/
FilterUrl *
filter_url_new (void)
{
	FilterUrl *o = (FilterUrl *) gtk_type_new (filter_url_get_type ());
	
	return o;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/*FilterUrl *fu = (FilterUrl *)fe;*/
	
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create (fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	FilterUrl *fu = (FilterUrl *)fe;
	
	d(printf ("Encoding url as xml\n"));
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "url");
	
	xmlSetProp (value, "url", fu->url);
	
	return value;
}

static gchar *
get_value (xmlNodePtr node, char *name)
{
	gchar *value;
	
	value = xmlGetProp (node, name);
	
	return value;
}


static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterUrl *fu = (FilterUrl *)fe;
	
	fe->name = xmlGetProp (node, "name");
	fu->url = get_value (node, "url");
	
	return 0;
}

static void
set_url (GtkWidget *entry, FilterUrl *fu)
{
	fu->url = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	GtkWidget *combo;
	GList *sources = NULL;  /* this needs to be a list of urls */
	
	combo = gtk_combo_new ();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo), sources);
	
	gtk_widget_show (combo);
	gtk_signal_connect (GTK_OBJECT (GTK_EDITABLE (GTK_COMBO (combo)->entry)), "changed", set_url, fe);
	
	return combo;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterUrl *fu = (FilterUrl *)fe;
	
	e_sexp_encode_string (out, fu->url);
}
