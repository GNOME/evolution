#ifndef _E_TABLE_COL_H_
#define _E_TABLE_COL_H_

#include "e-cell.h"

#define E_TABLE_COL_TYPE        (e_table_col_get_type ())
#define E_TABLE_COL(o)          (GTK_CHECK_CAST ((o), E_TABLE_COL_TYPE, ETableCol))
#define E_TABLE_COL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_COL_TYPE, ETableColClass))
#define E_IS_TABLE_COL(o)       (GTK_CHECK_TYPE ((o), E_TABLE_COL_TYPE))
#define E_IS_TABLE_COL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_COL_TYPE))

typedef struct _ETableCol ETableCol;

/*
 * Information about a single column
 */
struct _ETableCol {
	GtkObject    base;
	char        *text;
	short        width;
	short        min_width;
	short        x;
	GCompareFunc compare;
	unsigned int selected:1;
	unsigned int resizeable:1;
	int          col_idx;

	ECell             *ecell;
};

typedef struct {
	GtkObjectClass parent_class;
} ETableColClass;

GtkType    e_table_col_get_type (void);
ETableCol *e_table_col_new      (int col_idx, const char *text,
				 int width, int min_width,
				 ECell *ecell, GCompareFunc compare,
				 gboolean resizable);
void       e_table_col_destroy  (ETableCol *etc);


#endif /* _E_TABLE_COL_H_ */

