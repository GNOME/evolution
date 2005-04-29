/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-table.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <gnome.h>

#include "gal/widgets/e-cursors.h"
#include "gal/widgets/e-canvas.h"

#include "e-table-simple.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-table.h"
#include "e-table-config.h"

#include "table-test.h"

char buffer [1024];
char **column_labels;
char ***table_data;
int cols = 0;
int lines = 0;
int lines_alloc = 0;

static void
parse_headers (void)
{
	char *p, *s;
	int in_value = 0, i;

	fgets (buffer, sizeof (buffer)-1, stdin);

	for (p = buffer; *p; p++){
		if (*p == ' ' || *p == '\t'){
			if (in_value){
				cols++;
				in_value = 0;
			}
		} else
			in_value = 1;
	}
	if (in_value)
		cols++;

	if (!cols){
		fprintf (stderr, "No columns in first row\n");
		exit (1);
	}

	column_labels = g_new0 (char *, cols);

	p = buffer;
	for (i = 0; (s = strtok (p, " \t")) != NULL; i++){
		column_labels [i] = g_strdup (s);
		if (strchr (column_labels [i], '\n'))
			*strchr (column_labels [i], '\n') = 0;
		p = NULL;
	}

	printf ("%d headers:\n", cols);
	for (i = 0; i < cols; i++){
		printf ("header %d: %s\n", i, column_labels [i]);
	}
}

static char **
load_line (char *buffer, int cols)
{
	char **line = g_new0 (char *, cols);
	char *p;
	int i;

	for (i = 0; i < cols; i++){
		p = strtok (buffer, " \t\n");
		if (p == NULL){
			for (; i < cols; i++)
				line [i] = g_strdup ("");
			return line;
		} else
			line [i] = g_strdup (p);
		buffer = NULL;
	}
	return line;
}

static void
append_line (char **line)
{
	if (lines <= lines_alloc){
		lines_alloc = lines + 50;
		table_data = g_renew (char **, table_data, lines_alloc);
	}
	table_data [lines] = line;
	lines++;
}

static void
load_data (void)
{
	int i;

	{
		static int loaded;

		if (loaded)
			return;

		loaded = TRUE;
	}


	parse_headers ();

	while (fgets (buffer, sizeof (buffer)-1, stdin) != NULL){
		char **line;

		if (buffer [0] == '\n')
			continue;
		line = load_line (buffer, cols);
		append_line (line);
	}

	for (i = 0; i < lines; i++){
		int j;

		printf ("Line %d: ", i);
		for (j = 0; j < cols; j++)
			printf ("[%s] ", table_data [i][j]);
		printf ("\n");
	}
}

/*
 * ETableSimple callbacks
 */
static int
col_count (ETableModel *etc, void *data)
{
	return cols;
}

static int
row_count (ETableModel *etc, void *data)
{
	return lines;
}

static void
append_row (ETableModel *etm, ETableModel *model, int row, void *data)
{
	abort ();
}

static void *
value_at (ETableModel *etc, int col, int row, void *data)
{
	g_assert (col < cols);
	g_assert (row < lines);

	fprintf (stderr, "value_at[%d,%d]\n", col, row);

	return (void *) table_data [row][col];
}

static void
set_value_at (ETableModel *etc, int col, int row, const void *val, void *data)
{
	g_assert (col < cols);
	g_assert (row < lines);

	g_free (table_data [row][col]);
	table_data [row][col] = g_strdup (val);

	printf ("Value at %d,%d set to %s\n", col, row, (char *) val);
}

static gboolean
is_cell_editable (ETableModel *etc, int col, int row, void *data)
{
	return TRUE;
}

static gboolean
has_save_id (ETableModel *etm, void *data)
{
	return FALSE;
}

static char *
get_save_id (ETableModel *etm, int row, void *data)
{
	abort ();
}

static void *
duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup (value);
}

static void
free_value (ETableModel *etc, int col, void *value, void *data)
{
	g_free (value);
}

static void *
initialize_value (ETableModel *etc, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
value_is_empty (ETableModel *etc, int col, const void *value, void *data)
{
	return !(value && *(char *)value);
}

static char *
value_to_string (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup(value);
}

#ifdef BIT_ROT
static void
set_canvas_size (GnomeCanvas *canvas, GtkAllocation *alloc)
{
	gnome_canvas_set_scroll_region (canvas, 0, 0, alloc->width, alloc->height);
}

void
table_browser_test (void)
{
	GtkWidget *canvas, *window;
	ETableModel *e_table_model;
	ETableHeader *e_table_header;
	ECell *cell_left_just;
	GnomeCanvasItem *group;
	int i;
	int priority = 20;

	load_data ();

	/*
	 * Data model
	 */
	e_table_model = e_table_simple_new (
		col_count, row_count, append_row,

		value_at, set_value_at, is_cell_editable,

		has_save_id, get_save_id,

		duplicate_value, free_value,
		initialize_value, value_is_empty,
		value_to_string,
		NULL);

	/*
	 * Header
	 */
	e_table_header = e_table_header_new ();
	cell_left_just = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);

	for (i = 0; i < cols; i++){
		ETableCol *ecol = e_table_col_new (
			i, column_labels [i],
			1.0, 20, cell_left_just,
			g_str_compare, TRUE,
			priority);

		e_table_header_add_column (e_table_header, ecol, i);
	}

	/*
	 * Setup GUI
	 */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	canvas = e_canvas_new ();

	g_signal_connect (canvas, "size_allocate",
			  G_CALLBACK (set_canvas_size), NULL);

	gtk_container_add (GTK_CONTAINER (window), canvas);
	gtk_widget_show_all (window);
	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_header_item_get_type (),
		"ETableHeader", e_table_header,
		NULL);

	group = gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		gnome_canvas_group_get_type (),
		"x", 30.0,
		"y", 30.0,
		NULL);

	gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (group),
		e_table_item_get_type (),
		"ETableHeader", e_table_header,
		"ETableModel", e_table_model,
		"drawgrid", TRUE,
		"drawfocus", TRUE,
#if 0
		"spreadsheet", TRUE,
#endif
		NULL);
}
#endif

static void
save_spec (GtkWidget *button, ETable *e_table)
{
#ifdef BIT_ROT
	e_table_save_specification (e_table, "e-table-test.xml");
#endif
}

#ifdef BIT_ROT
static void
row_selection_test (ETable *table, int row, gboolean selected)
{
	if (selected)
		g_print ("Row %d selected\n", row);
	else
		g_print ("Row %d unselected\n", row);
}
#endif

static void
toggle_grid (void *nothing, ETable *etable)
{
	static gboolean shown;

	g_object_get (etable, "drawgrid", &shown, NULL);
	g_object_set (etable, "drawgrid", !shown, NULL);
}

static void
do_e_table_demo (const char *state)
{
	GtkWidget *e_table, *window, *frame, *vbox, *button, *bhide;
	ECell *cell_left_just;
	ETableHeader *full_header;
	int i;
	GString *spec;

	/*
	 * Data model
	 */
	static ETableModel *e_table_model = NULL;

	if (e_table_model == NULL)
		e_table_model =
			e_table_simple_new (col_count, row_count, append_row,
					    value_at, set_value_at, is_cell_editable,
					    has_save_id, get_save_id,
					    duplicate_value, free_value,
					    initialize_value, value_is_empty,
					    value_to_string,
					    NULL);

	full_header = e_table_header_new ();
	cell_left_just = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);

	spec = g_string_new ("\
<ETableSpecification \
cursor-mode=\"line\" \
selection-mode=\"browse\" \
draw-focus=\"true\">");
	for (i = 0; i < cols; i++) {
		char *colspec =
			g_strdup_printf ("\
  <ETableColumn model_col=\"%d\" \
_title=\"%s\" \
minimum_width=\"20\" \
resizable=\"true\" \
cell=\"string\" \
compare=\"string\"/>\n", i, column_labels[i]);
		g_string_append (spec, colspec);
		g_free (colspec);
	}
	g_string_append (spec, "</ETableSpecification>");
	e_table = e_table_new (e_table_model, NULL, spec->str, state);

	/* This makes value_at not called just to determine row height.  */
	g_object_set (e_table,
		      "uniform_row_height", 1,
		      NULL);

	g_string_free (spec, TRUE);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	frame = gtk_frame_new (NULL);
#ifdef BIT_ROT
	g_signal_connect (e_table, "row_selection",
			  G_CALLBACK(row_selection_test), NULL);
#endif

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), e_table, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_add (GTK_CONTAINER (window), frame);

#if 0
	/*
	 * gadgets
	 */
	button = gtk_button_new_with_label ("Save spec");
	g_signal_connect (button, "clicked",
			  G_CALLBACK (save_spec), e_table);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	bhide = gtk_button_new_with_label ("Toggle Grid");
	g_signal_connect (bhide, "clicked",
			  G_CALLBACk (toggle_grid), e_table);
	gtk_box_pack_start (GTK_BOX (vbox), bhide, FALSE, FALSE, 0);
#endif

	gtk_widget_set_usize (window, 400, 200);
	gtk_widget_show_all (window);

#ifdef BIT_ROT
	if (getenv ("TEST")){
		e_table_do_gui_config (NULL, E_TABLE(e_table));
	}
#endif
}

void
e_table_test (void)
{
	load_data ();

	if (1){/*getenv ("DO")){*/
	  do_e_table_demo ("\
<ETableState>\n\
    <column source=\"0\"/>\n\
    <column source=\"1\"/>\n\
    <column source=\"2\"/>\n\
    <column source=\"3\"/>\n\
    <column source=\"4\"/>\n\
    <grouping></grouping>\n\
</ETableState>");
#if 0
	  do_e_table_demo ("<ETableSpecification> <columns-shown> <column> 0 </column> <column> 0 </column> <column> 1 </column> <column> 2 </column> <column> 3 </column> <column> 4 </column> </columns-shown> <grouping> <group column=\"3\" ascending=\"true\"> <group column=\"4\" ascending=\"false\"> <leaf column=\"2\" ascending=\"true\"/> </group> </group> </grouping> </ETableSpecification>");
	  do_e_table_demo ("<ETableSpecification> <columns-shown> <column> 0 </column> <column> 1 </column> <column> 2 </column> <column> 3 </column> <column> 4 </column> </columns-shown> <grouping> <group column=\"4\" ascending=\"true\"> <leaf column=\"2\" ascending=\"true\"/> </group> </grouping> </ETableSpecification>");
	  do_e_table_demo ("<ETableSpecification> <columns-shown> <column> 0 </column> <column> 1 </column> <column> 2 </column> <column> 3 </column> <column> 4 </column> </columns-shown> <grouping> <group column=\"3\" ascending=\"true\"> <leaf column=\"2\" ascending=\"true\"/> </group> </grouping> </ETableSpecification>");
#endif
	}
}
