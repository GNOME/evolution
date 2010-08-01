/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-addressbook-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-addressbook-selector.h"

#include <e-util/e-selection.h>

#include <eab-book-util.h>
#include <eab-contact-merging.h>

#define E_ADDRESSBOOK_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_SELECTOR, EAddressbookSelectorPrivate))

#define PRIMARY_ADDRESSBOOK_KEY \
	"/apps/evolution/addressbook/display/primary_addressbook"

typedef struct _MergeContext MergeContext;

struct _EAddressbookSelectorPrivate {
	EAddressbookView *current_view;
};

struct _MergeContext {
	EBook *source_book;
	EBook *target_book;

	EContact *current_contact;
	GList *remaining_contacts;
	guint pending_removals;

	gint remove_from_source : 1;
	gint copy_done          : 1;
};

enum {
	PROP_0,
	PROP_CURRENT_VIEW
};

static GtkTargetEntry drag_types[] = {
	{ (gchar *) "text/x-source-vcard", 0, 0 }
};

static gpointer parent_class;

static void
merge_context_next (MergeContext *merge_context)
{
	GList *list;

	list = merge_context->remaining_contacts;
	merge_context->current_contact = list->data;
	list = g_list_delete_link (list, list);
	merge_context->remaining_contacts = list;
}

static MergeContext *
merge_context_new (EBook *source_book,
                   EBook *target_book,
                   GList *contact_list)
{
	MergeContext *merge_context;

	merge_context = g_slice_new0 (MergeContext);
	merge_context->source_book = source_book;
	merge_context->target_book = target_book;
	merge_context->remaining_contacts = contact_list;
	merge_context_next (merge_context);

	return merge_context;
}

static void
merge_context_free (MergeContext *merge_context)
{
	if (merge_context->source_book != NULL)
		g_object_unref (merge_context->source_book);

	if (merge_context->target_book != NULL)
		g_object_unref (merge_context->target_book);

	g_slice_free (MergeContext, merge_context);
}

static void
addressbook_selector_removed_cb (EBook *book,
                                 const GError *error,
                                 MergeContext *merge_context)
{
	merge_context->pending_removals--;

	if (merge_context->remaining_contacts != NULL)
		return;

	if (merge_context->pending_removals > 0)
		return;

	merge_context_free (merge_context);
}

static void
addressbook_selector_merge_next_cb (EBook *book,
                                    const GError *error,
                                    const gchar *id,
                                    MergeContext *merge_context)
{
	if (merge_context->remove_from_source && !error) {
		/* Remove previous contact from source. */
		e_book_remove_contact_async (
			merge_context->source_book,
			merge_context->current_contact,
			(EBookAsyncCallback) addressbook_selector_removed_cb,
			merge_context);
		merge_context->pending_removals++;
	}

	g_object_unref (merge_context->current_contact);

	if (merge_context->remaining_contacts != NULL) {
		merge_context_next (merge_context);
		eab_merging_book_add_contact (
			merge_context->target_book,
			merge_context->current_contact,
			(EBookIdAsyncCallback) addressbook_selector_merge_next_cb,
			merge_context);

	} else if (merge_context->pending_removals == 0)
		merge_context_free (merge_context);
}

static void
addressbook_selector_load_primary_source (ESourceSelector *selector)
{
	GConfClient *client;
	ESourceList *source_list;
	ESource *source = NULL;
	const gchar *key;
	gchar *uid;

	/* XXX If ESourceSelector had a "primary-uid" property,
	 *     we could just bind the GConf key to it. */

	source_list = e_source_selector_get_source_list (selector);

	client = gconf_client_get_default ();
	key = PRIMARY_ADDRESSBOOK_KEY;
	uid = gconf_client_get_string (client, key, NULL);
	g_object_unref (client);

	if (uid != NULL) {
		source = e_source_list_peek_source_by_uid (source_list, uid);
		g_free (uid);
	}

	if (source == NULL) {
		GSList *groups;

		/* Dig up the first source in the source list.
		 * XXX libedataserver should provide API for this. */
		groups = e_source_list_peek_groups (source_list);
		while (groups != NULL) {
			ESourceGroup *source_group = groups->data;
			GSList *sources;

			sources = e_source_group_peek_sources (source_group);
			if (sources != NULL) {
				source = sources->data;
				break;
			}

			groups = g_slist_next (groups);
		}
	}

	if (source != NULL)
		e_source_selector_set_primary_selection (selector, source);
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
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
addressbook_selector_constructed (GObject *object)
{
	ESourceSelector *selector;

	selector = E_SOURCE_SELECTOR (object);
	addressbook_selector_load_primary_source (selector);
}

static void
addressbook_selector_primary_selection_changed (ESourceSelector *selector)
{
	ESource *source;
	GConfClient *client;
	const gchar *key;
	const gchar *string;

	/* XXX If ESourceSelector had a "primary-uid" property,
	 *     we could just bind the GConf key to it. */

	source = e_source_selector_peek_primary_selection (selector);
	if (source == NULL)
		return;

	client = gconf_client_get_default ();
	key = PRIMARY_ADDRESSBOOK_KEY;
	string = e_source_peek_uid (source);
	gconf_client_set_string (client, key, string, NULL);
	g_object_unref (client);
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
	EBook *source_book;
	EBook *target_book;
	GList *list;
	const gchar *string;
	gboolean remove_from_source;

	priv = E_ADDRESSBOOK_SELECTOR_GET_PRIVATE (selector);
	g_return_val_if_fail (priv->current_view != NULL, FALSE);

	string = (const gchar *) gtk_selection_data_get_data (selection_data);
	remove_from_source = (action == GDK_ACTION_MOVE);

	target_book = e_book_new (destination, NULL);
	if (target_book == NULL)
		return FALSE;

	e_book_open (target_book, FALSE, NULL);

	/* XXX Function assumes both out arguments are provided.  All we
	 *     care about is the contact list; source_book will be NULL. */
	eab_book_and_contact_list_from_string (string, &source_book, &list);
	if (list == NULL)
		return FALSE;

	model = e_addressbook_view_get_model (priv->current_view);
	source_book = e_addressbook_model_get_book (model);
	g_return_val_if_fail (E_IS_BOOK (source_book), FALSE);

	merge_context = merge_context_new (source_book, target_book, list);
	merge_context->remove_from_source = remove_from_source;

	eab_merging_book_add_contact (
		target_book, merge_context->current_contact,
		(EBookIdAsyncCallback) addressbook_selector_merge_next_cb,
		merge_context);

	return TRUE;
}

static void
addressbook_selector_class_init (EAddressbookSelectorClass *class)
{
	GObjectClass *object_class;
	ESourceSelectorClass *selector_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAddressbookSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_selector_set_property;
	object_class->get_property = addressbook_selector_get_property;
	object_class->dispose = addressbook_selector_dispose;
	object_class->constructed = addressbook_selector_constructed;

	selector_class = E_SOURCE_SELECTOR_CLASS (class);
	selector_class->primary_selection_changed =
		addressbook_selector_primary_selection_changed;
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
addressbook_selector_init (EAddressbookSelector *selector)
{
	selector->priv = E_ADDRESSBOOK_SELECTOR_GET_PRIVATE (selector);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_directory_targets (GTK_WIDGET (selector));
}

GType
e_addressbook_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EAddressbookSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) addressbook_selector_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAddressbookSelector),
			0,     /* n_preallocs */
			(GInstanceInitFunc) addressbook_selector_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SOURCE_SELECTOR, "EAddressbookSelector",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_addressbook_selector_new (ESourceList *source_list)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	return g_object_new (
		E_TYPE_ADDRESSBOOK_SELECTOR,
		"source-list", source_list, NULL);
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

	if (selector->priv->current_view != NULL) {
		g_object_unref (selector->priv->current_view);
		selector->priv->current_view = NULL;
	}

	if (current_view != NULL)
		g_object_ref (current_view);

	selector->priv->current_view = current_view;

	g_object_notify (G_OBJECT (selector), "current-view");
}
