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
	const char *(*column_name)      (ETableModel *etm, int col);
	int         (*row_count)        (ETableModel *etm);
	void       *(*value_at)         (ETableModel *etm, int col, int row);
	void        (*set_value_at)     (ETableModel *etm, int col, int row, void *value);
	gboolean    (*is_cell_editable) (ETableModel *etm, int col, int row);
	int         (*row_height)       (ETableModel *etm, int row);
} ETableModelClass;

GtkType     e_table_model_get_type (void);
	
int         e_table_model_column_count     (ETableModel *e_table_model);
const char *e_table_model_column_name      (ETableModel *e_table_model, int col);
int         e_table_model_row_count        (ETableModel *e_table_model);
int         e_table_model_row_height       (ETableModel *e_table_model, int row);
int         e_table_model_height           (ETableModel *e_table_model);
void       *e_table_model_value_at         (ETableModel *e_table_model, int col, int row);
void        e_table_model_set_value_at     (ETableModel *e_table_model, int col, int row, void *data);
gboolean    e_table_model_is_cell_editable (ETableModel *e_table_model, int col, int row);

/*
 * Routines for emitting signals on the e_table
 */

#endif /* _E_TABLE_MODEL_H_ */

