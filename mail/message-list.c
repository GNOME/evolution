/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * message-list.c: Displays the messages.
 *                 Implements CORBA's Evolution::MessageList
 *
 * Author:
 *     Miguel de Icaza (miguel@helixcode.com)
 *     Bertrand Guiheneuf (bg@aful.org)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include "e-util/e-util.h"
#include "camel/camel-exception.h"
#include <camel/camel-folder.h>
#include "message-list.h"
#include "Mail.h"
#include "widgets/e-table/e-table-header-item.h"
#include "widgets/e-table/e-table-item.h"

#include "pixmaps.h"

/*
 * Default sizes for the ETable display
 *
 */
#define N_CHARS(x) (CHAR_WIDTH * (x))

#define COL_ICON_WIDTH        16
#define COL_CHECK_BOX_WIDTH   16
#define COL_FROM_WIDTH        N_CHARS(24)
#define COL_FROM_WIDTH_MIN    32
#define COL_SUBJECT_WIDTH     N_CHARS(30)
#define COL_SUBJECT_WIDTH_MIN 32
#define COL_SENT_WIDTH        N_CHARS(4) 
#define COL_SENT_WIDTH_MIN    1
#define COL_RECEIVE_WIDTH     N_CHARS(20)
#define COL_RECEIVE_WIDTH_MIN 32
#define COL_TO_WIDTH          N_CHARS(24)
#define COL_TO_WIDTH_MIN      32
#define COL_SIZE_WIDTH        N_CHARS(6)
#define COL_SIZE_WIDTH_MIN    32

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *message_list_parent_class;
static POA_Evolution_MessageList__vepv evolution_message_list_vepv;

static void
on_row_selection_cmd (ETable *table, 
		      int row, 
		      gboolean selected,
		      gpointer user_data);


CamelMessageInfo *get_message_info(MessageList *message_list, gint row)
{
	CamelMessageInfo *info = NULL;

	if (message_list->matches) {
		char *uid;

		uid = g_list_nth_data(message_list->matches, row);
		if (uid) {
			info = camel_folder_summary_get_by_uid(message_list->folder, uid);
		} else {
			g_warning("trying to get data for nonexistant row %d", row);
		}
	} else {
		GPtrArray *msg_info_array;
		msg_info_array = camel_folder_summary_get_message_info 
			(message_list->folder, row, 1);
		if (msg_info_array && msg_info_array->len > 0) {
			info = msg_info_array->pdata[0];
		}
		g_ptr_array_free(msg_info_array, TRUE);
	}
	return info;
}

static void
message_changed(CamelMimeMessage *m, enum _MessageChangeType type, MessageList *message_list)
{
	e_table_model_changed (message_list->table_model);
}

/* select a message and display it */
static void
select_msg (MessageList *message_list, gint row)
{
	CamelException ex;
	CamelMimeMessage *message = NULL;

	camel_exception_init (&ex);

	if (camel_folder_has_uid_capability  (message_list->folder)) {
		CamelMessageInfo *msg_info;

		msg_info = get_message_info(message_list, row);
		if (msg_info) {
			message = camel_folder_get_message_by_uid (message_list->folder, 
								   msg_info->uid,
								   &ex);
			if (camel_exception_get_id (&ex)) {
				printf ("Unable to get message: %s\n",
					ex.desc?ex.desc:"unknown_reason");
				return;
			}
		}
		
		if (message) {
			gtk_signal_connect((GtkObject *)message, "message_changed", message_changed, message_list);
			mail_display_set_message (message_list->parent_folder_browser->mail_display,
						  CAMEL_MEDIUM (message));
		}
	}
	
}

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
	CamelException ex;
	int v;
	
	if (!message_list->folder) {
		return 0;
	}

	if (message_list->matches) {
		v = g_list_length(message_list->matches);
	} else {
		camel_exception_init (&ex);
	
		v = camel_folder_get_message_count (message_list->folder, &ex);
		if (camel_exception_get_id (&ex))
			v = 0;
	}
	
	/* in the case where no message is available, return 1
	 * however, cause we want to be able to show a text */
	return (v ? v:1);
	
}

static void *
ml_value_at (ETableModel *etm, int col, int row, void *data)
{
	static char buffer [10];
	MessageList *message_list = data;
	CamelFolder *folder;
	CamelMessageInfo *msg_info;
	CamelException ex;
	void *retval = NULL;

	camel_exception_init (&ex);
	
	folder = message_list->folder;
	if (!folder)
		goto nothing_to_see;
	
	
	/* retrieve the message information array */
	msg_info = get_message_info(message_list, row);

	/* 
	 * in the case where it is zero message long 
	 * display nothing 
	 */
	if (msg_info == NULL)
		goto nothing_to_see;
	
	switch (col){
	case COL_ONLINE_STATUS:
		retval = GINT_TO_POINTER (0);
		break;
		
	case COL_MESSAGE_STATUS:
		if (msg_info->flags & CAMEL_MESSAGE_DELETED)
			retval = GINT_TO_POINTER (2);
		else
			retval = GINT_TO_POINTER (1);
		break;
		
	case COL_PRIORITY:
		retval = GINT_TO_POINTER (1);
		break;
		
	case COL_ATTACHMENT:
		retval = GINT_TO_POINTER (0);
		break;
		
	case COL_FROM:
		if (msg_info->from)
			retval = msg_info->from;
		else
			retval = "";
		break;
		
	case COL_SUBJECT:
		if (msg_info->subject)
			retval = msg_info->subject;
		else
			retval = "";
		break;
		
	case COL_SENT:
		retval = "sent";
		break;
		
	case COL_RECEIVE:
		retval = "receive";
		break;
		
	case COL_TO:
		retval = "dudes@server";
		break;
		
	case COL_SIZE:
		if (msg_info->content) {
			sprintf (buffer, "%d", msg_info->content->size);
		} else {
			sprintf (buffer, "0");
		}
		retval = buffer;
		break;
			
	default:
		g_assert_not_reached ();
	}

	return retval;
	
	
 nothing_to_see:
	/* 
	 * in the case there is nothing to look at, 
	 * notify the user.
	 */
	if (col == COL_SUBJECT)
		return "No item in this view";
	else 
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
	switch (col){
	case COL_ONLINE_STATUS:
	case COL_MESSAGE_STATUS:
	case COL_PRIORITY:
	case COL_ATTACHMENT:
		return (void *) value;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_SENT:
	case COL_RECEIVE:
	case COL_TO:
	case COL_SIZE:
		return g_strdup (value);
	default:
		g_assert_not_reached ();
	}
	return NULL;
}

static void
ml_free_value (ETableModel *etm, int col, void *value, void *data)
{
	switch (col){
	case COL_ONLINE_STATUS:
	case COL_MESSAGE_STATUS:
	case COL_PRIORITY:
	case COL_ATTACHMENT:
		break;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_SENT:
	case COL_RECEIVE:
	case COL_TO:
	case COL_SIZE:
		g_free (value);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
ml_thaw (ETableModel *etm, void *data)
{
	e_table_model_changed (etm);
}

static struct {
	char **image_base;
	GdkPixbuf  *pixbuf;
} states_pixmaps [] = {
	{ envelope_opened_xpm,   NULL },
	{ envelope_closed_xpm,   NULL },
	{ empty_xpm,             NULL },
	{ attachment_xpm,        NULL },
	{ attachment_header_xpm, NULL },
	{ online_status_xpm,     NULL },
	{ message_status_xpm,    NULL },
	{ envelope_deleted_xpm,  NULL },
	{ NULL,                  NULL },
};

static void
message_list_init_images (void)
{
	int i;

	/*
	 * Only load once, and share
	 */
	if (states_pixmaps [0].pixbuf)
		return;
	
	for (i = 0; states_pixmaps [i].image_base; i++){
		states_pixmaps [i].pixbuf = gdk_pixbuf_new_from_xpm_data (
			(const char **) states_pixmaps [i].image_base);
	}
}

static void
message_list_init_renderers (MessageList *message_list)
{
	g_assert (message_list);
	g_assert (message_list->table_model);

	message_list->render_text = e_cell_text_new (
		message_list->table_model,
		NULL, GTK_JUSTIFY_LEFT);

	message_list->render_online_status = e_cell_checkbox_new ();

	/*
	 * Message status
	 */
	{
		GdkPixbuf *images [3];

		images [0] = states_pixmaps [0].pixbuf;
		images [1] = states_pixmaps [1].pixbuf;
		images [2] = states_pixmaps [7].pixbuf;
		
		message_list->render_message_status = e_cell_toggle_new (0, 3, images);
	}

	/*
	 * Attachment
	 */
	{
		GdkPixbuf *images [2];

		images [0] = states_pixmaps [2].pixbuf;
		images [1] = states_pixmaps [3].pixbuf;
		
		message_list->render_attachment = e_cell_toggle_new (0, 2, images);
	}
	
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
	gtk_object_ref (GTK_OBJECT (message_list->header_model));
	gtk_object_sink (GTK_OBJECT (message_list->header_model));

	message_list->table_cols [COL_ONLINE_STATUS] =
		e_table_col_new_with_pixbuf (
			COL_ONLINE_STATUS, states_pixmaps [5].pixbuf,
			COL_CHECK_BOX_WIDTH, COL_CHECK_BOX_WIDTH,
			message_list->render_online_status,
			g_int_compare, FALSE);
	
	message_list->table_cols [COL_MESSAGE_STATUS] =
		e_table_col_new_with_pixbuf (
			COL_MESSAGE_STATUS, states_pixmaps [0].pixbuf,
			COL_CHECK_BOX_WIDTH, COL_CHECK_BOX_WIDTH,
			message_list->render_message_status, 
			g_int_compare, FALSE);

	message_list->table_cols [COL_PRIORITY] =
		e_table_col_new (
			COL_PRIORITY, _("Priority"),
			COL_CHECK_BOX_WIDTH, COL_CHECK_BOX_WIDTH,
			message_list->render_priority,
			g_int_compare, FALSE);
	
	message_list->table_cols [COL_ATTACHMENT] =
		e_table_col_new_with_pixbuf (
			COL_ATTACHMENT, states_pixmaps [4].pixbuf,
			COL_ICON_WIDTH, COL_ICON_WIDTH,
			message_list->render_attachment,
			g_int_compare, FALSE);

	message_list->table_cols [COL_FROM] =
		e_table_col_new (
			COL_FROM, _("From"),
			COL_FROM_WIDTH, COL_FROM_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);

	message_list->table_cols [COL_SUBJECT] =
		e_table_col_new (
			COL_SUBJECT, _("Subject"),
			COL_SUBJECT_WIDTH, COL_SUBJECT_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);

	message_list->table_cols [COL_SENT] =
		e_table_col_new (
			COL_SENT, _("Sent"),
			COL_SENT_WIDTH, COL_SENT_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);
	
	message_list->table_cols [COL_RECEIVE] =
		e_table_col_new (
			COL_RECEIVE, _("Receive"),
			COL_RECEIVE_WIDTH, COL_RECEIVE_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);

	message_list->table_cols [COL_TO] =
		e_table_col_new (
			COL_TO, _("To"),
			COL_TO_WIDTH, COL_TO_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);

	message_list->table_cols [COL_SIZE] =
		e_table_col_new (
			COL_SIZE, _("Size"),
			COL_SIZE_WIDTH, COL_SIZE_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);
	
	/*
	 * Dummy init: It setups the headers to match the order in which
	 * they are defined.  In the future e-table widget will take care
	 * of this.
	 */
	for (i = 0; i < COL_LAST; i++) {
		gtk_object_ref (GTK_OBJECT (message_list->table_cols [i]));
		e_table_header_add_column (message_list->header_model,
					   message_list->table_cols [i], i);
	}
}

static char *
message_list_get_layout (MessageList *message_list)
{
	if (0)
		return g_strdup ("<ETableSpecification> <columns-shown> <column> 0 </column> <column> 1 </column> <column> 2 </column> <column> 3 </column> <column> 4 </column> <column> 5 </column> <column> 6 </column> <column> 7 </column> <column> 8 </column> <column> 9 </column> </columns-shown> <grouping> <group column=\"4\" ascending=\"1\"> <leaf column=\"5\" ascending=\"1\"/> </group> </grouping> </ETableSpecification>");
	else {
		/* Message status, From, Sent, Subject */
		return g_strdup ("<ETableSpecification> <columns-shown> <column> 1 </column> <column> 4 </column> <column> 5 </column> </columns-shown> <grouping> </grouping> </ETableSpecification>");
	}
}

/*
 * GtkObject::init
 */
static void
message_list_init (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	char *spec;
		
	message_list->table_model = e_table_simple_new (
		ml_col_count, ml_row_count, ml_value_at,
		ml_set_value_at, ml_is_cell_editable, ml_duplicate_value, ml_free_value,
		ml_thaw, message_list);

	message_list_init_renderers (message_list);
	message_list_init_header (message_list);

	/*
	 * The etable
	 */

	spec = message_list_get_layout (message_list);
	message_list->etable = e_table_new (
		message_list->header_model, message_list->table_model, spec);
	g_free (spec);
	
	gtk_signal_connect (GTK_OBJECT (message_list->etable), "row_selection",
			   GTK_SIGNAL_FUNC (on_row_selection_cmd), message_list);

	gtk_widget_show (message_list->etable);
	
	gtk_object_ref (GTK_OBJECT (message_list->table_model));
	gtk_object_sink (GTK_OBJECT (message_list->table_model));
	
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

	message_list_init_images ();
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
message_list_new (FolderBrowser *parent_folder_browser)
{
	Evolution_MessageList corba_object;
	MessageList *message_list;

	g_assert (parent_folder_browser);

	message_list = gtk_type_new (message_list_get_type ());

	corba_object = create_corba_message_list (BONOBO_OBJECT (message_list));
	if (corba_object == CORBA_OBJECT_NIL){
		gtk_object_destroy (GTK_OBJECT (message_list));
		return NULL;
	}

	message_list->parent_folder_browser = parent_folder_browser;

	message_list->idle_id = 0;

	message_list_construct (message_list, corba_object);

	return BONOBO_OBJECT (message_list);
}

void
message_list_set_search (MessageList *message_list, const char *search)
{
	if (message_list->matches) {
		/* FIXME: free contents too ... */
		g_list_free(message_list->matches);
		message_list->matches = NULL;
	}
	if (search) {
		CamelException ex;

		camel_exception_init (&ex);
		message_list->matches = camel_folder_search_by_expression(message_list->folder, search, &ex);
	}

	e_table_model_changed (message_list->table_model);
	select_msg (message_list, 0);
}

static void
folder_changed(CamelFolder *f, int type, MessageList *message_list)
{
	e_table_model_changed (message_list->table_model);
	select_msg (message_list, 0);
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

	if (message_list->matches) {
		/* FIXME: free contents too ... */
		g_list_free(message_list->matches);
		message_list->matches = NULL;
	}
	
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

	gtk_signal_connect((GtkObject *)camel_folder, "folder_changed", folder_changed, message_list);

	gtk_object_ref (GTK_OBJECT (camel_folder));
	
	e_table_model_changed (message_list->table_model);

	select_msg (message_list, 0);
}

GtkWidget *
message_list_get_widget (MessageList *message_list)
{
	return message_list->etable;
}

E_MAKE_TYPE (message_list, "MessageList", MessageList, message_list_class_init, message_list_init, PARENT_TYPE);

static gboolean
on_row_selection_idle (gpointer data)
{
	MessageList *message_list = data;

	select_msg (message_list, message_list->row_to_select);

	message_list->idle_id = 0;
	return FALSE;
}

static void
on_row_selection_cmd (ETable *table, 
		      int row, 
		      gboolean selected,
		      gpointer user_data)
{
	if (selected) {
		MessageList *message_list;

		message_list = MESSAGE_LIST (user_data);

		message_list->row_to_select = row;

		if (!message_list->idle_id)
			g_idle_add_full (G_PRIORITY_LOW, on_row_selection_idle, message_list, NULL);
	}
}

