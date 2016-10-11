/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <camel/camel.h>

#include "shell/e-shell.h"

#include "e-mail-account-store.h"
#include "e-mail-ui-session.h"

#include "em-filter-source-element.h"

#define EM_FILTER_SOURCE_ELEMENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FILTER_SOURCE_ELEMENT, EMFilterSourceElementPrivate))

struct _EMFilterSourceElementPrivate {
	EMailSession *session;
	gchar *active_id;
};

G_DEFINE_TYPE (
	EMFilterSourceElement,
	em_filter_source_element,
	E_TYPE_FILTER_ELEMENT)

enum {
	PROP_0,
	PROP_SESSION
};

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
filter_source_element_set_session (EMFilterSourceElement *element,
                                   EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (element->priv->session == NULL);

	element->priv->session = g_object_ref (session);
}

static void
filter_source_element_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			filter_source_element_set_session (
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
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_filter_source_element_get_session (
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

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_filter_source_element_parent_class)->dispose (object);
}

static void
filter_source_element_finalize (GObject *object)
{
	EMFilterSourceElementPrivate *priv;

	priv = EM_FILTER_SOURCE_ELEMENT_GET_PRIVATE (object);

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
	xmlSetProp (value, (const guchar *) "name", (guchar *) fe->name);
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
	EMailSession *session;
	gchar *active_id = NULL;

	session = em_filter_source_element_get_session (fs);

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
		if (strcmp ((gchar *) node->name, "uri") == 0) {
			CamelService *service = NULL;
			xmlChar *content;
			CamelURL *url;

			content = xmlNodeGetContent (node);
			url = camel_url_new ((gchar *) content, NULL);
			xmlFree (content);

			if (url != NULL) {
				service = camel_session_ref_service_by_url (
					CAMEL_SESSION (session),
					url, CAMEL_PROVIDER_STORE);
				camel_url_free (url);
			}

			if (service != NULL) {
				const gchar *uid;

				uid = camel_service_get_uid (service);
				active_id = g_strdup (uid);

				g_object_unref (service);
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
	EMailSession *session;

	session = em_filter_source_element_get_session (fs);
	cpy = (EMFilterSourceElement *) em_filter_source_element_new (session);
	((EFilterElement *) cpy)->name = (gchar *) xmlStrdup ((guchar *) fe->name);

	cpy->priv->active_id = g_strdup (fs->priv->active_id);

	return (EFilterElement *) cpy;
}

static void
filter_source_element_add_to_combo (GtkComboBox *combo_box,
                                    CamelService *service,
                                    ESourceRegistry *registry)
{
	ESource *source;
	ESourceMailIdentity *extension;
	const gchar *extension_name;
	const gchar *display_name;
	const gchar *address;
	const gchar *name;
	const gchar *uid;
	gchar *label;

	source = e_source_registry_ref_source (
		registry,
		camel_service_get_uid (service));
	if (!source)
		return;

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	if (e_source_has_extension (source, extension_name)) {
		ESource *identity_source;
		ESourceMailAccount *mail_account;

		mail_account = e_source_get_extension (source, extension_name);
		uid = e_source_mail_account_get_identity_uid (mail_account);

		if (!uid || !*uid) {
			g_object_unref (source);
			return;
		}

		identity_source = e_source_registry_ref_source (registry, uid);
		g_object_unref (source);
		source = identity_source;

		if (!source)
			return;
	}

	/* use UID of the service, because that's the one used in camel-filter-driver */
	uid = camel_service_get_uid (service);
	display_name = e_source_get_display_name (source);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return;
	}

	extension = e_source_get_extension (source, extension_name);

	name = e_source_mail_identity_get_name (extension);
	address = e_source_mail_identity_get_address (extension);

	if (name == NULL || address == NULL) {
		if (name == NULL && address == NULL)
			label = g_strdup (display_name);
		else
			label = g_strdup_printf ("%s (%s)", name ? name : address, display_name);

	} else if (g_strcmp0 (display_name, address) == 0)
		label = g_strdup_printf ("%s <%s>", name, address);
	else
		label = g_strdup_printf ("%s <%s> (%s)", name, address, display_name);

	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (combo_box), uid, label);

	g_free (label);
	g_object_unref (source);
}

static GtkWidget *
filter_source_element_get_widget (EFilterElement *fe)
{
	EMFilterSourceElement *fs = (EMFilterSourceElement *) fe;
	EMailSession *session;
	ESourceRegistry *registry;
	EMailAccountStore *account_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GtkComboBox *combo_box;

	widget = gtk_combo_box_text_new ();
	combo_box = GTK_COMBO_BOX (widget);

	session = em_filter_source_element_get_session (fs);
	registry = e_mail_session_get_registry (session);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	model = GTK_TREE_MODEL (account_store);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		CamelService *service;

		do {
			gboolean enabled = FALSE, builtin = TRUE;

			service = NULL;

			gtk_tree_model_get (
				model, &iter,
				E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service,
				E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, &enabled,
				E_MAIL_ACCOUNT_STORE_COLUMN_BUILTIN, &builtin,
				-1);

			if (CAMEL_IS_STORE (service) && enabled && !builtin)
				filter_source_element_add_to_combo (combo_box, service, registry);

			if (service)
				g_object_unref (service);
		} while (gtk_tree_model_iter_next (model, &iter));
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
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
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
em_filter_source_element_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_FILTER_SOURCE_ELEMENT,
		"session", session, NULL);
}

EMailSession *
em_filter_source_element_get_session (EMFilterSourceElement *element)
{
	g_return_val_if_fail (EM_IS_FILTER_SOURCE_ELEMENT (element), NULL);

	return element->priv->session;
}
