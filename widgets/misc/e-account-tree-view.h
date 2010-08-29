/*
 * e-account-tree-view.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ACCOUNT_TREE_VIEW_H
#define E_ACCOUNT_TREE_VIEW_H

#include <gtk/gtk.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

/* Standard GObject macros */
#define E_TYPE_ACCOUNT_TREE_VIEW \
	(e_account_tree_view_get_type ())
#define E_ACCOUNT_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACCOUNT_TREE_VIEW, EAccountTreeView))
#define E_ACCOUNT_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACCOUNT_TREE_VIEW, EAccountTreeViewClass))
#define E_IS_ACCOUNT_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACCOUNT_TREE_VIEW))
#define E_IS_ACCOUNT_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACCOUNT_TREE_VIEW))
#define E_ACCOUNT_TREE_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACCOUNT_TREE_VIEW, EAccountTreeViewClass))

G_BEGIN_DECLS

typedef struct _EAccountTreeView EAccountTreeView;
typedef struct _EAccountTreeViewClass EAccountTreeViewClass;
typedef struct _EAccountTreeViewPrivate EAccountTreeViewPrivate;

struct _EAccountTreeView {
	GtkTreeView parent;
	EAccountTreeViewPrivate *priv;
};

struct _EAccountTreeViewClass {
	GtkTreeViewClass parent_class;

	void		(*enable_account)	(EAccountTreeView *tree_view);
	void		(*disable_account)	(EAccountTreeView *tree_view);
	void		(*refreshed)		(EAccountTreeView *tree_view);
};

GType		e_account_tree_view_get_type	(void);
GtkWidget *	e_account_tree_view_new		(void);
void		e_account_tree_view_enable_account
						(EAccountTreeView *tree_view);
void		e_account_tree_view_disable_account
						(EAccountTreeView *tree_view);
EAccountList *	e_account_tree_view_get_account_list
						(EAccountTreeView *tree_view);
void		e_account_tree_view_set_account_list
						(EAccountTreeView *tree_view,
						 EAccountList *account_list);
EAccount *	e_account_tree_view_get_selected
						(EAccountTreeView *tree_view);
gboolean	e_account_tree_view_set_selected
						(EAccountTreeView *tree_view,
						 EAccount *account);

G_END_DECLS

#endif /* E_ACCOUNT_TREE_VIEW_H */
