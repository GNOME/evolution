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

#include <gtk/gtk.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-file-entry.h>

#include "filter-label.h"
#include <libedataserver/e-sexp.h>

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
filter_label_finalise (GObject *obj)
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
	char *title;
	char *value;
} labels[] = {
	{ N_("Important"), "important" },
	{ N_("Work"),      "work"      },
	{ N_("Personal"),  "personal"  },
	{ N_("To Do"),     "todo"      },
	{ N_("Later"),     "later"     },
};

int filter_label_count (void)
{
	return sizeof (labels) / sizeof (labels[0]);
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
	FilterOption *fo = (FilterOption *) fe;
	GConfClient *gconf;
	GSList *list, *l;
	char *title, *p;
	int i = 0;
	
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
	
	gconf = gconf_client_get_default ();
	
	l = list = gconf_client_get_list (gconf, "/apps/evolution/mail/labels", GCONF_VALUE_STRING, NULL);
	while (l != NULL) {
		title = (char *) l->data;
		if ((p = strrchr (title, ':')))
			*p++ = '\0';
		
		filter_option_add (fo, i < 5 ? labels[i++].value : (p ? p : "#ffffff"), title, NULL);
		g_free (title);
		
		l = l->next;
	}
	g_slist_free (list);
	
	g_object_unref (gconf);
}
