/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table.c: A graphical view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *   Chris Lahey (clahey@helixcode.com)
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
#include <gnome-xml/xmlmemory.h>
#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"
#include "e-util/e-canvas.h"
#include "e-table.h"
#include "e-table-header-item.h"
#include "e-table-subset.h"
#include "e-table-item.h"
#include "e-table-group.h"
#include "e-table-group-leaf.h"

#define COLUMN_HEADER_HEIGHT 16
#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE gtk_table_get_type ()

static GtkObjectClass *e_table_parent_class;

enum {
	ROW_SELECTION,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_TABLE_DRAW_GRID,
	ARG_TABLE_DRAW_FOCUS,
	ARG_MODE_SPREADSHEET,
	ARG_LENGTH_THRESHOLD,
};

static gint et_signals [LAST_SIGNAL] = { 0, };

static void e_table_fill_table (ETable *e_table, ETableModel *model);
static gboolean changed_idle (gpointer data);

static void
et_destroy (GtkObject *object)
{
	ETable *et = E_TABLE (object);
	

	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_model_change_id);
	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_row_change_id);
	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_cell_change_id);
	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_row_inserted_id);
	gtk_signal_disconnect (GTK_OBJECT (et->model),
			       et->table_row_deleted_id);
	if (et->group_info_change_id)
		gtk_signal_disconnect (GTK_OBJECT (et->sort_info),
				       et->group_info_change_id);

	gtk_object_unref (GTK_OBJECT (et->model));
	gtk_object_unref (GTK_OBJECT (et->full_header));
	gtk_object_unref (GTK_OBJECT (et->header));
	gtk_object_unref (GTK_OBJECT (et->sort_info));
	gtk_widget_destroy (GTK_WIDGET (et->header_canvas));
	gtk_widget_destroy (GTK_WIDGET (et->table_canvas));

	if (et->rebuild_idle_id) {
		g_source_remove (et->rebuild_idle_id);
		et->rebuild_idle_id = 0;
	}
	
	(*e_table_parent_class->destroy)(object);
}

static void
e_table_init (GtkObject *object)
{
	ETable *e_table = E_TABLE (object);
	GtkTable *gtk_table = GTK_TABLE (object);

	gtk_table->homogeneous = FALSE;
	
	e_table->sort_info = NULL;
	e_table->group_info_change_id = 0;

	e_table->draw_grid = 1;
	e_table->draw_focus = 1;
	e_table->spreadsheet = 1;
	e_table->length_threshold = 200;
	
	e_table->need_rebuild = 0;
	e_table->rebuild_idle_id = 0;
}

static void
header_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc, ETable *e_table)
{
	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (e_table->header_canvas),
		0, 0, alloc->width, COLUMN_HEADER_HEIGHT);
}

static void
sort_info_changed (ETableSortInfo *info, ETable *et)
{
	et->need_rebuild = TRUE;
	if (!et->rebuild_idle_id)
		et->rebuild_idle_id = g_idle_add_full (20, changed_idle, et, NULL);
}

static void
e_table_setup_header (ETable *e_table)
{
	e_table->header_canvas = GNOME_CANVAS (e_canvas_new ());
	
	gtk_widget_show (GTK_WIDGET (e_table->header_canvas));

	e_table->header_item = gnome_canvas_item_new (
		gnome_canvas_root (e_table->header_canvas),
		e_table_header_item_get_type (),
		"ETableHeader", e_table->header,
		"sort_info", e_table->sort_info,
		NULL);

	gtk_signal_connect (
		GTK_OBJECT (e_table->header_canvas), "size_allocate",
		GTK_SIGNAL_FUNC (header_canvas_size_allocate), e_table);

	gtk_widget_set_usize (GTK_WIDGET (e_table->header_canvas), -1, COLUMN_HEADER_HEIGHT);
}

static void
table_canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc,
			    ETable *e_table)
{
	gdouble width;
	width = alloc->width;

	gtk_object_set (GTK_OBJECT (e_table->group),
			"minimum_width", width,
			NULL);
	gtk_object_set (GTK_OBJECT (e_table->header),
			"width", width,
			NULL);
	
}

static void
table_canvas_reflow (GnomeCanvas *canvas, ETable *e_table)
{
	gdouble height, width;
	GtkAllocation *alloc = &(GTK_WIDGET (canvas)->allocation);

	gtk_object_get (GTK_OBJECT (e_table->group),
			"height", &height,
			"width", &width,
			NULL);
	/* I have no idea why this needs to be -1, but it works. */
	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (e_table->table_canvas),
		0, 0, MAX((int)width, alloc->width) - 1, MAX ((int)height, alloc->height) - 1);
}

static void
group_row_selection (ETableGroup *etg, int row, gboolean selected, ETable *et)
{
	gtk_signal_emit (GTK_OBJECT (et),
			 et_signals [ROW_SELECTION],
			 row, selected);
}

static gboolean
changed_idle (gpointer data)
{
	ETable *et = E_TABLE (data);

	if (et->need_rebuild) {
		gtk_object_destroy (GTK_OBJECT (et->group));
		et->group = e_table_group_new (GNOME_CANVAS_GROUP (et->table_canvas->root),
					       et->full_header,
					       et->header,
					       et->model,
					       et->sort_info,
					       0);
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(et->group),
				      "drawgrid", et->draw_grid,
				      "drawfocus", et->draw_focus,
				      "spreadsheet", et->spreadsheet,
				      "length_threshold", et->length_threshold,
				      NULL);
		gtk_signal_connect (GTK_OBJECT (et->group), "row_selection",
				    GTK_SIGNAL_FUNC (group_row_selection), et);
		e_table_fill_table (et, et->model);
		
		gtk_object_set (GTK_OBJECT (et->group),
				"minimum_width", (double) GTK_WIDGET (et->table_canvas)->allocation.width,
				NULL);
	}

	et->need_rebuild = 0;
	et->rebuild_idle_id = 0;

	return FALSE;
}

static void
et_table_model_changed (ETableModel *model, ETable *et)
{
	et->need_rebuild = TRUE;
	if (!et->rebuild_idle_id)
		et->rebuild_idle_id = g_idle_add_full (20, changed_idle, et, NULL);
}

static void
et_table_row_changed (ETableModel *table_model, int row, ETable *et)
{
	if (!et->need_rebuild) {
		if (e_table_group_remove (et->group, row))
			e_table_group_add (et->group, row);
	}
}

static void
et_table_cell_changed (ETableModel *table_model, int view_col, int row, ETable *et)
{
	et_table_row_changed (table_model, row, et);
}

static void
et_table_row_inserted (ETableModel *table_model, int row, ETable *et)
{
	if (!et->need_rebuild) {
		e_table_group_add (et->group, row);
	}
}

static void
et_table_row_deleted (ETableModel *table_model, int row, ETable *et)
{
	if (!et->need_rebuild) {
		e_table_group_remove (et->group, row);
	}
}

static void
e_table_setup_table (ETable *e_table, ETableHeader *full_header, ETableHeader *header,
		     ETableModel *model)
{
	e_table->table_canvas = GNOME_CANVAS (e_canvas_new ());
	gtk_signal_connect (
		GTK_OBJECT (e_table->table_canvas), "size_allocate",
		GTK_SIGNAL_FUNC (table_canvas_size_allocate), e_table);
	
	gtk_signal_connect (GTK_OBJECT(e_table->table_canvas), "reflow",
			    GTK_SIGNAL_FUNC (table_canvas_reflow), e_table);
				 
	gtk_widget_show (GTK_WIDGET (e_table->table_canvas));
	
	e_table->group = e_table_group_new (
		GNOME_CANVAS_GROUP (e_table->table_canvas->root),
		full_header, header,
		model, e_table->sort_info, 0);

	gnome_canvas_item_set(GNOME_CANVAS_ITEM(e_table->group),
			      "drawgrid", e_table->draw_grid,
			      "drawfocus", e_table->draw_focus,
			      "spreadsheet", e_table->spreadsheet,
			      "length_threshold", e_table->length_threshold,
			      NULL);

	gtk_signal_connect (GTK_OBJECT (e_table->group), "row_selection",
			    GTK_SIGNAL_FUNC(group_row_selection), e_table);
	
	e_table->table_model_change_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_changed",
		GTK_SIGNAL_FUNC (et_table_model_changed), e_table);

	e_table->table_row_change_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_row_changed",
		GTK_SIGNAL_FUNC (et_table_row_changed), e_table);

	e_table->table_cell_change_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_cell_changed",
		GTK_SIGNAL_FUNC (et_table_cell_changed), e_table);
	
	e_table->table_row_inserted_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_row_inserted",
		GTK_SIGNAL_FUNC (et_table_row_inserted), e_table);
	
	e_table->table_row_deleted_id = gtk_signal_connect (
		GTK_OBJECT (model), "model_row_deleted",
		GTK_SIGNAL_FUNC (et_table_row_deleted), e_table);
}

static void
e_table_fill_table (ETable *e_table, ETableModel *model)
{
	e_table_group_add_all (e_table->group);
}

static ETableHeader *
et_xml_to_header (ETable *e_table, ETableHeader *full_header, xmlNode *xmlColumns)
{
	ETableHeader *nh;
	xmlNode *column;
	const int max_cols = e_table_header_count (full_header);

	g_return_val_if_fail (e_table, NULL);
	g_return_val_if_fail (full_header, NULL);
	g_return_val_if_fail (xmlColumns, NULL);
	
	nh = e_table_header_new ();

	for (column = xmlColumns->childs; column; column = column->next) {
		gchar *content;
		int col;

		content = xmlNodeListGetString (column->doc, column->childs, 1);
		col = atoi (content);
		xmlFree (content);

		if (col >= max_cols)
			continue;

		e_table_header_add_column (nh, e_table_header_get_column (full_header, col), -1);
	}

	return nh;
}

static void
et_grouping_xml_to_sort_info (ETable *table, xmlNode *grouping)
{
	int i;

	g_return_if_fail (table!=NULL);
	g_return_if_fail (grouping!=NULL);	
	
	table->sort_info = e_table_sort_info_new ();
	
	gtk_object_ref (GTK_OBJECT (table->sort_info));
	gtk_object_sink (GTK_OBJECT (table->sort_info));

	i = 0;
	for (grouping = grouping->childs; grouping && !strcmp (grouping->name, "group"); grouping = grouping->childs) {
		ETableSortColumn column;
		column.column = e_xml_get_integer_prop_by_name (grouping, "column");
		column.ascending = e_xml_get_integer_prop_by_name (grouping, "ascending");
		e_table_sort_info_grouping_set_nth(table->sort_info, i++, column);
	}
	i = 0;
	for (; grouping && !strcmp (grouping->name, "leaf"); grouping = grouping->childs) {
		ETableSortColumn column;
		column.column = e_xml_get_integer_prop_by_name (grouping, "column");
		column.ascending = e_xml_get_integer_prop_by_name (grouping, "ascending");
		e_table_sort_info_sorting_set_nth(table->sort_info, i++, column);
	}

	table->group_info_change_id = 
		gtk_signal_connect (GTK_OBJECT (table->sort_info), "group_info_changed", 
				    GTK_SIGNAL_FUNC (sort_info_changed), table);
}

static ETable *
et_real_construct (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
		   xmlDoc *xmlSpec)
{
	xmlNode *xmlRoot;
	xmlNode *xmlColumns;
	xmlNode *xmlGrouping;
	int no_header;
	int row = 0;
	
	GtkWidget *scrolledwindow;

	xmlRoot = xmlDocGetRootElement (xmlSpec);
	xmlColumns = e_xml_get_child_by_name (xmlRoot, "columns-shown");
	xmlGrouping = e_xml_get_child_by_name (xmlRoot, "grouping");

	if ((xmlColumns == NULL) || (xmlGrouping == NULL))
		return NULL;

	no_header = e_xml_get_integer_prop_by_name(xmlRoot, "no-header");
	
	e_table->full_header = full_header;
	gtk_object_ref (GTK_OBJECT (full_header));

	e_table->model = etm;
	gtk_object_ref (GTK_OBJECT (etm));

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	e_table->header = et_xml_to_header (e_table, full_header, xmlColumns);
	et_grouping_xml_to_sort_info (e_table, xmlGrouping);
	
	gtk_object_set(GTK_OBJECT(e_table->header),
		       "sort_info", e_table->sort_info,
		       NULL);

	if (!no_header) {
		e_table_setup_header (e_table);
	}
	e_table_setup_table (e_table, full_header, e_table->header, etm);
	e_table_fill_table (e_table, etm);

	scrolledwindow = gtk_scrolled_window_new (
		gtk_layout_get_hadjustment (GTK_LAYOUT (e_table->table_canvas)),
		gtk_layout_get_vadjustment (GTK_LAYOUT (e_table->table_canvas)));

	gtk_layout_get_vadjustment (GTK_LAYOUT (e_table->table_canvas))->step_increment = 20;
	gtk_adjustment_changed(gtk_layout_get_vadjustment (GTK_LAYOUT (e_table->table_canvas)));
			       
	
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	
	gtk_container_add (
		GTK_CONTAINER (scrolledwindow),
		GTK_WIDGET (e_table->table_canvas));
	gtk_widget_show (scrolledwindow);
	
	if (!no_header) {
		/*
		 * The header
		 */
		gtk_table_attach (
				  GTK_TABLE (e_table), GTK_WIDGET (e_table->header_canvas),
				  0, 1, 0, 1,
				  GTK_FILL | GTK_EXPAND,
				  GTK_FILL, 0, 0);
		row ++;
	}
	/*
	 * The body
	 */
	gtk_table_attach (
		GTK_TABLE (e_table), GTK_WIDGET (scrolledwindow),
		0, 1, 0 + row, 1 + row,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND, 0, 0);
		
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	return e_table;
}

ETable *
e_table_construct (ETable *e_table, ETableHeader *full_header,
		   ETableModel *etm, const char *spec)
{
	xmlDoc *xmlSpec;
	char *copy;
	copy = g_strdup (spec);

	xmlSpec = xmlParseMemory (copy, strlen(copy));
	e_table = et_real_construct (e_table, full_header, etm, xmlSpec);
	xmlFreeDoc (xmlSpec);
	g_free (copy);

	return e_table;
}

ETable *
e_table_construct_from_spec_file (ETable *e_table, ETableHeader *full_header, ETableModel *etm,
				  const char *filename)
{
	xmlDoc *xmlSpec;

	xmlSpec = xmlParseFile (filename);
	e_table = et_real_construct (e_table, full_header, etm, xmlSpec);
	xmlFreeDoc (xmlSpec);

	return e_table;
}

GtkWidget *
e_table_new (ETableHeader *full_header, ETableModel *etm, const char *spec)
{
	ETable *e_table;

	e_table = gtk_type_new (e_table_get_type ());

	e_table = e_table_construct (e_table, full_header, etm, spec);
		
	return GTK_WIDGET (e_table);
}

GtkWidget *
e_table_new_from_spec_file (ETableHeader *full_header, ETableModel *etm, const char *filename)
{
	ETable *e_table;

	e_table = gtk_type_new (e_table_get_type ());

	e_table = e_table_construct_from_spec_file (e_table, full_header, etm, filename);
		
	return (GtkWidget *) e_table;
}

static xmlNode *
et_build_column_spec (ETable *e_table)
{
	xmlNode *columns_shown;
	gint i;
	gint col_count;

	columns_shown = xmlNewNode (NULL, "columns-shown");

	col_count = e_table_header_count (e_table->header);
	for (i = 0; i < col_count; i++){
		gchar *text = g_strdup_printf ("%d", e_table_header_index(e_table->header, i));
		xmlNewChild (columns_shown, NULL, "column", text);
		g_free (text);
	}

	return columns_shown;
}

static xmlNode *
et_build_grouping_spec (ETable *e_table)
{
	xmlNode *node;
	xmlNode *grouping;
	int i;
	const int sort_count = e_table_sort_info_sorting_get_count (e_table->sort_info);
	const int group_count = e_table_sort_info_grouping_get_count (e_table->sort_info);
	
	grouping = xmlNewNode (NULL, "grouping");
	node = grouping;

	for (i = 0; i < group_count; i++) {
		ETableSortColumn column = e_table_sort_info_grouping_get_nth(e_table->sort_info, i);
		xmlNode *new_node = xmlNewChild(node, NULL, "group", NULL);

		e_xml_set_integer_prop_by_name (new_node, "column", column.column);
		e_xml_set_integer_prop_by_name (new_node, "ascending", column.ascending);
		node = new_node;
	}

	for (i = 0; i < sort_count; i++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(e_table->sort_info, i);
		xmlNode *new_node = xmlNewChild(node, NULL, "leaf", NULL);
		
		e_xml_set_integer_prop_by_name (new_node, "column", column.column);
		e_xml_set_integer_prop_by_name (new_node, "ascending", column.ascending);
		node = new_node;
	}

	return grouping;
}

static xmlDoc *
et_build_tree (ETable *e_table)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc ("1.0");
	if (doc == NULL)
		return NULL;
	
	root = xmlNewDocNode (doc, NULL, "ETableSpecification", NULL);
	xmlDocSetRootElement (doc, root);
	xmlAddChild (root, et_build_column_spec (e_table));
	xmlAddChild (root, et_build_grouping_spec (e_table));

	return doc;
}

gchar *
e_table_get_specification (ETable *e_table)
{
	xmlDoc *doc;
	xmlChar *buffer;
	gint size;

	doc = et_build_tree (e_table);
	xmlDocDumpMemory (doc, &buffer, &size);
	xmlFreeDoc (doc);

	return buffer;
}

void
e_table_save_specification (ETable *e_table, gchar *filename)
{
	xmlDoc *doc = et_build_tree (e_table);

	xmlSaveFile (filename, doc);
	xmlFreeDoc (doc);
}

void
e_table_select_row (ETable *e_table, int row)
{
	
}

static void
et_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETable *etable = E_TABLE (o);

	switch (arg_id){
	case ARG_TABLE_DRAW_GRID:
		GTK_VALUE_BOOL (*arg) = etable->draw_grid;
		break;

	case ARG_TABLE_DRAW_FOCUS:
		GTK_VALUE_BOOL (*arg) = etable->draw_focus;
		break;
	}
}

typedef struct {
	char     *arg;
	gboolean  setting;
} bool_closure;

static void
et_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETable *etable = E_TABLE (o);
	
	switch (arg_id){
	case ARG_LENGTH_THRESHOLD:
		etable->length_threshold = GTK_VALUE_INT (*arg);
		if (etable->group) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etable->group),
					       "length_threshold", GTK_VALUE_INT (*arg),
					       NULL);
		}
		break;
		
	case ARG_TABLE_DRAW_GRID:
		etable->draw_grid = GTK_VALUE_BOOL (*arg);
		if (etable->group) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etable->group),
					       "drawgrid", GTK_VALUE_BOOL (*arg),
					       NULL);
		}
		break;

	case ARG_TABLE_DRAW_FOCUS:
		etable->draw_focus = GTK_VALUE_BOOL (*arg);
		if (etable->group) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etable->group),
					"drawfocus", GTK_VALUE_BOOL (*arg),
					NULL);
		}
		break;

	case ARG_MODE_SPREADSHEET:
		etable->spreadsheet = GTK_VALUE_BOOL (*arg);
		if (etable->group) {
			gnome_canvas_item_set (GNOME_CANVAS_ITEM(etable->group),
					"spreadsheet", GTK_VALUE_BOOL (*arg),
					NULL);
		}
		break;
	}
}
	
static void
e_table_class_init (GtkObjectClass *object_class)
{
	ETableClass *klass = E_TABLE_CLASS(object_class);
	e_table_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = et_destroy;
	object_class->set_arg = et_set_arg;
	object_class->get_arg = et_get_arg;
	
	klass->row_selection = NULL;

	et_signals [ROW_SELECTION] =
		gtk_signal_new ("row_selection",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableClass, row_selection),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, et_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ETable::drawgrid", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_TABLE_DRAW_GRID);
	gtk_object_add_arg_type ("ETable::drawfocus", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_TABLE_DRAW_FOCUS);
	gtk_object_add_arg_type ("ETable::spreadsheet", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_MODE_SPREADSHEET);
	gtk_object_add_arg_type ("ETable::length_threshold", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_LENGTH_THRESHOLD);
	
	
}

E_MAKE_TYPE(e_table, "ETable", ETable, e_table_class_init, e_table_init, PARENT_TYPE);

