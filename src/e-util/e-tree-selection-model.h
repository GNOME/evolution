/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TREE_SELECTION_MODEL_H_
#define _E_TREE_SELECTION_MODEL_H_

#include <e-util/e-selection-model.h>
#include <e-util/e-sorter.h>
#include <e-util/e-tree-model.h>

/* Standard GObject macros */
#define E_TYPE_TREE_SELECTION_MODEL \
	(e_tree_selection_model_get_type ())
#define E_TREE_SELECTION_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_SELECTION_MODEL, ETreeSelectionModel))
#define E_TREE_SELECTION_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_SELECTION_MODEL, ETreeSelectionModelClass))
#define E_IS_TREE_SELECTION_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_SELECTION_MODEL))
#define E_IS_TREE_SELECTION_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_SELECTION_MODEL))
#define E_TREE_SELECTION_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_SELECTION_MODEL, ETreeSelectionModelClass))

G_BEGIN_DECLS

typedef void	(*ETreeForeachFunc)		(ETreePath path,
						 gpointer closure);

typedef struct _ETreeSelectionModel ETreeSelectionModel;
typedef struct _ETreeSelectionModelClass ETreeSelectionModelClass;
typedef struct _ETreeSelectionModelPrivate ETreeSelectionModelPrivate;

struct _ETreeSelectionModel {
	ESelectionModel parent;
	ETreeSelectionModelPrivate *priv;
};

struct _ETreeSelectionModelClass {
	ESelectionModelClass parent_class;
};

GType		e_tree_selection_model_get_type	(void) G_GNUC_CONST;
ESelectionModel *
		e_tree_selection_model_new	(void);
void		e_tree_selection_model_foreach	(ETreeSelectionModel *etsm,
						 ETreeForeachFunc callback,
						 gpointer closure);
void		e_tree_selection_model_select_single_path
						(ETreeSelectionModel *etsm,
						 ETreePath path);
void		e_tree_selection_model_select_paths
						(ETreeSelectionModel *etsm,
						 GPtrArray *paths);

void		e_tree_selection_model_add_to_selection
						(ETreeSelectionModel *etsm,
						 ETreePath path);
void		e_tree_selection_model_change_cursor
						(ETreeSelectionModel *etsm,
						 ETreePath path);
ETreePath	e_tree_selection_model_get_cursor
						(ETreeSelectionModel *etsm);
gint		e_tree_selection_model_get_selection_start_row
						(ETreeSelectionModel *etsm);
void		e_tree_selection_model_set_selection_start_row
						(ETreeSelectionModel *etsm,
						 gint row);
G_END_DECLS

#endif /* _E_TREE_SELECTION_MODEL_H_ */
