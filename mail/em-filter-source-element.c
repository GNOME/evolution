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

#include <e-util/e-account-utils.h>
#include <filter/e-filter-part.h>

#define EM_FILTER_SOURCE_ELEMENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FILTER_SOURCE_ELEMENT, EMFilterSourceElementPrivate))

typedef struct _SourceInfo {
	gchar *account_name;
	gchar *name;
	gchar *address;
	gchar *uid;
} SourceInfo;

struct _EMFilterSourceElementPrivate {
	EMailBackend *backend;
	GList *sources;
	gchar *active_id;
};

G_DEFINE_TYPE (
	EMFilterSourceElement,
	em_filter_source_element,
	E_TYPE_FILTER_ELEMENT)

enum {
	PROP_0,
	PROP_BACKEND
};

static void
source_info_free (SourceInfo *info)
{
	g_free (info->account_name);
	g_free (info->name);
	g_free (info->address);
	g_free (info->uid);
	g_free (info);
}

static void
filter_source_element_source_changed (GtkComboBox *combo_box,
                                      EMFilterSourceElement *fs)
{
	const gchar *active_id;

	active_id = gtk_combo_box_get_active_id (combo_box);

	g_free (fs->priv->active_id);
	fs->priv->active_id = g_strdup (active_id);
}

static void
filter_source_element_add_source (EMFilterSourceElement *fs,
                                  const gchar *account_name,
                                  const gchar *name,
                                  const gchar *addr,
                                  const gchar *uid)
{
	SourceInfo *info;

	g_return_if_fail (EM_IS_FILTER_SOURCE_ELEMENT (fs));

	info = g_new0 (SourceInfo, 1);
	info->account_name = g_strdup (account_name);
	info->name = g_strdup (name);
	info->address = g_strdup (addr);
	info->uid = g_strdup (uid);

	fs->priv->sources = g_list_append (fs->priv->sources, info);
}

static void
filter_source_element_get_sources (EMFilterSourceElement *fs)
{
	EAccountList *accounts;
	const EAccount *account;
	EIterator *it;

	/* should this get the global object from mail? */
	accounts = e_get_account_list ();

	for (it = e_list_get_iterator ((EList *) accounts);
	     e_iterator_is_valid (it);
	     e_iterator_next (it)) {
		account = (const EAccount *) e_iterator_get (it);

		if (account->source == NULL)
			continue;

		filter_source_element_add_source (
			fs, account->name, account->id->name,
			account->id->address, account->uid);
	}

	g_object_unref (it);
}

static void
filter_source_element_set_backend (EMFilterSourceElement *element,
                                   EMailBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (element->priv->backend == NULL);

	element->priv->backend = g_object_ref (backend);
}

static void
filter_source_element_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			filter_source_element_set_backend (
				EM_FILTER_SOURCE_ELEMENT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_source_element_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (
				value,
				em_filter_source_element_get_backend (
				EM_FILTER_SOURCE_ELEMENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_source_element_dispose (GObject *object)
{
	EMFilterSourceElementPrivate *priv;

	priv = EM_FILTER_SOURCE_ELEMENT_GET_PRIVATE (object);

	if (priv->backend != NULL) {
		g_object_unref (priv->backend);
		priv->backend = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_filter_source_element_parent_class)->dispose (object);
}

static void
filter_source_element_finalize (GObject *object)
{
	EMFilterSourceElementPrivate *priv;

	priv = EM_FILTER_SOURCE_ELEMENT_GET_PRIVATE (object);

	g_list_foreach (priv->sources, (GFunc) source_info_free, NULL);
	g_list_free (priv->sources);
	g_free (priv->active_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_filter_source_element_parent_class)->finalize (object);
}

static gint
filter_source_element_eq (EFilterElement *fe,
                          EFilterElement *cm)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;
	EMFilterSourceElement *cs = (EMFilterSourceElement *) cm;

	return E_FILTER_ELEMENT_CLASS (em_filter_source_element_parent_class)->eq (fe, cm)
		&& g_strcmp0 (fs->priv->active_id, cs->priv->active_id) == 0;
}

static xmlNodePtr
filter_source_element_xml_encode (EFilterElement *fe)
{
	xmlNodePtr value;

	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;

	value = xmlNewNode (NULL, (const guchar *) "value");
	xmlSetProp (value, (const guchar *) "name", (guchar *)fe->name);
	xmlSetProp (value, (const guchar *) "type", (const guchar *) "uid");

	if (fs->priv->active_id != NULL)
		xmlNewTextChild (
			value, NULL, (const guchar *) "uid",
			(guchar *) fs->priv->active_id);

	return value;
}

static gint
filter_source_element_xml_decode (EFilterElement *fe,
                                  xmlNodePtr node)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;
	EMailBackend *backend;
	EMailSession *session;
	gchar *active_id = NULL;

	backend = em_filter_source_element_get_backend (fs);
	session = e_mail_backend_get_session (backend);

	node = node->children;
	while (node != NULL) {

		if (strcmp ((gchar *) node->name, "uid") == 0) {
			xmlChar *content;

			content = xmlNodeGetContent (node);
			active_id = g_strdup ((gchar *) content);
			xmlFree (content);

			break;
		}

		/* For backward-compatibility: We used to store
		 * sources by their URI string, which can change. */
		if (strcmp ((gchar *)node->name, "uri") == 0) {
			CamelService *service = NULL;
			xmlChar *content;
			CamelURL *url;

			content = xmlNodeGetContent (node);
			url = camel_url_new ((gchar *) content, NULL);
			xmlFree (content);

			if (url != NULL) {
				service = camel_session_get_service_by_url (
					CAMEL_SESSION (session),
					url, CAMEL_PROVIDER_STORE);
				camel_url_free (url);
			}

			if (service != NULL) {
				const gchar *uid;

				uid = camel_service_get_uid (service);
				active_id = g_strdup (uid);
			}

			break;
		}

		node = node->next;
	}

	if (active_id != NULL) {
		g_free (fs->priv->active_id);
		fs->priv->active_id = active_id;
	} else
		g_free (active_id);

	return 0;
}

static EFilterElement *
filter_source_element_clone (EFilterElement *fe)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;
	EMFilterSourceElement *cpy;
	EMailBackend *backend;
	GList *i;

	backend = em_filter_source_element_get_backend (fs);
	cpy = (EMFilterSourceElement *) em_filter_source_element_new (backend);
	((EFilterElement *) cpy)->name = (gchar *) xmlStrdup ((guchar *) fe->name);

	cpy->priv->active_id = g_strdup (fs->priv->active_id);

	for (i = fs->priv->sources; i != NULL; i = g_list_next (i)) {
		SourceInfo *info = (SourceInfo *) i->data;
		filter_source_element_add_source (
			cpy, info->account_name, info->name,
			info->address, info->uid);
	}

	return (EFilterElement *) cpy;
}

static GtkWidget *
filter_source_element_get_widget (EFilterElement *fe)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;
	GtkWidget *widget;
	GtkComboBox *combo_box;
	GList *i;

	if (fs->priv->sources == NULL)
		filter_source_element_get_sources (fs);

	widget = gtk_combo_box_text_new ();
	combo_box = GTK_COMBO_BOX (widget);

	for (i = fs->priv->sources; i != NULL; i = g_list_next (i)) {
		SourceInfo *info = (SourceInfo *) i->data;
		const gchar *display_name;
		const gchar *address;
		const gchar *name;
		const gchar *uid;
		gchar *label;

		uid = info->uid;
		display_name = info->account_name;

		name = info->name;
		address = info->address;

		if (g_strcmp0 (display_name, address) == 0)
			label = g_strdup_printf (
				"%s <%s>", name, address);
		else
			label = g_strdup_printf (
				"%s <%s> (%s)", name,
				address, display_name);

		gtk_combo_box_text_append (
			GTK_COMBO_BOX_TEXT (combo_box), uid, label);

		g_free (label);
	}

	if (fs->priv->active_id != NULL) {
		gtk_combo_box_set_active_id (combo_box, fs->priv->active_id);
	} else {
		const gchar *active_id;

		gtk_combo_box_set_active (combo_box, 0);
		active_id = gtk_combo_box_get_active_id (combo_box);

		g_free (fs->priv->active_id);
		fs->priv->active_id = g_strdup (active_id);
	}

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (filter_source_element_source_changed), fs);

	return widget;
}

static void
filter_source_element_build_code (EFilterElement *fe,
                                  GString *out,
                                  EFilterPart *ff)
{
	/* We are doing nothing on purpose. */
}

static void
filter_source_element_format_sexp (EFilterElement *fe,
                                   GString *out)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;

	camel_sexp_encode_string (out, fs->priv->active_id);
}

static void
em_filter_source_element_class_init (EMFilterSourceElementClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	g_type_class_add_private (class, sizeof (EMFilterSourceElementPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = filter_source_element_set_property;
	object_class->get_property = filter_source_element_get_property;
	object_class->dispose = filter_source_element_dispose;
	object_class->finalize = filter_source_element_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->eq = filter_source_element_eq;
	filter_element_class->xml_encode = filter_source_element_xml_encode;
	filter_element_class->xml_decode = filter_source_element_xml_decode;
	filter_element_class->clone = filter_source_element_clone;
	filter_element_class->get_widget = filter_source_element_get_widget;
	filter_element_class->build_code = filter_source_element_build_code;
	filter_element_class->format_sexp = filter_source_element_format_sexp;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			NULL,
			NULL,
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
em_filter_source_element_init (EMFilterSourceElement *element)
{
	element->priv = EM_FILTER_SOURCE_ELEMENT_GET_PRIVATE (element);
}

EFilterElement *
em_filter_source_element_new (EMailBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return g_object_new (
		EM_TYPE_FILTER_SOURCE_ELEMENT,
		"backend", backend, NULL);
}

EMailBackend *
em_filter_source_element_get_backend (EMFilterSourceElement *element)
{
	g_return_val_if_fail (EM_IS_FILTER_SOURCE_ELEMENT (element), NULL);

	return element->priv->backend;
}
