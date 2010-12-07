/*
 * e-mail-account-tree-view.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_MAIL_ACCOUNT_TREE_VIEW_H
#define E_MAIL_ACCOUNT_TREE_VIEW_H

#include <gtk/gtk.h>
#include <libedataserver/e-source-registry.h>

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

	void	(*enable_selected)	(EMailAccountTreeView *tree_view);
	void	(*disable_selected)	(EMailAccountTreeView *tree_view);
};

GType		e_mail_account_tree_view_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_account_tree_view_new
					(ESourceRegistry *registry);
void		e_mail_account_tree_view_refresh
					(EMailAccountTreeView *tree_view);
void		e_mail_account_tree_view_enable_selected
					(EMailAccountTreeView *tree_view);
void		e_mail_account_tree_view_disable_selected
					(EMailAccountTreeView *tree_view);
ESourceRegistry *
		e_mail_account_tree_view_get_registry
					(EMailAccountTreeView *tree_view);
ESource *	e_mail_account_tree_view_get_selected_source
					(EMailAccountTreeView *tree_view);
void		e_mail_account_tree_view_set_selected_source
					(EMailAccountTreeView *tree_view,
					 ESource *source);

G_END_DECLS

#endif /* E_MAIL_ACCOUNT_TREE_VIEW_H */
