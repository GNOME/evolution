/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Ripped off by Sam Creasey <sammy@oh.verio.com> from
 *  filter-score by Jeffrey Stedfast <fejj@helixcode.com>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkspinbutton.h>

#include "e-util/e-sexp.h"
#include "filter-int.h"

#define d(x)

static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_int_class_init (FilterIntClass *class);
static void filter_int_init (FilterInt *gspaper);
static void filter_int_finalise (GtkObject *obj);

#define _PRIVATE(x) (((FilterInt *)(x))->priv)

struct _FilterIntPrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_int_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterInt",
			sizeof (FilterInt),
			sizeof (FilterIntClass),
			(GtkClassInitFunc) filter_int_class_init,
			(GtkObjectInitFunc) filter_int_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_int_class_init (FilterIntClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class (filter_element_get_type ());
	
	object_class->finalize = filter_int_finalise;
	
	/* override methods */
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;
	
	/* signals */
	
	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_int_init (FilterInt *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_int_finalise(GtkObject *obj)
{
	FilterInt *o = (FilterInt *)obj;
	
	o = o;
	
        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_int_new:
 *
 * Create a new FilterInt object.
 * 
 * Return value: A new #FilterInt object.
 **/
FilterInt *
filter_int_new (void)
{
	FilterInt *o = (FilterInt *)gtk_type_new(filter_int_get_type ());
	return o;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	FilterInt *fs = (FilterInt *)fe;
	char *intval;
	
	d(printf("Encoding integer as xml\n"));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "integer");
	
	intval = g_strdup_printf ("%d", fs->val);
	xmlSetProp (value, "integer", intval);
	g_free (intval);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterInt *fs = (FilterInt *)fe;
	char *name;
	char *intval;
	
	d(printf("Decoding integer from xml %p\n", fe));
	
	name = xmlGetProp (node, "name");
	d(printf ("Name = %s\n", name));
	xmlFree (fe->name);
	fe->name = name;
	intval = xmlGetProp (node, "integer");
	if (intval) {
		fs->val = atoi (intval);
		xmlFree (intval);
	} else
		fs->val = 0;
	
	return 0;
}

static void
spin_changed (GtkWidget *spin, FilterElement *fe)
{
	FilterInt *fs = (FilterInt *)fe;
	
	fs->val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	GtkWidget *spin;
	GtkObject *adjustment;
	FilterInt *fs = (FilterInt *)fe;
	
	adjustment = gtk_adjustment_new (0.0, 0.0, (gfloat)G_MAXINT, 
					 1.0, 1.0, 1.0);
	spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 5.0, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
	
	if (fs->val) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), (gfloat) fs->val);
	}
	
	gtk_signal_connect (GTK_OBJECT (spin), "changed", spin_changed, fe);
	
	return spin;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterInt *fs = (FilterInt *)fe;
	char *str;
	
	str = g_strdup_printf ("%d", fs->val);
	g_string_append (out, str);
	g_free (str);
}
