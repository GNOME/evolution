/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_ADDRESSBOOK_MODEL_H_
#define _E_ADDRESSBOOK_MODEL_H_

#include <glib.h>
#include <glib-object.h>
#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-book-view.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#define E_TYPE_ADDRESSBOOK_MODEL        (e_addressbook_model_get_type ())
#define E_ADDRESSBOOK_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_ADDRESSBOOK_MODEL, EAddressbookModel))
#define E_ADDRESSBOOK_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_ADDRESSBOOK_MODEL, EAddressbookModelClass))
#define E_IS_ADDRESSBOOK_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_ADDRESSBOOK_MODEL))
#define E_IS_ADDRESSBOOK_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_ADDRESSBOOK_MODEL))

typedef struct _EAddressbookModel EAddressbookModel;
typedef struct _EAddressbookModelClass EAddressbookModelClass;

struct _EAddressbookModel {
	GObject parent;

	/* item specific fields */
	EBook *book;
	char *query;
	EBookView *book_view;

	int get_view_idle;

	ECard **data;
	int data_count;
	int allocated_count;

	int create_card_id, remove_card_id, modify_card_id;
	int status_message_id, writable_status_id, sequence_complete_id;
	int backend_died_id;

	guint search_in_progress : 1;
	guint editable : 1;
	guint editable_set : 1;
	guint first_get_view : 1;
};


struct _EAddressbookModelClass {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	void (*writable_status)    (EAddressbookModel *model, gboolean writable);
	void (*search_started)     (EAddressbookModel *model);
	void (*search_result)      (EAddressbookModel *model, EBookViewStatus status);
	void (*status_message)     (EAddressbookModel *model, const gchar *message);
	void (*folder_bar_message) (EAddressbookModel *model, const gchar *message);
	void (*card_added)         (EAddressbookModel *model, gint index, gint count);
	void (*card_removed)       (EAddressbookModel *model, gint index);
	void (*card_changed)       (EAddressbookModel *model, gint index);
	void (*model_changed)      (EAddressbookModel *model);
	void (*stop_state_changed) (EAddressbookModel *model);
	void (*backend_died)       (EAddressbookModel *model);
};


GType              e_addressbook_model_get_type                  (void);
EAddressbookModel *e_addressbook_model_new                       (void);

/* Returns object with ref count of 1. */
ECard             *e_addressbook_model_get_card                  (EAddressbookModel *model,
								  int                row);
const ECard       *e_addressbook_model_peek_card                 (EAddressbookModel *model,
								  int                row);
EBook             *e_addressbook_model_get_ebook                 (EAddressbookModel *model);

void               e_addressbook_model_stop                      (EAddressbookModel *model);
gboolean           e_addressbook_model_can_stop                  (EAddressbookModel *model);

void               e_addressbook_model_force_folder_bar_message  (EAddressbookModel *model);

int                e_addressbook_model_card_count                (EAddressbookModel *model);
ECard             *e_addressbook_model_card_at                   (EAddressbookModel *model,
								  int                index);
gboolean           e_addressbook_model_editable                  (EAddressbookModel *model);

#endif /* _E_ADDRESSBOOK_MODEL_H_ */
