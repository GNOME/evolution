/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_ADDRESSBOOK_MODEL_H_
#define _E_ADDRESSBOOK_MODEL_H_

#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-book-view.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#define E_ADDRESSBOOK_MODEL_TYPE        (e_addressbook_model_get_type ())
#define E_ADDRESSBOOK_MODEL(o)          (GTK_CHECK_CAST ((o), E_ADDRESSBOOK_MODEL_TYPE, EAddressbookModel))
#define E_ADDRESSBOOK_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_ADDRESSBOOK_MODEL_TYPE, EAddressbookModelClass))
#define E_IS_ADDRESSBOOK_MODEL(o)       (GTK_CHECK_TYPE ((o), E_ADDRESSBOOK_MODEL_TYPE))
#define E_IS_ADDRESSBOOK_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_ADDRESSBOOK_MODEL_TYPE))

typedef struct _EAddressbookModel EAddressbookModel;
typedef struct _EAddressbookModelClass EAddressbookModelClass;

struct _EAddressbookModel {
	GtkObject parent;

	/* item specific fields */
	EBook *book;
	char *query;
	EBookView *book_view;

	int get_view_idle;

	ECard **data;
	int data_count;
	int allocated_count;

	int create_card_id, remove_card_id, modify_card_id, status_message_id;

	guint editable : 1;
	guint first_get_view : 1;
};


struct _EAddressbookModelClass {
	GtkObjectClass parent_class;

	/*
	 * Signals
	 */
	void (*status_message) (EAddressbookModel *model, const gchar *message);
	void (*card_added)     (EAddressbookModel *model, gint index, gint count);
	void (*card_removed)   (EAddressbookModel *model, gint index);
	void (*card_changed)   (EAddressbookModel *model, gint index);
	void (*model_changed)  (EAddressbookModel *model);
};


GtkType            e_addressbook_model_get_type (void);
EAddressbookModel *e_addressbook_model_new (void);

/* Returns object with ref count of 1. */
ECard *e_addressbook_model_get_card  (EAddressbookModel *model,
				      int                row);
EBook *e_addressbook_model_get_ebook (EAddressbookModel *model);

void   e_addressbook_model_stop      (EAddressbookModel *model);


int          e_addressbook_model_card_count (EAddressbookModel *model);
ECard       *e_addressbook_model_card_at    (EAddressbookModel *model, int index);
gboolean     e_addressbook_model_editable   (EAddressbookModel *model);

#endif /* _E_ADDRESSBOOK_MODEL_H_ */
