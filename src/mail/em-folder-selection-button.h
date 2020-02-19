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

#ifndef EM_FOLDER_SELECTION_BUTTON_H
#define EM_FOLDER_SELECTION_BUTTON_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_FOLDER_SELECTION_BUTTON \
	(em_folder_selection_button_get_type ())
#define EM_FOLDER_SELECTION_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButton))
#define EM_FOLDER_SELECTION_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButtonClass))
#define EM_IS_FOLDER_SELECTION_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FOLDER_SELECTION_BUTTON))
#define EM_IS_FOLDER_SELECTION_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), EM_TYPE_FOLDER_SELECTION_BUTTON))
#define EM_FOLDER_SELECTION_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButtonClass))

G_BEGIN_DECLS

typedef struct _EMFolderSelectionButton EMFolderSelectionButton;
typedef struct _EMFolderSelectionButtonClass EMFolderSelectionButtonClass;
typedef struct _EMFolderSelectionButtonPrivate EMFolderSelectionButtonPrivate;

struct _EMFolderSelectionButton {
	GtkButton parent;
	EMFolderSelectionButtonPrivate *priv;
};

struct _EMFolderSelectionButtonClass {
	GtkButtonClass parent_class;

	/* Signals.  */

	void	(*selected)	(EMFolderSelectionButton *button);
};

GType		em_folder_selection_button_get_type (void);
GtkWidget *	em_folder_selection_button_new
					(EMailSession *session,
					 const gchar *title,
					 const gchar *caption);
EMailSession *	em_folder_selection_button_get_session
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_session
					(EMFolderSelectionButton *button,
					 EMailSession *session);
gboolean	em_folder_selection_button_get_can_none
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_can_none
					(EMFolderSelectionButton *button,
					 gboolean can_none);
const gchar *	em_folder_selection_button_get_caption
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_caption
					(EMFolderSelectionButton *button,
					 const gchar *caption);
const gchar *	em_folder_selection_button_get_folder_uri
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_folder_uri
					(EMFolderSelectionButton *button,
					 const gchar *folder_uri);
CamelStore *	em_folder_selection_button_get_store
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_store
					(EMFolderSelectionButton *button,
					 CamelStore *store);
const gchar *	em_folder_selection_button_get_title
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_title
					(EMFolderSelectionButton *button,
					 const gchar *title);

G_END_DECLS

#endif /* EM_FOLDER_SELECTION_BUTTON_H */
