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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include "e-addressbook-model.h"
#include <e-util/e-marshal.h>
#include <e-util/e-util.h>
#include "eab-gui-util.h"

struct _EAddressbookModelPrivate {
	EClientCache *client_cache;
	gulong client_notify_readonly_handler_id;
	gulong client_notify_capabilities_handler_id;

	EBookClient *book_client;
	gchar *query_str;
	EBookClientView *client_view;
	guint client_view_idle_id;

	/* Query Results */
	GPtrArray *contacts;

	/* Signal Handler IDs */
	gulong create_contact_id;
	gulong remove_contact_id;
	gulong modify_contact_id;
	gulong status_message_id;
	gulong view_complete_id;
	guint remove_status_id;

	guint search_in_progress : 1;
	guint editable : 1;
	guint first_get_view : 1;
};

enum {
	PROP_0,
	PROP_CLIENT,
	PROP_CLIENT_CACHE,
	PROP_EDITABLE,
	PROP_QUERY
};

enum {
	WRITABLE_STATUS,
	STATUS_MESSAGE,
	BEFORE_SEARCH,
	SEARCH_STARTED,
	SEARCH_RESULT,
	COUNT_CHANGED,
	CONTACT_ADDED,
	CONTACTS_REMOVED,
	CONTACT_CHANGED,
	MODEL_CHANGED,
	STOP_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EAddressbookModel, e_addressbook_model, G_TYPE_OBJECT)

static void
free_data (EAddressbookModel *model)
{
	GPtrArray *array;

	array = model->priv->contacts;
	g_ptr_array_foreach (array, (GFunc) g_object_unref, NULL);
	g_ptr_array_set_size (array, 0);
}

static void
remove_book_view (EAddressbookModel *model)
{
	if (model->priv->client_view && model->priv->create_contact_id)
		g_signal_handler_disconnect (
			model->priv->client_view,
			model->priv->create_contact_id);
	if (model->priv->client_view && model->priv->remove_contact_id)
		g_signal_handler_disconnect (
			model->priv->client_view,
			model->priv->remove_contact_id);
	if (model->priv->client_view && model->priv->modify_contact_id)
		g_signal_handler_disconnect (
			model->priv->client_view,
			model->priv->modify_contact_id);
	if (model->priv->client_view && model->priv->status_message_id)
		g_signal_handler_disconnect (
			model->priv->client_view,
			model->priv->status_message_id);
	if (model->priv->client_view && model->priv->view_complete_id)
		g_signal_handler_disconnect (
			model->priv->client_view,
			model->priv->view_complete_id);
	if (model->priv->remove_status_id)
		g_source_remove (model->priv->remove_status_id);

	model->priv->create_contact_id = 0;
	model->priv->remove_contact_id = 0;
	model->priv->modify_contact_id = 0;
	model->priv->status_message_id = 0;
	model->priv->view_complete_id = 0;
	model->priv->remove_status_id = 0;

	model->priv->search_in_progress = FALSE;

	if (model->priv->client_view) {
		GError *error = NULL;

		e_book_client_view_stop (model->priv->client_view, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to stop client view: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		}

		g_object_unref (model->priv->client_view);
		model->priv->client_view = NULL;

		g_signal_emit (model, signals[STATUS_MESSAGE], 0, NULL, -1);
	}
}

static void
view_create_contact_cb (EBookClientView *client_view,
                        const GSList *contact_list,
                        EAddressbookModel *model)
{
	GPtrArray *array;
	guint count;
	guint index;

	array = model->priv->contacts;
	index = array->len;
	count = g_list_length ((GList *) contact_list);

	while (contact_list != NULL) {
		EContact *contact = contact_list->data;

		g_ptr_array_add (array, g_object_ref (contact));
		contact_list = contact_list->next;
	}

	g_signal_emit (model, signals[CONTACT_ADDED], 0, index, count);
	g_signal_emit (model, signals[COUNT_CHANGED], 0, NULL);
}

static gint
sort_descending (gconstpointer ca,
                 gconstpointer cb)
{
	gint a = *((gint *) ca);
	gint b = *((gint *) cb);

	return (a == b) ? 0 : (a < b) ? 1 : -1;
}

static void
view_remove_contact_cb (EBookClientView *client_view,
                        const GSList *ids,
                        EAddressbookModel *model)
{
	/* XXX we should keep a hash around instead of this O(n*m) loop */
	const GSList *iter;
	GArray *indices;
	GPtrArray *array;
	gint ii;

	array = model->priv->contacts;
	indices = g_array_new (FALSE, FALSE, sizeof (gint));

	for (iter = ids; iter != NULL; iter = iter->next) {
		const gchar *target_uid = iter->data;

		for (ii = 0; ii < array->len; ii++) {
			EContact *contact;
			const gchar *uid;

			contact = array->pdata[ii];
			/* check if already removed */
			if (!contact)
				continue;

			uid = e_contact_get_const (contact, E_CONTACT_UID);
			g_return_if_fail (uid != NULL);

			if (strcmp (uid, target_uid) == 0) {
				g_object_unref (contact);
				g_array_append_val (indices, ii);
				array->pdata[ii] = NULL;
				break;
			}
		}
	}

	/* Sort the 'indices' array in descending order, since
	 * g_ptr_array_remove_index() shifts subsequent elements
	 * down one position to fill the gap. */
	g_array_sort (indices, sort_descending);

	for (ii = 0; ii < indices->len; ii++) {
		gint index;

		index = g_array_index (indices, gint, ii);
		g_ptr_array_remove_index (array, index);
	}

	g_signal_emit (model, signals[CONTACTS_REMOVED], 0, indices);
	g_array_free (indices, TRUE);

	g_signal_emit (model, signals[COUNT_CHANGED], 0, NULL);
}

static void
view_modify_contact_cb (EBookClientView *client_view,
                        const GSList *contact_list,
                        EAddressbookModel *model)
{
	GPtrArray *array;

	array = model->priv->contacts;

	while (contact_list != NULL) {
		EContact *new_contact = contact_list->data;
		const gchar *target_uid;
		gint ii;

		target_uid = e_contact_get_const (new_contact, E_CONTACT_UID);
		g_warn_if_fail (target_uid != NULL);

		/* skip contacts without UID */
		if (!target_uid) {
			contact_list = contact_list->next;
			continue;
		}

		for (ii = 0; ii < array->len; ii++) {
			EContact *old_contact;
			const gchar *uid;

			old_contact = array->pdata[ii];
			g_return_if_fail (old_contact != NULL);

			uid = e_contact_get_const (old_contact, E_CONTACT_UID);
			g_return_if_fail (uid != NULL);

			if (strcmp (uid, target_uid) != 0)
				continue;

			g_object_unref (old_contact);
			array->pdata[ii] = e_contact_duplicate (new_contact);

			g_signal_emit (
				model, signals[CONTACT_CHANGED], 0, ii);
			break;
		}

		contact_list = contact_list->next;
	}
}

static void
view_progress_cb (EBookClientView *client_view,
                  guint percent,
                  const gchar *message,
                  EAddressbookModel *model)
{
	if (model->priv->remove_status_id)
		g_source_remove (model->priv->remove_status_id);
	model->priv->remove_status_id = 0;

	g_signal_emit (model, signals[STATUS_MESSAGE], 0, message, percent);
}

static void
view_complete_cb (EBookClientView *client_view,
                  const GError *error,
                  EAddressbookModel *model)
{
	model->priv->search_in_progress = FALSE;
	view_progress_cb (client_view, -1, NULL, model);
	g_signal_emit (model, signals[SEARCH_RESULT], 0, error);
	g_signal_emit (model, signals[STOP_STATE_CHANGED], 0);
}

static void
addressbook_model_client_notify_readonly_cb (EClientCache *client_cache,
                                             EClient *client,
                                             GParamSpec *pspec,
                                             EAddressbookModel *model)
{
	if (!E_IS_BOOK_CLIENT (client))
		return;

	if (E_BOOK_CLIENT (client) == model->priv->book_client) {
		gboolean editable = !e_client_is_readonly (client);
		e_addressbook_model_set_editable (model, editable);
	}
}

static gboolean addressbook_model_idle_cb (EAddressbookModel *model);

static void
addressbook_model_client_notify_capabilities_cb (EClientCache *client_cache,
						 EClient *client,
						 GParamSpec *pspec,
						 EAddressbookModel *model)
{
	if (!E_IS_BOOK_CLIENT (client))
		return;

	if (E_BOOK_CLIENT (client) == model->priv->book_client &&
	    model->priv->client_view_idle_id == 0) {
		model->priv->client_view_idle_id = g_idle_add (
			(GSourceFunc) addressbook_model_idle_cb,
			g_object_ref (model));
	}
}

static void
client_view_ready_cb (GObject *source_object,
                      GAsyncResult *result,
                      gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EBookClientView *client_view = NULL;
	EAddressbookModel *model = user_data;
	GError *error = NULL;

	e_book_client_get_view_finish (
		book_client, result, &client_view, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client_view != NULL) && (error == NULL)) ||
		((client_view == NULL) && (error != NULL)));

	if (error != NULL) {
		eab_error_dialog (
			NULL, NULL, _("Error getting book view"), error);
		g_error_free (error);
		return;
	}

	g_signal_emit (model, signals[BEFORE_SEARCH], 0);

	remove_book_view (model);
	free_data (model);

	model->priv->client_view = client_view;
	if (model->priv->client_view) {
		model->priv->create_contact_id = g_signal_connect (
			model->priv->client_view, "objects-added",
			G_CALLBACK (view_create_contact_cb), model);
		model->priv->remove_contact_id = g_signal_connect (
			model->priv->client_view, "objects-removed",
			G_CALLBACK (view_remove_contact_cb), model);
		model->priv->modify_contact_id = g_signal_connect (
			model->priv->client_view, "objects-modified",
			G_CALLBACK (view_modify_contact_cb), model);
		model->priv->status_message_id = g_signal_connect (
			model->priv->client_view, "progress",
			G_CALLBACK (view_progress_cb), model);
		model->priv->view_complete_id = g_signal_connect (
			model->priv->client_view, "complete",
			G_CALLBACK (view_complete_cb), model);

		model->priv->search_in_progress = TRUE;
	}

	g_signal_emit (model, signals[MODEL_CHANGED], 0);
	g_signal_emit (model, signals[SEARCH_STARTED], 0);
	g_signal_emit (model, signals[STOP_STATE_CHANGED], 0);

	if (model->priv->client_view) {
		e_book_client_view_start (model->priv->client_view, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to start client view: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		}
	}
}

static gboolean
addressbook_model_idle_cb (EAddressbookModel *model)
{
	model->priv->client_view_idle_id = 0;

	if (model->priv->book_client && model->priv->query_str) {
		remove_book_view (model);

		if (model->priv->first_get_view) {
			model->priv->first_get_view = FALSE;

			if (e_client_check_capability (E_CLIENT (model->priv->book_client), "do-initial-query") ||
			    g_strcmp0 (model->priv->query_str, "(contains \"x-evolution-any-field\" \"\")") != 0) {
				e_book_client_get_view (
					model->priv->book_client, model->priv->query_str,
					NULL, client_view_ready_cb, model);
			} else {
				free_data (model);

				g_signal_emit (
					model, signals[MODEL_CHANGED], 0);
				g_signal_emit (
					model, signals[STOP_STATE_CHANGED], 0);
			}
		} else
			e_book_client_get_view (
				model->priv->book_client, model->priv->query_str,
				NULL, client_view_ready_cb, model);

	}

	g_object_unref (model);

	return FALSE;
}

static gboolean
remove_status_cb (gpointer data)
{
	EAddressbookModel *model = data;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), FALSE);

	g_signal_emit (model, signals[STATUS_MESSAGE], 0, NULL, -1);
	model->priv->remove_status_id = 0;

	return FALSE;
}

static void
addressbook_model_set_client_cache (EAddressbookModel *model,
                                    EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (model->priv->client_cache == NULL);

	model->priv->client_cache = g_object_ref (client_cache);
}

static void
addressbook_model_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT:
			e_addressbook_model_set_client (
				E_ADDRESSBOOK_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_CLIENT_CACHE:
			addressbook_model_set_client_cache (
				E_ADDRESSBOOK_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_EDITABLE:
			e_addressbook_model_set_editable (
				E_ADDRESSBOOK_MODEL (object),
				g_value_get_boolean (value));
			return;

		case PROP_QUERY:
			e_addressbook_model_set_query (
				E_ADDRESSBOOK_MODEL (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);

}

static void
addressbook_model_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT:
			g_value_set_object (
				value, e_addressbook_model_get_client (
				E_ADDRESSBOOK_MODEL (object)));
			return;

		case PROP_CLIENT_CACHE:
			g_value_set_object (
				value, e_addressbook_model_get_client_cache (
				E_ADDRESSBOOK_MODEL (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, e_addressbook_model_get_editable (
				E_ADDRESSBOOK_MODEL (object)));
			return;

		case PROP_QUERY:
			g_value_set_string (
				value, e_addressbook_model_get_query (
				E_ADDRESSBOOK_MODEL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_model_dispose (GObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL (object);

	remove_book_view (model);
	free_data (model);

	if (model->priv->client_notify_readonly_handler_id > 0) {
		g_signal_handler_disconnect (
			model->priv->client_cache,
			model->priv->client_notify_readonly_handler_id);
		model->priv->client_notify_readonly_handler_id = 0;
	}

	if (model->priv->client_notify_capabilities_handler_id > 0) {
		g_signal_handler_disconnect (
			model->priv->client_cache,
			model->priv->client_notify_capabilities_handler_id);
		model->priv->client_notify_capabilities_handler_id = 0;
	}

	g_clear_object (&model->priv->client_cache);
	g_clear_object (&model->priv->book_client);

	g_clear_pointer (&model->priv->query_str, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_addressbook_model_parent_class)->dispose (object);
}

static void
addressbook_model_finalize (GObject *object)
{
	EAddressbookModel *self = E_ADDRESSBOOK_MODEL (object);

	g_ptr_array_free (self->priv->contacts, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_addressbook_model_parent_class)->finalize (object);
}

static void
addressbook_model_constructed (GObject *object)
{
	EAddressbookModel *model;
	EClientCache *client_cache;
	gulong handler_id;

	model = E_ADDRESSBOOK_MODEL (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_addressbook_model_parent_class)->constructed (object);

	client_cache = e_addressbook_model_get_client_cache (model);

	handler_id = g_signal_connect (
		client_cache, "client-notify::readonly",
		G_CALLBACK (addressbook_model_client_notify_readonly_cb),
		model);
	model->priv->client_notify_readonly_handler_id = handler_id;

	handler_id = g_signal_connect (
		client_cache, "client-notify::capabilities",
		G_CALLBACK (addressbook_model_client_notify_capabilities_cb),
		model);
	model->priv->client_notify_capabilities_handler_id = handler_id;
}

static void
e_addressbook_model_class_init (EAddressbookModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_model_set_property;
	object_class->get_property = addressbook_model_get_property;
	object_class->dispose = addressbook_model_dispose;
	object_class->finalize = addressbook_model_finalize;
	object_class->constructed = addressbook_model_constructed;

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			"EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_QUERY,
		g_param_spec_string (
			"query",
			"Query",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	signals[WRITABLE_STATUS] = g_signal_new (
		"writable_status",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, writable_status),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status_message",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_INT,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_INT);

	signals[BEFORE_SEARCH] = g_signal_new (
		"before-search",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		/* G_STRUCT_OFFSET (EAddressbookModelClass, before_search) */ 0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SEARCH_STARTED] = g_signal_new (
		"search_started",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, search_started),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SEARCH_RESULT] = g_signal_new (
		"search_result",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, search_result),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		G_TYPE_ERROR);

	signals[COUNT_CHANGED] = g_signal_new (
		"count-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, count_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CONTACT_ADDED] = g_signal_new (
		"contact_added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, contact_added),
		NULL, NULL,
		e_marshal_VOID__INT_INT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[CONTACTS_REMOVED] = g_signal_new (
		"contacts_removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, contacts_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[CONTACT_CHANGED] = g_signal_new (
		"contact_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, contact_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	signals[MODEL_CHANGED] = g_signal_new (
		"model_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, model_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[STOP_STATE_CHANGED] = g_signal_new (
		"stop_state_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookModelClass, stop_state_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_addressbook_model_init (EAddressbookModel *model)
{
	model->priv = e_addressbook_model_get_instance_private (model);
	model->priv->contacts = g_ptr_array_new ();
	model->priv->first_get_view = TRUE;
}

EAddressbookModel *
e_addressbook_model_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_ADDRESSBOOK_MODEL,
		"client-cache", client_cache, NULL);
}

EClientCache *
e_addressbook_model_get_client_cache (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);

	return model->priv->client_cache;
}

EContact *
e_addressbook_model_get_contact (EAddressbookModel *model,
                                 gint row)
{
	GPtrArray *array;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);

	array = model->priv->contacts;

	if (0 <= row && row < array->len)
		return e_contact_duplicate (array->pdata[row]);

	return NULL;
}

void
e_addressbook_model_stop (EAddressbookModel *model)
{
	const gchar *message;

	g_return_if_fail (E_IS_ADDRESSBOOK_MODEL (model));

	remove_book_view (model);

	message = _("Search Interrupted");
	g_signal_emit (model, signals[STOP_STATE_CHANGED], 0);
	g_signal_emit (model, signals[STATUS_MESSAGE], 0, message, -1);

	if (model->priv->remove_status_id == 0) {
		model->priv->remove_status_id =
			e_named_timeout_add_seconds (
			3, remove_status_cb, model);
	}
}

gboolean
e_addressbook_model_can_stop (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), FALSE);

	return model->priv->search_in_progress;
}

gint
e_addressbook_model_contact_count (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), 0);

	return model->priv->contacts->len;
}

EContact *
e_addressbook_model_contact_at (EAddressbookModel *model,
                                gint index)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < model->priv->contacts->len, NULL);

	return model->priv->contacts->pdata[index];
}

gint
e_addressbook_model_find (EAddressbookModel *model,
                          EContact *contact)
{
	GPtrArray *array;
	gint ii;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), -1);
	g_return_val_if_fail (E_IS_CONTACT (contact), -1);

	array = model->priv->contacts;
	for (ii = 0; ii < array->len; ii++) {
		EContact *candidate = array->pdata[ii];

		if (contact == candidate ||
		    g_strcmp0 (e_contact_get_const (contact, E_CONTACT_UID), e_contact_get_const (candidate, E_CONTACT_UID)) == 0)
			return ii;
	}

	return -1;
}

EBookClient *
e_addressbook_model_get_client (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);

	return model->priv->book_client;
}

void
e_addressbook_model_set_client (EAddressbookModel *model,
                                EBookClient *book_client)
{
	gboolean editable;

	g_return_if_fail (E_IS_ADDRESSBOOK_MODEL (model));
	if (book_client)
		g_return_if_fail (E_IS_BOOK_CLIENT (book_client));

	if (model->priv->book_client == book_client)
		return;

	if (model->priv->book_client != NULL)
		g_object_unref (model->priv->book_client);

	model->priv->book_client = book_client ? g_object_ref (book_client) : NULL;
	model->priv->first_get_view = TRUE;

	editable = book_client && !e_client_is_readonly (E_CLIENT (book_client));
	e_addressbook_model_set_editable (model, editable);

	if (book_client && model->priv->client_view_idle_id == 0)
		model->priv->client_view_idle_id = g_idle_add (
			(GSourceFunc) addressbook_model_idle_cb,
			g_object_ref (model));

	g_object_notify (G_OBJECT (model), "client");
}

gboolean
e_addressbook_model_get_editable (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), FALSE);

	return model->priv->editable;
}

void
e_addressbook_model_set_editable (EAddressbookModel *model,
                                  gboolean editable)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_MODEL (model));

	if (model->priv->editable != editable) {
		model->priv->editable = editable;

		g_signal_emit (
			model, signals[WRITABLE_STATUS], 0,
			model->priv->editable);

		g_object_notify (G_OBJECT (model), "editable");
	}
}

const gchar *
e_addressbook_model_get_query (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);

	return model->priv->query_str;
}

void
e_addressbook_model_set_query (EAddressbookModel *model,
                               const gchar *query)
{
	EBookQuery *book_query;

	g_return_if_fail (E_IS_ADDRESSBOOK_MODEL (model));

	if (query == NULL)
		book_query = e_book_query_any_field_contains ("");
	else
		book_query = e_book_query_from_string (query);

	/* also checks whether the query is a valid query string */
	if (!book_query)
		return;

	if (model->priv->query_str != NULL) {
		gchar *new_query;

		new_query = e_book_query_to_string (book_query);

		if (new_query && g_str_equal (model->priv->query_str, new_query)) {
			g_free (new_query);
			e_book_query_unref (book_query);
			return;
		}

		g_free (new_query);
	}

	g_free (model->priv->query_str);
	model->priv->query_str = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	if (model->priv->client_view_idle_id == 0)
		model->priv->client_view_idle_id = g_idle_add (
			(GSourceFunc) addressbook_model_idle_cb,
			g_object_ref (model));

	g_object_notify (G_OBJECT (model), "query");
}
