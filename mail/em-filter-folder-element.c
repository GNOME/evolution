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
#include "shell/e-shell.h"
#include "filter/e-filter-part.h"
#include "libedataserver/e-sexp.h"
#include "e-util/e-alert.h"

struct _EMFilterFolderElementPrivate {
	EMailSession *session;
	gchar *uri;
};

enum {
	PROP_0,
	PROP_SESSION
};

static gboolean validate (EFilterElement *fe, EAlert **alert);
static gint folder_eq (EFilterElement *fe, EFilterElement *cm);
static xmlNodePtr xml_encode (EFilterElement *fe);
static gint xml_decode (EFilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (EFilterElement *fe);
static void build_code (EFilterElement *fe, GString *out, EFilterPart *ff);
static void format_sexp (EFilterElement *, GString *);
static void emff_copy_value (EFilterElement *de, EFilterElement *se);

G_DEFINE_TYPE (
	EMFilterFolderElement,
	em_filter_folder_element,
	E_TYPE_FILTER_ELEMENT)

static void
filter_folder_element_set_session (EMFilterFolderElement *element,
                                   EMailSession *session)
{
	if (!session)
		session = e_mail_backend_get_session (
			E_MAIL_BACKEND (e_shell_get_backend_by_name (
			e_shell_get_default (), "mail")));

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (element->priv->session == NULL);

	element->priv->session = g_object_ref (session);
}

static void
filter_folder_element_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			filter_folder_element_set_session (
				EM_FILTER_FOLDER_ELEMENT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_folder_element_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_filter_folder_element_get_session (
				EM_FILTER_FOLDER_ELEMENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_folder_element_dispose (GObject *object)
{
	EMFilterFolderElementPrivate *priv;

	priv = EM_FILTER_FOLDER_ELEMENT (object)->priv;

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_filter_folder_element_parent_class)->dispose (object);
}

static void
filter_folder_element_finalize (GObject *object)
{
	EMFilterFolderElementPrivate *priv;

	priv = EM_FILTER_FOLDER_ELEMENT (object)->priv;

	g_free (priv->uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_filter_folder_element_parent_class)->finalize (object);
}

static void
em_filter_folder_element_class_init (EMFilterFolderElementClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	g_type_class_add_private (class, sizeof (EMFilterFolderElementPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = filter_folder_element_set_property;
	object_class->get_property = filter_folder_element_get_property;
	object_class->dispose = filter_folder_element_dispose;
	object_class->finalize = filter_folder_element_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->validate = validate;
	filter_element_class->eq = folder_eq;
	filter_element_class->xml_encode = xml_encode;
	filter_element_class->xml_decode = xml_decode;
	filter_element_class->get_widget = get_widget;
	filter_element_class->build_code = build_code;
	filter_element_class->format_sexp = format_sexp;
	filter_element_class->copy_value = emff_copy_value;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_filter_folder_element_init (EMFilterFolderElement *element)
{
	element->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		element, EM_TYPE_FILTER_FOLDER_ELEMENT,
		EMFilterFolderElementPrivate);
}

EFilterElement *
em_filter_folder_element_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_FILTER_FOLDER_ELEMENT,
		"session", session, NULL);
}

EMailSession *
em_filter_folder_element_get_session (EMFilterFolderElement *element)
{
	g_return_val_if_fail (EM_IS_FILTER_FOLDER_ELEMENT (element), NULL);

	return element->priv->session;
}

const gchar *
em_filter_folder_element_get_uri (EMFilterFolderElement *element)
{
	g_return_val_if_fail (EM_IS_FILTER_FOLDER_ELEMENT (element), NULL);

	return element->priv->uri;
}

void
em_filter_folder_element_set_uri (EMFilterFolderElement *element,
                                  const gchar *uri)
{
	g_return_if_fail (EM_IS_FILTER_FOLDER_ELEMENT (element));

	g_free (element->priv->uri);
	element->priv->uri = g_strdup (uri);
}

static gboolean
validate (EFilterElement *fe, EAlert **alert)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (ff->priv->uri && *ff->priv->uri) {
		return TRUE;
	} else {
		if (alert)
			*alert = e_alert_new ("mail:no-folder", NULL);

		return FALSE;
	}
}

static gint
folder_eq (EFilterElement *fe, EFilterElement *cm)
{
	return E_FILTER_ELEMENT_CLASS (
		em_filter_folder_element_parent_class)->eq (fe, cm) &&
		strcmp (((EMFilterFolderElement *) fe)->priv->uri,
		((EMFilterFolderElement *) cm)->priv->uri)== 0;
}

static xmlNodePtr
xml_encode (EFilterElement *fe)
{
	xmlNodePtr value, work;
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	value = xmlNewNode(NULL, (xmlChar *) "value");
	xmlSetProp(value, (xmlChar *) "name", (xmlChar *) fe->name);
	if (ff->store_camel_uri)
		xmlSetProp(value, (xmlChar *) "type", (xmlChar *) "folder-curi");
	else
		xmlSetProp(value, (xmlChar *) "type", (xmlChar *) "folder");

	work = xmlNewChild(value, NULL, (xmlChar *) "folder", NULL);
	xmlSetProp(work, (xmlChar *) "uri", (xmlChar *) ff->priv->uri);

	return value;
}

static gint
xml_decode (EFilterElement *fe, xmlNodePtr node)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;
	xmlNodePtr n;
	xmlChar *type;

	xmlFree (fe->name);
	fe->name = (gchar *) xmlGetProp(node, (xmlChar *) "name");

	type = xmlGetProp (node, (xmlChar *) "type");
	if (type) {
		ff->store_camel_uri = g_str_equal ((const gchar *) type, "folder-curi");
		xmlFree (type);
	} else {
		ff->store_camel_uri = FALSE;
	}

	n = node->children;
	while (n) {
		if (!strcmp((gchar *) n->name, "folder")) {
			gchar *uri;

			uri = (gchar *) xmlGetProp(n, (xmlChar *) "uri");
			g_free (ff->priv->uri);
			ff->priv->uri = g_strdup (uri);
			xmlFree (uri);
			break;
		}
		n = n->next;
	}

	return 0;
}

static void
folder_selected (EMFolderSelectionButton *button,
                 EMFilterFolderElement *ff)
{
	GtkWidget *toplevel;
	const gchar *uri;

	uri = em_folder_selection_button_get_selection (button);
	g_free (ff->priv->uri);

	if (ff->store_camel_uri)
		ff->priv->uri = g_strdup (uri);
	else
		ff->priv->uri = uri != NULL ? em_uri_from_camel (uri) : NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	gtk_window_present (GTK_WINDOW (toplevel));
}

static GtkWidget *
get_widget (EFilterElement *fe)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;
	EMailSession *session;
	GtkWidget *button;
	gchar *uri;

	session = em_filter_folder_element_get_session (ff);

	if (ff->store_camel_uri)
		uri = ff->priv->uri;
	else
		uri = em_uri_to_camel (ff->priv->uri);

	button = em_folder_selection_button_new (
		session, _("Select Folder"), NULL);

	em_folder_selection_button_set_selection (
		EM_FOLDER_SELECTION_BUTTON (button), uri);

	if (!ff->store_camel_uri)
		g_free (uri);

	gtk_widget_show (button);

	g_signal_connect (
		button, "selected",
		G_CALLBACK (folder_selected), ff);

	return button;
}

static void
build_code (EFilterElement *fe, GString *out, EFilterPart *ff)
{
	return;
}

static void
format_sexp (EFilterElement *fe, GString *out)
{
	EMFilterFolderElement *ff = (EMFilterFolderElement *) fe;

	e_sexp_encode_string (out, ff->priv->uri);
}

static void
emff_copy_value (EFilterElement *de, EFilterElement *se)
{
	if (EM_IS_FILTER_FOLDER_ELEMENT (se)) {
		((EMFilterFolderElement *) de)->store_camel_uri =
			((EMFilterFolderElement *) se)->store_camel_uri;
		em_filter_folder_element_set_uri (
			EM_FILTER_FOLDER_ELEMENT (de),
			EM_FILTER_FOLDER_ELEMENT (se)->priv->uri);
	} else {
		E_FILTER_ELEMENT_CLASS (
		em_filter_folder_element_parent_class)->copy_value (de, se);
	}
}
