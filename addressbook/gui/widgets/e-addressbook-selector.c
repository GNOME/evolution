/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-addressbook-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-addressbook-selector.h"

#include <e-util/e-util.h>

#include <eab-book-util.h>
#include <eab-contact-merging.h>

#define E_ADDRESSBOOK_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_SELECTOR, EAddressbookSelectorPrivate))

typedef struct _MergeContext MergeContext;

struct _EAddressbookSelectorPrivate {
	EAddressbookView *current_view;
};

struct _MergeContext {
	ESourceRegistry *registry;
	EBookClient *source_client;
	EBookClient *target_client;

	EContact *current_contact;
	GSList *remaining_contacts;
	guint pending_removals;
	gboolean pending_adds;

	gint remove_from_source : 1;
	gint copy_done : 1;
};

enum {
	PROP_0,
	PROP_CURRENT_VIEW
};

static GtkTargetEntry drag_types[] = {
	{ (gchar *) "text/x-source-vcard", 0, 1 }
};

G_DEFINE_TYPE (
	EAddressbookSelector,
	e_addressbook_selector,
	E_TYPE_CLIENT_SELECTOR)

static void
merge_context_next (MergeContext *merge_context)
{
	GSList *list;

	merge_context->current_contact = NULL;
	if (!merge_context->remaining_contacts)
		return;

	list = merge_context->remaining_contacts;
	merge_context->current_contact = list->data;
	list = g_slist_delete_link (list, list);
	merge_context->remaining_contacts = list;
}

static MergeContext *
merge_context_new (ESourceRegistry *registry,
                   EBookClient *source_client,
                   EBookClient *target_client,
                   GSList *contact_list)
{
	MergeContext *merge_context;

	merge_context = g_slice_new0 (MergeContext);
	merge_context->registry = g_object_ref (registry);
	merge_context->source_client = source_client;
	merge_context->target_client = target_client;
	merge_context->remaining_contacts = contact_list;
	merge_context_next (merge_context);

	return merge_context;
}

static void
merge_context_free (MergeContext *merge_context)
{
	if (merge_context->registry != NULL)
		g_object_unref (merge_context->registry);

	if (merge_context->source_client != NULL)
		g_object_unref (merge_context->source_client);

	if (merge_context->target_client != NULL)
		g_object_unref (merge_context->target_client);

	g_slice_free (MergeContext, merge_context);
}

static void
addressbook_selector_removed_cb (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	MergeContext *merge_context = user_data;
	GError *error = NULL;

	e_book_client_remove_contact_finish (book_client, result, &error);

	if (error != NULL) {
		g_warning (
			"%s: Failed to remove contact: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	merge_context->pending_removals--;

	if (merge_context->pending_adds)
		return;

	if (merge_context->pending_removals > 0)
		return;

	merge_context_free (merge_context);
}

static void
addressbook_selector_merge_next_cb (EBookClient *book_client,
                                    const GError *error,
                                    const gchar *id,
                                    gpointer closure)
{
	MergeContext *merge_context = closure;

	if (merge_context->remove_from_source && !error) {
		/* Remove previous contact from source. */
		e_book_client_remove_contact (
			merge_context->source_client,
			merge_context->current_contact, NULL,
			addressbook_selector_removed_cb, merge_context);
		merge_context->pending_removals++;
	}

	g_object_unref (merge_context->current_contact);

	if (merge_context->remaining_contacts != NULL) {
		merge_context_next (merge_context);
		eab_merging_book_add_contact (
			merge_context->registry,
			merge_context->target_client,
			merge_context->current_contact,
			addressbook_selector_merge_next_cb, merge_context);

	} else if (merge_context->pending_removals == 0) {
		merge_context_free (merge_context);
	} else
		merge_context->pending_adds = FALSE;
}

static void
addressbook_selector_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			e_addressbook_selector_set_current_view (
				E_ADDRESSBOOK_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_selector_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			g_value_set_object (
				value,
				e_addressbook_selector_get_current_view (
				E_ADDRESSBOOK_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_selector_dispose (GObject *object)
{
	EAddressbookSelectorPrivate *priv;

	priv = E_ADDRESSBOOK_SELECTOR_GET_PRIVATE (object);

	if (priv->current_view != NULL) {
		g_object_unref (priv->current_view);
		priv->current_view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_addressbook_selector_parent_class)->dispose (object);
}

static void
addressbook_selector_constructed (GObject *object)
{
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source;

	selector = E_SOURCE_SELECTOR (object);
	registry = e_source_selector_get_registry (selector);
	source = e_source_registry_ref_default_address_book (registry);
	e_source_selector_set_primary_selection (selector, source);
	g_object_unref (source);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_addressbook_selector_parent_class)->constructed (object);
}

static void
target_client_connect_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	MergeContext *merge_context = user_data;
	EClient *client;
	GError *error = NULL;

	g_return_if_fail (merge_context != NULL);

	client = e_client_selector_get_client_finish (
		E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	merge_context->target_client = client ? E_BOOK_CLIENT (client) : NULL;

	if (!merge_context->target_client) {
		g_slist_foreach (
			merge_context->remaining_contacts,
			(GFunc) g_object_unref, NULL);
		g_slist_free (merge_context->remaining_contacts);

		merge_context_free (merge_context);
		return;
	}

	eab_merging_book_add_contact (
		merge_context->registry,
		merge_context->target_client,
		merge_context->current_contact,
		addressbook_selector_merge_next_cb, merge_context);
}

static gboolean
addressbook_selector_data_dropped (ESourceSelector *selector,
                                   GtkSelectionData *selection_data,
                                   ESource *destination,
                                   GdkDragAction action,
                                   guint info)
{
	EAddressbookSelectorPrivate *priv;
	MergeContext *merge_context;
	EAddressbookModel *model;
	EBookClient *source_client;
	ESource *source_source = NULL;
	ESourceRegistry *registry;
	GSList *list;
	const gchar *string;
	gboolean remove_from_source;

	priv = E_ADDRESSBOOK_SELECTOR_GET_PRIVATE (selector);
	g_return_val_if_fail (priv->current_view != NULL, FALSE);

	string = (const gchar *) gtk_selection_data_get_data (selection_data);
	remove_from_source = (action == GDK_ACTION_MOVE);

	registry = e_source_selector_get_registry (selector);

	if (info == drag_types[0].info)
		eab_source_and_contact_list_from_string (
			registry, string, &source_source, &list);
	else
		list = eab_contact_list_from_string (string);

	if (list == NULL) {
		g_clear_object (&source_source);
		return FALSE;
	}

	model = e_addressbook_view_get_model (priv->current_view);
	source_client = e_addressbook_model_get_client (model);
	g_return_val_if_fail (E_IS_BOOK_CLIENT (source_client), FALSE);

	if (remove_from_source && source_source &&
	    !e_source_equal (source_source, e_client_get_source (E_CLIENT (source_client)))) {
		g_warning ("%s: Source book '%s' doesn't match the view client '%s', skipping drop",
			G_STRFUNC, e_source_get_uid (source_source),
			e_source_get_uid (e_client_get_source (E_CLIENT (source_client))));
		g_clear_object (&source_source);
		return FALSE;
	}

	g_clear_object (&source_source);

	merge_context = merge_context_new (
		registry, g_object_ref (source_client), NULL, list);
	merge_context->remove_from_source = remove_from_source;
	merge_context->pending_adds = TRUE;

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), destination, FALSE, 30, NULL,
		target_client_connect_cb, merge_context);

	return TRUE;
}

static void
e_addressbook_selector_class_init (EAddressbookSelectorClass *class)
{
	GObjectClass *object_class;
	ESourceSelectorClass *selector_class;

	g_type_class_add_private (class, sizeof (EAddressbookSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_selector_set_property;
	object_class->get_property = addressbook_selector_get_property;
	object_class->dispose = addressbook_selector_dispose;
	object_class->constructed = addressbook_selector_constructed;

	selector_class = E_SOURCE_SELECTOR_CLASS (class);
	selector_class->data_dropped = addressbook_selector_data_dropped;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_VIEW,
		g_param_spec_object (
			"current-view",
			NULL,
			NULL,
			E_TYPE_ADDRESSBOOK_VIEW,
			G_PARAM_READWRITE));
}

static void
e_addressbook_selector_init (EAddressbookSelector *selector)
{
	selector->priv = E_ADDRESSBOOK_SELECTOR_GET_PRIVATE (selector);

	e_source_selector_set_show_colors (
		E_SOURCE_SELECTOR (selector), FALSE);

	e_source_selector_set_show_toggles (
		E_SOURCE_SELECTOR (selector), FALSE);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_directory_targets (GTK_WIDGET (selector));
}

GtkWidget *
e_addressbook_selector_new (EClientCache *client_cache)
{
	ESourceRegistry *registry;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	registry = e_client_cache_ref_registry (client_cache);

	widget = g_object_new (
		E_TYPE_ADDRESSBOOK_SELECTOR,
		"client-cache", client_cache,
		"extension-name", E_SOURCE_EXTENSION_ADDRESS_BOOK,
		"registry", registry, NULL);

	g_object_unref (registry);

	return widget;
}

EAddressbookView *
e_addressbook_selector_get_current_view (EAddressbookSelector *selector)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_SELECTOR (selector), NULL);

	return selector->priv->current_view;
}

void
e_addressbook_selector_set_current_view (EAddressbookSelector *selector,
                                         EAddressbookView *current_view)
{
	/* XXX This is only needed for moving contacts via drag-and-drop.
	 *     The selection data doesn't include the source of the data
	 *     (the model for the currently selected address book view),
	 *     so we have to rely on it being provided to us.  I would
	 *     be happy to see this function go away. */

	g_return_if_fail (E_IS_ADDRESSBOOK_SELECTOR (selector));

	if (current_view != NULL)
		g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (current_view));

	if (selector->priv->current_view == current_view)
		return;

	if (selector->priv->current_view != NULL) {
		g_object_unref (selector->priv->current_view);
		selector->priv->current_view = NULL;
	}

	if (current_view != NULL)
		g_object_ref (current_view);

	selector->priv->current_view = current_view;

	g_object_notify (G_OBJECT (selector), "current-view");
}
