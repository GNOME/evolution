/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SCROLLED_H_
#define _E_TABLE_SCROLLED_H_

#include "widgets/misc/e-scroll-frame.h"
#include "e-table-model.h"
#include "e-table-header.h"
#include "e-table.h"
#include "e-util/e-printable.h"

BEGIN_GNOME_DECLS

#define E_TABLE_SCROLLED_TYPE        (e_table_scrolled_get_type ())
#define E_TABLE_SCROLLED(o)          (GTK_CHECK_CAST ((o), E_TABLE_SCROLLED_TYPE, ETableScrolled))
#define E_TABLE_SCROLLED_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SCROLLED_TYPE, ETableScrolledClass))
#define E_IS_SCROLLED_TABLE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SCROLLED_TYPE))
#define E_IS_SCROLLED_TABLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SCROLLED_TYPE))

typedef struct {
	EScrollFrame parent;

	ETable *table;
} ETableScrolled;

typedef struct {
	GtkTableClass parent_class;

	void        (*row_selection)      (ETableScrolled *est, int row, gboolean selected);
	void        (*cursor_change)      (ETableScrolled *est, int row);
	void        (*double_click)       (ETableScrolled *est, int row);
	gint        (*right_click)        (ETableScrolled *est, int row, int col, GdkEvent *event);
	gint        (*key_press)          (ETableScrolled *est, int row, int col, GdkEvent *event);
} ETableScrolledClass;

GtkType         e_table_scrolled_get_type   		  (void);

ETableScrolled *e_table_scrolled_construct  		  (ETableScrolled *ets, ETableHeader *full_header, ETableModel *etm,
	      						   const char *spec);
GtkWidget      *e_table_scrolled_new        		  (ETableHeader *full_header, ETableModel *etm,
							   const char *spec);

ETableScrolled *e_table_scrolled_construct_from_spec_file (ETableScrolled *e_table_scrolled,
							   ETableHeader *full_header,
							   ETableModel *etm,
							   const char *filename);
GtkWidget      *e_table_scrolled_new_from_spec_file       (ETableHeader *full_header,
							   ETableModel *etm,
							   const char *filename);

gchar          *e_table_scrolled_get_specification        (ETableScrolled *e_table_scrolled);
void            e_table_scrolled_save_specification       (ETableScrolled *e_table_scrolled, gchar *filename);

void            e_table_scrolled_set_cursor_row           (ETableScrolled *e_table_scrolled,
							   int row);
/* -1 means we don't have the cursor. */
int             e_table_scrolled_get_cursor_row           (ETableScrolled *e_table_scrolled);
void            e_table_scrolled_selected_row_foreach     (ETableScrolled *e_table_scrolled,
							   ETableForeachFunc callback,
							   gpointer closure);
EPrintable     *e_table_scrolled_get_printable            (ETableScrolled *e_table_scrolled);

END_GNOME_DECLS

#endif /* _E_TABLE_SCROLLED_H_ */

