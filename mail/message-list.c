/*
 * message-list.c: Displays the messages.
 *                 Implements CORBA's Evolution::MessageList
 *
 * Author:
 *     Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include "e-util/e-util.h"
#include "camel/camel-exception.h"
#include "message-list.h"
#include "Mail.h"
#include "widgets/e-table/e-table-header-item.h"
#include "widgets/e-table/e-table-item.h"

/*
 * Default sizes for the ETable display
 *
 */
#define N_CHARS(x) (CHAR_WIDTH * (x))

#define COL_ICON_WIDTH        16
#define COL_FROM_WIDTH        N_CHARS(24)
#define COL_FROM_WIDTH_MIN    32
#define COL_SUBJECT_WIDTH     N_CHARS(30)
#define COL_SUBJECT_WIDTH_MIN 32
#define COL_SENT_WIDTH        N_CHARS(20)
#define COL_SENT_WIDTH_MIN    32
#define COL_RECEIVE_WIDTH     N_CHARS(20)
#define COL_RECEIVE_WIDTH_MIN 32
#define COL_TO_WIDTH          N_CHARS(24)
#define COL_TO_WIDTH_MIN      32
#define COL_SIZE_WIDTH        N_CHARS(6)
#define COL_SIZE_WIDTH_MIN    32

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *message_list_parent_class;
static POA_Evolution_MessageList__vepv evolution_message_list_vepv;

/*
 * SimpleTableModel::col_count
 */
static int
ml_col_count (ETableModel *etm, void *data)
{
	return COL_LAST;
}

/*
 * SimpleTableModel::row_count
 */
static int
ml_row_count (ETableModel *etm, void *data)
{
	MessageList *message_list = data;
	CamelException *ex;
	int v;
	
	if (!message_list->folder)
		return 0;

	ex = camel_exception_new ();
	v = camel_folder_get_message_count (message_list->folder, ex);
	camel_exception_free (ex);
	
	return v;
}

static void *
ml_value_at (ETableModel *etm, int col, int row, void *data)
{
	static char buffer [10];

	
	switch (col){
	case COL_ONLINE_STATUS:
		return GINT_TO_POINTER (0);
		
	case COL_MESSAGE_STATUS:
		return GINT_TO_POINTER (1);
		
	case COL_PRIORITY:
		return GINT_TO_POINTER (1);
		
	case COL_ATTACHMENT:
		return GINT_TO_POINTER (0);
		
	case COL_FROM:
		return "miguel@dudical.com";
		
	case COL_SUBJECT:
		return "MONEY FAST!";
		
	case COL_SENT:
		return "sent";
		
	case COL_RECEIVE:
		return "receive";
			
	case COL_TO:
		return "dudes@server";
		
	case COL_SIZE:
		sprintf (buffer, "%d", 20);
		return buffer;
			
	default:
		g_assert_not_reached ();
	}
	return NULL;
}

static void
ml_set_value_at (ETableModel *etm, int col, int row, const void *value, void *data)
{
}

static gboolean
ml_is_cell_editable (ETableModel *etm, int col, int row, void *data)
{
	return FALSE;
}

static void *
ml_duplicate_value (ETableModel *etm, int col, const void *value, void *data)
{
  return value;
}

static void
ml_free_value (ETableModel *etm, int col, void *value, void *data)
{
}

static void
ml_thaw (ETableModel *etm, void *data)
{
	e_table_model_changed(etm);
}

static void
message_list_init_renderers (MessageList *message_list)
{
	g_assert (message_list);
	g_assert (message_list->table_model);
	
	message_list->render_text = e_cell_text_new (
		message_list->table_model,
		NULL, GTK_JUSTIFY_LEFT, FALSE);

	message_list->render_online_status = e_cell_checkbox_new ();
	message_list->render_message_status = e_cell_checkbox_new ();
	message_list->render_attachment = e_cell_checkbox_new ();

	/*
	 * FIXME: We need a real renderer here
	 */
	message_list->render_priority = e_cell_checkbox_new ();
}

#define CHAR_WIDTH 10
static void
message_list_init_header (MessageList *message_list)
{
	int i;
	
	/*
	 * FIXME:
	 *
	 * Use the font metric to compute this.
	 */
	
	message_list->header_model = e_table_header_new ();

	message_list->table_cols [COL_ONLINE_STATUS] =
		e_table_col_new (COL_ONLINE_STATUS, _("Online status"),
				 COL_ICON_WIDTH, COL_ICON_WIDTH,
				 message_list->render_online_status,
				 g_int_equal, FALSE);

	message_list->table_cols [COL_MESSAGE_STATUS] =
		e_table_col_new (COL_MESSAGE_STATUS, _("Message status"),
				 COL_ICON_WIDTH, COL_ICON_WIDTH,
				 message_list->render_message_status,
				 g_int_equal, FALSE);

	message_list->table_cols [COL_PRIORITY] =
		e_table_col_new (COL_PRIORITY, _("Priority"),
				 COL_ICON_WIDTH, COL_ICON_WIDTH,
				 message_list->render_priority,
				 g_int_equal, FALSE);
	
	message_list->table_cols [COL_ATTACHMENT] =
		e_table_col_new (COL_ATTACHMENT, _("Attachment"),
				 COL_ICON_WIDTH, COL_ICON_WIDTH,
				 message_list->render_attachment,
				 g_int_equal, FALSE);

	message_list->table_cols [COL_FROM] =
		e_table_col_new (COL_FROM, _("From"),
				 COL_FROM_WIDTH, COL_FROM_WIDTH_MIN,
				 message_list->render_text,
				 g_str_equal, TRUE);

	message_list->table_cols [COL_SUBJECT] =
		e_table_col_new (COL_SUBJECT, _("Subject"),
				 COL_SUBJECT_WIDTH, COL_SUBJECT_WIDTH_MIN,
				 message_list->render_text,
				 g_str_equal, TRUE);

	message_list->table_cols [COL_SENT] =
		e_table_col_new (COL_SENT, _("Sent"),
				 COL_SUBJECT_WIDTH, COL_SENT_WIDTH_MIN,
				 message_list->render_text,
				 g_str_equal, TRUE);

	message_list->table_cols [COL_RECEIVE] =
		e_table_col_new (COL_SENT, _("Receive"),
				 COL_RECEIVE_WIDTH, COL_RECEIVE_WIDTH_MIN,
				 message_list->render_text,
				 g_str_equal, TRUE);
	message_list->table_cols [COL_TO] =
		e_table_col_new (COL_TO, _("To"),
				 COL_TO_WIDTH, COL_TO_WIDTH_MIN,
				 message_list->render_text,
				 g_str_equal, TRUE);

	message_list->table_cols [COL_SIZE] =
		e_table_col_new (COL_SIZE, _("Size"),
				 COL_SIZE_WIDTH, COL_SIZE_WIDTH_MIN,
				 message_list->render_text,
				 g_str_equal, TRUE);

	/*
	 * Dummy init: It setups the headers to match the order in which
	 * they are defined.  In the future e-table widget will take care
	 * of this.
	 */
	for (i = 0; i < COL_LAST; i++) {
		gtk_object_ref(GTK_OBJECT(message_list->table_cols [i]));
		e_table_header_add_column (message_list->header_model,
					   message_list->table_cols [i], i);
	}
}

static void
set_header_size (GnomeCanvas *canvas, GtkAllocation *alloc)
{
	printf ("Here\n");
	gnome_canvas_set_scroll_region (canvas, 0, 0, alloc->width, alloc->height);
}

static void
set_content_size (GnomeCanvas *canvas, GtkAllocation *alloc)
{
	printf ("Here2\n");
        gnome_canvas_set_scroll_region (canvas, 0, 0, alloc->width, alloc->height);
}

#if 0
static GtkWidget *
make_etable (MessageList *message_list)
{
	GtkTable *t;
	GtkWidget *header, *content;
	
	t = (GtkTable *) gtk_table_new (0, 0, 0);
	gtk_widget_show (GTK_WIDGET (t));

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	header = gnome_canvas_new ();
	gtk_signal_connect (GTK_OBJECT (header), "size_allocate",
                            GTK_SIGNAL_FUNC (set_header_size), NULL);
	gtk_widget_set_usize (header, 300, 20);
	gtk_widget_show (header);
	content = gnome_canvas_new ();
	gtk_widget_set_usize (content, 300, 20);
        gtk_signal_connect (GTK_OBJECT (content), "size_allocate",
                            GTK_SIGNAL_FUNC (set_content_size), NULL);
	gtk_widget_show (content);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
	
	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (header)),
		e_table_header_item_get_type (),
		"ETableHeader", message_list->header_model,
		NULL);

	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (content)),
		e_table_item_get_type (),
		"ETableHeader", message_list->header_model,
		"ETableModel", message_list->table_model,
		"drawgrid", TRUE,
		"drawfocus", TRUE,
		"spreadsheet", TRUE,
		NULL);

	gtk_table_attach (t, header,
			  0, 1, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

	gtk_table_attach (t, content,
			  0, 1, 1, 2,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);


	return t;
}
#endif

/*
 * GtkObject::init
 */
static void
message_list_init (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);

	message_list->table_model = e_table_simple_new (
		ml_col_count, ml_row_count, ml_value_at,
		ml_set_value_at, ml_is_cell_editable, ml_duplicate_value, ml_free_value,
		ml_thaw, message_list);

	message_list_init_renderers (message_list);
	message_list_init_header (message_list);

	/*
	 * The etable
	 */
	
	message_list->etable = e_table_new (message_list->header_model, message_list->table_model, "<ETableSpecification> <columns-shown> <column> 0 </column> <column> 1 </column> <column> 2 </column> <column> 3 </column> <column> 4 </column> <column> 5 </column> <column> 6 </column> <column> 7 </column> <column> 8 </column> <column> 9 </column> </columns-shown> <grouping> <leaf column=\"0\" ascending=\"1\"/> </grouping> </ETableSpecification>");
	
	/*
	 * We do own the Etable, not some widget container
	 */
	gtk_object_ref (GTK_OBJECT (message_list->etable));
	gtk_object_sink (GTK_OBJECT (message_list->etable));
}

static void
message_list_destroy (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	int i;

	gtk_object_unref (GTK_OBJECT (message_list->table_model));
	gtk_object_unref (GTK_OBJECT (message_list->header_model));

	/*
	 * Renderers
	 */
	gtk_object_unref (GTK_OBJECT (message_list->render_text));
	gtk_object_unref (GTK_OBJECT (message_list->render_online_status));
	gtk_object_unref (GTK_OBJECT (message_list->render_message_status));
	gtk_object_unref (GTK_OBJECT (message_list->render_priority));
	gtk_object_unref (GTK_OBJECT (message_list->render_attachment));
	
	gtk_object_unref (GTK_OBJECT (message_list->etable));

	for (i = 0; i < COL_LAST; i++)
		gtk_object_unref (GTK_OBJECT (message_list->table_cols [i]));

	GTK_OBJECT_CLASS (message_list_parent_class)->destroy (object);
}

/*
 * CORBA method: Evolution::MessageList::select_message
 */
static void
MessageList_select_message (PortableServer_Servant _servant,
			    const CORBA_long message_number,
			    CORBA_Environment *ev)
{
	printf ("FIXME: select message method\n");
}

/*
 * CORBA method: Evolution::MessageList::open_message
 */
static void
MessageList_open_message (PortableServer_Servant _servant,
			  const CORBA_long message_number,
			  CORBA_Environment *ev)
{
	printf ("FIXME: open message method\n");
}

static POA_Evolution_MessageList__epv *
evolution_message_list_get_epv (void)
{
	POA_Evolution_MessageList__epv *epv;

	epv = g_new0 (POA_Evolution_MessageList__epv, 1);

	epv->select_message = MessageList_select_message;
	epv->open_message   = MessageList_open_message;

	return epv;
}

static void
message_list_corba_class_init (void)
{
	evolution_message_list_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	evolution_message_list_vepv.Evolution_MessageList_epv = evolution_message_list_get_epv ();
}

/*
 * GtkObjectClass::init
 */
static void
message_list_class_init (GtkObjectClass *object_class)
{
	message_list_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = message_list_destroy;

	message_list_corba_class_init ();
}

static void
message_list_construct (MessageList *message_list, Evolution_MessageList corba_message_list)
{
	bonobo_object_construct (BONOBO_OBJECT (message_list), corba_message_list);
}

static Evolution_MessageList
create_corba_message_list (BonoboObject *object)
{
	POA_Evolution_MessageList *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_MessageList *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &evolution_message_list_vepv;

	CORBA_exception_init (&ev);
	POA_Evolution_MessageList__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Evolution_MessageList) bonobo_object_activate_servant (object, servant);
}

BonoboObject *
message_list_new (void)
{
	Evolution_MessageList corba_object;
	MessageList *message_list;

	message_list = gtk_type_new (message_list_get_type ());

	corba_object = create_corba_message_list (BONOBO_OBJECT (message_list));
	if (corba_object == CORBA_OBJECT_NIL){
		gtk_object_destroy (GTK_OBJECT (message_list));
		return NULL;
	}

	message_list_construct (message_list, corba_object);

	return BONOBO_OBJECT (message_list);
}

void
message_list_set_folder (MessageList *message_list, CamelFolder *camel_folder)
{
	CamelException ex;
	gboolean folder_exists;

	g_return_if_fail (message_list != NULL);
	g_return_if_fail (camel_folder != NULL);
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	g_return_if_fail (CAMEL_IS_FOLDER (camel_folder));
	g_return_if_fail (camel_folder_has_summary_capability (camel_folder));
	
	
	camel_exception_init (&ex);
	
	if (message_list->folder)
		gtk_object_unref (GTK_OBJECT (message_list->folder));

	message_list->folder = camel_folder;
	
	folder_exists = camel_folder_exists (camel_folder, NULL);
	
	if (camel_exception_get_id (&ex)) {
	      printf ("Unable to test for folder existence: %s\n",
		      ex.desc?ex.desc:"unknown reason");
	      return;
	}
	
	if (!folder_exists) {	  
	    g_warning ("Folder does not exist, creating it\n");
	    /* 
	       if you don't want the directory to be created
	       automatically here remove this.
	    */
	    camel_folder_create (camel_folder, &ex);
	    if (camel_exception_get_id (&ex)) {
	      printf ("Unable to create folder: %s\n",
		      ex.desc?ex.desc:"unknown_reason");
	      return;
	    }
	   
	}

	
	camel_folder_open (camel_folder, FOLDER_OPEN_RW, &ex);
	if (camel_exception_get_id (&ex)) {
		printf ("Unable to open folder: %s\n",
			ex.desc?ex.desc:"unknown_reason");	  
		return;
	}
	
	message_list->folder_summary =
		camel_folder_get_summary (camel_folder, &ex);
	
	if (camel_exception_get_id (&ex)) {
		printf ("Unable to get summary: %s\n",
			ex.desc?ex.desc:"unknown_reason");	  	  
		return;
	}
	
	
	gtk_object_ref (GTK_OBJECT (camel_folder));

	printf ("Modelo cambio!\n");
	e_table_model_changed (message_list->table_model);
}

GtkWidget *
message_list_get_widget (MessageList *message_list)
{
	return message_list->etable;
}

E_MAKE_TYPE (message_list, "MessageList", MessageList, message_list_class_init, message_list_init, PARENT_TYPE);
