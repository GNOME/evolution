/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef _E_TREE_SIMPLE_H_
#define _E_TREE_SIMPLE_H_

#include "e-tree-model.h"
#include "e-table-simple.h"

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

#endif /* _E_TREE_SIMPLE_H_ */
