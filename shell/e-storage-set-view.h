/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set-view.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifndef __E_STORAGE_SET_VIEW_H__
#define __E_STORAGE_SET_VIEW_H__

#include <gal/e-table/e-tree.h>
#include <bonobo/bonobo-ui-container.h>
#include "e-storage-set.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_STORAGE_SET_VIEW			(e_storage_set_view_get_type ())
#define E_STORAGE_SET_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_STORAGE_SET_VIEW, EStorageSetView))
#define E_STORAGE_SET_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_STORAGE_SET_VIEW, EStorageSetViewClass))
#define E_IS_STORAGE_SET_VIEW(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_STORAGE_SET_VIEW))
#define E_IS_STORAGE_SET_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_STORAGE_SET_VIEW))


typedef gboolean (* EStorageSetViewHasCheckBoxFunc)  (EStorageSet *storage_set,
						      const char  *path,
						      void        *data);

typedef struct _EStorageSetView        EStorageSetView;
typedef struct _EStorageSetViewPrivate EStorageSetViewPrivate;
typedef struct _EStorageSetViewClass   EStorageSetViewClass;

struct _EStorageSetView {
	ETree parent;

	EStorageSetViewPrivate *priv;
};

struct _EStorageSetViewClass {
	ETreeClass parent_class;

	/* Signals.  */

	void (* folder_selected)  (EStorageSetView *storage_set_view,
				   const char *path);
	void (* folder_opened)    (EStorageSetView *storage_set_view,
				   const char *path);
	
	void (* folder_dragged) (EStorageSetView *view, const char *path, GdkDragContext *context,
				 GtkSelectionData *selection, guint info, guint time);
	void (* folder_receive_drop) (EStorageSetView *view, const char *path, GdkDragContext *context,
				      GtkSelectionData *selection, guint info, guint time);
	
	void (* folder_context_menu_popping_up)  (EStorageSetView *storage_set_view,
						  const char *path);
	void (* folder_context_menu_popped_down) (EStorageSetView *storage_set_view);
	void (* checkboxes_changed) (EStorageSetView *storage_set_view);
};


GtkType    e_storage_set_view_get_type          (void);

/* DON'T USE THIS. Use e_storage_set_new_view() instead. */
GtkWidget *e_storage_set_view_new        (EStorageSet       *storage_set,
					  BonoboUIContainer *ui_container);
void       e_storage_set_view_construct  (EStorageSetView   *storage_set_view,
					  EStorageSet       *storage_set,
					  BonoboUIContainer *ui_container);

EStorageSet *e_storage_set_view_get_storage_set  (EStorageSetView *storage_set_view);

void        e_storage_set_view_set_drag_types (EStorageSetView *view, const GtkTargetEntry *drag_types, int ntypes);
void        e_storage_set_view_set_drop_types (EStorageSetView *view, const GtkTargetEntry *drop_types, int ntypes);

void        e_storage_set_view_set_current_folder  (EStorageSetView *storage_set_view,
						    const char      *path);
const char *e_storage_set_view_get_current_folder  (EStorageSetView *storage_set_view);

void        e_storage_set_view_set_show_folders    (EStorageSetView *storage_set_view,
						    gboolean         show);
gboolean    e_storage_set_view_get_show_folders    (EStorageSetView *storage_set_view);

void      e_storage_set_view_set_show_checkboxes  (EStorageSetView                *storage_set_view,
						   gboolean                        show,
						   EStorageSetViewHasCheckBoxFunc  has_checkbox_func,
						   void                           *func_data);
gboolean  e_storage_set_view_get_show_checkboxes  (EStorageSetView                *storage_set_view);

void      e_storage_set_view_enable_search	  (EStorageSetView *storage_set_view,
						   gboolean         enable);

void      e_storage_set_view_set_checkboxes_list  (EStorageSetView                  *storage_set_view,
						   GSList                           *checkboxes);
GSList   *e_storage_set_view_get_checkboxes_list  (EStorageSetView                  *storage_set_view);

void      e_storage_set_view_set_allow_dnd  (EStorageSetView *storage_set_view,
					     gboolean         allow_dnd);
gboolean  e_storage_set_view_get_allow_dnd  (EStorageSetView *storage_set_view);
						    
const char *e_storage_set_view_get_right_click_path  (EStorageSetView *storage_set_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_STORAGE_SET_VIEW_H__ */
