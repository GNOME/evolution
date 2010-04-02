/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-filter-source-element.h"

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <libedataserver/e-sexp.h>
#include <libedataserver/e-account-list.h>

#include "filter/e-filter-part.h"

static void em_filter_source_element_class_init(EMFilterSourceElementClass *klass);
static void em_filter_source_element_init(EMFilterSourceElement *fs);
static void em_filter_source_element_finalize(GObject *obj);

static gint source_eq(EFilterElement *fe, EFilterElement *cm);
static void xml_create(EFilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(EFilterElement *fe);
static gint xml_decode(EFilterElement *fe, xmlNodePtr node);
static EFilterElement *filter_clone(EFilterElement *fe);
static GtkWidget *get_widget(EFilterElement *fe);
static void build_code(EFilterElement *fe, GString *out, EFilterPart *ff);
static void format_sexp(EFilterElement *, GString *);

static void em_filter_source_element_add_source (EMFilterSourceElement *fs, const gchar *account_name, const gchar *name,
				       const gchar *addr, const gchar *url);
static void em_filter_source_element_get_sources(EMFilterSourceElement *fs);

typedef struct _SourceInfo {
	gchar *account_name;
	gchar *name;
	gchar *address;
	gchar *url;
} SourceInfo;

struct _EMFilterSourceElementPrivate {
	GList *sources;
	gchar *current_url;
};

static EFilterElementClass *parent_class = NULL;

GType
em_filter_source_element_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMFilterSourceElementClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)em_filter_source_element_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(EMFilterSourceElement),
			0,    /* n_preallocs */
			(GInstanceInitFunc)em_filter_source_element_init,
		};

		type = g_type_register_static(E_TYPE_FILTER_ELEMENT, "EMFilterSourceElement", &info, 0);
	}

	return type;
}

static void
em_filter_source_element_class_init(EMFilterSourceElementClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	EFilterElementClass *fe_class = E_FILTER_ELEMENT_CLASS(klass);

	parent_class = g_type_class_ref(E_TYPE_FILTER_ELEMENT);

	object_class->finalize = em_filter_source_element_finalize;

	/* override methods */
	fe_class->eq = source_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->clone = filter_clone;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
em_filter_source_element_init(EMFilterSourceElement *fs)
{
	fs->priv = g_new(struct _EMFilterSourceElementPrivate, 1);
	fs->priv->sources = NULL;
	fs->priv->current_url = NULL;
}

static void
em_filter_source_element_finalize(GObject *obj)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *)obj;
	GList *i = fs->priv->sources;

	while (i) {
		SourceInfo *info = i->data;
		g_free(info->account_name);
		g_free(info->name);
		g_free(info->address);
		g_free(info->url);
		g_free(info);
		i = g_list_next(i);
	}

	g_list_free(fs->priv->sources);
	g_free(fs->priv->current_url);

	g_free(fs->priv);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

EMFilterSourceElement *
em_filter_source_element_new(void)
{
	return (EMFilterSourceElement *)g_object_new(em_filter_source_element_get_type(), NULL, NULL);
}

static gint
source_eq(EFilterElement *fe, EFilterElement *cm)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *)fe, *cs = (EMFilterSourceElement *)cm;

	return E_FILTER_ELEMENT_CLASS(parent_class)->eq(fe, cm)
		&&((fs->priv->current_url && cs->priv->current_url
		     && strcmp(fs->priv->current_url, cs->priv->current_url)== 0)
		    ||(fs->priv->current_url == NULL && cs->priv->current_url == NULL));
}

static void
xml_create(EFilterElement *fe, xmlNodePtr node)
{
	/* Call parent implementation */
	E_FILTER_ELEMENT_CLASS(parent_class)->xml_create(fe, node);
}

static xmlNodePtr
xml_encode(EFilterElement *fe)
{
	xmlNodePtr value;

	EMFilterSourceElement *fs = (EMFilterSourceElement *)fe;

	value = xmlNewNode(NULL, (const guchar *)"value");
	xmlSetProp(value, (const guchar *)"name", (guchar *)fe->name);
	xmlSetProp(value, (const guchar *)"type", (const guchar *)"uri");

	if (fs->priv->current_url)
		xmlNewTextChild(value, NULL, (const guchar *)"uri", (guchar *)fs->priv->current_url);

	return value;
}

static gint
xml_decode(EFilterElement *fe, xmlNodePtr node)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *)fe;
	CamelURL *url;
	gchar *uri;

	node = node->children;
	while (node != NULL) {
		if (!strcmp((gchar *)node->name, "uri")) {
			uri = (gchar *)xmlNodeGetContent(node);
			url = camel_url_new(uri, NULL);
			xmlFree(uri);

			g_free(fs->priv->current_url);
			fs->priv->current_url = camel_url_to_string(url, CAMEL_URL_HIDE_ALL);
			camel_url_free(url);
			break;
		}

		node = node->next;
	}

	return 0;
}

static EFilterElement *
filter_clone(EFilterElement *fe)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *)fe;
	EMFilterSourceElement *cpy = em_filter_source_element_new();
	GList *i;

	((EFilterElement *)cpy)->name = (gchar *)xmlStrdup((guchar *)fe->name);

	cpy->priv->current_url = g_strdup(fs->priv->current_url);

	for (i = fs->priv->sources; i != NULL; i = g_list_next(i)) {
		SourceInfo *info = (SourceInfo *)i->data;
		em_filter_source_element_add_source(cpy, info->account_name, info->name, info->address, info->url);
	}

	return (EFilterElement *)cpy;
}

static void
source_changed(GtkComboBox *combobox, EMFilterSourceElement *fs)
{
	SourceInfo *info;
	gint idx;

	idx = gtk_combo_box_get_active (combobox);
	g_return_if_fail (idx >= 0 && idx < g_list_length (fs->priv->sources));

	info = (SourceInfo *) g_list_nth_data (fs->priv->sources, idx);
	g_return_if_fail (info != NULL);

	g_free (fs->priv->current_url);
	fs->priv->current_url = g_strdup (info->url);
}

static GtkWidget *
get_widget(EFilterElement *fe)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *)fe;
	GtkWidget *combobox;
	GList *i;
	SourceInfo *first = NULL;
	gint index, current_index;

	if (fs->priv->sources == NULL)
		em_filter_source_element_get_sources(fs);

	combobox = gtk_combo_box_new_text ();

	index = 0;
	current_index = -1;

	for (i = fs->priv->sources; i != NULL; i = g_list_next(i)) {
		SourceInfo *info = (SourceInfo *)i->data;
		gchar *label;

		if (info->url != NULL) {
			if (first == NULL)
				first = info;

			if (info->account_name && strcmp(info->account_name, info->address))
				label = g_strdup_printf("%s <%s>(%s)", info->name,
							 info->address, info->account_name);
			else
				label = g_strdup_printf("%s <%s>", info->name, info->address);

			gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), label);
			g_free(label);

			if (fs->priv->current_url && !strcmp(info->url, fs->priv->current_url))
				current_index = index;

			index++;
		}
	}

	if (current_index >= 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current_index);
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
		g_free(fs->priv->current_url);

		if (first)
			fs->priv->current_url = g_strdup(first->url);
		else
			fs->priv->current_url = NULL;
	}

	g_signal_connect (combobox, "changed", G_CALLBACK (source_changed), fs);

	return combobox;
}

static void
build_code(EFilterElement *fe, GString *out, EFilterPart *ff)
{
	/* We are doing nothing on purpose. */
}

static void
format_sexp(EFilterElement *fe, GString *out)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *)fe;

	e_sexp_encode_string(out, fs->priv->current_url);
}

static void
em_filter_source_element_add_source(EMFilterSourceElement *fs, const gchar *account_name, const gchar *name,
			  const gchar *addr, const gchar *url)
{
	SourceInfo *info;

	g_return_if_fail(EM_IS_FILTER_SOURCE_ELEMENT(fs));

	info = g_new0(SourceInfo, 1);
	info->account_name = g_strdup(account_name);
	info->name = g_strdup(name);
	info->address = g_strdup(addr);
	info->url = g_strdup(url);

	fs->priv->sources = g_list_append(fs->priv->sources, info);
}

static void
em_filter_source_element_get_sources(EMFilterSourceElement *fs)
{
	EAccountList *accounts;
	const EAccount *account;
	GConfClient *gconf;
	EIterator *it;
	gchar *uri;
	CamelURL *url;

	/* should this get the global object from mail? */
	gconf = gconf_client_get_default();
	accounts = e_account_list_new(gconf);
	g_object_unref(gconf);

	for (it = e_list_get_iterator((EList *)accounts);
	     e_iterator_is_valid(it);
	     e_iterator_next(it)) {
		account = (const EAccount *)e_iterator_get(it);

		if (account->source == NULL || account->source->url == NULL || account->source->url[0] == 0)
			continue;

		url = camel_url_new(account->source->url, NULL);
		if (url) {
			/* hide secret stuff */
			uri = camel_url_to_string(url, CAMEL_URL_HIDE_ALL);
			camel_url_free(url);
			em_filter_source_element_add_source(fs, account->name, account->id->name, account->id->address, uri);
			g_free(uri);
		}
	}
	g_object_unref(it);
	g_object_unref(accounts);
}
