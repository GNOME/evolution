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
#include "e-util/e-gui-utils.h"
#include "camel/camel-exception.h"
#include <camel/camel-folder.h>
#include "message-list.h"
#include "message-thread.h"
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

static void on_cursor_change_cmd (ETable *table, int row, gpointer user_data);
static void on_row_selection (ETable *table, int row, gboolean selected,
			      gpointer user_data);
static void select_row (ETable *table, gpointer user_data);
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
	g_return_val_if_fail (uid != NULL, NULL);

	return camel_folder_get_message_info (message_list->folder, uid);
}

/**
 * message_list_select_next:
 * @message_list: a MessageList
 * @row: the row to start from
 * @flags: a set of flag values
 * @mask: a mask for comparing against @flags
 *
 * This moves the message list selection to the first row on or after
 * @row whose flags match @flags when masked with @mask.
 **/
void
message_list_select_next (MessageList *message_list, int row,
			  guint32 flags, guint32 mask)
{
	const CamelMessageInfo *info;

	while ((info = get_message_info (message_list, row))) {
		if ((info->flags & mask) == flags) {
			e_table_set_cursor_row (E_TABLE (message_list->etable),
						row);
			return;
		}
		row++;
	}

	/* We know "row" is out of bounds now, so this will cause the
	 * MailDisplay to be cleared.
	 */
	select_msg (message_list, row);
}

static gint
mark_msg_seen (gpointer data)
{
	MessageList *ml = data;
	guint32 flags;

	if (!ml->cursor_uid)
		return FALSE;

	flags = camel_folder_get_message_flags (ml->folder, ml->cursor_uid,
						NULL);
	camel_folder_set_message_flags (ml->folder, ml->cursor_uid,
					CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN,
					NULL);
	return FALSE;
}

/* select a message and display it */
static void
select_msg (MessageList *message_list, gint row)
{
	CamelException ex;
	CamelMimeMessage *message;
	const CamelMessageInfo *msg_info;
	MailDisplay *md = message_list->parent_folder_browser->mail_display;

	camel_exception_init (&ex);

	msg_info = get_message_info (message_list, row);
	if (msg_info) {
		message = camel_folder_get_message (message_list->folder, 
						    msg_info->uid, &ex);
		if (camel_exception_get_id (&ex)) {
			printf ("Unable to get message: %s\n",
				ex.desc?ex.desc:"unknown_reason");
			return;
		}

		if (message_list->seen_id)
			gtk_timeout_remove (message_list->seen_id);

		mail_display_set_message (md, CAMEL_MEDIUM (message));
		gtk_object_unref (GTK_OBJECT (message));

		message_list->seen_id =
			gtk_timeout_add (1500, mark_msg_seen, message_list);
	} else
		mail_display_set_message (md, NULL);
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
	void *retval;

	if (!message_list->folder)
		goto nothing_to_see;
	
	/* retrieve the message information array */
	uid = e_tree_model_node_get_data (etm, path);
	if (!uid)
		goto nothing_to_see;

	msg_info = camel_folder_get_message_info (message_list->folder, uid);
	if (!msg_info)
		goto nothing_to_see;

	switch (col){
	case COL_ONLINE_STATUS:
		retval = GINT_TO_POINTER (0);
		break;
		
	case COL_MESSAGE_STATUS:
		if (msg_info->flags & CAMEL_MESSAGE_ANSWERED)
			retval = GINT_TO_POINTER (2);
		else if (msg_info->flags & CAMEL_MESSAGE_SEEN)
			retval = GINT_TO_POINTER (1);
		else
			retval = GINT_TO_POINTER (0);
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
		retval = GINT_TO_POINTER (msg_info->date_sent);
		break;
		
	case COL_RECEIVED:
		retval = GINT_TO_POINTER (msg_info->date_received);
		break;
		
	case COL_TO:
		if (msg_info->to)
			retval = msg_info->to;
		else
			retval = "";
		break;
		
	case COL_SIZE:
		sprintf (buffer, "%d", msg_info->size);
		retval = buffer;
		break;
			
	case COL_DELETED:
		retval = GINT_TO_POINTER(!!(msg_info->flags & CAMEL_MESSAGE_DELETED));
		break;

	case COL_UNREAD:
		retval = GINT_TO_POINTER(!(msg_info->flags & CAMEL_MESSAGE_SEEN));
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
	switch (col){
	case COL_ONLINE_STATUS:
	case COL_MESSAGE_STATUS:
	case COL_PRIORITY:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
		return (void *) 0;

	case COL_SUBJECT:
		return "No item in this view";
	case COL_FROM:
	case COL_TO:
	case COL_SIZE:
		return "";
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static void
ml_tree_set_value_at (ETreeModel *etm, ETreePath *path, int col,
		      const void *val, void *model_data)
{
	MessageList *message_list = model_data;
	const CamelMessageInfo *msg_info;
	char *uid;

	if (col != COL_MESSAGE_STATUS)
		return;

	uid = e_tree_model_node_get_data (etm, path);
	if (!uid)
		return;

	msg_info = camel_folder_get_message_info (message_list->folder, uid);
	if (!msg_info)
		return;

	camel_folder_set_message_flags (message_list->folder, msg_info->uid,
					CAMEL_MESSAGE_SEEN,
					~(msg_info->flags), NULL);
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
	message_list->etable = e_table_new (
		message_list->header_model, message_list->table_model, spec);
	g_free (spec);

	gtk_object_set(GTK_OBJECT(message_list->etable),
		       "cursor_mode", E_TABLE_CURSOR_LINE,
		       "drawfocus", FALSE,
		       "drawgrid", FALSE,
		       NULL);
	gtk_object_set(GTK_OBJECT(E_TABLE(message_list->etable)->header_item),
		       "dnd_code", "(unset)",
		       NULL);

	gtk_signal_connect (GTK_OBJECT (message_list->etable), "realize",
			    GTK_SIGNAL_FUNC (select_row), message_list);
	
	gtk_signal_connect (GTK_OBJECT (message_list->etable), "cursor_change",
			   GTK_SIGNAL_FUNC (on_cursor_change_cmd), message_list);

	gtk_signal_connect (GTK_OBJECT (message_list->etable), "row_selection",
			   GTK_SIGNAL_FUNC (on_row_selection), message_list);

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
	gtk_object_unref (GTK_OBJECT (message_list->render_tree));
	
	gtk_object_unref (GTK_OBJECT (message_list->etable));

	if (message_list->uid_rowmap)
		g_hash_table_destroy (message_list->uid_rowmap);

	for (i = 0; i < COL_LAST; i++)
		gtk_object_unref (GTK_OBJECT (message_list->table_cols [i]));
	
	if (message_list->idle_id != 0)
		g_source_remove(message_list->idle_id);

	gtk_object_unref (GTK_OBJECT (message_list->folder));

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

/* only call if we have a tree model */
/* builds the tree structure */
static void
build_tree (MessageList *ml, ETreePath *parent, struct _container *c,
	    int *row)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;

	while (c) {
		node = e_tree_model_node_insert (tree, parent, 0, NULL);
		if (c->message) {
			char *uid = g_strdup (c->message->uid);

			e_tree_model_node_set_data (tree, node, uid);
			g_hash_table_insert (ml->uid_rowmap, uid,
					     GINT_TO_POINTER ((*row)++));
		}
		if (c->child) {
			/* by default, open all trees */
			e_tree_model_node_set_expanded (tree, node, TRUE);
			build_tree (ml, node, c->child, row);
		}
		c = c->next;
	}
}

static void
build_flat (MessageList *ml, ETreePath *parent, GPtrArray *uids)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *uid;
	int i;

	for (i = 0; i < uids->len; i++) {
		uid = g_strdup (uids->pdata[i]);
		node = e_tree_model_node_insert (tree, ml->tree_root, i, uid);
		g_hash_table_insert (ml->uid_rowmap, uid, GINT_TO_POINTER (i));
	}
}

void
message_list_regenerate (MessageList *message_list, const char *search)
{
	ETreeModel *etm = E_TREE_MODEL (message_list->table_model);
	GPtrArray *uids;
	int row = 0;

	if (message_list->search) {
		g_free (message_list->search);
		message_list->search = NULL;
	}

	if (message_list->uid_rowmap)
		g_hash_table_destroy (message_list->uid_rowmap);
	message_list->uid_rowmap = g_hash_table_new (g_str_hash, g_str_equal);

	if (search) {
		CamelException ex;

		camel_exception_init (&ex);
		uids = camel_folder_search_by_expression (message_list->folder,
							  search, &ex);
		if (camel_exception_is_set (&ex)) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  "Search failed: %s",
				  camel_exception_get_description (&ex));
			camel_exception_clear (&ex);
		} else
			message_list->search = g_strdup (search);
	} else
		uids = camel_folder_get_uids (message_list->folder, NULL);

	/* FIXME: free the old tree data */

	/* Clear the old contents, build the new */
	if (message_list->tree_root)
		e_tree_model_node_remove(etm, message_list->tree_root);
	message_list->tree_root =
		e_tree_model_node_insert(etm, NULL, 0, message_list);
	e_tree_model_node_set_expanded (etm, message_list->tree_root, TRUE);

	if (threaded_view) {
		struct _container *head;

		head = thread_messages (message_list->folder, uids);
		build_tree (message_list, message_list->tree_root, head, &row);
		thread_messages_free (head);
	} else
		build_flat (message_list, message_list->tree_root, uids);

	if (search) {
		g_strfreev ((char **)uids->pdata);
		g_ptr_array_free (uids, FALSE);
	} else
		camel_folder_free_uids (message_list->folder, uids);

	e_table_model_changed (message_list->table_model);
	message_list->rows_selected = 0;
	select_msg (message_list, 0);
}

static void
folder_changed (CamelFolder *f, int type, MessageList *message_list)
{
	message_list_regenerate (message_list, message_list->search);
}

static void
message_changed (CamelFolder *f, const char *uid, MessageList *message_list)
{
	int row;

	row = GPOINTER_TO_INT (g_hash_table_lookup (message_list->uid_rowmap,
						    uid));
	if (row != -1)
		e_table_model_row_changed (message_list->table_model, row);
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
		gtk_object_unref (GTK_OBJECT (message_list->folder));

	message_list->folder = camel_folder;

	gtk_signal_connect(GTK_OBJECT (camel_folder), "folder_changed",
			   folder_changed, message_list);
	gtk_signal_connect(GTK_OBJECT (camel_folder), "message_changed",
			   message_changed, message_list);

	gtk_object_ref (GTK_OBJECT (camel_folder));

	folder_changed (camel_folder, 0, message_list);
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
on_cursor_change_cmd (ETable *table, int row, gpointer user_data)
{
	MessageList *message_list;
	const CamelMessageInfo *info;
	
	message_list = MESSAGE_LIST (user_data);
	
	info = get_message_info (message_list, row);
	if (!info)
		return;
	
	message_list->cursor_row = row;
	message_list->cursor_uid = info->uid;

	if (!message_list->idle_id)
		message_list->idle_id = g_idle_add_full (G_PRIORITY_LOW, on_cursor_change_idle, message_list, NULL);
}

static void
on_row_selection (ETable *table, int row, gboolean selected,
		  gpointer user_data)
{
	MessageList *message_list = user_data;

	if (selected)
		message_list->rows_selected++;
	else
		message_list->rows_selected--;
}


/* FIXME: this is all a kludge. */
static gint
idle_select_row (gpointer user_data)
{
	e_table_set_cursor_row (user_data, 0);
	return FALSE;
}

static void
select_row (ETable *table, gpointer user_data)
{
	MessageList *message_list = user_data;

	gtk_idle_add (idle_select_row, message_list->etable);
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
	const CamelMessageInfo *info;

	info = get_message_info (mlfe_data->message_list, row);
	if (info) {
		mlfe_data->callback (mlfe_data->message_list,
				     info->uid,
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
	e_table_selected_row_foreach (E_TABLE (message_list->etable),
				      mlfe_callback, &mlfe_data);
}

gboolean threaded_view = TRUE;

void
message_list_toggle_threads (BonoboUIHandler *uih, void *user_data,
			     const char *path)
{
	MessageList *ml = user_data;

	threaded_view = bonobo_ui_handler_menu_get_toggle_state (uih, path);
	message_list_regenerate (ml, ml->search);
}
