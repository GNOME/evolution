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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "e-composer-header-table.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libedataserverui/e-name-selector.h>

#include <shell/e-shell.h>
#include <e-util/e-binding.h>
#include <e-util/gconf-bridge.h>
#include <misc/e-signature-combo-box.h>

#include "e-msg-composer.h"
#include "e-composer-private.h"
#include "e-composer-from-header.h"
#include "e-composer-name-header.h"
#include "e-composer-post-header.h"
#include "e-composer-text-header.h"

#define E_COMPOSER_HEADER_TABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTablePrivate))

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
	PROP_SHELL,
	PROP_SIGNATURE,
	PROP_SIGNATURE_LIST,
	PROP_SUBJECT
};

struct _EComposerHeaderTablePrivate {
	EComposerHeader *headers[E_COMPOSER_NUM_HEADERS];
	guint gconf_bindings[E_COMPOSER_NUM_HEADERS];
	GtkWidget *signature_label;
	GtkWidget *signature_combo_box;
	ENameSelector *name_selector;
	EShell *shell;
};

G_DEFINE_TYPE (
	EComposerHeaderTable,
	e_composer_header_table,
	GTK_TYPE_TABLE)

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
	EShell *shell;
	GtkWidget *parent;

	/* FIXME Pass this in somehow. */
	shell = e_shell_get_default ();

	if (e_shell_get_small_screen_mode (shell)) {
		parent = gtk_widget_get_parent (widget);
		parent = g_object_get_data (G_OBJECT (parent), "pdata");
	} else
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

static EDestination **
composer_header_table_update_destinations (EDestination **old_destinations,
                                           const gchar *auto_addresses)
{
	CamelAddress *address;
	CamelInternetAddress *inet_address;
	EDestination **new_destinations;
	EDestination *destination;
	GList *list = NULL;
	guint length;
	gint ii;

	/* Include automatic recipients for the selected account. */

	if (auto_addresses == NULL)
		goto skip_auto;

	inet_address = camel_internet_address_new ();
	address = CAMEL_ADDRESS (inet_address);

	if (camel_address_decode (address, auto_addresses) != -1) {
		for (ii = 0; ii < camel_address_length (address); ii++) {
			const gchar *name, *email;

			if (!camel_internet_address_get (
				inet_address, ii, &name, &email))
				continue;

			destination = e_destination_new ();
			e_destination_set_auto_recipient (destination, TRUE);

			if (name != NULL)
				e_destination_set_name (destination, name);

			if (email != NULL)
				e_destination_set_email (destination, email);

			list = g_list_prepend (list, destination);
		}
	}

	g_object_unref (inet_address);

skip_auto:

	/* Include custom recipients for this message. */

	if (old_destinations == NULL)
		goto skip_custom;

	for (ii = 0; old_destinations[ii] != NULL; ii++) {
		if (e_destination_is_auto_recipient (old_destinations[ii]))
			continue;

		destination = e_destination_copy (old_destinations[ii]);
		list = g_list_prepend (list, destination);
	}

skip_custom:

	list = g_list_reverse (list);
	length = g_list_length (list);

	new_destinations = g_new0 (EDestination *, length + 1);

	for (ii = 0; list != NULL; ii++) {
		new_destinations[ii] = E_DESTINATION (list->data);
		list = g_list_delete_link (list, list);
	}

	return new_destinations;
}

static gboolean
from_header_should_be_visible (EComposerHeaderTable *table)
{
	EShell *shell;
	EComposerHeader *header;
	EComposerHeaderType type;
	EAccountComboBox *combo_box;

	shell = e_composer_header_table_get_shell (table);

	/* Always display From in standard mode. */
	if (!e_shell_get_express_mode (shell))
		return TRUE;

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	combo_box = E_ACCOUNT_COMBO_BOX (header->input_widget);

	return (e_account_combo_box_count_displayed_accounts (combo_box) > 1);
}

static void
composer_header_table_setup_mail_headers (EComposerHeaderTable *table)
{
	GConfBridge *bridge;
	gint ii;

	bridge = gconf_bridge_get ();

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeader *header;
		const gchar *key;
		guint binding_id;
		gboolean sensitive;
		gboolean visible;

		binding_id = table->priv->gconf_bindings[ii];
		header = e_composer_header_table_get_header (table, ii);

		if (binding_id > 0)
			gconf_bridge_unbind (bridge, binding_id);

		switch (ii) {
			case E_COMPOSER_HEADER_BCC:
				key = COMPOSER_GCONF_PREFIX "/show_mail_bcc";
				break;

			case E_COMPOSER_HEADER_CC:
				key = COMPOSER_GCONF_PREFIX "/show_mail_cc";
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				key = COMPOSER_GCONF_PREFIX "/show_mail_reply_to";
				break;

			default:
				key = NULL;
				break;
		}

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				sensitive = TRUE;
				visible = from_header_should_be_visible (table);
				break;

			case E_COMPOSER_HEADER_BCC:
			case E_COMPOSER_HEADER_CC:
			case E_COMPOSER_HEADER_REPLY_TO:
			case E_COMPOSER_HEADER_SUBJECT:
			case E_COMPOSER_HEADER_TO:
				sensitive = TRUE;
				visible = TRUE;
				break;

			default:
				sensitive = FALSE;
				visible = FALSE;
				break;
		}

		e_composer_header_set_sensitive (header, sensitive);
		e_composer_header_set_visible (header, visible);

		if (key != NULL)
			binding_id = gconf_bridge_bind_property (
				bridge, key, G_OBJECT (header), "visible");
		else
			binding_id = 0;

		table->priv->gconf_bindings[ii] = binding_id;
	}
}

static void
composer_header_table_setup_post_headers (EComposerHeaderTable *table)
{
	GConfBridge *bridge;
	gint ii;

	bridge = gconf_bridge_get ();

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeader *header;
		const gchar *key;
		guint binding_id;

		binding_id = table->priv->gconf_bindings[ii];
		header = e_composer_header_table_get_header (table, ii);

		if (binding_id > 0)
			gconf_bridge_unbind (bridge, binding_id);

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				key = COMPOSER_GCONF_PREFIX "/show_post_from";
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				key = COMPOSER_GCONF_PREFIX "/show_post_reply_to";
				break;

			default:
				key = NULL;
				break;
		}

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
			case E_COMPOSER_HEADER_POST_TO:
			case E_COMPOSER_HEADER_REPLY_TO:
			case E_COMPOSER_HEADER_SUBJECT:
				e_composer_header_set_sensitive (header, TRUE);
				e_composer_header_set_visible (header, TRUE);
				break;

			default:  /* this includes TO, CC and BCC */
				e_composer_header_set_sensitive (header, FALSE);
				e_composer_header_set_visible (header, FALSE);
				break;
		}

		if (key != NULL)
			binding_id = gconf_bridge_bind_property (
				bridge, key, G_OBJECT (header), "visible");
		else
			binding_id = 0;

		table->priv->gconf_bindings[ii] = binding_id;
	}
}

static void
composer_header_table_from_changed_cb (EComposerHeaderTable *table)
{
	EAccount *account;
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;
	EComposerTextHeader *text_header;
	EDestination **old_destinations;
	EDestination **new_destinations;
	const gchar *reply_to;
	const gchar *source_url;
	gboolean always_cc;
	gboolean always_bcc;

	/* Keep "Post-To" and "Reply-To" synchronized with "From" */

	account = e_composer_header_table_get_account (table);
	source_url = e_account_get_string (account, E_ACCOUNT_SOURCE_URL);

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);
	e_composer_post_header_set_account (post_header, account);

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	reply_to = (account != NULL) ? account->id->reply_to : NULL;
	text_header = E_COMPOSER_TEXT_HEADER (header);
	e_composer_text_header_set_text (text_header, reply_to);

	always_cc = (account != NULL && account->always_cc);
	always_bcc = (account != NULL && account->always_bcc);

	/* Update automatic CC destinations. */
	old_destinations =
		e_composer_header_table_get_destinations_cc (table);
	new_destinations =
		composer_header_table_update_destinations (
		old_destinations, always_cc ? account->cc_addrs : NULL);
	e_composer_header_table_set_destinations_cc (table, new_destinations);
	e_destination_freev (old_destinations);
	e_destination_freev (new_destinations);

	/* Update automatic BCC destinations. */
	old_destinations =
		e_composer_header_table_get_destinations_bcc (table);
	new_destinations =
		composer_header_table_update_destinations (
		old_destinations, always_bcc ? account->bcc_addrs : NULL);
	e_composer_header_table_set_destinations_bcc (table, new_destinations);
	e_destination_freev (old_destinations);
	e_destination_freev (new_destinations);

	/* XXX We should NOT be checking specific account types here.
	 *     Would prefer EAccount have a "send_method" enum item:
	 *
	 *         E_ACCOUNT_SEND_METHOD_MAIL
	 *         E_ACCOUNT_SEND_METHOD_POST
	 *
	 *     And that would dictate which set of headers we show
	 *     in the composer when an account is selected.  Alas,
	 *     EAccount has no private storage, so it would require
	 *     an ABI break and I don't want to deal with that now.
	 *     (But would anything besides Evolution be affected?)
	 *
	 *     Currently only NNTP accounts use the "POST" fields.
	 */
	if (source_url == NULL)
		composer_header_table_setup_mail_headers (table);
	else if (g_ascii_strncasecmp (source_url, "nntp:", 5) == 0)
		composer_header_table_setup_post_headers (table);
	else
		composer_header_table_setup_mail_headers (table);
}

static void
composer_header_table_set_shell (EComposerHeaderTable *table,
                                 EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (table->priv->shell == NULL);

	table->priv->shell = g_object_ref (shell);
}

static GObject *
composer_header_table_constructor (GType type,
                                   guint n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
	GObject *object;
	EComposerHeaderTablePrivate *priv;
	guint rows, ii;
	gint row_padding;
	gboolean small_screen_mode;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (
		e_composer_header_table_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	priv = E_COMPOSER_HEADER_TABLE_GET_PRIVATE (object);

	small_screen_mode = e_shell_get_small_screen_mode (priv->shell);

	rows = G_N_ELEMENTS (priv->headers);
	gtk_table_resize (GTK_TABLE (object), rows, 4);
	gtk_table_set_row_spacings (GTK_TABLE (object), 0);
	gtk_table_set_col_spacings (GTK_TABLE (object), 6);

	/* Use "ypadding" instead of "row-spacing" because some rows may
	 * be invisible and we don't want spacing around them. */

	/* For small screens, pack the table's rows closely together. */
	row_padding = small_screen_mode ? 0 : 3;

	for (ii = 0; ii < rows; ii++) {
		gtk_table_attach (
			GTK_TABLE (object), priv->headers[ii]->title_widget,
			0, 1, ii, ii + 1, GTK_FILL, GTK_FILL, 0, row_padding);
		gtk_table_attach (
			GTK_TABLE (object),
			priv->headers[ii]->input_widget, 1, 4,
			ii, ii + 1, GTK_FILL | GTK_EXPAND, 0, 0, row_padding);
	}

	ii = E_COMPOSER_HEADER_FROM;

	/* Leave room in the "From" row for signature stuff. */
	gtk_container_child_set (
		GTK_CONTAINER (object),
		priv->headers[ii]->input_widget,
		"right-attach", 2, NULL);

	e_binding_new (
		priv->headers[ii]->input_widget, "visible",
		priv->signature_label, "visible");

	e_binding_new (
		priv->headers[ii]->input_widget, "visible",
		priv->signature_combo_box, "visible");

	/* Now add the signature stuff. */
	if (!small_screen_mode) {
		gtk_table_attach (
			GTK_TABLE (object), priv->signature_label,
			2, 3, ii, ii + 1, 0, 0, 0, row_padding);
		gtk_table_attach (
			GTK_TABLE (object), priv->signature_combo_box,
			3, 4, ii, ii + 1, 0, 0, 0, row_padding);
	} else {
		GtkWidget *box = gtk_hbox_new (FALSE, 0);

		gtk_box_pack_start (
			GTK_BOX (box), priv->signature_label,
			FALSE, FALSE, 4);
		gtk_box_pack_end (
			GTK_BOX (box), priv->signature_combo_box,
			TRUE, TRUE, 0);
		g_object_set_data (G_OBJECT (box), "pdata", object);
		gtk_table_attach (
			GTK_TABLE (object), box,
			3, 4, ii, ii + 1, GTK_FILL, 0, 0, row_padding);
		gtk_widget_hide (box);
	}

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

		case PROP_SHELL:
			composer_header_table_set_shell (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
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

		case PROP_SHELL:
			g_value_set_object (
				value,
				e_composer_header_table_get_shell (
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

	if (priv->shell != NULL) {
		g_object_unref (priv->shell);
		priv->shell = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_header_table_parent_class)->dispose (object);
}

static void
e_composer_header_table_class_init (EComposerHeaderTableClass *class)
{
	GObjectClass *object_class;
	GParamSpec *element_spec;

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

	/* floating reference */
	element_spec = g_param_spec_string (
		"value-array-element",
		NULL,
		NULL,
		NULL,
		G_PARAM_READWRITE);

	g_object_class_install_property (
		object_class,
		PROP_POST_TO,
		g_param_spec_value_array (
			"post-to",
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
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			NULL,
			NULL,
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

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
e_composer_header_table_init (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	ENameSelector *name_selector;
	GtkWidget *widget;
	gint ii;

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

	/* XXX EComposerHeader ought to do this itself, but I need to
	 *     make the title_widget and input_widget members private. */
	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		header = table->priv->headers[ii];
		e_binding_new (
			header, "visible",
			header->title_widget, "visible");
		e_binding_new (
			header, "visible",
			header->input_widget, "visible");
	}
}

GtkWidget *
e_composer_header_table_new (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_HEADER_TABLE,
		"shell", shell, NULL);
}

EShell *
e_composer_header_table_get_shell (EComposerHeaderTable *table)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	return table->priv->shell;
}

EComposerHeader *
e_composer_header_table_get_header (EComposerHeaderTable *table,
                                    EComposerHeaderType type)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);
	g_return_val_if_fail (type < E_COMPOSER_NUM_HEADERS, NULL);

	return table->priv->headers[type];
}

EAccount *
e_composer_header_table_get_account (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_get_active (from_header);
}

gboolean
e_composer_header_table_set_account (EComposerHeaderTable *table,
                                     EAccount *account)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), FALSE);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_set_active (from_header, account);
}

EAccountList *
e_composer_header_table_get_account_list (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_get_account_list (from_header);
}

void
e_composer_header_table_set_account_list (EComposerHeaderTable *table,
                                          EAccountList *account_list)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	e_composer_from_header_set_account_list (from_header, account_list);
}

const gchar *
e_composer_header_table_get_account_name (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_get_active_name (from_header);
}

gboolean
e_composer_header_table_set_account_name (EComposerHeaderTable *table,
                                          const gchar *account_name)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), FALSE);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_set_active_name (from_header, account_name);
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
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_BCC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	return e_composer_name_header_get_destinations (name_header);
}

void
e_composer_header_table_add_destinations_bcc (EComposerHeaderTable *table,
                                              EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_BCC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_add_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

void
e_composer_header_table_set_destinations_bcc (EComposerHeaderTable *table,
                                              EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_BCC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_set_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

EDestination **
e_composer_header_table_get_destinations_cc (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_CC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	return e_composer_name_header_get_destinations (name_header);
}

void
e_composer_header_table_add_destinations_cc (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_CC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_add_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

void
e_composer_header_table_set_destinations_cc (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_CC;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_set_destinations (name_header, destinations);

	if (destinations != NULL && *destinations != NULL)
		e_composer_header_set_visible (header, TRUE);
}

EDestination **
e_composer_header_table_get_destinations_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_TO;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	return e_composer_name_header_get_destinations (name_header);
}

void
e_composer_header_table_add_destinations_to (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_TO;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_add_destinations (name_header, destinations);
}

void
e_composer_header_table_set_destinations_to (EComposerHeaderTable *table,
                                             EDestination **destinations)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerNameHeader *name_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_TO;
	header = e_composer_header_table_get_header (table, type);
	name_header = E_COMPOSER_NAME_HEADER (header);

	e_composer_name_header_set_destinations (name_header, destinations);
}

GList *
e_composer_header_table_get_post_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);

	return e_composer_post_header_get_folders (post_header);
}

void
e_composer_header_table_set_post_to_base (EComposerHeaderTable *table,
                                          const gchar *base_url,
                                          const gchar *folders)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);

	e_composer_post_header_set_folders_base (post_header, base_url, folders);
}

void
e_composer_header_table_set_post_to_list (EComposerHeaderTable *table,
                                          GList *folders)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerPostHeader *post_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);

	e_composer_post_header_set_folders (post_header, folders);
}

const gchar *
e_composer_header_table_get_reply_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	return e_composer_text_header_get_text (text_header);
}

void
e_composer_header_table_set_reply_to (EComposerHeaderTable *table,
                                      const gchar *reply_to)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	e_composer_text_header_set_text (text_header, reply_to);

	if (reply_to != NULL && *reply_to != '\0')
		e_composer_header_set_visible (header, TRUE);
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
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_SUBJECT;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	return e_composer_text_header_get_text (text_header);
}

void
e_composer_header_table_set_subject (EComposerHeaderTable *table,
                                     const gchar *subject)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_SUBJECT;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	e_composer_text_header_set_text (text_header, subject);
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
