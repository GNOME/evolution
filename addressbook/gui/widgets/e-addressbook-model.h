/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_ADDRESSBOOK_MODEL_H_
#define _E_ADDRESSBOOK_MODEL_H_

#include "e-table-model.h"
#include <ebook/e-book.h>
#include <ebook/e-book-view.h>
#include <ebook/e-card-simple.h>

#define E_ADDRESSBOOK_MODEL_TYPE        (e_addressbook_model_get_type ())
#define E_ADDRESSBOOK_MODEL(o)          (GTK_CHECK_CAST ((o), E_ADDRESSBOOK_MODEL_TYPE, EAddressbookModel))
#define E_ADDRESSBOOK_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_ADDRESSBOOK_MODEL_TYPE, EAddressbookModelClass))
#define E_IS_ADDRESSBOOK_MODEL(o)       (GTK_CHECK_TYPE ((o), E_ADDRESSBOOK_MODEL_TYPE))
#define E_IS_ADDRESSBOOK_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_ADDRESSBOOK_MODEL_TYPE))

/* Virtual Column list:
   0   Email
   1   Full Name
   2   Street
   3   Phone
*/

typedef struct {
	ETableModel parent;

	/* item specific fields */
	EBook *book;
	char *query;
	EBookView *book_view;

	int get_view_idle;

	ECardSimple **data;
	int data_count;

	int create_card_id, remove_card_id, modify_card_id;
} EAddressbookModel;


typedef struct {
	ETableModelClass parent_class;
} EAddressbookModelClass;


GtkType e_addressbook_model_get_type (void);
ETableModel *e_addressbook_model_new (void);

/* Returns object with ref count of 1. */
ECard *e_addressbook_model_get_card(EAddressbookModel *model,
				    int                row);

#endif /* _E_ADDRESSBOOK_MODEL_H_ */
