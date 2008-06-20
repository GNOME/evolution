/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-composer-header-table.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libedataserverui/e-name-selector.h>

#include "e-signature-combo-box.h"

#include "e-composer-from-header.h"
#include "e-composer-name-header.h"
#include "e-composer-post-header.h"
#include "e-composer-text-header.h"

#define E_COMPOSER_HEADER_TABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTablePrivate))

#define E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER(table) \
	(E_COMPOSER_FROM_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_FROM)))

#define E_COMPOSER_HEADER_TABLE_GET_REPLY_TO_HEADER(table) \
	(E_COMPOSER_TEXT_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_REPLY_TO)))

#define E_COMPOSER_HEADER_TABLE_GET_TO_HEADER(table) \
	(E_COMPOSER_NAME_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_TO)))

#define E_COMPOSER_HEADER_TABLE_GET_CC_HEADER(table) \
	(E_COMPOSER_NAME_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_CC)))

#define E_COMPOSER_HEADER_TABLE_GET_BCC_HEADER(table) \
	(E_COMPOSER_NAME_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_BCC)))

#define E_COMPOSER_HEADER_TABLE_GET_POST_TO_HEADER(table) \
	(E_COMPOSER_POST_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_POST_TO)))

#define E_COMPOSER_HEADER_TABLE_GET_SUBJECT_HEADER(table) \
	(E_COMPOSER_TEXT_HEADER (e_composer_header_table_get_header \
	(E_COMPOSER_HEADER_TABLE (table), E_COMPOSER_HEADER_SUBJECT)))

#define HEADER_TOOLTIP_TO \
	_("Enter the recipients of the message")
#define HEADER_TOOLTIP_CC \
	_("Enter the addresses that will receive a " \
	  "carbon copy of the message")
#define HEADER_TOOLTIP_BCC \
	_("Enter the addresses that will receive a " \
	  "carbon copy of the message without appearing " \
	  "in the recipient list of the message")

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_ACCOUNT_LIST,
	PROP_ACCOUNT_NAME,
	PROP_DESTINATIONS_BCC,
	PROP_DESTINATIONS_CC,
	PROP_DESTINATIONS_TO,
	PROP_POST_TO,
	PROP_REPLY_TO,
	PROP_SIGNATURE,
	PROP_SIGNATURE_LIST,
	PROP_SUBJECT
};

struct _EComposerHeaderTablePrivate {
	EComposerHeader *headers[E_COMPOSER_NUM_HEADERS];
	GtkWidget *signature_label;
	GtkWidget *signature_combo_box;
	ENameSelector *name_selector;
};

static gpointer parent_class;

static void
g_value_set_destinations (GValue *value,
                          EDestination **destinations)
{
	GValueArray *value_array;
	GValue element;
	gint ii;

	memset (&element, 0, sizeof (GValue));
	g_value_init (&element, E_TYPE_DESTINATION);

	/* Preallocate some reasonable number. */
	value_array = g_value_array_new (64);

	for (ii = 0; destinations[ii] != NULL; ii++) {
		g_value_set_object (&element, destinations[ii]);
		g_value_array_append (value_array, &element);
	}

	g_value_take_boxed (value, value_array);
}

static EDestination **
g_value_dup_destinations (const GValue *value)
{
	EDestination **destinations;
	GValueArray *value_array;
	GValue *element;
	gint ii;

	value_array = g_value_get_boxed (value);
	destinations = g_new0 (EDestination *, value_array->n_values + 1);

	for (ii = 0; ii < value_array->n_values; ii++) {
		element = g_value_array_get_nth (value_array, ii);
		destinations[ii] = g_value_dup_object (element);
	}

	return destinations;
}

static void
g_value_set_string_list (GValue *value,
                         GList *list)
{
	GValueArray *value_array;
	GValue element;

	memset (&element, 0, sizeof (GValue));
	g_value_init (&element, G_TYPE_STRING);

	value_array = g_value_array_new (g_list_length (list));

	while (list != NULL) {
		g_value_set_string (&element, list->data);
		g_value_array_append (value_array, &element);
	}

	g_value_take_boxed (value, value_array);
}

static GList *
g_value_dup_string_list (const GValue *value)
{
	GValueArray *value_array;
	GList *list = NULL;
	GValue *element;
	gint ii;

	value_array = g_value_get_boxed (value);

	for (ii = 0; ii < value_array->n_values; ii++) {
		element = g_value_array_get_nth (value_array, ii);
		list = g_list_prepend (list, g_value_dup_string (element));
	}

	return g_list_reverse (list);
}

static void
composer_header_table_notify_header (EComposerHeader *header,
                                     const gchar *property_name)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (header->input_widget);
	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (parent));
	g_object_notify (G_OBJECT (parent), property_name);
}

static void
composer_header_table_notify_widget (GtkWidget *widget,
                                     const gchar *property_name)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (widget);
	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (parent));
	g_object_notify (G_OBJECT (parent), property_name);
}

static void
composer_header_table_bind_header (const gchar *property_name,
                                   const gchar *signal_name,
                                   EComposerHeader *header)
{
	/* Propagate the signal as "notify::property_name". */

	g_signal_connect (
		header, signal_name,
		G_CALLBACK (composer_header_table_notify_header),
		(gpointer) property_name);
}

static void
composer_header_table_bind_widget (const gchar *property_name,
                                   const gchar *signal_name,
                                   GtkWidget *widget)
{
	/* Propagate the signal as "notify::property_name". */

	g_signal_connect (
		widget, signal_name,
		G_CALLBACK (composer_header_table_notify_widget),
		(gpointer) property_name);
}

static void
composer_header_table_from_changed_cb (EComposerHeaderTable *table)
{
	EAccount *account;
	EComposerPostHeader *post_header;
	EComposerTextHeader *text_header;
	const gchar *reply_to;

	/* Keep "Post-To" and "Reply-To" synchronized with "From" */

	account = e_composer_header_table_get_account (table);

	post_header = E_COMPOSER_HEADER_TABLE_GET_POST_TO_HEADER (table);
	e_composer_post_header_set_account (post_header, account);

	reply_to = (account != NULL) ? account->id->reply_to : NULL;
	text_header = E_COMPOSER_HEADER_TABLE_GET_REPLY_TO_HEADER (table);
	e_composer_text_header_set_text (text_header, reply_to);
}

static GObject *
composer_header_table_constructor (GType type,
                                   guint n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
	GObject *object;
	EComposerHeaderTablePrivate *priv;
	guint rows, ii;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	priv = E_COMPOSER_HEADER_TABLE_GET_PRIVATE (object);

	rows = G_N_ELEMENTS (priv->headers);
	gtk_table_resize (GTK_TABLE (object), rows, 4);
	gtk_table_set_row_spacings (GTK_TABLE (object), 0);
	gtk_table_set_col_spacings (GTK_TABLE (object), 6);

	/* Use "ypadding" instead of "row-spacing" because some rows may
	 * be invisible and we don't want spacing around them. */

	for (ii = 0; ii < rows; ii++) {
		gtk_table_attach (
			GTK_TABLE (object), priv->headers[ii]->title_widget,
			0, 1, ii, ii + 1, GTK_FILL, GTK_FILL, 0, 3);
		gtk_table_attach (
			GTK_TABLE (object), priv->headers[ii]->input_widget,
			1, 4, ii, ii + 1, GTK_FILL | GTK_EXPAND, 0, 0, 3);
	}

	ii = E_COMPOSER_HEADER_FROM;

	/* Leave room in the "From" row for signature stuff. */
	gtk_container_child_set (
		GTK_CONTAINER (object),
		priv->headers[ii]->input_widget,
		"right-attach", 2, NULL);

	/* Now add the signature stuff. */
	gtk_table_attach (
		GTK_TABLE (object), priv->signature_label,
		2, 3, ii, ii + 1, 0, 0, 0, 3);
	gtk_table_attach (
		GTK_TABLE (object), priv->signature_combo_box,
		3, 4, ii, ii + 1, 0, 0, 0, 3);

	return object;
}

static void
composer_header_table_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	EDestination **destinations;
	GList *list;

	switch (property_id) {
		case PROP_ACCOUNT:
			e_composer_header_table_set_account (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_ACCOUNT_LIST:
			e_composer_header_table_set_account_list (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_ACCOUNT_NAME:
			e_composer_header_table_set_account_name (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_DESTINATIONS_BCC:
			destinations = g_value_dup_destinations (value);
			e_composer_header_table_set_destinations_bcc (
				E_COMPOSER_HEADER_TABLE (object),
				destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_CC:
			destinations = g_value_dup_destinations (value);
			e_composer_header_table_set_destinations_cc (
				E_COMPOSER_HEADER_TABLE (object),
				destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_TO:
			destinations = g_value_dup_destinations (value);
			e_composer_header_table_set_destinations_to (
				E_COMPOSER_HEADER_TABLE (object),
				destinations);
			e_destination_freev (destinations);
			return;

		case PROP_POST_TO:
			list = g_value_dup_string_list (value);
			e_composer_header_table_set_post_to_list (
				E_COMPOSER_HEADER_TABLE (object), list);
			g_list_foreach (list, (GFunc) g_free, NULL);
			g_list_free (list);
			return;

		case PROP_REPLY_TO:
			e_composer_header_table_set_reply_to (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_SIGNATURE:
			e_composer_header_table_set_signature (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_SIGNATURE_LIST:
			e_composer_header_table_set_signature_list (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_SUBJECT:
			e_composer_header_table_set_subject (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_header_table_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	EDestination **destinations;
	GList *list;

	switch (property_id) {
		case PROP_ACCOUNT:
			g_value_set_object (
				value,
				e_composer_header_table_get_account (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_ACCOUNT_LIST:
			g_value_set_object (
				value,
				e_composer_header_table_get_account_list (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_ACCOUNT_NAME:
			g_value_set_string (
				value,
				e_composer_header_table_get_account_name (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_DESTINATIONS_BCC:
			destinations =
				e_composer_header_table_get_destinations_bcc (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_destinations (value, destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_CC:
			destinations =
				e_composer_header_table_get_destinations_cc (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_destinations (value, destinations);
			e_destination_freev (destinations);
			return;

		case PROP_DESTINATIONS_TO:
			destinations =
				e_composer_header_table_get_destinations_to (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_destinations (value, destinations);
			e_destination_freev (destinations);
			return;

		case PROP_POST_TO:
			list = e_composer_header_table_get_post_to (
				E_COMPOSER_HEADER_TABLE (object));
			g_value_set_string_list (value, list);
			g_list_foreach (list, (GFunc) g_free, NULL);
			g_list_free (list);
			return;

		case PROP_REPLY_TO:
			g_value_set_string (
				value,
				e_composer_header_table_get_reply_to (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SIGNATURE:
			g_value_set_object (
				value,
				e_composer_header_table_get_signature (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SIGNATURE_LIST:
			g_value_set_object (
				value,
				e_composer_header_table_get_signature_list (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SUBJECT:
			g_value_set_string (
				value,
				e_composer_header_table_get_subject (
				E_COMPOSER_HEADER_TABLE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_header_table_dispose (GObject *object)
{
	EComposerHeaderTablePrivate *priv;
	gint ii;

	priv = E_COMPOSER_HEADER_TABLE_GET_PRIVATE (object);

	for (ii = 0; ii < G_N_ELEMENTS (priv->headers); ii++) {
		if (priv->headers[ii] != NULL) {
			g_object_unref (priv->headers[ii]);
			priv->headers[ii] = NULL;
		}
	}

	if (priv->signature_combo_box != NULL) {
		g_object_unref (priv->signature_combo_box);
		priv->signature_combo_box = NULL;
	}

	if (priv->name_selector != NULL) {
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
composer_header_table_class_init (EComposerHeaderTableClass *class)
{
	GObjectClass *object_class;
	GParamSpec *element_spec;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EComposerHeaderTablePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = composer_header_table_constructor;
	object_class->set_property = composer_header_table_set_property;
	object_class->get_property = composer_header_table_get_property;
	object_class->dispose = composer_header_table_dispose;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT,
		g_param_spec_object (
			"account",
			NULL,
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_LIST,
		g_param_spec_object (
			"account-list",
			NULL,
			NULL,
			E_TYPE_ACCOUNT_LIST,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_NAME,
		g_param_spec_string (
			"account-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	/* floating reference */
	element_spec = g_param_spec_object (
		"value-array-element",
		NULL,
		NULL,
		E_TYPE_DESTINATION,
		G_PARAM_READWRITE);

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_BCC,
		g_param_spec_value_array (
			"destinations-bcc",
			NULL,
			NULL,
			element_spec,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_CC,
		g_param_spec_value_array (
			"destinations-cc",
			NULL,
			NULL,
			element_spec,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_TO,
		g_param_spec_value_array (
			"destinations-to",
			NULL,
			NULL,
			element_spec,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REPLY_TO,
		g_param_spec_string (
			"reply-to",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE,
		g_param_spec_object (
			"signature",
			NULL,
			NULL,
			E_TYPE_SIGNATURE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_LIST,
		g_param_spec_object (
			"signature-list",
			NULL,
			NULL,
			E_TYPE_SIGNATURE_LIST,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SUBJECT,
		g_param_spec_string (
			"subject",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));
}

static void
composer_header_table_init (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	ENameSelector *name_selector;
	GtkWidget *widget;

	table->priv = E_COMPOSER_HEADER_TABLE_GET_PRIVATE (table);

	name_selector = e_name_selector_new ();
	table->priv->name_selector = name_selector;

	header = e_composer_from_header_new (_("Fr_om:"));
	composer_header_table_bind_header ("account", "changed", header);
	composer_header_table_bind_header ("account-list", "refreshed", header);
	composer_header_table_bind_header ("account-name", "changed", header);
	g_signal_connect_swapped (
		header, "changed", G_CALLBACK (
		composer_header_table_from_changed_cb), table);
	table->priv->headers[E_COMPOSER_HEADER_FROM] = header;

	header = e_composer_text_header_new_label (_("_Reply-To:"));
	composer_header_table_bind_header ("reply-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_REPLY_TO] = header;

	header = e_composer_name_header_new (_("_To:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_TO);
	composer_header_table_bind_header ("destinations-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_TO] = header;

	header = e_composer_name_header_new (_("_Cc:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_CC);
	composer_header_table_bind_header ("destinations-cc", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_CC] = header;

	header = e_composer_name_header_new (_("_Bcc:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_BCC);
	composer_header_table_bind_header ("destinations-bcc", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_BCC] = header;

	header = e_composer_post_header_new (_("_Post To:"));
	composer_header_table_bind_header ("post-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_POST_TO] = header;

	header = e_composer_text_header_new_label (_("S_ubject:"));
	composer_header_table_bind_header ("subject", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_SUBJECT] = header;

	widget = e_signature_combo_box_new ();
	composer_header_table_bind_widget ("signature", "changed", widget);
	composer_header_table_bind_widget ("signature-list", "refreshed", widget);
	table->priv->signature_combo_box = g_object_ref_sink (widget);

	widget = gtk_label_new_with_mnemonic (_("Si_gnature:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), table->priv->signature_combo_box);
	table->priv->signature_label = g_object_ref_sink (widget);
}

GType
e_composer_header_table_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EComposerHeaderTableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) composer_header_table_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EComposerHeaderTable),
			0,     /* n_preallocs */
			(GInstanceInitFunc) composer_header_table_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_TABLE, "EComposerHeaderTable", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_composer_header_table_new (void)
{
	return g_object_new (E_TYPE_COMPOSER_HEADER_TABLE, NULL);
}

EComposerHeader *
e_composer_header_table_get_header (EComposerHeaderTable *table,
                                    EComposerHeaderType type)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);
	g_return_val_if_fail (type < E_COMPOSER_NUM_HEADERS, NULL);
	g_return_val_if_fail (type >= 0, NULL);

	return table->priv->headers[type];
}

gboolean
e_composer_header_table_get_header_visible (EComposerHeaderTable *table,
                                            EComposerHeaderType type)
{
	EComposerHeader *header;

	header = e_composer_header_table_get_header (table, type);
	return e_composer_header_get_visible (header);
}

void
e_composer_header_table_set_header_visible (EComposerHeaderTable *table,
                                            EComposerHeaderType type,
                                            gboolean visible)
{
	EComposerHeader *header;

	header = e_composer_header_table_get_header (table, type);
	e_composer_header_set_visible (header, visible);

	/* Signature widgets track the "From" header. */
	if (type == E_COMPOSER_HEADER_FROM) {
		if (visible) {
			gtk_widget_show (table->priv->signature_label);
			gtk_widget_show (table->priv->signature_combo_box);
		} else {
			gtk_widget_hide (table->priv->signature_label);
			gtk_widget_hide (table->priv->signature_combo_box);
		}
	}
}

gboolean
e_composer_header_table_get_header_sensitive (EComposerHeaderTable *table,
                                              EComposerHeaderType type)
{
	EComposerHeader *header;

	header = e_composer_header_table_get_header (table, type);
	return e_composer_header_get_sensitive (header);
}

void
e_composer_header_table_set_header_sensitive (EComposerHeaderTable *table,
                                              EComposerHeaderType type,
                                              gboolean sensitive)
{
	EComposerHeader *header;

	header = e_composer_header_table_get_header (table, type);
	e_composer_header_set_sensitive (header, sensitive);
}

EAccount *
e_composer_header_table_get_account (EComposerHeaderTable *table)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER (table);
	return e_composer_from_header_get_active (header);
}

gboolean
e_composer_header_table_set_account (EComposerHeaderTable *table,
                                     EAccount *account)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), FALSE);

	header = E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER (table);
	return e_composer_from_header_set_active (header, account);
}

EAccountList *
e_composer_header_table_get_account_list (EComposerHeaderTable *table)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER (table);
	return e_composer_from_header_get_account_list (header);
}

void
e_composer_header_table_set_account_list (EComposerHeaderTable *table,
                                          EAccountList *account_list)
{
	EComposerFromHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER (table);
	e_composer_from_header_set_account_list (header, account_list);
}

const gchar *
e_composer_header_table_get_account_name (EComposerHeaderTable *table)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER (table);
	return e_composer_from_header_get_active_name (header);
}

gboolean
e_composer_header_table_set_account_name (EComposerHeaderTable *table,
                                          const gchar *account_name)
{
	EComposerFromHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), FALSE);

	header = E_COMPOSER_HEADER_TABLE_GET_FROM_HEADER (table);
	return e_composer_from_header_set_active_name (header, account_name);
}

EDestination **
e_composer_header_table_get_destinations (EComposerHeaderTable *table)
{
	EDestination **destinations;
	EDestination **to, **cc, **bcc;
	gint total, n_to, n_cc, n_bcc;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	to = e_composer_header_table_get_destinations_to (table);
	for (n_to = 0; to != NULL && to[n_to] != NULL; n_to++);

	cc = e_composer_header_table_get_destinations_cc (table);
	for (n_cc = 0; cc != NULL && cc[n_cc] != NULL; n_cc++);

	bcc = e_composer_header_table_get_destinations_bcc (table);
	for (n_bcc = 0; bcc != NULL && bcc[n_bcc] != NULL; n_bcc++);

	total = n_to + n_cc + n_bcc;
	destinations = g_new0 (EDestination *, total + 1);

	while (n_bcc > 0 && total > 0)
		destinations[--total] = g_object_ref (bcc[--n_bcc]);

	while (n_cc > 0 && total > 0)
		destinations[--total] = g_object_ref (cc[--n_cc]);

	while (n_to > 0 && total > 0)
		destinations[--total] = g_object_ref (to[--n_to]);

	/* Counters should all be zero now. */
	g_assert (total == 0 && n_to == 0 && n_cc == 0 && n_bcc == 0);

	e_destination_freev (to);
	e_destination_freev (cc);
	e_destination_freev (bcc);

	return destinations;
}

EDestination **
e_composer_header_table_get_destinations_bcc (EComposerHeaderTable *table)
{
	EComposerNameHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_BCC_HEADER (table);
	return e_composer_name_header_get_destinations (header);
}

void
e_composer_header_table_set_destinations_bcc (EComposerHeaderTable *table,
                                              EDestination **destinations)
{
	EComposerNameHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_BCC_HEADER (table);
	e_composer_name_header_set_destinations (header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_table_set_header_visible (
			table, E_COMPOSER_HEADER_BCC, TRUE);
}

EDestination **
e_composer_header_table_get_destinations_cc (EComposerHeaderTable *table)
{
	EComposerNameHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_CC_HEADER (table);
	return e_composer_name_header_get_destinations (header);
}

void
e_composer_header_table_set_destinations_cc (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerNameHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_CC_HEADER (table);
	e_composer_name_header_set_destinations (header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_table_set_header_visible (
			table, E_COMPOSER_HEADER_CC, TRUE);
}

EDestination **
e_composer_header_table_get_destinations_to (EComposerHeaderTable *table)
{
	EComposerNameHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_TO_HEADER (table);
	return e_composer_name_header_get_destinations (header);
}

void
e_composer_header_table_set_destinations_to (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerNameHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_TO_HEADER (table);
	e_composer_name_header_set_destinations (header, destinations);
}

GList *
e_composer_header_table_get_post_to (EComposerHeaderTable *table)
{
	EComposerPostHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_POST_TO_HEADER (table);
	return e_composer_post_header_get_folders (header);
}

void
e_composer_header_table_set_post_to_base (EComposerHeaderTable *table,
                                          const gchar *base_url,
                                          const gchar *folders)
{
	EComposerPostHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_POST_TO_HEADER (table);
	e_composer_post_header_set_folders_base (header, base_url, folders);
}

void
e_composer_header_table_set_post_to_list (EComposerHeaderTable *table,
                                          GList *folders)
{
	EComposerPostHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_POST_TO_HEADER (table);
	e_composer_post_header_set_folders (header, folders);
}

const gchar *
e_composer_header_table_get_reply_to (EComposerHeaderTable *table)
{
	EComposerTextHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_REPLY_TO_HEADER (table);
	return e_composer_text_header_get_text (header);
}

void
e_composer_header_table_set_reply_to (EComposerHeaderTable *table,
                                      const gchar *reply_to)
{
	EComposerTextHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_REPLY_TO_HEADER (table);
	e_composer_text_header_set_text (header, reply_to);

	if (reply_to != NULL && *reply_to != '\0')
		e_composer_header_table_set_header_visible (
			table, E_COMPOSER_HEADER_REPLY_TO, TRUE);
}

ESignature *
e_composer_header_table_get_signature (EComposerHeaderTable *table)
{
	ESignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	combo_box = E_SIGNATURE_COMBO_BOX (table->priv->signature_combo_box);
	return e_signature_combo_box_get_active (combo_box);
}

gboolean
e_composer_header_table_set_signature (EComposerHeaderTable *table,
                                       ESignature *signature)
{
	ESignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), FALSE);

	combo_box = E_SIGNATURE_COMBO_BOX (table->priv->signature_combo_box);
	return e_signature_combo_box_set_active (combo_box, signature);
}

ESignatureList *
e_composer_header_table_get_signature_list (EComposerHeaderTable *table)
{
	ESignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	combo_box = E_SIGNATURE_COMBO_BOX (table->priv->signature_combo_box);
	return e_signature_combo_box_get_signature_list (combo_box);
}

void
e_composer_header_table_set_signature_list (EComposerHeaderTable *table,
                                            ESignatureList *signature_list)
{
	ESignatureComboBox *combo_box;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	combo_box = E_SIGNATURE_COMBO_BOX (table->priv->signature_combo_box);
	e_signature_combo_box_set_signature_list (combo_box, signature_list);
}

const gchar *
e_composer_header_table_get_subject (EComposerHeaderTable *table)
{
	EComposerTextHeader *header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	header = E_COMPOSER_HEADER_TABLE_GET_SUBJECT_HEADER (table);
	return e_composer_text_header_get_text (header);
}

void
e_composer_header_table_set_subject (EComposerHeaderTable *table,
                                     const gchar *subject)
{
	EComposerTextHeader *header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	header = E_COMPOSER_HEADER_TABLE_GET_SUBJECT_HEADER (table);
	e_composer_text_header_set_text (header, subject);
}
