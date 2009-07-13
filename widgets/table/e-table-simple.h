/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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

typedef gint         (*ETableSimpleColumnCountFn)     (ETableModel *etm, gpointer data);
typedef	gint         (*ETableSimpleRowCountFn)        (ETableModel *etm, gpointer data);
typedef void        (*ETableSimpleAppendRowFn)       (ETableModel *etm, ETableModel *model, gint row, gpointer data);

typedef	void       *(*ETableSimpleValueAtFn)         (ETableModel *etm, gint col, gint row, gpointer data);
typedef	void        (*ETableSimpleSetValueAtFn)      (ETableModel *etm, gint col, gint row, gconstpointer val, gpointer data);
typedef	gboolean    (*ETableSimpleIsCellEditableFn)  (ETableModel *etm, gint col, gint row, gpointer data);

typedef gboolean    (*ETableSimpleHasSaveIdFn)       (ETableModel *etm, gpointer data);
typedef gchar       *(*ETableSimpleGetSaveIdFn)       (ETableModel *etm, gint row, gpointer data);

typedef	void       *(*ETableSimpleDuplicateValueFn)  (ETableModel *etm, gint col, gconstpointer val, gpointer data);
typedef	void        (*ETableSimpleFreeValueFn)       (ETableModel *etm, gint col, gpointer val, gpointer data);
typedef void       *(*ETableSimpleInitializeValueFn) (ETableModel *etm, gint col, gpointer data);
typedef gboolean    (*ETableSimpleValueIsEmptyFn)    (ETableModel *etm, gint col, gconstpointer val, gpointer data);
typedef gchar       *(*ETableSimpleValueToStringFn)   (ETableModel *etm, gint col, gconstpointer val, gpointer data);

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
	gpointer data;
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
						      gint                            col,
						      const void                    *val,
						      void                          *data);
void         e_table_simple_string_free_value        (ETableModel                   *etm,
						      gint                            col,
						      void                          *val,
						      void                          *data);
void        *e_table_simple_string_initialize_value  (ETableModel                   *etm,
						      gint                            col,
						      void                          *data);
gboolean     e_table_simple_string_value_is_empty    (ETableModel                   *etm,
						      gint                            col,
						      const void                    *val,
						      void                          *data);
gchar        *e_table_simple_string_value_to_string   (ETableModel                   *etm,
						      gint                            col,
						      const void                    *val,
						      void                          *data);

G_END_DECLS

#endif /* _E_TABLE_SIMPLE_H_ */
