/*
 * e-signature-tree-view.h
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

#ifndef E_SIGNATURE_TREE_VIEW_H
#define E_SIGNATURE_TREE_VIEW_H

#include <gtk/gtk.h>
#include <e-util/e-signature.h>
#include <e-util/e-signature-list.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_TREE_VIEW \
	(e_signature_tree_view_get_type ())
#define E_SIGNATURE_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_TREE_VIEW, ESignatureTreeView))
#define E_SIGNATURE_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_TREE_VIEW, ESignatureTreeViewClass))
#define E_IS_SIGNATURE_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_TREE_VIEW))
#define E_IS_SIGNATURE_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_TREE_VIEW))
#define E_SIGNATURE_TREE_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_TREE_VIEW, ESignatureTreeViewClass))

G_BEGIN_DECLS

typedef struct _ESignatureTreeView ESignatureTreeView;
typedef struct _ESignatureTreeViewClass ESignatureTreeViewClass;
typedef struct _ESignatureTreeViewPrivate ESignatureTreeViewPrivate;

struct _ESignatureTreeView {
	GtkTreeView parent;
	ESignatureTreeViewPrivate *priv;
};

struct _ESignatureTreeViewClass {
	GtkTreeViewClass parent_class;
};

GType		e_signature_tree_view_get_type	(void);
GtkWidget *	e_signature_tree_view_new	(void);
ESignatureList *e_signature_tree_view_get_signature_list
					(ESignatureTreeView *tree_view);
void		e_signature_tree_view_set_signature_list
					(ESignatureTreeView *tree_view,
					 ESignatureList *signature_list);
ESignature *	e_signature_tree_view_get_selected
					(ESignatureTreeView *tree_view);
gboolean	e_signature_tree_view_set_selected
					(ESignatureTreeView *tree_view,
					 ESignature *signature);

G_END_DECLS

#endif /* E_SIGNATURE_TREE_VIEW_H */
