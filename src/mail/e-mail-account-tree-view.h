/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_ACCOUNT_TREE_VIEW_H
#define E_MAIL_ACCOUNT_TREE_VIEW_H

#include <gtk/gtk.h>
#include <mail/e-mail-account-store.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_ACCOUNT_TREE_VIEW \
	(e_mail_account_tree_view_get_type ())
#define E_MAIL_ACCOUNT_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_ACCOUNT_TREE_VIEW, EMailAccountTreeView))
#define E_MAIL_ACCOUNT_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_ACCOUNT_TREE_VIEW, EMailAccountTreeViewClass))
#define E_IS_MAIL_ACCOUNT_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_ACCOUNT_TREE_VIEW))
#define E_IS_MAIL_ACCOUNT_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_ACCOUNT_TREE_VIEW))
#define E_MAIL_ACCOUNT_TREE_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_ACCOUNT_TREE_VIEW, EMailAccountTreeViewClass))

G_BEGIN_DECLS

typedef struct _EMailAccountTreeView EMailAccountTreeView;
typedef struct _EMailAccountTreeViewClass EMailAccountTreeViewClass;
typedef struct _EMailAccountTreeViewPrivate EMailAccountTreeViewPrivate;

struct _EMailAccountTreeView {
	GtkTreeView parent;
	EMailAccountTreeViewPrivate *priv;
};

struct _EMailAccountTreeViewClass {
	GtkTreeViewClass parent_class;

	/* Signals */
	void		(*enable)	(EMailAccountTreeView *tree_view);
	void		(*disable)	(EMailAccountTreeView *tree_view);
};

GType		e_mail_account_tree_view_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_account_tree_view_new
					(EMailAccountStore *store);
CamelService *	e_mail_account_tree_view_get_selected_service
					(EMailAccountTreeView *tree_view);
void		e_mail_account_tree_view_set_selected_service
					(EMailAccountTreeView *tree_view,
					 CamelService *service);

G_END_DECLS

#endif /* E_MAIL_ACCOUNT_TREE_VIEW_H */
