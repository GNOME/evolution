/*
 * e-account-manager.h
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

#ifndef E_ACCOUNT_MANAGER_H
#define E_ACCOUNT_MANAGER_H

#include <gtk/gtk.h>
#include <libedataserver/e-account-list.h>
#include <misc/e-account-tree-view.h>

/* Standard GObject macros */
#define E_TYPE_ACCOUNT_MANAGER \
	(e_account_manager_get_type ())
#define E_ACCOUNT_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACCOUNT_MANAGER, EAccountManager))
#define E_ACCOUNT_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACCOUNT_MANAGER, EAccountManagerClass))
#define E_IS_ACCOUNT_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACCOUNT_MANAGER))
#define E_IS_ACCOUNT_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACCOUNT_MANAGER))
#define E_ACCOUNT_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACCOUNT_MANAGER, EAccountManagerClass))

G_BEGIN_DECLS

typedef struct _EAccountManager EAccountManager;
typedef struct _EAccountManagerClass EAccountManagerClass;
typedef struct _EAccountManagerPrivate EAccountManagerPrivate;

struct _EAccountManager {
	GtkTable parent;
	EAccountManagerPrivate *priv;
};

struct _EAccountManagerClass {
	GtkTableClass parent_class;

	void		(*add_account)		(EAccountManager *manager);
	void		(*edit_account)		(EAccountManager *manager);
	void		(*delete_account)	(EAccountManager *manager);
};

GType		e_account_manager_get_type	(void);
GtkWidget *	e_account_manager_new		(EAccountList *account_list);
void		e_account_manager_add_account	(EAccountManager *manager);
void		e_account_manager_edit_account	(EAccountManager *manager);
void		e_account_manager_delete_account
						(EAccountManager *manager);
EAccountList *	e_account_manager_get_account_list
						(EAccountManager *manager);
void		e_account_manager_set_account_list
						(EAccountManager *manager,
						 EAccountList *account_list);
EAccountTreeView *
		e_account_manager_get_tree_view	(EAccountManager *manager);

G_END_DECLS

#endif /* E_ACCOUNT_MANAGER_H */
