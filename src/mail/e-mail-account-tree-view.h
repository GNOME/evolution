/*
 * e-mail-account-tree-view.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
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
