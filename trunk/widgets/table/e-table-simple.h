/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-simple.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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

#ifndef _E_TABLE_SIMPLE_H_
#define _E_TABLE_SIMPLE_H_

#include <table/e-table-model.h>

G_BEGIN_DECLS

#define E_TABLE_SIMPLE_TYPE        (e_table_simple_get_type ())
#define E_TABLE_SIMPLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SIMPLE_TYPE, ETableSimple))
#define E_TABLE_SIMPLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SIMPLE_TYPE, ETableSimpleClass))
#define E_IS_TABLE_SIMPLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SIMPLE_TYPE))
#define E_IS_TABLE_SIMPLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SIMPLE_TYPE))
#define E_TABLE_SIMPLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_SIMPLE_TYPE, ETableSimpleClass))

typedef int         (*ETableSimpleColumnCountFn)     (ETableModel *etm, void *data);
typedef	int         (*ETableSimpleRowCountFn)        (ETableModel *etm, void *data);
typedef void        (*ETableSimpleAppendRowFn)       (ETableModel *etm, ETableModel *model, int row, void *data);

typedef	void       *(*ETableSimpleValueAtFn)         (ETableModel *etm, int col, int row, void *data);
typedef	void        (*ETableSimpleSetValueAtFn)      (ETableModel *etm, int col, int row, const void *val, void *data);
typedef	gboolean    (*ETableSimpleIsCellEditableFn)  (ETableModel *etm, int col, int row, void *data);

typedef gboolean    (*ETableSimpleHasSaveIdFn)       (ETableModel *etm, void *data);
typedef char       *(*ETableSimpleGetSaveIdFn)       (ETableModel *etm, int row, void *data);

typedef	void       *(*ETableSimpleDuplicateValueFn)  (ETableModel *etm, int col, const void *val, void *data);
typedef	void        (*ETableSimpleFreeValueFn)       (ETableModel *etm, int col, void *val, void *data);
typedef void       *(*ETableSimpleInitializeValueFn) (ETableModel *etm, int col, void *data);
typedef gboolean    (*ETableSimpleValueIsEmptyFn)    (ETableModel *etm, int col, const void *val, void *data);
typedef char       *(*ETableSimpleValueToStringFn)   (ETableModel *etm, int col, const void *val, void *data);

typedef struct {
	ETableModel parent;

	ETableSimpleColumnCountFn     col_count;
	ETableSimpleRowCountFn        row_count;
	ETableSimpleAppendRowFn       append_row;

	ETableSimpleValueAtFn         value_at;
	ETableSimpleSetValueAtFn      set_value_at;
	ETableSimpleIsCellEditableFn  is_cell_editable;

	ETableSimpleHasSaveIdFn       has_save_id;
	ETableSimpleGetSaveIdFn       get_save_id;

	ETableSimpleDuplicateValueFn  duplicate_value;
	ETableSimpleFreeValueFn       free_value;
	ETableSimpleInitializeValueFn initialize_value;
	ETableSimpleValueIsEmptyFn    value_is_empty;
	ETableSimpleValueToStringFn   value_to_string; 
	void *data;
} ETableSimple;

typedef struct {
	ETableModelClass parent_class;
} ETableSimpleClass;

GType        e_table_simple_get_type                 (void);
ETableModel *e_table_simple_new                      (ETableSimpleColumnCountFn      col_count,
						      ETableSimpleRowCountFn         row_count,
						      ETableSimpleAppendRowFn        append_row,
						      ETableSimpleValueAtFn          value_at,
						      ETableSimpleSetValueAtFn       set_value_at,
						      ETableSimpleIsCellEditableFn   is_cell_editable,
						      ETableSimpleHasSaveIdFn        has_save_id,
						      ETableSimpleGetSaveIdFn        get_save_id,
						      ETableSimpleDuplicateValueFn   duplicate_value,
						      ETableSimpleFreeValueFn        free_value,
						      ETableSimpleInitializeValueFn  initialize_value,
						      ETableSimpleValueIsEmptyFn     value_is_empty,
						      ETableSimpleValueToStringFn    value_to_string,
						      void                          *data);


/* Helper functions for if your values are all just strings. */
void        *e_table_simple_string_duplicate_value   (ETableModel                   *etm,
						      int                            col,
						      const void                    *val,
						      void                          *data);
void         e_table_simple_string_free_value        (ETableModel                   *etm,
						      int                            col,
						      void                          *val,
						      void                          *data);
void        *e_table_simple_string_initialize_value  (ETableModel                   *etm,
						      int                            col,
						      void                          *data);
gboolean     e_table_simple_string_value_is_empty    (ETableModel                   *etm,
						      int                            col,
						      const void                    *val,
						      void                          *data);
char        *e_table_simple_string_value_to_string   (ETableModel                   *etm,
						      int                            col,
						      const void                    *val,
						      void                          *data);

G_END_DECLS

#endif /* _E_TABLE_SIMPLE_H_ */
