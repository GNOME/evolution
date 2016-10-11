/*
 * e-mail-label-tree-view.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_LABEL_TREE_VIEW_H
#define E_MAIL_LABEL_TREE_VIEW_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_LABEL_TREE_VIEW \
	(e_mail_label_tree_view_get_type ())
#define E_MAIL_LABEL_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_LABEL_TREE_VIEW, EMailLabelTreeView))
#define E_MAIL_LABEL_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_LABEL_TREE_VIEW, EMailLabelTreeViewClass))
#define E_IS_MAIL_LABEL_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_LABEL_TREE_VIEW))
#define E_IS_MAIL_LABEL_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_LABEL_TREE_VIEW))
#define E_MAIL_LABEL_TREE_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_LABEL_TREE_VIEW, EMailLabelTreeViewClass))

G_BEGIN_DECLS

typedef struct _EMailLabelTreeView EMailLabelTreeView;
typedef struct _EMailLabelTreeViewClass EMailLabelTreeViewClass;
typedef struct _EMailLabelTreeViewPrivate EMailLabelTreeViewPrivate;

struct _EMailLabelTreeView {
	GtkTreeView parent;
	EMailLabelTreeViewPrivate *priv;
};

struct _EMailLabelTreeViewClass {
	GtkTreeViewClass parent_class;
};

GType		e_mail_label_tree_view_get_type		(void);
GtkWidget *	e_mail_label_tree_view_new		(void);

G_END_DECLS

#endif /* E_MAIL_LABEL_TREE_VIEW_H */
