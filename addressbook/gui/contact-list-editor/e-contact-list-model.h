/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_CONTACT_LIST_MODEL_H_
#define _E_CONTACT_LIST_MODEL_H_

#include <gal/e-table/e-table-model.h>
#include <libebook/e-contact.h>
#include <libebook/e-destination.h>

G_BEGIN_DECLS

#define E_TYPE_CONTACT_LIST_MODEL        (e_contact_list_model_get_type ())
#define E_CONTACT_LIST_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_CONTACT_LIST_MODEL, EContactListModel))
#define E_CONTACT_LIST_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_CONTACT_LIST_MODEL, EContactListModelClass))
#define E_IS_CONTACT_LIST_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_CONTACT_LIST_MODEL))
#define E_IS_CONTACT_LIST_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_CONTACT_LIST_MODEL))

typedef struct _EContactListModel EContactListModel;
typedef struct _EContactListModelClass EContactListModelClass;

struct _EContactListModel {
	ETableModel parent;

	EDestination **data;
	int data_count;
	int data_alloc;
};


struct _EContactListModelClass {
	ETableModelClass parent_class;
};


GType      e_contact_list_model_get_type (void);
void         e_contact_list_model_construct (EContactListModel *model);
ETableModel *e_contact_list_model_new (void);

void         e_contact_list_model_add_destination (EContactListModel *model, EDestination *dest);
void         e_contact_list_model_add_email       (EContactListModel *model, const char *email);
void         e_contact_list_model_add_contact     (EContactListModel *model, EContact *contact, int email_num);

void	     e_contact_list_model_remove_row (EContactListModel *model, int row);
void         e_contact_list_model_remove_all (EContactListModel *model);

const EDestination *e_contact_list_model_get_destination (EContactListModel *model, int row);

G_END_DECLS

#endif /* _E_CONTACT_LIST_MODEL_H_ */
