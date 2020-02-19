/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FOLDER_SELECTOR_H
#define EM_FOLDER_SELECTOR_H

#include <gtk/gtk.h>
#include <mail/em-folder-tree.h>

/* Standard GObject macros */
#define EM_TYPE_FOLDER_SELECTOR \
	(em_folder_selector_get_type ())
#define EM_FOLDER_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelector))
#define EM_FOLDER_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorClass))
#define EM_IS_FOLDER_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FOLDER_SELECTOR))
#define EM_IS_FOLDER_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FOLDER_SELECTOR))
#define EM_FOLDER_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorClass))

G_BEGIN_DECLS

typedef struct _EMFolderSelector EMFolderSelector;
typedef struct _EMFolderSelectorClass EMFolderSelectorClass;
typedef struct _EMFolderSelectorPrivate EMFolderSelectorPrivate;

struct _EMFolderSelector {
	GtkDialog parent;
	EMFolderSelectorPrivate *priv;
};

struct _EMFolderSelectorClass {
	GtkDialogClass parent_class;

	/* Signals */
	void		(*folder_selected)	(EMFolderSelector *selector,
						 CamelStore *store,
						 const gchar *folder_name);
};

GType		em_folder_selector_get_type	(void);
GtkWidget *	em_folder_selector_new		(GtkWindow *parent,
						 EMFolderTreeModel *model);
gboolean	em_folder_selector_get_can_create
						(EMFolderSelector *selector);
void		em_folder_selector_set_can_create
						(EMFolderSelector *selector,
						 gboolean can_create);
gboolean	em_folder_selector_get_can_none	(EMFolderSelector *selector);
void		em_folder_selector_set_can_none	(EMFolderSelector *selector,
						 gboolean can_none);
const gchar *	em_folder_selector_get_caption	(EMFolderSelector *selector);
void		em_folder_selector_set_caption	(EMFolderSelector *selector,
						 const gchar *caption);
const gchar *	em_folder_selector_get_default_button_label
						(EMFolderSelector *selector);
void		em_folder_selector_set_default_button_label
						(EMFolderSelector *selector,
						 const gchar *button_label);
EMFolderTreeModel *
		em_folder_selector_get_model	(EMFolderSelector *selector);
GtkWidget *	em_folder_selector_get_content_area
						(EMFolderSelector *selector);
EMFolderTree *	em_folder_selector_get_folder_tree
						(EMFolderSelector *selector);
gboolean	em_folder_selector_get_selected	(EMFolderSelector *selector,
						 CamelStore **out_store,
						 gchar **out_folder_name);
void		em_folder_selector_set_selected	(EMFolderSelector *selector,
						 CamelStore *store,
						 const gchar *folder_name);
const gchar *	em_folder_selector_get_selected_uri
						(EMFolderSelector *selector);
EActivity *	em_folder_selector_new_activity	(EMFolderSelector *selector);
void		em_folder_selector_maybe_collapse_archive_folders
						(EMFolderSelector *selector);

G_END_DECLS

#endif /* EM_FOLDER_SELECTOR_H */
