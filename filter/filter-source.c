/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * filter-source.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "filter-source.h"

#include <gtk/gtk.h>
#include <gnome.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-url.h>
#include <e-util/e-sexp.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <camel/camel-url.h>

typedef struct _SourceInfo SourceInfo;
struct _SourceInfo {
	char *account_name;
	char *name;
	char *address;
	char *url;
};

struct _FilterSourcePrivate {
	GList *sources;
	char *current_url;
};

static FilterElementClass *parent_class = NULL;
enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void filter_source_class_init (FilterSourceClass *);
static void filter_source_init (FilterSource *);
static void filter_source_finalize (GtkObject *);

static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static FilterElement *clone(FilterElement *fe);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

static void filter_source_add_source  (FilterSource *fs, const char *account_name, const char *name,
				       const char *addr, const char *url);
static void filter_source_get_sources (FilterSource *fs);


GtkType
filter_source_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterSource",
			sizeof(FilterSource),
			sizeof(FilterSourceClass),
			(GtkClassInitFunc)filter_source_class_init,
			(GtkObjectInitFunc)filter_source_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_source_class_init (FilterSourceClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_element_get_type ());

	object_class->finalize = filter_source_finalize;

	/* override methods */
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->clone = clone;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;
	
	/* signals */
	
	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_source_init (FilterSource *fs)
{
	fs->priv = g_new (struct _FilterSourcePrivate, 1);
	fs->priv->sources = NULL;
	fs->priv->current_url = NULL;
}

static void
filter_source_finalize (GtkObject *obj)
{
	FilterSource *fs = FILTER_SOURCE (obj);
	GList *i = fs->priv->sources;

	while (i) {
		SourceInfo *info = i->data;
		g_free (info->account_name);
		g_free (info->name);
		g_free (info->address);
		g_free (info->url);
		g_free (info);
		i = g_list_next (i);
	}

	g_list_free (fs->priv->sources);

	g_free (fs->priv);
	
	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		GTK_OBJECT_CLASS (parent_class)->finalize (obj);
}

FilterSource *
filter_source_new (void)
{
	FilterSource *s = (FilterSource *) gtk_type_new (filter_source_get_type ());
	return s;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* Call parent implementation */
	((FilterElementClass *)parent_class)->xml_create (fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value;
	
	FilterSource *fs = (FilterSource *) fe;
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "uri");
	
	if (fs->priv->current_url)
		xmlNewTextChild (value, NULL, "uri", fs->priv->current_url);
	
	return value;
}

static gint
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterSource *fs = (FilterSource *) fe;
	CamelURL *url;
	char *uri;
	
	node = node->childs;
	if (node && node->name && !strcmp (node->name, "uri")) {
		uri = xmlNodeGetContent (node);
		url = camel_url_new (uri, NULL);
		xmlFree (uri);
		
		g_free (fs->priv->current_url);
		fs->priv->current_url = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
	}

	return 0;
}

static FilterElement *
clone (FilterElement *fe)
{
	FilterSource *fs = (FilterSource *) fe;
	FilterSource *cpy = filter_source_new ();
	GList *i;

	((FilterElement *)cpy)->name = xmlStrdup (fe->name);

	cpy->priv->current_url = g_strdup (fs->priv->current_url);

	for (i = fs->priv->sources; i != NULL; i = g_list_next (i)) {
		SourceInfo *info = (SourceInfo *) i->data;
		filter_source_add_source (cpy, info->account_name, info->name, info->address, info->url);
	}

	return (FilterElement *) cpy;
}

static void
source_changed (GtkWidget *w, FilterSource *fs)
{
	SourceInfo *info = (SourceInfo *) gtk_object_get_data (GTK_OBJECT (w), "source");
	
	g_free (fs->priv->current_url);
	fs->priv->current_url = g_strdup (info->url);
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterSource *fs = (FilterSource *) fe;
	GtkWidget *menu;
	GtkWidget *omenu;
	GtkWidget *item;
	GList *i;
	SourceInfo *first = NULL;
	int index, current_index;
	
	if (fs->priv->sources == NULL)
		filter_source_get_sources (fs);
	
	menu = gtk_menu_new ();
	
	index = 0;
	current_index = -1;
	
	for (i = fs->priv->sources; i != NULL; i = g_list_next (i)) {
		SourceInfo *info = (SourceInfo *) i->data;
		char *label, *native_label;
		
		if (info->url != NULL) {
			if (first == NULL)
				first = info;
			
			if (info->account_name && strcmp (info->account_name, info->address))
				label = g_strdup_printf ("%s <%s> (%s)", info->name,
							 info->address, info->account_name);
			else
				label = g_strdup_printf ("%s <%s>", info->name, info->address);
			
			native_label = e_utf8_to_gtk_string (GTK_WIDGET (menu), label);
			item = gtk_menu_item_new_with_label (native_label);
			g_free (label);
			g_free (native_label);
			
			gtk_object_set_data (GTK_OBJECT (item), "source", info);
			gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC (source_changed), fs);
			gtk_menu_append (GTK_MENU (menu), item);
			gtk_widget_show (item);
			
			/* FIXME: don't use e_url_equal */
			if (fs->priv->current_url && e_url_equal (info->url, fs->priv->current_url)) {
				current_index = index;
			}
			
			index++;
		}
	}
	
	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	
	if (current_index >= 0) {
		gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), current_index);
	} else {
		gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), 0);
		g_free (fs->priv->current_url);
		
		if (first)
			fs->priv->current_url = g_strdup (first->url);
		else
			fs->priv->current_url = NULL;
	}
	
	return omenu;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	/* We are doing nothing on purpose. */
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterSource *fs = (FilterSource *) fe;
	
	e_sexp_encode_string (out, fs->priv->current_url);
}


static void
filter_source_add_source (FilterSource *fs, const char *account_name, const char *name,
			  const char *addr, const char *url)
{
	SourceInfo *info;
	
	g_return_if_fail (fs && IS_FILTER_SOURCE (fs));
	
	info = g_new0 (SourceInfo, 1);
	info->account_name = g_strdup (account_name);
	info->name = g_strdup (name);
	info->address = g_strdup (addr);
	info->url = g_strdup (url);
	
	fs->priv->sources = g_list_append (fs->priv->sources, info);
}

static void
filter_source_get_sources (FilterSource *fs)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	int i, len;
	
	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return;
	}
	
	CORBA_exception_free (&ev);
	
	len = bonobo_config_get_long_with_default (db, "/Mail/Accounts/num", 0, NULL);
	
	for (i = 0; i < len; ++i) {
		char *path, *account_name, *name, *addr, *uri;
		CamelURL *url;
		
		path = g_strdup_printf ("/Mail/Accounts/source_url_%d", i);
		uri = bonobo_config_get_string (db, path, NULL);
		g_free (path);
		
		if (uri == NULL || *uri == '\0') {
			g_free (uri);
			continue;
		}
		
		path = g_strdup_printf ("/Mail/Accounts/account_name_%d", i);
		account_name = bonobo_config_get_string (db, path, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_name_%d", i);
		name = bonobo_config_get_string (db, path, NULL);
		g_free (path);
		
		path = g_strdup_printf ("/Mail/Accounts/identity_address_%d", i);
		addr = bonobo_config_get_string (db, path, NULL);
		g_free (path);
		
		/* hide unwanted url params and stuff */
		url = camel_url_new (uri, NULL);
		g_free (uri);
		uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);
		
		filter_source_add_source (fs, account_name, name, addr, uri);
		
		g_free (account_name);
		g_free (name);
		g_free (addr);
		g_free (uri);
	}
}
