/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <stdio.h>
#include <string.h>
#include <gnome.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include "e-util/e-cursors.h"
#include "e-canvas.h"
#include "e-table-simple.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-checkbox.h"
#include "e-table.h"
#include "e-reflow.h"
#include "e-minicard.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "table-test.h"

#define COLS 4

/* Here we define the initial layout of the table.  This is an xml
   format that allows you to change the initial ordering of the
   columns or to do sorting or grouping initially.  This specification
   shows all 5 columns, but moves the importance column nearer to the
   front.  It also sorts by the "Full Name" column (ascending.)
   Sorting and grouping take the model column as their arguments
   (sorting is specified by the "column" argument to the leaf elemnt. */
#define INITIAL_SPEC "<ETableSpecification>                    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
		<column> 2 </column>     			       \
		<column> 3 </column>     			       \
	</columns-shown>                 			       \
	<grouping> <leaf column=\"1\" ascending=\"1\"/> </grouping>    \
</ETableSpecification>"

char *headers[COLS] = {
  "Email",
  "Full Name",
  "Address",
  "Phone"
};

typedef struct _address address;
typedef enum _rows rows;

struct _address {
	gchar *email;
	gchar *full_name;
	gchar *street;
	gchar *phone;
};

enum _rows {
	EMAIL,
	FULL_NAME,
	STREET,
	PHONE,
	LAST_COL
};

/* Virtual Column list:
   0   Email
   1   Full Name
   2   Street
   3   Phone
*/



static address **data;
static int data_count;
static ETableModel *e_table_model = NULL;
static GtkWidget *e_table;
static int window_count = 0;

/*
 * ETableSimple callbacks
 * These are the callbacks that define the behavior of our custom model.
 */

/* Since our model is a constant size, we can just return its size in
   the column and row count fields. */

/* This function returns the number of columns in our ETableModel. */
static int
my_col_count (ETableModel *etc, void *data)
{
	return COLS;
}

/* This function returns the number of rows in our ETableModel. */
static int
my_row_count (ETableModel *etc, void *data)
{
	return data_count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
my_value_at (ETableModel *etc, int col, int row, void *unused)
{
	if ( col >= LAST_COL || row >= data_count )
		return NULL;
	switch (col) {
	case EMAIL:
		return data[row]->email;
	case FULL_NAME:
		return data[row]->full_name;
	case STREET:
		return data[row]->street;
	case PHONE:
		return data[row]->phone;
	default:
		return NULL;
	}
}

/* This function sets the value at a particular point in our ETableModel. */
static void
my_set_value_at (ETableModel *etc, int col, int row, const void *val, void *unused)
{
	if ( col >= LAST_COL || row >= data_count )
		return;
	switch (col) {
	case EMAIL:
		g_free (data[row]->email);
		data[row]->email = g_strdup (val);	
		break;
	case FULL_NAME:
		g_free (data[row]->full_name);
		data[row]->full_name = g_strdup (val);	
		break;
	case STREET:
		g_free (data[row]->street);
		data[row]->street = g_strdup (val);	
		break;
	case PHONE:
		g_free (data[row]->phone);
		data[row]->phone = g_strdup (val);	
		break;
	default:
		return;
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
my_is_cell_editable (ETableModel *etc, int col, int row, void *data)
{
	return TRUE;
}

/* This function duplicates the value passed to it. */
static void *
my_duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
my_free_value (ETableModel *etc, int col, void *value, void *data)
{
	g_free(value);
}

/* This function is for when the model is unfrozen.  This can mostly
   be ignored for simple models.  */
static void
my_thaw (ETableModel *etc, void *data)
{
}

static int idle;

static gboolean
save(gpointer unused)
{
	int i;
	xmlDoc *document = xmlNewDoc("1.0");
	xmlNode *root;
	root = xmlNewDocNode(document, NULL, "address-book", NULL);
	xmlDocSetRootElement(document, root);
	for ( i = 0; i < data_count; i++ ) {
		xmlNode *xml_address = xmlNewChild(root, NULL, "address", NULL);
		if ( data[i]->email && *data[i]->email )
			xmlSetProp(xml_address, "email", data[i]->email);
		if ( data[i]->email && *data[i]->street )
			xmlSetProp(xml_address, "street", data[i]->street);
		if ( data[i]->email && *data[i]->full_name )
			xmlSetProp(xml_address, "full-name", data[i]->full_name);
		if ( data[i]->email && *data[i]->phone )
			xmlSetProp(xml_address, "phone", data[i]->phone);
	}
	xmlSaveFile ("addressbook.xml", document);
	idle = 0;
	/*	e_table_save_specification(E_TABLE(e_table), "spec"); */
	return FALSE;
}

static void
queue_save()
{
	if ( !idle )
		idle = g_idle_add(save, NULL);
}

static void
add_column (ETableModel *etc, address *newadd)
{
	data = g_realloc(data, (++data_count) * sizeof(address *));
	data[data_count - 1] = newadd;
	queue_save();
	if ( etc )
		e_table_model_changed(etc);
}


static void
init_data()
{
	data = NULL;
	data_count = 0;
}

static void
create_model()
{
	xmlDoc *document;
	xmlNode *xml_addressbook;
	xmlNode *xml_address;

	/* First we fill in the simple data. */
	init_data();
	if ( g_file_exists("addressbook.xml") ) {
		document = xmlParseFile("addressbook.xml");
		xml_addressbook = xmlDocGetRootElement(document);
		for (xml_address = xml_addressbook->childs; xml_address; xml_address = xml_address->next) {
			char *datum;
			address *newadd;

			newadd = g_new(address, 1);

			datum = xmlGetProp(xml_address, "email");
			if ( datum ) {
				newadd->email = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->email = g_strdup("");

			datum = xmlGetProp(xml_address, "street");
			if ( datum ) {
				newadd->street = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->street = g_strdup("");

			datum = xmlGetProp(xml_address, "full-name");
			if ( datum ) {
				newadd->full_name = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->full_name = g_strdup("");

			datum = xmlGetProp(xml_address, "phone");
			if ( datum ) {
				newadd->phone = g_strdup(datum);
				xmlFree(datum);
			} else
				newadd->phone = g_strdup("");
			add_column (NULL, newadd);
		}
		xmlFreeDoc(document);
	}
	
	
	e_table_model = e_table_simple_new (
					    my_col_count, my_row_count, my_value_at,
					    my_set_value_at, my_is_cell_editable,
					    my_duplicate_value, my_free_value, my_thaw, NULL);
	
	gtk_signal_connect(GTK_OBJECT(e_table_model), "model_changed",
			   GTK_SIGNAL_FUNC(queue_save), NULL);
	gtk_signal_connect(GTK_OBJECT(e_table_model), "model_row_changed",
			   GTK_SIGNAL_FUNC(queue_save), NULL);
	gtk_signal_connect(GTK_OBJECT(e_table_model), "model_cell_changed",
			   GTK_SIGNAL_FUNC(queue_save), NULL);
}

static void
add_address_cb(GtkWidget *button, gpointer data)
{
	address *newadd = g_new(address, 1);
	newadd->email = g_strdup("");
	newadd->phone = g_strdup("");
	newadd->full_name = g_strdup("");
	newadd->street = g_strdup("");
	add_column (e_table_model, newadd);
}

typedef struct {
	GtkAllocation last_alloc;
	GnomeCanvasItem *reflow;
	GtkWidget *canvas;
	GnomeCanvasItem *rect;
} reflow_demo;

static void
rebuild_reflow(ETableModel *model, gpointer data)
{
	int i;
	reflow_demo *demo = (reflow_demo *)data;
	gtk_object_destroy(GTK_OBJECT(demo->reflow));
	demo->reflow = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( demo->canvas ) ),
					      e_reflow_get_type(),
					      "x", (double) 0,
					      "y", (double) 0,
					      "height", (double) 100,
					      "minimum_width", (double) 100,
					      NULL );

	for ( i = 0; i < data_count; i++ )
		{
			GnomeCanvasItem *item;
			item = gnome_canvas_item_new( GNOME_CANVAS_GROUP(demo->reflow),
						      e_minicard_get_type(),
						      "model", e_table_model,
						      "row", i,
						      NULL);
			e_reflow_add_item(E_REFLOW(demo->reflow), item);
		}
	e_canvas_item_request_reflow(demo->reflow);
}

static void destroy_callback(GtkWidget *app, gpointer data)
{
	reflow_demo *demo = (reflow_demo *)data;
	g_free(demo);
	window_count --;
	if ( window_count <= 0 )
		exit(0);
}

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, gpointer data)
{
  double width;
  reflow_demo *demo = (reflow_demo *)data;
  demo->last_alloc = *allocation;
  gnome_canvas_item_set( demo->reflow,
			 "height", (double) allocation->height,
			 NULL );
  gnome_canvas_item_set( demo->reflow,
			 "minimum_width", (double) allocation->width,
			 NULL );
  gtk_object_get(GTK_OBJECT(demo->reflow),
		 "width", &width,
		 NULL);
  width = MAX(width, allocation->width);
  gnome_canvas_set_scroll_region(GNOME_CANVAS( demo->canvas ), 0, 0, width, allocation->height );
  gnome_canvas_item_set( demo->rect,
			 "x2", (double) width,
			 "y2", (double) allocation->height,
			 NULL );
}

static void resize(ECanvas *canvas, gpointer data)
{
	double width;
	reflow_demo *demo = (reflow_demo *)data;
  	gtk_object_get(GTK_OBJECT(demo->reflow),
		       "width", &width,
		       NULL);
	width = MAX(width, demo->last_alloc.width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS(demo->canvas), 0, 0, width, demo->last_alloc.height );
	gnome_canvas_item_set( demo->rect,
			       "x2", (double) width,
			       "y2", (double) demo->last_alloc.height,
			       NULL );	
}

static void
create_reflow()
{
	GtkWidget *window, *frame;
	GtkWidget *button;
	GtkWidget *vbox;
	GtkWidget *inner_vbox;
	GtkWidget *scrollbar;
	int i;
	reflow_demo *demo = g_new(reflow_demo, 1);

	/* Next we create our model.  This uses the functions we defined
	   earlier. */
	if ( e_table_model == NULL )
		create_model();

	/* Here we create a window for our new table.  This window
	   will get shown and the person will be able to test their
	   item. */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	/* This frame is simply to get a bevel around our table. */
	frame = gtk_frame_new (NULL);
	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	inner_vbox = gtk_vbox_new(FALSE, 0);
	demo->canvas = e_canvas_new();
	demo->rect = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( demo->canvas ) ),
					    gnome_canvas_rect_get_type(),
					    "x1", (double) 0,
					    "y1", (double) 0,
					    "x2", (double) 100,
					    "y2", (double) 100,
					    "fill_color", "white",
					    NULL );
	demo->reflow = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( demo->canvas ) ),
					      e_reflow_get_type(),
					      "x", (double) 0,
					      "y", (double) 0,
					      "height", (double) 100,
					      "minimum_width", (double) 100,
					      NULL );
	
	gtk_signal_connect( GTK_OBJECT( demo->canvas ), "reflow",
			    GTK_SIGNAL_FUNC( resize ),
			    ( gpointer ) demo);
	
	for ( i = 0; i < data_count; i++ )
		{
			GnomeCanvasItem *item;
			item = gnome_canvas_item_new( GNOME_CANVAS_GROUP(demo->reflow),
						      e_minicard_get_type(),
						      "model", e_table_model,
						      "row", i,
						      NULL);
			e_reflow_add_item(E_REFLOW(demo->reflow), item);
		}
	gnome_canvas_set_scroll_region ( GNOME_CANVAS( demo->canvas ),
					 0, 0,
					 100, 100 );

	scrollbar = gtk_hscrollbar_new(gtk_layout_get_hadjustment(GTK_LAYOUT(demo->canvas)));

	/* Connect the signals */
	gtk_signal_connect( GTK_OBJECT( window ), "destroy",
			    GTK_SIGNAL_FUNC( destroy_callback ),
			    ( gpointer ) demo );

	gtk_signal_connect( GTK_OBJECT( demo->canvas ), "size_allocate",
			    GTK_SIGNAL_FUNC( allocate_callback ),
			    ( gpointer ) demo );

	gdk_window_set_back_pixmap( GTK_LAYOUT(demo->canvas)->bin_window, NULL, FALSE);
	
	vbox = gtk_vbox_new(FALSE, 0);
	
	button = gtk_button_new_with_label("Add address");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(add_address_cb), demo);

	gtk_signal_connect(GTK_OBJECT( e_table_model), "model_changed",
			   GTK_SIGNAL_FUNC(rebuild_reflow), demo);

	/* Build the gtk widget hierarchy. */
	gtk_box_pack_start(GTK_BOX(inner_vbox), demo->canvas, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(inner_vbox), scrollbar, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), inner_vbox);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	/* Size the initial window. */
	gtk_widget_set_usize (window, 200, 200);
	/* Show it all. */
	gtk_widget_show_all (window);
	window_count ++;
}

static void e_table_destroy_callback(GtkWidget *app, gpointer data)
{
	window_count --;
	if ( window_count <= 0 )
		exit(0);
}

/* We create a window containing our new table. */
static void
create_table()
{
	GtkWidget *window, *frame;
	ECell *cell_left_just;
	ETableHeader *e_table_header;
	GtkWidget *button;
	GtkWidget *vbox;
	int i;
	/* Next we create our model.  This uses the functions we defined
	   earlier. */
	if ( e_table_model == NULL )
		create_model();
	/*
	  Next we create a header.  The ETableHeader is used in two
	  different way.  The first is the full_header.  This is the
	  list of possible columns in the view.  The second use is
	  completely internal.  Many of the ETableHeader functions are
	  for that purpose.  The only functions we really need are
	  e_table_header_new and e_table_header_add_col.

	  First we create the header.  */
	e_table_header = e_table_header_new ();
	
	/* Next we have to build renderers for all of the columns.
	   Since all our columns are text columns, we can simply use
	   the same renderer over and over again.  If we had different
	   types of columns, we could use a different renderer for
	   each column. */
	cell_left_just = e_cell_text_new (e_table_model, NULL, GTK_JUSTIFY_LEFT, TRUE);
		
	/* Next we create a column object for each view column and add
	   them to the header.  We don't create a column object for
	   the importance column since it will not be shown. */
	for (i = 0; i < COLS; i++){
		/* Create the column. */
		ETableCol *ecol = e_table_col_new (
						   i, headers [i],
						   80, 20, cell_left_just,
						   g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	/* Here we create a window for our new table.  This window
	   will get shown and the person will be able to test their
	   item. */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	/* This frame is simply to get a bevel around our table. */
	frame = gtk_frame_new (NULL);
	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	e_table = e_table_new_from_spec_file (e_table_header, e_table_model, "spec");

	gtk_signal_connect(GTK_OBJECT(E_TABLE(e_table)->sort_info), "sort_info_changed",
			   GTK_SIGNAL_FUNC(queue_save), NULL);

	gtk_signal_connect(GTK_OBJECT(E_TABLE(e_table)->header), "structure_change",
			   GTK_SIGNAL_FUNC(queue_save), NULL);
	gtk_signal_connect(GTK_OBJECT(E_TABLE(e_table)->header), "dimension_change",
			   GTK_SIGNAL_FUNC(queue_save), NULL);

	gtk_signal_connect( GTK_OBJECT( window ), "destroy",
			    GTK_SIGNAL_FUNC( e_table_destroy_callback ),
			    NULL );

	vbox = gtk_vbox_new(FALSE, 0);
	
	button = gtk_button_new_with_label("Add address");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(add_address_cb), NULL);

	/* Build the gtk widget hierarchy. */
	gtk_container_add (GTK_CONTAINER (frame), e_table);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	/* Size the initial window. */
	gtk_widget_set_usize (window, 200, 200);
	/* Show it all. */
	gtk_widget_show_all (window);
	window_count ++;
}

/* This is the main function which just initializes gnome and call our create_table function */

int
main (int argc, char *argv [])
{
	gnome_init ("TableExample", "TableExample", argc, argv);
	e_cursors_init ();

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	create_table();
	create_table();
	create_table();
	create_reflow();
	create_reflow();

	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}







