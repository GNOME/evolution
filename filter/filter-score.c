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
#include "filter-score.h"

#define d(x)

static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_score_class_init (FilterScoreClass *class);
static void filter_score_init (FilterScore *gspaper);
static void filter_score_finalise (GtkObject *obj);

#define _PRIVATE(x) (((FilterScore *)(x))->priv)

struct _FilterScorePrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_score_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterScore",
			sizeof (FilterScore),
			sizeof (FilterScoreClass),
			(GtkClassInitFunc) filter_score_class_init,
			(GtkObjectInitFunc) filter_score_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_score_class_init (FilterScoreClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class (filter_element_get_type ());
	
	object_class->finalize = filter_score_finalise;
	
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
filter_score_init (FilterScore *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_score_finalise(GtkObject *obj)
{
	FilterScore *o = (FilterScore *)obj;
	
	o = o;
	
        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_score_new:
 *
 * Create a new FilterScore object.
 * 
 * Return value: A new #FilterScore object.
 **/
FilterScore *
filter_score_new (void)
{
	FilterScore *o = (FilterScore *)gtk_type_new(filter_score_get_type ());
	return o;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/*FilterScore *fs = (FilterScore *)fe;*/
	
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	FilterScore *fs = (FilterScore *)fe;
	char *score;
	
	d(printf("Encoding score as xml\n"));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "score");
	
	score = g_strdup_printf ("%d", fs->score);
	xmlSetProp (value, "score", score);
	g_free (score);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterScore *fs = (FilterScore *)fe;
	char *name;
	char *score;
	
	d(printf("Decoding score from xml %p\n", fe));
	
	name = xmlGetProp (node, "name");
	d(printf ("Name = %s\n", name));
	fe->name = name;
	score = xmlGetProp (node, name);
	if (score)
		fs->score = atoi (score);
	else
		fs->score = 0;
	
	return 0;
}

static void
spin_changed (GtkWidget *spin, FilterElement *fe)
{
	FilterScore *fs = (FilterScore *)fe;
	
	fs->score = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	GtkWidget *spin;
	GtkObject *adjustment;
	FilterScore *fs = (FilterScore *)fe;
	
	adjustment = gtk_adjustment_new (0.0, -100.0, 100.0, 1.0, 1.0, 1.0);
	spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1.0, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
	
	if (fs->score) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), (gfloat) fs->score);
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
	FilterScore *fs = (FilterScore *)fe;
	char *score;
	
	score = g_strdup_printf ("%d", fs->score);
	e_sexp_encode_string (out, score);
	g_free (score);
}
