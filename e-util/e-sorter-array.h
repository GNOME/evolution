/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_SORTER_ARRAY_H_
#define _E_SORTER_ARRAY_H_

#include <gtk/gtkobject.h>
#include <gal/util/e-sorter.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define E_SORTER_ARRAY_TYPE        (e_sorter_array_get_type ())
#define E_SORTER_ARRAY(o)          (GTK_CHECK_CAST ((o), E_SORTER_ARRAY_TYPE, ESorterArray))
#define E_SORTER_ARRAY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SORTER_ARRAY_TYPE, ESorterArrayClass))
#define E_IS_SORTER_ARRAY(o)       (GTK_CHECK_TYPE ((o), E_SORTER_ARRAY_TYPE))
#define E_IS_SORTER_ARRAY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SORTER_ARRAY_TYPE))

#ifndef _E_COMPARE_ROWS_FUNC_H_
#define _E_COMPARE_ROWS_FUNC_H_
typedef int (*ECompareRowsFunc) (int row1,
				 int row2,
				 gpointer closure);
#endif

typedef struct {
	ESorter      base;

	ECompareRowsFunc compare;
	gpointer     closure;

	/* If needs_sorting is 0, then model_to_sorted and sorted_to_model are no-ops. */
	int         *sorted;
	int         *backsorted;

	int rows;
} ESorterArray;

typedef struct {
	ESorterClass parent_class;
} ESorterArrayClass;

GtkType       e_sorter_array_get_type   (void);
ESorterArray *e_sorter_array_construct  (ESorterArray     *sorter,
					 ECompareRowsFunc  compare,
					 gpointer          closure);
ESorterArray *e_sorter_array_new        (ECompareRowsFunc  compare,
					 gpointer          closure);
void          e_sorter_array_clean      (ESorterArray     *esa);
void          e_sorter_array_set_count  (ESorterArray     *esa,
					 int               count);
void          e_sorter_array_append     (ESorterArray     *esa,
					 int               count);

END_GNOME_DECLS

#endif /* _E_SORTER_ARRAY_H_ */
