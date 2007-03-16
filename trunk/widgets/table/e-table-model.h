/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-model.h
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

#ifndef _E_TABLE_MODEL_H_
#define _E_TABLE_MODEL_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define E_TABLE_MODEL_TYPE        (e_table_model_get_type ())
#define E_TABLE_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_MODEL_TYPE, ETableModel))
#define E_TABLE_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_MODEL_TYPE, ETableModelClass))
#define E_IS_TABLE_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_MODEL_TYPE))
#define E_IS_TABLE_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_MODEL_TYPE))
#define E_TABLE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_MODEL_TYPE, ETableModelClass))

typedef struct {
	GObject   base;
} ETableModel;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	int         (*column_count)       (ETableModel *etm);
	int         (*row_count)          (ETableModel *etm);
	void        (*append_row)         (ETableModel *etm, ETableModel *source, int row);

	void       *(*value_at)           (ETableModel *etm, int col, int row);
	void        (*set_value_at)       (ETableModel *etm, int col, int row, const void *value);
	gboolean    (*is_cell_editable)   (ETableModel *etm, int col, int row);

	gboolean    (*has_save_id)        (ETableModel *etm);
	char       *(*get_save_id)        (ETableModel *etm, int row);

	gboolean    (*has_change_pending) (ETableModel *etm);

	/* Allocate a copy of the given value. */
	void       *(*duplicate_value)    (ETableModel *etm, int col, const void *value);
	/* Free an allocated value. */
	void        (*free_value)         (ETableModel *etm, int col, void *value);
	/* Return an allocated empty value. */
	void	   *(*initialize_value)   (ETableModel *etm, int col);
	/* Return TRUE if value is equivalent to an empty cell. */
	gboolean    (*value_is_empty)     (ETableModel *etm, int col, const void *value);
	/* Return an allocated string. */
	char       *(*value_to_string)    (ETableModel *etm, int col, const void *value);

	
	/*
	 * Signals
	 */

	/*
	 * These all come after the change has been made.
	 * No changes, cancel pre_change: no_change
	 * Major structural changes: model_changed
	 * Changes only in a row: row_changed
	 * Only changes in a cell: cell_changed
	 * A row inserted: row_inserted
	 * A row deleted: row_deleted
	 */
	void        (*model_pre_change)    (ETableModel *etm);

	void        (*model_no_change)     (ETableModel *etm);
	void        (*model_changed)       (ETableModel *etm);
	void        (*model_row_changed)   (ETableModel *etm, int row);
	void        (*model_cell_changed)  (ETableModel *etm, int col, int row);
	void        (*model_rows_inserted) (ETableModel *etm, int row, int count);
	void        (*model_rows_deleted)  (ETableModel *etm, int row, int count);
} ETableModelClass;

GType       e_table_model_get_type            (void);

/**/
int         e_table_model_column_count        (ETableModel *e_table_model);
const char *e_table_model_column_name         (ETableModel *e_table_model,
					       int          col);
int         e_table_model_row_count           (ETableModel *e_table_model);
void        e_table_model_append_row          (ETableModel *e_table_model,
					       ETableModel *source,
					       int          row);

/**/
void       *e_table_model_value_at            (ETableModel *e_table_model,
					       int          col,
					       int          row);
void        e_table_model_set_value_at        (ETableModel *e_table_model,
					       int          col,
					       int          row,
					       const void  *value);
gboolean    e_table_model_is_cell_editable    (ETableModel *e_table_model,
					       int          col,
					       int          row);

/**/
gboolean    e_table_model_has_save_id         (ETableModel *etm);
char       *e_table_model_get_save_id         (ETableModel *etm,
					       int          row);

/**/
gboolean    e_table_model_has_change_pending  (ETableModel *etm);


/**/
void       *e_table_model_duplicate_value     (ETableModel *e_table_model,
					       int          col,
					       const void  *value);
void        e_table_model_free_value          (ETableModel *e_table_model,
					       int          col,
					       void        *value);
void       *e_table_model_initialize_value    (ETableModel *e_table_model,
					       int          col);
gboolean    e_table_model_value_is_empty      (ETableModel *e_table_model,
					       int          col,
					       const void  *value);
char       *e_table_model_value_to_string     (ETableModel *e_table_model,
					       int          col,
					       const void  *value);

/*
 * Routines for emitting signals on the e_table
 */
void        e_table_model_pre_change          (ETableModel *e_table_model);
void        e_table_model_no_change           (ETableModel *e_table_model);
void        e_table_model_changed             (ETableModel *e_table_model);
void        e_table_model_row_changed         (ETableModel *e_table_model,
					       int          row);
void        e_table_model_cell_changed        (ETableModel *e_table_model,
					       int          col,
					       int          row);
void        e_table_model_rows_inserted       (ETableModel *e_table_model,
int          row,
int          count);
void        e_table_model_rows_deleted        (ETableModel *e_table_model,
int          row,
int          count);

/**/
void        e_table_model_row_inserted        (ETableModel *e_table_model,
int          row);
void        e_table_model_row_deleted         (ETableModel *e_table_model,
int          row);

void        e_table_model_freeze              (ETableModel *e_table_model);
void        e_table_model_thaw                (ETableModel *e_table_model);

G_END_DECLS

#endif /* _E_TABLE_MODEL_H_ */
