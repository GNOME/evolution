/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_SELECTION_MODEL_H_
#define _E_SELECTION_MODEL_H_

#include <gtk/gtkobject.h>
#include <gal/util/e-sorter.h>
#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SELECTION_MODEL_TYPE        (e_selection_model_get_type ())
#define E_SELECTION_MODEL(o)          (GTK_CHECK_CAST ((o), E_SELECTION_MODEL_TYPE, ESelectionModel))
#define E_SELECTION_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_TYPE, ESelectionModelClass))
#define E_IS_SELECTION_MODEL(o)       (GTK_CHECK_TYPE ((o), E_SELECTION_MODEL_TYPE))
#define E_IS_SELECTION_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_TYPE))

typedef void (*EForeachFunc) (int model_row,
			      gpointer closure);

/* list selection modes */
typedef enum {
	E_CURSOR_LINE,
	E_CURSOR_SIMPLE,
	E_CURSOR_SPREADSHEET,
} ECursorMode;

typedef struct {
	GtkObject     base;

	ESorter *sorter;

	gint row_count;
        guint32 *selection;

	gint cursor_row;
	gint cursor_col;
	gint selection_start_row;

	guint model_changed_id;
	guint model_row_inserted_id, model_row_deleted_id;

	guint frozen : 1;
	guint selection_model_changed : 1;
	guint group_info_changed : 1;

	GtkSelectionMode mode;
	ECursorMode cursor_mode;
} ESelectionModel;

typedef struct {
	GtkObjectClass parent_class;

	gint (*get_row_count)     (ESelectionModel *selection);

	/*
	 * Signals
	 */

	void (*cursor_changed)    (ESelectionModel *selection, int row, int col);
	void (*cursor_activated)  (ESelectionModel *selection, int row, int col);
	void (*selection_changed) (ESelectionModel *selection);

} ESelectionModelClass;

GtkType   e_selection_model_get_type            (void);
gboolean  e_selection_model_is_row_selected     (ESelectionModel *selection,
						 gint             n);
void      e_selection_model_foreach             (ESelectionModel *selection,
						 EForeachFunc     callback,
						 gpointer         closure);
void      e_selection_model_do_something        (ESelectionModel *selection,
						 guint            row,
						 guint            col,
						 GdkModifierType  state);
void      e_selection_model_maybe_do_something  (ESelectionModel *selection,
						 guint            row,
						 guint            col,
						 GdkModifierType  state);
gint      e_selection_model_key_press           (ESelectionModel *selection,
						 GdkEventKey     *key);
void      e_selection_model_clear               (ESelectionModel *selection);
gint      e_selection_model_selected_count      (ESelectionModel *selection);
void      e_selection_model_select_all          (ESelectionModel *selection);
void      e_selection_model_invert_selection    (ESelectionModel *selection);
void      e_selection_model_insert_row          (ESelectionModel *esm,
						 int              row);
void      e_selection_model_delete_row          (ESelectionModel *esm,
						 int              row);

/* Virtual Function */
gint      e_selection_model_get_row_count       (ESelectionModel *esm);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_SELECTION_MODEL_H_ */
