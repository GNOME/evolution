/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* em-folder-selection-button.h
 *
 * Copyright (C) 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _EM_FOLDER_SELECTION_BUTTON_H_
#define _EM_FOLDER_SELECTION_BUTTON_H_

#include <gtk/gtkbutton.h>

#define EM_TYPE_FOLDER_SELECTION_BUTTON			(em_folder_selection_button_get_type ())
#define EM_FOLDER_SELECTION_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButton))
#define EM_FOLDER_SELECTION_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButtonClass))
#define EM_IS_FOLDER_SELECTION_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EM_TYPE_FOLDER_SELECTION_BUTTON))
#define EM_IS_FOLDER_SELECTION_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EM_TYPE_FOLDER_SELECTION_BUTTON))

typedef struct _EMFolderSelectionButton        EMFolderSelectionButton;
typedef struct _EMFolderSelectionButtonPrivate EMFolderSelectionButtonPrivate;
typedef struct _EMFolderSelectionButtonClass   EMFolderSelectionButtonClass;

struct _EMFolderSelectionButton {
	GtkButton parent;

	EMFolderSelectionButtonPrivate *priv;
};

struct _EMFolderSelectionButtonClass {
	GtkButtonClass parent_class;

	/* Signals.  */

	void  (* selected)  (EMFolderSelectionButton *button);
};

GType    em_folder_selection_button_get_type (void);

GtkWidget *em_folder_selection_button_new(const char *title, const char *caption);

void        em_folder_selection_button_set_selection(EMFolderSelectionButton *button, const char *uri);
const char *em_folder_selection_button_get_selection(EMFolderSelectionButton *button);

#endif /* _EM_FOLDER_SELECTION_BUTTON_H_ */
