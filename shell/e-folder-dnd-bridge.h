/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-dnd-bridge.h - Utility functions for handling dnd to Evolution
 * folders using the ShellComponentDnd interface.
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

/* The purpose of this file is to share the logic for dropping objects into
   folders between different widgets that handle the display of EStorageSets
   (namely, the shortcut bar and the folder bar).  */

#ifndef E_FOLDER_BAR_DND_BRIDGE_H
#define E_FOLDER_BAR_DND_BRIDGE_H

#include "e-storage-set.h"

#include <gtk/gtkwidget.h>
#include <gtk/gtkdnd.h>

#define E_FOLDER_DND_PATH_TARGET_TYPE "_EVOLUTION_PRIVATE_PATH"

gboolean  e_folder_dnd_bridge_motion         (GtkWidget        *widget,
					      GdkDragContext   *context,
					      unsigned int      time,
					      EStorageSet      *storage_set,
					      const char       *path);
gboolean  e_folder_dnd_bridge_drop           (GtkWidget        *widget,
					      GdkDragContext   *context,
					      unsigned int      time,
					      EStorageSet      *storage_set,
					      const char       *path);
void      e_folder_dnd_bridge_data_received  (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkSelectionData *selection_data,
					      unsigned int      time,
					      EStorageSet      *storage_set,
					      const char       *path);

#endif				/* E_FOLDER_BAR_DND_BRIDGE_H */
