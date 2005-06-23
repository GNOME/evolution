/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-selection-model.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_TREE_SELECTION_MODEL_H_
#define _E_TREE_SELECTION_MODEL_H_

#include <gdk/gdktypes.h>
#include <gtk/gtkobject.h>
#include <e-util/e-sorter.h>
#include <misc/e-selection-model.h>
#include <table/e-tree-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*ETreeForeachFunc) (ETreePath path,
				  gpointer closure);

typedef struct ETreeSelectionModelPriv ETreeSelectionModelPriv;

#define E_TREE_SELECTION_MODEL_TYPE        (e_tree_selection_model_get_type ())
#define E_TREE_SELECTION_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_SELECTION_MODEL_TYPE, ETreeSelectionModel))
#define E_TREE_SELECTION_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_SELECTION_MODEL_TYPE, ETreeSelectionModelClass))
#define E_IS_TREE_SELECTION_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_SELECTION_MODEL_TYPE))
#define E_IS_TREE_SELECTION_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_SELECTION_MODEL_TYPE))

typedef struct {
	ESelectionModel base;

	ETreeSelectionModelPriv *priv;
} ETreeSelectionModel;

typedef struct {
	ESelectionModelClass parent_class;
} ETreeSelectionModelClass;


GType            e_tree_selection_model_get_type            (void);
ESelectionModel *e_tree_selection_model_new                 (void);
void             e_tree_selection_model_foreach             (ETreeSelectionModel *etsm,
							     ETreeForeachFunc     callback,
							     gpointer             closure);
void             e_tree_selection_model_select_single_path  (ETreeSelectionModel *etsm,
							     ETreePath            path);
void		 e_tree_selection_model_select_paths        (ETreeSelectionModel *etsm, GPtrArray *paths);

void             e_tree_selection_model_add_to_selection    (ETreeSelectionModel *etsm,
							     ETreePath            path);
void             e_tree_selection_model_change_cursor       (ETreeSelectionModel *etsm,
							     ETreePath            path);
ETreePath        e_tree_selection_model_get_cursor          (ETreeSelectionModel *etsm);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TREE_SELECTION_MODEL_H_ */
