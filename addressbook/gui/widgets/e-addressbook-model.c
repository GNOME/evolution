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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>
#include <string.h>
#include <glib/gi18n.h>
#include "e-addressbook-model.h"
#include <e-util/e-marshal.h>
#include <e-util/e-util.h>
#include "eab-gui-util.h"

#define E_ADDRESSBOOK_MODEL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_MODEL, EAddressbookModelPrivate))

struct _EAddressbookModelPrivate {
	EBook *book;
	EBookQuery *query;
	EBookView *book_view;
	guint book_view_idle_id;

	/* Query Results */
	GPtrArray *contacts;

	/* Signal Handler IDs */
	gulong create_contact_id;
	gulong remove_contact_id;
	gulong modify_contact_id;
	gulong status_message_id;
	gulong writable_status_id;
	gulong view_complete_id;
	gulong backend_died_id;

	guint search_in_progress	: 1;
	guint editable			: 1;
	guint editable_set		: 1;
	guint first_get_view		: 1;
};

enum {
	PROP_0,
	PROP_BOOK,
	PROP_EDITABLE,
	PROP_QUERY
};

enum {
	WRITABLE_STATUS,
	STATUS_MESSAGE,
	SEARCH_STARTED,
	SEARCH_RESULT,
	FOLDER_BAR_MESSAGE,
	CONTACT_ADDED,
	CONTACTS_REMOVED,
	CONTACT_CHANGED,
	MODEL_CHANGED,
	STOP_STATE_CHANGED,
	BACKEND_DIED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
free_data (EAddressbookModel *model)
{
	GPtrArray *array;

	array = model->priv->contacts;
	g_ptr_array_foreach (array, (GFunc) g_object_unref, NULL);
	g_ptr_array_set_size (array, 0);
}

static void
remove_book_view(EAddressbookModel *model)
{
	if (model->priv->book_view && model->priv->create_contact_id)
		g_signal_handler_disconnect (
			model->priv->book_view,
			model->priv->create_contact_id);
	if (model->priv->book_view && model->priv->remove_contact_id)
		g_signal_handler_disconnect (
			model->priv->book_view,
			model->priv->remove_contact_id);
	if (model->priv->book_view && model->priv->modify_contact_id)
		g_signal_handler_disconnect (
			model->priv->book_view,
			model->priv->modify_contact_id);
	if (model->priv->book_view && model->priv->status_message_id)
		g_signal_handler_disconnect (
			model->priv->book_view,
			model->priv->status_message_id);
	if (model->priv->book_view && model->priv->view_complete_id)
		g_signal_handler_disconnect (
			model->priv->book_view,
			model->priv->view_complete_id);

	model->priv->create_contact_id = 0;
	model->priv->remove_contact_id = 0;
	model->priv->modify_contact_id = 0;
	model->priv->status_message_id = 0;
	model->priv->view_complete_id = 0;

	model->priv->search_in_progress = FALSE;

	if (model->priv->book_view) {
		e_book_view_stop (model->priv->book_view);
		g_object_unref (model->priv->book_view);
		model->priv->book_view = NULL;
	}
}

static void
update_folder_bar_message (EAddressbookModel *model)
{
	guint count;
	gchar *message;

	count = model->priv->contacts->len;

	switch (count) {
	case 0:
		message = g_strdup (_("No contacts"));
		break;
	default:
		message = g_strdup_printf (
			ngettext ("%d contact", "%d contacts", count), count);
		break;
	}

	g_signal_emit (model, signals[FOLDER_BAR_MESSAGE], 0, message);

	g_free (message);
}

static void
create_contact (EBookView *book_view,
		const GList *contact_list,
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
	update_folder_bar_message (model);
}

static void
remove_contact(EBookView *book_view,
	       GList *ids,
	       EAddressbookModel *model)
{
	/* XXX we should keep a hash around instead of this O(n*m) loop */
	GList *iter;
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

	for (ii = 0; ii < indices->len; ii++) {
		gint index;

		index = g_array_index (indices, gint, ii);
		g_ptr_array_remove_index (array, index);
	}

	g_signal_emit (model, signals[CONTACTS_REMOVED], 0, indices);
	g_array_free (indices, FALSE);

	update_folder_bar_message (model);
}

static void
modify_contact(EBookView *book_view,
	       const GList *contact_list,
	       EAddressbookModel *model)
{
	GPtrArray *array;

	array = model->priv->contacts;

	while (contact_list != NULL) {
		EContact *contact = contact_list->data;
		const gchar *target_uid;
		gint ii;

		target_uid = e_contact_get_const (contact, E_CONTACT_UID);

		for (ii = 0; ii < array->len; ii++) {
			const gchar *uid;

			uid = e_contact_get_const (
				array->pdata[ii], E_CONTACT_UID);

			if (strcmp (uid, target_uid) != 0)
				continue;

			g_object_unref (array->pdata[ii]);
			contact = e_contact_duplicate (contact);
			array->pdata[ii] = contact;

			g_signal_emit (
				model, signals[CONTACT_CHANGED], 0, ii);
			break;
		}

		contact_list = contact_list->next;
	}
}

static void
status_message (EBookView *book_view,
		gchar * status,
		EAddressbookModel *model)
{
	g_signal_emit (model, signals[STATUS_MESSAGE], 0, status);
}

static void
view_complete (EBookView *book_view,
                   EBookViewStatus status,
		   const gchar *error_msg,
                   EAddressbookModel *model)
{
	model->priv->search_in_progress = FALSE;
	status_message (book_view, NULL, model);
	g_signal_emit (model, signals[SEARCH_RESULT], 0, status, error_msg);
	g_signal_emit (model, signals[STOP_STATE_CHANGED], 0);
}

static void
writable_status (EBook *book,
		 gboolean writable,
		 EAddressbookModel *model)
{
	if (!model->priv->editable_set) {
		model->priv->editable = writable;

		g_signal_emit (model, signals[WRITABLE_STATUS], 0, writable);
	}
}

static void
backend_died (EBook *book,
	      EAddressbookModel *model)
{
	g_signal_emit (model, signals[BACKEND_DIED], 0);
}

static void
book_view_loaded (EBook *book,
                  const GError *error,
                  EBookView *book_view,
                  gpointer closure)
{
	EAddressbookModel *model = closure;

	if (error) {
		eab_error_dialog (_("Error getting book view"), error);
		return;
	}

	remove_book_view (model);
	free_data (model);

	model->priv->book_view = book_view;
	if (model->priv->book_view)
		g_object_ref (model->priv->book_view);

	model->priv->create_contact_id = g_signal_connect (
		model->priv->book_view, "contacts-added",
		G_CALLBACK (create_contact), model);
	model->priv->remove_contact_id = g_signal_connect (
		model->priv->book_view, "contacts-removed",
		G_CALLBACK (remove_contact), model);
	model->priv->modify_contact_id = g_signal_connect (
		model->priv->book_view, "contacts-changed",
		G_CALLBACK (modify_contact), model);
	model->priv->status_message_id = g_signal_connect (
		model->priv->book_view, "status-message",
		G_CALLBACK (status_message), model);
	model->priv->view_complete_id = g_signal_connect (
		model->priv->book_view, "view-complete",
		G_CALLBACK (view_complete), model);

	model->priv->search_in_progress = TRUE;
	g_signal_emit (model, signals[MODEL_CHANGED], 0);
	g_signal_emit (model, signals[SEARCH_STARTED], 0);
	g_signal_emit (model, signals[STOP_STATE_CHANGED], 0);

	e_book_view_start (model->priv->book_view);
}

static gboolean
addressbook_model_idle_cb (EAddressbookModel *model)
{
	model->priv->book_view_idle_id = 0;

	if (model->priv->book && model->priv->query) {
		ESource *source;
		const gchar *limit_str;
		gint limit = -1;

		source = e_book_get_source (model->priv->book);

		limit_str = e_source_get_property (source, "limit");
		if (limit_str && *limit_str)
			limit = atoi (limit_str);

		remove_book_view(model);

		if (model->priv->first_get_view) {
			model->priv->first_get_view = FALSE;

			if (e_book_check_static_capability (model->priv->book, "do-initial-query")) {
				e_book_get_book_view_async (
					model->priv->book, model->priv->query,
					NULL, limit, book_view_loaded, model);
			} else {
				free_data (model);

				g_signal_emit (model,
					       signals[MODEL_CHANGED], 0);
				g_signal_emit (model,
					       signals[STOP_STATE_CHANGED], 0);
			}
		} else
			e_book_get_book_view_async (
				model->priv->book, model->priv->query,
				NULL, limit, book_view_loaded, model);

	}

	g_object_unref (model);

	return FALSE;
}

static void
addressbook_model_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BOOK:
			e_addressbook_model_set_book (
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
		case PROP_BOOK:
			g_value_set_object (
				value, e_addressbook_model_get_book (
				E_ADDRESSBOOK_MODEL (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, e_addressbook_model_get_editable (
				E_ADDRESSBOOK_MODEL (object)));
			return;

		case PROP_QUERY:
			g_value_take_string (
				value, e_addressbook_model_get_query (
				E_ADDRESSBOOK_MODEL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_model_dispose (GObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);

	remove_book_view (model);
	free_data (model);

	if (model->priv->book) {
		if (model->priv->writable_status_id)
			g_signal_handler_disconnect (
				model->priv->book,
				model->priv->writable_status_id);
		model->priv->writable_status_id = 0;

		if (model->priv->backend_died_id)
			g_signal_handler_disconnect (
				model->priv->book,
				model->priv->backend_died_id);
		model->priv->backend_died_id = 0;

		g_object_unref (model->priv->book);
		model->priv->book = NULL;
	}

	if (model->priv->query) {
		e_book_query_unref (model->priv->query);
		model->priv->query = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
addressbook_model_finalize (GObject *object)
{
	EAddressbookModelPrivate *priv;

	priv = E_ADDRESSBOOK_MODEL_GET_PRIVATE (object);

	g_ptr_array_free (priv->contacts, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
addressbook_model_class_init (EAddressbookModelClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAddressbookModelPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_model_set_property;
	object_class->get_property = addressbook_model_get_property;
	object_class->dispose = addressbook_model_dispose;
	object_class->finalize = addressbook_model_finalize;

	g_object_class_install_property (
		object_class,
		PROP_BOOK,
		g_param_spec_object (
			"book",
			"Book",
			NULL,
			E_TYPE_BOOK,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_QUERY,
		g_param_spec_string (
			"query",
			"Query",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[WRITABLE_STATUS] =
		g_signal_new ("writable_status",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, writable_status),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	signals[STATUS_MESSAGE] =
		g_signal_new ("status_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, status_message),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[SEARCH_STARTED] =
		g_signal_new ("search_started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, search_started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[SEARCH_RESULT] =
		g_signal_new ("search_result",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, search_result),
			      NULL, NULL,
			      e_marshal_VOID__UINT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);

	signals[FOLDER_BAR_MESSAGE] =
		g_signal_new ("folder_bar_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, folder_bar_message),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[CONTACT_ADDED] =
		g_signal_new ("contact_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, contact_added),
			      NULL, NULL,
			      e_marshal_NONE__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	signals[CONTACTS_REMOVED] =
		g_signal_new ("contacts_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, contacts_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[CONTACT_CHANGED] =
		g_signal_new ("contact_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, contact_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	signals[MODEL_CHANGED] =
		g_signal_new ("model_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, model_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[STOP_STATE_CHANGED] =
		g_signal_new ("stop_state_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, stop_state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[BACKEND_DIED] =
		g_signal_new ("backend_died",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, backend_died),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
addressbook_model_init (EAddressbookModel *model)
{
	model->priv = E_ADDRESSBOOK_MODEL_GET_PRIVATE (model);

	model->priv->contacts = g_ptr_array_new ();
	model->priv->first_get_view = TRUE;
}

GType
e_addressbook_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info =  {
			sizeof (EAddressbookModelClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) addressbook_model_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAddressbookModel),
			0,     /* n_preallocs */
			(GInstanceInitFunc) addressbook_model_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EAddressbookModel", &type_info, 0);
	}

	return type;
}

EAddressbookModel*
e_addressbook_model_new (void)
{
	return g_object_new (E_TYPE_ADDRESSBOOK_MODEL, NULL);
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
	g_signal_emit (model, signals[STATUS_MESSAGE], 0, message);
}

gboolean
e_addressbook_model_can_stop (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), FALSE);

	return model->priv->search_in_progress;
}

void
e_addressbook_model_force_folder_bar_message (EAddressbookModel *model)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_MODEL (model));

	update_folder_bar_message (model);
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

	return model->priv->contacts->pdata[index];
}

gint
e_addressbook_model_find (EAddressbookModel *model,
                          EContact *contact)
{
	GPtrArray *array;
	gint ii;

	/* XXX This searches for a particular EContact instance,
	 *     as opposed to an equivalent but possibly different
	 *     EContact instance.  Might have to revise this in
	 *     the future. */

	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), -1);
	g_return_val_if_fail (E_IS_CONTACT (contact), -1);

	array = model->priv->contacts;
	for (ii = 0; ii < array->len; ii++) {
		EContact *candidate = array->pdata[ii];

		if (contact == candidate)
			return ii;
	}

	return -1;
}

EBook *
e_addressbook_model_get_book (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);

	return model->priv->book;
}

void
e_addressbook_model_set_book (EAddressbookModel *model,
                              EBook *book)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_MODEL (model));
	g_return_if_fail (E_IS_BOOK (book));

	if (model->priv->book != NULL) {
		if (model->priv->book == book)
			return;

		if (model->priv->writable_status_id != 0)
			g_signal_handler_disconnect (
				model->priv->book,
				model->priv->writable_status_id);
		model->priv->writable_status_id = 0;

		if (model->priv->backend_died_id != 0)
			g_signal_handler_disconnect (
				model->priv->book,
				model->priv->backend_died_id);
		model->priv->backend_died_id = 0;

		g_object_unref (model->priv->book);
	}

	model->priv->book = g_object_ref (book);
	model->priv->first_get_view = TRUE;

	model->priv->writable_status_id = g_signal_connect (
		book, "writable-status",
		G_CALLBACK (writable_status), model);

	model->priv->backend_died_id = g_signal_connect (
		book, "backend-died",
		G_CALLBACK (backend_died), model);

	if (!model->priv->editable_set) {
		model->priv->editable = e_book_is_writable (book);
		g_signal_emit (
			model, signals[WRITABLE_STATUS], 0,
			model->priv->editable);
	}

	if (model->priv->book_view_idle_id == 0)
		model->priv->book_view_idle_id = g_idle_add (
			(GSourceFunc) addressbook_model_idle_cb,
			g_object_ref (model));

	g_object_notify (G_OBJECT (model), "book");
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

	model->priv->editable = editable;
	model->priv->editable_set = TRUE;

	g_object_notify (G_OBJECT (model), "editable");
}

gchar *
e_addressbook_model_get_query (EAddressbookModel *model)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_MODEL (model), NULL);

	return e_book_query_to_string (model->priv->query);
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

	if (model->priv->query != NULL) {
		gchar *old_query, *new_query;

		old_query = e_book_query_to_string (model->priv->query);
		new_query = e_book_query_to_string (book_query);

		if (old_query && new_query && g_str_equal (old_query, new_query)) {
			g_free (old_query);
			g_free (new_query);
			e_book_query_unref (book_query);
			return;
		}

		g_free (old_query);
		g_free (new_query);
		e_book_query_unref (model->priv->query);
	}

	model->priv->query = book_query;

	if (model->priv->book_view_idle_id == 0)
		model->priv->book_view_idle_id = g_idle_add (
			(GSourceFunc) addressbook_model_idle_cb,
			g_object_ref (model));

	g_object_notify (G_OBJECT (model), "query");
}
