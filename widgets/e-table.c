/*
 * E-table-view.c: A graphical view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc
 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <stdio.h>
#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtksignal.h>
#include "e-table.h"
#include "e-util.h"
#include "e-table-header-item.h"
#include "e-table-subset.h"
#include "e-table-item.h"

#define COLUMN_HEADER_HEIGHT 16
#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE gtk_table_get_type ()

static GtkObjectClass *e_table_parent_class;

typedef struct {
	ETableModel     *table;
	GnomeCanvasItem *table_item;
	GnomeCanvasItem *rect;
} Leaf;

typedef struct {
	
} Node;


static void
et_destroy (GtkObject *object)
{
	ETable *et = E_TABLE (object);
	
	gtk_object_unref (GTK_OBJECT (et->model));
	gtk_object_unref (GTK_OBJECT (et->full_header));
	gtk_object_unref (GTK_OBJECT (et->header));

	g_free (et->group_spec);
	
	(*e_table_parent_class->destroy)(object);
}

static void
e_table_init (GtkObject *object)
{
	ETable *e_table = E_TABLE (object);

	e_table->draw_grid = 1;
	e_table->draw_focus = 1;
	e_table->spreadsheet = 1;
}

static ETableHeader *
e_table_make_header (ETable *e_table, ETableHeader *full_header, const char *cols)
{
	ETableHeader *nh;
	char *copy = alloca (strlen (cols) + 1);
	char *p, *state;
	const int max_cols = e_table_header_count (full_header);
	
	nh = e_table_header_new ();
	strcpy (copy, cols);
	while ((p = strtok_r (copy, ",", &state)) != NULL){
		int col = atoi (p);

		copy = NULL;
		if (col >= max_cols)
			continue;

		e_table_header_add_column (nh, e_table_header_get_column (full_header, col), -1);
	}
	
	return nh;
}

static void
e_table_setup_header (ETable *e_table)
{
	e_table->header_canvas = GNOME_CANVAS (gnome_canvas_new ());

	gtk_widget_show (GTK_WIDGET (e_table->header_canvas));

	e_table->header_item = gnome_canvas_item_new (
		gnome_canvas_root (e_table->header_canvas),
		e_table_header_item_get_type (),
		"ETableHeader", e_table->header,
		"x", 0,
		"y", 0,
		NULL);

	gtk_widget_set_usize (GTK_WIDGET (e_table->header_canvas), -1, COLUMN_HEADER_HEIGHT);
	g_warning ("Aqui");
	e_table->header_canvas = 0;
	
	gtk_table_attach (
		GTK_TABLE (e_table), GTK_WIDGET (e_table->header_canvas),
		0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
		
}

static Leaf *
e_table_create_leaf (ETable *e_table, int x_off, int y_off, ETableModel *etm)
{
	Leaf *leaf;
	int height;
	
	leaf = g_new (Leaf, 1);
	leaf->table = etm;
	
	leaf->table_item = gnome_canvas_item_new (
		gnome_canvas_root (e_table->table_canvas),
		e_table_item_get_type (),
		"ETableHeader", e_table->header,
		"ETableModel",  etm,
		"x", x_off + GROUP_INDENT,
		"y", y_off + TITLE_HEIGHT,
		"drawgrid", e_table->draw_grid,
		"drawfocus", e_table->draw_focus,
		"spreadsheet", e_table->spreadsheet,
		NULL);

	height = E_TABLE_ITEM (leaf->table_item)->height;

	leaf->rect = gnome_canvas_item_new (
		gnome_canvas_root (e_table->table_canvas),
		gnome_canvas_rect_get_type (),
		"x1", (double) x_off,
		"y1", (double) y_off,
		"x2", (double) x_off + e_table->gen_header_width + GROUP_INDENT,
		"y2", (double) y_off + TITLE_HEIGHT + height,
		"fill_color", "gray",
		"outline_color", "gray20",
		NULL);
	gnome_canvas_item_lower (leaf->rect, 1);

	return leaf;
}

typedef struct {
	void *value;
	GArray *array;
} group_key_t;

static GArray *
e_table_create_groups (ETableModel *etm, int key_col, GCompareFunc comp)
{
	GArray *groups;
	const int rows = e_table_model_row_count (etm);
	int row, i;
	
	groups = g_array_new (FALSE, FALSE, sizeof (group_key_t));

	for (row = 0; row < rows; row++){
		void *val = e_table_model_value_at (etm, key_col, row);
		const int n_groups = groups->len;

		/*
		 * Should replace this with a bsearch later
		 */
		for (i = 0; i < n_groups; i++){
			group_key_t *g = &g_array_index (groups, group_key_t, i);
  			
			if ((*comp) (g->value, val)){
				g_array_append_val (g->array, row);
				break;
			}
		}
		if (i != n_groups)
			continue;

		/*
		 * We need to create a new group
		 */
		{
			group_key_t gk;

			gk.value = val;
			gk.array = g_array_new (FALSE, FALSE, sizeof (int));

			g_array_append_val (gk.array, row);
			g_array_append_val (groups, gk);
		}
	}

	return groups;
}

static void
e_table_destroy_groups (GArray *groups)
{
	const int n = groups->len;
	int i;
	
	for (i = 0; i < n; i++){
		group_key_t *g = &g_array_index (groups, group_key_t, i);

		g_array_free (g->array, TRUE);
	}
	g_array_free (groups, TRUE);
}

static ETableModel **
e_table_make_subtables (ETableModel *model, GArray *groups)
{
	const int n_groups = groups->len;
	ETableModel **tables;
	int i;

	tables = g_new (ETableModel *, n_groups+1);

	for (i = 0; i < n_groups; i++){
		group_key_t *g = &g_array_index (groups, group_key_t, i);
		const int sub_size = g->array->len;
		ETableSubset *ss;
		int j;

		printf ("Creating subset of %d elements\n", sub_size);
		tables [i] = e_table_subset_new (model, sub_size);
		ss = E_TABLE_SUBSET (tables [i]);
		
		for (j = 0; j < sub_size; j++)
			ss->map_table [j] = g_array_index (g->array, int, j);
	}
	tables [i] = NULL;
		
	return (ETableModel **) tables;
}

static int
leaf_height (Leaf *leaf)
{
	return E_TABLE_ITEM (leaf->table_item)->height + TITLE_HEIGHT;
}

static int
e_table_create_nodes (ETable *e_table, ETableModel *model, ETableHeader *header,
		      GnomeCanvasGroup *root, int x_off, int y_off,
		      int *groups_list)
{
	GArray *groups;
	ETableModel **tables;
	int key_col;
	int height, i;
	GCompareFunc comp;

	if (groups_list)
		key_col = *groups_list;
	else
		key_col = -1;
	
	if (key_col == -1){
		Leaf *leaf;

		printf ("Leaf->with %d rows\n", e_table_model_row_count (model));
		leaf = e_table_create_leaf (e_table, x_off, y_off, model);

		{
			static int warn_shown;

			if (!warn_shown){
				g_warning ("Leak");
				warn_shown = 1;
			}
		}
		return leaf_height (leaf);
	}

	/*
	 * Create groups
	 */
	comp = e_table_header_get_column (header, key_col)->compare;
	groups = e_table_create_groups (model, key_col, comp);
	tables = e_table_make_subtables (e_table->model, groups);
	e_table_destroy_groups (groups);

	height = 0;
	for (i = 0; tables [i] != NULL; i++){
		printf ("Creating->%d with %d rows\n", i, e_table_model_row_count (tables [i]));
		height += e_table_create_nodes (
			e_table, tables [i], header, root,
			x_off + 20,
			y_off + height, &groups_list [1]);
	}
	
	return height;
}

static int *
group_spec_to_desc (const char *group_spec)
{
	int a_size = 10;
	int *elements;
	char *p, *copy, *follow;
	int n_elements = 0;

	if (group_spec == NULL)
		return NULL;

	elements = g_new (int, a_size);	
	copy = alloca (strlen (group_spec) + 1);
	strcpy (copy, group_spec);

	while ((p = strtok_r (copy, ",", &follow)) != NULL){
		elements [n_elements] = atoi (p);
		++n_elements;
		if (n_elements+1 == a_size){
			int *new_e;
			
			n_elements += 10;
			new_e = g_renew (int, elements, n_elements);
			if (new_e == NULL){
				g_free (elements);
				return NULL;
			}
			elements = new_e;
		}
		copy = NULL;
	}

	/* Tag end */
	elements [n_elements] = -1;
	
	return elements;
}

/*
 * The ETableCanvas object is just used to enable us to
 * hook up to the realize/unrealize phases of the canvas
 * initialization (as laying out the subtables requires us to
 * know the actual size of the subtables we are inserting
 */
 
#define E_TABLE_CANVAS_PARENT_TYPE gnome_canvas_get_type ()

typedef struct {
	GnomeCanvas base;

	ETable *e_table;
} ETableCanvas;

typedef struct {
	GnomeCanvasClass base_class;
} ETableCanvasClass;

static GnomeCanvasClass *e_table_canvas_parent_class;

static void
e_table_canvas_realize (GtkWidget *widget)
{
	ETableCanvas *e_table_canvas = (ETableCanvas *) widget;
	ETable *e_table = e_table_canvas->e_table;
	int *groups;
	int height;

	GTK_WIDGET_CLASS (e_table_canvas_parent_class)->realize (widget);
	
	groups = group_spec_to_desc (e_table->group_spec);

	e_table->root = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (e_table->table_canvas->root),
		gnome_canvas_group_get_type (),
		"x", 0.0,
		"y", 0.0,
		NULL);
		
	e_table->gen_header_width = e_table_header_total_width (e_table->header);
	
	height = e_table_create_nodes (
		e_table, e_table->model,
		e_table->header, GNOME_CANVAS_GROUP (e_table->root), 0, 0, groups);

	{
		static int warn_shown;

		if (!warn_shown){
			g_warning ("Precompute the width, and update on model changes");
			warn_shown = 1;
		}
	}

	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (e_table_canvas),
		0, 0,
		e_table_header_total_width (e_table->header) + 200,
		height);

	if (groups)
		g_free (groups);
}

static void
e_table_canvas_unrealize (GtkWidget *widget)
{
	ETableCanvas *e_table_canvas = (ETableCanvas *) widget;
	ETable *e_table = e_table_canvas->e_table;
	
	gtk_object_destroy (GTK_OBJECT (e_table->root));
	e_table->root = NULL;

	GTK_WIDGET_CLASS (e_table_canvas_parent_class)->unrealize (widget);
}

static void
e_table_canvas_class_init (GtkObjectClass *object_class)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *) object_class;

	widget_class->realize = e_table_canvas_realize;
	widget_class->unrealize = e_table_canvas_unrealize;

	e_table_canvas_parent_class = gtk_type_class (E_TABLE_CANVAS_PARENT_TYPE);
}

GtkType e_table_canvas_get_type (void);

E_MAKE_TYPE (e_table_canvas, "ETableCanvas", ETableCanvas, e_table_canvas_class_init,
	     NULL, E_TABLE_CANVAS_PARENT_TYPE);

static GnomeCanvas *
e_table_canvas_new (ETable *e_table)
{
	ETableCanvas *e_table_canvas;

	e_table_canvas = gtk_type_new (e_table_canvas_get_type ());
	e_table_canvas->e_table = e_table;

	return GNOME_CANVAS (e_table_canvas);
}

static void
e_table_setup_table (ETable *e_table)
{
	e_table->table_canvas = e_table_canvas_new (e_table);

	gtk_widget_show (GTK_WIDGET (e_table->table_canvas));
	gtk_table_attach (
		GTK_TABLE (e_table), GTK_WIDGET (e_table->table_canvas),
		0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
}

void
e_table_construct (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
		   const char *cols_spec, const char *group_spec)
{
	GTK_TABLE (e_table)->homogeneous = FALSE;

	gtk_table_resize (GTK_TABLE (e_table), 1, 2);

	e_table->full_header = full_header;
	gtk_object_ref (GTK_OBJECT (full_header));

	e_table->model = etm;
	gtk_object_ref (GTK_OBJECT (etm));

	e_table->header = e_table_make_header (e_table, full_header, cols_spec);

	e_table_setup_header (e_table);
	e_table_setup_table (e_table);

	e_table->group_spec = g_strdup (group_spec);

}

GtkWidget *
e_table_new (ETableHeader *full_header, ETableModel *etm, const char *cols_spec, const char *group_spec)
{
	ETable *e_table;

	e_table = gtk_type_new (e_table_get_type ());

	e_table_construct (e_table, full_header, etm, cols_spec, group_spec);
		
	return (GtkWidget *) e_table;
}

static void
e_table_class_init (GtkObjectClass *object_class)
{
	e_table_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = et_destroy;
}

E_MAKE_TYPE(e_table, "ETable", ETable, e_table_class_init, e_table_init, PARENT_TYPE);

