/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * message-list.c: Displays the messages.
 *                 Implements CORBA's Evolution::MessageList
 *
 * Author:
 *     Miguel de Icaza (miguel@helixcode.com)
 *     Bertrand Guiheneuf (bg@aful.org)
 *     And just about everyone else in evolution ...
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include <e-util/ename/e-name-western.h>

#include <string.h>
#include <ctype.h>

#include "mail-config.h"
#include "message-list.h"
#include "message-thread.h"
#include "mail-threads.h"
#include "mail-tools.h"
#include "Mail.h"

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/e-table/e-table-header-item.h>
#include <gal/e-table/e-table-item.h>

#include "art/mail-new.xpm"
#include "art/mail-read.xpm"
#include "art/mail-replied.xpm"
#include "art/attachment.xpm"
#include "art/priority-high.xpm"
#include "art/empty.xpm"
#include "art/score-lowest.xpm"
#include "art/score-lower.xpm"
#include "art/score-low.xpm"
#include "art/score-normal.xpm"
#include "art/score-high.xpm"
#include "art/score-higher.xpm"
#include "art/score-highest.xpm"

#define TIMEIT

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

#define d(x)
#define t(x)

/*
 * Default sizes for the ETable display
 *
 */
#define N_CHARS(x) (CHAR_WIDTH * (x))

#define COL_ICON_WIDTH         (16)
#define COL_ATTACH_WIDTH       (16)
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
static POA_GNOME_Evolution_MessageList__vepv evolution_message_list_vepv;

static void on_cursor_change_cmd (ETableScrolled *table, int row, gpointer user_data);
static gint on_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, MessageList *list);
static char *filter_date (const void *data);
static void free_tree_ids (ETreeModel *etm);

static void save_tree_state(MessageList *ml);

/* note: @changes is owned/freed by the caller */
static void mail_do_regenerate_messagelist (MessageList *list, const gchar *search, CamelFolderChangeInfo *changes);

/* macros for working with id's (stored in the tree nodes) */
#define id_is_uid(id) (id[0] == 'u')/* is this a uid id? */
#define id_is_subject(id) (id[0] == 's') /* is this a subject id? */
#define id_uid(id) (&id[1])	/* get the uid part of the id */
#define id_subject(id) (&id[1])	/* get the subject part of the id */

enum {
	MESSAGE_SELECTED,
	LAST_SIGNAL
};

static guint message_list_signals [LAST_SIGNAL] = {0, };

static struct {
	char **image_base;
	GdkPixbuf  *pixbuf;
} states_pixmaps [] = {
	{ mail_new_xpm,		NULL },
	{ mail_read_xpm,	NULL },
	{ mail_replied_xpm,	NULL },
	{ empty_xpm,		NULL },
	{ attachment_xpm,	NULL },
	{ priority_high_xpm,	NULL },
	{ score_lowest_xpm,     NULL },
	{ score_lower_xpm,      NULL },
	{ score_low_xpm,        NULL },
	{ score_normal_xpm,     NULL },
	{ score_high_xpm,       NULL },
	{ score_higher_xpm,     NULL },
	{ score_highest_xpm,    NULL },
	{ NULL,			NULL }
};

enum DndTargetTyhpe {
	DND_TARGET_LIST_TYPE_URI,
};
#define URI_LIST_TYPE "text/uri-list"
static GtkTargetEntry drag_types[] = {
	{ URI_LIST_TYPE, 0, DND_TARGET_LIST_TYPE_URI },
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static gint
address_compare (gconstpointer address1, gconstpointer address2)
{
	CamelInternetAddress *ia1, *ia2;
	const char *name1, *name2;
	const char *addr1, *addr2;
	gint retval = 0;
	
	ia1 = camel_internet_address_new ();
	ia2 = camel_internet_address_new ();
	
	camel_address_decode (CAMEL_ADDRESS (ia1), (const char *) address1);
	camel_address_decode (CAMEL_ADDRESS (ia2), (const char *) address2);
	
	if (!camel_internet_address_get (ia1, 0, &name1, &addr1)) {
		camel_object_unref (CAMEL_OBJECT (ia1));
		camel_object_unref (CAMEL_OBJECT (ia2));
		return 1;
	}
	
	if (!camel_internet_address_get (ia2, 0, &name2, &addr2)) {
		camel_object_unref (CAMEL_OBJECT (ia1));
		camel_object_unref (CAMEL_OBJECT (ia2));
		return -1;
	}
	
	if (!name1 && !name2) {
		/* if neither has a name we should compare addresses */
		retval = g_strcasecmp (addr1, addr2);
	} else {
		if (!name1)
			retval = -1;
		else if (!name2)
			retval = 1;
		else {
			ENameWestern *wname1, *wname2;
			
			wname1 = e_name_western_parse (name1);
			wname2 = e_name_western_parse (name2);
			
			if (!wname1->last && !wname2->last) {
				/* neither has a last name */
				retval = g_strcasecmp (name1, name2);
			} else {
				/* compare last names */
				if (!wname1->last)
					retval = -1;
				else if (!wname2->last)
					retval = 1;
				else {
					retval = g_strcasecmp (wname1->last, wname2->last);
					if (!retval) {
						/* last names are identical - compare first names */
						if (!wname1->first)
							retval = -1;
						else if (!wname2->first)
							retval = 1;
						else {
							retval = g_strcasecmp (wname1->first, wname2->first);
							if (!retval) {
								/* first names are identical - compare addresses */
								retval = g_strcasecmp (addr1, addr2);
							}
						}
					}
				}
			}
			
			e_name_western_free (wname1);
			e_name_western_free (wname2);
		}
	}
	
	camel_object_unref (CAMEL_OBJECT (ia1));
	camel_object_unref (CAMEL_OBJECT (ia2));
	
	return retval;
}

static gint
subject_compare (gconstpointer subject1, gconstpointer subject2)
{
	char *sub1;
	char *sub2;
	
	/* trim off any "Re:"'s at the beginning of subject1 */
	sub1 = (char *) subject1;
	while (!g_strncasecmp (sub1, "Re:", 3)) {
		sub1 += 3;
		/* jump over any spaces */
		for ( ; *sub1 && isspace (*sub1); sub1++);
	}
	
	/* trim off any "Re:"'s at the beginning of subject2 */
	sub2 = (char *) subject2;
	while (!g_strncasecmp (sub2, "Re:", 3)) {
		sub2 += 3;
		/* jump over any spaces */
		for ( ; *sub2 && isspace (*sub2); sub2++);
	}
	
	/* jump over any spaces */
	for ( ; *sub1 && isspace (*sub1); sub1++);
	for ( ; *sub2 && isspace (*sub2); sub2++);
	
	return g_strcasecmp (sub1, sub2);
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

	if (!id_is_uid(uid))
		return NULL;

	return id_uid(uid);
}

/* Gets the CamelMessageInfo for the message displayed at the given
 * view row.
 */
static const CamelMessageInfo *
get_message_info (MessageList *message_list, int row)
{
	const char *uid;
	
	uid = get_message_uid(message_list, row);
	if (uid)
		return camel_folder_get_message_info(message_list->folder, uid);

	return NULL;
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
 * row -1 is mapped to the last row. @flags and @mask combine to specify
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

	switch (direction) {
	case MESSAGE_LIST_SELECT_PREVIOUS:
		last = -1;
		break;
	case MESSAGE_LIST_SELECT_NEXT:
		last = e_table_model_row_count (message_list->table_model);
		break;
	default:
		g_warning("Invalid argument to message_list_select");
		return;
	}

	if (base_row == -1)
		base_row = e_table_model_row_count(message_list->table_model) - 1;

	/* model_to_view_row etc simply dont work for sorted views.  Sigh. */
	vrow = e_table_model_to_view_row (ets->table, base_row);

	/* We don't know whether to use < or > due to "direction" */
	while (vrow != last) {
		mrow = e_table_view_to_model_row (ets->table, vrow);
		info = get_message_info (message_list, mrow);
		if (info && (info->flags & mask) == flags) {
			e_table_scrolled_set_cursor_row (ets, vrow);
			gtk_signal_emit(GTK_OBJECT (message_list), message_list_signals [MESSAGE_SELECTED], info->uid);
			return;
		}
		vrow += direction;
	}

	gtk_signal_emit(GTK_OBJECT (message_list), message_list_signals [MESSAGE_SELECTED], NULL);
}

#if 0
static void
message_list_drag_data_get (ETable             *table,
			    int                 row,
			    int                 col,
			    GdkDragContext     *context,
			    GtkSelectionData   *selection_data,
			    guint               info,
			    guint               time,
			    gpointer            user_data)
{
	MessageList *mlist = (MessageList *) user_data;
	CamelMessageInfo *info = get_message_info (mlist, row);
	CamelException *ex;
	CamelFolder *folder;
	char *dirname = "/tmp/ev-XXXXXXXXXX";
	char *filename;
	char *url;
	
	switch (info) {
	case DND_TARGET_LIST_TYPE_URI:
		/* drag & drop into nautilus */
		mktemp (dirname);
		filename = g_strdup_printf ("%s.eml", info->subject);
		url = g_strdup_printf ("file:%s", dirname);
		
		ex = camel_exception_new ();
		folder = mail_tool_get_folder_from_urlname (url, filename, CAMEL_STORE_FOLDER_CREATE, ex);
		if (camel_exception_is_set (ex)) {
			camel_exception_free (ex);
			g_free (url);
			return;
		}
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					(guchar *) url, strlen (url));
		
		camel_object_unref (CAMEL_OBJECT (folder));
		g_free (filename);
		g_free (url);
		break;
	default:
		break;
	}
}
#endif

/*
 * SimpleTableModel::col_count
 */
static int
ml_col_count (ETableModel *etm, void *data)
{
	return COL_LAST;
}

static void *
ml_duplicate_value (ETableModel *etm, int col, const void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
		return (void *) value;

	case COL_FROM:
	case COL_SUBJECT:
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
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
		break;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_SIZE:
		g_free (value);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void *
ml_initialize_value (ETableModel *etm, int col, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
		return NULL;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_SIZE:
		return g_strdup("");
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static gboolean
ml_value_is_empty (ETableModel *etm, int col, const void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_SENT:
	case COL_RECEIVED:
		return value == NULL;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_SIZE:
		return !(value && *(char *)value);
	default:
		g_assert_not_reached ();
		return FALSE;
	}
}

static char *
ml_value_to_string (ETableModel *etm, int col, const void *value, void *data)
{
	switch (col){
	case COL_MESSAGE_STATUS:
		switch ((int) value) {
		case 0:
			return g_strdup (_("Unseen"));
			break;
		case 1:
			return g_strdup (_("Seen"));
			break;
		case 2:
			return g_strdup (_("Answered"));
			break;
		default:
			return g_strdup ("");
			break;
		}
		break;
		
	case COL_SCORE:
		switch ((int) value) {
		case -3:
			return g_strdup ("Lowest");
			break;
		case -2:
			return g_strdup ("Lower");
			break;
		case -1:
			return g_strdup ("Low");
			break;
		case 1:
			return g_strdup ("High");
			break;
		case 2:
			return g_strdup ("Higher");
			break;
		case 3:
			return g_strdup ("Highest");
			break;
		default:
			return g_strdup ("Normal");
			break;
		}
		break;
		
	case COL_ATTACHMENT:
	case COL_FLAGGED:
	case COL_DELETED:
	case COL_UNREAD:
		return g_strdup_printf("%d", (int) value);
		
	case COL_SENT:
	case COL_RECEIVED:
		return filter_date (value);
		
	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_SIZE:
		return g_strdup (value);
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static GdkPixbuf *
ml_tree_icon_at (ETreeModel *etm, ETreePath *path, void *model_data)
{
	/* we dont really need an icon ... */
	return NULL;
}

/* return true if there are any unread messages in the subtree */
static int
subtree_unread(MessageList *ml, ETreePath *node)
{
	const CamelMessageInfo *info;
	char *uid;

	while (node) {
		ETreePath *child;
		uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
		if (id_is_uid(uid)) {
			info = camel_folder_get_message_info(ml->folder, id_uid(uid));
			if (!(info->flags & CAMEL_MESSAGE_SEEN))
				return TRUE;
		}
		if ((child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->table_model), node)))
			if (subtree_unread(ml, child))
				return TRUE;
		node = e_tree_model_node_get_next (E_TREE_MODEL (ml->table_model), node);
	}
	return FALSE;
}

static gboolean
content_is_attachment(CamelMessageContentInfo *ci)
{
	CamelMessageContentInfo *child;

	/* no info about content */
	if (ci == NULL)
		return FALSE;

	/* we assume multipart/mixed is an attachment always
	   other multipart / * is only an attachment if it contains multipart/mixed's, or
	   non-text parts */
	if (header_content_type_is(ci->type, "multipart", "*")) {
		if (header_content_type_is(ci->type, "multipart", "mixed")) {
			return TRUE;
		}
		child = ci->childs;
		while (child) {
			if (content_is_attachment(child)) {
				return TRUE;
			}
			child = child->next;
		}
		return FALSE;
	} else {
		return !header_content_type_is(ci->type, "text", "*");
	}
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
	if (!id_is_uid(uid))
		goto fake;
	uid = id_uid(uid);

	msg_info = camel_folder_get_message_info (message_list->folder, uid);
	if (msg_info == NULL) {
		g_warning("UID for message-list not found in folder: %s", uid);
		return NULL;
	}
	
	switch (col){
	case COL_MESSAGE_STATUS:
		if (msg_info->flags & CAMEL_MESSAGE_ANSWERED)
			return GINT_TO_POINTER (2);
		else if (msg_info->flags & CAMEL_MESSAGE_SEEN)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
		
	case COL_FLAGGED:
		if (msg_info->flags & CAMEL_MESSAGE_FLAGGED)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);

	case COL_SCORE:
	{
		const char *tag;
		int score = 0;
		
		tag = camel_tag_get ((CamelTag **) &msg_info->user_tags, "score");
		if (tag)
			score = atoi (tag);
		
		return GINT_TO_POINTER (score);
	}
		
	case COL_ATTACHMENT:
		if (content_is_attachment(msg_info->content))
			return (void *)1;
		else
			return (void *)0;
		
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
		if (msg_info->flags & CAMEL_MESSAGE_DELETED)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
		
	case COL_UNREAD:
		return GINT_TO_POINTER (!(msg_info->flags & CAMEL_MESSAGE_SEEN));
		
	case COL_COLOUR:
	{
		const char *colour;

		colour = camel_tag_get ((CamelTag **) &msg_info->user_tags,
					"colour");
		if (colour)
			return (void *)colour;
		else if (msg_info->flags & CAMEL_MESSAGE_FLAGGED)
			/* FIXME: extract from the xpm somehow. */
			return "#A7453E";
		else
			return NULL;
	}
	}

	g_assert_not_reached ();
	
 fake:
	/* This is a fake tree parent */
	switch (col){
	case COL_UNREAD:
		/* this value should probably be cached, as it could take a bit
		   of processing to evaluate all the time */
		return (void *)subtree_unread(message_list,
					      e_tree_model_node_get_first_child(etm, path));
	case COL_MESSAGE_STATUS:
	case COL_FLAGGED:
	case COL_SCORE:
	case COL_ATTACHMENT:
	case COL_DELETED:
	case COL_COLOUR:
	case COL_SENT:
	case COL_RECEIVED:
		return (void *) 0;
		
	case COL_SUBJECT:
		return id_subject(uid);
		
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
	g_assert_not_reached ();
}

static gboolean
ml_tree_is_cell_editable (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	return FALSE;
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

static ETableExtras *
message_list_create_extras (void)
{
	int i;
	GdkPixbuf *images [7];
	ETableExtras *extras;
	ECell *cell;

	extras = e_table_extras_new();
	e_table_extras_add_pixbuf(extras, "status", states_pixmaps [0].pixbuf);
	e_table_extras_add_pixbuf(extras, "score", states_pixmaps [11].pixbuf);
	e_table_extras_add_pixbuf(extras, "attachment", states_pixmaps [4].pixbuf);
	e_table_extras_add_pixbuf(extras, "flagged", states_pixmaps [5].pixbuf);

	e_table_extras_add_compare(extras, "address_compare", address_compare);
	e_table_extras_add_compare(extras, "subject_compare", subject_compare);

	for (i = 0; i < 3; i++)
		images [i] = states_pixmaps [i].pixbuf;
	
	e_table_extras_add_cell(extras, "render_message_status", e_cell_toggle_new (0, 3, images));

	for (i = 0; i < 2; i++)
		images [i] = states_pixmaps [i + 3].pixbuf;
	
	e_table_extras_add_cell(extras, "render_attachment", e_cell_toggle_new (0, 2, images));
	
	images [1] = states_pixmaps [5].pixbuf;
	e_table_extras_add_cell(extras, "render_flagged", e_cell_toggle_new (0, 2, images));

	for (i = 0; i < 7; i++)
		images[i] = states_pixmaps [i + 5].pixbuf;
	
	e_table_extras_add_cell(extras, "render_score", e_cell_toggle_new (0, 7, images));

	cell = e_cell_text_new (
		NULL, GTK_JUSTIFY_LEFT);
	
	gtk_object_set (GTK_OBJECT (cell),
			"text_filter", filter_date,
			NULL);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", COL_DELETED,
			NULL);
	gtk_object_set (GTK_OBJECT (cell),
			"bold_column", COL_UNREAD,
			NULL);
	gtk_object_set (GTK_OBJECT (cell),
			"color_column", COL_COLOUR,
			NULL);
	e_table_extras_add_cell(extras, "render_date", cell);

	cell = e_cell_text_new (
		NULL, GTK_JUSTIFY_LEFT);

	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", COL_DELETED,
			NULL);
	gtk_object_set (GTK_OBJECT (cell),
			"bold_column", COL_UNREAD,
			NULL);
	gtk_object_set (GTK_OBJECT (cell),
			"color_column", COL_COLOUR,
			NULL);
	e_table_extras_add_cell(extras, "render_text", cell);

	e_table_extras_add_cell(extras, "render_tree", 
		e_cell_tree_new (NULL, NULL, /* let the tree renderer default the pixmaps */
				 TRUE, cell));

	return extras;
}

static void
save_header_state(MessageList *ml)
{
	char *filename;

	if (ml->folder == NULL
	    || ml->etable == NULL)
		return;

	filename = mail_config_folder_to_cachename(ml->folder, "et-header-");
	e_table_scrolled_save_state(E_TABLE_SCROLLED(ml->etable), filename);
	g_free(filename);
}

static char *
message_list_get_layout (MessageList *message_list)
{
	/* Default: Status, Attachments, Priority, From, Subject, Date */
	return g_strdup ("<ETableSpecification cursor-mode=\"line\" draw-grid=\"true\">"
			 "<ETableColumn model_col= \"0\" pixbuf=\"status\" expansion=\"0.0\" minimum_width=\"18\" resizable=\"false\" cell=\"render_message_status\" compare=\"integer\" sortable=\"false\"/>"
			 "<ETableColumn model_col= \"1\" pixbuf=\"flagged\" expansion=\"0.0\" minimum_width=\"20\" resizable=\"false\" cell=\"render_flagged\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"2\" pixbuf=\"score\" expansion=\"0.0\" minimum_width=\"20\" resizable=\"false\" cell=\"render_score\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"3\" pixbuf=\"attachment\" expansion=\"0.0\" minimum_width=\"18\" resizable=\"false\" cell=\"render_attachment\" compare=\"integer\" sortable=\"false\"/>"
			 "<ETableColumn model_col= \"4\" _title=\"From\" expansion=\"24.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_text\" compare=\"address_compare\"/>"
			 "<ETableColumn model_col= \"5\" _title=\"Subject\" expansion=\"30.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_tree\" compare=\"subject_compare\"/>"
			 "<ETableColumn model_col= \"6\" _title=\"Date\" expansion=\"24.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_date\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"7\" _title=\"Received\" expansion=\"20.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_date\" compare=\"integer\"/>"
			 "<ETableColumn model_col= \"8\" _title=\"To\" expansion=\"24.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_text\" compare=\"address_compare\"/>"
			 "<ETableColumn model_col= \"9\" _title=\"Size\" expansion=\"6.0\" minimum_width=\"32\" resizable=\"true\" cell=\"render_text\" compare=\"string\"/>"
			 "<ETableState> <column source=\"0\"/> <column source=\"3\"/> <column source=\"1\"/>"
			 "<column source=\"4\"/> <column source=\"5\"/> <column source=\"6\"/>"
			 "<grouping> </grouping> </ETableState>"
			 "</ETableSpecification>");
}

static void
message_list_setup_etable(MessageList *message_list)
{
	/* build the spec based on the folder, and possibly from a saved file */
	/* otherwise, leave default */
	if (message_list->folder) {
		char *path;
		char *name;
		struct stat st;
		
		name = camel_service_get_name (CAMEL_SERVICE (message_list->folder->parent_store), TRUE);
		printf ("folder name is '%s'\n", name);
		path = mail_config_folder_to_cachename (message_list->folder, "et-header-");
		
		if (path && stat (path, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode)) {
			/* build based on saved file */
			e_table_scrolled_load_state (E_TABLE_SCROLLED (message_list->etable), path);
		} else if (strstr (name, "/Drafts") || strstr (name, "/Outbox") || strstr (name, "/Sent")) {
			/* these folders have special defaults */
			char *state = "<ETableState>"
				"<column source=\"0\"/> <column source=\"1\"/> "
				"<column source=\"8\"/> <column source=\"5\"/> "
				"<column source=\"6\"/> <grouping> </grouping> </ETableState>";
			
			e_table_scrolled_set_state (E_TABLE_SCROLLED (message_list->etable), state);
		}
		
		g_free (path);
		g_free (name);
	}
}

/*
 * GtkObject::init
 */
static void
message_list_init (GtkObject *object)
{
	ETableExtras *extras;
	MessageList *message_list = MESSAGE_LIST (object);
	char *spec;

	message_list->table_model = (ETableModel *)
		e_tree_simple_new (ml_col_count,
				   ml_duplicate_value,
				   ml_free_value,
				   ml_initialize_value,
				   ml_value_is_empty,
				   ml_value_to_string,
				   ml_tree_icon_at, ml_tree_value_at,
				   ml_tree_set_value_at,
				   ml_tree_is_cell_editable,
				   message_list);
	e_tree_model_root_node_set_visible ((ETreeModel *)message_list->table_model, FALSE);
	gtk_signal_connect (GTK_OBJECT (message_list->table_model), "destroy", 
			    (GtkSignalFunc) free_tree_ids, NULL);

	/*
	 * The etable
	 */

	spec = message_list_get_layout (message_list);
	extras = message_list_create_extras();
	message_list->etable = e_table_scrolled_new (message_list->table_model, extras, spec, NULL);
	g_free (spec);
	gtk_object_sink(GTK_OBJECT(extras));

	e_scroll_frame_set_policy (E_SCROLL_FRAME (message_list->etable), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	gtk_object_set(GTK_OBJECT(message_list->etable),
		       "drawfocus", FALSE,
		       NULL);

	/*
	 *gtk_signal_connect (GTK_OBJECT (message_list->etable), "realize",
	 *		    GTK_SIGNAL_FUNC (select_row), message_list);
	 */

	gtk_signal_connect (GTK_OBJECT (message_list->etable), "cursor_change",
			   GTK_SIGNAL_FUNC (on_cursor_change_cmd), message_list);

	gtk_signal_connect (GTK_OBJECT (message_list->etable), "click",
			    GTK_SIGNAL_FUNC (on_click), message_list);

#if 0
	/* drag & drop */
	e_table_drag_source_set (message_list->etable, GDK_BUTTON1_MASK,
				 drag_types, num_drag_types, GDK_ACTION_MOVE);
	
	gtk_signal_connect (GTK_OBJECT (message_list->etable), "drag_data_get",
			    GTK_SIGNAL_FUNC (message_list_drag_data_get), message_list);
#endif
	
	gtk_widget_show (message_list->etable);

	gtk_object_ref (GTK_OBJECT (message_list->table_model));
	gtk_object_sink (GTK_OBJECT (message_list->table_model));
	
	/*
	 * We do own the Etable, not some widget container
	 */
	gtk_object_ref (GTK_OBJECT (message_list->etable));
	gtk_object_sink (GTK_OBJECT (message_list->etable));

	message_list->uid_rowmap = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
message_list_destroy (GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);

	if (message_list->folder) {
		save_tree_state(message_list);
		save_header_state(message_list);
	}
	
	gtk_object_unref (GTK_OBJECT (message_list->table_model));
	gtk_object_unref (GTK_OBJECT (message_list->etable));
	
	g_hash_table_destroy (message_list->uid_rowmap);

	if (message_list->idle_id != 0)
		g_source_remove(message_list->idle_id);
	
	if (message_list->seen_id)
		gtk_timeout_remove (message_list->seen_id);
	
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

static POA_GNOME_Evolution_MessageList__epv *
evolution_message_list_get_epv (void)
{
	POA_GNOME_Evolution_MessageList__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_MessageList__epv, 1);

	epv->selectMessage = MessageList_select_message;
	epv->openMessage   = MessageList_open_message;

	return epv;
}

static void
message_list_corba_class_init (void)
{
	evolution_message_list_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	evolution_message_list_vepv.GNOME_Evolution_MessageList_epv = evolution_message_list_get_epv ();
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

	message_list_signals[MESSAGE_SELECTED] =
		gtk_signal_new ("message_selected",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (MessageListClass, message_selected),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

	gtk_object_class_add_signals(object_class, message_list_signals, LAST_SIGNAL);

	message_list_init_images ();
}

static void
message_list_construct (MessageList *message_list, GNOME_Evolution_MessageList corba_message_list)
{
	bonobo_object_construct (BONOBO_OBJECT (message_list), corba_message_list);
}

static GNOME_Evolution_MessageList
create_corba_message_list (BonoboObject *object)
{
	POA_GNOME_Evolution_MessageList *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_MessageList *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &evolution_message_list_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_MessageList__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Evolution_MessageList) bonobo_object_activate_servant (object, servant);
}

BonoboObject *
message_list_new (void)
{
	GNOME_Evolution_MessageList corba_object;
	MessageList *message_list;

	message_list = gtk_type_new (message_list_get_type ());

	corba_object = create_corba_message_list (BONOBO_OBJECT (message_list));
	if (corba_object == CORBA_OBJECT_NIL){
		gtk_object_destroy (GTK_OBJECT (message_list));
		return NULL;
	}

	message_list->idle_id = 0;

	message_list_construct (message_list, corba_object);

	return BONOBO_OBJECT (message_list);
}

static void
clear_tree (MessageList *ml)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Clearing tree\n");
	gettimeofday(&start, NULL);
#endif

	/* we also reset the uid_rowmap since it is no longer useful/valid anyway */
	g_hash_table_destroy (ml->uid_rowmap);
	ml->uid_rowmap = g_hash_table_new(g_str_hash, g_str_equal);
	free_tree_ids(etm);
	
	if (ml->tree_root) {
		/* FIXME: Freeze here, buddy. */
		e_tree_model_node_remove (etm, ml->tree_root);
	}

	ml->tree_root = e_tree_model_node_insert (etm, NULL, 0, NULL);
	e_tree_model_node_set_expanded (etm, ml->tree_root, TRUE);

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Clearing tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

/* we save the node id to the file if the node should be closed when
   we start up.  We only save nodeid's for messages with children */
static void
save_node_state(MessageList *ml, FILE *out, ETreePath *node)
{
	char *data;
	const CamelMessageInfo *info;

	while (node) {
		ETreePath *child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->table_model), node);
		if (child
		    && !e_tree_model_node_is_expanded((ETreeModel *)ml->table_model, node)) {
			data = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
			if (data) {
				if (id_is_uid(data)) {
					info = camel_folder_get_message_info(ml->folder, id_uid(data));
					if (info) {
						fprintf(out, "%s\n", info->message_id);
					}
				} else {
					fprintf(out, "%s\n", data);
				}
			}
		}
		if (child) {
			save_node_state(ml, out, child);
		}
		node = e_tree_model_node_get_next (E_TREE_MODEL (ml->table_model), node);
	}
}

static GHashTable *
load_tree_state(MessageList *ml)
{
	char *filename, linebuf[10240];
	GHashTable *result;
	FILE *in;
	int len;

	result = g_hash_table_new(g_str_hash, g_str_equal);
	filename = mail_config_folder_to_cachename(ml->folder, "treestate-");
	in = fopen(filename, "r");
	if (in) {
		while (fgets(linebuf, sizeof(linebuf), in) != NULL) {
			len = strlen(linebuf);
			if (len) {
				linebuf[len-1] = 0;
				g_hash_table_insert(result, g_strdup(linebuf), (void *)1);
			}
		}
		fclose(in);
	}
	g_free(filename);
	return result;
}

/* save tree info */
static void
save_tree_state(MessageList *ml)
{
	char *filename;
	ETreePath *node;
	ETreePath *child;
	FILE *out;

	filename = mail_config_folder_to_cachename(ml->folder, "treestate-");
	out = fopen(filename, "w");
	if (out) {
		node = e_tree_model_get_root((ETreeModel *)ml->table_model);
		child = e_tree_model_node_get_first_child ((ETreeModel *)ml->table_model, node);
		if (node && child) {
			save_node_state(ml, out, child);
		}
		fclose(out);
	}
	g_free(filename);
}

static void
free_node_state(void *key, void *value, void *data)
{
	g_free(key);
}

static void
free_tree_state(GHashTable *expanded_nodes)
{
	g_hash_table_foreach(expanded_nodes, free_node_state, 0);
	g_hash_table_destroy(expanded_nodes);
}

/* only call if we have a tree model */
/* builds the tree structure */
static void build_subtree (MessageList *ml, ETreePath *parent, struct _container *c, int *row, GHashTable *);

static void build_subtree_diff (MessageList *ml, ETreePath *parent, ETreePath *path, struct _container *c, int *row, GHashTable *expanded_nodes);

static void
build_tree (MessageList *ml, struct _thread_messages *thread)
{
	int row = 0;
	GHashTable *expanded_nodes;
	ETreeModel *etm = (ETreeModel *)ml->table_model;
	ETreePath *top;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Building tree\n");
	gettimeofday(&start, NULL);
#endif
	expanded_nodes = load_tree_state(ml);

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Loading tree state took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

	if (ml->tree_root == NULL) {
		ml->tree_root =	e_tree_model_node_insert(etm, NULL, 0, NULL);
		e_tree_model_node_set_expanded(etm, ml->tree_root, TRUE);
	}

	top = e_tree_model_node_get_first_child(etm, ml->tree_root);
	if (top == NULL) {
		build_subtree(ml, ml->tree_root, thread->tree, &row, expanded_nodes);
	} else {
		build_subtree_diff(ml, ml->tree_root, top,  thread->tree, &row, expanded_nodes);
	}

	free_tree_state(expanded_nodes);
	
#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Building tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static char *
new_id_from_uid(const char *uid)
{
	char *res;
	int len;

	len = strlen(uid)+2;
	res = g_malloc(len);
	res[0] = 'u';
	strcpy(res+1, uid);
	return res;
}

static char *
new_id_from_subject(const char *subject)
{
	char *res;
	int len;

	len = strlen(subject)+2;
	res = g_malloc(len);
	res[0] = 's';
	strcpy(res+1, subject);
	return res;
}

/* this is about 20% faster than build_subtree_diff,
   entirely because e_tree_model_node_insert(xx, -1 xx)
   is faster than inserting to the right row :( */
/* Otherwise, this code would probably go as it does the same thing essentially */
static void
build_subtree (MessageList *ml, ETreePath *parent, struct _container *c, int *row, GHashTable *expanded_nodes)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *id;
	int expanded = FALSE;

	e_tree_model_freeze (tree);

	while (c) {
		if (c->message) {
			id = new_id_from_uid(c->message->uid);
			g_hash_table_insert(ml->uid_rowmap, id_uid(id), GINT_TO_POINTER ((*row)++));
			if (c->child) {
				if (c->message && c->message->message_id)
					expanded = !g_hash_table_lookup(expanded_nodes, c->message->message_id) != 0;
				else
					expanded = TRUE;
			}
		} else {
			id = new_id_from_subject(c->root_subject);
			if (c->child) {
				expanded = !g_hash_table_lookup(expanded_nodes, id) != 0;
			}
		}
		node = e_tree_model_node_insert(tree, parent, -1, id);
		if (c->child) {
			/* by default, open all trees */
			if (expanded)
				e_tree_model_node_set_expanded(tree, node, expanded);
			build_subtree(ml, node, c->child, row, expanded_nodes);
		}
		c = c->next;
	}

	e_tree_model_thaw (tree);
}

/* compares a thread tree node with the etable tree node to see if they point to
   the same object */
static int
node_equal(ETreeModel *etm, ETreePath *ap, struct _container *bp)
{
	char *uid;

	uid = e_tree_model_node_get_data(etm, ap);

	if (id_is_uid(uid)) {
		if (bp->message && strcmp(id_uid(uid), bp->message->uid)==0)
			return 1;
	} else if (id_is_subject(uid)) {
		if (bp->message == NULL && strcmp(id_subject(uid), bp->root_subject) == 0)
			return 1;
	}
	return 0;
}

/* adds a single node, retains save state, and handles adding children if required */
static void
add_node_diff(MessageList *ml, ETreePath *parent, ETreePath *path, struct _container *c, int *row, int myrow, GHashTable *expanded_nodes)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *id;
	int expanded = FALSE;

	if (c->message) {
		id = new_id_from_uid(c->message->uid);
		/* need to remove the id first, as GHashTable' wont replace the key pointer for us */
		g_hash_table_remove(ml->uid_rowmap, id_uid(id));
		g_hash_table_insert(ml->uid_rowmap, id_uid(id), GINT_TO_POINTER (*row));
		if (c->child) {
			if (c->message && c->message->message_id)
				expanded = !g_hash_table_lookup(expanded_nodes, c->message->message_id) != 0;
			else
				expanded = TRUE;
		}
	} else {
		id = new_id_from_subject(c->root_subject);
		if (c->child) {
			expanded = !g_hash_table_lookup(expanded_nodes, id) != 0;
		}
	}

	t(printf("Adding node: %s row %d\n", id, myrow));

	node = e_tree_model_node_insert(etm, parent, myrow, id);
	(*row)++;
	if (c->child) {
		e_tree_model_node_set_expanded(etm, node, expanded);
		t(printf("Building subtree ...\n"));
		build_subtree_diff(ml, node, NULL, c->child, row, expanded_nodes);
	}
}

/* removes node, children recursively and all associated data */
static void
remove_node_diff(MessageList *ml, ETreePath *node, int depth)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);
	ETreePath *cp, *cn;
	char *uid, *olduid;
	int oldrow;

	t(printf("Removing node: %s\n", (char *)e_tree_model_node_get_data(etm, node)));

	/* we depth-first remove all node data's ... */
	cp = e_tree_model_node_get_first_child(etm, node);
	while (cp) {
		cn = e_tree_model_node_get_next(etm, cp);
		remove_node_diff(ml, cp, depth+1);
		cp = cn;
	}

	/* and the rowid entry - if and only if it is referencing this node */
	uid = e_tree_model_node_get_data(etm, node);
	if (id_is_uid(uid)
	    && g_hash_table_lookup_extended(ml->uid_rowmap, id_uid(uid), (void *)&olduid, (void *)&oldrow)
	    && olduid == id_uid(uid)) {
		t(printf("removing rowid map entry: %s\n", id_uid(uid)));
		g_hash_table_remove(ml->uid_rowmap, id_uid(uid));
	}
	g_free(uid);
	e_tree_model_node_set_data(etm, node, NULL);

	/* and only at the toplevel, remove the node (etree should optimise this remove somewhat) */
	if (depth == 0)
		e_tree_model_node_remove(etm, node);
}

/* applies a new tree structure to an existing tree, but only by changing things
   that have changed */
static void
build_subtree_diff(MessageList *ml, ETreePath *parent, ETreePath *path, struct _container *c, int *row, GHashTable *expanded_nodes)
{
	ETreeModel *etm = E_TREE_MODEL (ml->table_model);
	ETreePath *ap, *ai, *at, *tmp;
	struct _container *bp, *bi, *bt;
	int i, j, myrow = 0;

	ap = path;
	bp = c;

	while (ap || bp) {
		t(printf("Processing row: %d (subtree row %d)\n", *row, myrow));
		if (ap == NULL) {
			t(printf("out of old nodes\n"));
			/* ran out of old nodes - remaining nodes are added */
			add_node_diff(ml, parent, ap, bp, row, myrow, expanded_nodes);
			myrow++;
			bp = bp->next;
		} else if (bp == NULL) {
			t(printf("out of new nodes\n"));
			/* ran out of new nodes - remaining nodes are removed */
			tmp = e_tree_model_node_get_next(etm, ap);
			remove_node_diff(ml, ap, 0);
			ap = tmp;
		} else if (node_equal(etm, ap, bp)) {
			/*t(printf("nodes match, verify\n"));*/
			/* matching nodes, verify details/children */
			if (bp->message) {
				char *olduid;
				int oldrow;

				/* if this is a message row, check/update the row id map */
				if (g_hash_table_lookup_extended(ml->uid_rowmap, bp->message->uid, (void *)&olduid, (void *)&oldrow)) {
					if (oldrow != (*row)) {
						g_hash_table_insert(ml->uid_rowmap, olduid, (void *)(*row));
					}
				} else {
					g_warning("Cannot find uid %s in table?", bp->message->uid);
					/*g_assert_not_reached();*/
				}
			}
			*row = (*row)+1;
			myrow++;
			tmp = e_tree_model_node_get_first_child(etm, ap);
			/* make child lists match (if either has one) */
			if (bp->child || tmp) {
				build_subtree_diff(ml, ap, tmp, bp->child, row, expanded_nodes);
			}
			ap = e_tree_model_node_get_next(etm, ap);
			bp = bp->next;
		} else {
			t(printf("searching for matches\n"));
			/* we have to scan each side for a match */
			bi = bp->next;
			ai = e_tree_model_node_get_next(etm, ap);
			for (i=1;bi!=NULL;i++,bi=bi->next) {
				if (node_equal(etm, ap, bi))
					break;
			}
			for (j=1;ai!=NULL;j++,ai=e_tree_model_node_get_next(etm, ai)) {
				if (node_equal(etm, ai, bp))
					break;
			}
			if (i<j) {
				/* smaller run of new nodes - must be nodes to add */
				if (bi) {
					bt = bp;
					while (bt != bi) {
						t(printf("adding new node 0\n"));
						add_node_diff(ml, parent, NULL, bt, row, myrow, expanded_nodes);
						myrow++;
						bt = bt->next;
					}
					bp = bi;
				} else {
					t(printf("adding new node 1\n"));
					/* no match in new nodes, add one, try next */
					add_node_diff(ml, parent, NULL, bp, row, myrow, expanded_nodes);
					myrow++;
					bp = bp->next;
				}
			} else {
				/* bigger run of old nodes - must be nodes to remove */
				if (ai) {
					at = ap;
					while (at != ai) {
						t(printf("removing old node 0\n"));
						tmp = e_tree_model_node_get_next(etm, at);
						remove_node_diff(ml, at, 0);
						at = tmp;
					}
					ap = ai;
				} else {
					t(printf("adding new node 2\n"));
					/* didn't find match in old nodes, must be new node? */
					add_node_diff(ml, parent, NULL, bp, row, myrow, expanded_nodes);
					myrow++;
					bp = bp->next;
#if 0
					tmp = e_tree_model_node_get_next(etm, ap);
					remove_node_diff(etm, ap, 0);
					ap = tmp;
#endif
				}
			}
		}
	}
}

static gboolean
free_ids_cb (ETreeModel *model, ETreePath *node, gpointer data)
{
	g_free (e_tree_model_node_get_data (model, node));
	return FALSE;
}

static void
free_tree_ids (ETreeModel *etm)
{
	ETreePath *root = e_tree_model_get_root (etm);

	if (root)
		e_tree_model_node_traverse(etm, root, free_ids_cb, NULL);
}

static void build_flat_diff(MessageList *ml, CamelFolderChangeInfo *changes);

static void
build_flat (MessageList *ml, GPtrArray *uids, CamelFolderChangeInfo *changes)
{
	ETreeModel *tree = E_TREE_MODEL (ml->table_model);
	ETreePath *node;
	char *uid;
	int i;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Building flat\n");
	gettimeofday(&start, NULL);
#endif

	if (changes) {
		build_flat_diff(ml, changes);
	} else {
		e_table_model_pre_change(ml->table_model);
		clear_tree (ml);
		for (i = 0; i < uids->len; i++) {
			uid = new_id_from_uid(uids->pdata[i]);
			node = e_tree_model_node_insert (tree, ml->tree_root, -1, uid);
			g_hash_table_insert (ml->uid_rowmap, id_uid(uid), GINT_TO_POINTER (i));
		}
		e_table_model_changed(ml->table_model);
	}

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Building flat took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

/* used to sort the rows to match list order */
struct _uidsort {
	int row;
	char *uid;
};

static int
sort_uid_cmp(const void *ap, const void *bp)
{
	const struct _uidsort *a = (struct _uidsort *)ap;
	const struct _uidsort *b = (struct _uidsort *)bp;

	if (a->row < b->row)
		return -1;
	else if (a->row > b->row)
		return 1;
	return 0;
}

static void
sort_uid_to_rows(MessageList *ml, GPtrArray *uids)
{
	struct _uidsort *uidlist;
	int i;

	uidlist = g_malloc(sizeof(struct _uidsort) * uids->len);
	for (i=0;i<uids->len;i++) {
		uidlist[i].row = (int)g_hash_table_lookup(ml->uid_rowmap, uids->pdata[i]);
		uidlist[i].uid = uids->pdata[i];
	}
	qsort(uidlist, uids->len, sizeof(struct _uidsort), sort_uid_cmp);
	for (i=0;i<uids->len;i++) {
		uids->pdata[i] = uidlist[i].uid;
	}
	g_free(uidlist);
}

static void
build_flat_diff(MessageList *ml, CamelFolderChangeInfo *changes)
{
	int row, i;
	ETreePath *node;
	char *uid;
	int oldrow;
	char *olduid;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;
		
	gettimeofday(&start, NULL);
#endif

	printf("updating changes to display\n");

	/* remove individual nodes? */
	if (changes->uid_removed->len > 0) {
		/* first, we need to sort the row id's to match the summary order */
		sort_uid_to_rows(ml, changes->uid_removed);

		/* we remove from the end, so that the rowmap remains valid as we go */
		d(printf("Removing messages from view:\n"));
		for (i=changes->uid_removed->len-1;i>=0;i--) {
			d(printf(" %s\n", (char *)changes->uid_removed->pdata[i]));
			if (g_hash_table_lookup_extended(ml->uid_rowmap, changes->uid_removed->pdata[i], (void *)&olduid, (void *)&row)) {
				node = e_tree_model_node_at_row((ETreeModel *)ml->table_model, row);
				uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
				if (uid && id_is_uid(uid) && !strcmp(id_uid(uid), changes->uid_removed->pdata[i])) {
					g_hash_table_remove(ml->uid_rowmap, olduid);
					e_tree_model_node_remove((ETreeModel *)ml->table_model, node);
					g_free(uid);
					d(printf("  - removed\n"));
				} else {
					d(printf("  - is this the right uid, it doesn't match my map?\n"));
				}
			}
		}
	}

	/* add new nodes? - just append to the end */
	if (changes->uid_added->len > 0) {
		node = e_tree_model_node_get_last_child((ETreeModel *)ml->table_model, ml->tree_root);
		row = e_tree_model_row_of_node((ETreeModel *)ml->table_model, node) + 1;
		d(printf("Adding messages to view:\n"));
		for (i=0;i<changes->uid_added->len;i++) {
			d(printf(" %s\n", (char *)changes->uid_added->pdata[i]));
			uid = new_id_from_uid(changes->uid_added->pdata[i]);
			node = e_tree_model_node_insert((ETreeModel *)ml->table_model, ml->tree_root, row, uid);
			g_hash_table_insert(ml->uid_rowmap, id_uid(uid), GINT_TO_POINTER (row));
			row++;
		}
	}

	/* now, check the rowmap, some rows might've changed (with removes) */
	if (changes->uid_removed->len) {
		d(printf("checking uid mappings\n"));
		row = 0;
		node = e_tree_model_node_get_first_child ((ETreeModel *)ml->table_model, ml->tree_root);
		while (node) {
			uid = e_tree_model_node_get_data((ETreeModel *)ml->table_model, node);
			if (id_is_uid(uid)) {
				if (g_hash_table_lookup_extended(ml->uid_rowmap, id_uid(uid), (void *)&olduid, (void *)&oldrow)) {
					if (oldrow != row) {
						d(printf("row %d moved to new row %d\n", oldrow, row));
						g_hash_table_insert(ml->uid_rowmap, olduid, (void *)row);
					}
				} else { /* missing?  shouldn't happen */
					g_warning("Uid vanished from rowmap?: %s\n", uid);
				}
			}
			row++;
			node = e_tree_model_node_get_next((ETreeModel *)ml->table_model, node);
		}
	}

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Inserting changes took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static void
main_folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	MessageList *ml = MESSAGE_LIST (user_data);
	CamelFolderChangeInfo *changes = (CamelFolderChangeInfo *)event_data;

	printf("folder changed event, changes = %p\n", changes);

	mail_do_regenerate_messagelist(ml, ml->search, changes);
}

static void
folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	/* similarly to message_changed, copy the change list and propagate it to
	   the main thread and free it */
	CamelFolderChangeInfo *changes;

	if (event_data) {
		changes = camel_folder_change_info_new();
		camel_folder_change_info_cat(changes, (CamelFolderChangeInfo *)event_data);
	} else {
		changes = NULL;
	}
	mail_op_forward_event (main_folder_changed, o, changes, user_data);
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

	if (message_list->folder == camel_folder)
		return;

	camel_exception_init (&ex);

	if (message_list->folder)
		camel_object_unref (CAMEL_OBJECT (message_list->folder));

	message_list->folder = camel_folder;

	/* build the etable suitable for this folder */
	message_list_setup_etable(message_list);

	camel_object_hook_event(CAMEL_OBJECT (camel_folder), "folder_changed",
			   folder_changed, message_list);
	camel_object_hook_event(CAMEL_OBJECT (camel_folder), "message_changed",
			   message_changed, message_list);

	camel_object_ref (CAMEL_OBJECT (camel_folder));

	clear_tree(message_list);
	mail_do_regenerate_messagelist (message_list, message_list->search, NULL);
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

	printf("emitting cursor changed signal, for uid %s\n", message_list->cursor_uid);
	gtk_signal_emit(GTK_OBJECT (message_list), message_list_signals [MESSAGE_SELECTED], message_list->cursor_uid);

	message_list->idle_id = 0;
	return FALSE;
}

static void
on_cursor_change_cmd (ETableScrolled *table, int row, gpointer user_data)
{
	MessageList *message_list;
	
	message_list = MESSAGE_LIST (user_data);
	
	message_list->cursor_row = row;
	message_list->cursor_uid = get_message_uid (message_list, row);

	if (!message_list->idle_id) {
		message_list->idle_id =
			g_idle_add_full (G_PRIORITY_LOW, on_cursor_change_idle,
					 message_list, NULL);
	}
}

static gint
on_click (ETableScrolled *table, gint row, gint col, GdkEvent *event, MessageList *list)
{
	int flag;
	const CamelMessageInfo *info;

	if (col == COL_MESSAGE_STATUS)
		flag = CAMEL_MESSAGE_SEEN;
	else if (col == COL_FLAGGED)
		flag = CAMEL_MESSAGE_FLAGGED;
	else
		return FALSE;

	mail_tool_camel_lock_up();

	info = get_message_info(list, row);
	if (info == NULL) {
		mail_tool_camel_lock_down();
		return FALSE;
	}
	
	camel_folder_set_message_flags(list->folder, info->uid, flag, ~info->flags);

	mail_tool_camel_lock_down();

	if (flag == CAMEL_MESSAGE_SEEN && list->seen_id) {
		gtk_timeout_remove (list->seen_id);
		list->seen_id = 0;
	}

	return TRUE;
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

/* set whether we are in threaded view or flat view */
void
message_list_set_threaded(MessageList *ml, gboolean threaded)
{
	if (ml->threaded ^ threaded) {
		ml->threaded = threaded;

		clear_tree(ml);
		mail_do_regenerate_messagelist(ml, ml->search, NULL);
	}
}

void
message_list_set_search(MessageList *ml, const char *search)
{
	if (search == NULL || search[0] == '\0')
		if (ml->search == NULL || ml->search[0]=='\0')
			return;

	if (search != NULL && ml->search !=NULL && strcmp(search, ml->search)==0)
		return;

	clear_tree(ml);
	mail_do_regenerate_messagelist(ml, search, NULL);
}

/* ** REGENERATE MESSAGELIST ********************************************** */

typedef struct regenerate_messagelist_input_s {
	MessageList *ml;
	CamelFolder *folder;
	char *search;
	CamelFolderChangeInfo *changes;
	gboolean dotree;	/* we are building a tree */
} regenerate_messagelist_input_t;

typedef struct regenerate_messagelist_data_s {
	GPtrArray *uids;
	struct _thread_messages *tree;
	CamelFolderChangeInfo *changes;
} regenerate_messagelist_data_t;

static gchar *describe_regenerate_messagelist (gpointer in_data, gboolean gerund);
static void setup_regenerate_messagelist   (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_regenerate_messagelist      (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex);

static gchar *describe_regenerate_messagelist (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup (_("Rebuilding message view"));
	else
		return g_strdup (_("Rebuild message view"));
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
}

static void do_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;
	regenerate_messagelist_data_t *data = (regenerate_messagelist_data_t *) op_data;

	mail_tool_camel_lock_up();

	if (input->search) {
		data->uids = camel_folder_search_by_expression(input->ml->folder, input->search, ex);
	} else {
		data->uids = camel_folder_get_uids (input->ml->folder);
	}

	if (camel_exception_is_set (ex)) {
		mail_tool_camel_lock_down();
		return;
	}

	if (input->dotree && data->uids)
		data->tree = thread_messages(input->ml->folder, data->uids);
	else
		data->tree = NULL;

	mail_tool_camel_lock_down();
}

static void cleanup_regenerate_messagelist (gpointer in_data, gpointer op_data, CamelException *ex)
{
	regenerate_messagelist_input_t *input = (regenerate_messagelist_input_t *) in_data;
	regenerate_messagelist_data_t *data = (regenerate_messagelist_data_t *) op_data;

	ETreeModel *etm;

	etm = E_TREE_MODEL (input->ml->table_model);

	if (data->uids == NULL) { /*exception*/
		gtk_object_unref (GTK_OBJECT (input->ml));
		return;
	}

	if (input->dotree)
		build_tree(input->ml, data->tree);
	else
		build_flat(input->ml, data->uids, input->changes);

	if (input->search)
		camel_folder_search_free (input->ml->folder, data->uids);
	else
		camel_folder_free_uids (input->ml->folder, data->uids);

	/* update what we have as our search string */
        if (input->ml->search && input->ml->search != input->search)
                g_free(input->ml->search);
	input->ml->search = input->search;

	if (data->tree)
		thread_messages_free(data->tree);

	if (input->changes)
		camel_folder_change_info_free(input->changes);

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

/* if changes == NULL, then update the whole list, otherwise just update the changes */
static void
mail_do_regenerate_messagelist (MessageList *list, const gchar *search, CamelFolderChangeInfo *changes)
{
	regenerate_messagelist_input_t *input;

	/* This gets called on empty folder-browsers by the bonobo ui
	 * callback for threaded view.
	 */
	if (!list->folder)
		return;

	/* see if we need to goto the child thread at all anyway */
	/* currently the only case is the flat view with updates and no search */
	if (search == NULL && changes != NULL && !list->threaded) {
		build_flat_diff(list, changes);
		camel_folder_change_info_free(changes);
		return;
	}

	input = g_new (regenerate_messagelist_input_t, 1);
	input->ml = list;
	input->search = g_strdup (search);
	input->changes = changes;
	input->dotree = list->threaded;

	mail_operation_queue (&op_regenerate_messagelist, input, TRUE);
}
