#ifndef _E_TABLE_COLUMN_H_
#define _E_TABLE_COLUMN_H_

#include <gtk/gtkobject.h>
#include <gdk/gdk.h>
#include "e-table-col.h"

typedef struct _ETableHeader ETableHeader;

#define E_TABLE_HEADER_TYPE        (e_table_header_get_type ())
#define E_TABLE_HEADER(o)          (GTK_CHECK_CAST ((o), E_TABLE_HEADER_TYPE, ETableHeader))
#define E_TABLE_HEADER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_HEADER_TYPE, ETableHeaderClass))
#define E_IS_TABLE_HEADER(o)       (GTK_CHECK_TYPE ((o), E_TABLE_HEADER_TYPE))
#define E_IS_TABLE_HEADER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_HEADER_TYPE))

/*
 * A Columnar header.
 */
struct _ETableHeader {
	GtkObject base;

	int col_count;
	ETableCol **columns;
	gboolean selectable;
};

typedef struct {
	GtkObjectClass parent_class;

	void (*structure_change) (ETableHeader *eth);
	void (*dimension_change) (ETableHeader *eth, int col);
} ETableHeaderClass;

GtkType        e_table_header_get_type   (void);
ETableHeader  *e_table_header_new        (void);

void        e_table_header_add_column    (ETableHeader *eth,
					  ETableCol *tc, int pos);
ETableCol * e_table_header_get_column    (ETableHeader *eth,
					  int column);
int         e_table_header_count         (ETableHeader *eth);
int         e_table_header_index         (ETableHeader *eth,
					  int col);
int         e_table_header_get_index_at  (ETableHeader *eth,
					  int x_offset);
ETableCol **e_table_header_get_columns   (ETableHeader *eth);
gboolean    e_table_header_selection_ok  (ETableHeader *eth);
int         e_table_header_get_selected  (ETableHeader *eth);
int         e_table_header_total_width   (ETableHeader *eth);
void        e_table_header_move          (ETableHeader *eth,
					  int source_index,
					  int target_index);
void        e_table_header_remove        (ETableHeader *eth, int idx);
void        e_table_header_set_size      (ETableHeader *eth, int idx, int size);
void        e_table_header_set_selection (ETableHeader *eth,
					  gboolean allow_selection);

int         e_table_header_col_diff      (ETableHeader *eth,
					  int start_col, int end_col);

GList      *e_table_header_get_selected_indexes(ETableHeader *eth);


#endif /* _E_TABLE_HEADER_H_ */

