/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-simple.h
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

#ifndef _E_TREE_SIMPLE_H_
#define _E_TREE_SIMPLE_H_

#include <gal/e-table/e-tree-model.h>
#include <gal/e-table/e-table-simple.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TREE_SIMPLE_TYPE        (e_tree_simple_get_type ())
#define E_TREE_SIMPLE(o)          (GTK_CHECK_CAST ((o), E_TREE_SIMPLE_TYPE, ETreeSimple))
#define E_TREE_SIMPLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_SIMPLE_TYPE, ETreeSimpleClass))
#define E_IS_TREE_SIMPLE(o)       (GTK_CHECK_TYPE ((o), E_TREE_SIMPLE_TYPE))
#define E_IS_TREE_SIMPLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_SIMPLE_TYPE))


typedef GdkPixbuf* (*ETreeSimpleIconAtFn)     (ETreeModel *etree, ETreePath *path, void *model_data);
typedef void*      (*ETreeSimpleValueAtFn)    (ETreeModel *etree, ETreePath *path, int col, void *model_data);
typedef void       (*ETreeSimpleSetValueAtFn) (ETreeModel *etree, ETreePath *path, int col, const void *val, void *model_data);
typedef gboolean   (*ETreeSimpleIsEditableFn) (ETreeModel *etree, ETreePath *path, int col, void *model_data);

typedef struct {
	ETreeModel parent;

	/* Table methods */
	ETableSimpleColumnCountFn     col_count;
	ETableSimpleDuplicateValueFn  duplicate_value;
	ETableSimpleFreeValueFn       free_value;
	ETableSimpleInitializeValueFn initialize_value;
	ETableSimpleValueIsEmptyFn    value_is_empty;
	ETableSimpleValueToStringFn   value_to_string;

	/* Tree methods */
	ETreeSimpleIconAtFn icon_at;
	ETreeSimpleValueAtFn value_at;
	ETreeSimpleSetValueAtFn set_value_at;
	ETreeSimpleIsEditableFn is_editable;

	gpointer model_data;
} ETreeSimple;

typedef struct {
	ETreeModelClass parent_class;
} ETreeSimpleClass;

GtkType e_tree_simple_get_type (void);

ETreeModel *e_tree_simple_new  (ETableSimpleColumnCountFn     col_count,
				ETableSimpleDuplicateValueFn  duplicate_value,
				ETableSimpleFreeValueFn       free_value,
				ETableSimpleInitializeValueFn initialize_value,
				ETableSimpleValueIsEmptyFn    value_is_empty,
				ETableSimpleValueToStringFn   value_to_string,
				ETreeSimpleIconAtFn           icon_at,
				ETreeSimpleValueAtFn          value_at,
				ETreeSimpleSetValueAtFn       set_value_at,
				ETreeSimpleIsEditableFn       is_editable,
				gpointer                      model_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TREE_SIMPLE_H_ */
