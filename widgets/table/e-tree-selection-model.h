/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_SELECTION_MODEL_H_
#define _E_TREE_SELECTION_MODEL_H_

#include <gtk/gtkobject.h>
#include <gal/util/e-sorter.h>
#include <gdk/gdktypes.h>
#include <gal/widgets/e-selection-model.h>
#include <gal/e-table/e-tree-model.h>
#include <gal/e-table/e-tree-sorted.h>
#include <gal/e-table/e-tree-table-adapter.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TREE_SELECTION_MODEL_TYPE        (e_tree_selection_model_get_type ())
#define E_TREE_SELECTION_MODEL(o)          (GTK_CHECK_CAST ((o), E_TREE_SELECTION_MODEL_TYPE, ETreeSelectionModel))
#define E_TREE_SELECTION_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_SELECTION_MODEL_TYPE, ETreeSelectionModelClass))
#define E_IS_TREE_SELECTION_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TREE_SELECTION_MODEL_TYPE))
#define E_IS_TREE_SELECTION_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_SELECTION_MODEL_TYPE))

typedef struct {
	ESelectionModel base;

	ETreeTableAdapter *etta;
	ETreeSorted *ets;
	ETreeModel *model;

	GHashTable *data;

	gboolean invert_selection;

	ETreePath cursor_path;
	gint cursor_col;
	gint selection_start_row;

	guint model_changed_id;
	guint model_row_inserted_id, model_row_deleted_id;

	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;
} ETreeSelectionModel;

typedef struct {
	ESelectionModelClass parent_class;
} ETreeSelectionModelClass;

GtkType          e_tree_selection_model_get_type  (void);
ESelectionModel *e_tree_selection_model_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TREE_SELECTION_MODEL_H_ */
