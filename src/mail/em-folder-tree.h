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

#ifndef EM_FOLDER_TREE_H
#define EM_FOLDER_TREE_H

#include <gtk/gtk.h>
#include <e-util/e-util.h>
#include <mail/em-folder-tree-model.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_FOLDER_TREE \
	(em_folder_tree_get_type ())
#define EM_FOLDER_TREE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FOLDER_TREE, EMFolderTree))
#define EM_FOLDER_TREE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FOLDER_TREE, EMFolderTreeClass))
#define EM_IS_FOLDER_TREE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FOLDER_TREE))
#define EM_IS_FOLDER_TREE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FOLDER_TREE))
#define EM_FOLDER_TREE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FOLDER_TREE, EMFolderTreeClass))

G_BEGIN_DECLS

typedef struct _EMFolderTree EMFolderTree;
typedef struct _EMFolderTreeClass EMFolderTreeClass;
typedef struct _EMFolderTreePrivate EMFolderTreePrivate;

#define STATE_KEY_EXPANDED	"Expanded"

/* XXX Not sure this api is the best, but its the easiest
 *     to implement and will cover what we need. */
#define EMFT_EXCLUDE_NOSELECT CAMEL_FOLDER_NOSELECT
#define EMFT_EXCLUDE_NOINFERIORS CAMEL_FOLDER_NOINFERIORS
#define EMFT_EXCLUDE_VIRTUAL CAMEL_FOLDER_VIRTUAL
#define EMFT_EXCLUDE_SYSTEM CAMEL_FOLDER_SYSTEM
#define EMFT_EXCLUDE_VTRASH CAMEL_FOLDER_VTRASH

typedef gboolean	(*EMFTExcludeFunc)	(EMFolderTree *folder_tree,
						 GtkTreeModel *model,
						 GtkTreeIter *iter,
						 gpointer user_data);

struct _EMFolderTree {
	GtkTreeView parent;
	EMFolderTreePrivate *priv;
};

struct _EMFolderTreeClass {
	GtkTreeViewClass parent_class;

	/* signals */
	void		(*folder_activated)	(EMFolderTree *folder_tree,
						 CamelStore *store,
						 const gchar *folder_name);
	void		(*folder_selected)	(EMFolderTree *folder_tree,
						 CamelStore *store,
						 const gchar *folder_name,
						 CamelFolderInfoFlags flags);
	void		(*popup_event)		(EMFolderTree *folder_tree);
	void		(*hidden_key_event)	(EMFolderTree *emft, GdkEvent *event);
};

GType		em_folder_tree_get_type		(void);
GtkWidget *	em_folder_tree_new		(EMailSession *session,
						 EAlertSink *alert_sink);
GtkWidget *	em_folder_tree_new_with_model	(EMailSession *session,
						 EAlertSink *alert_sink,
						 EMFolderTreeModel *model);
EActivity *	em_folder_tree_new_activity	(EMFolderTree *folder_tree);
EAlertSink *	em_folder_tree_get_alert_sink	(EMFolderTree *folder_tree);
EMailSession *	em_folder_tree_get_session	(EMFolderTree *folder_tree);
gboolean	em_folder_tree_get_show_unread_count
						(EMFolderTree *folder_tree);
void		em_folder_tree_set_show_unread_count
						(EMFolderTree *folder_tree,
						 gboolean show_unread_count);
void		em_folder_tree_enable_drag_and_drop
						(EMFolderTree *folder_tree);
void		em_folder_tree_set_excluded	(EMFolderTree *folder_tree,
						 guint32 flags);
void		em_folder_tree_set_excluded_func
						(EMFolderTree *folder_tree,
						 EMFTExcludeFunc exclude,
						 gpointer data);
void		em_folder_tree_set_selected_list
						(EMFolderTree *folder_tree,
						 GList *list,
						 gboolean expand_only);
GList *		em_folder_tree_get_selected_uris
						(EMFolderTree *folder_tree);
GList *		em_folder_tree_get_selected_paths
						(EMFolderTree *folder_tree);
void		em_folder_tree_set_selected	(EMFolderTree *folder_tree,
						 const gchar *uri,
						 gboolean expand_only);
gboolean	em_folder_tree_select_next_path	(EMFolderTree *folder_tree,
						 gboolean skip_read_folders);
gboolean	em_folder_tree_select_prev_path	(EMFolderTree *folder_tree,
						 gboolean skip_read_folders);
void		em_folder_tree_edit_selected	(EMFolderTree *folder_tree);
gboolean	em_folder_tree_get_selected	(EMFolderTree *folder_tree,
						 CamelStore **out_store,
						 gchar **out_folder_name);
gboolean	em_folder_tree_store_root_selected
						(EMFolderTree *folder_tree,
						 CamelStore **out_store);
gchar *		em_folder_tree_get_selected_uri	(EMFolderTree *folder_tree);
CamelStore *	em_folder_tree_ref_selected_store
						(EMFolderTree *folder_tree);
gboolean	em_folder_tree_create_folder	(EMFolderTree *folder_tree,
						 const gchar *full_name,
						 const gchar *uri);
void		em_folder_tree_set_skip_double_click
						(EMFolderTree *folder_tree,
						 gboolean skip);
void		em_folder_tree_restore_state	(EMFolderTree *folder_tree,
						 GKeyFile *key_file);
void		em_folder_tree_set_selectable_widget
						(EMFolderTree *folder_tree,
						 GtkWidget *selectable);
void		em_folder_tree_select_store_when_added
						(EMFolderTree *folder_tree,
						 const gchar *store_uid);
const gchar *	em_folder_tree_get_new_message_text_color
						(EMFolderTree *self);
void		em_folder_tree_set_new_message_text_color
						(EMFolderTree *self,
						 const gchar *color_text);

G_END_DECLS

#endif /* EM_FOLDER_TREE_H */
