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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include "e-composer-header-table.h"

#include <glib/gi18n-lib.h>

#include "e-msg-composer.h"
#include "e-composer-private.h"
#include "e-composer-from-header.h"
#include "e-composer-name-header.h"
#include "e-composer-post-header.h"
#include "e-composer-spell-header.h"
#include "e-composer-text-header.h"

#define HEADER_TOOLTIP_TO \
	_("Enter the recipients of the message")
#define HEADER_TOOLTIP_CC \
	_("Enter the addresses that will receive a " \
	  "carbon copy of the message")
#define HEADER_TOOLTIP_BCC \
	_("Enter the addresses that will receive a " \
	  "carbon copy of the message without appearing " \
	  "in the recipient list of the message")

struct _EComposerHeaderTablePrivate {
	EComposerHeader *headers[E_COMPOSER_NUM_HEADERS];
	GtkWidget *signature_label;
	GtkWidget *signature_combo_box;
	ENameSelector *name_selector;
	EClientCache *client_cache;

	gchar *previous_from_uid;

	guint size_allocated_idle_id;
	gint last_allocated_width;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_DESTINATIONS_BCC,
	PROP_DESTINATIONS_CC,
	PROP_DESTINATIONS_TO,
	PROP_IDENTITY_UID,
	PROP_POST_TO,
	PROP_REPLY_TO,
	PROP_SIGNATURE_COMBO_BOX,
	PROP_SIGNATURE_UID,
	PROP_SUBJECT,
	PROP_MAIL_FOLLOWUP_TO,
	PROP_MAIL_REPLY_TO
};

G_DEFINE_TYPE_WITH_PRIVATE (EComposerHeaderTable, e_composer_header_table, GTK_TYPE_GRID)

static void
g_value_set_destinations (GValue *value,
                          EDestination **destinations)
{
	GPtrArray *array;
	gint ii;

	/* Preallocate some reasonable number. */
	array = g_ptr_array_new_full (64, g_object_unref);

	for (ii = 0; destinations[ii] != NULL; ii++) {
		g_ptr_array_add (array, e_destination_copy (destinations[ii]));
	}

	g_value_take_boxed (value, array);
}

static EDestination **
g_value_dup_destinations (const GValue *value)
{
	EDestination **destinations;
	const EDestination *dest;
	GPtrArray *array;
	guint ii;

	array = g_value_get_boxed (value);
	destinations = g_new0 (EDestination *, array->len + 1);

	for (ii = 0; ii < array->len; ii++) {
		dest = g_ptr_array_index (array, ii);
		destinations[ii] = e_destination_copy (dest);
	}

	return destinations;
}

static void
g_value_set_string_list (GValue *value,
                         GList *list)
{
	GPtrArray *array;

	array = g_ptr_array_new_full (g_list_length (list), g_free);

	while (list != NULL) {
		g_ptr_array_add (array, g_strdup (list->data));
		list = list->next;
	}

	g_value_take_boxed (value, array);
}

static GList *
g_value_dup_string_list (const GValue *value)
{
	GPtrArray *array;
	GList *list = NULL;
	gint ii;

	array = g_value_get_boxed (value);

	for (ii = 0; ii < array->len; ii++) {
		const gchar *element = g_ptr_array_index (array, ii);
		list = g_list_prepend (list, g_strdup (element));
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

static EDestination **
composer_header_table_update_destinations (EDestination **old_destinations,
                                           const gchar * const *auto_addresses)
{
	CamelAddress *address;
	CamelInternetAddress *inet_address;
	EDestination **new_destinations;
	EDestination *destination;
	GQueue queue = G_QUEUE_INIT;
	guint length;
	gint ii;

	/* Include automatic recipients for the selected account. */

	if (auto_addresses == NULL)
		goto skip_auto;

	inet_address = camel_internet_address_new ();
	address = CAMEL_ADDRESS (inet_address);

	/* XXX Calling camel_address_decode() multiple times on the same
	 *     CamelInternetAddress has a cumulative effect, which isn't
	 *     well documented. */
	for (ii = 0; auto_addresses[ii] != NULL; ii++)
		camel_address_decode (address, auto_addresses[ii]);

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

		g_queue_push_tail (&queue, destination);
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
		g_queue_push_tail (&queue, destination);
	}

skip_custom:

	length = g_queue_get_length (&queue);
	new_destinations = g_new0 (EDestination *, length + 1);

	for (ii = 0; ii < length; ii++)
		new_destinations[ii] = g_queue_pop_head (&queue);

	/* Sanity check. */
	g_warn_if_fail (g_queue_is_empty (&queue));

	return new_destinations;
}

static void
composer_header_table_setup_mail_headers (EComposerHeaderTable *table)
{
	GSettings *settings;
	gint ii;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeader *header;
		const gchar *key;
		gboolean sensitive;
		gboolean visible;

		header = e_composer_header_table_get_header (table, ii);

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				key = "composer-show-from-override";
				break;

			case E_COMPOSER_HEADER_BCC:
				key = "composer-show-bcc";
				break;

			case E_COMPOSER_HEADER_CC:
				key = "composer-show-cc";
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				key = "composer-show-reply-to";
				break;

			case E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO:
				key = "composer-show-mail-followup-to";
				break;

			case E_COMPOSER_HEADER_MAIL_REPLY_TO:
				key = "composer-show-mail-reply-to";
				break;

			default:
				key = NULL;
				break;
		}

		if (key != NULL)
			g_settings_unbind (header, "visible");

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				sensitive = TRUE;
				visible = TRUE;
				break;

			case E_COMPOSER_HEADER_BCC:
			case E_COMPOSER_HEADER_CC:
			case E_COMPOSER_HEADER_REPLY_TO:
			case E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO:
			case E_COMPOSER_HEADER_MAIL_REPLY_TO:
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

		if (key != NULL) {
			if (ii == E_COMPOSER_HEADER_FROM)
				g_settings_bind (
					settings, key,
					header, "override-visible",
					G_SETTINGS_BIND_DEFAULT);
			else
				g_settings_bind (
					settings, key,
					header, "visible",
					G_SETTINGS_BIND_DEFAULT);
		}
	}

	g_object_unref (settings);
}

static void
composer_header_table_setup_post_headers (EComposerHeaderTable *table)
{
	GSettings *settings;
	gint ii;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeader *header;
		const gchar *key;

		header = e_composer_header_table_get_header (table, ii);

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				key = "composer-show-post-from";
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				key = "composer-show-post-reply-to";
				break;

			case E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO:
				key = "composer-show-post-mail-followup-to";
				break;

			case E_COMPOSER_HEADER_MAIL_REPLY_TO:
				key = "composer-show-post-mail-reply-to";
				break;

			default:
				key = NULL;
				break;
		}

		if (key != NULL)
			g_settings_unbind (header, "visible");

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
			case E_COMPOSER_HEADER_POST_TO:
			case E_COMPOSER_HEADER_REPLY_TO:
			case E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO:
			case E_COMPOSER_HEADER_MAIL_REPLY_TO:
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
			g_settings_bind (
				settings, key,
				header, "visible",
				G_SETTINGS_BIND_DEFAULT);
	}

	g_object_unref (settings);
}

static gboolean
composer_header_table_show_post_headers (EComposerHeaderTable *table)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *extension_name;
	gchar *target_uid;
	gboolean show_post_headers = FALSE;

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	target_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	list = e_source_registry_list_sources (registry, extension_name);

	/* Look for a mail account referencing this mail identity.
	 * If the mail account's backend name is "nntp", show the
	 * post headers.  Otherwise show the mail headers.
	 *
	 * XXX What if multiple accounts use this identity but only
	 *     one is "nntp"?  Maybe it should be indicated by the
	 *     transport somehow?
	 */
	for (link = list; link != NULL; link = link->next) {
		ESource *source = E_SOURCE (link->data);
		ESourceExtension *extension;
		const gchar *backend_name;
		const gchar *identity_uid;

		extension = e_source_get_extension (source, extension_name);

		backend_name = e_source_backend_get_backend_name (
			E_SOURCE_BACKEND (extension));
		identity_uid = e_source_mail_account_get_identity_uid (
			E_SOURCE_MAIL_ACCOUNT (extension));

		if (g_strcmp0 (identity_uid, target_uid) != 0)
			continue;

		if (g_strcmp0 (backend_name, "nntp") != 0)
			continue;

		show_post_headers = TRUE;
		break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_object_unref (client_cache);
	g_object_unref (registry);
	g_free (target_uid);

	return show_post_headers;
}

static void
composer_header_table_from_changed_cb (EComposerHeaderTable *table)
{
	ESource *source = NULL;
	ESource *mail_account = NULL;
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;
	EComposerPostHeader *post_header;
	EComposerTextHeader *text_header;
	EDestination **old_destinations;
	EDestination **new_destinations;
	gchar *name = NULL;
	gchar *address = NULL;
	gchar *uid;
	const gchar *reply_to = NULL;
	const gchar * const *bcc = NULL;
	const gchar * const *cc = NULL;

	/* Keep "Post-To" and "Reply-To" synchronized with "From" */

	uid = e_composer_header_table_dup_identity_uid (table, &name, &address);
	if (uid != NULL)
		source = e_composer_header_table_ref_source (table, uid);
	g_free (uid);

	/* Make sure this is really a mail identity source. */
	if (source != NULL) {
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		if (!e_source_has_extension (source, extension_name)) {
			g_object_unref (source);
			source = NULL;
		}
	}

	if (source != NULL) {
		ESourceMailIdentity *mi;
		ESourceMailComposition *mc;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		mi = e_source_get_extension (source, extension_name);

		extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
		mc = e_source_get_extension (source, extension_name);

		if (!address) {
			g_free (name);

			name = e_source_mail_identity_dup_name (mi);
			address = e_source_mail_identity_dup_address (mi);
		}

		if (!name)
			name = e_source_mail_identity_dup_name (mi);

		reply_to = e_source_mail_identity_get_reply_to (mi);
		bcc = e_source_mail_composition_get_bcc (mc);
		cc = e_source_mail_composition_get_cc (mc);

		if (table->priv->previous_from_uid) {
			ESource *previous_source;

			previous_source = e_composer_header_table_ref_source (table, table->priv->previous_from_uid);
			if (previous_source && e_source_has_extension (previous_source, E_SOURCE_EXTENSION_MAIL_IDENTITY)) {
				const gchar *previous_reply_to;
				const gchar *current_reply_to;
				gboolean matches;

				mi = e_source_get_extension (previous_source, E_SOURCE_EXTENSION_MAIL_IDENTITY);
				previous_reply_to = e_source_mail_identity_get_reply_to (mi);

				header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_REPLY_TO);
				text_header = E_COMPOSER_TEXT_HEADER (header);
				current_reply_to = e_composer_text_header_get_text (text_header);

				matches = ((!current_reply_to || !*current_reply_to) && (!previous_reply_to || !*previous_reply_to)) ||
					g_strcmp0 (current_reply_to, previous_reply_to) == 0;

				/* Do not change Reply-To, if the user changed it. */
				if (!matches)
					reply_to = current_reply_to;
			}
		}

		g_free (table->priv->previous_from_uid);
		table->priv->previous_from_uid = g_strdup (e_source_get_uid (source));

		g_object_unref (source);
	} else {
		g_free (table->priv->previous_from_uid);
		table->priv->previous_from_uid = NULL;
	}

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);
	e_composer_from_header_set_name (from_header, name);
	e_composer_from_header_set_address (from_header, address);

	type = E_COMPOSER_HEADER_POST_TO;
	header = e_composer_header_table_get_header (table, type);
	post_header = E_COMPOSER_POST_HEADER (header);
	e_composer_post_header_set_mail_account (post_header, mail_account);

	type = E_COMPOSER_HEADER_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);
	e_composer_text_header_set_text (text_header, reply_to);

	/* Update automatic CC destinations. */
	old_destinations =
		e_composer_header_table_get_destinations_cc (table);
	new_destinations =
		composer_header_table_update_destinations (
		old_destinations, cc);
	e_composer_header_table_set_destinations_cc (table, new_destinations);
	e_destination_freev (old_destinations);
	e_destination_freev (new_destinations);

	/* Update automatic BCC destinations. */
	old_destinations =
		e_composer_header_table_get_destinations_bcc (table);
	new_destinations =
		composer_header_table_update_destinations (
		old_destinations, bcc);
	e_composer_header_table_set_destinations_bcc (table, new_destinations);
	e_destination_freev (old_destinations);
	e_destination_freev (new_destinations);

	if (composer_header_table_show_post_headers (table))
		composer_header_table_setup_post_headers (table);
	else
		composer_header_table_setup_mail_headers (table);

	g_free (name);
	g_free (address);
}

static gboolean
composer_header_table_size_allocated_idle_cb (gpointer user_data)
{
	EComposerHeaderTable *self = user_data;

	if (self->priv->size_allocated_idle_id) {
		GtkWidget *from_combo_box;
		GtkWidget *signature_combo_box;

		self->priv->size_allocated_idle_id = 0;
		self->priv->last_allocated_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));

		from_combo_box = self->priv->headers[E_COMPOSER_HEADER_FROM] ? self->priv->headers[E_COMPOSER_HEADER_FROM]->input_widget : NULL;
		signature_combo_box = self->priv->signature_combo_box;

		if (from_combo_box && gtk_widget_is_visible (from_combo_box) && signature_combo_box && gtk_widget_is_visible (signature_combo_box)) {
			EMailIdentityComboBox *from_combo = E_MAIL_IDENTITY_COMBO_BOX (from_combo_box);
			EMailSignatureComboBox *signature_combo = E_MAIL_SIGNATURE_COMBO_BOX (signature_combo_box);
			GtkAllocation from_alloc = { 0, }, signature_alloc = { 0, };
			gint left_space, from_natural, signature_natural, signature_to_set;

			gtk_widget_get_allocation (from_combo_box, &from_alloc);
			gtk_widget_get_allocation (signature_combo_box, &signature_alloc);

			left_space = from_alloc.width + signature_alloc.width;
			from_natural = e_mail_identity_combo_box_get_last_natural_width (from_combo);
			signature_natural = e_mail_signature_combo_box_get_last_natural_width (signature_combo);

			if (from_natural + signature_natural <= left_space)
				signature_to_set = 0;
			else
				signature_to_set = left_space / 3; /* to prefer From over Signature */

			e_mail_signature_combo_box_set_max_natural_width (signature_combo, signature_to_set);
		} else {
			if (from_combo_box)
				e_mail_identity_combo_box_set_max_natural_width (E_MAIL_IDENTITY_COMBO_BOX (from_combo_box), 0);
			if (signature_combo_box)
				e_mail_signature_combo_box_set_max_natural_width (E_MAIL_SIGNATURE_COMBO_BOX (signature_combo_box), 0);
		}
	}

	return G_SOURCE_REMOVE;
}

static void
composer_header_table_size_allocate (GtkWidget *widget,
				     GtkAllocation *allocation)
{
	EComposerHeaderTable *self = E_COMPOSER_HEADER_TABLE (widget);

	GTK_WIDGET_CLASS (e_composer_header_table_parent_class)->size_allocate (widget, allocation);

	if (allocation->width != self->priv->last_allocated_width && !self->priv->size_allocated_idle_id)
		self->priv->size_allocated_idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 50, composer_header_table_size_allocated_idle_cb, self, NULL);
}

static void
composer_header_table_set_client_cache (EComposerHeaderTable *table,
                                        EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (table->priv->client_cache == NULL);

	table->priv->client_cache = g_object_ref (client_cache);
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
		case PROP_CLIENT_CACHE:
			composer_header_table_set_client_cache (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_object (value));
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

		case PROP_IDENTITY_UID:
			e_composer_header_table_set_identity_uid (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value), NULL, NULL);
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

		case PROP_MAIL_FOLLOWUP_TO:
			e_composer_header_table_set_mail_followup_to (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_MAIL_REPLY_TO:
			e_composer_header_table_set_mail_reply_to (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
			return;

		case PROP_SIGNATURE_UID:
			e_composer_header_table_set_signature_uid (
				E_COMPOSER_HEADER_TABLE (object),
				g_value_get_string (value));
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
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_composer_header_table_ref_client_cache (
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

		case PROP_IDENTITY_UID:
			g_value_take_string (
				value,
				e_composer_header_table_dup_identity_uid (
				E_COMPOSER_HEADER_TABLE (object), NULL, NULL));
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

		case PROP_MAIL_FOLLOWUP_TO:
			g_value_set_string (
				value,
				e_composer_header_table_get_mail_followup_to (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_MAIL_REPLY_TO:
			g_value_set_string (
				value,
				e_composer_header_table_get_mail_reply_to (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SIGNATURE_COMBO_BOX:
			g_value_set_object (
				value,
				e_composer_header_table_get_signature_combo_box (
				E_COMPOSER_HEADER_TABLE (object)));
			return;

		case PROP_SIGNATURE_UID:
			g_value_set_string (
				value,
				e_composer_header_table_get_signature_uid (
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
	EComposerHeaderTable *self = E_COMPOSER_HEADER_TABLE (object);
	gint ii;

	if (self->priv->size_allocated_idle_id) {
		g_source_remove (self->priv->size_allocated_idle_id);
		self->priv->size_allocated_idle_id = 0;
	}

	for (ii = 0; ii < G_N_ELEMENTS (self->priv->headers); ii++) {
		g_clear_object (&self->priv->headers[ii]);
	}

	g_clear_object (&self->priv->signature_combo_box);

	if (self->priv->name_selector != NULL) {
		e_name_selector_cancel_loading (self->priv->name_selector);
		g_clear_object (&self->priv->name_selector);
	}

	g_clear_object (&self->priv->client_cache);

	g_clear_pointer (&self->priv->previous_from_uid, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_header_table_parent_class)->dispose (object);
}

static void
composer_header_table_constructed (GObject *object)
{
	EComposerHeaderTable *table;
	EComposerFromHeader *from_header;
	ENameSelector *name_selector;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EComposerHeader *header;
	GtkWidget *widget;
	guint ii;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_header_table_parent_class)->constructed (object);

	table = E_COMPOSER_HEADER_TABLE (object);

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	name_selector = e_name_selector_new (client_cache);
	table->priv->name_selector = name_selector;

	header = e_composer_from_header_new (registry, _("Fr_om:"));
	composer_header_table_bind_header ("identity-uid", "changed", header);
	g_signal_connect_swapped (
		header, "changed", G_CALLBACK (
		composer_header_table_from_changed_cb), table);
	table->priv->headers[E_COMPOSER_HEADER_FROM] = header;

	header = e_composer_text_header_new_label (registry, _("_Reply-To:"));
	composer_header_table_bind_header ("reply-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_REPLY_TO] = header;

	header = e_composer_text_header_new_label (registry, _("Mail-Followu_p-To:"));
	composer_header_table_bind_header ("mail-followup-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO] = header;

	header = e_composer_text_header_new_label (registry, _("Mail-Repl_y-To:"));
	composer_header_table_bind_header ("mail-reply-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_MAIL_REPLY_TO] = header;

	header = e_composer_name_header_new (
		registry, _("_To:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_TO);
	composer_header_table_bind_header ("destinations-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_TO] = header;

	header = e_composer_name_header_new (
		registry, _("_Cc:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_CC);
	composer_header_table_bind_header ("destinations-cc", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_CC] = header;

	header = e_composer_name_header_new (
		registry, _("_Bcc:"), name_selector);
	e_composer_header_set_input_tooltip (header, HEADER_TOOLTIP_BCC);
	composer_header_table_bind_header ("destinations-bcc", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_BCC] = header;

	header = e_composer_post_header_new (registry, _("_Post To:"));
	composer_header_table_bind_header ("post-to", "changed", header);
	table->priv->headers[E_COMPOSER_HEADER_POST_TO] = header;

	header = e_composer_spell_header_new_label (registry, _("S_ubject:"));
	composer_header_table_bind_header ("subject", "changed", header);
	e_composer_header_set_title_has_tooltip (header, FALSE);
	e_composer_header_set_input_has_tooltip (header, FALSE);
	table->priv->headers[E_COMPOSER_HEADER_SUBJECT] = header;

	widget = e_mail_signature_combo_box_new (registry);
	composer_header_table_bind_widget ("signature-uid", "changed", widget);
	table->priv->signature_combo_box = g_object_ref_sink (widget);

	widget = gtk_label_new_with_mnemonic (_("Si_gnature:"));
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), table->priv->signature_combo_box);
	table->priv->signature_label = g_object_ref_sink (widget);
	gtk_grid_set_column_spacing (GTK_GRID (object), 6);
	gtk_grid_set_row_spacing (GTK_GRID (object), 4);

	for (ii = 0; ii < G_N_ELEMENTS (table->priv->headers); ii++) {
		gint row = ii;

		if (ii > E_COMPOSER_HEADER_FROM)
			row++;

		gtk_grid_attach (
			GTK_GRID (object),
			table->priv->headers[ii]->title_widget,
			0, row, 1, 1);
		gtk_grid_attach (
			GTK_GRID (object),
			table->priv->headers[ii]->input_widget,
			1, row, 3, 1);
		gtk_widget_set_hexpand (table->priv->headers[ii]->input_widget, TRUE);
	}

	ii = E_COMPOSER_HEADER_FROM;

	/* Leave room in the "From" row for signature stuff. */
	gtk_container_child_set (
		GTK_CONTAINER (object),
		table->priv->headers[ii]->input_widget,
		"width", 1, NULL);

	e_binding_bind_property (
		table->priv->headers[ii]->input_widget, "visible",
		table->priv->signature_combo_box, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		table->priv->signature_combo_box, "visible",
		table->priv->signature_label, "visible",
		G_BINDING_SYNC_CREATE);

	/* Now add the signature stuff. */
	gtk_grid_attach (
		GTK_GRID (object),
		table->priv->signature_label,
		2, ii, 1, 1);
	gtk_grid_attach (
		GTK_GRID (object),
		table->priv->signature_combo_box,
		3, ii, 1, 1);

	from_header = E_COMPOSER_FROM_HEADER (e_composer_header_table_get_header (table, E_COMPOSER_HEADER_FROM));

	gtk_grid_attach (
		GTK_GRID (object),
		from_header->override_widget,
		1, ii + 1, 1, 1);

	/* Initialize the headers. */
	composer_header_table_from_changed_cb (table);

	g_object_unref (client_cache);
	g_object_unref (registry);
}

static void
e_composer_header_table_class_init (EComposerHeaderTableClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_header_table_set_property;
	object_class->get_property = composer_header_table_get_property;
	object_class->dispose = composer_header_table_dispose;
	object_class->constructed = composer_header_table_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_allocate = composer_header_table_size_allocate;

	/**
	 * EComposerHeaderTable:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_BCC,
		g_param_spec_boxed (
			"destinations-bcc",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_CC,
		g_param_spec_boxed (
			"destinations-cc",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DESTINATIONS_TO,
		g_param_spec_boxed (
			"destinations-to",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_UID,
		g_param_spec_string (
			"identity-uid",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_POST_TO,
		g_param_spec_boxed (
			"post-to",
			NULL,
			NULL,
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REPLY_TO,
		g_param_spec_string (
			"reply-to",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MAIL_FOLLOWUP_TO,
		g_param_spec_string (
			"mail-followup-to",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MAIL_REPLY_TO,
		g_param_spec_string (
			"mail-reply-to",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_COMBO_BOX,
		g_param_spec_string (
			"signature-combo-box",
			NULL,
			NULL,
			NULL,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_UID,
		g_param_spec_string (
			"signature-uid",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SUBJECT,
		g_param_spec_string (
			"subject",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
composer_header_table_realize_cb (EComposerHeaderTable *table)
{
	g_return_if_fail (table != NULL);
	g_return_if_fail (table->priv != NULL);

	g_signal_handlers_disconnect_by_func (
		table, composer_header_table_realize_cb, NULL);

	e_name_selector_load_books (table->priv->name_selector);
}

static void
e_composer_header_table_init (EComposerHeaderTable *table)
{
	table->priv = e_composer_header_table_get_instance_private (table);

	gtk_grid_set_column_spacing (GTK_GRID (table), 6);

	/* postpone name_selector loading, do that only when really needed */
	g_signal_connect (
		table, "realize",
		G_CALLBACK (composer_header_table_realize_cb), NULL);
}

GtkWidget *
e_composer_header_table_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_HEADER_TABLE,
		"client-cache", client_cache, NULL);
}

EClientCache *
e_composer_header_table_ref_client_cache (EComposerHeaderTable *table)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	return g_object_ref (table->priv->client_cache);
}

EComposerHeader *
e_composer_header_table_get_header (EComposerHeaderTable *table,
                                    EComposerHeaderType type)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);
	g_return_val_if_fail (type < E_COMPOSER_NUM_HEADERS, NULL);

	return table->priv->headers[type];
}

EMailSignatureComboBox *
e_composer_header_table_get_signature_combo_box (EComposerHeaderTable *table)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	return E_MAIL_SIGNATURE_COMBO_BOX (table->priv->signature_combo_box);
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
	g_return_val_if_fail (total == 0 && n_to == 0 && n_cc == 0 && n_bcc == 0, destinations);

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

gchar *
e_composer_header_table_dup_identity_uid (EComposerHeaderTable *table,
					  gchar **chosen_alias_name,
					  gchar **chosen_alias_address)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_dup_active_id (from_header, chosen_alias_name, chosen_alias_address);
}

void
e_composer_header_table_set_identity_uid (EComposerHeaderTable *table,
					  const gchar *identity_uid,
					  const gchar *alias_name,
					  const gchar *alias_address)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	e_composer_from_header_set_active_id (from_header, identity_uid, alias_name, alias_address);
}

const gchar *
e_composer_header_table_get_from_name (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_get_name (from_header);
}

const gchar *
e_composer_header_table_get_from_address (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerFromHeader *from_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_FROM;
	header = e_composer_header_table_get_header (table, type);
	from_header = E_COMPOSER_FROM_HEADER (header);

	return e_composer_from_header_get_address (from_header);
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

const gchar *
e_composer_header_table_get_mail_followup_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	return e_composer_text_header_get_text (text_header);
}

void
e_composer_header_table_set_mail_followup_to (EComposerHeaderTable *table,
					      const gchar *mail_followup_to)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	e_composer_text_header_set_text (text_header, mail_followup_to);

	if (mail_followup_to != NULL && *mail_followup_to != '\0')
		e_composer_header_set_visible (header, TRUE);
}

const gchar *
e_composer_header_table_get_mail_reply_to (EComposerHeaderTable *table)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	type = E_COMPOSER_HEADER_MAIL_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	return e_composer_text_header_get_text (text_header);
}

void
e_composer_header_table_set_mail_reply_to (EComposerHeaderTable *table,
					   const gchar *mail_reply_to)
{
	EComposerHeader *header;
	EComposerHeaderType type;
	EComposerTextHeader *text_header;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	type = E_COMPOSER_HEADER_MAIL_REPLY_TO;
	header = e_composer_header_table_get_header (table, type);
	text_header = E_COMPOSER_TEXT_HEADER (header);

	e_composer_text_header_set_text (text_header, mail_reply_to);

	if (mail_reply_to != NULL && *mail_reply_to != '\0')
		e_composer_header_set_visible (header, TRUE);
}

const gchar *
e_composer_header_table_get_signature_uid (EComposerHeaderTable *table)
{
	EMailSignatureComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);

	combo_box = e_composer_header_table_get_signature_combo_box (table);

	return gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
}

void
e_composer_header_table_set_signature_uid (EComposerHeaderTable *table,
                                           const gchar *signature_uid)
{
	EMailSignatureComboBox *combo_box;

	g_return_if_fail (E_IS_COMPOSER_HEADER_TABLE (table));

	combo_box = e_composer_header_table_get_signature_combo_box (table);

	gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), signature_uid);
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

/**
 * e_composer_header_table_ref_source:
 * @table: an #EComposerHeaderTable
 * @uid: a unique identifier string
 *
 * Convenience function that works just like e_source_registry_ref_source(),
 * but spares the caller from digging out the #ESourceRegistry from @table.
 *
 * The returned #ESource is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ESource, or %NULL if no match was found
 **/
ESource *
e_composer_header_table_ref_source (EComposerHeaderTable *table,
                                    const gchar *uid)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	ESource *source;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER_TABLE (table), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	source = e_source_registry_ref_source (registry, uid);

	g_object_unref (client_cache);
	g_object_unref (registry);

	return source;
}

