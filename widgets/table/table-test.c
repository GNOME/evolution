/*
 * Test code for the ETable package
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gnome.h>
#include "e-table-simple.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cursors.h"
#include "e-cell-text.h"

char buffer [1024];
char **column_labels;
char ***table_data;
int cols = 0;
int lines = 0;
int lines_alloc = 0;

static void
parse_headers ()
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
load_data ()
{
	int i;

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

static const char *
col_name (ETableModel *etc, int col, void *data)
{
	g_assert (col < cols);

	return column_labels [col];
}

static int
row_count (ETableModel *etc, void *data)
{
	return lines;
}

static void *
value_at (ETableModel *etc, int col, int row, void *data)
{
	g_assert (col < cols);
	g_assert (row < lines);

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

static void
set_canvas_size (GnomeCanvas *canvas, GtkAllocation *alloc)
{
	gnome_canvas_set_scroll_region (canvas, 0, 0, alloc->width, alloc->height);
}

int
main (int argc, char *argv [])
{
	GtkWidget *canvas, *window;
	ETableModel *e_table_model;
	ETableHeader *e_table_header;
	ECell *cell_left_just;
	int i;
	
	gnome_init ("TableTest", "TableTest", argc, argv);
	e_cursors_init ();
	
	load_data ();

	/*
	 * Data model
	 */
	e_table_model = e_table_simple_new (
		col_count, col_name, row_count, value_at,
		set_value_at, is_cell_editable, NULL);

	/*
	 * Header
	 */
	e_table_header = e_table_header_new ();
	cell_left_just = e_cell_text_new (e_table_model, NULL, GTK_JUSTIFY_LEFT);
	
	for (i = 0; i < cols; i++){
		ETableCol *ecol = e_table_col_new (
			column_labels [i], 80, 20, cell_left_just,
			g_str_equal, TRUE);

		e_table_header_add_column (e_table_header, ecol, i);
	}

	/*
	 * Setup GUI
	 */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	canvas = gnome_canvas_new ();

	gtk_signal_connect (GTK_OBJECT (canvas), "size_allocate",
			    GTK_SIGNAL_FUNC (set_canvas_size), NULL);
	
	gtk_container_add (GTK_CONTAINER (window), canvas);
	gtk_widget_show_all (window);
	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_header_item_get_type (),
		"ETableHeader", e_table_header,
		"x",  0,
		"y",  0,
		NULL);

	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_item_get_type (),
		"ETableHeader", e_table_header,
		"ETableModel", e_table_model,
		"x",  10,
		"y",  30,
		"drawgrid", TRUE,
		"drawfocus", TRUE,
		"spreadsheet", TRUE,
		NULL);
	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
