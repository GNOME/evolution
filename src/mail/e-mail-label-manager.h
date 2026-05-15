/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_LABEL_MANAGER_H
#define E_MAIL_LABEL_MANAGER_H

#include <gtk/gtk.h>
#include <mail/e-mail-label-list-store.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_LABEL_MANAGER \
	(e_mail_label_manager_get_type ())
#define E_MAIL_LABEL_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_LABEL_MANAGER, EMailLabelManager))
#define E_MAIL_LABEL_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_LABEL_MANAGER, EMailLabelManagerClass))
#define E_IS_MAIL_LABEL_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_LABEL_MANAGER))
#define E_IS_MAIL_LABEL_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_LABEL_MANAGER))
#define E_MAIL_LABEL_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_LABEL_MANAGER, EMailLabelManagerClass))

G_BEGIN_DECLS

typedef struct _EMailLabelManager EMailLabelManager;
typedef struct _EMailLabelManagerClass EMailLabelManagerClass;
typedef struct _EMailLabelManagerPrivate EMailLabelManagerPrivate;

struct _EMailLabelManager {
	GtkGrid parent;
	EMailLabelManagerPrivate *priv;
};

struct _EMailLabelManagerClass {
	GtkGridClass parent_class;

	void		(*add_label)		(EMailLabelManager *manager);
	void		(*edit_label)		(EMailLabelManager *manager);
	void		(*remove_label)		(EMailLabelManager *manager);
};

GType		e_mail_label_manager_get_type	(void);
GtkWidget *	e_mail_label_manager_new	(void);
void		e_mail_label_manager_add_label	(EMailLabelManager *manager);
void		e_mail_label_manager_edit_label	(EMailLabelManager *manager);
void		e_mail_label_manager_remove_label
						(EMailLabelManager *manager);
EMailLabelListStore *
		e_mail_label_manager_get_list_store
						(EMailLabelManager *manager);
void		e_mail_label_manager_set_list_store
						(EMailLabelManager *manager,
						 EMailLabelListStore *list_store);

G_END_DECLS

#endif /* E_MAIL_LABEL_MANAGER_H */
