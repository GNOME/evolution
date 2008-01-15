/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef _E_CONTACT_LIST_MODEL_H_
#define _E_CONTACT_LIST_MODEL_H_

#include <gtk/gtk.h>
#include <libebook/e-contact.h>
#include <libebook/e-destination.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_LIST_MODEL \
	(e_contact_list_model_get_type ())
#define E_CONTACT_LIST_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_LIST_MODEL, EContactListModel))
#define E_CONTACT_LIST_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_LIST_MODEL, EContactListModelClass))
#define E_IS_CONTACT_LIST_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_LIST_MODEL))
#define E_IS_CONTACT_LIST_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_LIST_MODEL))
#define E_CONTACT_LIST_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_LIST_MODEL, EContactListModelClass))

G_BEGIN_DECLS

typedef struct _EContactListModel EContactListModel;
typedef struct _EContactListModelClass EContactListModelClass;

struct _EContactListModel {
	GtkListStore parent;
};

struct _EContactListModelClass {
	GtkListStoreClass parent_class;
};

GType		e_contact_list_model_get_type	(void);
GtkTreeModel *	e_contact_list_model_new	(void);
gboolean	e_contact_list_model_has_email	(EContactListModel *model,
						 const gchar *email);
void		e_contact_list_model_add_destination
						(EContactListModel *model,
						 EDestination *dest);
void		e_contact_list_model_add_email	(EContactListModel *model,
						 const gchar *email);
void		e_contact_list_model_add_contact(EContactListModel *model,
						 EContact *contact,
						 gint email_num);
void		e_contact_list_model_remove_row	(EContactListModel *model,
						 gint row);
void		e_contact_list_model_remove_all	(EContactListModel *model);
EDestination *	e_contact_list_model_get_destination
						(EContactListModel *model,
						 gint row);

G_END_DECLS

#endif /* _E_CONTACT_LIST_MODEL_H_ */
