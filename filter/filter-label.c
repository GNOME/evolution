/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <notzed@ximian.com>
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
#include <gtk/gtkwidget.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-file-entry.h>
#include <gal/widgets/e-unicode.h>
#include <gal/util/e-unicode-i18n.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-conf/bonobo-config-database.h>

#include "filter-label.h"
#include "e-util/e-sexp.h"

#define d(x) 

static void xml_create (FilterElement *fe, xmlNodePtr node);

static void filter_label_class_init (FilterLabelClass *klass);
static void filter_label_init (FilterLabel *label);
static void filter_label_finalise (GObject *obj);


static FilterElementClass *parent_class;


GType
filter_label_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterLabelClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_label_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterLabel),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_label_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_OPTION, "FilterLabel", &info, 0);
	}
	
	return type;
}

static void
filter_label_class_init (FilterLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);
	
	parent_class = g_type_class_ref (FILTER_TYPE_OPTION);
	
	object_class->finalize = filter_label_finalise;
	
	/* override methods */
	fe_class->xml_create = xml_create;
}

static void
filter_label_init (FilterLabel *fl)
{
	((FilterOption *) fl)->type = "label";
}

static void
filter_label_finalise (GtkObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
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
	return (FilterLabel *) g_object_new (FILTER_TYPE_LABEL, NULL, NULL);
}

static struct {
	char *path;
	char *title;
	char *value;
} labels[] = {
	{ "/Mail/Labels/label_0", N_("Important"), "important" },
	{ "/Mail/Labels/label_1", N_("Work"), "work" },
	{ "/Mail/Labels/label_2", N_("Personal"), "personal" },
	{ "/Mail/Labels/label_3", N_("To Do"), "todo" },
	{ "/Mail/Labels/label_4", N_("Later"), "later" },
};

int
filter_label_count (void)
{
	return (sizeof (labels) / sizeof (labels[0]));
}

const char *
filter_label_label (int i)
{
	if (i < 0 || i >= sizeof (labels) / sizeof (labels[0]))
		return NULL;
	else
		return labels[i].value;
}

int
filter_label_index (const char *label)
{
	int i;
	
	for (i = 0; i < sizeof (labels) / sizeof (labels[0]); i++) {
		if (strcmp (labels[i].value, label) == 0)
			return i;
	}
	
	return -1;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *)fe;
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	int i;
	
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
	
	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL)
		db = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);
	
	for (i = 0; i < sizeof (labels) / sizeof (labels[0]); i++) {
		const char *title;
		char *btitle;
		
		if (db == CORBA_OBJECT_NIL
		    || (title = btitle = bonobo_config_get_string (db, labels[i].path, NULL)) == NULL) {
			btitle = NULL;
			title = U_(labels[i].title);
		}
		
		filter_option_add (fo, labels[i].value, title, NULL);
		g_free (btitle);
	}
}
