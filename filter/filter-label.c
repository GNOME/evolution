/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-file-entry.h>
#include <gal/widgets/e-unicode.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-conf/bonobo-config-database.h>

#include "filter-label.h"
#include "e-util/e-sexp.h"

#define d(x) 

static gboolean validate (FilterElement *fe);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *fe, GString *out);

static void filter_label_class_init (FilterLabelClass *klass);
static void filter_label_init (FilterLabel *label);
static void filter_label_finalise (GtkObject *obj);


static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


GtkType
filter_label_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterLabel",
			sizeof (FilterLabel),
			sizeof (FilterLabelClass),
			(GtkClassInitFunc) filter_label_class_init,
			(GtkObjectInitFunc) filter_label_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_label_class_init (FilterLabelClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	FilterElementClass *filter_element = (FilterElementClass *) klass;
	
	parent_class = gtk_type_class (filter_element_get_type ());
	
	object_class->finalize = filter_label_finalise;
	
	/* override methods */
	filter_element->validate = validate;
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
filter_label_init (FilterLabel *o)
{
	
}

static void
filter_label_finalise (GtkObject *obj)
{
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

/**
 * filter_label_new:
 *
 * Create a new FilterLabel object.
 * 
 * Return value: A new #FilterLabel object.
 **/
FilterLabel *
filter_label_new (void)
{
	return (FilterLabel *) gtk_type_new (filter_label_get_type ());
}


void
filter_label_set_label (FilterLabel *filter, int label)
{
	filter->label = label;
}

static gboolean
validate (FilterElement *fe)
{
	FilterLabel *label = (FilterLabel *) fe;
	GtkWidget *dialog;
	
	if (label->label < 0 || label->label > 4) {
		dialog = gnome_ok_dialog (_("You must specify a label name"));
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		return FALSE;
	}
	
	return TRUE;
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
	FilterLabel *label = (FilterLabel *) fe;
	xmlNodePtr value;
	char *encstr;
	
	d(printf ("Encoding label as xml\n"));
	
	encstr = g_strdup_printf ("%d", label->label);
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "label");
	xmlSetProp (value, "label", encstr);
	g_free (encstr);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterLabel *label = (FilterLabel *) fe;
	char *name, *str, *type;
	
	type = xmlGetProp (node, "type");
	if (strcmp (type, "label") != 0) {
		xmlFree (type);
		return -1;
	}
	
	xmlFree (type);
	
	d(printf("Decoding label from xml %p\n", fe));
	
	name = xmlGetProp (node, "name");
	xmlFree (fe->name);
	fe->name = name;
	
	str = xmlGetProp (node, "label");
	label->label = atoi (str);
	xmlFree (str);
	
	return 0;
}

static void
label_selected (GtkWidget *item, gpointer user_data)
{
	FilterLabel *label = user_data;
	
	label->label = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "label"));
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterLabel *label = (FilterLabel *) fe;
	GtkWidget *omenu, *menu, *item;
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *path, *num;
	int i;
	
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	
	/* sigh. This is a fucking nightmare... */
	
	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		
		/* I guess we'll have to return an empty menu? */
		gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
		return omenu;
	}
	
	CORBA_exception_free (&ev);
	
	path = g_strdup ("/Mail/Labels/label_#");
	num = path + strlen (path) - 1;
	
	for (i = 0; i < 5; i++) {
		char *utf8_label, *native_label;
		
		sprintf (num, "%d", i);
		utf8_label = bonobo_config_get_string (db, path, NULL);
		
		native_label = e_utf8_to_gtk_string (GTK_WIDGET (menu), utf8_label);
		g_free (utf8_label);
		
		item = gtk_menu_item_new_with_label (native_label);
		g_free (native_label);
		
		gtk_object_set_data (GTK_OBJECT (item), "label", GINT_TO_POINTER (i));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    label_selected, label);
		
		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);
	}
	
	g_free (path);
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), label->label);
	
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
	FilterLabel *label = (FilterLabel *) fe;
	char *str;
	
	str = g_strdup_printf ("%d", label->label);
	g_string_append (out, str);
	g_free (str);
}
