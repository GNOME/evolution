/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SELECTION_MODEL_H_
#define _E_TABLE_SELECTION_MODEL_H_

#include <gtk/gtkobject.h>

#define E_TABLE_SELECTION_MODEL_TYPE        (e_table_selection_model_get_type ())
#define E_TABLE_SELECTION_MODEL(o)          (GTK_CHECK_CAST ((o), E_TABLE_SELECTION_MODEL_TYPE, ETableSelectionModel))
#define E_TABLE_SELECTION_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SELECTION_MODEL_TYPE, ETableSelectionModelClass))
#define E_IS_TABLE_SELECTION_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SELECTION_MODEL_TYPE))
#define E_IS_TABLE_SELECTION_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SELECTION_MODEL_TYPE))

typedef struct _ETableSortColumn ETableSortColumn;

struct _ETableSortColumn {
	guint column : 31;
	guint ascending : 1;
};

typedef struct {
	GtkObject   base;

	ETableModel *model;

	gint row_count;
        guint32 *selection;
	
	gint cursor_row;
	gint cursor_col;

	guint model_changed_id;
	guint model_row_inserted_id, model_row_deleted_id;
	
	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;
} ETableSelectionModel;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Signals
	 */
	void        (*selection_model_changed)      (ETableSelectionModel *selection);
	void        (*group_model_changed)     (ETableSelectionModel *selection);
} ETableSelectionModelClass;

GtkType      	 e_table_selection_model_get_type (void);

gboolean         e_table_selection_model_is_row_selected    (ETableSelectionModel *selection,
							     int                   n);
GList           *e_table_selection_model_get_selection_list (ETableSelectionModel *selection);

ETableSelectionModel  *e_table_selection_model_new                (void);

#endif /* _E_TABLE_SELECTION_MODEL_H_ */
