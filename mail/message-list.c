/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Miguel de Icaza (miguel@ximian.com)
 *           Bertrand Guiheneuf (bg@aful.org)
 *           And just about everyone else in evolution ...
 *
 *  Copyright 2000-2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>
#include <ctype.h>

#include <glib/gunicode.h>

#include <gconf/gconf-client.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkinvisible.h>
#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-cell-tree.h>
#include <gal/e-table/e-cell-date.h>
#include <gal/e-table/e-cell-size.h>
#include <gal/e-table/e-tree-memory.h>
#include <gal/e-table/e-tree-memory-callbacks.h>

#include <camel/camel-exception.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-folder.h>
#include <camel/camel-folder-thread.h>
#include <camel/camel-vee-folder.h>
#include <libedataserver/e-memory.h>

#include "filter/filter-label.h"

#include "mail-config.h"
#include "message-list.h"
#include "mail-mt.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "em-popup.h"

#include "em-utils.h"
#include <e-util/e-icon-factory.h>

#include "art/empty.xpm"

/*#define TIMEIT */

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

#define d(x)
#define t(x)

struct _MLSelection {
	GPtrArray *uids;
	CamelFolder *folder;
	char *folder_uri;
};

struct _MessageListPrivate {
	GtkWidget *invisible;	/* 4 selection */

	struct _MLSelection clipboard;
};

static struct {
	char *target;
	GdkAtom atom;
	guint32 actions;
} ml_drag_info[] = {
	{ "x-uid-list", 0, GDK_ACTION_ASK|GDK_ACTION_MOVE|GDK_ACTION_COPY },
	{ "message/rfc822", 0, GDK_ACTION_COPY },
	{ "text/uri-list", 0, GDK_ACTION_COPY },
};

enum {
	DND_X_UID_LIST,		/* x-uid-list */
	DND_MESSAGE_RFC822,	/* message/rfc822 */
	DND_TEXT_URI_LIST,	/* text/uri-list */
};

/* What we send */
static GtkTargetEntry ml_drag_types[] = {
	{ "x-uid-list", 0, DND_X_UID_LIST },
	{ "text/uri-list", 0, DND_TEXT_URI_LIST },
};

/* What we accept */
static GtkTargetEntry ml_drop_types[] = {
	{ "x-uid-list", 0, DND_X_UID_LIST },
	{ "message/rfc822", 0, DND_MESSAGE_RFC822 },
	{ "text/uri-list", 0, DND_TEXT_URI_LIST },
};

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

enum {
	NORMALISED_SUBJECT,
	NORMALISED_FROM,
	NORMALISED_TO,
	NORMALISED_LAST,
};

/* #define SMART_ADDRESS_COMPARE */

#ifdef SMART_ADDRESS_COMPARE
struct _EMailAddress {
	ENameWestern *wname;
	char *address;
};

typedef struct _EMailAddress EMailAddress;
#endif /* SMART_ADDRESS_COMPARE */

G_DEFINE_TYPE (MessageList, message_list, E_TREE_SCROLLED_TYPE);

static void on_cursor_activated_cmd (ETree *tree, int row, ETreePath path, gpointer user_data);
static void on_selection_changed_cmd(ETree *tree, MessageList *ml);
static gint on_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, MessageList *list);
static char *filter_date (time_t date);
static char *filter_size (int size);

static void folder_changed (CamelObject *o, gpointer event_data, gpointer user_data);

static void save_hide_state(MessageList *ml);
static void load_hide_state(MessageList *ml);

/* note: @changes is owned/freed by the caller */
/*static void mail_do_regenerate_messagelist (MessageList *list, const char *search, const char *hideexpr, CamelFolderChangeInfo *changes);*/
static void mail_regen_list(MessageList *ml, const char *search, const char *hideexpr, CamelFolderChangeInfo *changes);
static void mail_regen_cancel(MessageList *ml);

static void clear_info(char *key, ETreePath *node, MessageList *ml);

enum {
	MESSAGE_SELECTED,
	MESSAGE_LIST_BUILT,
	MESSAGE_LIST_SCROLLED,
	LAST_SIGNAL
};

static guint message_list_signals [LAST_SIGNAL] = {0, };

static struct {
	char *icon_name;
	GdkPixbuf  *pixbuf;
} states_pixmaps[] = {
	{ "stock_mail-unread",                 NULL },
	{ "stock_mail-open",                   NULL },
	{ "stock_mail-replied",                NULL },
	{ "stock_mail-unread-multiple",        NULL },
	{ "stock_mail-open-multiple",          NULL },
	{ NULL,                                NULL },
	{ "stock_attach",                      NULL },
	{ "stock_mail-priority-high",          NULL },
	{ "stock_score-lowest",                NULL },
	{ "stock_score-lower",                 NULL },
	{ "stock_score-low",                   NULL },
	{ "stock_score-normal",                NULL },
	{ "stock_score-high",                  NULL },
	{ "stock_score-higher",                NULL },
	{ "stock_score-highest",               NULL },
	{ "stock_mail-flag-for-followup",      NULL },
	{ "stock_mail-flag-for-followup-done", NULL },
};

/* FIXME: junk prefs */
static gboolean junk_folder = TRUE;

#ifdef SMART_ADDRESS_COMPARE
static EMailAddress *
e_mail_address_new (const char *address)
{
	CamelInternetAddress *cia;
	EMailAddress *new;
	const char *name = NULL, *addr = NULL;
	
	cia = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (cia), address) == -1) {
		camel_object_unref (cia);
		return NULL;
	}
	camel_internet_address_get (cia, 0, &name, &addr);
	
	new = g_new (EMailAddress, 1);
	new->address = g_strdup (addr);
	if (name && *name) {
		new->wname = e_name_western_parse (name);
	} else {
		new->wname = NULL;
	}
	
	camel_object_unref (cia);
	
	return new;
}

static void
e_mail_address_free (EMailAddress *addr)
{
	g_return_if_fail (addr != NULL);
	
	g_free (addr->address);
	if (addr->wname)
		e_name_western_free (addr->wname);
	g_free (addr);
}

/* Note: by the time this function is called, the strings have already
   been normalised which means we can assume all lowercase chars and
   we can just use strcmp for the final comparison. */
static int
e_mail_address_compare (gconstpointer address1, gconstpointer address2)
{
	const EMailAddress *addr1 = address1;
	const EMailAddress *addr2 = address2;
	int retval;
	
	g_return_val_if_fail (addr1 != NULL, 1);
	g_return_val_if_fail (addr2 != NULL, -1);
	
	if (!addr1->wname && !addr2->wname) {
		/* have to compare addresses, one or both don't have names */
		g_return_val_if_fail (addr1->address != NULL, 1);
		g_return_val_if_fail (addr2->address != NULL, -1);
		
		return strcmp (addr1->address, addr2->address);
	}
	
	if (!addr1->wname)
		return -1;
	if (!addr2->wname)
		return 1;
	
	if (!addr1->wname->last && !addr2->wname->last) {
		/* neither has a last name - default to address? */
		/* FIXME: what do we compare next? */
		g_return_val_if_fail (addr1->address != NULL, 1);
		g_return_val_if_fail (addr2->address != NULL, -1);
		
		return strcmp (addr1->address, addr2->address);
	}
	
	if (!addr1->wname->last)
		return -1;
	if (!addr2->wname->last)
		return 1;
	
	retval = strcmp (addr1->wname->last, addr2->wname->last);
	if (retval)
		return retval;
	
	/* last names are identical - compare first names */
	
	if (!addr1->wname->first && !addr2->wname->first)
		return strcmp (addr1->address, addr2->address);
	
	if (!addr1->wname->first)
		return -1;
	if (!addr2->wname->first)
		return 1;
	
	retval = strcmp (addr1->wname->first, addr2->wname->first);
	if (retval)
		return retval;
	
	return strcmp (addr1->address, addr2->address);
}
#endif /* SMART_ADDRESS_COMPARE */

/* Note: by the time this function is called, the strings have already
   been normalised which means we can assume all lowercase chars and
   we can just use strcmp for the final comparison. */
static int
address_compare (gconstpointer address1, gconstpointer address2)
{
#ifdef SMART_ADDRESS_COMPARE
	EMailAddress *addr1, *addr2;
#endif /* SMART_ADDRESS_COMPARE */
	int retval;
	
	g_return_val_if_fail (address1 != NULL, 1);
	g_return_val_if_fail (address2 != NULL, -1);
	
#ifdef SMART_ADDRESS_COMPARE
	addr1 = e_mail_address_new (address1);
	addr2 = e_mail_address_new (address2);
	retval = e_mail_address_compare (addr1, addr2);
	e_mail_address_free (addr1);
	e_mail_address_free (addr2);
#else
	retval = strcmp ((const char *) address1, (const char *) address2);
#endif /* SMART_ADDRESS_COMPARE */
	
	return retval;
}

static char *
filter_size (gint size)
{
	gfloat fsize;
	
	if (size < 1024) {
		return g_strdup_printf ("%d", size);
	} else {
		fsize = ((gfloat) size) / 1024.0;
		if (fsize < 1024.0) {
			return g_strdup_printf ("%.2f K", fsize);
		} else {
			fsize /= 1024.0;
			return g_strdup_printf ("%.2f M", fsize);
		}
	}
}

/* Gets the uid of the message displayed at a given view row */
static const char *
get_message_uid (MessageList *message_list, ETreePath node)
{
	CamelMessageInfo *info;
	
	g_assert (node != NULL);
	info = e_tree_memory_node_get_data (E_TREE_MEMORY (message_list->model), node);
	/* correct me if I'm wrong, but this should never be NULL, should it? */
	g_assert (info != NULL);
	
	return camel_message_info_uid (info);
}

/* Gets the CamelMessageInfo for the message displayed at the given
 * view row.
 */
static CamelMessageInfo *
get_message_info (MessageList *message_list, ETreePath node)
{
	CamelMessageInfo *info;
	
	g_assert (node != NULL);
	info = e_tree_memory_node_get_data (E_TREE_MEMORY (message_list->model), node);
	g_assert (info != NULL);
	
	return info;
}

static const char *
get_normalised_string (MessageList *message_list, CamelMessageInfo *info, int col)
{
	const char *string, *str;
	char *normalised;
	EPoolv *poolv;
	int index;
	
	switch (col) {
	case COL_SUBJECT_NORM:
		string = camel_message_info_subject (info);
		index = NORMALISED_SUBJECT;
		break;
	case COL_FROM_NORM:
		string = camel_message_info_from (info);
		index = NORMALISED_FROM;
		break;
	case COL_TO_NORM:
		string = camel_message_info_to (info);
		index = NORMALISED_TO;
		break;
	default:
		string = NULL;
		index = NORMALISED_LAST;
		g_assert_not_reached ();
	}
	
	/* slight optimisation */
	if (string == NULL || string[0] == '\0')
		return "";
	
	poolv = g_hash_table_lookup (message_list->normalised_hash, camel_message_info_uid (info));
	if (poolv == NULL) {
		poolv = e_poolv_new (NORMALISED_LAST);
		g_hash_table_insert (message_list->normalised_hash, (char *) camel_message_info_uid (info), poolv);
	} else {
		str = e_poolv_get (poolv, index);
		if (*str)
			return str;
	}
	
	if (col == COL_SUBJECT_NORM) {
		const unsigned char *subject;
		
		subject = (const unsigned char *) string;
		while (!g_ascii_strncasecmp (subject, "Re:", 3)) {
			subject += 3;
			
			/* jump over any spaces */
			while (*subject && isspace ((int) *subject))
				subject++;
		}
		
		/* jump over any spaces */
		while (*subject && isspace ((int) *subject))
			subject++;
		
		string = (const char *) subject;
	}
	
	normalised = g_utf8_collate_key (string, -1);
	e_poolv_set (poolv, index, normalised, TRUE);
	
	return e_poolv_get (poolv, index);
}

static void
clear_selection(MessageList *ml, struct _MLSelection *selection)
{
	if (selection->uids) {
		message_list_free_uids(ml, selection->uids);
		selection->uids = NULL;
	}
	if (selection->folder) {
		camel_object_unref(selection->folder);
		selection->folder = NULL;
	}
	g_free(selection->folder_uri);
	selection->folder_uri = NULL;
}

static ETreePath
ml_search_forward(MessageList *ml, int start, int end, guint32 flags, guint32 mask)
{
	ETreePath path;
	int row;
	CamelMessageInfo *info;
	ETreeTableAdapter *etta = e_tree_get_table_adapter(ml->tree);

	for (row = start; row <= end; row ++) {
		path = e_tree_table_adapter_node_at_row(etta, row);
		if (path
		    && (info = get_message_info(ml, path))
		    && (camel_message_info_flags(info) & mask) == flags)
			return path;
	}

	return NULL;
}

static ETreePath
ml_search_backward(MessageList *ml, int start, int end, guint32 flags, guint32 mask)
{
	ETreePath path;
	int row;
	CamelMessageInfo *info;
	ETreeTableAdapter *etta = e_tree_get_table_adapter(ml->tree);

	for (row = start; row >= end; row --) {
		path = e_tree_table_adapter_node_at_row(etta, row);
		if (path
		    && (info = get_message_info(ml, path))
		    && (camel_message_info_flags(info) & mask) == flags)
			return path;
	}

	return NULL;
}

static ETreePath
ml_search_path(MessageList *ml, MessageListSelectDirection direction, guint32 flags, guint32 mask)
{
	ETreePath node;
	int row, count;
	ETreeTableAdapter *etta = e_tree_get_table_adapter(ml->tree);

	if (ml->cursor_uid == NULL
	    || (node = g_hash_table_lookup(ml->uid_nodemap, ml->cursor_uid)) == NULL)
		return NULL;

	row = e_tree_table_adapter_row_of_node(etta, node);
	if (row == -1)
		return NULL;
	count = e_table_model_row_count((ETableModel *)etta);

	if ((direction & MESSAGE_LIST_SELECT_DIRECTION) == MESSAGE_LIST_SELECT_NEXT)
		node = ml_search_forward(ml, row + 1, count - 1, flags, mask);
	else
		node = ml_search_backward(ml, row-1, 0, flags, mask);

	if (node == NULL && (direction & MESSAGE_LIST_SELECT_WRAP)) {
		if ((direction & MESSAGE_LIST_SELECT_DIRECTION) == MESSAGE_LIST_SELECT_NEXT)
			node = ml_search_forward(ml, 0, row, flags, mask);
		else
			node = ml_search_backward(ml, count-1, row, flags, mask);
	}

	return node;
}

static void
select_path(MessageList *ml, ETreePath path)
{
	ETreeSelectionModel *etsm = (ETreeSelectionModel *)e_tree_get_selection_model(ml->tree);

	g_free(ml->cursor_uid);
	ml->cursor_uid = NULL;

	e_tree_table_adapter_show_node(e_tree_get_table_adapter(ml->tree), path);
	e_tree_set_cursor(ml->tree, path);
	e_tree_selection_model_select_single_path(etsm, path);
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
 * previous. %MESSAGE_LIST_SELECT_WRAP is an option bit which specifies the
 * search should wrap.
 *
 * If no suitable row is found, the selection will be
 * unchanged.
 *
 * Returns %TRUE if a new message has been selected or %FALSE otherwise.
 **/
gboolean
message_list_select(MessageList *ml, MessageListSelectDirection direction, guint32 flags, guint32 mask)
{
	ETreePath path;

	path = ml_search_path(ml, direction, flags, mask);
	if (path) {
		select_path(ml, path);
		return TRUE;
	} else
		return FALSE;
}

/**
 * message_list_can_select:
 * @ml: 
 * @direction: 
 * @flags: 
 * @mask: 
 * 
 * Returns true if the selection specified is possible with the current view.
 * 
 * Return value: 
 **/
gboolean
message_list_can_select(MessageList *ml, MessageListSelectDirection direction, guint32 flags, guint32 mask)
{
	return ml_search_path(ml, direction, flags, mask) != NULL;
}

/**
 * message_list_select_uid:
 * @message_list:
 * @uid:
 *
 * Selects the message with the given UID.
 **/
void
message_list_select_uid (MessageList *message_list, const char *uid)
{
	ETreePath node;

	if (message_list->folder == NULL)
		return;

	if (message_list->regen || message_list->regen_timeout_id) {
		g_free(message_list->pending_select_uid);
		message_list->pending_select_uid = g_strdup(uid);
	}
	
	node = g_hash_table_lookup (message_list->uid_nodemap, uid);
	if (node) {
		CamelMessageInfo *info;

		info = get_message_info (message_list, node);

		/* This will emit a changed signal that we'll pick up */
		e_tree_set_cursor (message_list->tree, node);
	} else {
		g_free (message_list->cursor_uid);
		message_list->cursor_uid = NULL;
		g_signal_emit (GTK_OBJECT (message_list), message_list_signals[MESSAGE_SELECTED], 0, NULL);
	}
}

void
message_list_select_next_thread (MessageList *ml)
{
	ETreePath node;
	ETreeTableAdapter *etta = e_tree_get_table_adapter(ml->tree);
	int i, count, row;

	if (!ml->cursor_uid
	    || (node = g_hash_table_lookup(ml->uid_nodemap, ml->cursor_uid)) == NULL)
		return;

	row = e_tree_table_adapter_row_of_node(etta, node);
	if (row == -1)
		return;
	count = e_table_model_row_count((ETableModel *)etta);

	/* find the next node which has a root parent (i.e. toplevel node) */
	for (i=row+1;i<count-1;i++) {
		node = e_tree_table_adapter_node_at_row(etta, i);
		if (node
		    && e_tree_model_node_is_root(ml->model, e_tree_model_node_get_parent(ml->model, node))) {
			select_path(ml, node);
			return;
		}
	}
}

/**
 * message_list_select_all:
 * @message_list: Message List widget
 *
 * Selects all messages in the message list.
 **/
void
message_list_select_all (MessageList *message_list)
{
	ESelectionModel *etsm;
	
	etsm = e_tree_get_selection_model (message_list->tree);
	
	e_selection_model_select_all (etsm);
}


typedef struct thread_select_info {
	MessageList *ml;
	GPtrArray *paths;
} thread_select_info_t;

static gboolean
select_node (ETreeModel *model, ETreePath path, gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	
	g_ptr_array_add (tsi->paths, path);
	return FALSE; /*not done yet*/
}

static void
thread_select_foreach (ETreePath path, gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	ETreeModel *model = tsi->ml->model;
	ETreePath node;
	
	/* @path part of the initial selection. If it has children,
	 * we select them as well. If it doesn't, we select its siblings and
	 * their children (ie, the current node must be inside the thread
	 * that the user wants to mark.
	 */
	
	if (e_tree_model_node_get_first_child (model, path)) {
		node = path;
	} else {
		node = e_tree_model_node_get_parent (model, path);
		
		/* Let's make an exception: if no parent, then we're about
		 * to mark the whole tree. No. */
		if (e_tree_model_node_is_root (model, node)) 
			node = path;
	}
	
	e_tree_model_node_traverse (model, node, select_node, tsi);
}

/**
 * message_list_select_thread:
 * @message_list: Message List widget
 *
 * Selects all messages in the current thread (based on cursor).
 **/
void
message_list_select_thread (MessageList *message_list)
{
	ETreeSelectionModel *etsm;
	thread_select_info_t tsi;
	int i;
	
	tsi.ml = message_list;
	tsi.paths = g_ptr_array_new ();
	
	etsm = (ETreeSelectionModel *) e_tree_get_selection_model (message_list->tree);
	
	e_tree_selected_path_foreach (message_list->tree, thread_select_foreach, &tsi);
	
	for (i = 0; i < tsi.paths->len; i++)
		e_tree_selection_model_add_to_selection (etsm, tsi.paths->pdata[i]);
	
	g_ptr_array_free (tsi.paths, TRUE);
}


/**
 * message_list_invert_selection:
 * @message_list: Message List widget
 *
 * Invert the current selection in the message-list.
 **/
void
message_list_invert_selection (MessageList *message_list)
{
	ESelectionModel *etsm;
	
	etsm = e_tree_get_selection_model (message_list->tree);
	
	e_selection_model_invert_selection (etsm);
}

void
message_list_copy(MessageList *ml, gboolean cut)
{
	struct _MessageListPrivate *p = ml->priv;
	GPtrArray *uids;

	clear_selection(ml, &p->clipboard);
	
	uids = message_list_get_selected(ml);

	if (uids->len > 0) {
		if (cut) {
			int i;

			camel_folder_freeze(ml->folder);
			for (i=0;i<uids->len;i++)
				camel_folder_set_message_flags(ml->folder, uids->pdata[i],
							       CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED,
							       CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED);

			camel_folder_thaw(ml->folder);
		}

		p->clipboard.uids = uids;
		p->clipboard.folder = ml->folder;
		camel_object_ref(p->clipboard.folder);
		p->clipboard.folder_uri = g_strdup(ml->folder_uri);
		gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
	} else {
		message_list_free_uids(ml, uids);
		gtk_selection_owner_set(NULL, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
	}
}

void
message_list_paste(MessageList *ml)
{
	gtk_selection_convert(ml->priv->invisible, GDK_SELECTION_CLIPBOARD,
			      gdk_atom_intern ("x-uid-list", FALSE),
			      GDK_CURRENT_TIME);
}

/*
 * SimpleTableModel::col_count
 */
static int
ml_column_count (ETreeModel *etm, void *data)
{
	return COL_LAST;
}

/*
 * SimpleTableModel::has_save_id
 */
static gboolean
ml_has_save_id (ETreeModel *etm, void *data)
{
	return TRUE;
}

/*
 * SimpleTableModel::get_save_id
 */
static char *
ml_get_save_id (ETreeModel *etm, ETreePath path, void *data)
{
	CamelMessageInfo *info;

	if (e_tree_model_node_is_root(etm, path))
		return g_strdup("root");

	/* Note: etable can ask for the save_id while we're clearing it,
	   which is the only time data should be null */
	info = e_tree_memory_node_get_data (E_TREE_MEMORY(etm), path);
	if (info == NULL)
		return NULL;

	return g_strdup (camel_message_info_uid(info));
}

/*
 * SimpleTableModel::has_save_id
 */
static gboolean
ml_has_get_node_by_id (ETreeModel *etm, void *data)
{
	return TRUE;
}

/*
 * SimpleTableModel::get_save_id
 */
static ETreePath
ml_get_node_by_id (ETreeModel *etm, const char *save_id, void *data)
{
	MessageList *ml;

	ml = data;

	if (!strcmp (save_id, "root"))
		return e_tree_model_get_root (etm);

	return g_hash_table_lookup(ml->uid_nodemap, save_id);
}

static void *
ml_duplicate_value (ETreeModel *etm, int col, const void *value, void *data)
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
	case COL_SIZE:
	case COL_FOLLOWUP_FLAG_STATUS:
	case COL_FOLLOWUP_DUE_BY:
		return (void *) value;
		
	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
		return g_strdup (value);
		
	default:
		g_assert_not_reached ();
	}
	return NULL;
}

static void
ml_free_value (ETreeModel *etm, int col, void *value, void *data)
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
	case COL_SIZE:
	case COL_FOLLOWUP_FLAG_STATUS:
	case COL_FOLLOWUP_DUE_BY:
		break;
		
	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
		g_free (value);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void *
ml_initialize_value (ETreeModel *etm, int col, void *data)
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
	case COL_SIZE:
	case COL_FOLLOWUP_FLAG_STATUS:
	case COL_FOLLOWUP_DUE_BY:
		return NULL;
		
	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
		return g_strdup ("");
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static gboolean
ml_value_is_empty (ETreeModel *etm, int col, const void *value, void *data)
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
	case COL_SIZE:
	case COL_FOLLOWUP_FLAG_STATUS:
	case COL_FOLLOWUP_DUE_BY:
		return value == NULL;
		
	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
		return !(value && *(char *)value);
	default:
		g_assert_not_reached ();
		return FALSE;
	}
}

static const char *status_map[] = {
	N_("Unseen"),
	N_("Seen"),
	N_("Answered"),
	N_("Multiple Unseen Messages"),
	N_("Multiple Messages"),
};

static const char *score_map[] = {
	N_("Lowest"),
	N_("Lower"),
	N_("Low"),
	N_("Normal"),
	N_("High"),
	N_("Higher"),
	N_("Highest"),
};


static char *
ml_value_to_string (ETreeModel *etm, int col, const void *value, void *data)
{
	unsigned int i;
	
	switch (col){
	case COL_MESSAGE_STATUS:
		i = GPOINTER_TO_UINT(value);
		if (i > 4)
			return g_strdup ("");
		return g_strdup (_(status_map[i]));
		
	case COL_SCORE:
		i = GPOINTER_TO_UINT(value) + 3;
		if (i > 6)
			i = 3;
		return g_strdup (_(score_map[i]));
		
	case COL_ATTACHMENT:
	case COL_FLAGGED:
	case COL_DELETED:
	case COL_UNREAD:
	case COL_FOLLOWUP_FLAG_STATUS:
		return g_strdup_printf ("%d", GPOINTER_TO_UINT(value));
		
	case COL_SENT:
	case COL_RECEIVED:
	case COL_FOLLOWUP_DUE_BY:
		return filter_date (GPOINTER_TO_INT (value));
		
	case COL_SIZE:
		return filter_size (GPOINTER_TO_INT (value));
		
	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
		return g_strdup (value);
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static GdkPixbuf *
ml_tree_icon_at (ETreeModel *etm, ETreePath path, void *model_data)
{
	/* we dont really need an icon ... */
	return NULL;
}

/* return true if there are any unread messages in the subtree */
static int
subtree_unread(MessageList *ml, ETreePath node)
{
	CamelMessageInfo *info;
	ETreePath child;
	
	while (node) {
		info = e_tree_memory_node_get_data((ETreeMemory *)ml->model, node);
		g_assert(info);
		
		if (!(camel_message_info_flags(info) & CAMEL_MESSAGE_SEEN))
			return TRUE;
		
		if ((child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->model), node)))
			if (subtree_unread(ml, child))
				return TRUE;
		node = e_tree_model_node_get_next (ml->model, node);
	}
	return FALSE;
}

static int
subtree_size(MessageList *ml, ETreePath node)
{
	CamelMessageInfo *info;
	int size = 0;
	ETreePath child;
	
	while (node) {
		info = e_tree_memory_node_get_data((ETreeMemory *)ml->model, node);
		g_assert(info);
		
		size += camel_message_info_size(info);
		if ((child = e_tree_model_node_get_first_child (E_TREE_MODEL (ml->model), node)))
			size += subtree_size(ml, child);
		
		node = e_tree_model_node_get_next (ml->model, node);
	}
	return size;
}

static time_t
subtree_earliest(MessageList *ml, ETreePath node, int sent)
{
	CamelMessageInfo *info;
	time_t earliest = 0, date;
	ETreePath *child;
	
	while (node) {
		info = e_tree_memory_node_get_data((ETreeMemory *)ml->model, node);
		g_assert(info);
		
		if (sent)
			date = camel_message_info_date_sent(info);
		else
			date = camel_message_info_date_received(info);
		
		if (earliest == 0 || date < earliest)
			earliest = date;
		
		if ((child = e_tree_model_node_get_first_child (ml->model, node))) {
			date = subtree_earliest(ml, child, sent);
			if (earliest == 0 || (date != 0 && date < earliest))
				earliest = date;
		}
		
		node = e_tree_model_node_get_next (ml->model, node);
	}
	
	return earliest;
}

static void *
ml_tree_value_at (ETreeModel *etm, ETreePath path, int col, void *model_data)
{
	MessageList *message_list = model_data;
	CamelMessageInfo *msg_info;
	const char *str;
	guint32 flags;

	if (e_tree_model_node_is_root (etm, path))
		return NULL;
	
	/* retrieve the message information array */
	msg_info = e_tree_memory_node_get_data (E_TREE_MEMORY(etm), path);
	g_assert (msg_info != NULL);
	
	switch (col){
	case COL_MESSAGE_STATUS:
		flags = camel_message_info_flags(msg_info);
		if (flags & CAMEL_MESSAGE_ANSWERED)
			return GINT_TO_POINTER (2);
		else if (flags & CAMEL_MESSAGE_SEEN)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
		break;
	case COL_FLAGGED:
		return GINT_TO_POINTER ((camel_message_info_flags(msg_info) & CAMEL_MESSAGE_FLAGGED) != 0);
	case COL_SCORE: {
		const char *tag;
		int score = 0;
		
		tag = camel_message_info_user_tag(msg_info, "score");
		if (tag)
			score = atoi (tag);
		
		return GINT_TO_POINTER (score);
	}
	case COL_FOLLOWUP_FLAG_STATUS: {
		const char *tag, *cmp;
		
		/* FIXME: this all should be methods off of message-tag-followup class,
		   FIXME: the tag names should be namespaced :( */
		tag = camel_message_info_user_tag(msg_info, "follow-up");
		cmp = camel_message_info_user_tag(msg_info, "completed-on");
		if (tag && tag[0]) {
			if (cmp && cmp[0])
				return GINT_TO_POINTER(2);
			else
				return GINT_TO_POINTER(1);
		} else
			return GINT_TO_POINTER(0);
	}
	case COL_FOLLOWUP_DUE_BY: {
		const char *tag;
		time_t due_by;
		
		tag = camel_message_info_user_tag(msg_info, "due-by");
		if (tag && *tag) {
			due_by = camel_header_decode_date (tag, NULL);
			return GINT_TO_POINTER (due_by);
		} else {
			return GINT_TO_POINTER (0);
		}
	}
	case COL_FOLLOWUP_FLAG:
		str = camel_message_info_user_tag(msg_info, "follow-up");
		return (void *)(str ? str : "");
	case COL_ATTACHMENT:
		return GINT_TO_POINTER ((camel_message_info_flags(msg_info) & CAMEL_MESSAGE_ATTACHMENTS) != 0);
	case COL_FROM:
		str = camel_message_info_from (msg_info);
		return (void *)(str ? str : "");
	case COL_FROM_NORM:
		return (void *) get_normalised_string (message_list, msg_info, col);
	case COL_SUBJECT:
		str = camel_message_info_subject (msg_info);
		return (void *)(str ? str : "");
	case COL_SUBJECT_NORM:
		return (void *) get_normalised_string (message_list, msg_info, col);
	case COL_SENT:
		return GINT_TO_POINTER (camel_message_info_date_sent(msg_info));
	case COL_RECEIVED:
		return GINT_TO_POINTER (camel_message_info_date_received(msg_info));
	case COL_TO:
		str = camel_message_info_to (msg_info);
		return (void *)(str ? str : "");
	case COL_TO_NORM:
		return (void *) get_normalised_string (message_list, msg_info, col);
	case COL_SIZE:
		return GINT_TO_POINTER (camel_message_info_size(msg_info));
	case COL_DELETED:
		return GINT_TO_POINTER ((camel_message_info_flags(msg_info) & CAMEL_MESSAGE_DELETED) != 0);
	case COL_UNREAD: {
		ETreePath child;
		flags = camel_message_info_flags(msg_info);

		child = e_tree_model_node_get_first_child(etm, path);
		if (child && !e_tree_node_is_expanded(message_list->tree, path)
		    && (flags & CAMEL_MESSAGE_SEEN)) {
			return GINT_TO_POINTER (subtree_unread (message_list, child));
		}
		
		return GINT_TO_POINTER (!(flags & CAMEL_MESSAGE_SEEN));
	}
	case COL_COLOUR: {
		const char *colour, *due_by, *completed, *label;
		
		/* Priority: colour tag; label tag; important flag; due-by tag */

		/* This is astonisngly poorly written code */

		colour = camel_message_info_user_tag(msg_info, "colour");
		due_by = camel_message_info_user_tag(msg_info, "due-by");
		completed = camel_message_info_user_tag(msg_info, "completed-on");
		label = camel_message_info_user_tag(msg_info, "label");
		if (colour == NULL) {
		find_colour:
			if (label != NULL) {
				colour = mail_config_get_label_color_by_name (label);
				if (colour == NULL) {
					/* dead label? */
					label = NULL;
					goto find_colour;
				}
			} else if (camel_message_info_flags(msg_info) & CAMEL_MESSAGE_FLAGGED) {
				/* FIXME: extract from the important.xpm somehow. */
				colour = "#A7453E";
			} else if ((due_by && *due_by) && !(completed && *completed)) {
				time_t now = time (NULL);
				time_t target_date;
				
				target_date = camel_header_decode_date (due_by, NULL);
				if (now >= target_date)
					colour = "#A7453E";
			}
		}
		return (void *) colour;
	}
	case COL_LOCATION: {
		CamelFolder *folder;
		char *name;
		
		if (CAMEL_IS_VEE_FOLDER(message_list->folder)) {
			folder = camel_vee_folder_get_location((CamelVeeFolder *)message_list->folder, (CamelVeeMessageInfo *)msg_info, NULL);
		} else {
			folder = message_list->folder;
		}
		
		camel_object_get(folder, NULL, CAMEL_OBJECT_DESCRIPTION, &name, 0);
		return name;
	}
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static void
ml_tree_set_value_at (ETreeModel *etm, ETreePath path, int col,
		      const void *val, void *model_data)
{
	g_assert_not_reached ();
}

static gboolean
ml_tree_is_cell_editable (ETreeModel *etm, ETreePath path, int col, void *model_data)
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
	if (states_pixmaps[0].pixbuf)
		return;
	
	for (i = 0; i < G_N_ELEMENTS (states_pixmaps); i++) {
		if (states_pixmaps[i].icon_name)
			states_pixmaps[i].pixbuf = e_icon_factory_get_icon (states_pixmaps[i].icon_name, E_ICON_SIZE_MENU);
		else
			states_pixmaps[i].pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) empty_xpm);
	}
}

static char *
filter_date (time_t date)
{
	time_t nowdate = time(NULL);
	time_t yesdate;
	struct tm then, now, yesterday;
	char buf[26];
	gboolean done = FALSE;
	
	if (date == 0)
		return g_strdup (_("?"));
	
	localtime_r (&date, &then);
	localtime_r (&nowdate, &now);
	if (then.tm_mday == now.tm_mday &&
	    then.tm_mon == now.tm_mon &&
	    then.tm_year == now.tm_year) {
		e_utf8_strftime_fix_am_pm (buf, 26, _("Today %l:%M %p"), &then);
		done = TRUE;
	}
	if (!done) {
		yesdate = nowdate - 60 * 60 * 24;
		localtime_r (&yesdate, &yesterday);
		if (then.tm_mday == yesterday.tm_mday &&
		    then.tm_mon == yesterday.tm_mon &&
		    then.tm_year == yesterday.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 26, _("Yesterday %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		int i;
		for (i = 2; i < 7; i++) {
			yesdate = nowdate - 60 * 60 * 24 * i;
			localtime_r (&yesdate, &yesterday);
			if (then.tm_mday == yesterday.tm_mday &&
			    then.tm_mon == yesterday.tm_mon &&
			    then.tm_year == yesterday.tm_year) {
				e_utf8_strftime_fix_am_pm (buf, 26, _("%a %l:%M %p"), &then);
				done = TRUE;
				break;
			}
		}
	}
	if (!done) {
		if (then.tm_year == now.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 26, _("%b %d %l:%M %p"), &then);
		} else {
			e_utf8_strftime_fix_am_pm (buf, 26, _("%b %d %Y"), &then);
		}
	}
#if 0
#ifdef CTIME_R_THREE_ARGS
	ctime_r (&date, buf, 26);
#else
	ctime_r (&date, buf);
#endif
#endif

	return g_strdup (buf);
}

static ETableExtras *
message_list_create_extras (void)
{
	int i;
	GdkPixbuf *images [7];
	ETableExtras *extras;
	ECell *cell;
	
	extras = e_table_extras_new ();
	e_table_extras_add_pixbuf (extras, "status", states_pixmaps [0].pixbuf);
	e_table_extras_add_pixbuf (extras, "score", states_pixmaps [13].pixbuf);
	e_table_extras_add_pixbuf (extras, "attachment", states_pixmaps [6].pixbuf);
	e_table_extras_add_pixbuf (extras, "flagged", states_pixmaps [7].pixbuf);
	e_table_extras_add_pixbuf (extras, "followup", states_pixmaps [15].pixbuf);
	
	e_table_extras_add_compare (extras, "address_compare", address_compare);
	
	for (i = 0; i < 5; i++)
		images [i] = states_pixmaps [i].pixbuf;
	
	e_table_extras_add_cell (extras, "render_message_status", e_cell_toggle_new (0, 5, images));
	
	for (i = 0; i < 2; i++)
		images [i] = states_pixmaps [i + 5].pixbuf;
	
	e_table_extras_add_cell (extras, "render_attachment", e_cell_toggle_new (0, 2, images));
	
	images [1] = states_pixmaps [7].pixbuf;
	e_table_extras_add_cell (extras, "render_flagged", e_cell_toggle_new (0, 2, images));
	
	images[1] = states_pixmaps [15].pixbuf;
	images[2] = states_pixmaps [16].pixbuf;
	e_table_extras_add_cell (extras, "render_flag_status", e_cell_toggle_new (0, 3, images));
	
	for (i = 0; i < 7; i++)
		images[i] = states_pixmaps [i + 7].pixbuf;
	
	e_table_extras_add_cell (extras, "render_score", e_cell_toggle_new (0, 7, images));
	
	/* date cell */
	cell = e_cell_date_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "bold_column", COL_UNREAD,
		      "color_column", COL_COLOUR,
		      NULL);
	e_table_extras_add_cell (extras, "render_date", cell);
	
	/* text cell */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "bold_column", COL_UNREAD,
		      "color_column", COL_COLOUR,
		      NULL);
	e_table_extras_add_cell (extras, "render_text", cell);
	
	e_table_extras_add_cell (extras, "render_tree", 
				 e_cell_tree_new (NULL, NULL, /* let the tree renderer default the pixmaps */
						  TRUE, cell));
	
	/* size cell */
	cell = e_cell_size_new (NULL, GTK_JUSTIFY_RIGHT);
	g_object_set (G_OBJECT (cell),
		      "bold_column", COL_UNREAD,
		      "color_column", COL_COLOUR,
		      NULL);
	e_table_extras_add_cell (extras, "render_size", cell);

	return extras;
}

static void
save_tree_state(MessageList *ml)
{
	char *filename;
	
	if (ml->folder == NULL || ml->tree == NULL)
		return;
	
	filename = mail_config_folder_to_cachename(ml->folder, "et-expanded-");
	e_tree_save_expanded_state(ml->tree, filename);
	g_free(filename);
}

static void
load_tree_state (MessageList *ml)
{
	char *filename;
	
	if (ml->folder == NULL || ml->tree == NULL)
		return;
	
	filename = mail_config_folder_to_cachename (ml->folder, "et-expanded-");
	e_tree_load_expanded_state (ml->tree, filename);
	g_free (filename);
}


void
message_list_save_state (MessageList *ml)
{
	save_tree_state (ml);
	save_hide_state (ml);
}

static void
message_list_setup_etree (MessageList *message_list, gboolean outgoing)
{
	/* build the spec based on the folder, and possibly from a saved file */
	/* otherwise, leave default */
	if (message_list->folder) {
		char *path;
		char *name;
		struct stat st;

		g_object_set (message_list->tree,
			      "uniform_row_height", TRUE,
			      NULL);
		
		name = camel_service_get_name (CAMEL_SERVICE (message_list->folder->parent_store), TRUE);
		d(printf ("folder name is '%s'\n", name));
		
		path = mail_config_folder_to_cachename (message_list->folder, "et-expanded-");
		if (path && stat (path, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode)) {
			/* build based on saved file */
			e_tree_load_expanded_state (message_list->tree, path);
		}
		g_free (path);
		
		g_free (name);
	}
}

static void
ml_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, MessageList *ml)
{
	struct _MLSelection *selection;

	selection = &ml->priv->clipboard;

	if (selection->uids == NULL)
		return;

	if (info & 2) {
		/* text/plain */
		printf("setting text/plain selection for uids\n");
		em_utils_selection_set_mailbox(data, selection->folder, selection->uids);
	} else {
		/* x-uid-list */
		printf("setting x-uid-list selection for uids\n");
		em_utils_selection_set_uidlist(data, selection->folder_uri, selection->uids);
	}
}

static gboolean
ml_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, MessageList *ml)
{
	struct _MessageListPrivate *p = ml->priv;

	clear_selection(ml, &p->clipboard);

	return TRUE;
}

static void
ml_selection_received(GtkWidget *widget, GtkSelectionData *data, guint time, MessageList *ml)
{
	if (data->target != gdk_atom_intern ("x-uid-list", FALSE)) {
		printf("Unknown selection received by message-list\n");

		return;
	}

	em_utils_selection_get_uidlist(data, ml->folder, FALSE, NULL);
}

static void
ml_tree_drag_data_get (ETree *tree, int row, ETreePath path, int col,
		       GdkDragContext *context, GtkSelectionData *data,
		       guint info, guint time, MessageList *ml)
{
	GPtrArray *uids;

	uids = message_list_get_selected(ml);

	if (uids->len > 0) {
		switch (info) {
		case DND_X_UID_LIST:
			em_utils_selection_set_uidlist(data, ml->folder_uri, uids);
			break;
		case DND_TEXT_URI_LIST:
			em_utils_selection_set_urilist(data, ml->folder, uids);
			break;
		}
	}

	message_list_free_uids(ml, uids);
}

/* TODO: merge this with the folder tree stuff via empopup targets */
/* Drop handling */
struct _drop_msg {
	struct _mail_msg msg;
	
	GdkDragContext *context;

	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	CamelFolder *folder;

	guint32 action;
	guint info;
	
	unsigned int move:1;
	unsigned int moved:1;
	unsigned int aborted:1;
};

static char *
ml_drop_async_desc (struct _mail_msg *mm, int done)
{
	struct _drop_msg *m = (struct _drop_msg *) mm;

	if (m->move)
		return g_strdup_printf(_("Moving messages into folder %s"), m->folder->full_name);
	else
		return g_strdup_printf(_("Copying messages into folder %s"), m->folder->full_name);
}

static void
ml_drop_async_drop(struct _mail_msg *mm)
{
	struct _drop_msg *m = (struct _drop_msg *)mm;

	switch (m->info) {
	case DND_X_UID_LIST:
		em_utils_selection_get_uidlist(m->selection, m->folder, m->action == GDK_ACTION_MOVE, &mm->ex);
		break;
	case DND_MESSAGE_RFC822:
		em_utils_selection_get_message(m->selection, m->folder);
		break;
	case DND_TEXT_URI_LIST:
		em_utils_selection_get_urilist(m->selection, m->folder);
		break;
	}
}

static void
ml_drop_async_done(struct _mail_msg *mm)
{
	struct _drop_msg *m = (struct _drop_msg *)mm;
	gboolean success, delete;
	
	/* ?? */
	if (m->aborted) {
		success = FALSE;
		delete = FALSE;
	} else {
		success = !camel_exception_is_set (&mm->ex);
		delete = success && m->move && !m->moved;
	}

	gtk_drag_finish(m->context, success, delete, GDK_CURRENT_TIME);
}

static void
ml_drop_async_free(struct _mail_msg *mm)
{
	struct _drop_msg *m = (struct _drop_msg *)mm;
	
	g_object_unref(m->context);
	camel_object_unref(m->folder);

	g_free(m->selection->data);
	g_free(m->selection);
}

static struct _mail_msg_op ml_drop_async_op = {
	ml_drop_async_desc,
	ml_drop_async_drop,
	ml_drop_async_done,
	ml_drop_async_free,
};

static void
ml_drop_action(struct _drop_msg *m)
{
	m->move = m->action == GDK_ACTION_MOVE;
	e_thread_put (mail_thread_new, (EMsg *) m);
}

static void
ml_drop_popup_copy(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_msg *m = data;

	m->action = GDK_ACTION_COPY;
	ml_drop_action(m);
}

static void
ml_drop_popup_move(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_msg *m = data;

	m->action = GDK_ACTION_MOVE;
	ml_drop_action(m);
}

static void
ml_drop_popup_cancel(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_msg *m = data;

	m->aborted = TRUE;
	mail_msg_free(&m->msg);
}

static EPopupItem ml_drop_popup_menu[] = {
	{ E_POPUP_ITEM, "00.emc.02", N_("_Copy"), ml_drop_popup_copy, NULL, "stock_folder-copy", 0 },
	{ E_POPUP_ITEM, "00.emc.03", N_("_Move"), ml_drop_popup_move, NULL, "stock_folder-move", 0 },
	{ E_POPUP_BAR, "10.emc" },
	{ E_POPUP_ITEM, "99.emc.00", N_("Cancel _Drag"), ml_drop_popup_cancel, NULL, NULL, 0 },
};

static void
ml_drop_popup_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);

	/* FIXME: free data if no item was selected? */
}

static void
ml_tree_drag_data_received (ETree *tree, int row, ETreePath path, int col,
			    GdkDragContext *context, gint x, gint y,
			    GtkSelectionData *data, guint info,
			    guint time, MessageList *ml)
{
	struct _drop_msg *m;

	/* this means we are receiving no data */
	if (data->data == NULL || data->length == -1)
		return;

	m = mail_msg_new(&ml_drop_async_op, NULL, sizeof(*m));
	m->context = context;
	g_object_ref(context);
	m->folder = ml->folder;
	camel_object_ref(m->folder);
	m->action = context->action;
	m->info = info;

	/* need to copy, goes away once we exit */
	m->selection = g_malloc0(sizeof(*m->selection));
	m->selection->data = g_malloc(data->length);
	memcpy(m->selection->data, data->data, data->length);
	m->selection->length = data->length;

	if (context->action == GDK_ACTION_ASK) {
		EMPopup *emp;
		GSList *menus = NULL;
		GtkMenu *menu;
		int i;

		emp = em_popup_new("org.gnome.mail.messagelist.popup.drop");
		for (i=0;i<sizeof(ml_drop_popup_menu)/sizeof(ml_drop_popup_menu[0]);i++)
			menus = g_slist_append(menus, &ml_drop_popup_menu[i]);

		e_popup_add_items((EPopup *)emp, menus, ml_drop_popup_free, m);
		menu = e_popup_create_menu_once((EPopup *)emp, NULL, 0);
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	} else {
		ml_drop_action(m);
	}
}

static gboolean
ml_tree_drag_motion(ETree *tree, GdkDragContext *context, gint x, gint y, guint time, MessageList *ml)
{
	GList *targets;
	GdkDragAction action, actions = 0;

	for (targets = context->targets; targets; targets = targets->next) {
		int i;

		printf("atom drop '%s'\n", gdk_atom_name(targets->data));
		for (i=0;i<sizeof(ml_drag_info)/sizeof(ml_drag_info[0]);i++)
			if (targets->data == (void *)ml_drag_info[i].atom)
				actions |= ml_drag_info[i].actions;
	}
	printf("\n");

	actions &= context->actions;
	action = context->suggested_action;
	if (action == GDK_ACTION_COPY && (actions & GDK_ACTION_MOVE))
		action = GDK_ACTION_MOVE;
	else if (action == GDK_ACTION_ASK && (actions & (GDK_ACTION_MOVE|GDK_ACTION_COPY)) != (GDK_ACTION_MOVE|GDK_ACTION_COPY))
		action = GDK_ACTION_MOVE;

	gdk_drag_status(context, action, time);

	return action != 0;
}

static void
ml_scrolled (GtkAdjustment *adj, MessageList *ml)
{
	g_signal_emit (ml, message_list_signals[MESSAGE_LIST_SCROLLED], 0);
}

/*
 * GObject::init
 */
static void
message_list_init (MessageList *message_list)
{
	struct _MessageListPrivate *p;
	GtkAdjustment *adjustment;
	GdkAtom matom;
	
	adjustment = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, G_MAXDOUBLE, 0.0, 0.0, 0.0);
	gtk_scrolled_window_set_vadjustment ((GtkScrolledWindow *) message_list, adjustment);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (message_list), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	
	message_list->normalised_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	message_list->hidden = NULL;
	message_list->hidden_pool = NULL;
	message_list->hide_before = ML_HIDE_NONE_START;
	message_list->hide_after = ML_HIDE_NONE_END;
	
	message_list->search = NULL;
	
	message_list->hide_lock = g_mutex_new();
	
	message_list->uid_nodemap = g_hash_table_new (g_str_hash, g_str_equal);
	message_list->async_event = mail_async_event_new();

	/* TODO: Should this only get the selection if we're realised? */
	p = message_list->priv = g_malloc0(sizeof(*message_list->priv));
	p->invisible = gtk_invisible_new();
	g_object_ref(p->invisible);
	gtk_object_sink((GtkObject *)p->invisible);

	matom = gdk_atom_intern ("x-uid-list", FALSE);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, matom, 0);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 2);

	g_signal_connect(p->invisible, "selection_get", G_CALLBACK(ml_selection_get), message_list);
	g_signal_connect(p->invisible, "selection_clear_event", G_CALLBACK(ml_selection_clear_event), message_list);
	g_signal_connect(p->invisible, "selection_received", G_CALLBACK(ml_selection_received), message_list);
	
	g_signal_connect (((GtkScrolledWindow *) message_list)->vscrollbar, "value-changed", G_CALLBACK (ml_scrolled), message_list);
}

static void
normalised_free (gpointer key, gpointer value, gpointer user_data)
{
	e_poolv_destroy (value);
}

static void
message_list_destroy(GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	struct _MessageListPrivate *p = message_list->priv;

	if (message_list->async_event) {
		mail_async_event_destroy(message_list->async_event);
		message_list->async_event = NULL;
	}

	if (message_list->folder) {
		/* need to do this before removing folder, folderinfo's might not exist after */
		save_tree_state(message_list);
		save_hide_state(message_list);

		mail_regen_cancel(message_list);

		if (message_list->uid_nodemap) {
			g_hash_table_foreach(message_list->uid_nodemap, (GHFunc)clear_info, message_list);
			g_hash_table_destroy (message_list->uid_nodemap);
			message_list->uid_nodemap = NULL;
		}
		
		camel_object_unhook_event(message_list->folder, "folder_changed", folder_changed, message_list);
		camel_object_unref (message_list->folder);
		message_list->folder = NULL;
	}

	if (p->invisible) {
		g_object_unref(p->invisible);
		p->invisible = NULL;
	}

	if (message_list->extras) {
		g_object_unref (message_list->extras);
		message_list->extras = NULL;
	}
	
	if (message_list->model) {
		g_object_unref (message_list->model);
		message_list->model = NULL;
	}
	
	if (message_list->idle_id != 0) {
		g_source_remove (message_list->idle_id);
		message_list->idle_id = 0;
	}
	
	if (message_list->seen_id) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	message_list->destroyed = TRUE;
	
	GTK_OBJECT_CLASS (message_list_parent_class)->destroy(object);
}

static void
message_list_finalise (GObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	struct _MessageListPrivate *p = message_list->priv;
	
	g_hash_table_foreach (message_list->normalised_hash, normalised_free, NULL);
	g_hash_table_destroy (message_list->normalised_hash);
	
	if (message_list->thread_tree)
		camel_folder_thread_messages_unref(message_list->thread_tree);

	if (message_list->hidden) {
		g_hash_table_destroy(message_list->hidden);
		e_mempool_destroy(message_list->hidden_pool);
		message_list->hidden = NULL;
		message_list->hidden_pool = NULL;
	}

	g_free(message_list->frozen_search);
	g_free(message_list->cursor_uid);

	g_mutex_free(message_list->hide_lock);

	g_free(message_list->folder_uri);
	message_list->folder_uri = NULL;

	clear_selection(message_list, &p->clipboard);

	g_free(p);

	G_OBJECT_CLASS (message_list_parent_class)->finalize (object);
}

/*
 * GObjectClass::init
 */
static void
message_list_class_init (MessageListClass *message_list_class)
{
	GObjectClass *object_class = (GObjectClass *) message_list_class;
	GtkObjectClass *gtkobject_class = (GtkObjectClass *) message_list_class;
	int i;

	for (i=0;i<sizeof(ml_drag_info)/sizeof(ml_drag_info[0]);i++)
		ml_drag_info[i].atom = gdk_atom_intern(ml_drag_info[i].target, FALSE);

	object_class->finalize = message_list_finalise;
	gtkobject_class->destroy = message_list_destroy;

	message_list_signals[MESSAGE_SELECTED] =
		g_signal_new ("message_selected",
			      MESSAGE_LIST_TYPE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (MessageListClass, message_selected),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	
	message_list_signals[MESSAGE_LIST_BUILT] =
		g_signal_new ("message_list_built",
			      MESSAGE_LIST_TYPE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (MessageListClass, message_list_built),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	message_list_signals[MESSAGE_LIST_SCROLLED] =
		g_signal_new ("message_list_scrolled",
			      MESSAGE_LIST_TYPE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (MessageListClass, message_list_scrolled),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	message_list_init_images ();
}

static void
message_list_construct (MessageList *message_list)
{
	AtkObject *a11y;
	gboolean construct_failed;
	message_list->model =
		e_tree_memory_callbacks_new (ml_tree_icon_at,
					     
					     ml_column_count,
					     
					     ml_has_save_id,
					     ml_get_save_id,
					     
					     ml_has_get_node_by_id,
					     ml_get_node_by_id,
					     
					     ml_tree_value_at,
					     ml_tree_set_value_at,
					     ml_tree_is_cell_editable,
					     
					     ml_duplicate_value,
					     ml_free_value,
					     ml_initialize_value,
					     ml_value_is_empty,
					     ml_value_to_string,
					     
					     message_list);
	
	e_tree_memory_set_expanded_default(E_TREE_MEMORY(message_list->model), TRUE);
	
	/*
	 * The etree
	 */
	message_list->extras = message_list_create_extras ();
	construct_failed = (e_tree_scrolled_construct_from_spec_file (E_TREE_SCROLLED (message_list),
								      message_list->model,
								      message_list->extras, 
								      EVOLUTION_ETSPECDIR "/message-list.etspec",
								      NULL)
			    == NULL);

	message_list->tree = e_tree_scrolled_get_tree(E_TREE_SCROLLED (message_list));
	if (!construct_failed)
		e_tree_root_node_set_visible (message_list->tree, FALSE);

	if (atk_get_root () != NULL) {
		a11y = gtk_widget_get_accessible (message_list->tree);
		atk_object_set_name (a11y, _("Message List"));
	}

	g_signal_connect((message_list->tree), "cursor_activated",
			 G_CALLBACK (on_cursor_activated_cmd),
			 message_list);
	
	g_signal_connect((message_list->tree), "click",
			 G_CALLBACK (on_click), message_list);

	g_signal_connect((message_list->tree), "selection_change",
			 G_CALLBACK (on_selection_changed_cmd), message_list);


	e_tree_drag_source_set(message_list->tree, GDK_BUTTON1_MASK,
			       ml_drag_types, sizeof(ml_drag_types)/sizeof(ml_drag_types[0]),
			       GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_ASK);
	
	g_signal_connect(message_list->tree, "tree_drag_data_get",
			 G_CALLBACK(ml_tree_drag_data_get), message_list);

	e_tree_drag_dest_set(message_list->tree, GTK_DEST_DEFAULT_ALL,
			     ml_drop_types, sizeof(ml_drop_types)/sizeof(ml_drop_types[0]),
			     GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_ASK);
	
	g_signal_connect(message_list->tree, "tree_drag_data_received",
			 G_CALLBACK(ml_tree_drag_data_received), message_list);
	g_signal_connect(message_list->tree, "drag-motion", G_CALLBACK(ml_tree_drag_motion), message_list);
}

/**
 * message_list_new:
 *
 * Creates a new message-list widget.
 *
 * Returns a new message-list widget.
 **/
GtkWidget *
message_list_new (void)
{
	MessageList *message_list;

	message_list = MESSAGE_LIST (g_object_new(message_list_get_type (),
						  "hadjustment", NULL,
						  "vadjustment", NULL,
						  NULL));
	message_list_construct (message_list);

	return GTK_WIDGET (message_list);
}

static void
clear_info(char *key, ETreePath *node, MessageList *ml)
{
	CamelMessageInfo *info;

	info = e_tree_memory_node_get_data((ETreeMemory *)ml->model, node);
	camel_folder_free_message_info(ml->folder, info);
	e_tree_memory_node_set_data((ETreeMemory *)ml->model, node, NULL);
}

static void
clear_tree (MessageList *ml)
{
	ETreeModel *etm = ml->model;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Clearing tree\n");
	gettimeofday(&start, NULL);
#endif

	/* we also reset the uid_rowmap since it is no longer useful/valid anyway */
	if (ml->folder)
		g_hash_table_foreach (ml->uid_nodemap, (GHFunc)clear_info, ml);
	g_hash_table_destroy (ml->uid_nodemap);
	ml->uid_nodemap = g_hash_table_new (g_str_hash, g_str_equal);
	
	if (ml->tree_root) {
		/* we should be frozen already */
		e_tree_memory_node_remove (E_TREE_MEMORY(etm), ml->tree_root);
	}

	ml->tree_root = e_tree_memory_node_insert (E_TREE_MEMORY(etm), NULL, 0, NULL);

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Clearing tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

/* we try and find something that isn't deleted in our tree
   there is actually no assurance that we'll find somethign that will
   still be there next time, but its probably going to work most of the time */
static char *
find_next_undeleted (MessageList *ml)
{
	ETreePath node;
	int last;
	int vrow;
	ETree *et = ml->tree;
	CamelMessageInfo *info;
	guint32 check;

	node = g_hash_table_lookup (ml->uid_nodemap, ml->cursor_uid);
	if (node == NULL)
		return NULL;

	check = CAMEL_MESSAGE_JUNK;
	if (ml->hidedeleted)
		check |= CAMEL_MESSAGE_DELETED;

	info = get_message_info (ml, node);
	if (info && (camel_message_info_flags(info) & check) == 0) {
		return NULL;
	}

	last = e_tree_row_count (ml->tree);

	/* model_to_view_row etc simply dont work for sorted views.  Sigh. */
	vrow = e_tree_row_of_node (et, node);

	/* We already checked this node. */
	vrow ++;

	while (vrow < last) {
		CamelMessageInfo *info;

		node = e_tree_node_at_row (et, vrow);
		info = get_message_info (ml, node);
		if (info && (camel_message_info_flags(info) & check) == 0) {
			return g_strdup (camel_message_info_uid (info));
		}
		vrow ++;
	}

	return NULL;
}

/* only call if we have a tree model */
/* builds the tree structure */

#define BROKEN_ETREE	/* avoid some broken code in etree(?) by not using the incremental update */

static void build_subtree (MessageList *ml, ETreePath parent, CamelFolderThreadNode *c, int *row);

static void build_subtree_diff (MessageList *ml, ETreePath parent, ETreePath path, CamelFolderThreadNode *c, int *row);

static void
build_tree (MessageList *ml, CamelFolderThread *thread, CamelFolderChangeInfo *changes)
{
	int row = 0;
	ETreeModel *etm = ml->model;
	ETreePath *top;
	char *saveuid = NULL;
#ifdef BROKEN_ETREE
	GPtrArray *selected;
#endif
#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Building tree\n");
	gettimeofday(&start, NULL);
#endif

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Loading tree state took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

	if (ml->tree_root == NULL) {
		ml->tree_root =	e_tree_memory_node_insert(E_TREE_MEMORY(etm), NULL, 0, NULL);
	}

	if (ml->cursor_uid)
		saveuid = find_next_undeleted(ml);

	top = e_tree_model_node_get_first_child(etm, ml->tree_root);
#ifndef BROKEN_ETREE
	if (top == NULL || changes == NULL) {
#else
		selected = message_list_get_selected(ml);
#endif
		e_tree_memory_freeze(E_TREE_MEMORY(etm));
		clear_tree (ml);

		build_subtree(ml, ml->tree_root, thread->tree, &row);
		e_tree_memory_thaw(E_TREE_MEMORY(etm));
#ifdef BROKEN_ETREE
		message_list_set_selected(ml, selected);
		message_list_free_uids(ml, selected);
#else
	} else {
		static int tree_equal(ETreeModel *etm, ETreePath ap, CamelFolderThreadNode *bp);

		build_subtree_diff(ml, ml->tree_root, top,  thread->tree, &row);
		top = e_tree_model_node_get_first_child(etm, ml->tree_root);
		tree_equal(ml->model, top, thread->tree);
	}
#endif
	if (saveuid) {
		ETreePath *node = g_hash_table_lookup (ml->uid_nodemap, saveuid);
		if (node == NULL) {
			g_free (ml->cursor_uid);
			ml->cursor_uid = NULL;
			g_signal_emit (ml, message_list_signals[MESSAGE_SELECTED], 0, NULL);
		} else {
			e_tree_set_cursor (ml->tree, node);
		}
		g_free (saveuid);
	} else if (ml->cursor_uid && !g_hash_table_lookup (ml->uid_nodemap, ml->cursor_uid)) {
		g_free (ml->cursor_uid);
		ml->cursor_uid = NULL;
		g_signal_emit (ml, message_list_signals[MESSAGE_SELECTED], 0, NULL);
	}

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Building tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif
}

/* this is about 20% faster than build_subtree_diff,
   entirely because e_tree_model_node_insert(xx, -1 xx)
   is faster than inserting to the right row :( */
/* Otherwise, this code would probably go as it does the same thing essentially */
static void
build_subtree (MessageList *ml, ETreePath parent, CamelFolderThreadNode *c, int *row)
{
	ETreeModel *tree = ml->model;
	ETreePath node;

	while (c) {
		/* phantom nodes no longer allowed */
		g_assert(c->message);

		node = e_tree_memory_node_insert(E_TREE_MEMORY(tree), parent, -1, (void *)c->message);
		g_hash_table_insert(ml->uid_nodemap, (void *)camel_message_info_uid(c->message), node);
		camel_folder_ref_message_info(ml->folder, (CamelMessageInfo *)c->message);

		if (c->child) {
			build_subtree(ml, node, c->child, row);
		}
		c = c->next;
	}
}

/* compares a thread tree node with the etable tree node to see if they point to
   the same object */
static int
node_equal(ETreeModel *etm, ETreePath ap, CamelFolderThreadNode *bp)
{
	CamelMessageInfo *info;

	info = e_tree_memory_node_get_data(E_TREE_MEMORY(etm), ap);

	if (bp->message && strcmp(camel_message_info_uid(info), camel_message_info_uid(bp->message))==0)
		return 1;

	return 0;
}

#ifndef BROKEN_ETREE
/* debug function - compare the two trees to see if they are the same */
static int
tree_equal(ETreeModel *etm, ETreePath ap, CamelFolderThreadNode *bp)
{
	CamelMessageInfo *info;

	while (ap && bp) {
		if (!node_equal(etm, ap, bp)) {
			g_warning("Nodes in tree differ");
			info = e_tree_memory_node_get_data(E_TREE_MEMORY(etm), ap);
			printf("table uid = %s\n", camel_message_info_uid(info));
			printf("camel uid = %s\n", camel_message_info_uid(bp->message));
			return FALSE;
		} else {
			if (!tree_equal(etm, e_tree_model_node_get_first_child(etm, ap), bp->child))
				return FALSE;
		}
		bp = bp->next;
		ap = e_tree_model_node_get_next(etm, ap);
	}

	if (ap || bp) {
		g_warning("Tree differs, out of nodes in one branch");
		if (ap) {
			info = e_tree_memory_node_get_data(E_TREE_MEMORY(etm), ap);
			if (info)
				printf("table uid = %s\n", camel_message_info_uid(info));
			else
				printf("info is empty?\n");
		}
		if (bp) {
			printf("camel uid = %s\n", camel_message_info_uid(bp->message));
			return FALSE;
		}
		return FALSE;
	}
	return TRUE;
}
#endif

/* adds a single node, retains save state, and handles adding children if required */
static void
add_node_diff(MessageList *ml, ETreePath parent, ETreePath path, CamelFolderThreadNode *c, int *row, int myrow)
{
	ETreeModel *etm = ml->model;
	ETreePath node;

	g_assert(c->message);
 
	/* we just update the hashtable key, umm, does this leak the info on the message node? */
	g_hash_table_remove(ml->uid_nodemap, camel_message_info_uid(c->message));
	node = e_tree_memory_node_insert(E_TREE_MEMORY(etm), parent, myrow, (void *)c->message);
	g_hash_table_insert(ml->uid_nodemap, (void *)camel_message_info_uid(c->message), node);
	camel_folder_ref_message_info(ml->folder, (CamelMessageInfo *)c->message);
	(*row)++;

	if (c->child) {
		build_subtree_diff(ml, node, NULL, c->child, row);
	}
}

/* removes node, children recursively and all associated data */
static void
remove_node_diff(MessageList *ml, ETreePath node, int depth)
{
	ETreeModel *etm = ml->model;
	ETreePath cp, cn;
	CamelMessageInfo *info;

	t(printf("Removing node: %s\n", (char *)e_tree_memory_node_get_data(etm, node)));

	/* we depth-first remove all node data's ... */
	cp = e_tree_model_node_get_first_child(etm, node);
	while (cp) {
		cn = e_tree_model_node_get_next(etm, cp);
		remove_node_diff(ml, cp, depth+1);
		cp = cn;
	}

	/* and the rowid entry - if and only if it is referencing this node */
	info = e_tree_memory_node_get_data(E_TREE_MEMORY (etm), node);

	/* and only at the toplevel, remove the node (etree should optimise this remove somewhat) */
	if (depth == 0)
		e_tree_memory_node_remove(E_TREE_MEMORY(etm), node);

	g_assert(info);
	g_hash_table_remove(ml->uid_nodemap, camel_message_info_uid(info));
	camel_folder_free_message_info(ml->folder, info);
}

/* applies a new tree structure to an existing tree, but only by changing things
   that have changed */
static void
build_subtree_diff(MessageList *ml, ETreePath parent, ETreePath path, CamelFolderThreadNode *c, int *row)
{
	ETreeModel *etm = ml->model;
	ETreePath ap, *ai, *at, *tmp;
	CamelFolderThreadNode *bp, *bi, *bt;
	int i, j, myrow = 0;

	ap = path;
	bp = c;

	while (ap || bp) {
		t(printf("Processing row: %d (subtree row %d)\n", *row, myrow));
		if (ap == NULL) {
			t(printf("out of old nodes\n"));
			/* ran out of old nodes - remaining nodes are added */
			add_node_diff(ml, parent, ap, bp, row, myrow);
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
#if 0
			if (bp->message) {
				char *olduid;
				int oldrow;
				/* if this is a message row, check/update the row id map */
				if (g_hash_table_lookup_extended(ml->uid_rowmap, camel_message_info_uid(bp->message), (void *)&olduid, (void *)&oldrow)) {
					if (oldrow != (*row)) {
						g_hash_table_insert(ml->uid_rowmap, olduid, (void *)(*row));
					}
				} else {
					g_warning("Cannot find uid %s in table?", camel_message_info_uid(bp->message));
					/*g_assert_not_reached();*/
				}
			}
#endif
			*row = (*row)+1;
			myrow++;
			tmp = e_tree_model_node_get_first_child(etm, ap);
			/* make child lists match (if either has one) */
			if (bp->child || tmp) {
				build_subtree_diff(ml, ap, tmp, bp->child, row);
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
						add_node_diff(ml, parent, NULL, bt, row, myrow);
						myrow++;
						bt = bt->next;
					}
					bp = bi;
				} else {
					t(printf("adding new node 1\n"));
					/* no match in new nodes, add one, try next */
					add_node_diff(ml, parent, NULL, bp, row, myrow);
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
					add_node_diff(ml, parent, NULL, bp, row, myrow);
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

#ifndef BROKEN_ETREE
static void build_flat_diff(MessageList *ml, CamelFolderChangeInfo *changes);
#endif

static void
build_flat (MessageList *ml, GPtrArray *summary, CamelFolderChangeInfo *changes)
{
	ETreeModel *etm = ml->model;
	ETreePath node;
	char *saveuid = NULL;
	int i;
#ifdef BROKEN_ETREE
	GPtrArray *selected;
#endif
#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	printf("Building flat\n");
	gettimeofday(&start, NULL);
#endif

	if (ml->cursor_uid)
		saveuid = find_next_undeleted(ml);

#ifndef BROKEN_ETREE
	if (changes) {
		build_flat_diff(ml, changes);
	} else {
#else
		selected = message_list_get_selected(ml);
#endif
		e_tree_memory_freeze(E_TREE_MEMORY(etm));
		clear_tree (ml);
		for (i = 0; i < summary->len; i++) {
			CamelMessageInfo *info = summary->pdata[i];

			node = e_tree_memory_node_insert(E_TREE_MEMORY(etm), ml->tree_root, -1, info);
			g_hash_table_insert(ml->uid_nodemap, (void *)camel_message_info_uid(info), node);
			camel_folder_ref_message_info(ml->folder, info);
		}
		e_tree_memory_thaw(E_TREE_MEMORY(etm));
#ifdef BROKEN_ETREE
		message_list_set_selected(ml, selected);
		message_list_free_uids(ml, selected);
#else
	}
#endif

	if (saveuid) {
		ETreePath *node = g_hash_table_lookup(ml->uid_nodemap, saveuid);
		if (node == NULL) {
			g_free (ml->cursor_uid);
			ml->cursor_uid = NULL;
			g_signal_emit (ml, message_list_signals[MESSAGE_SELECTED], 0, NULL);
		} else {
			e_tree_set_cursor (ml->tree, node);
		}
		g_free (saveuid);
	}

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Building flat took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static void
message_list_change_first_visible_parent (MessageList *ml, ETreePath node)
{
	ETreePath first_visible = NULL;
	
	while (node && (node = e_tree_model_node_get_parent (ml->model, node))) {
		if (!e_tree_node_is_expanded (ml->tree, node))
			first_visible = node;
	}
	
	if (first_visible != NULL) {
		e_tree_model_pre_change (ml->model);
		e_tree_model_node_data_changed (ml->model, first_visible);
	}
}

#ifndef BROKEN_ETREE

static void
build_flat_diff(MessageList *ml, CamelFolderChangeInfo *changes)
{
	int i;
	ETreePath node;
	CamelMessageInfo *info;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;
		
	gettimeofday(&start, NULL);
#endif

	d(printf("updating changes to display\n"));

	/* remove individual nodes? */
	d(printf("Removing messages from view:\n"));
	for (i=0;i<changes->uid_removed->len;i++) {
		node = g_hash_table_lookup(ml->uid_nodemap, changes->uid_removed->pdata[i]);
		if (node) {
			info = e_tree_memory_node_get_data(E_TREE_MEMORY(ml->model), node);
			e_tree_memory_node_remove(E_TREE_MEMORY(ml->model), node);
			camel_folder_free_message_info(ml->folder, info);
			g_hash_table_remove(ml->uid_nodemap, changes->uid_removed->pdata[i]);
		}
	}

	/* add new nodes? - just append to the end */
	d(printf("Adding messages to view:\n"));
	for (i=0;i<changes->uid_added->len;i++) {
		info = camel_folder_get_message_info (ml->folder, changes->uid_added->pdata[i]);
		if (info) {
			d(printf(" %s\n", (char *)changes->uid_added->pdata[i]));
			node = e_tree_memory_node_insert (E_TREE_MEMORY (ml->model), ml->tree_root, -1, info);
			g_hash_table_insert (ml->uid_nodemap, (void *)camel_message_info_uid (info), node);
		}
	}

	/* and update changes too */
	d(printf("Changing messages to view:\n"));
	for (i = 0; i < changes->uid_changed->len; i++) {
		ETreePath *node = g_hash_table_lookup (ml->uid_nodemap, changes->uid_changed->pdata[i]);
		if (node) {
			e_tree_model_pre_change (ml->model);
			e_tree_model_node_data_changed (ml->model, node);
			
			message_list_change_first_visible_parent (ml, node);
		}
	}

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Inserting changes took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}
#endif /* ! BROKEN_ETREE */


static void
mail_folder_hide_by_flag (CamelFolder *folder, MessageList *ml, CamelFolderChangeInfo **changes, int flag)
{
	CamelFolderChangeInfo *newchanges, *oldchanges = *changes;
	CamelMessageInfo *info;
	int i;

	newchanges = camel_folder_change_info_new ();
			
	for (i = 0; i < oldchanges->uid_changed->len; i++) {
		ETreePath node = g_hash_table_lookup (ml->uid_nodemap, oldchanges->uid_changed->pdata[i]);
		guint32 flags;

		info = camel_folder_get_message_info (folder, oldchanges->uid_changed->pdata[i]);
		if (info)
			flags = camel_message_info_flags(info);

		if (node != NULL && info != NULL && (flags & flag) != 0)
			camel_folder_change_info_remove_uid (newchanges, oldchanges->uid_changed->pdata[i]);
		else if (node == NULL && info != NULL && (flags & flag) == 0)
			camel_folder_change_info_add_uid (newchanges, oldchanges->uid_changed->pdata[i]);
		else
			camel_folder_change_info_change_uid (newchanges, oldchanges->uid_changed->pdata[i]);
		camel_folder_free_message_info (folder, info);
	}

	if (newchanges->uid_added->len > 0 || newchanges->uid_removed->len > 0) {
		for (i = 0; i < oldchanges->uid_added->len; i++)
			camel_folder_change_info_add_uid (newchanges, oldchanges->uid_added->pdata[i]);
		for (i = 0; i < oldchanges->uid_removed->len; i++)
			camel_folder_change_info_remove_uid (newchanges, oldchanges->uid_removed->pdata[i]);
		camel_folder_change_info_free (oldchanges);
		*changes = newchanges;
	} else {
		camel_folder_change_info_free (newchanges);
	}
}

static void
main_folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	MessageList *ml = MESSAGE_LIST (user_data);
	CamelFolderChangeInfo *changes = (CamelFolderChangeInfo *)event_data;
	CamelFolder *folder = (CamelFolder *)o;
	int i;

	/* may be NULL if we're in the process of being destroyed */
	if (ml->async_event == NULL)
		return;
	
	d(printf("folder changed event, changes = %p\n", changes));
	if (changes) {
		d(printf("changed = %d added = %d removed = %d\n",
			 changes->uid_changed->len, changes->uid_added->len, changes->uid_removed->len));
		
		for (i = 0; i < changes->uid_removed->len; i++) {
			/* uncache the normalised strings for these uids */
			EPoolv *poolv;
			
			poolv = g_hash_table_lookup (ml->normalised_hash, changes->uid_removed->pdata[i]);
			if (poolv != NULL) {
				g_hash_table_remove (ml->normalised_hash, changes->uid_removed->pdata[i]);
				e_poolv_destroy (poolv);
			}
		}
		
		/* check if the hidden state has changed, if so modify accordingly, then regenerate */
		if (ml->hidejunk || ml->hidedeleted)
			mail_folder_hide_by_flag (folder, ml, &changes, (ml->hidejunk ? CAMEL_MESSAGE_JUNK : 0) | (ml->hidedeleted ? CAMEL_MESSAGE_DELETED : 0));

		if (changes->uid_added->len == 0 && changes->uid_removed->len == 0 && changes->uid_changed->len < 100) {
			for (i = 0; i < changes->uid_changed->len; i++) {
				ETreePath node = g_hash_table_lookup (ml->uid_nodemap, changes->uid_changed->pdata[i]);
				if (node) {
					e_tree_model_pre_change (ml->model);
					e_tree_model_node_data_changed (ml->model, node);
					
					message_list_change_first_visible_parent (ml, node);
				}
			}
			
			camel_folder_change_info_free (changes);
			return;
		}
	}
	
	mail_regen_list (ml, ml->search, NULL, changes);
}

static void
folder_changed (CamelObject *o, gpointer event_data, gpointer user_data)
{
	CamelFolderChangeInfo *changes;
	MessageList *ml = MESSAGE_LIST (user_data);

	if (event_data) {
		changes = camel_folder_change_info_new();
		camel_folder_change_info_cat(changes, (CamelFolderChangeInfo *)event_data);
	} else {
		changes = NULL;
	}
	
	mail_async_event_emit(ml->async_event, MAIL_ASYNC_GUI, (MailAsyncFunc)main_folder_changed, o, changes, user_data);
}

/**
 * message_list_set_folder:
 * @message_list: Message List widget
 * @folder: folder backend to be set
 * @uri: uri of @folder.
 * @outgoing: whether this is an outgoing folder
 *
 * Sets @folder to be the backend folder for @message_list. If
 * @outgoing is %TRUE, then the message-list UI changes to default to
 * the "Outgoing folder" column view.
 **/
void
message_list_set_folder (MessageList *message_list, CamelFolder *folder, const char *uri, gboolean outgoing)
{
	gboolean hide_deleted;
	GConfClient *gconf;
	CamelException ex;
	
	g_return_if_fail (IS_MESSAGE_LIST (message_list));
	
	if (message_list->folder == folder)
		return;
	
	camel_exception_init (&ex);
	
	/* remove the cursor activate idle handler */
	if (message_list->idle_id != 0) {
		g_source_remove (message_list->idle_id);
		message_list->idle_id = 0;
	}

	mail_regen_cancel(message_list);
	
	if (message_list->folder != NULL) {
		save_tree_state (message_list);
		save_hide_state (message_list);
	}
	
	clear_tree (message_list);
	
	if (message_list->folder) {
		camel_object_unhook_event((CamelObject *)message_list->folder, "folder_changed",
					  folder_changed, message_list);
		camel_object_unref (message_list->folder);
		message_list->folder = NULL;
	}
	
	if (message_list->thread_tree) {
		camel_folder_thread_messages_unref(message_list->thread_tree);
		message_list->thread_tree = NULL;
	}

	if (message_list->folder_uri != uri) {
		g_free(message_list->folder_uri);
		message_list->folder_uri = g_strdup(uri);
	}
	
	if (message_list->cursor_uid) {
		g_free(message_list->cursor_uid);
		message_list->cursor_uid = NULL;
		g_signal_emit(message_list, message_list_signals[MESSAGE_SELECTED], 0, NULL);
	}
	
	if (folder) {
		int strikeout_col = -1;
		ECell *cell;
		
		camel_object_ref (folder);
		message_list->folder = folder;
		message_list->just_set_folder = TRUE;
		
		/* Setup the strikeout effect for non-trash folders */
		if (!(folder->folder_flags & CAMEL_FOLDER_IS_TRASH))
			strikeout_col = COL_DELETED;
		
		cell = e_table_extras_get_cell (message_list->extras, "render_date");
		g_object_set (cell, "strikeout_column", strikeout_col, NULL);
		
		cell = e_table_extras_get_cell (message_list->extras, "render_text");
		g_object_set (cell, "strikeout_column", strikeout_col, NULL);
		
		cell = e_table_extras_get_cell (message_list->extras, "render_size");
		g_object_set (cell, "strikeout_column", strikeout_col, NULL);
		
		/* Build the etree suitable for this folder */
		message_list_setup_etree (message_list, outgoing);
		
		camel_object_hook_event (folder, "folder_changed", folder_changed, message_list);
		
		gconf = mail_config_get_gconf_client ();
		hide_deleted = !gconf_client_get_bool (gconf, "/apps/evolution/mail/display/show_deleted", NULL);
		message_list->hidedeleted = hide_deleted && !(folder->folder_flags & CAMEL_FOLDER_IS_TRASH);
		message_list->hidejunk = junk_folder && !(folder->folder_flags & CAMEL_FOLDER_IS_JUNK) && !(folder->folder_flags & CAMEL_FOLDER_IS_TRASH);
		
		load_hide_state (message_list);
		if (message_list->frozen == 0)
			mail_regen_list (message_list, message_list->search, NULL, NULL);
	}
}

static gboolean
on_cursor_activated_idle (gpointer data)
{
	MessageList *message_list = data;
	ESelectionModel *esm = e_tree_get_selection_model (message_list->tree);
	int selected = e_selection_model_selected_count (esm);
	
	if (selected == 1 && message_list->cursor_uid) {
		d(printf ("emitting cursor changed signal, for uid %s\n", message_list->cursor_uid));
		g_signal_emit (message_list, message_list_signals[MESSAGE_SELECTED], 0, message_list->cursor_uid);
	} else {
		g_signal_emit (message_list, message_list_signals[MESSAGE_SELECTED], 0, NULL);
	}
		
	message_list->idle_id = 0;
	return FALSE;
}

static void
on_cursor_activated_cmd (ETree *tree, int row, ETreePath path, gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (user_data);
	const char *new_uid;

	if (path == NULL)
		new_uid = NULL;
	else
		new_uid = get_message_uid (message_list, path);

	if ((message_list->cursor_uid == NULL && new_uid == NULL)
	    || (message_list->cursor_uid != NULL && new_uid != NULL && !strcmp (message_list->cursor_uid, new_uid)))
		return;
	
	message_list->cursor_row = row;
	g_free (message_list->cursor_uid);
	message_list->cursor_uid = g_strdup (new_uid);
	
	if (!message_list->idle_id) {
		message_list->idle_id =
			g_idle_add_full (G_PRIORITY_LOW, on_cursor_activated_idle,
					 message_list, NULL);
	}
}

static void
on_selection_changed_cmd(ETree *tree, MessageList *ml)
{
	GPtrArray *uids;
	char *newuid;

	/* not sure if we could just ignore this for the cursor, i think sometimes you
	   only get a selection changed when you should also get a cursor activated? */
	uids = message_list_get_selected(ml);
	if (uids->len == 1)
		newuid = uids->pdata[0];
	else
		newuid = NULL;

	/* If the selection isn't empty, then we ignore the no-uid check, since this event
	   is also used for other updating.  If it is empty, it might just be a setup event
	   from etree which we do need to ignore */
	if ((newuid == NULL && ml->cursor_uid == NULL && uids->len == 0)
	    || (newuid != NULL && ml->cursor_uid != NULL && !strcmp(ml->cursor_uid, newuid))) {
		/* noop */
	} else {
		g_free(ml->cursor_uid);
		ml->cursor_uid = g_strdup(newuid);
		if (!ml->idle_id)
			ml->idle_id = g_idle_add_full (G_PRIORITY_LOW, on_cursor_activated_idle, ml, NULL);
	}

	message_list_free_uids(ml, uids);
}

static gint
on_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, MessageList *list)
{
	CamelMessageInfo *info;
	int flag;
	guint32 flags;

	if (col == COL_MESSAGE_STATUS)
		flag = CAMEL_MESSAGE_SEEN;
	else if (col == COL_FLAGGED)
		flag = CAMEL_MESSAGE_FLAGGED;
	else
		return FALSE;
	
	if (!(info = get_message_info (list, path)))
		return FALSE;

	flags = camel_message_info_flags(info);

	/* If a message was marked as deleted and the user flags it as
	   important, marks it as needing a reply, marks it as unread,
	   then undelete the message. */
	if (flags & CAMEL_MESSAGE_DELETED) {		
		if (col == COL_FLAGGED && !(flags & CAMEL_MESSAGE_FLAGGED))
			flag |= CAMEL_MESSAGE_DELETED;
		
		if (col == COL_MESSAGE_STATUS && (flags & CAMEL_MESSAGE_SEEN))
			flag |= CAMEL_MESSAGE_DELETED;
	}
	
	camel_folder_set_message_flags (list->folder, camel_message_info_uid (info), flag, ~flags);
	
	if (flag == CAMEL_MESSAGE_SEEN && list->seen_id) {
		g_source_remove (list->seen_id);
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
mlfe_callback (ETreePath path, gpointer user_data)
{
	struct message_list_foreach_data *mlfe_data = user_data;
	const char *uid;

	if (e_tree_model_node_is_root (mlfe_data->message_list->model, path))
		return;
	
	uid = get_message_uid (mlfe_data->message_list,
			       path);
	if (uid) {
		mlfe_data->callback (mlfe_data->message_list, uid,
				     mlfe_data->user_data);
	} else {
		/* FIXME: could this the cause of bug #6637 and friends? */
		g_warning ("I wonder if this could be the cause of bug #6637 and friends?");
		g_assert_not_reached ();
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
	e_tree_selected_path_foreach (message_list->tree,
				      mlfe_callback, &mlfe_data);
}

struct _ml_selected_data {
	MessageList *ml;
	GPtrArray *uids;
};

static void
ml_getselected_cb(ETreePath path, void *user_data)
{
	struct _ml_selected_data *data = user_data;
	const char *uid;

	if (e_tree_model_node_is_root (data->ml->model, path))
		return;

	uid = get_message_uid(data->ml, path);
	g_assert(uid != NULL);
	g_ptr_array_add(data->uids, g_strdup(uid));
}

GPtrArray *
message_list_get_selected(MessageList *ml)
{
	struct _ml_selected_data data = {
		ml,
		g_ptr_array_new()
	};

	e_tree_selected_path_foreach(ml->tree, ml_getselected_cb, &data);

	return data.uids;
}

void
message_list_set_selected(MessageList *ml, GPtrArray *uids)
{
	int i;
	ETreeSelectionModel *etsm;
	ETreePath node;
	GPtrArray *paths = g_ptr_array_new();

	etsm = (ETreeSelectionModel *)e_tree_get_selection_model(ml->tree);
	for (i=0; i<uids->len; i++) {
		node = g_hash_table_lookup(ml->uid_nodemap, uids->pdata[i]);
		if (node)
			g_ptr_array_add(paths, node);
	}

	e_tree_selection_model_select_paths(etsm, paths);
	g_ptr_array_free(paths, TRUE);
}

void
message_list_freeze(MessageList *ml)
{
	ml->frozen++;
}

void
message_list_thaw(MessageList *ml)
{
	g_assert(ml->frozen != 0);

	ml->frozen--;
	if (ml->frozen == 0) {
		mail_regen_list(ml, ml->frozen_search?ml->frozen_search:ml->search, NULL, NULL);
		g_free(ml->frozen_search);
		ml->frozen_search = NULL;
	}
}

void message_list_free_uids(MessageList *ml, GPtrArray *uids)
{
	int i;

	for (i=0;i<uids->len;i++)
		g_free(uids->pdata[i]);
	g_ptr_array_free(uids, TRUE);
}

/* set whether we are in threaded view or flat view */
void
message_list_set_threaded (MessageList *ml, gboolean threaded)
{
	if (ml->threaded != threaded) {
		ml->threaded = threaded;
		
		if (ml->frozen == 0)
			mail_regen_list (ml, ml->search, NULL, NULL);
	}
}

void
message_list_set_hidedeleted (MessageList *ml, gboolean hidedeleted)
{
	if (ml->hidedeleted != hidedeleted) {
		ml->hidedeleted = hidedeleted;
		
		if (ml->frozen == 0)
			mail_regen_list (ml, ml->search, NULL, NULL);
	}
}

void
message_list_set_search (MessageList *ml, const char *search)
{
	if (search == NULL || search[0] == '\0')
		if (ml->search == NULL || ml->search[0] == '\0')
			return;
	
	if (search != NULL && ml->search != NULL && strcmp (search, ml->search) == 0)
		return;

	if (ml->thread_tree) {
		camel_folder_thread_messages_unref(ml->thread_tree);
		ml->thread_tree = NULL;
	}

	if (ml->frozen == 0)
		mail_regen_list (ml, search, NULL, NULL);
	else {
		g_free(ml->frozen_search);
		ml->frozen_search = g_strdup(search);
	}
}

/* returns the number of messages displayable *after* expression hiding has taken place */
unsigned int
message_list_length (MessageList *ml)
{
	return ml->hide_unhidden;
}

struct _glibsuxcrap {
	unsigned int count;
	CamelFolder *folder;
};

static void
glib_crapback(void *key, void *data, void *x)
{
	struct _glibsuxcrap *y = x;
	CamelMessageInfo *mi;

	mi = camel_folder_get_message_info(y->folder, key);
	if (mi) {
		y->count++;
		camel_folder_free_message_info(y->folder, mi);
	}
}

/* returns number of hidden messages */
unsigned int
message_list_hidden(MessageList *ml)
{
	unsigned int hidden = 0;

	MESSAGE_LIST_LOCK (ml, hide_lock);
	if (ml->hidden && ml->folder) {
		/* this is a hack, should probably just maintain the hidden table better */
		struct _glibsuxcrap x = { 0, ml->folder };
		g_hash_table_foreach(ml->hidden, glib_crapback, &x);
		hidden = x.count;
	}
	MESSAGE_LIST_UNLOCK (ml, hide_lock);

	return hidden;
}

/* add a new expression to hide, or set the range.
   @expr: A new search expression - all matching messages will be hidden.  May be %NULL.
   @lower: Use ML_HIDE_NONE_START to specify no messages hidden from the start of the list.
   @upper: Use ML_HIDE_NONE_END to specify no message hidden from the end of the list.

   For either @upper or @lower, use ML_HIDE_SAME, to keep the previously set hide range.
   If either range is negative, then the range is taken from the end of the available list
   of messages, once other hiding has been performed.  Use message_list_length() to find out
   how many messages are available for hiding.

   Example: hide_add(ml, NULL, -100, ML_HIDE_NONE_END) -> hide all but the last (most recent)
   100 messages.
*/
void
message_list_hide_add (MessageList *ml, const char *expr, unsigned int lower, unsigned int upper)
{
	MESSAGE_LIST_LOCK (ml, hide_lock);
	
	if (lower != ML_HIDE_SAME)
		ml->hide_before = lower;
	if (upper != ML_HIDE_SAME)
		ml->hide_after = upper;
	
	MESSAGE_LIST_UNLOCK (ml, hide_lock);
	
	mail_regen_list (ml, ml->search, expr, NULL);
}

/* hide specific uid's */
void
message_list_hide_uids (MessageList *ml, GPtrArray *uids)
{
	int i;
	char *uid;

	/* first see if we need to do any work, if so, then do it all at once */
	for (i = 0; i < uids->len; i++) {
		if (g_hash_table_lookup (ml->uid_nodemap, uids->pdata[i])) {
			MESSAGE_LIST_LOCK (ml, hide_lock);
			if (ml->hidden == NULL) {
				ml->hidden = g_hash_table_new (g_str_hash, g_str_equal);
				ml->hidden_pool = e_mempool_new (512, 256, E_MEMPOOL_ALIGN_BYTE);
			}
			
			uid =  e_mempool_strdup (ml->hidden_pool, uids->pdata[i]);
			g_hash_table_insert (ml->hidden, uid, uid);
			for ( ; i < uids->len; i++) {
				if (g_hash_table_lookup (ml->uid_nodemap, uids->pdata[i])) {
					uid =  e_mempool_strdup (ml->hidden_pool, uids->pdata[i]);
					g_hash_table_insert (ml->hidden, uid, uid);
				}
			}
			MESSAGE_LIST_UNLOCK (ml, hide_lock);
			/* save this here incase the user pops up another window, so they are consistent */
			save_hide_state(ml);
			if (ml->frozen == 0)
				mail_regen_list (ml, ml->search, NULL, NULL);
			break;
		}
	}
}

/* no longer hide any messages */
void
message_list_hide_clear (MessageList *ml)
{
	MESSAGE_LIST_LOCK (ml, hide_lock);
	if (ml->hidden) {
		g_hash_table_destroy (ml->hidden);
		e_mempool_destroy (ml->hidden_pool);
		ml->hidden = NULL;
		ml->hidden_pool = NULL;
	}
	ml->hide_before = ML_HIDE_NONE_START;
	ml->hide_after = ML_HIDE_NONE_END;
	MESSAGE_LIST_UNLOCK (ml, hide_lock);

	if (ml->thread_tree) {
		camel_folder_thread_messages_unref(ml->thread_tree);
		ml->thread_tree = NULL;
	}

	/* save this here incase the user pops up another window, so they are consistent */
	save_hide_state(ml);
	if (ml->frozen == 0)
		mail_regen_list (ml, ml->search, NULL, NULL);
}

#define HIDE_STATE_VERSION (1)

/* version 1 file is:
   uintf	1
   uintf	hide_before
   uintf       	hide_after
   string*	uids
*/

static void
load_hide_state (MessageList *ml)
{
	char *filename;
	FILE *in;
	guint32 version, lower, upper;

	MESSAGE_LIST_LOCK(ml, hide_lock);
	if (ml->hidden) {
		g_hash_table_destroy (ml->hidden);
		e_mempool_destroy (ml->hidden_pool);
		ml->hidden = NULL;
		ml->hidden_pool = NULL;
	}
	ml->hide_before = ML_HIDE_NONE_START;
	ml->hide_after = ML_HIDE_NONE_END;

	filename = mail_config_folder_to_cachename(ml->folder, "hidestate-");
	in = fopen(filename, "r");
	if (in) {
		camel_file_util_decode_fixed_int32 (in, &version);
		if (version == HIDE_STATE_VERSION) {
			ml->hidden = g_hash_table_new(g_str_hash, g_str_equal);
			ml->hidden_pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);
			camel_file_util_decode_fixed_int32 (in, &lower);
			ml->hide_before = lower;
			camel_file_util_decode_fixed_int32 (in, &upper);
			ml->hide_after = upper;
			while (!feof(in)) {
				char *olduid, *uid;
				
				if (camel_file_util_decode_string (in, &olduid) != -1) {
					uid =  e_mempool_strdup(ml->hidden_pool, olduid);
					g_free (olduid);
					g_hash_table_insert(ml->hidden, uid, uid);
				}
			}
		}
		fclose(in);
	}
	g_free(filename);

	MESSAGE_LIST_UNLOCK(ml, hide_lock);
}

static void
hide_save_1 (char *uid, char *keydata, FILE *out)
{
	camel_file_util_encode_string (out, uid);
}

/* save the hide state.  Note that messages are hidden by uid, if the uid's change, then
   this will become invalid, but is easy to reset in the ui */
static void
save_hide_state (MessageList *ml)
{
	char *filename;
	FILE *out;
	
	if (ml->folder == NULL)
		return;
	
	MESSAGE_LIST_LOCK(ml, hide_lock);

	filename = mail_config_folder_to_cachename(ml->folder, "hidestate-");
	if (ml->hidden == NULL && ml->hide_before == ML_HIDE_NONE_START && ml->hide_after == ML_HIDE_NONE_END) {
		unlink(filename);
	} else if ((out = fopen (filename, "w"))) {
		camel_file_util_encode_fixed_int32 (out, HIDE_STATE_VERSION);
		camel_file_util_encode_fixed_int32 (out, ml->hide_before);
		camel_file_util_encode_fixed_int32 (out, ml->hide_after);
		if (ml->hidden)
			g_hash_table_foreach(ml->hidden, (GHFunc)hide_save_1, out);
		fclose(out);
	}
	g_free (filename);

	MESSAGE_LIST_UNLOCK(ml, hide_lock);
}

/* ** REGENERATE MESSAGELIST ********************************************** */
struct _regen_list_msg {
	struct _mail_msg msg;

	int complete;

	MessageList *ml;
	char *search;
	char *hideexpr;
	CamelFolderChangeInfo *changes;
	gboolean dotree;	/* we are building a tree */
	gboolean hidedel;	/* we want to/dont want to show deleted messages */
	gboolean hidejunk;	/* we want to/dont want to show junk messages */
	gboolean thread_subject;
	CamelFolderThread *tree;

	CamelFolder *folder;
	GPtrArray *summary;
};

/*
  maintain copy of summary

  any new messages added
  any removed removed, etc.

  use vfolder to implement searches ???

 */

static char *
regen_list_describe (struct _mail_msg *mm, gint complete)
{
	return g_strdup (_("Generating message list"));
}

static void
regen_list_regen (struct _mail_msg *mm)
{
	struct _regen_list_msg *m = (struct _regen_list_msg *)mm;
	GPtrArray *uids, *uidnew, *showuids, *searchuids = NULL;
	CamelMessageInfo *info;
	int i;
	
	if (m->folder != m->ml->folder)
		return;

	/* if we have hidedeleted on, use a search to find it out, merge with existing search if set */
	if (!camel_folder_has_search_capability(m->folder)) {
		/* if we have no search capability, dont let search or hide deleted work */
		uids = camel_folder_get_uids(m->folder);
	} else if (m->hidedel) {
		char *expr;

		if (m->hidejunk) {
			if (m->search) {
				expr = alloca(strlen(m->search) + 92);
				sprintf(expr, "(and (match-all (and (not (system-flag \"deleted\")) (not (system-flag \"junk\"))))\n %s)", m->search);
			} else
				expr = "(match-all (and (not (system-flag \"deleted\")) (not (system-flag \"junk\"))))";
		} else {
			if (m->search) {
				expr = alloca(strlen(m->search) + 64);
				sprintf(expr, "(and (match-all (not (system-flag \"deleted\")))\n %s)", m->search);
			} else
				expr = "(match-all (not (system-flag \"deleted\")))";
		}
		searchuids = uids = camel_folder_search_by_expression (m->folder, expr, &mm->ex);
	} else {
		char *expr;

		if (m->hidejunk) {
			if (m->search) {
				expr = alloca(strlen(m->search) + 64);
				sprintf(expr, "(and (match-all (not (system-flag \"junk\")))\n %s)", m->search);
			} else
				expr = "(match-all (not (system-flag \"junk\")))";
			searchuids = uids = camel_folder_search_by_expression (m->folder, expr, &mm->ex);
		} else {
			if (m->search)
				searchuids = uids = camel_folder_search_by_expression (m->folder, m->search, &mm->ex);
			else
				uids = camel_folder_get_uids (m->folder);
		}
	}
	
	if (camel_exception_is_set (&mm->ex))
		return;
	
	/* perform hiding */
	if (m->hideexpr && camel_folder_has_search_capability(m->folder)) {
		uidnew = camel_folder_search_by_expression (m->ml->folder, m->hideexpr, &mm->ex);
		/* well, lets not abort just because this faileld ... */
		camel_exception_clear (&mm->ex);
		
		if (uidnew) {
			MESSAGE_LIST_LOCK(m->ml, hide_lock);
			
			if (m->ml->hidden == NULL) {
				m->ml->hidden = g_hash_table_new (g_str_hash, g_str_equal);
				m->ml->hidden_pool = e_mempool_new (512, 256, E_MEMPOOL_ALIGN_BYTE);
			}
			
			for (i = 0; i < uidnew->len; i++) {
				if (g_hash_table_lookup (m->ml->hidden, uidnew->pdata[i]) == 0) {
					char *uid = e_mempool_strdup (m->ml->hidden_pool, uidnew->pdata[i]);
					g_hash_table_insert (m->ml->hidden, uid, uid);
				}
			}
			
			MESSAGE_LIST_UNLOCK(m->ml, hide_lock);
			
			camel_folder_search_free (m->ml->folder, uidnew);
		}
	}
	
	MESSAGE_LIST_LOCK(m->ml, hide_lock);
	
	m->ml->hide_unhidden = uids->len;
	
	/* what semantics do we want from hide_before, hide_after?
	   probably <0 means measure from the end of the list */
	
	/* perform uid hiding */
	if (m->ml->hidden || m->ml->hide_before != ML_HIDE_NONE_START || m->ml->hide_after != ML_HIDE_NONE_END) {
		int start, end;
		uidnew = g_ptr_array_new ();
		
		/* first, hide matches */
		if (m->ml->hidden) {
			for (i = 0; i < uids->len; i++) {
				if (g_hash_table_lookup (m->ml->hidden, uids->pdata[i]) == 0)
					g_ptr_array_add (uidnew, uids->pdata[i]);
			}
		}
		
		/* then calculate the subrange visible and chop it out */
		m->ml->hide_unhidden = uidnew->len;
		
		if (m->ml->hide_before != ML_HIDE_NONE_START || m->ml->hide_after != ML_HIDE_NONE_END) {
			GPtrArray *uid2 = g_ptr_array_new ();
			
			start = m->ml->hide_before;
			if (start < 0)
				start += m->ml->hide_unhidden;
			end = m->ml->hide_after;
			if (end < 0)
				end += m->ml->hide_unhidden;
			
			start = MAX(start, 0);
			end = MIN(end, uidnew->len);
			for (i = start; i < end; i++) {
				g_ptr_array_add (uid2, uidnew->pdata[i]);
			}
			
			g_ptr_array_free (uidnew, TRUE);
			uidnew = uid2;
		}
		showuids = uidnew;
	} else {
		uidnew = NULL;
		showuids = uids;
	}
	
	MESSAGE_LIST_UNLOCK(m->ml, hide_lock);
	
	if (!camel_operation_cancel_check(mm->cancel)) {
		/* update/build a new tree */
		if (m->dotree) {
			if (m->tree)
				camel_folder_thread_messages_apply (m->tree, showuids);
			else
				m->tree = camel_folder_thread_messages_new (m->folder, showuids, m->thread_subject);
		} else {
			m->summary = g_ptr_array_new ();
			for (i = 0; i < showuids->len; i++) {
				info = camel_folder_get_message_info (m->folder, showuids->pdata[i]);
				if (info)
					g_ptr_array_add(m->summary, info);
			}
		}
		
		m->complete = TRUE;
	}

	if (uidnew)
		g_ptr_array_free (uidnew, TRUE);

	if (searchuids)
		camel_folder_search_free (m->folder, searchuids);
	else
		camel_folder_free_uids (m->folder, uids);
}

static void
regen_list_regened (struct _mail_msg *mm)
{
	struct _regen_list_msg *m = (struct _regen_list_msg *)mm;

	if (m->ml->destroyed)
		return;
	
	if (!m->complete)
		return;
	
	if (camel_operation_cancel_check(mm->cancel))
		return;

	if (m->ml->folder != m->folder)
		return;

	if (m->dotree) {
		if (m->ml->just_set_folder)
			m->ml->just_set_folder = FALSE;
		else
			save_tree_state (m->ml);
		
		build_tree (m->ml, m->tree, m->changes);
		if (m->ml->thread_tree)
			camel_folder_thread_messages_unref(m->ml->thread_tree);
		m->ml->thread_tree = m->tree;
		m->tree = NULL;
		
		load_tree_state (m->ml);
	} else
		build_flat (m->ml, m->summary, m->changes);

        if (m->ml->search && m->ml->search != m->search)
                g_free (m->ml->search);
	m->ml->search = m->search;

	m->ml->regen = g_list_remove(m->ml->regen, m);

	if (m->ml->regen == NULL && m->ml->pending_select_uid) {
		char *uid = m->ml->pending_select_uid;

		m->ml->pending_select_uid = NULL;
		message_list_select_uid(m->ml, uid);
		g_free(uid);
	}

	g_signal_emit (m->ml, message_list_signals[MESSAGE_LIST_BUILT], 0);
}

static void
regen_list_free (struct _mail_msg *mm)
{
	struct _regen_list_msg *m = (struct _regen_list_msg *)mm;
	int i;

	if (m->summary) {
		for (i = 0; i < m->summary->len; i++)
			camel_folder_free_message_info (m->folder, m->summary->pdata[i]);
		g_ptr_array_free (m->summary, TRUE);
	}
	
	if (m->tree)
		camel_folder_thread_messages_unref (m->tree);
	
	g_free (m->hideexpr);
	
	camel_object_unref (m->folder);
	
	if (m->changes)
		camel_folder_change_info_free (m->changes);

	/* we have to poke this here as well since we might've been cancelled and regened wont get called */
	m->ml->regen = g_list_remove(m->ml->regen, m);

	g_object_unref(m->ml);
}

static struct _mail_msg_op regen_list_op = {
	regen_list_describe,
	regen_list_regen,
	regen_list_regened,
	regen_list_free,
};

static gboolean
ml_regen_timeout(struct _regen_list_msg *m)
{
	m->ml->regen = g_list_prepend(m->ml->regen, m);
	/* TODO: we should manage our own thread stuff, would make cancelling outstanding stuff easier */
	e_thread_put (mail_thread_queued, (EMsg *)m);

	m->ml->regen_timeout_msg = NULL;
	m->ml->regen_timeout_id = 0;

	return FALSE;
}

static void
mail_regen_cancel(MessageList *ml)
{
	/* cancel any outstanding regeneration requests, not we don't clear, they clear themselves */
	if (ml->regen) {
		GList *l = ml->regen;
		
		while (l) {
			struct _mail_msg *mm = l->data;
			
			if (mm->cancel)
				camel_operation_cancel(mm->cancel);
			l = l->next;
		}
	}

	/* including unqueued ones */
	if (ml->regen_timeout_id) {
		g_source_remove(ml->regen_timeout_id);
		ml->regen_timeout_id = 0;
		mail_msg_free((struct _mail_msg *)ml->regen_timeout_msg);
		ml->regen_timeout_msg = NULL;
	}
}

static void
mail_regen_list (MessageList *ml, const char *search, const char *hideexpr, CamelFolderChangeInfo *changes)
{
	struct _regen_list_msg *m;
	GConfClient *gconf;
	
	if (ml->folder == NULL)
		return;

	mail_regen_cancel(ml);

	gconf = mail_config_get_gconf_client ();
	
#ifndef BROKEN_ETREE
	/* this can sometimes crash,so ... */
	
	/* see if we need to goto the child thread at all anyway */
	/* currently the only case is the flat view with updates and no search */
	if (hideexpr == NULL && search == NULL && changes != NULL && !ml->threaded) {
		build_flat_diff(ml, changes);
		camel_folder_change_info_free(changes);
		return;
	}
#endif

	m = mail_msg_new (&regen_list_op, NULL, sizeof (*m));
	m->ml = ml;
	m->search = g_strdup (search);
	m->hideexpr = g_strdup (hideexpr);
	m->changes = changes;
	m->dotree = ml->threaded;
	m->hidedel = ml->hidedeleted;
	m->hidejunk = ml->hidejunk;
	m->thread_subject = gconf_client_get_bool (gconf, "/apps/evolution/mail/display/thread_subject", NULL);
	g_object_ref(ml);
	m->folder = ml->folder;
	camel_object_ref(m->folder);

	if ((!m->hidedel || !m->dotree) && ml->thread_tree) {
		camel_folder_thread_messages_unref(ml->thread_tree);
		ml->thread_tree = NULL;
	} else if (ml->thread_tree) {
		m->tree = ml->thread_tree;
		camel_folder_thread_messages_ref(m->tree);
	}

	/* if we're busy already kick off timeout processing, so normal updates are immediate */
	if (ml->regen == NULL)
		ml_regen_timeout(m);
	else {
		ml->regen_timeout_msg = m;
		ml->regen_timeout_id = g_timeout_add(500, (GSourceFunc)ml_regen_timeout, m);
	}
}


double
message_list_get_scrollbar_position (MessageList *ml)
{
	return gtk_range_get_value ((GtkRange *) ((GtkScrolledWindow *) ml)->vscrollbar);
}


void
message_list_set_scrollbar_position (MessageList *ml, double pos)
{
	gtk_range_set_value ((GtkRange *) ((GtkScrolledWindow *) ml)->vscrollbar, pos);
}
