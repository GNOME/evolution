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

static void xml_create (FilterElement *fe, xmlNodePtr node);

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
		
		type = gtk_type_unique (filter_option_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_label_class_init (FilterLabelClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	FilterElementClass *filter_element = (FilterElementClass *) klass;
	
	parent_class = gtk_type_class (filter_option_get_type ());
	
	object_class->finalize = filter_label_finalise;
	
	/* override methods */
	filter_element->xml_create = xml_create;
	
	/* signals */
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_label_init (FilterLabel *o)
{
	((FilterOption *)o)->type = "label";
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

int filter_label_count(void)
{
	return sizeof(labels)/sizeof(labels[0]);
}

const char *filter_label_label(int i)
{
	if (i<0 || i >= sizeof(labels)/sizeof(labels[0]))
		return NULL;
	else
		return labels[i].value;
}

int filter_label_index(const char *label)
{
	int i;

	for (i=0;i<sizeof(labels)/sizeof(labels[0]);i++) {
		if (strcmp(labels[i].value, label) == 0)
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

        ((FilterElementClass *)(parent_class))->xml_create(fe, node);

	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL)
		db = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);

	for (i=0;i<sizeof(labels)/sizeof(labels[0]);i++) {
		char *title, *btitle;
		
		if (db == CORBA_OBJECT_NIL
		    || (title = btitle = bonobo_config_get_string(db, labels[i].path, NULL)) == NULL) {
			btitle = NULL;
			title = labels[i].title;
		}

		filter_option_add(fo, labels[i].value, title, NULL);
		g_free(btitle);
	}
}
