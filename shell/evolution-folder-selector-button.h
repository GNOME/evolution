/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-folder-selector-button.h
 *
 * Copyright (C) 2002 Ximian, Inc.
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
 */

#ifndef __EVOLUTION_FOLDER_SELECTOR_BUTTON_H__
#define __EVOLUTION_FOLDER_SELECTOR_BUTTON_H__

#include <gtk/gtkbutton.h>
#include "evolution-shell-client.h"

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON			(evolution_folder_selector_button_get_type ())
#define EVOLUTION_FOLDER_SELECTOR_BUTTON(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON, EvolutionFolderSelectorButton))
#define EVOLUTION_FOLDER_SELECTOR_BUTTON_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON, EvolutionFolderSelectorButtonClass))
#define EVOLUTION_IS_FOLDER_SELECTOR_BUTTON(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON))
#define EVOLUTION_IS_FOLDER_SELECTOR_BUTTON_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON))


typedef struct _EvolutionFolderSelectorButton        EvolutionFolderSelectorButton;
typedef struct _EvolutionFolderSelectorButtonPrivate EvolutionFolderSelectorButtonPrivate;
typedef struct _EvolutionFolderSelectorButtonClass   EvolutionFolderSelectorButtonClass;

struct _EvolutionFolderSelectorButton {
	GtkButton parent;

	EvolutionFolderSelectorButtonPrivate *priv;
};

struct _EvolutionFolderSelectorButtonClass {
	GtkButtonClass parent_class;

	/* signals */
	void (*popped_up) (EvolutionFolderSelectorButton *button);
	void (*selected) (EvolutionFolderSelectorButton *button, GNOME_Evolution_Folder *folder);
	void (*canceled) (EvolutionFolderSelectorButton *button);
};


GtkType    evolution_folder_selector_button_get_type  (void);
	   
void       evolution_folder_selector_button_construct (EvolutionFolderSelectorButton *folder_selector_button,
						       EvolutionShellClient          *shell_client,
						       const char                    *title,
						       const char                    *initial_uri,
						       const char                    *possible_types[]);
GtkWidget *evolution_folder_selector_button_new       (EvolutionShellClient          *shell_client,
						       const char                    *title,
						       const char                    *initial_uri,
						       const char                    *possible_types[]);

gboolean                evolution_folder_selector_button_set_uri    (EvolutionFolderSelectorButton *folder_selector_button,
								     const char                    *uri);
GNOME_Evolution_Folder *evolution_folder_selector_button_get_folder (EvolutionFolderSelectorButton *folder_selector_button);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_FOLDER_SELECTOR_BUTTON_H__ */
