/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_CONTACT_LIST_MODEL_H_
#define _E_CONTACT_LIST_MODEL_H_

#include <gal/e-table/e-table-model.h>
#include "addressbook/backend/ebook/e-book.h"
#include "addressbook/backend/ebook/e-book-view.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#define E_CONTACT_LIST_MODEL_TYPE        (e_contact_list_model_get_type ())
#define E_CONTACT_LIST_MODEL(o)          (GTK_CHECK_CAST ((o), E_CONTACT_LIST_MODEL_TYPE, EContactListModel))
#define E_CONTACT_LIST_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CONTACT_LIST_MODEL_TYPE, EContactListModelClass))
#define E_IS_CONTACT_LIST_MODEL(o)       (GTK_CHECK_TYPE ((o), E_CONTACT_LIST_MODEL_TYPE))
#define E_IS_CONTACT_LIST_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CONTACT_LIST_MODEL_TYPE))

typedef struct _EContactListModel EContactListModel;
typedef struct _EContactListModelClass EContactListModelClass;

typedef struct {
	ECardSimple *simple;
	char *string;
} SimpleAndString;

struct _EContactListModel {
	ETableModel parent;

	SimpleAndString **simples;
	int simple_count;
	int simple_alloc;
	char **emails;
	int email_count;
	int email_alloc;
};


struct _EContactListModelClass {
	ETableModelClass parent_class;
};


GtkType      e_contact_list_model_get_type (void);
void         e_contact_list_model_construct (EContactListModel *model);
ETableModel *e_contact_list_model_new (void);

void         e_contact_list_model_add_email (EContactListModel *model, const char *email);
void         e_contact_list_model_add_card (EContactListModel *model, ECardSimple *simple);
void	     e_contact_list_model_remove_row (EContactListModel *model, int row);

#endif /* _E_CONTACT_LIST_MODEL_H_ */
