#ifndef _E_TABLE_COL_H_
#define _E_TABLE_COL_H_

typedef struct _ETableCol ETableCol;

/*
 * Rendering function for the column header
 */
typedef struct ERenderContext ERenderContext;

typedef void (*ETableColRenderFn)(ERenderContext *ctxt);

/*
 * Information about a single column
 */
struct _ETableCol {
	char              *id;
	short              width;
	short              min_width;
	short              x;
	ETableColRenderFn  render;
	GCompareFunc       compare;
	void              *render_data;
	unsigned int       selected:1;
	unsigned int       resizeable:1;
};

ETableCol *e_table_col_new (const char *id, int width, int min_width,
			    ETableColRenderFn render, void *render_data,
			    GCompareFunc compare, gboolean resizable);
	

#endif /* _E_TABLE_COL_H_ */
