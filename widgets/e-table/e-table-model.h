/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_MODEL_H_
#define _E_TABLE_MODEL_H_

#include <gtk/gtkobject.h>

#define E_TABLE_MODEL_TYPE        (e_table_model_get_type ())
#define E_TABLE_MODEL(o)          (GTK_CHECK_CAST ((o), E_TABLE_MODEL_TYPE, ETableModel))
#define E_TABLE_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_MODEL_TYPE, ETableModelClass))
#define E_IS_TABLE_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TABLE_MODEL_TYPE))
#define E_IS_TABLE_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_MODEL_TYPE))

typedef struct {
	GtkObject   base;
} ETableModel;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	int         (*column_count)     (ETableModel *etm);
	int         (*row_count)        (ETableModel *etm);
	void       *(*value_at)         (ETableModel *etm, int col, int row);
	void        (*set_value_at)     (ETableModel *etm, int col, int row, const void *value);
	gboolean    (*is_cell_editable) (ETableModel *etm, int col, int row);
	gint        (*append_row)       (ETableModel *etm);

	/* Allocate a copy of the given value. */
	void       *(*duplicate_value)  (ETableModel *etm, int col, const void *value);
	/* Free an allocated value. */
	void        (*free_value)       (ETableModel *etm, int col, void *value);
	/* Return an allocated empty value. */
	void	   *(*initialize_value) (ETableModel *etm, int col);
	/* Return TRUE if value is equivalent to an empty cell. */
	gboolean    (*value_is_empty)   (ETableModel *etm, int col, const void *value);
	/* Return an allocated string. */
	char       *(*value_to_string)  (ETableModel *etm, int col, const void *value);
	
	/*
	 * Signals
	 */

	/*
	 * These all come after the change has been made.
	 * Major structural changes: model_changed
	 * Changes only in a row: row_changed
	 * Only changes in a cell: cell_changed
	 * A row inserted: row_inserted
	 * A row deleted: row_deleted
	 */
	void        (*model_changed)      (ETableModel *etm);
	void        (*model_row_changed)  (ETableModel *etm, int row);
	void        (*model_cell_changed) (ETableModel *etm, int col, int row);
	void        (*model_row_inserted) (ETableModel *etm, int row);
	void        (*model_row_deleted)  (ETableModel *etm, int row);
} ETableModelClass;

GtkType     e_table_model_get_type (void);
	
int         e_table_model_column_count     (ETableModel *e_table_model);
const char *e_table_model_column_name      (ETableModel *e_table_model, int col);
int         e_table_model_row_count        (ETableModel *e_table_model);
void       *e_table_model_value_at         (ETableModel *e_table_model, int col, int row);
void        e_table_model_set_value_at     (ETableModel *e_table_model, int col, int row, const void *value);
gboolean    e_table_model_is_cell_editable (ETableModel *e_table_model, int col, int row);
gint        e_table_model_append_row       (ETableModel *e_table_model);

void       *e_table_model_duplicate_value  (ETableModel *e_table_model, int col, const void *value);
void        e_table_model_free_value       (ETableModel *e_table_model, int col, void *value);
void       *e_table_model_initialize_value (ETableModel *e_table_model, int col);
gboolean    e_table_model_value_is_empty   (ETableModel *e_table_model, int col, const void *value);
char       *e_table_model_value_to_string  (ETableModel *e_table_model, int col, const void *value);

/*
 * Routines for emitting signals on the e_table
 */
void        e_table_model_changed          (ETableModel *e_table_model);
void        e_table_model_row_changed      (ETableModel *e_table_model, int row);
void        e_table_model_cell_changed     (ETableModel *e_table_model, int col, int row);
void        e_table_model_row_inserted     (ETableModel *e_table_model, int row);
void        e_table_model_row_deleted      (ETableModel *e_table_model, int row);

#endif /* _E_TABLE_MODEL_H_ */
