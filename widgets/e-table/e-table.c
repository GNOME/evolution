/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table.c: A graphical view of a Table.
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
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdio.h>
#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"
#include "e-table.h"
#include "e-table-header-item.h"
#include "e-table-subset.h"
#include "e-table-item.h"
#include "e-table-group.h"

#define COLUMN_HEADER_HEIGHT 16
#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE gtk_table_get_type ()

static GtkObjectClass *e_table_parent_class;

static void e_table_fill_table (ETable *e_table, ETableModel *model);

static void
et_destroy (GtkObject *object)
{
	ETable *et = E_TABLE (object);
	
	gtk_object_unref (GTK_OBJECT (et->model));
	gtk_object_unref (GTK_OBJECT (et->full_header));
	gtk_object_unref (GTK_OBJECT (et->header));
	gtk_widget_destroy (GTK_WIDGET (et->header_canvas));
	gtk_widget_destroy (GTK_WIDGET (et->table_canvas));

	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_model_change_id);
	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_row_change_id);
	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_cell_change_id);

	if (et->rebuild_idle_id) {
		g_source_remove(et->rebuild_idle_id);
		et->rebuild_idle_id = 0;
	}
	
	xmlFreeDoc (et->specification);

	(*e_table_parent_class->destroy)(object);
}

static void
e_table_init (GtkObject *object)
{
	ETable *e_table = E_TABLE (object);

	e_table->draw_grid = 1;
	e_table->draw_focus = 1;
	e_table->spreadsheet = 1;
	
	e_table->need_rebuild = 0;
	e_table->need_row_changes = 0;
	e_table->row_changes_list = NULL;
	e_table->rebuild_idle_id = 0;
}

static ETableHeader *
e_table_make_header (ETable *e_table, ETableHeader *full_header, xmlNode *xmlColumns)
{
	ETableHeader *nh;
	xmlNode *column;
	const int max_cols = e_table_header_count (full_header);
	
	nh = e_table_header_new ();
	for (column = xmlColumns->childs; column; column = column->next) {
		int col = atoi (column->childs->content);

		if (col >= max_cols)
			continue;

		e_table_header_add_column (nh, e_table_header_get_column (full_header, col), -1);
	}

	e_table_header_set_frozen_columns( nh, e_xml_get_integer_prop_by_name(xmlColumns, "frozen_columns") );
	
	return nh;
}

static void
header_canvas_size_alocate (GtkWidget *widget, GtkAllocation *alloc, ETable *e_table)
{
	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (e_table->header_canvas),
		0, 0, alloc->width, COLUMN_HEADER_HEIGHT);
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

	gtk_signal_connect (
		GTK_OBJECT (e_table->header_canvas), "size_allocate",
		GTK_SIGNAL_FUNC (header_canvas_size_alocate), e_table);
				 
	gtk_widget_set_usize (GTK_WIDGET (e_table->header_canvas), -1, COLUMN_HEADER_HEIGHT);
	
	gtk_table_attach (
		GTK_TABLE (e_table), GTK_WIDGET (e_table->header_canvas),
		0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
		
}

#if 0
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

		tables [i] = e_table_subset_new (model, sub_size);
		ss = E_TABLE_SUBSET (tables [i]);
		
		for (j = 0; j < sub_size; j++)
			ss->map_table [j] = g_array_index (g->array, int, j);
	}
	tables [i] = NULL;
		
	return (ETableModel **) tables;
}

typedef struct _Node Node;

struct _Node {
	Node            *parent;
	GnomeCanvasItem *item;
	ETableModel     *table_model;
	GSList          *children;

	guint is_leaf:1;
};

static Node *
leaf_new (GnomeCanvasItem *table_item, ETableModel *table_model, Node *parent)
{
	Node *node = g_new (Node, 1);

	g_assert (table_item != NULL);
	g_assert (table_model != NULL);
	g_assert (parent != NULL);
	
	node->item = table_item;
	node->parent = parent;
	node->table_model = table_model;
	node->is_leaf = 1;

	g_assert (!parent->is_leaf);
	
	parent->children = g_slist_append (parent->children, node);

	e_table_group_add (E_TABLE_GROUP (parent->item), table_item);

	return node;
}

static Node *
node_new (GnomeCanvasItem *group_item, ETableModel *table_model, Node *parent)
{
	Node *node = g_new (Node, 1);

	g_assert (table_model != NULL);

	node->children = NULL;
	node->item = group_item;
	node->parent = parent;
	node->table_model = table_model;
	node->is_leaf = 0;

	if (parent){
		parent->children = g_slist_append (parent->children, node);

		e_table_group_add (E_TABLE_GROUP (parent->item), group_item);
	}
	
	return node;
}

static Node *
e_table_create_leaf (ETable *e_table, ETableModel *etm, Node *parent)
{
	GnomeCanvasItem *table_item;
	Node *leaf;
	
	table_item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (parent->item),
		e_table_item_get_type (),
		"ETableHeader", e_table->header,
		"ETableModel",  etm,
		"drawgrid", e_table->draw_grid,
		"drawfocus", e_table->draw_focus,
		"spreadsheet", e_table->spreadsheet,
		NULL);

	leaf = leaf_new (table_item, etm, parent);
	
	return leaf;
}

static int
leaf_height (Node *leaf)
{
	const GnomeCanvasItem *item = leaf->item;
	
	return item->y2 - item->y1;
}

static int
leaf_event (GnomeCanvasItem *item, GdkEvent *event)
{
	static int last_x = -1;
	static int last_y = -1;
	
	if (event->type == GDK_BUTTON_PRESS){
		last_x = event->button.x;
		last_y = event->button.y;
	} else if (event->type == GDK_BUTTON_RELEASE){
		last_x = -1;
		last_y = -1;
	} else if (event->type == GDK_MOTION_NOTIFY){
		if (last_x == -1)
			return FALSE;
		
		gnome_canvas_item_move (item, event->motion.x - last_x, event->motion.y - last_y);
		last_x = event->motion.x;
		last_y = event->motion.y;
	} else
		return FALSE;
	return TRUE;
}

static Node *
e_table_create_nodes (ETable *e_table, ETableModel *model, ETableHeader *header,
		      GnomeCanvasGroup *root, Node *parent, int *groups_list)
{
	GArray *groups;
	ETableModel **tables;
	ETableCol *ecol;
	int key_col, i;
	GnomeCanvasItem *group_item;
	Node *group;

	key_col = *groups_list;
	g_assert (key_col != -1);
	
	/*
	 * Create groups
	 */
	ecol = e_table_header_get_column (header, key_col);

	g_assert (ecol != NULL);
	
	groups = e_table_create_groups (model, key_col, ecol->compare);
	tables = e_table_make_subtables (e_table->model, groups);
	e_table_destroy_groups (groups);
	group_item = gnome_canvas_item_new (root,
					    e_table_group_get_type(),
					    "columns", ecol, TRUE, parent == NULL);
	group = node_new (group_item, model, parent);
	
	for (i = 0; tables [i] != NULL; i++){
		/*
		 * Leafs
		 */
		if (groups_list [1] == -1){
			GnomeCanvasItem *item_leaf_header;
			Node *leaf_header;
			
			/* FIXME *//*
			item_leaf_header = e_table_group_new (
			GNOME_CANVAS_GROUP (group_item), ecol, TRUE, FALSE);*/
			leaf_header = node_new (item_leaf_header, tables [i], group);

			e_table_create_leaf (e_table, tables [i], leaf_header);
		} else {
			e_table_create_nodes (
				e_table, tables [i], header, GNOME_CANVAS_GROUP (group_item),
				group, &groups_list [1]);
		}
	}

	return group;
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
#if 0
	GnomeCanvasItem *group_item;
	
	group_item = gnome_canvas_item_new (root,
					    e_table_group_get_type(),
					    "header", E_TABLE, TRUE, parent == NULL);
	

	ETableCanvas *e_table_canvas = (ETableCanvas *) widget;
	ETable *e_table = e_table_canvas->e_table;
	int *groups;
	Node *leaf;
	
	GTK_WIDGET_CLASS (e_table_canvas_parent_class)->realize (widget);
	
	groups = group_spec_to_desc (e_table->group_spec);

	

	leaf = e_table_create_nodes (
		e_table, e_table->model,
		e_table->header, GNOME_CANVAS_GROUP (e_table->root), 0, groups);

	
	if (groups)
		g_free (groups);
#endif
}

static void
e_table_canvas_unrealize (GtkWidget *widget)
{
	ETableCanvas *e_table_canvas = (ETableCanvas *) widget;
	ETable *e_table = e_table_canvas->e_table;
	
	gtk_object_destroy (GTK_OBJECT (e_table->root));

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

static void
e_table_canvas_init (GtkObject *canvas)
{
	ETableCanvas *e_table_canvas = (ETableCanvas *) (canvas);
	ETable *e_table = e_table_canvas->e_table;
	
	GTK_WIDGET_SET_FLAGS (canvas, GTK_CAN_FOCUS);

}

GtkType e_table_canvas_get_type (void);

E_MAKE_TYPE (e_table_canvas, "ETableCanvas", ETableCanvas, e_table_canvas_class_init,
	     e_table_canvas_init, E_TABLE_CANVAS_PARENT_TYPE);

static GnomeCanvas *
e_table_canvas_new (ETable *e_table)
{
	ETableCanvas *e_table_canvas;

	e_table_canvas = gtk_type_new (e_table_canvas_get_type ());
	e_table_canvas->e_table = e_table;
	
	e_table->root = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS (e_table_canvas)->root),
		gnome_canvas_group_get_type (),
		"x", 0.0,
		"y", 0.0,
		NULL);

	return GNOME_CANVAS (e_table_canvas);
}
#endif

static void
table_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc, ETable *e_table)
{
	gdouble height;
	gdouble width;
	gtk_object_get(GTK_OBJECT(e_table->group),
		       "height", &height,
		       NULL);
	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (e_table->table_canvas),
		0, 0, alloc->width, MAX(height, alloc->height));
	width = alloc->width;
	gtk_object_set(GTK_OBJECT(e_table->group),
		       "width", width,
		       NULL);
}

static void
change_row (gpointer key, gpointer value, gpointer data)
{
	ETable *et = E_TABLE(data);
	gint row = GPOINTER_TO_INT(key);
	if ( e_table_group_remove(et->group, row) ) {
		e_table_group_add(et->group, row);
	}
}

static gboolean
changed_idle (gpointer data)
{
	ETable *et = E_TABLE(data);
	if ( et->need_rebuild ) {
		gtk_object_destroy( GTK_OBJECT(et->group ) );
		et->group = e_table_group_new(GNOME_CANVAS_GROUP(et->table_canvas->root),
					      et->full_header,
					      et->header,
					      et->model,
					      e_xml_get_child_by_name(xmlDocGetRootElement(et->specification), "grouping")->childs);
		e_table_fill_table(et, et->model);
	} else if (et->need_row_changes) {
		g_hash_table_foreach(et->row_changes_list, change_row, et);
	}	
	et->need_rebuild = 0;
	et->need_row_changes = 0;
	if (et->row_changes_list)
		g_hash_table_destroy(et->row_changes_list);
	et->row_changes_list = NULL;
	et->rebuild_idle_id = 0;
	return FALSE;
}

static void
et_table_model_changed (ETableModel *model, ETable *et)
{
	et->need_rebuild = TRUE;
	if ( !et->rebuild_idle_id ) {
		et->rebuild_idle_id = g_idle_add(changed_idle, et);
	}
}

static void
et_table_row_changed (ETableModel *table_model, int row, ETable *et)
{
	if ( !et->need_rebuild ) {
		if (!et->need_row_changes) {
			et->need_row_changes = 1;
			et->row_changes_list = g_hash_table_new (g_direct_hash, g_direct_equal);
		}
		if (!g_hash_table_lookup(et->row_changes_list, GINT_TO_POINTER(row))) {
			g_hash_table_insert(et->row_changes_list, GINT_TO_POINTER(row), GINT_TO_POINTER(row + 1));
		}
	}
	if ( !et->rebuild_idle_id ) {
		et->rebuild_idle_id = g_idle_add(changed_idle, et);
	}
}

static void
et_table_cell_changed (ETableModel *table_model, int view_col, int row, ETable *et)
{
	et_table_row_changed(table_model, row, et);
}

static void
e_table_setup_table (ETable *e_table, ETableHeader *full_header, ETableHeader *header, ETableModel *model, xmlNode *xml_grouping)
{
	e_table->table_canvas = GNOME_CANVAS(gnome_canvas_new ());
	gtk_signal_connect (
		GTK_OBJECT (e_table->table_canvas), "size_allocate",
		GTK_SIGNAL_FUNC (table_canvas_size_allocate), e_table);
	
	gtk_widget_show (GTK_WIDGET (e_table->table_canvas));
	gtk_table_attach (
		GTK_TABLE (e_table), GTK_WIDGET (e_table->table_canvas),
		0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	
	e_table->group = e_table_group_new(GNOME_CANVAS_GROUP(e_table->table_canvas->root),
					   full_header,
					   header,
					   model,
					   xml_grouping->childs);
	
	e_table->table_model_change_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_changed",
		GTK_SIGNAL_FUNC (et_table_model_changed), e_table);

	e_table->table_row_change_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_row_changed",
		GTK_SIGNAL_FUNC (et_table_row_changed), e_table);

	e_table->table_cell_change_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_cell_changed",
		GTK_SIGNAL_FUNC (et_table_cell_changed), e_table);
}

static void
e_table_fill_table (ETable *e_table, ETableModel *model)
{
	int count;
	int i;
	count = e_table_model_row_count(model);
	gtk_object_set(GTK_OBJECT(e_table->group),
		       "frozen", TRUE,
		       NULL);
	for ( i = 0; i < count; i++ ) {
		e_table_group_add(e_table->group, i);
	}
	gtk_object_set(GTK_OBJECT(e_table->group),
		       "frozen", FALSE,
		       NULL);
}

static void
et_real_construct (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
			xmlDoc *xmlSpec)
{
	xmlNode *xmlRoot;
	xmlNode *xmlColumns;
	xmlNode *xmlGrouping;
	
	GtkWidget *vscrollbar;

	GTK_TABLE (e_table)->homogeneous = FALSE;

	gtk_table_resize (GTK_TABLE (e_table), 2, 2);

	e_table->full_header = full_header;
	gtk_object_ref (GTK_OBJECT (full_header));

	e_table->model = etm;
	gtk_object_ref (GTK_OBJECT (etm));

	e_table->specification = xmlSpec;
	xmlRoot = xmlDocGetRootElement(xmlSpec);
	xmlColumns = e_xml_get_child_by_name(xmlRoot, "columns-shown");
	xmlGrouping = e_xml_get_child_by_name(xmlRoot, "grouping");
	
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	e_table->header = e_table_make_header (e_table, full_header, xmlColumns);

	e_table_setup_header (e_table);
	e_table_setup_table (e_table, full_header, e_table->header, etm, xmlGrouping);
	e_table_fill_table (e_table, etm);

	vscrollbar = gtk_vscrollbar_new(gtk_layout_get_vadjustment(GTK_LAYOUT(e_table->table_canvas)));
	gtk_widget_show (vscrollbar);
	gtk_table_attach (
		GTK_TABLE (e_table), vscrollbar,
		1, 2, 1, 2, 0, GTK_FILL | GTK_EXPAND, 0, 0);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
}

void
e_table_construct (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
		   const char *spec)
{
	xmlDoc *xmlSpec;
	char *copy;
	copy = g_strdup(spec);

	xmlSpec = xmlParseMemory(copy, strlen(copy) + 1);
	et_real_construct(e_table, full_header, etm, xmlSpec);
	g_free(copy);
}

void
e_table_construct_from_spec_file (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
				  const char *filename)
{
	xmlDoc *xmlSpec;

	xmlSpec = xmlParseFile(filename);
	et_real_construct(e_table, full_header, etm, xmlSpec);
}

GtkWidget *
e_table_new (ETableHeader *full_header, ETableModel *etm, const char *spec)
{
	ETable *e_table;

	e_table = gtk_type_new (e_table_get_type ());

	e_table_construct (e_table, full_header, etm, spec);
		
	return (GtkWidget *) e_table;
}

GtkWidget *
e_table_new_from_spec_file (ETableHeader *full_header, ETableModel *etm, const char *filename)
{
	ETable *e_table;

	e_table = gtk_type_new (e_table_get_type ());

	e_table_construct (e_table, full_header, etm, filename);
		
	return (GtkWidget *) e_table;
}

static xmlNode *
et_build_column_spec(ETable *e_table)
{
	xmlNode *columns_shown;
	gint i;
	gint col_count;

	columns_shown = xmlNewNode(NULL, "columns-shown");

	col_count = e_table_header_count(e_table->header);
	for ( i = 0; i < col_count; i++ ) {
		gchar *text = g_strdup_printf("%d", e_table_header_index(e_table->header, i));
		xmlNewChild(columns_shown, NULL, "column", text);
		g_free(text);
	}
	if ( e_table->header->frozen_count != 0 )
		e_xml_set_integer_prop_by_name(columns_shown, "frozen_columns", e_table->header->frozen_count);
	return columns_shown;
}

static xmlNode *
et_build_grouping_spec(ETable *e_table)
{
	xmlNode *grouping;
	xmlNode *root;

	root = xmlDocGetRootElement(e_table->specification);
	grouping = xmlCopyNode(e_xml_get_child_by_name(root, "grouping"), TRUE);
	return grouping;
}

static xmlDoc *
et_build_tree (ETable *e_table)
{
	xmlDoc *doc;
	xmlNode *root;
	doc = xmlNewDoc( "1.0" );
	if ( doc == NULL )
		return NULL;
	root = xmlNewDocNode(doc, NULL, "ETableSpecification", NULL);
	xmlDocSetRootElement(doc, root);
	xmlAddChild(root, et_build_column_spec(e_table));
	xmlAddChild(root, et_build_grouping_spec(e_table));
	return doc;
}

gchar *
e_table_get_specification (ETable *e_table)
{
	xmlDoc *doc = et_build_tree (e_table);
	xmlChar *buffer;
	gint size;
	xmlDocDumpMemory(doc,
			 &buffer,
			 &size);
	xmlFreeDoc(doc);
	return buffer;
}

void
e_table_save_specification (ETable *e_table, gchar *filename)
{
	xmlDoc *doc = et_build_tree (e_table);
	xmlSaveFile(filename, doc);
	xmlFreeDoc(doc);
}


static void
e_table_class_init (GtkObjectClass *object_class)
{
	e_table_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = et_destroy;
}

E_MAKE_TYPE(e_table, "ETable", ETable, e_table_class_init, e_table_init, PARENT_TYPE);

