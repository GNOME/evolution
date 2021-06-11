/*
 * Copyright (C) 2021 Red Hat (www.redhat.com)
 *
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
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "em-filter-mail-identity-element.h"

struct _EMFilterMailIdentityElementPrivate {
	ESourceRegistry *registry;
	gchar *display_name;
	gchar *identity_uid;
	gchar *alias_name;
	gchar *alias_address;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMFilterMailIdentityElement, em_filter_mail_identity_element, E_TYPE_FILTER_ELEMENT)

static void
filter_mail_identity_take_value (EMFilterMailIdentityElement *mail_identity,
				 gchar *display_name,
				 gchar *identity_uid,
				 gchar *alias_name,
				 gchar *alias_address)
{
	if (mail_identity->priv->display_name != display_name) {
		g_free (mail_identity->priv->display_name);
		mail_identity->priv->display_name = display_name;
	} else {
		g_free (display_name);
	}

	if (mail_identity->priv->identity_uid != identity_uid) {
		g_free (mail_identity->priv->identity_uid);
		mail_identity->priv->identity_uid = identity_uid;
	} else {
		g_free (identity_uid);
	}

	if (mail_identity->priv->alias_name != alias_name) {
		g_free (mail_identity->priv->alias_name);
		mail_identity->priv->alias_name = alias_name;
	} else {
		g_free (alias_name);
	}

	if (mail_identity->priv->alias_address != alias_address) {
		g_free (mail_identity->priv->alias_address);
		mail_identity->priv->alias_address = alias_address;
	} else {
		g_free (alias_address);
	}
}

static void
filter_mail_identity_element_finalize (GObject *object)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (object);

	g_clear_object (&mail_identity->priv->registry);
	g_free (mail_identity->priv->display_name);
	g_free (mail_identity->priv->identity_uid);
	g_free (mail_identity->priv->alias_name);
	g_free (mail_identity->priv->alias_address);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (em_filter_mail_identity_element_parent_class)->finalize (object);
}

static gint
filter_mail_identity_element_eq (EFilterElement *element_a,
				 EFilterElement *element_b)
{
	EMFilterMailIdentityElement *mail_identity_a = EM_FILTER_MAIL_IDENTITY_ELEMENT (element_a);
	EMFilterMailIdentityElement *mail_identity_b = EM_FILTER_MAIL_IDENTITY_ELEMENT (element_b);

	/* Chain up to parent's method. */
	if (!E_FILTER_ELEMENT_CLASS (em_filter_mail_identity_element_parent_class)->eq (element_a, element_b))
		return FALSE;

	return g_strcmp0 (mail_identity_a->priv->display_name, mail_identity_b->priv->display_name) == 0 &&
	       g_strcmp0 (mail_identity_a->priv->identity_uid, mail_identity_b->priv->identity_uid) == 0 &&
	       g_strcmp0 (mail_identity_a->priv->alias_name, mail_identity_b->priv->alias_name) == 0 &&
	       g_strcmp0 (mail_identity_a->priv->alias_address, mail_identity_b->priv->alias_address) == 0;
}

static void
filter_mail_identity_element_xml_create (EFilterElement *element,
					 xmlNodePtr node)
{
	xmlNodePtr n;

	/* Chain up to parent's method. */
	E_FILTER_ELEMENT_CLASS (em_filter_mail_identity_element_parent_class)->xml_create (element, node);

	n = node->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node within 'label': %s\n", n->name);
		}
		n = n->next;
	}
}

static xmlNodePtr
filter_mail_identity_element_xml_encode (EFilterElement *element)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (element);
	xmlNodePtr value;

	value = xmlNewNode (NULL, (xmlChar *) "value");
	xmlSetProp (value, (xmlChar *) "name", (xmlChar *) element->name);

	if (mail_identity->priv->display_name)
		xmlSetProp (value, (xmlChar *) "display-name", (xmlChar *) mail_identity->priv->display_name);

	if (mail_identity->priv->identity_uid)
		xmlSetProp (value, (xmlChar *) "identity-uid", (xmlChar *) mail_identity->priv->identity_uid);

	if (mail_identity->priv->alias_name)
		xmlSetProp (value, (xmlChar *) "alias-name", (xmlChar *) mail_identity->priv->alias_name);

	if (mail_identity->priv->alias_address)
		xmlSetProp (value, (xmlChar *) "alias-address", (xmlChar *) mail_identity->priv->alias_address);

	return value;
}

static gint
filter_mail_identity_element_xml_decode (EFilterElement *element,
					 xmlNodePtr node)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (element);
	xmlChar *x_display_name, *x_identity_uid, *x_alias_name, *x_alias_address;

	xmlFree (element->name);
	element->name = (gchar *) xmlGetProp (node, (xmlChar *) "name");

	x_display_name = xmlGetProp (node, (xmlChar *) "display-name");
	x_identity_uid = xmlGetProp (node, (xmlChar *) "identity-uid");
	x_alias_name = xmlGetProp (node, (xmlChar *) "alias-name");
	x_alias_address = xmlGetProp (node, (xmlChar *) "alias-address");

	filter_mail_identity_take_value (mail_identity,
		g_strdup ((gchar *) x_display_name),
		g_strdup ((gchar *) x_identity_uid),
		g_strdup ((gchar *) x_alias_name),
		g_strdup ((gchar *) x_alias_address));

	g_clear_pointer (&x_display_name, xmlFree);
	g_clear_pointer (&x_identity_uid, xmlFree);
	g_clear_pointer (&x_alias_name, xmlFree);
	g_clear_pointer (&x_alias_address, xmlFree);

	return 0;
}

static EFilterElement *
filter_mail_identity_element_clone (EFilterElement *element)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (element);
	EMFilterMailIdentityElement *clone_mail_identity;
	EFilterElement *clone;

	/* Chain up to parent's method. */
	clone = E_FILTER_ELEMENT_CLASS (em_filter_mail_identity_element_parent_class)->clone (element);

	clone_mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (clone);
	clone_mail_identity->priv->display_name = g_strdup (mail_identity->priv->display_name);
	clone_mail_identity->priv->identity_uid = g_strdup (mail_identity->priv->identity_uid);
	clone_mail_identity->priv->alias_name = g_strdup (mail_identity->priv->alias_name);
	clone_mail_identity->priv->alias_address = g_strdup (mail_identity->priv->alias_address);

	if (mail_identity->priv->registry)
		clone_mail_identity->priv->registry = g_object_ref (mail_identity->priv->registry);

	return clone;
}

static void
filter_mail_identity_element_changed_cb (GtkComboBox *combo_box,
					 gpointer user_data)
{
	EMFilterMailIdentityElement *mail_identity = user_data;
	GtkTreeIter iter;
	gchar *display_name = NULL, *identity_uid = NULL, *alias_name = NULL, *alias_address = NULL;

	g_return_if_fail (EM_IS_FILTER_MAIL_IDENTITY_ELEMENT (mail_identity));

	if (!e_mail_identity_combo_box_get_active_uid (E_MAIL_IDENTITY_COMBO_BOX (combo_box), &identity_uid, &alias_name, &alias_address)) {
		identity_uid = NULL;
		alias_name = NULL;
		alias_address = NULL;
	}

	if (gtk_combo_box_get_active_iter (combo_box, &iter)) {
		GtkTreeModel *model;

		model = gtk_combo_box_get_model (combo_box);
		gtk_tree_model_get (model, &iter,
			E_MAIL_IDENTITY_COMBO_BOX_COLUMN_DISPLAY_NAME, &display_name,
			-1);
	}

	filter_mail_identity_take_value (mail_identity, display_name, identity_uid, alias_name, alias_address);
}

static GtkWidget *
filter_mail_identity_element_get_widget (EFilterElement *element)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (element);
	EMailIdentityComboBox *combo_box;
	GtkWidget *widget;

	widget = e_mail_identity_combo_box_new (mail_identity->priv->registry);
	combo_box = E_MAIL_IDENTITY_COMBO_BOX (widget);
	e_mail_identity_combo_box_set_none_title (combo_box, _("Default Account"));
	e_mail_identity_combo_box_set_allow_none (combo_box, TRUE);
	e_mail_identity_combo_box_set_allow_aliases (combo_box, TRUE);

	g_signal_connect_object (combo_box, "changed",
		G_CALLBACK (filter_mail_identity_element_changed_cb), mail_identity, 0);

	if (mail_identity->priv->identity_uid) {
		e_mail_identity_combo_box_set_active_uid (combo_box,
			mail_identity->priv->identity_uid,
			mail_identity->priv->alias_name,
			mail_identity->priv->alias_address);
	} else {
		e_mail_identity_combo_box_set_active_uid (combo_box, "", NULL, NULL);
	}

	return widget;
}

static void
filter_mail_identity_element_add_value (GString *str,
					const gchar *value)
{
	const gchar *pp;

	if (!value)
		return;

	for (pp = value; *pp; pp++) {
		if (*pp == '\\' || *pp == '|')
			g_string_append_c (str, '\\');
		g_string_append_c (str, *pp);
	}
}

static void
filter_mail_identity_element_format_sexp (EFilterElement *element,
					  GString *out)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (element);
	GString *value = NULL;

	if (mail_identity->priv->identity_uid && *mail_identity->priv->identity_uid) {
		/* Encode the value as: "identity_uid|alias_name|alias_value" */
		value = g_string_sized_new (strlen (mail_identity->priv->identity_uid) * 2);

		filter_mail_identity_element_add_value (value, mail_identity->priv->identity_uid);
		g_string_append_c (value, '|');
		filter_mail_identity_element_add_value (value, mail_identity->priv->alias_name);
		g_string_append_c (value, '|');
		filter_mail_identity_element_add_value (value, mail_identity->priv->alias_address);
	}

	camel_sexp_encode_string (out, value ? value->str : NULL);

	if (value)
		g_string_free (value, TRUE);
}

static void
filter_mail_identity_element_describe (EFilterElement *element,
				       GString *out)
{
	EMFilterMailIdentityElement *mail_identity = EM_FILTER_MAIL_IDENTITY_ELEMENT (element);

	if (mail_identity->priv->display_name && *mail_identity->priv->display_name)
		g_string_append (out, mail_identity->priv->display_name);
}

static void
em_filter_mail_identity_element_class_init (EMFilterMailIdentityElementClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = filter_mail_identity_element_finalize;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->eq = filter_mail_identity_element_eq;
	filter_element_class->xml_create = filter_mail_identity_element_xml_create;
	filter_element_class->xml_encode = filter_mail_identity_element_xml_encode;
	filter_element_class->xml_decode = filter_mail_identity_element_xml_decode;
	filter_element_class->clone = filter_mail_identity_element_clone;
	filter_element_class->get_widget = filter_mail_identity_element_get_widget;
	filter_element_class->format_sexp = filter_mail_identity_element_format_sexp;
	filter_element_class->describe = filter_mail_identity_element_describe;
}

static void
em_filter_mail_identity_element_init (EMFilterMailIdentityElement *mail_identity)
{
	mail_identity->priv = em_filter_mail_identity_element_get_instance_private (mail_identity);
}

EFilterElement *
em_filter_mail_identity_element_new (ESourceRegistry *registry)
{
	EMFilterMailIdentityElement *mail_identity;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	mail_identity = g_object_new (EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT, NULL);
	mail_identity->priv->registry = g_object_ref (registry);

	return E_FILTER_ELEMENT (mail_identity);
}

ESourceRegistry *
em_filter_mail_identity_element_get_registry (EMFilterMailIdentityElement *mail_identity)
{
	g_return_val_if_fail (EM_IS_FILTER_MAIL_IDENTITY_ELEMENT (mail_identity), NULL);

	return mail_identity->priv->registry;
}
