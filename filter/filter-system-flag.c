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

#include <gal/widgets/e-unicode.h>

#include "filter-system-flag.h"
#include "e-util/e-sexp.h"

#define d(x)

static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_system_flag_class_init (FilterSystemFlagClass *class);
static void filter_system_flag_init       (FilterSystemFlag *gspaper);
static void filter_system_flag_finalise   (GtkObject *obj);

#define _PRIVATE(x) (((FilterSystemFlag *)(x))->priv)

struct _FilterSystemFlagPrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _system_flag {
	char *title;
	char *value;
} system_flags[] = {
	{ _("Replied to"), "Answered" },
	/*{ _("Deleted"), "Deleted" },*/
	/*{ _("Draft"), "Draft" },*/
	{ _("Important"), "Flagged" },
	{ _("Read"), "Seen" },
	{ NULL, NULL }
};

static struct _system_flag *
find_option (const char *value)
{
	struct _system_flag *flag;
	
	for (flag = system_flags; flag->title; flag++) {
		if (!g_strcasecmp (value, flag->value))
			return flag;
	}
	
	return NULL;
}

GtkType
filter_system_flag_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterSystemFlag",
			sizeof (FilterSystemFlag),
			sizeof (FilterSystemFlagClass),
			(GtkClassInitFunc) filter_system_flag_class_init,
			(GtkObjectInitFunc) filter_system_flag_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_system_flag_class_init (FilterSystemFlagClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_element_get_type ());
	
	object_class->finalize = filter_system_flag_finalise;
	
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
filter_system_flag_init (FilterSystemFlag *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_system_flag_finalise (GtkObject *obj)
{
	FilterSystemFlag *o = (FilterSystemFlag *)obj;
	
	o = o;
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

/**
 * filter_system_flag_new:
 *
 * Create a new FilterSystemFlag object.
 * 
 * Return value: A new #FilterSystemFlag object.
 **/
FilterSystemFlag *
filter_system_flag_new (void)
{
	FilterSystemFlag *o = (FilterSystemFlag *)gtk_type_new (filter_system_flag_get_type ());
	return o;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create (fe, node);	
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	FilterSystemFlag *fsf = (FilterSystemFlag *) fe;
	xmlNodePtr value;
	
	d(printf ("Encoding system-flag as xml\n"));
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "system-flag");
	xmlSetProp (value, "value", fsf->value);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterSystemFlag *fsf = (FilterSystemFlag *) fe;
	struct _system_flag *flag;
	char *value;
	
	fe->name = xmlGetProp (node, "name");
	
	value = xmlGetProp (node, "value");
	if (value) {
		flag = find_option (value);
		fsf->value = flag ? flag->value : NULL;
		xmlFree (value);
	} else {
		fsf->value = NULL;
	}
	
	return 0;
}

static void
item_selected (GtkWidget *widget, FilterElement *fe)
{
	FilterSystemFlag *fsf = (FilterSystemFlag *) fe;
	struct _system_flag *flag;
	
	flag = gtk_object_get_data (GTK_OBJECT (widget), "flag");
	
	fsf->value = flag->value;
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterSystemFlag *fsf = (FilterSystemFlag *) fe;
	GtkWidget *omenu, *menu, *item, *first = NULL;
	struct _system_flag *flag;
	int index = 0, current = 0;
	
	menu = gtk_menu_new ();
	for (flag = system_flags; flag->title; flag++) {
		item = gtk_menu_item_new_with_label (flag->title);
		gtk_object_set_data (GTK_OBJECT (item), "flag", flag);
		gtk_signal_connect (GTK_OBJECT (item), "activate", item_selected, fe);
		
		gtk_menu_append (GTK_MENU (menu), item);
		
		gtk_widget_show (item);
		
		if (fsf->value && !g_strcasecmp (fsf->value, flag->value)) {
			current = index;
			first = item;
		} else if (!first) {
			first = item;
		}
		
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
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterSystemFlag *fsf = (FilterSystemFlag *)fe;
	
	e_sexp_encode_string (out, fsf->value);
}
