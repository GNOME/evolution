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
#include <e-util/e-util.h>
#include <e-util/e-gui-utils.h>
#include <e-util/e-popup-menu.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include "message-list.h"
#include "message-thread.h"
#include "mail-threads.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-config.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail.h"
#include "Mail.h"
#include "widgets/e-table/e-table-header-item.h"
#include "widgets/e-table/e-table-item.h"

#include "art/mail-new.xpm"
#include "art/mail-read.xpm"
#include "art/mail-replied.xpm"
#include "art/attachment.xpm"
#include "art/empty.xpm"
#include "art/tree-expanded.xpm"
#include "art/tree-unexpanded.xpm"

/*
 * Default sizes for the ETable display
 *
 */
#define N_CHARS(x) (CHAR_WIDTH * (x))

#define COL_ICON_WIDTH         (16)
#define COL_CHECK_BOX_WIDTH    (16)
#define COL_FROM_EXPANSION     (24.0)
#define COL_FROM_WIDTH_MIN     (32)
#define COL_SUBJECT_EXPANSION  (30.0)
#define COL_SUBJECT_WIDTH_MIN  (32)
#define COL_SENT_EXPANSION     (24.0)
#define COL_SENT_WIDTH_MIN     (32)
#define COL_RECEIVED_EXPANSION (20.0)
#define COL_RECEIVED_WIDTH_MIN (32)
#define COL_TO_EXPANSION       (24.0)
#define COL_TO_WIDTH_MIN       (32)
#define COL_SIZE_EXPANSION     (6.0)
#define COL_SIZE_WIDTH_MIN     (32)

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *message_list_parent_class;
static POA_Evolution_MessageList__vepv evolution_message_list_vepv;

static void on_cursor_change_cmd (ETableScrolled *table, int row, gpointer user_data);
static void select_row (ETableScrolled *table, gpointer user_data);
static gint on_right_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, MessageList *list);
static void on_double_click (ETableScrolled *table, gint row, MessageList *list);
static void select_msg (MessageList *message_list, gint row);
static char *filter_date (const void *data);

static struct {
	char **image_base;
	GdkPixbuf  *pixbuf;
} states_pixmaps [] = {
	{ mail_new_xpm,		NULL },
	{ mail_read_xpm,	NULL },
	{ mail_replied_xpm,	NULL },
	{ empty_xpm,		NULL },
	{ attachment_xpm,	NULL },
	{ tree_expanded_xpm,	NULL },
	{ tree_unexpanded_xpm,	NULL },
	{ NULL,			NULL }
};

/* Gets the CamelMessageInfo for the message displayed at the given
 * view row.
 */
static const CamelMessageInfo *
get_message_info (MessageList *message_list, int row)
{
	ETreeModel *model = (ETreeModel *)message_list->table_model;
	ETreePath *node;
	char *uid;

	if (row >= e_table_model_row_count (message_list->table_model))
		return NULL;

	node = e_tree_model_node_at_row (model, row);
	g_return_val_if_fail (node != NULL, NULL);
	uid = e_tree_model_node_get_data (model, node);

	if (strncmp (uid, "uid:", 4) != 0)
		return NULL;
	uid += 4;

	return camel_folder_get_message_info (message_list->folder, uid);
}

/* Gets the uid of the message displayed at a given view row */
static const char *
get_message_uid (MessageList *message_list, int row)
{
	ETreeModel *model = (ETreeModel *)message_list->table_model;
	ETreePath *node;
	const char *uid;

	if (row >= e_table_model_row_count (message_list->table_model))
		return NULL;

	node = e_tree_model_node_at_row (model, row);
	g_return_val_if_fail (node != NULL, NULL);
	uid = e_tree_model_node_get_data (model, node);

	if (strncmp (uid, "uid:", 4) != 0)
		return NULL;
	uid += 4;

	return uid;
}

static gint 
mark_msg_seen (gpointer data)
{
	MessageList *ml = data;
	GPtrArray *uids;

	if (!ml->cursor_uid) 
		return FALSE;

	uids = g_ptr_array_new ();
	g_ptr_array_add (uids, g_strdup (ml->cursor_uid));
	mail_do_flag_messages (ml->folder, uids, FALSE,
			       CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	return FALSE;
}

/**
 * message_list_select:
 * @message_list: a MessageList
 * @base_row: the (model) row to start from
 * @direction: the direction to search in
 * @flags: a set of flag values
 * @mask: a mask for comparing against @flags
 *
 * This moves the message list selection to a suitable row. @base_row
 * lists the first (model) row to try, but as a special case, model
 * row -1 is mapped to view row 0. @flags and @mask combine to specify
 * what constitutes a suitable row. @direction is
 * %MESSAGE_LIST_SELECT_NEXT if it should find the next matching
 * message, or %MESSAGE_LIST_SELECT_PREVIOUS if it should find the
 * previous. If no suitable row is found, the selection will be
 * unchanged but the message display will be cleared.
 **/
void
message_list_select (MessageList *message_list, int base_row,
		     MessageListSelectDirection direction,
		     guint32 flags, guint32 mask)
{
	const CamelMessageInfo *info;
	int vrow, mrow, last;
	ETableScrolled *ets = E_TABLE_SCROLLED (message_list->etable);

	if (direction == MESSAGE_LIST_SELECT_PREVIOUS)
		last = 0;
	else
		last = e_table_model_row_count (message_list->table_model);

	if (base_row == -1)
		vrow = 0;
	else
		vrow = e_table_model_to_view_row (ets->table, base_row);

	/* We don't know whether to use < or > due to "direction" */
	while (vrow != last) {
		mrow = e_table_view_to_model_row (ets->table, vrow);
		info = get_message_info (message_list, mrow);
		if (info && (info->flags & mask) == flags) {
			e_table_scrolled_set_cursor_row (ets, mrow);
			mail_do_display_message (message_list, info->uid, mark_msg_seen);
			return;
		}
		vrow += direction;
	}

	mail_display_set_message (message_list->parent_folder_browser->mail_display, NULL);
}

/* select a message and display it */
static void
select_msg (MessageList *message_list, gint row)
{
	const char *uid;

	uid = get_message_uid (message_list, row);
	mail_do_display_message (message_list, uid, mark_msg_seen);
}


static GdkPixbuf *
ml_tree_icon_at (ETreeModel *etm, ETreePath *path, void *model_data)
{
	/* we dont really need an icon ... */
	return NULL;
}

static void *
ml_tree_value_at (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	MessageList *message_list = model_data;
	const CamelMessageInfo *msg_info;
	static char buffer [10];
	char *uid;

	/* retrieve the message information array */
	uid = e_tree_model_node_get_data (etm, path);
	if (strncmp (uid, "uid:", 4) != 0)
		goto fake;
	uid += 4;

	msg_info = camel_folder_get_message_info (message_list->folder, uid);
	g_return_val_if_fail (msg_info != NULL, NULL);

	switch (col){
	case COL_ONLINE_STATUS:
		return GINT_TO_POINTER (0);
		
	case COL_MESSAGE_STATUS:
		if (msg_info->flags & CAMEL_MESSAGE_ANSWERED)
			return GINT_TO_POINTER (2);
		else if (msg_info->flags & CAMEL_MESSAGE_SEEN)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
		
	case COL_PRIORITY:
		return GINT_TO_POINTER (1);
		
	case COL_ATTACHMENT:
		return GINT_TO_POINTER (0);
		
	case COL_FROM:
		if (msg_info->from)
			return msg_info->from;
		else
			return "";
		
	case COL_SUBJECT:
		if (msg_info->subject)
			return msg_info->subject;
		else
			return "";
		
	case COL_SENT:
		return GINT_TO_POINTER (msg_info->date_sent);
		
	case COL_RECEIVED:
		return GINT_TO_POINTER (msg_info->date_received);
		
	case COL_TO:
		if (msg_info->to)
			return msg_info->to;
		else
			return "";
		
	case COL_SIZE:
		sprintf (buffer, "%d", msg_info->size);
		return buffer;
			
	case COL_DELETED:
		return GINT_TO_POINTER(!!(msg_info->flags & CAMEL_MESSAGE_DELETED));

	case COL_UNREAD:
		return GINT_TO_POINTER(!(msg_info->flags & CAMEL_MESSAGE_SEEN));

	case COL_COLOUR:
		return (void *) camel_tag_get((CamelTag **) &msg_info->user_tags, "colour");
	}

	g_assert_not_reached ();

 fake:
	/* This is a fake tree parent */
	switch (col){
	case COL_ONLINE_STATUS:
	case COL_MESSAGE_STATUS:
	case COL_PRIORITY:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_COLOUR:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
		return (void *) 0;

	case COL_SUBJECT:
		return strchr (uid, ':') + 1;

	case COL_FROM:
	case COL_TO:
	case COL_SIZE:
		return "?";
	}
	g_assert_not_reached ();

	return NULL;
}

static void
ml_tree_set_value_at (ETreeModel *etm, ETreePath *path, int col,
		      const void *val, void *model_data)
{
	MessageList *message_list = model_data;
	const CamelMessageInfo *msg_info;
	char *uid;
	GPtrArray *uids;

	if (col != COL_MESSAGE_STATUS)
		return;

	uid = e_tree_model_node_get_data (etm, path);
	if (strncmp (uid, "uid:", 4) != 0)
		return;
	uid += 4;

	msg_info = camel_folder_get_message_info (message_list->folder, uid);
	if (!msg_info)
		return;

	uids = g_ptr_array_new ();
	g_ptr_array_add (uids, g_strdup (uid));
	mail_do_flag_messages (message_list->folder, uids, TRUE,
			       CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

	if (message_list->seen_id) {
		gtk_timeout_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}
}

static gboolean
ml_tree_is_cell_editable (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	return col == COL_MESSAGE_STATUS;
}

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

static char *
filter_date (const void *data)
{
	time_t date = GPOINTER_TO_INT (data);
	char buf[26], *p;

	if (date == 0)
		return g_strdup ("?");

#ifdef CTIME_R_THREE_ARGS
	ctime_r (&date, buf, 26);
#else
	ctime_r (&date, buf);
#endif

	p = strchr (buf, '\n');
	if (p)
		*p = '\0';

	return g_strdup (buf);
}

static void
message_list_init_renderers (MessageList *message_list)
{
	GdkPixbuf *images [3];

	g_assert (message_list);
	g_assert (message_list->table_model);

	message_list->render_text = e_cell_text_new (
		message_list->table_model,
		NULL, GTK_JUSTIFY_LEFT);

	gtk_object_set(GTK_OBJECT(message_list->render_text),
		       "strikeout_column", COL_DELETED,
		       NULL);
	gtk_object_set(GTK_OBJECT(message_list->render_text),
		       "bold_column", COL_UNREAD,
		       NULL);
	gtk_object_set(GTK_OBJECT(message_list->render_text),
		       "color_column", COL_COLOUR,
		       NULL);

	message_list->render_date = e_cell_text_new (
		message_list->table_model,
		NULL, GTK_JUSTIFY_LEFT);

	gtk_object_set(GTK_OBJECT(message_list->render_date),
		       "text_filter", filter_date,
		       NULL);
	gtk_object_set(GTK_OBJECT(message_list->render_date),
		       "strikeout_column", COL_DELETED,
		       NULL);
	gtk_object_set(GTK_OBJECT(message_list->render_date),
		       "bold_column", COL_UNREAD,
		       NULL);
	gtk_object_set(GTK_OBJECT(message_list->render_date),
		       "color_column", COL_COLOUR,
		       NULL);

	message_list->render_online_status = e_cell_checkbox_new ();

	/*
	 * Message status
	 */
	images [0] = states_pixmaps [0].pixbuf;
	images [1] = states_pixmaps [1].pixbuf;
	images [2] = states_pixmaps [2].pixbuf;

	message_list->render_message_status = e_cell_toggle_new (0, 3, images);

	/*
	 * Attachment
	 */
	images [0] = states_pixmaps [3].pixbuf;
	images [1] = states_pixmaps [4].pixbuf;

	message_list->render_attachment = e_cell_toggle_new (0, 2, images);
	
	/*
	 * FIXME: We need a real renderer here
	 */
	message_list->render_priority = e_cell_checkbox_new ();

	/*
	 * for tree view
	 */
	message_list->render_tree =
		e_cell_tree_new (message_list->table_model,
				 states_pixmaps[5].pixbuf,
				 states_pixmaps[6].pixbuf,
				 TRUE, message_list->render_text);
}

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
		e_table_col_new (
			COL_ONLINE_STATUS, _("Online Status"),
			0.0, COL_CHECK_BOX_WIDTH,
			message_list->render_online_status,
			g_int_compare, FALSE);
	
	message_list->table_cols [COL_MESSAGE_STATUS] =
		e_table_col_new_with_pixbuf (
			COL_MESSAGE_STATUS, states_pixmaps [0].pixbuf,
			0.0, COL_CHECK_BOX_WIDTH,
			message_list->render_message_status, 
			g_int_compare, FALSE);

	gtk_object_set(GTK_OBJECT(message_list->table_cols[COL_MESSAGE_STATUS]),
		       "sortable", FALSE,
		       NULL);

	message_list->table_cols [COL_PRIORITY] =
		e_table_col_new (
			COL_PRIORITY, _("Priority"),
			0.0, COL_CHECK_BOX_WIDTH,
			message_list->render_priority,
			g_int_compare, FALSE);
	
	message_list->table_cols [COL_ATTACHMENT] =
		e_table_col_new_with_pixbuf (
			COL_ATTACHMENT, states_pixmaps [4].pixbuf,
			0.0, COL_ICON_WIDTH,
			message_list->render_attachment,
			g_int_compare, FALSE);

	gtk_object_set(GTK_OBJECT(message_list->table_cols[COL_ATTACHMENT]),
		       "sortable", FALSE,
		       NULL);

	message_list->table_cols [COL_FROM] =
		e_table_col_new (
			COL_FROM, _("From"),
			COL_FROM_EXPANSION, COL_FROM_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);

	message_list->table_cols [COL_SUBJECT] =
		e_table_col_new (
			COL_SUBJECT, _("Subject"),
			COL_SUBJECT_EXPANSION, COL_SUBJECT_WIDTH_MIN,
			message_list->render_tree,
			g_str_compare, TRUE);

	message_list->table_cols [COL_SENT] =
		e_table_col_new (
			COL_SENT, _("Date"),
			COL_SENT_EXPANSION, COL_SENT_WIDTH_MIN,
			message_list->render_date,
			g_int_compare, TRUE);
	
	message_list->table_cols [COL_RECEIVED] =
		e_table_col_new (
			COL_RECEIVED, _("Received"),
			COL_RECEIVED_EXPANSION, COL_RECEIVED_WIDTH_MIN,
			message_list->render_date,
			g_int_compare, TRUE);

	message_list->table_cols [COL_TO] =
		e_table_col_new (
			COL_TO, _("To"),
			COL_TO_EXPANSION, COL_TO_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);

	message_list->table_cols [COL_SIZE] =
		e_table_col_new (
			COL_SIZE, _("Size"),
			COL_SIZE_EXPANSION, COL_SIZE_WIDTH_MIN,
			message_list->render_text,
			g_str_compare, TRUE);
	
	for (i = 0; i < COL_LAST; i++) {
		gtk_object_ref (GTK_OBJECT (message_list->table_cols [i]));
		e_table_header_add_column (message_list->header_model,
					   message_list->table_cols [i], i);
	}
}

static char *
message_list_get_layout (MessageList *message_list)
{
	/* Message status, From, Subject, Sent Date */
	return g_strdup ("<ETableSpecification> <columns-shown> <column> 1 </column> <column> 4 </column> <column> 5 </column> <column> 6 </column> </columns-shown> <grouping> </grouping> </ETableSpecification>");
}

/*
 * GtkObject::init
 */
static void
message_list_init (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	char *spec;

	message_list->table_model = (ETableModel *)
		e_tree_simple_new (ml_tree_icon_at, ml_tree_value_at,
				   ml_tree_set_value_at,
				   ml_tree_is_cell_editable,
				   message_list);
	e_tree_model_root_node_set_visible ((ETreeModel *)message_list->table_model, FALSE);

	message_list_init_renderers (message_list);
	message_list_init_header (message_list);

	/*
	 * The etable
	 */

	spec = message_list_get_layout (message_list);
	message_list->etable = e_table_scrolled_new (
		message_list->header_model, message_list->table_model, spec);
	g_free (spec);

	gtk_object_set(GTK_OBJECT(message_list->etable),
		       "cursor_mode", E_TABLE_CURSOR_LINE,
		       "drawfocus", FALSE,
		       "drawgrid", FALSE,
		       NULL);

	/*
	 *gtk_signal_connect (GTK_OBJECT (message_list->etable), "realize",
	 *		    GTK_SIGNAL_FUNC (select_row), message_list);
	 */

	gtk_signal_connect (GTK_OBJECT (message_list->etable), "cursor_change",
			   GTK_SIGNAL_FUNC (on_cursor_change_cmd), message_list);

	gtk_signal_connect (GTK_OBJECT (message_list->etable), "right_click",
			    GTK_SIGNAL_FUNC (on_right_click), message_list);
	
	gtk_signal_connect (GTK_OBJECT (message_list->etable), "double_click",
			    GTK_SIGNAL_FUNC (on_double_click), message_list);
	
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
free_key (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
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
	gtk_object_unref (GTK_OBJECT (message_list->render_tree));
	
	gtk_object_unref (GTK_OBJECT (message_list->etable));

	if (message_list->uid_rowmap) {
		g_hash_table_foreach (message_list->uid_rowmap,
				      free_key, NULL);
		g_hash_table_destroy (message_list->uid_rowmap);
	}

	for (i = 0; i < COL_LAST; i++)
		gtk_object_unref (GTK_OBJECT (message_list->table_cols [i]));
	
	if (message_list->idle_id != 0)
		g_source_remove(message_list->idle_id);

	if (message_list->folder)
		camel_object_unref (CAMEL_OBJECT (message_list->folder));

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

static void
clear_tree (MessageList *ml)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);

	if (ml->tree_root)
		e_tree_model_node_remove (etm, ml->tree_root);
	ml->tree_root =
		e_tree_model_node_insert (etm, NULL, 0, ml);
	e_tree_model_node_set_expanded (etm, ml->tree_root, TRUE);
}

/* only call if we have a tree model */
/* builds the tree structure */
static void build_subtree (MessageList *ml, ETreePath *parent,
			   struct _container *c, int *row);

static void
build_tree (MessageList *ml, struct _container *c)
{
	int row = 0;

	clear_tree (ml);
	build_subtree (ml, ml->tree_root, c, &row);
}

static void
build_subtree (MessageList *ml, ETreePath *parent,
	       struct _container *c, int *row)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *id;

	while (c) {
		if (c->message) {
			id = g_strdup_printf ("uid:%s", c->message->uid);
			g_hash_table_insert (ml->uid_rowmap,
					     g_strdup (c->message->uid),
					     GINT_TO_POINTER ((*row)++));
		} else
			id = g_strdup_printf ("subject:%s", c->root_subject);
		node = e_tree_model_node_insert (tree, parent, 0, id);
		if (c->child) {
			/* by default, open all trees */
			e_tree_model_node_set_expanded (tree, node, TRUE);
			build_subtree (ml, node, c->child, row);
		}
		c = c->next;
	}
}

static void
build_flat (MessageList *ml, GPtrArray *uids)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *uid;
	int i;

	clear_tree (ml);
	for (i = 0; i < uids->len; i++) {
		uid = g_strdup_printf ("uid:%s", (char *)uids->pdata[i]);
		node = e_tree_model_node_insert (tree, ml->tree_root, i, uid);
		g_hash_table_insert (ml->uid_rowmap, g_strdup (uids->pdata[i]),
				     GINT_TO_POINTER (i));
	}
}

static void
main_folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (user_data);

	mail_do_regenerate_messagelist (message_list, message_list->search);
}

static void
folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	mail_op_forward_event (main_folder_changed, o, event_data, user_data);
}

static void
main_message_changed (CamelObject *o, gpointer uid, gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (user_data);
	int row;

	row = GPOINTER_TO_INT (g_hash_table_lookup (message_list->uid_rowmap,
						    uid));
	if (row != -1)
		e_table_model_row_changed (message_list->table_model, row);

	g_free (uid);
}

static void
message_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	/* Here we copy the data because our thread may free the copy that we would reference.
	 * The other thread would be passed a uid parameter that pointed to freed data.
	 * We copy it and free it in the handler. 
	 */
	mail_op_forward_event (main_message_changed, o, g_strdup ((gchar *)event_data), user_data);
}

void
message_list_set_folder (MessageList *message_list, CamelFolder *camel_folder)
{
	CamelException ex;

	g_return_if_fail (message_list != NULL);
	g_return_if_fail (camel_folder != NULL);
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	g_return_if_fail (CAMEL_IS_FOLDER (camel_folder));
	g_return_if_fail (camel_folder_has_summary_capability (camel_folder));

	camel_exception_init (&ex);
	
	if (message_list->folder)
		camel_object_unref (CAMEL_OBJECT (message_list->folder));

	message_list->folder = camel_folder;

	camel_object_hook_event(CAMEL_OBJECT (camel_folder), "folder_changed",
			   folder_changed, message_list);
	camel_object_hook_event(CAMEL_OBJECT (camel_folder), "message_changed",
			   message_changed, message_list);

	camel_object_ref (CAMEL_OBJECT (camel_folder));

	/*gtk_idle_add (regen_message_list, message_list);*/
	/*folder_changed (CAMEL_OBJECT (camel_folder), 0, message_list);*/
	mail_do_regenerate_messagelist (message_list, message_list->search);
}

GtkWidget *
message_list_get_widget (MessageList *message_list)
{
	return message_list->etable;
}

E_MAKE_TYPE (message_list, "MessageList", MessageList, message_list_class_init, message_list_init, PARENT_TYPE);

static gboolean
on_cursor_change_idle (gpointer data)
{
	MessageList *message_list = data;

	select_msg (message_list, message_list->cursor_row);

	message_list->idle_id = 0;
	return FALSE;
}

static void
on_cursor_change_cmd (ETableScrolled *table, int row, gpointer user_data)
{
	MessageList *message_list;
	const char *uid;
	
	message_list = MESSAGE_LIST (user_data);
	
	message_list->cursor_row = row;
	uid = get_message_uid (message_list, row);
	message_list->cursor_uid = uid; /*NULL ok*/

	if (!message_list->idle_id) {
		message_list->idle_id =
			g_idle_add_full (G_PRIORITY_LOW, on_cursor_change_idle,
					 message_list, NULL);
	}
}

/* FIXME: this is all a kludge. */
static gint
idle_select_row (gpointer user_data)
{
	MessageList *ml = MESSAGE_LIST (user_data);

	message_list_select (ml, -1, MESSAGE_LIST_SELECT_NEXT,
			     0, CAMEL_MESSAGE_SEEN);
	return FALSE;
}

static void
select_row (ETableScrolled *table, gpointer user_data)
{
	MessageList *message_list = user_data;

	gtk_idle_add (idle_select_row, message_list);
}

static void
vfolder_subject(GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message(fb->mail_display->current_message, AUTO_SUBJECT,
				     fb->uri);
}

static void
vfolder_sender(GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message(fb->mail_display->current_message, AUTO_FROM,
				     fb->uri);
}

static void
vfolder_recipient(GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message(fb->mail_display->current_message, AUTO_TO,
				     fb->uri);
}

static void
filter_subject(GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message(fb->mail_display->current_message, AUTO_SUBJECT);
}

static void
filter_sender(GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message(fb->mail_display->current_message, AUTO_FROM);
}

static void
filter_recipient(GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message(fb->mail_display->current_message, AUTO_TO);
}

static gint
on_right_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, MessageList *list)
{
	FolderBrowser *fb = list->parent_folder_browser;
	extern CamelFolder *drafts_folder;
	int enable_mask = 0;
	EPopupMenu menu[] = {
		{ "Open in New Window",      NULL, GTK_SIGNAL_FUNC (view_msg),          0 },
		{ "Edit Message",            NULL, GTK_SIGNAL_FUNC (edit_msg),          1 },
		{ "Print Message",           NULL, GTK_SIGNAL_FUNC (print_msg),         0 },
		{ "",                        NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ "Reply to Sender",         NULL, GTK_SIGNAL_FUNC (reply_to_sender),   0 },
		{ "Reply to All",            NULL, GTK_SIGNAL_FUNC (reply_to_all),      0 },
		{ "Forward Message",         NULL, GTK_SIGNAL_FUNC (forward_msg),       0 },
		{ "",                        NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ "Delete Message",          NULL, GTK_SIGNAL_FUNC (delete_msg),        0 },
		{ "Move Message",            NULL, GTK_SIGNAL_FUNC (move_msg),          0 },
		{ "Copy Message",            NULL, GTK_SIGNAL_FUNC (copy_msg),          0 },
		{ "",                        NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ "VFolder on Subject",      NULL, GTK_SIGNAL_FUNC (vfolder_subject),   2 },
		{ "VFolder on Sender",       NULL, GTK_SIGNAL_FUNC (vfolder_sender),    2 },
		{ "VFolder on Recipients",   NULL, GTK_SIGNAL_FUNC (vfolder_recipient), 2 },
		{ "",                        NULL, GTK_SIGNAL_FUNC (NULL),              0 },
		{ "Filter on Subject",       NULL, GTK_SIGNAL_FUNC (filter_subject),    2 },
		{ "Filter on Sender",        NULL, GTK_SIGNAL_FUNC (filter_sender),     2 },
		{ "Filter on Recipients",    NULL, GTK_SIGNAL_FUNC (filter_recipient),  2 },
		{ NULL,                      NULL, NULL,                                0 }
	};

	if (fb->folder != drafts_folder)
		enable_mask |= 1;
	if (fb->mail_display->current_message == NULL)
		enable_mask |= 2;

	e_popup_menu_run (menu, (GdkEventButton *)event, enable_mask, 0, fb);
	
	return TRUE;
}

static void
on_double_click (ETableScrolled *table, gint row, MessageList *list)
{
	FolderBrowser *fb = list->parent_folder_browser;
	
	view_msg (NULL, fb);
}

struct message_list_foreach_data {
	MessageList *message_list;
	MessageListForeachFunc callback;
	gpointer user_data;
};

static void
mlfe_callback (int row, gpointer user_data)
{
	struct message_list_foreach_data *mlfe_data = user_data;
	const char *uid;

	uid = get_message_uid (mlfe_data->message_list, row);
	if (uid) {
		mlfe_data->callback (mlfe_data->message_list,
				     uid,
				     mlfe_data->user_data);
	}
}

void
message_list_foreach (MessageList *message_list,
		      MessageListForeachFunc callback,
		      gpointer user_data)
{
	struct message_list_foreach_data mlfe_data;

	mlfe_data.message_list = message_list;
	mlfe_data.callback = callback;
	mlfe_data.user_data = user_data;
	e_table_scrolled_selected_row_foreach (E_TABLE_SCROLLED (message_list->etable),
					       mlfe_callback, &mlfe_data);
}

void
message_list_toggle_threads (BonoboUIHandler *uih, void *user_data,
			     const char *path)
{
	MessageList *ml = user_data;

	mail_config_set_thread_list (bonobo_ui_handler_menu_get_toggle_state (uih, path));
	mail_do_regenerate_messagelist (ml, ml->search);
}

/* ** REGENERATE MESSAGELIST ********************************************** */

typedef struct regenerate_messagelist_input_s {
	MessageList *ml;
	char *search;
} regenerate_messagelist_input_t;

typedef struct regenerate_messagelist_data_s {
	GPtrArray *uids;
} regenerate_messagelist_data_t;

static gchar *describe_regenerate_messagelist (gpointer in_data, gboolean gerund);
static void setup_regenerate_messagelist   (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_regenerate_messagelist      (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex);

static gchar *describe_regenerate_messagelist (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Rebuilding message view");
	else
		return g_strdup ("Rebuild message view");
}

static void setup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;

	if (!IS_MESSAGE_LIST (input->ml)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No messagelist specified to regenerate");
		return;
	}
	
	gtk_object_ref (GTK_OBJECT (input->ml));
	e_table_model_pre_change (input->ml->table_model);
}

static void do_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;
	regenerate_messagelist_data_t *data = (regenerate_messagelist_data_t *) op_data;

        if (input->ml->search) {
                g_free (input->ml->search);
                input->ml->search = NULL;
        }

        if (input->ml->uid_rowmap) {
                g_hash_table_foreach (input->ml->uid_rowmap,
                                      free_key, NULL);
                g_hash_table_destroy (input->ml->uid_rowmap);
        }
        input->ml->uid_rowmap = g_hash_table_new (g_str_hash, g_str_equal);

	mail_tool_camel_lock_up();

	if (input->search) {
		data->uids = camel_folder_search_by_expression (input->ml->folder,
								input->search, ex);
		if (camel_exception_is_set (ex)) {
			mail_tool_camel_lock_down();
			return;
		}

		input->ml->search = g_strdup (input->search);
	} else
		data->uids = camel_folder_get_uids (input->ml->folder);

	mail_tool_camel_lock_down();
}

static void cleanup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;
	regenerate_messagelist_data_t *data = (regenerate_messagelist_data_t *) op_data;

	ETreeModel *etm;

	etm = E_TREE_MODEL (input->ml->table_model);

	/* FIXME: free the old tree data */

	if (data->uids == NULL) { /*exception*/
		gtk_object_unref (GTK_OBJECT (input->ml));
		return;
	}

	if (mail_config_thread_list()) {
		mail_do_thread_messages (input->ml, data->uids, 
					 (gboolean) !(input->search),
					 build_tree);
	} else {
		build_flat (input->ml, data->uids);

		if (input->search) {
			camel_folder_search_free (input->ml->folder, data->uids);
		} else {
			camel_folder_free_uids (input->ml->folder, data->uids);
		}
	}

	e_table_model_changed (input->ml->table_model);
	select_row (NULL, input->ml);
	g_free (input->search);
	gtk_object_unref (GTK_OBJECT (input->ml));
}

static const mail_operation_spec op_regenerate_messagelist =
{
	describe_regenerate_messagelist,
	sizeof (regenerate_messagelist_data_t),
	setup_regenerate_messagelist,
	do_regenerate_messagelist,
	cleanup_regenerate_messagelist
};

void mail_do_regenerate_messagelist (MessageList *list, const gchar *search)
{
	regenerate_messagelist_input_t *input;

	input = g_new (regenerate_messagelist_input_t, 1);
	input->ml = list;
	input->search = g_strdup (search);

	mail_operation_queue (&op_regenerate_messagelist, input, TRUE);
}
