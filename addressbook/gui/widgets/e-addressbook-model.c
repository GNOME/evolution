/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 1999 Ximian, Inc.
 */

#include <config.h>
#include "e-addressbook-marshal.h"
#include "e-addressbook-model.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gnome.h>
#include <gal/widgets/e-gui-utils.h>
#include "e-addressbook-util.h"
#include "e-addressbook-marshal.h"

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

/*
 * EAddressbookModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */
static void e_addressbook_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_addressbook_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);


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
	CARD_ADDED,
	CARD_REMOVED,
	CARD_CHANGED,
	MODEL_CHANGED,
	STOP_STATE_CHANGED,
	BACKEND_DIED,
	LAST_SIGNAL
};

#define COLS (E_CARD_SIMPLE_FIELD_LAST)

static guint e_addressbook_model_signals [LAST_SIGNAL] = {0, };

static void
free_data (EAddressbookModel *model)
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
remove_book_view(EAddressbookModel *model)
{
	if (model->book_view && model->create_card_id)
		g_signal_handler_disconnect (model->book_view,
					     model->create_card_id);
	if (model->book_view && model->remove_card_id)
		g_signal_handler_disconnect (model->book_view,
					     model->remove_card_id);
	if (model->book_view && model->modify_card_id)
		g_signal_handler_disconnect (model->book_view,
					     model->modify_card_id);
	if (model->book_view && model->status_message_id)
		g_signal_handler_disconnect (model->book_view,
					     model->status_message_id);
	if (model->book_view && model->sequence_complete_id)
		g_signal_handler_disconnect (model->book_view,
					     model->sequence_complete_id);

	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
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
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);

	if (model->get_view_idle) {
		g_source_remove(model->get_view_idle);
		model->get_view_idle = 0;
	}

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
		g_free (model->query);
		model->query = NULL;
	}

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
update_folder_bar_message (EAddressbookModel *model)
{
	int count;
	char *message;

	count = model->data_count;

	switch (count) {
	case 0:
		message = g_strdup (_("No cards"));
		break;
	case 1:
		message = g_strdup (_("1 card"));
		break;
	default:
		message = g_strdup_printf (_("%d cards"), count);
		break;
	}

	g_signal_emit (model,
		       e_addressbook_model_signals [FOLDER_BAR_MESSAGE], 0,
		       message);

	g_free (message);
}

static void
create_card(EBookView *book_view,
	    const GList *cards,
	    EAddressbookModel *model)
{
	int old_count = model->data_count;
	int length = g_list_length ((GList *)cards);

	if (model->data_count + length > model->allocated_count) {
		while (model->data_count + length > model->allocated_count)
			model->allocated_count = model->allocated_count * 2 + 1;
		model->data = g_renew(ECard *, model->data, model->allocated_count);
	}

	for ( ; cards; cards = cards->next) {
		model->data[model->data_count++] = cards->data;
		g_object_ref (cards->data);
	}

	g_signal_emit (model,
		       e_addressbook_model_signals [CARD_ADDED], 0,
		       old_count, model->data_count - old_count);

	update_folder_bar_message (model);
}

static void
remove_card(EBookView *book_view,
	    GList *ids,
	    EAddressbookModel *model)
{
	int i = 0;
	GList *l;

	for (l = ids; l; l = l->next) {
		char *id = l->data;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_get_id(model->data[i]), id) ) {
				g_object_unref (model->data[i]);
				memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
				model->data_count--;

				g_signal_emit (model,
					       e_addressbook_model_signals [CARD_REMOVED], 0,
					       i);

				break;
			}
		}
	}

	update_folder_bar_message (model);
}

static void
modify_card(EBookView *book_view,
	    const GList *cards,
	    EAddressbookModel *model)
{
	for ( ; cards; cards = cards->next) {
		int i;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_get_id(model->data[i]), e_card_get_id(E_CARD(cards->data))) ) {
				g_object_unref (model->data[i]);
				model->data[i] = e_card_duplicate(E_CARD(cards->data));
				g_signal_emit (model,
					       e_addressbook_model_signals [CARD_CHANGED], 0,
					       i);
				break;
			}
		}
	}
}

static void
status_message (EBookView *book_view,
		char* status,
		EAddressbookModel *model)
{
	g_signal_emit (model,
		       e_addressbook_model_signals [STATUS_MESSAGE], 0,
		       status);
}

static void
sequence_complete (EBookView *book_view,
		   EBookViewStatus status,
		   EAddressbookModel *model)
{
	model->search_in_progress = FALSE;
	status_message (book_view, NULL, model);
	g_signal_emit (model,
		       e_addressbook_model_signals [SEARCH_RESULT], 0,
		       status);
	g_signal_emit (model,
		       e_addressbook_model_signals [STOP_STATE_CHANGED], 0);
}

static void
writable_status (EBook *book,
		 gboolean writable,
		 EAddressbookModel *model)
{
	if (!model->editable_set) {
		model->editable = writable;

		g_signal_emit (model,
			       e_addressbook_model_signals [WRITABLE_STATUS], 0,
			       writable);
	}
}

static void
backend_died (EBook *book,
	      EAddressbookModel *model)
{
	g_signal_emit (model,
		       e_addressbook_model_signals [BACKEND_DIED], 0);
}

static void
e_addressbook_model_class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = addressbook_dispose;
	object_class->set_property   = e_addressbook_model_set_property;
	object_class->get_property   = e_addressbook_model_get_property;

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

	e_addressbook_model_signals [WRITABLE_STATUS] =
		g_signal_new ("writable_status",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, writable_status),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__BOOL,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	e_addressbook_model_signals [STATUS_MESSAGE] =
		g_signal_new ("status_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, status_message),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	e_addressbook_model_signals [SEARCH_STARTED] =
		g_signal_new ("search_started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, search_started),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
	
	e_addressbook_model_signals [SEARCH_RESULT] =
		g_signal_new ("search_result",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, search_result),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	
	e_addressbook_model_signals [FOLDER_BAR_MESSAGE] =
		g_signal_new ("folder_bar_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, folder_bar_message),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	e_addressbook_model_signals [CARD_ADDED] =
		g_signal_new ("card_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, card_added),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	e_addressbook_model_signals [CARD_REMOVED] =
		g_signal_new ("card_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, card_removed),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	e_addressbook_model_signals [CARD_CHANGED] =
		g_signal_new ("card_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, card_changed),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	e_addressbook_model_signals [MODEL_CHANGED] =
		g_signal_new ("model_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, model_changed),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_addressbook_model_signals [STOP_STATE_CHANGED] =
		g_signal_new ("stop_state_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, stop_state_changed),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_addressbook_model_signals [BACKEND_DIED] =
		g_signal_new ("backend_died",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookModelClass, backend_died),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
e_addressbook_model_init (GObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);
	model->book = NULL;
	model->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");
	model->book_view = NULL;
	model->get_view_idle = 0;
	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
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
	EAddressbookModel *model = closure;

	remove_book_view(model);

	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error getting book view"), status);
		return;
	}

	model->book_view = book_view;
	if (model->book_view)
		g_object_ref (model->book_view);
	model->create_card_id = g_signal_connect(model->book_view,
						 "card_added",
						 G_CALLBACK (create_card),
						 model);
	model->remove_card_id = g_signal_connect(model->book_view,
						 "card_removed",
						 G_CALLBACK (remove_card),
						 model);
	model->modify_card_id = g_signal_connect(model->book_view,
						 "card_changed",
						 G_CALLBACK(modify_card),
						 model);
	model->status_message_id = g_signal_connect(model->book_view,
						    "status_message",
						    G_CALLBACK(status_message),
						    model);
	model->sequence_complete_id = g_signal_connect(model->book_view,
						       "sequence_complete",
						       G_CALLBACK(sequence_complete),
						       model);

	free_data (model);

	model->search_in_progress = TRUE;
	g_signal_emit (model,
		       e_addressbook_model_signals [MODEL_CHANGED], 0);
	g_signal_emit (model,
		       e_addressbook_model_signals [SEARCH_STARTED], 0);
	g_signal_emit (model,
		       e_addressbook_model_signals [STOP_STATE_CHANGED], 0);
}

static gboolean
get_view (EAddressbookModel *model)
{
	if (model->book && model->query) {
		if (model->first_get_view) {
			if (e_book_check_static_capability (model->book, "do-initial-query")) {
				e_book_get_book_view (model->book, model->query, book_view_loaded, model);
			} else {
				remove_book_view(model);
				free_data (model);
				g_signal_emit (model,
					       e_addressbook_model_signals [MODEL_CHANGED], 0);
				g_signal_emit (model,
					       e_addressbook_model_signals [STOP_STATE_CHANGED], 0);
			}
			model->first_get_view = FALSE;
		}
		else
			e_book_get_book_view (model->book, model->query, book_view_loaded, model);
	}

	model->get_view_idle = 0;
	return FALSE;
}

ECard *
e_addressbook_model_get_card(EAddressbookModel *model,
			     int                row)
{
	if (model->data && 0 <= row && row < model->data_count) {
		ECard *card;
		card = e_card_duplicate (model->data[row]);
		return card;
	}
	return NULL;
}

const ECard *
e_addressbook_model_peek_card(EAddressbookModel *model,
			      int                row)
{
	if (model->data && 0 <= row && row < model->data_count) {
		return model->data[row];
	}
	return NULL;
}

static void
e_addressbook_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EAddressbookModel *model;

	model = E_ADDRESSBOOK_MODEL (object);
	
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
			model->first_get_view = TRUE;
			g_object_ref (model->book);
			if (model->get_view_idle == 0)
				model->get_view_idle = g_idle_add((GSourceFunc)get_view, model);
			model->writable_status_id =
				g_signal_connect (model->book,
						  "writable_status",
						  G_CALLBACK (writable_status), model);
			model->backend_died_id =
				g_signal_connect (model->book,
						  "backend_died",
						  G_CALLBACK (backend_died), model);
		}
		break;
	case PROP_QUERY:
		if (model->query)
			g_free(model->query);
		model->query = g_strdup(g_value_get_string (value));
		if (model->get_view_idle == 0)
			model->get_view_idle = g_idle_add((GSourceFunc)get_view, model);
		break;
	case PROP_EDITABLE:
		model->editable = g_value_get_boolean (value);
		model->editable_set = TRUE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_addressbook_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EAddressbookModel *e_addressbook_model;

	e_addressbook_model = E_ADDRESSBOOK_MODEL (object);

	switch (prop_id) {
	case PROP_BOOK:
		g_value_set_object (value, e_addressbook_model->book);
		break;
	case PROP_QUERY:
		g_value_set_string (value, g_strdup(e_addressbook_model->query));
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_addressbook_model->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GType
e_addressbook_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EAddressbookModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_addressbook_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_addressbook_model_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EAddressbookModel", &info, 0);
	}

	return type;
}

EAddressbookModel*
e_addressbook_model_new (void)
{
	EAddressbookModel *et;

	et = g_object_new (E_TYPE_ADDRESSBOOK_MODEL, NULL);
	
	return et;
}

void   e_addressbook_model_stop    (EAddressbookModel *model)
{
	remove_book_view(model);
	g_signal_emit (model,
		       e_addressbook_model_signals [STOP_STATE_CHANGED], 0);
	g_signal_emit (model,
		       e_addressbook_model_signals [STATUS_MESSAGE], 0,
		       "Search Interrupted.");
}

gboolean
e_addressbook_model_can_stop (EAddressbookModel *model)
{
	return model->search_in_progress;
}

void
e_addressbook_model_force_folder_bar_message (EAddressbookModel *model)
{
	update_folder_bar_message (model);
}

int
e_addressbook_model_card_count (EAddressbookModel *model)
{
	return model->data_count;
}

ECard *
e_addressbook_model_card_at (EAddressbookModel *model, int index)
{
	return model->data[index];
}

gboolean
e_addressbook_model_editable (EAddressbookModel *model)
{
	return model->editable;
}

EBook *
e_addressbook_model_get_ebook (EAddressbookModel *model)
{
	return model->book;
}
