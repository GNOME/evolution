/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "em-marshal.h"
#include "em-folder-tree-model.h"


/* GObject virtual method overrides */
static void em_folder_tree_model_class_init (EMFolderTreeModelClass *klass);
static void em_folder_tree_model_init (EMFolderTreeModel *model);
static void em_folder_tree_model_finalize (GObject *obj);

/* interface init methods */
static void tree_model_iface_init (GtkTreeModelIface *iface);
static void tree_drag_dest_iface_init (GtkTreeDragDestIface *iface);
static void tree_drag_source_iface_init (GtkTreeDragSourceIface *iface);

/* drag & drop iface methods */
static gboolean model_drag_data_received (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data);
static gboolean model_row_drop_possible  (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data);
static gboolean model_row_draggable      (GtkTreeDragSource *drag_source,
					  GtkTreePath *src_path);
static gboolean model_drag_data_get      (GtkTreeDragSource *drag_source,
					  GtkTreePath *src_path,
					  GtkSelectionData *selection_data);
static gboolean model_drag_data_delete   (GtkTreeDragSource *drag_source,
					  GtkTreePath *src_path);


enum {
	DRAG_DATA_RECEIVED,
	ROW_DROP_POSSIBLE,
	ROW_DRAGGABLE,
	DRAG_DATA_GET,
	DRAG_DATA_DELETE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };


static GtkTreeStore *parent_class = NULL;


GType
em_folder_tree_model_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeModelClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_model_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTreeModel),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_model_init,
		};
		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) tree_model_iface_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo drag_dest_info = {
			(GInterfaceInitFunc) tree_drag_dest_iface_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo drag_source_info = {
			(GInterfaceInitFunc) tree_drag_source_iface_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GTK_TYPE_TREE_STORE, "EMFolderTreeModel", &info, 0);
		
		g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_DRAG_DEST,
					     &drag_dest_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_DRAG_SOURCE,
					     &drag_source_info);
	}
	
	return type;
}


static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GTK_TYPE_TREE_STORE);
	
	object_class->finalize = em_folder_tree_model_finalize;
	
	/* signals */
	signals[DRAG_DATA_RECEIVED] =
		g_signal_new ("drag-data-received",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, drag_data_received),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[ROW_DROP_POSSIBLE] =
		g_signal_new ("row-drop-possible",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, row_drop_possible),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[ROW_DRAGGABLE] =
		g_signal_new ("row-draggable",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, row_draggable),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER,
			      G_TYPE_BOOLEAN, 1,
			      G_TYPE_POINTER);
	
	signals[DRAG_DATA_GET] =
		g_signal_new ("drag-data-get",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, drag_data_get),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER_POINTER,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);
	
	signals[DRAG_DATA_DELETE] =
		g_signal_new ("drag-data-delete",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, drag_data_delete),
			      NULL, NULL,
			      em_marshal_BOOLEAN__POINTER,
			      G_TYPE_BOOLEAN, 1,
			      G_TYPE_POINTER);
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	;
}

static void
em_folder_tree_model_finalize (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
	;
}

static void
tree_drag_dest_iface_init (GtkTreeDragDestIface *iface)
{
	iface->drag_data_received = model_drag_data_received;
	iface->row_drop_possible = model_row_drop_possible;
}

static void
tree_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = model_row_draggable;
	iface->drag_data_get = model_drag_data_get;
	iface->drag_data_delete = model_drag_data_delete;
}


static gboolean
model_drag_data_received (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_dest;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[DRAG_DATA_RECEIVED], 0, &retval, dest_path, selection_data);
	
	return retval;
}

static gboolean
model_row_drop_possible (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_dest;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[ROW_DROP_POSSIBLE], 0, &retval, dest_path, selection_data);
	
	return retval;
}

static gboolean
model_row_draggable (GtkTreeDragSource *drag_source, GtkTreePath *src_path)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_source;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[ROW_DRAGGABLE], 0, &retval, src_path);
	
	return retval;
}

static gboolean
model_drag_data_get (GtkTreeDragSource *drag_source, GtkTreePath *src_path, GtkSelectionData *selection_data)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_source;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[DRAG_DATA_GET], 0, &retval, src_path, selection_data);
	
	return retval;
}

static gboolean
model_drag_data_delete (GtkTreeDragSource *drag_source, GtkTreePath *src_path)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) drag_source;
	gboolean retval = FALSE;
	
	g_signal_emit (model, signals[DRAG_DATA_DELETE], 0, &retval, src_path);
	
	return retval;
}


EMFolderTreeModel *
em_folder_tree_model_new (int n_columns, GType *types)
{
	EMFolderTreeModel *model;
	
	model = g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
	gtk_tree_store_set_column_types ((GtkTreeStore *) model, n_columns, types);
	
	return model;
}
