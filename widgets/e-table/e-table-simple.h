/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SIMPLE_H_
#define _E_TABLE_SIMPLE_H_

#include "e-table-model.h"

#define E_TABLE_SIMPLE_TYPE        (e_table_simple_get_type ())
#define E_TABLE_SIMPLE(o)          (GTK_CHECK_CAST ((o), E_TABLE_SIMPLE_TYPE, ETableSimple))
#define E_TABLE_SIMPLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SIMPLE_TYPE, ETableSimpleClass))
#define E_IS_TABLE_SIMPLE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SIMPLE_TYPE))
#define E_IS_TABLE_SIMPLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SIMPLE_TYPE))

typedef int         (*ETableSimpleColumnCountFn)     (ETableModel *etm, void *data);
typedef	int         (*ETableSimpleRowCountFn)        (ETableModel *etm, void *data);
typedef	void       *(*ETableSimpleValueAtFn)         (ETableModel *etm, int col, int row, void *data);
typedef	void        (*ETableSimpleSetValueAtFn)      (ETableModel *etm, int col, int row, const void *val, void *data);
typedef	gboolean    (*ETableSimpleIsCellEditableFn)  (ETableModel *etm, int col, int row, void *data);
typedef void        (*ETableSimpleAppendRowFn)       (ETableModel *etm, ETableModel *model, int row, void *data);
typedef	void       *(*ETableSimpleDuplicateValueFn)  (ETableModel *etm, int col, const void *val, void *data);
typedef	void        (*ETableSimpleFreeValueFn)       (ETableModel *etm, int col, void *val, void *data);
typedef void       *(*ETableSimpleInitializeValueFn) (ETableModel *etm, int col, void *data);
typedef gboolean    (*ETableSimpleValueIsEmptyFn)    (ETableModel *etm, int col, const void *val, void *data);
typedef char       *(*ETableSimpleValueToStringFn)   (ETableModel *etm, int col, const void *val, void *data);

typedef struct {
	ETableModel parent;

	ETableSimpleColumnCountFn     col_count;
	ETableSimpleRowCountFn        row_count;
	ETableSimpleValueAtFn         value_at;
	ETableSimpleSetValueAtFn      set_value_at;
	ETableSimpleIsCellEditableFn  is_cell_editable;
	ETableSimpleDuplicateValueFn  duplicate_value;
	ETableSimpleFreeValueFn       free_value;
	ETableSimpleInitializeValueFn initialize_value;
	ETableSimpleValueIsEmptyFn    value_is_empty;
	ETableSimpleValueToStringFn   value_to_string;
	ETableSimpleAppendRowFn       append_row;
	void *data;
} ETableSimple;

typedef struct {
	ETableModelClass parent_class;
} ETableSimpleClass;

GtkType e_table_simple_get_type (void);

ETableModel *e_table_simple_new (ETableSimpleColumnCountFn col_count,
				 ETableSimpleRowCountFn row_count,
				 ETableSimpleValueAtFn value_at,
				 ETableSimpleSetValueAtFn set_value_at,
				 ETableSimpleIsCellEditableFn is_cell_editable,
				 ETableSimpleDuplicateValueFn duplicate_value,
				 ETableSimpleFreeValueFn free_value,
				 ETableSimpleInitializeValueFn initialize_value,
				 ETableSimpleValueIsEmptyFn value_is_empty,
				 ETableSimpleValueToStringFn value_to_string,
				 void *data);

#endif /* _E_TABLE_SIMPLE_H_ */

