/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_SELECTION_MODEL_H_
#define _E_TREE_SELECTION_MODEL_H_

#include <gdk/gdktypes.h>
#include <gtk/gtkobject.h>
#include <gal/util/e-sorter.h>
#include <gal/widgets/e-selection-model.h>
#include <gal/e-table/e-tree-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*ETreeForeachFunc) (ETreePath path,
				  gpointer closure);

typedef struct ETreeSelectionModelPriv ETreeSelectionModelPriv;

#define E_TREE_SELECTION_MODEL_TYPE        (e_tree_selection_model_get_type ())
#define E_TREE_SELECTION_MODEL(o)          (GTK_CHECK_CAST ((o), E_TREE_SELECTION_MODEL_TYPE, ETreeSelectionModel))
#define E_TREE_SELECTION_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_SELECTION_MODEL_TYPE, ETreeSelectionModelClass))
#define E_IS_TREE_SELECTION_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TREE_SELECTION_MODEL_TYPE))
#define E_IS_TREE_SELECTION_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_SELECTION_MODEL_TYPE))

typedef struct {
	ESelectionModel base;

	ETreeSelectionModelPriv *priv;
} ETreeSelectionModel;

typedef struct {
	ESelectionModelClass parent_class;
} ETreeSelectionModelClass;


GtkType          e_tree_selection_model_get_type            (void);
ESelectionModel *e_tree_selection_model_new                 (void);
void             e_tree_selection_model_foreach             (ETreeSelectionModel *etsm,
							     ETreeForeachFunc     callback,
							     gpointer             closure);
void             e_tree_selection_model_select_single_path  (ETreeSelectionModel *etsm,
							     ETreePath            path);
void             e_tree_selection_model_change_cursor       (ETreeSelectionModel *etsm,
							     ETreePath            path);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_TREE_SELECTION_MODEL_H_ */
