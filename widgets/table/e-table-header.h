#ifndef _E_TABLE_COLUMN_H_
#define _E_TABLE_COLUMN_H_

#include <gtk/gtkobject.h>
#include <gdk/gdk.h>
#include "e-table-col.h"

typedef struct _ETableHeader ETableHeader;
typedef struct _ETableCol ETableCol;

/*
 * Rendering function for the column header
 */
typedef void (*ETableColRenderFn)(
	ETableCol *etc, void *gnome_canvas_item, void *drawable,
	int x, int y, int w, int h, void *data);

/*
 * Information about a single column
 */
struct _ETableCol {
	const char        *id;
	int                width;
	ETableColRenderFn  render;
	void              *render_data;
	unsigned int       selected:1;
	int                resizeable:1;
	int                min_size;
};

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

	void (*structure_change) (ETableHeader *etc);
	void (*dimension_change) (ETableHeader *etc, int col);
} ETableHeaderClass;

GtkType        e_table_header_get_type   (void);
ETableHeader  *e_table_header_new        (void);

void        e_table_header_add_column    (ETableHeader *etc,
					  ETableCol *tc, int pos);
ETableCol * e_table_header_get_column    (ETableHeader *etc,
					  int column);
int         e_table_header_count         (ETableHeader *etc);
int         e_table_header_index         (ETableHeader *etc,
					  const char *identifier);
int         e_table_header_get_index_at  (ETableHeader *etc,
					  int x_offset);
ETableCol **e_table_header_get_columns   (ETableHeader *etc);
gboolean    e_table_header_selection_ok  (ETableHeader *etc);
int         e_table_header_get_selected  (ETableHeader *etc);
int         e_table_header_total_width   (ETableHeader *etc);
void        e_table_header_move          (ETableHeader *etc,
					  int source_index,
					  int target_index);
void        e_table_header_remove        (ETableHeader *etc, int idx);
void        e_table_header_set_size      (ETableHeader *etc, int idx, int size);
void        e_table_header_set_selection (ETableHeader *etc,
					  gboolean allow_selection);

GList      *e_table_header_get_selected_indexes(ETableHeader *etc);

#endif /* _E_TABLE_HEADER_H_ */
