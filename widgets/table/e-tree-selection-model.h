/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TREE_SELECTION_MODEL_H_
#define _E_TREE_SELECTION_MODEL_H_

#include <glib-object.h>
#include <e-util/e-sorter.h>
#include <misc/e-selection-model.h>
#include <table/e-tree-model.h>

G_BEGIN_DECLS

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

G_END_DECLS

#endif /* _E_TREE_SELECTION_MODEL_H_ */
