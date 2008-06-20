/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
#include "e-util/e-util-labels.h"

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

static GStaticMutex cache_lock = G_STATIC_MUTEX_INIT;
static guint cache_notifier_id = 0;
static GSList *tracked_filters = NULL;
static GSList *labels_cache = NULL;
static GConfClient *gconf_client = NULL;

static void fill_cache (void);
static void clear_cache (void);
static void gconf_labels_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);

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

	g_static_mutex_lock (&cache_lock);

	if (!tracked_filters) {
		fill_cache ();

		gconf_client = gconf_client_get_default ();
		gconf_client_add_dir (gconf_client, E_UTIL_LABELS_GCONF_KEY, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
		cache_notifier_id = gconf_client_notify_add (gconf_client, E_UTIL_LABELS_GCONF_KEY, gconf_labels_changed, NULL, NULL, NULL);
	}

	tracked_filters = g_slist_prepend (tracked_filters, fl);

	g_static_mutex_unlock (&cache_lock);
}

static void
filter_label_finalise (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->finalize (obj);

	g_static_mutex_lock (&cache_lock);

	tracked_filters = g_slist_remove (tracked_filters, obj);

	if (!tracked_filters) {
		clear_cache ();

		if (cache_notifier_id)
			gconf_client_notify_remove (gconf_client, cache_notifier_id);

		cache_notifier_id = 0;
		g_object_unref (gconf_client);
		gconf_client = NULL;
	}

	g_static_mutex_unlock (&cache_lock);
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

/* ************************************************************************* */

/* should already hold the lock when calling this function */
static void
fill_cache (void)
{
	labels_cache = e_util_labels_parse (NULL);
}

/* should already hold the lock when calling this function */
static void
clear_cache (void)
{
	e_util_labels_free (labels_cache);
	labels_cache = NULL;
}

static void
fill_options (FilterOption *fo)
{
	GSList *l;

	g_static_mutex_lock (&cache_lock);

	for (l = labels_cache; l; l = l->next) {
		EUtilLabel *label = l->data;
		const char *tag;
		char *title;

		if (!label)
			continue;

		title = e_str_without_underscores (label->name);
		tag = label->tag;

		if (tag && strncmp (tag, "$Label", 6) == 0)
			tag += 6;

		filter_option_add (fo, tag, title, NULL);

		g_free (title);
	}

	g_static_mutex_unlock (&cache_lock);
}

static void
regen_label_options (FilterOption *fo)
{
	char *current;

	if (!fo)
		return;

	current = g_strdup (filter_option_get_current (fo));

	filter_option_remove_all (fo);
	fill_options (fo);

	if (current)
		filter_option_set_current (fo, current);

	g_free (current);
}

static void
gconf_labels_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	g_static_mutex_lock (&cache_lock);
	clear_cache ();
	fill_cache ();
	g_static_mutex_unlock (&cache_lock);

	g_slist_foreach (tracked_filters, (GFunc)regen_label_options, NULL);
}

/* ************************************************************************* */

int
filter_label_count (void)
{
	int res;

	g_static_mutex_lock (&cache_lock);

	res = g_slist_length (labels_cache);

	g_static_mutex_unlock (&cache_lock);
	
	return res;
}

const char *
filter_label_label (int i)
{
	const char *res = NULL;
	GSList *l;
	EUtilLabel *label;

	g_static_mutex_lock (&cache_lock);
	
	l = g_slist_nth (labels_cache,  i);

	if (l)
		label = l->data;
	else
		label = NULL;

	if (label && label->tag) {
		if (strncmp (label->tag, "$Label", 6) == 0)
			res = label->tag + 6;
		else
			res = label->tag;
	}

	g_static_mutex_unlock (&cache_lock);

	return res;
}

int
filter_label_index (const char *label)
{
	int i;
	GSList *l;

	g_static_mutex_lock (&cache_lock);

	for (i = 0, l = labels_cache; l; i++, l = l->next) {
		EUtilLabel *lbl = l->data;
		const char *tag = lbl->tag;

		if (tag && strncmp (tag, "$Label", 6) == 0)
			tag += 6;

		if (tag && strcmp (tag, label) == 0)
			break;
	}

	g_static_mutex_unlock (&cache_lock);

	if (l)
		return i;

	return -1;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);

	fill_options ((FilterOption *) fe);
}
