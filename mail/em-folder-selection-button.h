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

#ifndef __EM_FOLDER_SELECTION_BUTTON_H__
#define __EM_FOLDER_SELECTION_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EM_TYPE_FOLDER_SELECTION_BUTTON            (em_folder_selection_button_get_type ())
#define EM_FOLDER_SELECTION_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButton))
#define EM_FOLDER_SELECTION_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButtonClass))
#define EM_IS_FOLDER_SELECTION_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EM_TYPE_FOLDER_SELECTION_BUTTON))
#define EM_IS_FOLDER_SELECTION_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EM_TYPE_FOLDER_SELECTION_BUTTON))

typedef struct _EMFolderSelectionButton        EMFolderSelectionButton;
typedef struct _EMFolderSelectionButtonClass   EMFolderSelectionButtonClass;

struct _EMFolderSelectionButton {
	GtkButton parent;

	struct _EMFolderSelectionButtonPrivate *priv;
};

struct _EMFolderSelectionButtonClass {
	GtkButtonClass parent_class;

	/* Signals.  */

	void  (* selected)  (EMFolderSelectionButton *button);
};

GType    em_folder_selection_button_get_type (void);

GtkWidget *em_folder_selection_button_new (const gchar *title, const gchar *caption);

void        em_folder_selection_button_set_selection (EMFolderSelectionButton *button, const gchar *uri);
const gchar *em_folder_selection_button_get_selection (EMFolderSelectionButton *button);

void   em_folder_selection_button_set_selection_mult (EMFolderSelectionButton *button, GList *uris);
GList *em_folder_selection_button_get_selection_mult (EMFolderSelectionButton *button);

void     em_folder_selection_button_set_multiselect (EMFolderSelectionButton *button, gboolean value);
gboolean em_folder_selection_button_get_multiselect (EMFolderSelectionButton *button);

G_END_DECLS

#endif /* __EM_FOLDER_SELECTION_BUTTON_H__ */
