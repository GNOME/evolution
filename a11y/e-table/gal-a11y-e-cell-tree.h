/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Tim Wo <tim.wo@sun.com>, Sun Microsystem Inc. 2003.
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#ifndef __GAL_A11Y_E_CELL_TREE_H__
#define __GAL_A11Y_E_CELL_TREE_H__

#include <glib-object.h>
#include <table/e-table-item.h>
#include <table/e-cell-tree.h>
#include "gal-a11y-e-cell.h"

#define GAL_A11Y_TYPE_E_CELL_TREE            (gal_a11y_e_cell_tree_get_type ())
#define GAL_A11Y_E_CELL_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_TREE, GalA11yECellTree))
#define GAL_A11Y_E_CELL_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_TYPE_E_CELL_TREE, GalA11yECellTreeClass))
#define GAL_A11Y_IS_E_CELL_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_TREE))
#define GAL_A11Y_IS_E_CELL_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_TREE))

typedef struct _GalA11yECellTree GalA11yECellTree;
typedef struct _GalA11yECellTreeClass GalA11yECellTreeClass;
typedef struct _GalA11yECellTreePrivate GalA11yECellTreePrivate;

/* This struct should actually be larger as this isn't what we derive from.
 * The GalA11yECellTreePrivate comes right after the parent class structure.
 **/
struct _GalA11yECellTree {
	GalA11yECell object;

	int model_row_changed_id;
};

struct _GalA11yECellTreeClass {
	GalA11yECellClass parent_class;
};


/* Standard Glib function */
GType      gal_a11y_e_cell_tree_get_type   (void);
AtkObject *gal_a11y_e_cell_tree_new	   (ETableItem *item,
					    ECellView  *cell_view,
					    AtkObject  *parent,
					    int         model_col,
					    int         view_col,
					    int         row);

#endif /* ! __GAL_A11Y_E_CELL_TREE_H__ */
