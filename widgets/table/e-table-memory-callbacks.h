/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_MEMORY_CALLBACKS_H_
#define _E_TABLE_MEMORY_CALLBACKS_H_

#include <gal/e-table/e-table-memory.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TABLE_MEMORY_CALLBACKS_TYPE        (e_table_memory_callbacks_get_type ())
#define E_TABLE_MEMORY_CALLBACKS(o)          (GTK_CHECK_CAST ((o), E_TABLE_MEMORY_CALLBACKS_TYPE, ETableMemoryCalbacks))
#define E_TABLE_MEMORY_CALLBACKS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_MEMORY_CALLBACKS_TYPE, ETableMemoryCalbacksClass))
#define E_IS_TABLE_MEMORY_CALLBACKS(o)       (GTK_CHECK_TYPE ((o), E_TABLE_MEMORY_CALLBACKS_TYPE))
#define E_IS_TABLE_MEMORY_CALLBACKS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_MEMORY_CALLBACKS_TYPE))

typedef int         (*ETableMemoryCalbacksColumnCountFn)     (ETableModel *etm, void *data);
typedef void        (*ETableMemoryCalbacksAppendRowFn)       (ETableModel *etm, ETableModel *model, int row, void *data);

typedef	void       *(*ETableMemoryCalbacksValueAtFn)         (ETableModel *etm, int col, int row, void *data);
typedef	void        (*ETableMemoryCalbacksSetValueAtFn)      (ETableModel *etm, int col, int row, const void *val, void *data);
typedef	gboolean    (*ETableMemoryCalbacksIsCellEditableFn)  (ETableModel *etm, int col, int row, void *data);

typedef	void       *(*ETableMemoryCalbacksDuplicateValueFn)  (ETableModel *etm, int col, const void *val, void *data);
typedef	void        (*ETableMemoryCalbacksFreeValueFn)       (ETableModel *etm, int col, void *val, void *data);
typedef void       *(*ETableMemoryCalbacksInitializeValueFn) (ETableModel *etm, int col, void *data);
typedef gboolean    (*ETableMemoryCalbacksValueIsEmptyFn)    (ETableModel *etm, int col, const void *val, void *data);
typedef char       *(*ETableMemoryCalbacksValueToStringFn)   (ETableModel *etm, int col, const void *val, void *data);

typedef struct {
	ETableMemory parent;

	ETableMemoryCalbacksColumnCountFn     col_count;
	ETableMemoryCalbacksAppendRowFn       append_row;

	ETableMemoryCalbacksValueAtFn         value_at;
	ETableMemoryCalbacksSetValueAtFn      set_value_at;
	ETableMemoryCalbacksIsCellEditableFn  is_cell_editable;

	ETableMemoryCalbacksDuplicateValueFn  duplicate_value;
	ETableMemoryCalbacksFreeValueFn       free_value;
	ETableMemoryCalbacksInitializeValueFn initialize_value;
	ETableMemoryCalbacksValueIsEmptyFn    value_is_empty;
	ETableMemoryCalbacksValueToStringFn   value_to_string;
	void *data;
} ETableMemoryCalbacks;

typedef struct {
	ETableMemoryClass parent_class;
} ETableMemoryCalbacksClass;

GtkType e_table_memory_callbacks_get_type (void);

ETableModel *e_table_memory_callbacks_new (ETableMemoryCalbacksColumnCountFn col_count,

				 ETableMemoryCalbacksValueAtFn value_at,
				 ETableMemoryCalbacksSetValueAtFn set_value_at,
				 ETableMemoryCalbacksIsCellEditableFn is_cell_editable,

				 ETableMemoryCalbacksDuplicateValueFn duplicate_value,
				 ETableMemoryCalbacksFreeValueFn free_value,
				 ETableMemoryCalbacksInitializeValueFn initialize_value,
				 ETableMemoryCalbacksValueIsEmptyFn value_is_empty,
				 ETableMemoryCalbacksValueToStringFn value_to_string,
				 void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TABLE_MEMORY_CALLBACKS_H_ */

