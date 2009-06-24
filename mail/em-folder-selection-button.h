/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FOLDER_SELECTION_BUTTON_H
#define EM_FOLDER_SELECTION_BUTTON_H

#include <gtk/gtk.h>

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
					(const gchar *title,
					 const gchar *caption);
const gchar *	em_folder_selection_button_get_caption
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_caption
					(EMFolderSelectionButton *button,
					 const gchar *caption);
gboolean	em_folder_selection_button_get_multiselect
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_multiselect
					(EMFolderSelectionButton *button,
					 gboolean multiselect);
const gchar *	em_folder_selection_button_get_selection
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_selection
					(EMFolderSelectionButton *button,
					 const gchar *uri);
GList *		em_folder_selection_button_get_selection_mult
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_selection_mult
					(EMFolderSelectionButton *button,
					 GList *uris);
const gchar *	em_folder_selection_button_get_title
					(EMFolderSelectionButton *button);
void		em_folder_selection_button_set_title
					(EMFolderSelectionButton *button,
					 const gchar *title);

G_END_DECLS

#endif /* EM_FOLDER_SELECTION_BUTTON_H */
