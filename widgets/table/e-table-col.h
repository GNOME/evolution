#ifndef _E_TABLE_COL_H_
#define _E_TABLE_COL_H_

typedef struct _ETableCol ETableCol;
typedef struct _ECell     ECell;

/*
 * Information about a single column
 */
struct _ETableCol {
	char              *id;
	short              width;
	short              min_width;
	short              x;
	GCompareFunc       compare;
	unsigned int       selected:1;
	unsigned int       resizeable:1;

	ECell             *ecell;
};

ETableCol *e_table_col_new (const char *id, int width, int min_width,
			    ECell *ecell, GCompareFunc compare,
			    gboolean resizable);
	

#endif /* _E_TABLE_COL_H_ */

