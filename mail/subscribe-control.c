/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * subscribe-control.c: Subscribe control top level component
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "subscribe-control.h"
#include "e-util/e-html-utils.h"
#include <gtkhtml/gtkhtml.h>
#include <gal/util/e-util.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-tree-simple.h>

#include "art/mark.xpm"

#define PARENT_TYPE (gtk_table_get_type ())

#define ETABLE_SPEC "<ETableSpecification>                    	       \
	<columns-shown>                  			       \
		<column> 1 </column>     			       \
		<column> 0 </column>     			       \
		<column> 2 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                         \
</ETableSpecification>"

enum {
	COL_FOLDER_NAME,
	COL_FOLDER_SUBSCRIBED,
	COL_FOLDER_DESCRIPTION,
	COL_LAST
};

/*
 * Virtual Column list:
 * 0   Folder Name
 * 1   Subscribed
 * 2   Description
 */
char *headers [COL_LAST] = {
  "Folder",
  "Subscribed",
  "Description",
};

static GtkObjectClass *subscribe_control_parent_class;

void
subscribe_select_all (BonoboUIHandler *uih,
		      void *user_data, const char *path)
{
}

void
subscribe_unselect_all (BonoboUIHandler *uih,
			void *user_data, const char *path)
{
}

void
subscribe_folder (GtkWidget *widget, gpointer user_data)
{
}

void
unsubscribe_folder (GtkWidget *widget, gpointer user_data)
{
}

gboolean
subscribe_control_set_uri (SubscribeControl *subscribe_control,
			   const char *uri)
{
	printf ("set_uri (%s) called\n", uri);
	return TRUE;
}


/* HTML Helpers */
static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	if (GTK_LAYOUT (widget)->height > 90)
		requisition->height = 90;
	else
		requisition->height = GTK_LAYOUT (widget)->height;
}

/* Returns a GtkHTML which is already inside a GtkScrolledWindow. If
 * @white is TRUE, the GtkScrolledWindow will be inside a GtkFrame.
 */
static GtkWidget *
html_new (gboolean white)
{
	GtkWidget *html, *scrolled, *frame;
	GtkStyle *style;
	
	html = gtk_html_new ();
	GTK_LAYOUT (html)->height = 0;
	gtk_signal_connect (GTK_OBJECT (html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	style = gtk_rc_get_style (html);
	if (style) {
		gtk_html_set_default_background_color (GTK_HTML (html),
						       white ? &style->white :
						       &style->bg[0]);
	}
	gtk_widget_set_sensitive (html, FALSE);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), html);
	
	if (white) {
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame),
					   GTK_SHADOW_ETCHED_IN);
		gtk_container_add (GTK_CONTAINER (frame), scrolled);
		gtk_widget_show_all (frame);
	} else
		gtk_widget_show_all (scrolled);
	
	return html;
}

static void
put_html (GtkHTML *html, char *text)
{
	GtkHTMLStream *handle;
	
	text = e_text_to_html (text, (E_TEXT_TO_HTML_CONVERT_NL |
				      E_TEXT_TO_HTML_CONVERT_SPACES |
				      E_TEXT_TO_HTML_CONVERT_URLS));
	handle = gtk_html_begin (html);
	gtk_html_write (html, handle, "<HTML><BODY>", 12);
	gtk_html_write (html, handle, text, strlen (text));
	gtk_html_write (html, handle, "</BODY></HTML>", 14);
	g_free (text);
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}



/* etable stuff for the subscribe ui */

static int
etable_col_count (ETableModel *etm, void *data)
{
	return COL_LAST;
}

static void*
etable_duplicate_value (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
etable_free_value (ETableModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void*
etable_init_value (ETableModel *etm, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
etable_value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	return !(val && *(char *)val);
}

static char*
etable_value_to_string (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup(val);
}

static GdkPixbuf*
etree_icon_at (ETreeModel *etree, ETreePath *path, void *model_data)
{
	return NULL; /* XXX no icons for now */
}

static void*
etree_value_at (ETreeModel *etree, ETreePath *path, int col, void *model_data)
{
	if (col == COL_FOLDER_NAME)
		return "Folder Name";
	else if (col == COL_FOLDER_DESCRIPTION)
		return "Folder Description";
	else /* COL_FOLDER_SUBSCRIBED */
		return 1; /* XXX */
}

static void
etree_set_value_at (ETreeModel *etree, ETreePath *path, int col, const void *val, void *model_data)
{
	/* nothing */
}

static gboolean
etree_is_editable (ETreeModel *etree, ETreePath *path, int col, void *model_data)
{
	return FALSE;
}



#define EXAMPLE_DESCR "And the beast shall come forth surrounded by a roiling cloud of vengeance.\n" \
"  The house of the unbelievers shall be razed and they shall be scorched to the\n" \
"          earth. Their tags shall blink until the end of days. \n" \
"                 from The Book of Mozilla, 12:10"

static void
subscribe_control_gui_init (SubscribeControl *sc)
{
	int i;
	ECell *cells[3];
	ETableHeader *e_table_header;
	GdkPixbuf *toggles[2];

	sc->description = html_new (TRUE);
	put_html (GTK_HTML (sc->description), EXAMPLE_DESCR);

	gtk_table_attach (
		GTK_TABLE (sc), sc->description->parent->parent,
		0, 1, 0, 1,
		GTK_FILL | GTK_EXPAND,
		0,
		0, 0);

	sc->model = e_tree_simple_new (etable_col_count,
				       etable_duplicate_value,
				       etable_free_value,
				       etable_init_value,
				       etable_value_is_empty,
				       etable_value_to_string,
				       etree_icon_at,
				       etree_value_at,
				       etree_set_value_at,
				       etree_is_editable,
				       sc);

	sc->root = e_tree_model_node_insert (sc->model, NULL,
					     0, NULL);

	e_tree_model_root_node_set_visible (sc->model, FALSE);


	for (i = 0; i < 100; i ++)
		e_tree_model_node_insert (sc->model, sc->root,
					  0, NULL);

	e_table_header = e_table_header_new ();

	toggles[0] = NULL;
	toggles[1] = gdk_pixbuf_new_from_xpm_data ((const char **)mark_xpm);

	cells[2] = e_cell_text_new (E_TABLE_MODEL(sc->model), NULL, GTK_JUSTIFY_LEFT);
	cells[1] = e_cell_toggle_new (0, 2, toggles);
	cells[0] = e_cell_tree_new (E_TABLE_MODEL(sc->model),
				    NULL, NULL,
				    /*tree_expanded_pixbuf, tree_unexpanded_pixbuf,*/
				    TRUE, cells[2]);

	for (i = 0; i < COL_LAST; i++) {
		/* Create the column. */
		ETableCol *ecol;

		if (i == 1)
			ecol = e_table_col_new_with_pixbuf (i, toggles[1],
							    0, gdk_pixbuf_get_width (toggles[1]),
							    cells[i], g_str_compare, FALSE);
		else 
			ecol = e_table_col_new (i, headers [i],
						80, 20,
						cells[i],
						g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	sc->table = e_table_scrolled_new (e_table_header, E_TABLE_MODEL(sc->model), ETABLE_SPEC);

	gtk_table_attach (
		GTK_TABLE (sc), sc->table,
		0, 1, 1, 3,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND,
		0, 0);

	gtk_widget_show_all (GTK_WIDGET(sc));
}

static void
subscribe_control_destroy (GtkObject *object)
{
	SubscribeControl *subscribe_control;
	CORBA_Environment ev;

	subscribe_control = SUBSCRIBE_CONTROL (object);

	CORBA_exception_init (&ev);

	if (subscribe_control->shell != CORBA_OBJECT_NIL)
		CORBA_Object_release (subscribe_control->shell, &ev);

	CORBA_exception_free (&ev);

	subscribe_control_parent_class->destroy (object);
}

static void
subscribe_control_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = subscribe_control_destroy;

	subscribe_control_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
subscribe_control_init (GtkObject *object)
{
}

static void
subscribe_control_construct (GtkObject *object)
{
	SubscribeControl *sc = SUBSCRIBE_CONTROL (object);

	/*
	 * Setup parent class fields.
	 */ 
	GTK_TABLE (sc)->homogeneous = FALSE;
	gtk_table_resize (GTK_TABLE (sc), 1, 2);

	/*
	 * Our instance data
	 */

	subscribe_control_gui_init (sc);
}

GtkWidget *
subscribe_control_new (const Evolution_Shell shell)
{
	static int serial = 0;
	CORBA_Environment ev;
	SubscribeControl *subscribe_control;

	CORBA_exception_init (&ev);

	subscribe_control = gtk_type_new (subscribe_control_get_type ());

	subscribe_control_construct (GTK_OBJECT (subscribe_control));
	subscribe_control->uri = NULL;
	subscribe_control->serial = serial++;

	subscribe_control->shell = CORBA_Object_duplicate (shell, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		subscribe_control->shell = CORBA_OBJECT_NIL;
		gtk_widget_destroy (GTK_WIDGET (subscribe_control));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return GTK_WIDGET (subscribe_control);
}

E_MAKE_TYPE (subscribe_control, "SubscribeControl", SubscribeControl, subscribe_control_class_init, subscribe_control_init, PARENT_TYPE);

