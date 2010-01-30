/*
 *
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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "em-filter-folder-element.h"
#include "mail/em-folder-selection-button.h"
#include "mail/em-utils.h"
#include "filter/e-filter-part.h"
#include "libedataserver/e-sexp.h"
#include "e-util/e-alert.h"

#define d(x)

static gboolean validate(EFilterElement *fe, EAlert **alert);
static gint folder_eq(EFilterElement *fe, EFilterElement *cm);
static void xml_create(EFilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(EFilterElement *fe);
static gint xml_decode(EFilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(EFilterElement *fe);
static void build_code(EFilterElement *fe, GString *out, EFilterPart *ff);
static void format_sexp(EFilterElement *, GString *);
static void emff_copy_value(EFilterElement *de, EFilterElement *se);

static void em_filter_folder_element_class_init(EMFilterFolderElementClass *class);
static void em_filter_folder_element_init(EMFilterFolderElement *ff);
static void em_filter_folder_element_finalise(GObject *obj);

static EFilterElementClass *parent_class = NULL;

GType
em_filter_folder_element_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMFilterFolderElementClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)em_filter_folder_element_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(EMFilterFolderElement),
			0,    /* n_preallocs */
			(GInstanceInitFunc)em_filter_folder_element_init,
		};

		type = g_type_register_static(E_TYPE_FILTER_ELEMENT, "EMFilterFolderElement", &info, 0);
	}

	return type;
}

static void
em_filter_folder_element_class_init(EMFilterFolderElementClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	EFilterElementClass *fe_class = E_FILTER_ELEMENT_CLASS(klass);

	parent_class = g_type_class_ref(E_TYPE_FILTER_ELEMENT);

	object_class->finalize = em_filter_folder_element_finalise;

	/* override methods */
	fe_class->validate = validate;
	fe_class->eq = folder_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
	fe_class->copy_value = emff_copy_value;
}

static void
em_filter_folder_element_init(EMFilterFolderElement *ff)
{
	;
}

static void
em_filter_folder_element_finalise(GObject *obj)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *)obj;

	g_free(ff->uri);

        G_OBJECT_CLASS(parent_class)->finalize(obj);
}

/**
 * em_filter_folder_element_new:
 *
 * Create a new EMFilterFolderElement object.
 *
 * Return value: A new #EMFilterFolderElement object.
 **/
EMFilterFolderElement *
em_filter_folder_element_new(void)
{
	return(EMFilterFolderElement *)g_object_new(em_filter_folder_element_get_type(), NULL, NULL);
}

void
em_filter_folder_element_set_value(EMFilterFolderElement *ff, const gchar *uri)
{
	g_free(ff->uri);
	ff->uri = g_strdup(uri);
}

static gboolean
validate(EFilterElement *fe, EAlert **alert)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *)fe;

	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (ff->uri && *ff->uri) {
		return TRUE;
	} else {
		if (alert)
			*alert = e_alert_new ("mail:no-folder", NULL);

		return FALSE;
	}
}

static gint
folder_eq(EFilterElement *fe, EFilterElement *cm)
{
        return E_FILTER_ELEMENT_CLASS(parent_class)->eq(fe, cm)
		&& strcmp(((EMFilterFolderElement *)fe)->uri, ((EMFilterFolderElement *)cm)->uri)== 0;
}

static void
xml_create(EFilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        E_FILTER_ELEMENT_CLASS(parent_class)->xml_create(fe, node);
}

static xmlNodePtr
xml_encode(EFilterElement *fe)
{
	xmlNodePtr value, work;
	EMFilterFolderElement *ff = (EMFilterFolderElement *)fe;

	d(printf("Encoding folder as xml\n"));

	value = xmlNewNode(NULL, (const guchar *)"value");
	xmlSetProp(value, (const guchar *)"name", (guchar *)fe->name);
	if (ff->store_camel_uri)
		xmlSetProp(value, (const guchar *)"type", (const guchar *)"folder-curi");
	else
		xmlSetProp(value, (const guchar *)"type", (const guchar *)"folder");

	work = xmlNewChild(value, NULL, (const guchar *)"folder", NULL);
	xmlSetProp(work, (const guchar *)"uri", (const guchar *)ff->uri);

	return value;
}

static gint
xml_decode(EFilterElement *fe, xmlNodePtr node)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *)fe;
	xmlNodePtr n;
	xmlChar *type;

	d(printf("Decoding folder from xml %p\n", fe));

	xmlFree(fe->name);
	fe->name = (gchar *)xmlGetProp(node, (const guchar *)"name");

	type = xmlGetProp (node, (const guchar *)"type");
	if (type) {
		ff->store_camel_uri = g_str_equal ((const gchar *)type, "folder-curi");
		xmlFree (type);
	} else {
		ff->store_camel_uri = FALSE;
	}

	n = node->children;
	while (n) {
		if (!strcmp((gchar *)n->name, "folder")) {
			gchar *uri;

			uri = (gchar *)xmlGetProp(n, (const guchar *)"uri");
			g_free(ff->uri);
			ff->uri = g_strdup(uri);
			xmlFree(uri);
			break;
		}
		n = n->next;
	}

	return 0;
}

static void
folder_selected(EMFolderSelectionButton *button, EMFilterFolderElement *ff)
{
	GtkWidget *toplevel;
	const gchar *uri;

	uri = em_folder_selection_button_get_selection(button);
	g_free(ff->uri);

	if (ff->store_camel_uri)
		ff->uri = g_strdup (uri);
	else
		ff->uri = uri != NULL ? em_uri_from_camel (uri) : NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	gtk_window_present (GTK_WINDOW (toplevel));
}

static GtkWidget *
get_widget(EFilterElement *fe)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *)fe;
	GtkWidget *button;
	gchar *uri;

	if (ff->store_camel_uri)
		uri = ff->uri;
	else
		uri = em_uri_to_camel (ff->uri);

	button = em_folder_selection_button_new (_("Select Folder"), NULL);

	em_folder_selection_button_set_selection(
		EM_FOLDER_SELECTION_BUTTON (button), uri);

	if (!ff->store_camel_uri)
		g_free(uri);

	gtk_widget_show(button);
	g_signal_connect(button, "selected", G_CALLBACK(folder_selected), ff);

	return button;
}

static void
build_code(EFilterElement *fe, GString *out, EFilterPart *ff)
{
	return;
}

static void
format_sexp(EFilterElement *fe, GString *out)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *)fe;

	e_sexp_encode_string(out, ff->uri);
}

static void
emff_copy_value(EFilterElement *de, EFilterElement *se)
{
	if (EM_IS_FILTER_FOLDER_ELEMENT(se)) {
		((EMFilterFolderElement *)de)->store_camel_uri = ((EMFilterFolderElement *)se)->store_camel_uri;
		em_filter_folder_element_set_value((EMFilterFolderElement *)de, ((EMFilterFolderElement *)se)->uri);
	} else
		parent_class->copy_value(de, se);
}
