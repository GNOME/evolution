/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jon Trowbridge <trow@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2002 Ximian, Inc. (www.ximian.com)
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

#include "filter-source.h"

#include <gtk/gtk.h>
#include <e-util/e-url.h>
#include <e-util/e-sexp.h>
#include <e-util/e-account-list.h>
#include <camel/camel-url.h>


static void filter_source_class_init (FilterSourceClass *klass);
static void filter_source_init (FilterSource *fs);
static void filter_source_finalize (GObject *obj);

static int source_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static FilterElement *clone (FilterElement *fe);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_source_add_source  (FilterSource *fs, const char *account_name, const char *name,
				       const char *addr, const char *url);
static void filter_source_get_sources (FilterSource *fs);

typedef struct _SourceInfo {
	char *account_name;
	char *name;
	char *address;
	char *url;
} SourceInfo;

struct _FilterSourcePrivate {
	GList *sources;
	char *current_url;
};


static FilterElementClass *parent_class = NULL;


GType
filter_source_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterSourceClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_source_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterSource),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_source_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterSource", &info, 0);
	}
	
	return type;
}

static void
filter_source_class_init (FilterSourceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);
	
	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);
	
	object_class->finalize = filter_source_finalize;
	
	/* override methods */
	fe_class->eq = source_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->clone = clone;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_source_init (FilterSource *fs)
{
	fs->priv = g_new (struct _FilterSourcePrivate, 1);
	fs->priv->sources = NULL;
	fs->priv->current_url = NULL;
}

static void
filter_source_finalize (GObject *obj)
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
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

FilterSource *
filter_source_new (void)
{
	return (FilterSource *) g_object_new (FILTER_TYPE_SOURCE, NULL, NULL);
}

static int
source_eq (FilterElement *fe, FilterElement *cm)
{
	FilterSource *fs = (FilterSource *)fe, *cs = (FilterSource *)cm;
	
	return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& ((fs->priv->current_url && cs->priv->current_url
		     && strcmp (fs->priv->current_url, cs->priv->current_url) == 0)
		    || (fs->priv->current_url == NULL && cs->priv->current_url == NULL));
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* Call parent implementation */
	FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
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
	
	node = node->children;
	while (node != NULL) {
		if (!strcmp (node->name, "uri")) {
			uri = xmlNodeGetContent (node);
			url = camel_url_new (uri, NULL);
			xmlFree (uri);
			
			g_free (fs->priv->current_url);
			fs->priv->current_url = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
			camel_url_free (url);
			break;
		}
		
		node = node->next;
	}
	
	return 0;
}

static FilterElement *
clone (FilterElement *fe)
{
	FilterSource *fs = (FilterSource *) fe;
	FilterSource *cpy = filter_source_new ();
	GList *i;
	
	((FilterElement *) cpy)->name = xmlStrdup (fe->name);
	
	cpy->priv->current_url = g_strdup (fs->priv->current_url);
	
	for (i = fs->priv->sources; i != NULL; i = g_list_next (i)) {
		SourceInfo *info = (SourceInfo *) i->data;
		filter_source_add_source (cpy, info->account_name, info->name, info->address, info->url);
	}
	
	return (FilterElement *) cpy;
}

static void
source_changed (GtkWidget *item, FilterSource *fs)
{
	SourceInfo *info = (SourceInfo *) g_object_get_data ((GObject *) item, "source");
	
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
		char *label;
		
		if (info->url != NULL) {
			if (first == NULL)
				first = info;
			
			if (info->account_name && strcmp (info->account_name, info->address))
				label = g_strdup_printf ("%s <%s> (%s)", info->name,
							 info->address, info->account_name);
			else
				label = g_strdup_printf ("%s <%s>", info->name, info->address);
			
			item = gtk_menu_item_new_with_label (label);
			g_free (label);
			
			g_object_set_data ((GObject *) item, "source", info);
			g_signal_connect (item, "activate", G_CALLBACK (source_changed), fs);
			
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
			
			if (fs->priv->current_url && !strcmp (info->url, fs->priv->current_url))
				current_index = index;
			
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
	
	g_return_if_fail (IS_FILTER_SOURCE (fs));
	
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
	EAccountList *accounts;
	const EAccount *account;
	GConfClient *gconf;
	EIterator *it;
	char *uri;
	CamelURL *url;
	
	/* should this get the global object from mail? */
	gconf = gconf_client_get_default ();
	accounts = e_account_list_new (gconf);
	g_object_unref (gconf);
	
	for (it = e_list_get_iterator((EList *)accounts);
	     e_iterator_is_valid(it);
	     e_iterator_next(it)) {
		account = (const EAccount *)e_iterator_get(it);

		if (account->source == NULL || account->source->url == NULL)
			continue;

		/* hide secret stuff */
		url = camel_url_new (account->source->url, NULL);
		uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);
		camel_url_free (url);

		filter_source_add_source (fs, account->name, account->id->name, account->id->address, uri);
		g_free(uri);
	}
	g_object_unref(it);
	g_object_unref(accounts);
}
