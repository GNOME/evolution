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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#include "filter-label.h"
#include <libedataserver/e-sexp.h>
#include "e-util/e-util.h"
#include "mail/mail-config.h"

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

int filter_label_count (void)
{
	GSList *labels = mail_config_get_labels ();

	return g_slist_length (labels);
}

const char *
filter_label_label (int i)
{
	GSList *labels = mail_config_get_labels ();

	if (i < 0 || i >= g_slist_length (labels))
		return NULL;
	else
		/* the return value is always without preceding "$Label" */
		return ((MailConfigLabel *) g_slist_nth (labels, i))->tag + 6;
}

int
filter_label_index (const char *label)
{
	int i;
	GSList *labels = mail_config_get_labels ();

	for (i = 0; labels; i++, labels = labels->next) {
		MailConfigLabel *lbl = labels->data;

		/* the return value is always without preceding "$Label" */
		if (lbl && lbl->tag && strcmp (lbl->tag + 6, label) == 0)
			return i;
	}

	return -1;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	FilterOption *fo = (FilterOption *) fe;
	GSList *list, *l;

        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);

	list = mail_config_get_labels ();

	for (l = list; l; l = l->next) {
		MailConfigLabel *label = l->data;
		char *title;

		title = e_str_without_underscores (label->name);

		filter_option_add (fo, label->tag + 6, title, NULL);

		g_free (title);
	}
}
