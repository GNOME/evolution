/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * subscribe-dialog.c: Subscribe dialog
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "subscribe-dialog.h"
#include "e-util/e-html-utils.h"
#include "e-title-bar.h"
#include <gtkhtml/gtkhtml.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-tree-simple.h>
#include <gal/e-paned/e-hpaned.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include "art/empty.xpm"
#include "art/mark.xpm"

#define DEFAULT_STORAGE_SET_WIDTH         150
#define DEFAULT_WIDTH                     500
#define DEFAULT_HEIGHT                    300

#define PARENT_TYPE (gtk_object_get_type ())

#define ETABLE_SPEC "<ETableSpecification>                    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
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

/* per node structure */
typedef struct {
	gboolean subscribed;
} SubscribeData;


static GtkObjectClass *subscribe_dialog_parent_class;

static void subscribe_close (BonoboUIHandler *uih, void *user_data, const char *path);
static void subscribe_select_all (BonoboUIHandler *uih, void *user_data, const char *path);
static void subscribe_unselect_all (BonoboUIHandler *uih, void *user_data, const char *path);
static void subscribe_folder (GtkWidget *widget, gpointer user_data);
static void unsubscribe_folder (GtkWidget *widget, gpointer user_data);
static void subscribe_refresh_list (GtkWidget *widget, gpointer user_data);
static void subscribe_search (GtkWidget *widget, gpointer user_data);

static BonoboUIVerb verbs [] = {
	/* File Menu */
	BONOBO_UI_VERB ("FileCloseWin", subscribe_close),

	/* Edit Menu */
	BONOBO_UI_VERB ("EditSelectAll", subscribe_select_all),
	BONOBO_UI_VERB ("EditUnSelectAll", subscribe_unselect_all),
	
	/* Folder Menu / Toolbar */
	BONOBO_UI_VERB ("SubscribeFolder", subscribe_folder),
	BONOBO_UI_VERB ("UnsubscribeFolder", unsubscribe_folder),

	/* Toolbar Specific */
	BONOBO_UI_VERB ("RefreshList", subscribe_refresh_list),

	BONOBO_UI_VERB_END
};

static void
set_pixmap (Bonobo_UIContainer container,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_return_if_fail (pixbuf != NULL);

	bonobo_ui_util_set_pixbuf (container, xml_path, pixbuf);

	gdk_pixbuf_unref (pixbuf);

	g_free (path);
}

static void
update_pixmaps (Bonobo_UIContainer container)
{
	set_pixmap (container, "/Toolbar/SubscribeFolder", "fetch-mail.png"); /* XXX */
	set_pixmap (container, "/Toolbar/UnsubscribeFolder", "compose-message.png"); /* XXX */
	set_pixmap (container, "/Toolbar/RefreshList", "forward.png"); /* XXX */
}

static GtkWidget*
make_folder_search_widget (GtkSignalFunc start_search_func,
			   gpointer user_data_for_search)
{
	GtkWidget *search_hbox = gtk_hbox_new (FALSE, 0);
	GtkWidget *search_entry = gtk_entry_new ();

	if (start_search_func) {
		gtk_signal_connect (GTK_OBJECT (search_entry), "activate",
				    start_search_func,
				    user_data_for_search);
	}
	
	/* add the search entry to the our search_vbox */
	gtk_box_pack_start (GTK_BOX (search_hbox),
			    gtk_label_new(_("Display folders containing:")),
			    FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (search_hbox), search_entry,
			    FALSE, TRUE, 3);

	return search_hbox;
}



static void
subscribe_close (BonoboUIHandler *uih,
		 void *user_data, const char *path)
{
	SubscribeDialog *sc = (SubscribeDialog*)user_data;

	gtk_widget_destroy (sc->app);
}

static void
subscribe_select_all (BonoboUIHandler *uih,
		      void *user_data, const char *path)
{
}

static void
subscribe_unselect_all (BonoboUIHandler *uih,
			void *user_data, const char *path)
{
}

static void
subscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath *node = e_tree_model_node_at_row (sc->model, model_row);
	SubscribeData *data = e_tree_model_node_get_data (sc->model, node);

	printf ("subscribe: row %d, node_data %p\n", model_row,
		e_tree_model_node_get_data (sc->model, node));

	data->subscribed = TRUE;

	e_tree_model_node_changed (sc->model, node);
}

static void
subscribe_folder (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_table_selected_row_foreach (E_TABLE_SCROLLED(sc->etable)->table,
				      subscribe_folder_foreach, sc);
}

static void
unsubscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath *node = e_tree_model_node_at_row (sc->model, model_row);
	SubscribeData *data = e_tree_model_node_get_data (sc->model, node);

	printf ("unsubscribe: row %d, node_data %p\n", model_row,
		e_tree_model_node_get_data (sc->model, node));

	data->subscribed = FALSE;

	e_tree_model_node_changed (sc->model, node);
}


static void
unsubscribe_folder (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_table_selected_row_foreach (E_TABLE_SCROLLED(sc->etable)->table,
				      unsubscribe_folder_foreach, sc);
}

static void
subscribe_refresh_list (GtkWidget *widget, gpointer user_data)
{
	printf ("subscribe_refresh_list\n");
}

static void
subscribe_search (GtkWidget *widget, gpointer user_data)
{
	char* search_pattern = e_utf8_gtk_entry_get_text(GTK_ENTRY(widget));

	printf ("subscribe_search (%s)\n", search_pattern);

	g_free (search_pattern);
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
	SubscribeData *data = e_tree_model_node_get_data (etree, path);
	if (col == COL_FOLDER_NAME)
		return "Folder Name";
	else if (col == COL_FOLDER_DESCRIPTION)
		return "Folder Description";
	else /* COL_FOLDER_SUBSCRIBED */
		return GINT_TO_POINTER(data->subscribed);
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
subscribe_dialog_gui_init (SubscribeDialog *sc)
{
	int i;
	ECell *cells[3];
	ETableHeader *e_table_header;
	GdkPixbuf *toggles[2];
	BonoboUIComponent *component;
	Bonobo_UIContainer container;
	GtkWidget         *folder_search_widget, *vbox, *storage_set_title_bar;
	BonoboControl     *search_control;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* Construct the app */
	sc->app = bonobo_win_new ("subscribe-dialog", "Manage Subscriptions");

	/* Build the menu and toolbar */
	sc->uih = bonobo_ui_handler_new ();
	if (!sc->uih) {
		g_message ("subscribe_dialog_gui_init(): eeeeek, could not create the UI handler!");
		return;
	}

	bonobo_ui_handler_set_app (sc->uih, BONOBO_WIN (sc->app));

	/* set up the bonobo stuff */
	component = bonobo_ui_compat_get_component (sc->uih);
	container = bonobo_ui_compat_get_container (sc->uih);
	
	bonobo_ui_component_add_verb_list_with_data (
		component, verbs, sc);

	bonobo_ui_container_freeze (container, NULL);

	bonobo_ui_util_set_ui (component, container,
			       EVOLUTION_DATADIR,
			       "evolution-subscribe.xml",
			       "evolution-subscribe");

	update_pixmaps (container);

	bonobo_ui_container_thaw (container, NULL);

	sc->storage_set_control = Evolution_Shell_create_storage_set_view (sc->shell, &ev);
	sc->storage_set_view = Bonobo_Unknown_query_interface (sc->storage_set_control,
							       "IDL:Evolution/StorageSetView:1.0",
							       &ev);

	/* we just want to show storages */
	Evolution_StorageSetView__set_show_folders (sc->storage_set_view, FALSE, &ev);

	sc->storage_set_view_widget = bonobo_widget_new_control_from_objref (sc->storage_set_control,
									     container);

	sc->table = gtk_table_new (1, 2, FALSE);

	sc->hpaned = e_hpaned_new ();
	vbox = gtk_vbox_new (FALSE, 0);

	storage_set_title_bar = e_title_bar_new (_("Storages"));
	e_title_bar_show_button (E_TITLE_BAR (storage_set_title_bar), FALSE);

	gtk_box_pack_start (GTK_BOX (vbox), storage_set_title_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sc->storage_set_view_widget, TRUE, TRUE, 0);

	e_paned_add1 (E_PANED (sc->hpaned), vbox);
	e_paned_add2 (E_PANED (sc->hpaned), sc->table);
	e_paned_set_position (E_PANED (sc->hpaned), DEFAULT_STORAGE_SET_WIDTH);

	bonobo_win_set_contents (BONOBO_WIN (sc->app), sc->hpaned);

	folder_search_widget = make_folder_search_widget (subscribe_search, sc);
	gtk_widget_show_all (folder_search_widget);
	search_control = bonobo_control_new (folder_search_widget);

	bonobo_ui_container_object_set (container,
					"/Toolbar/FolderSearch",
					bonobo_object_corba_objref (BONOBO_OBJECT (search_control)),
					NULL);
					
	/* set our our contents */
	sc->description = html_new (TRUE);
	put_html (GTK_HTML (sc->description), EXAMPLE_DESCR);

	gtk_table_attach (
		GTK_TABLE (sc->table), sc->description->parent->parent,
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


	for (i = 0; i < 100; i ++) {
		SubscribeData *data = g_new (SubscribeData, 1);
		data->subscribed = FALSE;
		e_tree_model_node_insert (sc->model, sc->root,
					  0, data);
	}

	e_table_header = e_table_header_new ();

	toggles[0] = gdk_pixbuf_new_from_xpm_data ((const char **)empty_xpm);
	toggles[1] = gdk_pixbuf_new_from_xpm_data ((const char **)mark_xpm);

	cells[2] = e_cell_text_new (E_TABLE_MODEL(sc->model), NULL, GTK_JUSTIFY_LEFT);
	cells[1] = e_cell_toggle_new (0, 2, toggles);
	cells[0] = e_cell_tree_new (E_TABLE_MODEL(sc->model),
				    NULL, NULL,
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

	sc->etable = e_table_scrolled_new (e_table_header, E_TABLE_MODEL(sc->model), ETABLE_SPEC);

	gtk_object_set (GTK_OBJECT (E_TABLE_SCROLLED (sc->etable)->table),
			"cursor_mode", E_TABLE_CURSOR_LINE,
			NULL);
	gtk_object_set (GTK_OBJECT (cells[2]),
			"bold_column", COL_FOLDER_SUBSCRIBED,
			NULL);
	gtk_table_attach (
		GTK_TABLE (sc->table), sc->etable,
		0, 1, 1, 3,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND,
		0, 0);
	
	gtk_widget_show (sc->description);
	gtk_widget_show (sc->etable);
	gtk_widget_show (sc->table);
	gtk_widget_show (sc->storage_set_view_widget);
	gtk_widget_show (storage_set_title_bar);
	gtk_widget_show (vbox);
	gtk_widget_show (sc->hpaned);

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (
		GTK_WINDOW (sc->app),
		DEFAULT_WIDTH, DEFAULT_HEIGHT);
}

static void
subscribe_dialog_destroy (GtkObject *object)
{
	SubscribeDialog *subscribe_dialog;

	subscribe_dialog = SUBSCRIBE_DIALOG (object);

	subscribe_dialog_parent_class->destroy (object);
}

static void
subscribe_dialog_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = subscribe_dialog_destroy;

	subscribe_dialog_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
subscribe_dialog_init (GtkObject *object)
{
}

static void
subscribe_dialog_construct (GtkObject *object, Evolution_Shell shell)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (object);

	/*
	 * Our instance data
	 */
	sc->shell = shell;

	subscribe_dialog_gui_init (sc);
}

GtkWidget *
subscribe_dialog_new (Evolution_Shell shell)
{
	SubscribeDialog *subscribe_dialog;

	subscribe_dialog = gtk_type_new (subscribe_dialog_get_type ());

	subscribe_dialog_construct (GTK_OBJECT (subscribe_dialog), shell);

	return GTK_WIDGET (subscribe_dialog->app);
}

E_MAKE_TYPE (subscribe_dialog, "SubscribeDialog", SubscribeDialog, subscribe_dialog_class_init, subscribe_dialog_init, PARENT_TYPE);

