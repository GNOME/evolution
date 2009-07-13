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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
	gint         (*column_count)       (ETableModel *etm);
	gint         (*row_count)          (ETableModel *etm);
	void        (*append_row)         (ETableModel *etm, ETableModel *source, gint row);

	void       *(*value_at)           (ETableModel *etm, gint col, gint row);
	void        (*set_value_at)       (ETableModel *etm, gint col, gint row, gconstpointer value);
	gboolean    (*is_cell_editable)   (ETableModel *etm, gint col, gint row);

	gboolean    (*has_save_id)        (ETableModel *etm);
	gchar      *(*get_save_id)        (ETableModel *etm, gint row);

	gboolean    (*has_change_pending) (ETableModel *etm);

	/* Allocate a copy of the given value. */
	void       *(*duplicate_value)    (ETableModel *etm, gint col, gconstpointer value);
	/* Free an allocated value. */
	void        (*free_value)         (ETableModel *etm, gint col, gpointer value);
	/* Return an allocated empty value. */
	void	   *(*initialize_value)   (ETableModel *etm, gint col);
	/* Return TRUE if value is equivalent to an empty cell. */
	gboolean    (*value_is_empty)     (ETableModel *etm, gint col, gconstpointer value);
	/* Return an allocated string. */
	gchar       *(*value_to_string)    (ETableModel *etm, gint col, gconstpointer value);

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
	void        (*model_row_changed)   (ETableModel *etm, gint row);
	void        (*model_cell_changed)  (ETableModel *etm, gint col, gint row);
	void        (*model_rows_inserted) (ETableModel *etm, gint row, gint count);
	void        (*model_rows_deleted)  (ETableModel *etm, gint row, gint count);
} ETableModelClass;

GType       e_table_model_get_type            (void);

/**/
gint         e_table_model_column_count        (ETableModel *e_table_model);
const gchar *e_table_model_column_name         (ETableModel *e_table_model,
					       gint          col);
gint         e_table_model_row_count           (ETableModel *e_table_model);
void        e_table_model_append_row          (ETableModel *e_table_model,
					       ETableModel *source,
					       gint          row);

/**/
void       *e_table_model_value_at            (ETableModel *e_table_model,
					       gint          col,
					       gint          row);
void        e_table_model_set_value_at        (ETableModel *e_table_model,
					       gint          col,
					       gint          row,
					       const void  *value);
gboolean    e_table_model_is_cell_editable    (ETableModel *e_table_model,
					       gint          col,
					       gint          row);

/**/
gboolean    e_table_model_has_save_id         (ETableModel *etm);
gchar       *e_table_model_get_save_id         (ETableModel *etm,
					       gint          row);

/**/
gboolean    e_table_model_has_change_pending  (ETableModel *etm);

/**/
void       *e_table_model_duplicate_value     (ETableModel *e_table_model,
					       gint          col,
					       const void  *value);
void        e_table_model_free_value          (ETableModel *e_table_model,
					       gint          col,
					       void        *value);
void       *e_table_model_initialize_value    (ETableModel *e_table_model,
					       gint          col);
gboolean    e_table_model_value_is_empty      (ETableModel *e_table_model,
					       gint          col,
					       const void  *value);
gchar       *e_table_model_value_to_string     (ETableModel *e_table_model,
					       gint          col,
					       const void  *value);

/*
 * Routines for emitting signals on the e_table
 */
void        e_table_model_pre_change          (ETableModel *e_table_model);
void        e_table_model_no_change           (ETableModel *e_table_model);
void        e_table_model_changed             (ETableModel *e_table_model);
void        e_table_model_row_changed         (ETableModel *e_table_model,
					       gint          row);
void        e_table_model_cell_changed        (ETableModel *e_table_model,
					       gint          col,
					       gint          row);
void        e_table_model_rows_inserted       (ETableModel *e_table_model,
gint          row,
gint          count);
void        e_table_model_rows_deleted        (ETableModel *e_table_model,
gint          row,
gint          count);

/**/
void        e_table_model_row_inserted        (ETableModel *e_table_model,
gint          row);
void        e_table_model_row_deleted         (ETableModel *e_table_model,
gint          row);

void        e_table_model_freeze              (ETableModel *e_table_model);
void        e_table_model_thaw                (ETableModel *e_table_model);

G_END_DECLS

#endif /* _E_TABLE_MODEL_H_ */
