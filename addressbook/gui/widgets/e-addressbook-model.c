/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 1999 Ximian, Inc.
 */

#include <config.h>
#include "eab-marshal.h"
#include "e-addressbook-model.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gnome.h>
#include <gal/widgets/e-gui-utils.h>
#include "eab-gui-util.h"

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

/*
 * EABModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */
static void eab_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void eab_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);


enum {
	PROP_0,
	PROP_BOOK,
	PROP_QUERY,
	PROP_EDITABLE,
};

enum {
	WRITABLE_STATUS,
	STATUS_MESSAGE,
	SEARCH_STARTED,
	SEARCH_RESULT,
	FOLDER_BAR_MESSAGE,
	CONTACT_ADDED,
	CONTACT_REMOVED,
	CONTACT_CHANGED,
	MODEL_CHANGED,
	STOP_STATE_CHANGED,
	BACKEND_DIED,
	LAST_SIGNAL
};

static guint eab_model_signals [LAST_SIGNAL] = {0, };

static void
free_data (EABModel *model)
{
	if (model->data) {
		int i;

		for ( i = 0; i < model->data_count; i++ ) {
			g_object_unref (model->data[i]);
		}

		g_free(model->data);
		model->data = NULL;
		model->data_count = 0;
		model->allocated_count = 0;
	}
}

static void
remove_book_view(EABModel *model)
{
	if (model->book_view && model->create_contact_id)
		g_signal_handler_disconnect (model->book_view,
					     model->create_contact_id);
	if (model->book_view && model->remove_contact_id)
		g_signal_handler_disconnect (model->book_view,
					     model->remove_contact_id);
	if (model->book_view && model->modify_contact_id)
		g_signal_handler_disconnect (model->book_view,
					     model->modify_contact_id);
	if (model->book_view && model->status_message_id)
		g_signal_handler_disconnect (model->book_view,
					     model->status_message_id);
	if (model->book_view && model->sequence_complete_id)
		g_signal_handler_disconnect (model->book_view,
					     model->sequence_complete_id);

	model->create_contact_id = 0;
	model->remove_contact_id = 0;
	model->modify_contact_id = 0;
	model->status_message_id = 0;
	model->sequence_complete_id = 0;

	model->search_in_progress = FALSE;

	if (model->book_view) {
		e_book_view_stop (model->book_view);
		g_object_unref (model->book_view);
		model->book_view = NULL;
	}
}

static void
addressbook_dispose(GObject *object)
{
	EABModel *model = EAB_MODEL(object);

	remove_book_view(model);
	free_data (model);

	if (model->book) {
		if (model->writable_status_id)
			g_signal_handler_disconnect (model->book,
						     model->writable_status_id);
		model->writable_status_id = 0;

		if (model->backend_died_id)
			g_signal_handler_disconnect (model->book,
						     model->backend_died_id);
		model->backend_died_id = 0;

		g_object_unref (model->book);
		model->book = NULL;
	}

	if (model->query) {
		e_book_query_unref (model->query);
		model->query = NULL;
	}

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
update_folder_bar_message (EABModel *model)
{
	int count;
	char *message;

	count = model->data_count;

	switch (count) {
	case 0:
		message = g_strdup (_("No contacts"));
		break;
	default:
		message = g_strdup_printf (ngettext("%d contact", "%d contacts", count), count);
		break;
	}

	g_signal_emit (model,
		       eab_model_signals [FOLDER_BAR_MESSAGE], 0,
		       message);

	g_free (message);
}

static void
create_contact(EBookView *book_view,
	       const GList *contacts,
	       EABModel *model)
{
	int old_count = model->data_count;
	int length = g_list_length ((GList *)contacts);

	if (model->data_count + length > model->allocated_count) {
		while (model->data_count + length > model->allocated_count)
			model->allocated_count = model->allocated_count * 2 + 1;
		model->data = g_renew(EContact *, model->data, model->allocated_count);
	}

	for ( ; contacts; contacts = contacts->next) {
		model->data[model->data_count++] = contacts->data;
		g_object_ref (contacts->data);
	}

	g_signal_emit (model,
		       eab_model_signals [CONTACT_ADDED], 0,
		       old_count, model->data_count - old_count);

	update_folder_bar_message (model);
}

static void
remove_contact(EBookView *book_view,
	       GList *ids,
	       EABModel *model)
{
	/* XXX we should keep a hash around instead of this O(n*m) loop */
	int i = 0;
	GList *l;

	for (l = ids; l; l = l->next) {
		char *id = l->data;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_contact_get_const (model->data[i], E_CONTACT_UID), id) ) {
				g_object_unref (model->data[i]);
				memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (EContact *));
				model->data_count--;

				g_signal_emit (model,
					       eab_model_signals [CONTACT_REMOVED], 0,
					       i);

				break;
			}
		}
	}

	update_folder_bar_message (model);
}

static void
modify_contact(EBookView *book_view,
	       const GList *contacts,
	       EABModel *model)
{
	for ( ; contacts; contacts = contacts->next) {
		int i;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_contact_get_const(model->data[i], E_CONTACT_UID),
				     e_contact_get_const(E_CONTACT(contacts->data), E_CONTACT_UID)) ) {
				g_object_unref (model->data[i]);
				model->data[i] = e_contact_duplicate(E_CONTACT(contacts->data));
				g_signal_emit (model,
					       eab_model_signals [CONTACT_CHANGED], 0,
					       i);
				break;
			}
		}
	}
}

static void
status_message (EBookView *book_view,
		char* status,
		EABModel *model)
{
	g_signal_emit (model,
		       eab_model_signals [STATUS_MESSAGE], 0,
		       status);
}

static void
sequence_complete (EBookView *book_view,
		   EBookViewStatus status,
		   EABModel *model)
{
	model->search_in_progress = FALSE;
	status_message (book_view, NULL, model);
	g_signal_emit (model,
		       eab_model_signals [SEARCH_RESULT], 0,
		       status);
	g_signal_emit (model,
		       eab_model_signals [STOP_STATE_CHANGED], 0);
}

static void
writable_status (EBook *book,
		 gboolean writable,
		 EABModel *model)
{
	if (!model->editable_set) {
		model->editable = writable;

		g_signal_emit (model,
			       eab_model_signals [WRITABLE_STATUS], 0,
			       writable);
	}
}

static void
backend_died (EBook *book,
	      EABModel *model)
{
	g_signal_emit (model,
		       eab_model_signals [BACKEND_DIED], 0);
}

static void
eab_model_class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = addressbook_dispose;
	object_class->set_property   = eab_model_set_property;
	object_class->get_property   = eab_model_get_property;

	g_object_class_install_property (object_class, PROP_BOOK,
					 g_param_spec_object ("book",
							      _("Book"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_string ("query",
							      _("Query"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	eab_model_signals [WRITABLE_STATUS] =
		g_signal_new ("writable_status",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, writable_status),
			      NULL, NULL,
			      eab_marshal_NONE__BOOL,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	eab_model_signals [STATUS_MESSAGE] =
		g_signal_new ("status_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, status_message),
			      NULL, NULL,
			      eab_marshal_NONE__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	eab_model_signals [SEARCH_STARTED] =
		g_signal_new ("search_started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, search_started),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
	
	eab_model_signals [SEARCH_RESULT] =
		g_signal_new ("search_result",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, search_result),
			      NULL, NULL,
			      eab_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	
	eab_model_signals [FOLDER_BAR_MESSAGE] =
		g_signal_new ("folder_bar_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, folder_bar_message),
			      NULL, NULL,
			      eab_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	eab_model_signals [CONTACT_ADDED] =
		g_signal_new ("contact_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, contact_added),
			      NULL, NULL,
			      eab_marshal_NONE__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	eab_model_signals [CONTACT_REMOVED] =
		g_signal_new ("contact_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, contact_removed),
			      NULL, NULL,
			      eab_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	eab_model_signals [CONTACT_CHANGED] =
		g_signal_new ("contact_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, contact_changed),
			      NULL, NULL,
			      eab_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	eab_model_signals [MODEL_CHANGED] =
		g_signal_new ("model_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, model_changed),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	eab_model_signals [STOP_STATE_CHANGED] =
		g_signal_new ("stop_state_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, stop_state_changed),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	eab_model_signals [BACKEND_DIED] =
		g_signal_new ("backend_died",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABModelClass, backend_died),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
eab_model_init (GObject *object)
{
	EABModel *model = EAB_MODEL(object);
	model->book = NULL;
	model->query = e_book_query_any_field_contains ("");
	model->book_view = NULL;
	model->create_contact_id = 0;
	model->remove_contact_id = 0;
	model->modify_contact_id = 0;
	model->status_message_id = 0;
	model->writable_status_id = 0;
	model->backend_died_id = 0;
	model->sequence_complete_id = 0;
	model->data = NULL;
	model->data_count = 0;
	model->allocated_count = 0;
	model->search_in_progress = FALSE;
	model->editable = FALSE;
	model->editable_set = FALSE;
	model->first_get_view = TRUE;
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	EABModel *model = closure;

	if (status != E_BOOK_ERROR_OK) {
		eab_error_dialog (_("Error getting book view"), status);
		return;
	}

	remove_book_view (model);
	free_data (model);

	model->book_view = book_view;
	if (model->book_view)
		g_object_ref (model->book_view);
	model->create_contact_id = g_signal_connect(model->book_view,
						    "contacts_added",
						    G_CALLBACK (create_contact),
						    model);
	model->remove_contact_id = g_signal_connect(model->book_view,
						    "contacts_removed",
						    G_CALLBACK (remove_contact),
						    model);
	model->modify_contact_id = g_signal_connect(model->book_view,
						    "contacts_changed",
						    G_CALLBACK(modify_contact),
						    model);
	model->status_message_id = g_signal_connect(model->book_view,
						    "status_message",
						    G_CALLBACK(status_message),
						    model);
	model->sequence_complete_id = g_signal_connect(model->book_view,
						       "sequence_complete",
						       G_CALLBACK(sequence_complete),
						       model);

	model->search_in_progress = TRUE;
	g_signal_emit (model,
		       eab_model_signals [MODEL_CHANGED], 0);
	g_signal_emit (model,
		       eab_model_signals [SEARCH_STARTED], 0);
	g_signal_emit (model,
		       eab_model_signals [STOP_STATE_CHANGED], 0);

	e_book_view_start (model->book_view);
}

static void
get_view (EABModel *model)
{
	gboolean success;

	if (model->book && model->query) {
		ESource *source;
		const char *limit_str;
		int limit = -1;

		source = e_book_get_source (model->book);

		limit_str = e_source_get_property (source, "limit");
		if (limit_str && *limit_str)
			limit = atoi (limit_str);

		remove_book_view(model);
		free_data (model);

		if (model->first_get_view) {
			model->first_get_view = FALSE;

			if (e_book_check_static_capability (model->book, "do-initial-query")) {
				success = e_book_async_get_book_view (model->book, model->query, NULL, limit, book_view_loaded, model);
			} else {
				g_signal_emit (model,
					       eab_model_signals [MODEL_CHANGED], 0);
				g_signal_emit (model,
					       eab_model_signals [STOP_STATE_CHANGED], 0);
				return;
			}
		}
		else
			success = e_book_async_get_book_view (model->book, model->query, NULL, limit, book_view_loaded, model);

	}
}

static gboolean
get_view_idle (EABModel *model)
{
	model->book_view_idle_id = 0;
	get_view (model);
	g_object_unref (model);
	return FALSE;
}


EContact *
eab_model_get_contact(EABModel *model,
		      int       row)
{
	if (model->data && 0 <= row && row < model->data_count) {
		return e_contact_duplicate (model->data[row]);
	}
	return NULL;
}

static void
eab_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EABModel *model;
	gboolean need_get_book_view = FALSE;

	model = EAB_MODEL (object);
	
	switch (prop_id){
	case PROP_BOOK:
		if (model->book) {
			if (model->writable_status_id)
				g_signal_handler_disconnect (model->book,
							     model->writable_status_id);
			model->writable_status_id = 0;

			if (model->backend_died_id)
				g_signal_handler_disconnect (model->book,
							     model->backend_died_id);
			model->backend_died_id = 0;

			g_object_unref (model->book);
		}
		model->book = E_BOOK(g_value_get_object (value));
		if (model->book) {
			model->writable_status_id =
				g_signal_connect (model->book,
						  "writable_status",
						  G_CALLBACK (writable_status), model);
			model->backend_died_id =
				g_signal_connect (model->book,
						  "backend_died",
						  G_CALLBACK (backend_died), model);

			if (!model->editable_set) {
				model->editable = e_book_is_writable (model->book);

				g_signal_emit (model,
					       eab_model_signals [WRITABLE_STATUS], 0,
					       model->editable);
			}

			model->first_get_view = TRUE;
			g_object_ref (model->book);
			need_get_book_view = TRUE;
		}
		break;
	case PROP_QUERY:
		if (model->query)
			e_book_query_unref (model->query);
		model->query = e_book_query_from_string (g_value_get_string (value));
		need_get_book_view = TRUE;
		break;
	case PROP_EDITABLE:
		model->editable = g_value_get_boolean (value);
		model->editable_set = TRUE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	if (need_get_book_view) {
		if (!model->book_view_idle_id) {
			g_object_ref (model);
			model->book_view_idle_id = g_idle_add ((GSourceFunc)get_view_idle, model);
		}
	}

}

static void
eab_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EABModel *eab_model;

	eab_model = EAB_MODEL (object);

	switch (prop_id) {
	case PROP_BOOK:
		g_value_set_object (value, eab_model->book);
		break;
	case PROP_QUERY: {
		char *query_string = e_book_query_to_string (eab_model->query);
		g_value_set_string (value, query_string);
		break;
	}
	case PROP_EDITABLE:
		g_value_set_boolean (value, eab_model->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GType
eab_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EABModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_model_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EABModel", &info, 0);
	}

	return type;
}

EABModel*
eab_model_new (void)
{
	EABModel *et;

	et = g_object_new (EAB_TYPE_MODEL, NULL);
	
	return et;
}

void   eab_model_stop    (EABModel *model)
{
	remove_book_view(model);
	g_signal_emit (model,
		       eab_model_signals [STOP_STATE_CHANGED], 0);
	g_signal_emit (model,
		       eab_model_signals [STATUS_MESSAGE], 0,
		       "Search Interrupted.");
}

gboolean
eab_model_can_stop (EABModel *model)
{
	return model->search_in_progress;
}

void
eab_model_force_folder_bar_message (EABModel *model)
{
	update_folder_bar_message (model);
}

int
eab_model_contact_count (EABModel *model)
{
	return model->data_count;
}

const EContact *
eab_model_contact_at (EABModel *model, int index)
{
	return model->data[index];
}

gboolean
eab_model_editable (EABModel *model)
{
	return model->editable;
}

EBook *
eab_model_get_ebook (EABModel *model)
{
	return model->book;
}
