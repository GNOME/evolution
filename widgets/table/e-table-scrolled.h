/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SCROLLED_H_
#define _E_TABLE_SCROLLED_H_

#include <gal/widgets/e-scroll-frame.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table.h>
#include <gal/widgets/e-printable.h>

BEGIN_GNOME_DECLS

#define E_TABLE_SCROLLED_TYPE        (e_table_scrolled_get_type ())
#define E_TABLE_SCROLLED(o)          (GTK_CHECK_CAST ((o), E_TABLE_SCROLLED_TYPE, ETableScrolled))
#define E_TABLE_SCROLLED_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SCROLLED_TYPE, ETableScrolledClass))
#define E_IS_TABLE_SCROLLED(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SCROLLED_TYPE))
#define E_IS_TABLE_SCROLLED_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SCROLLED_TYPE))

typedef struct {
	EScrollFrame parent;

	ETable *table;
} ETableScrolled;

typedef struct {
	EScrollFrameClass parent_class;

	void        (*cursor_change)      (ETableScrolled *est, int row);
	void        (*double_click)       (ETableScrolled *est, int row);
	gint        (*right_click)        (ETableScrolled *est, int row, int col, GdkEvent *event);
	gint        (*click)              (ETableScrolled *est, int row, int col, GdkEvent *event);
	gint        (*key_press)          (ETableScrolled *est, int row, int col, GdkEvent *event);
} ETableScrolledClass;

GtkType         e_table_scrolled_get_type         (void);

ETableScrolled *e_table_scrolled_construct                 (ETableScrolled    *ets,
							    ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec,
							    const char        *state);
GtkWidget      *e_table_scrolled_new                       (ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec,
							    const char        *state);

ETableScrolled *e_table_scrolled_construct_from_spec_file  (ETableScrolled    *ets,
							    ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec_fn,
							    const char        *state_fn);
GtkWidget      *e_table_scrolled_new_from_spec_file        (ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec_fn,
							    const char        *state_fn);

gchar          *e_table_scrolled_get_state                 (ETableScrolled    *ets);
void            e_table_scrolled_save_state                (ETableScrolled    *ets,
							    const gchar       *filename);

/* note that it is more efficient to provide the state at creation time */
void            e_table_scrolled_set_state                 (ETableScrolled    *ets,
							    const gchar       *state);
void            e_table_scrolled_load_state                (ETableScrolled    *ets,
							    const gchar       *filename);

void            e_table_scrolled_set_cursor_row            (ETableScrolled    *ets,
							    int                row);
/* -1 means we don't have the cursor. */
int             e_table_scrolled_get_cursor_row            (ETableScrolled    *ets);
void            e_table_scrolled_selected_row_foreach      (ETableScrolled    *ets,
							    ETableForeachFunc  callback,
							    gpointer           closure);
EPrintable     *e_table_scrolled_get_printable             (ETableScrolled    *ets);

END_GNOME_DECLS

#endif /* _E_TABLE_SCROLLED_H_ */

