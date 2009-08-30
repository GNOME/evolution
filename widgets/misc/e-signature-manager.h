/*
 * e-signature-manager.h
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

#ifndef E_SIGNATURE_MANAGER_H
#define E_SIGNATURE_MANAGER_H

#include <gtk/gtk.h>
#include <e-util/e-signature-list.h>
#include <misc/e-signature-editor.h>
#include <misc/e-signature-tree-view.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_MANAGER \
	(e_signature_manager_get_type ())
#define E_SIGNATURE_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_MANAGER, ESignatureManager))
#define E_SIGNATURE_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_MANAGER, ESignatureManagerClass))
#define E_IS_SIGNATURE_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_MANAGER))
#define E_IS_SIGNATURE_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_MANAGER))
#define E_SIGNATURE_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_MANAGER, ESignatureManagerClass))

G_BEGIN_DECLS

typedef struct _ESignatureManager ESignatureManager;
typedef struct _ESignatureManagerClass ESignatureManagerClass;
typedef struct _ESignatureManagerPrivate ESignatureManagerPrivate;

struct _ESignatureManager {
	GtkTable parent;
	ESignatureManagerPrivate *priv;
};

struct _ESignatureManagerClass {
	GtkTableClass parent_class;

	void		(*add_signature)	(ESignatureManager *manager);
	void		(*add_signature_script)	(ESignatureManager *manager);
	void		(*editor_created)	(ESignatureManager *manager,
						 ESignatureEditor *editor);
	void		(*edit_signature)	(ESignatureManager *manager);
	void		(*remove_signature)	(ESignatureManager *manager);
};

GType		e_signature_manager_get_type	(void);
GtkWidget *	e_signature_manager_new		(ESignatureList *signature_list);
void		e_signature_manager_add_signature
						(ESignatureManager *manager);
void		e_signature_manager_add_signature_script
						(ESignatureManager *manager);
void		e_signature_manager_edit_signature
						(ESignatureManager *manager);
void		e_signature_manager_remove_signature
						(ESignatureManager *manager);
gboolean	e_signature_manager_get_allow_scripts
						(ESignatureManager *manager);
void		e_signature_manager_set_allow_scripts
						(ESignatureManager *manager,
						 gboolean allow_scripts);
gboolean	e_signature_manager_get_prefer_html
						(ESignatureManager *manager);
void		e_signature_manager_set_prefer_html
						(ESignatureManager *manager,
						 gboolean prefer_html);
ESignatureList *e_signature_manager_get_signature_list
						(ESignatureManager *manager);
void		e_signature_manager_set_signature_list
						(ESignatureManager *manager,
						 ESignatureList *signature_list);
ESignatureTreeView *
		e_signature_manager_get_tree_view
						(ESignatureManager *manager);

#endif /* E_SIGNATURE_MANAGER_H */
