#ifndef _E_TABLE_SIMPLE_H_
#define _E_TABLE_SIMPLE_H_

#include "e-table-model.h"

typedef int         (*ETableSimpleColumnCountFn)    (ETableModel *etm, void *data);
typedef	const char *(*ETableSimpleColumnNameFn)     (ETableModel *etm, int col, void *data);
typedef	int         (*ETableSimpleRowCountFn)       (ETableModel *etm, void *data);
typedef	void       *(*ETableSimpleValueAtFn)        (ETableModel *etm, int col, int row, void *data);
typedef	void        (*ETableSimpleSetValueAtFn)     (ETableModel *etm, int col, int row, void *val, void *data);
typedef	gboolean    (*ETableSimpleIsCellEditableFn) (ETableModel *etm, int col, int row, void *data);
typedef int         (*ETableSimpleRowHeightFn)      (ETableModel *etm, int row, void *data);

typedef struct {
	ETableModel parent;

	ETableSimpleColumnCountFn    col_count;
	ETableSimpleColumnNameFn     col_name;
	ETableSimpleRowCountFn       row_count;
	ETableSimpleValueAtFn        value_at;
	ETableSimpleSetValueAtFn     set_value_at;
	ETableSimpleIsCellEditableFn is_cell_editable;
	ETableSimpleRowHeightFn      row_height;
	void *data;
} ETableSimple;

typedef struct {
	ETableModelClass parent_class;
} ETableSimpleClass;

GtkType e_table_simple_get_type (void);

ETableModel *e_table_simple_new (ETableSimpleColumnCountFn col_count,
				 ETableSimpleColumnNameFn col_name,
				 ETableSimpleRowCountFn row_count,
				 ETableSimpleValueAtFn value_at,
				 ETableSimpleSetValueAtFn set_value_at,
				 ETableSimpleIsCellEditableFn is_cell_editable,
				 void *data);

#endif /* _E_TABLE_SIMPLE_H_ */

