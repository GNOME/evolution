/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Miguel de Icaza (miguel@ximian.com)
 *      Bertrand Guiheneuf (bg@aful.org)
 *      And just about everyone else in evolution ...
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>
#include <ctype.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gconf/gconf-client.h>

#include "e-util/e-icon-factory.h"
#include "e-util/e-poolv.h"
#include "e-util/e-profile-event.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#include "e-util/gtk-compat.h"

#include "misc/e-selectable.h"

#include "shell/e-shell.h"
#include "shell/e-shell-settings.h"

#include "table/e-cell-checkbox.h"
#include "table/e-cell-hbox.h"
#include "table/e-cell-date.h"
#include "table/e-cell-size.h"
#include "table/e-cell-text.h"
#include "table/e-cell-toggle.h"
#include "table/e-cell-tree.h"
#include "table/e-cell-vbox.h"
#include "table/e-table-sorting-utils.h"
#include "table/e-tree-memory-callbacks.h"
#include "table/e-tree-memory.h"

#include "e-mail-label-list-store.h"
#include "em-utils.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "message-list.h"

#if HAVE_CLUTTER
#include <clutter/clutter.h>
#include <mx/mx.h>
#include <clutter-gtk/clutter-gtk.h>
#endif

/*#define TIMEIT */

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif
#ifdef localtime_r
#undef localtime_r
#endif

/* The gmtime() and localtime() in Microsoft's C library are MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

#define d(x)
#define t(x)

#define MESSAGE_LIST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), MESSAGE_LIST_TYPE, MessageListPrivate))

struct _MLSelection {
	GPtrArray *uids;
	CamelFolder *folder;
	gchar *folder_uri;
};

struct _MessageListPrivate {
	GtkWidget *invisible;	/* 4 selection */

	EShellBackend *shell_backend;

	struct _MLSelection clipboard;
	gboolean destroyed;

	gboolean thread_latest;
	gboolean any_row_changed; /* save state before regen list when this is set to true */

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	/* This aids in automatic message selection. */
	time_t newest_read_date;
	const gchar *newest_read_uid;
	time_t oldest_unread_date;
	const gchar *oldest_unread_uid;

#if HAVE_CLUTTER
	ClutterActor *search_texture;
	ClutterTimeline *timeline;
#endif
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_PASTE_TARGET_LIST,
	PROP_SHELL_BACKEND
};

static gpointer parent_class;

static struct {
	const gchar *target;
	GdkAtom atom;
	guint32 actions;
} ml_drag_info[] = {
	{ "x-uid-list", NULL, GDK_ACTION_MOVE|GDK_ACTION_COPY },
	{ "message/rfc822", NULL, GDK_ACTION_COPY },
	{ "text/uri-list", NULL, GDK_ACTION_COPY },
};

enum {
	DND_X_UID_LIST,		/* x-uid-list */
	DND_MESSAGE_RFC822,	/* message/rfc822 */
	DND_TEXT_URI_LIST	/* text/uri-list */
};

/* What we send */
static GtkTargetEntry ml_drag_types[] = {
	{ (gchar *) "x-uid-list", 0, DND_X_UID_LIST },
	{ (gchar *) "text/uri-list", 0, DND_TEXT_URI_LIST },
};

/* What we accept */
static GtkTargetEntry ml_drop_types[] = {
	{ (gchar *) "x-uid-list", 0, DND_X_UID_LIST },
	{ (gchar *) "message/rfc822", 0, DND_MESSAGE_RFC822 },
	{ (gchar *) "text/uri-list", 0, DND_TEXT_URI_LIST },
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
#define COL_SENDER_EXPANSION     (24.0)
#define COL_SENDER_WIDTH_MIN     (32)

enum {
	NORMALISED_SUBJECT,
	NORMALISED_FROM,
	NORMALISED_TO,
	NORMALISED_LAST
};

/* #define SMART_ADDRESS_COMPARE */

#ifdef SMART_ADDRESS_COMPARE
struct _EMailAddress {
	ENameWestern *wname;
	gchar *address;
};

typedef struct _EMailAddress EMailAddress;
#endif /* SMART_ADDRESS_COMPARE */

static void on_cursor_activated_cmd (ETree *tree, gint row, ETreePath path, gpointer user_data);
static void on_selection_changed_cmd(ETree *tree, MessageList *ml);
static gint on_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, MessageList *list);
static gchar *filter_date (time_t date);
static gchar *filter_size (gint size);

/* note: @changes is owned/freed by the caller */
/*static void mail_do_regenerate_messagelist (MessageList *list, const gchar *search, const gchar *hideexpr, CamelFolderChangeInfo *changes);*/
static void mail_regen_list(MessageList *ml, const gchar *search, const gchar *hideexpr, CamelFolderChangeInfo *changes);
static void mail_regen_cancel(MessageList *ml);

static void clear_info(gchar *key, ETreePath *node, MessageList *ml);

static void	folder_changed			(CamelFolder *folder,
						 CamelFolderChangeInfo *info,
						 MessageList *ml);

enum {
	MESSAGE_SELECTED,
	MESSAGE_LIST_BUILT,
	LAST_SIGNAL
};

static guint message_list_signals[LAST_SIGNAL] = {0, };

static const gchar *status_icons[] = {
	"mail-unread",
	"mail-read",
	"mail-replied",
	"mail-forward",
	"stock_mail-unread-multiple",
	"stock_mail-open-multiple"
};

static const gchar *score_icons[] = {
	"stock_score-lowest",
	"stock_score-lower",
	"stock_score-low",
	"stock_score-normal",
	"stock_score-high",
	"stock_score-higher",
	"stock_score-highest"
};

static const gchar *attachment_icons[] = {
	NULL,  /* empty icon */
	"mail-attachment",
	"stock_new-meeting"
};

static const gchar *flagged_icons[] = {
	NULL,  /* empty icon */
	"emblem-important"
};

static const gchar *followup_icons[] = {
	NULL,  /* empty icon */
	"stock_mail-flag-for-followup",
	"stock_mail-flag-for-followup-done"
};

#ifdef SMART_ADDRESS_COMPARE
static EMailAddress *
e_mail_address_new (const gchar *address)
{
	CamelInternetAddress *cia;
	EMailAddress *new;
	const gchar *name = NULL, *addr = NULL;

	cia = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (cia), address) == -1) {
		g_object_unref (cia);
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

	g_object_unref (cia);

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

static gint
e_mail_address_compare (gconstpointer address1, gconstpointer address2)
{
	const EMailAddress *addr1 = address1;
	const EMailAddress *addr2 = address2;
	gint retval;

	g_return_val_if_fail (addr1 != NULL, 1);
	g_return_val_if_fail (addr2 != NULL, -1);

	if (!addr1->wname && !addr2->wname) {
		/* have to compare addresses, one or both don't have names */
		g_return_val_if_fail (addr1->address != NULL, 1);
		g_return_val_if_fail (addr2->address != NULL, -1);

		return g_ascii_strcasecmp (addr1->address, addr2->address);
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

		return g_ascii_strcasecmp (addr1->address, addr2->address);
	}

	if (!addr1->wname->last)
		return -1;
	if (!addr2->wname->last)
		return 1;

	retval = g_ascii_strcasecmp (addr1->wname->last, addr2->wname->last);
	if (retval)
		return retval;

	/* last names are identical - compare first names */

	if (!addr1->wname->first && !addr2->wname->first)
		return g_ascii_strcasecmp (addr1->address, addr2->address);

	if (!addr1->wname->first)
		return -1;
	if (!addr2->wname->first)
		return 1;

	retval = g_ascii_strcasecmp (addr1->wname->first, addr2->wname->first);
	if (retval)
		return retval;

	return g_ascii_strcasecmp (addr1->address, addr2->address);
}
#endif /* SMART_ADDRESS_COMPARE */

static gint
address_compare (gconstpointer address1, gconstpointer address2, gpointer cmp_cache)
{
#ifdef SMART_ADDRESS_COMPARE
	EMailAddress *addr1, *addr2;
#endif /* SMART_ADDRESS_COMPARE */
	gint retval;

	g_return_val_if_fail (address1 != NULL, 1);
	g_return_val_if_fail (address2 != NULL, -1);

#ifdef SMART_ADDRESS_COMPARE
	addr1 = e_mail_address_new (address1);
	addr2 = e_mail_address_new (address2);
	retval = e_mail_address_compare (addr1, addr2);
	e_mail_address_free (addr1);
	e_mail_address_free (addr2);
#else
	retval = g_ascii_strcasecmp ((gchar *) address1, (gchar *) address2);
#endif /* SMART_ADDRESS_COMPARE */

	return retval;
}

static gchar *
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
static const gchar *
get_message_uid (MessageList *message_list, ETreePath node)
{
	CamelMessageInfo *info;

	g_return_val_if_fail (node != NULL, NULL);
	info = e_tree_memory_node_get_data (E_TREE_MEMORY (message_list->model), node);
	/* correct me if I'm wrong, but this should never be NULL, should it? */
	g_return_val_if_fail (info != NULL, NULL);

	return camel_message_info_uid (info);
}

/* Gets the CamelMessageInfo for the message displayed at the given
 * view row.
 */
static CamelMessageInfo *
get_message_info (MessageList *message_list, ETreePath node)
{
	CamelMessageInfo *info;

	g_return_val_if_fail (node != NULL, NULL);
	info = e_tree_memory_node_get_data (E_TREE_MEMORY (message_list->model), node);
	g_return_val_if_fail (info != NULL, NULL);

	return info;
}

static const gchar *
get_normalised_string (MessageList *message_list, CamelMessageInfo *info, gint col)
{
	const gchar *string, *str;
	gchar *normalised;
	EPoolv *poolv;
	gint index;

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
		g_warning ("Should not be reached\n");
	}

	/* slight optimisation */
	if (string == NULL || string[0] == '\0')
		return "";

	poolv = g_hash_table_lookup (message_list->normalised_hash, camel_message_info_uid (info));
	if (poolv == NULL) {
		poolv = e_poolv_new (NORMALISED_LAST);
		g_hash_table_insert (message_list->normalised_hash, (gchar *) camel_message_info_uid (info), poolv);
	} else {
		str = e_poolv_get (poolv, index);
		if (*str)
			return str;
	}

	if (col == COL_SUBJECT_NORM) {
		const guchar *subject;

		subject = (const guchar *) string;
		while (!g_ascii_strncasecmp ((gchar *)subject, "Re:", 3)) {
			subject += 3;

			/* jump over any spaces */
			while (*subject && isspace ((gint) *subject))
				subject++;
		}

		/* jump over any spaces */
		while (*subject && isspace ((gint) *subject))
			subject++;

		string = (const gchar *) subject;
		normalised = g_utf8_collate_key (string, -1);
	} else {
		/* because addresses require strings, not collate keys */
		normalised = g_strdup (string);
	}

	e_poolv_set (poolv, index, normalised, TRUE);

	return e_poolv_get (poolv, index);
}

static void
clear_selection(MessageList *ml, struct _MLSelection *selection)
{
	if (selection->uids) {
		em_utils_uids_free(selection->uids);
		selection->uids = NULL;
	}
	if (selection->folder) {
		g_object_unref (selection->folder);
		selection->folder = NULL;
	}
	g_free(selection->folder_uri);
	selection->folder_uri = NULL;
}

static ETreePath
ml_search_forward(MessageList *ml, gint start, gint end, guint32 flags, guint32 mask)
{
	ETreePath path;
	gint row;
	CamelMessageInfo *info;
	ETreeTableAdapter *etta;

	etta = e_tree_get_table_adapter (E_TREE (ml));

	for (row = start; row <= end; row++) {
		path = e_tree_table_adapter_node_at_row(etta, row);
		if (path
		    && (info = get_message_info(ml, path))
		    && (camel_message_info_flags(info) & mask) == flags)
			return path;
	}

	return NULL;
}

static ETreePath
ml_search_backward(MessageList *ml, gint start, gint end, guint32 flags, guint32 mask)
{
	ETreePath path;
	gint row;
	CamelMessageInfo *info;
	ETreeTableAdapter *etta;

	etta = e_tree_get_table_adapter (E_TREE (ml));

	for (row = start; row >= end; row--) {
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
	gint row, count;
	ETreeTableAdapter *etta;

	etta = e_tree_get_table_adapter (E_TREE (ml));

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
	ETree *tree;
	ETreeTableAdapter *etta;
	ETreeSelectionModel *etsm;

	tree = E_TREE (ml);
	etta = e_tree_get_table_adapter (tree);
	etsm = (ETreeSelectionModel *) e_tree_get_selection_model (tree);

	g_free(ml->cursor_uid);
	ml->cursor_uid = NULL;

	e_tree_table_adapter_show_node (etta, path);
	e_tree_set_cursor (tree, path);
	e_tree_selection_model_select_single_path (etsm, path);
}

/**
 * message_list_select:
 * @message_list: a MessageList
 * @direction: the direction to search in
 * @flags: a set of flag values
 * @mask: a mask for comparing against @flags
 *
 * This moves the message list selection to a suitable row. @flags and
 * @mask combine to specify what constitutes a suitable row. @direction is
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

		/* This function is usually called in response to a key
		 * press, so grab focus if the message list is visible. */
		if (gtk_widget_get_visible (GTK_WIDGET (ml)))
			gtk_widget_grab_focus (GTK_WIDGET (ml));

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
message_list_select_uid (MessageList *message_list,
                         const gchar *uid,
                         gboolean with_fallback)
{
	MessageListPrivate *priv;
	GHashTable *uid_nodemap;
	ETreePath node = NULL;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	priv = message_list->priv;
	uid_nodemap = message_list->uid_nodemap;

	if (message_list->folder == NULL)
		return;

	/* Try to find the requested message UID. */
	if (uid != NULL)
		node = g_hash_table_lookup (uid_nodemap, uid);

	/* If we're busy or waiting to regenerate the message list, cache
	 * the UID so we can try again when we're done.  Otherwise if the
	 * requested message UID was not found and 'with_fallback' is set,
	 * try a couple fallbacks:
	 *
	 * 1) Oldest unread message in the list, by date received.
	 * 2) Newest read message in the list, by date received.
	 */
	if (message_list->regen || message_list->regen_timeout_id) {
		g_free (message_list->pending_select_uid);
		message_list->pending_select_uid = g_strdup (uid);
		message_list->pending_select_fallback = with_fallback;
	} else if (with_fallback) {
		if (node == NULL && priv->oldest_unread_uid != NULL)
			node = g_hash_table_lookup (
				uid_nodemap, priv->oldest_unread_uid);
		if (node == NULL && priv->newest_read_uid != NULL)
			node = g_hash_table_lookup (
				uid_nodemap, priv->newest_read_uid);
	}

	if (node) {
		ETree *tree;
		ETreePath old_cur;

		tree = E_TREE (message_list);
		old_cur = e_tree_get_cursor (tree);

		/* This will emit a changed signal that we'll pick up */
		e_tree_set_cursor (tree, node);

		if (old_cur == node)
			g_signal_emit (
				message_list,
				message_list_signals[MESSAGE_SELECTED],
				0, message_list->cursor_uid);
	} else {
		g_free (message_list->cursor_uid);
		message_list->cursor_uid = NULL;
		g_signal_emit (
			message_list,
			message_list_signals[MESSAGE_SELECTED],
			0, NULL);
	}
}

void
message_list_select_next_thread (MessageList *ml)
{
	ETreePath node;
	ETreeTableAdapter *etta;
	gint i, count, row;

	etta = e_tree_get_table_adapter (E_TREE (ml));

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

static gboolean
message_list_select_all_timeout_cb (MessageList *message_list)
{
	ESelectionModel *etsm;

	etsm = e_tree_get_selection_model (E_TREE (message_list));

	e_selection_model_select_all (etsm);

	return FALSE;
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
	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list->threaded) {
		/* XXX The timeout below is added so that the execution
		 *     thread to expand all conversation threads would
		 *     have completed.  The timeout 505 is just to ensure
		 *     that the value is a small delta more than the
		 *     timeout value in mail_regen_list(). */
		g_timeout_add (
			505, (GSourceFunc)
			message_list_select_all_timeout_cb,
			message_list);
	} else
		/* If there is no threading, just select all immediately. */
		message_list_select_all_timeout_cb (message_list);
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
select_thread (MessageList *message_list, void (*selector)(ETreePath, gpointer))
{
	ETree *tree;
	ETreeSelectionModel *etsm;
	thread_select_info_t tsi;

	tsi.ml = message_list;
	tsi.paths = g_ptr_array_new ();

	tree = E_TREE (message_list);
	etsm = (ETreeSelectionModel *) e_tree_get_selection_model (tree);

	e_tree_selected_path_foreach (tree, selector, &tsi);

	e_tree_selection_model_select_paths (etsm, tsi.paths);

	g_ptr_array_free (tsi.paths, TRUE);
}

static void
thread_select_foreach (ETreePath path, gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	ETreeModel *model = tsi->ml->model;
	ETreePath node, last;

	node = path;

	do {
		last = node;
		node = e_tree_model_node_get_parent (model, node);
	} while (!e_tree_model_node_is_root (model, node));

	g_ptr_array_add (tsi->paths, last);

	e_tree_model_node_traverse (model, last, select_node, tsi);
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
	select_thread (message_list, thread_select_foreach);
}

static void
subthread_select_foreach (ETreePath path, gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	ETreeModel *model = tsi->ml->model;

	e_tree_model_node_traverse (model, path, select_node, tsi);
}

/**
 * message_list_select_subthread:
 * @message_list: Message List widget
 *
 * Selects all messages in the current subthread (based on cursor).
 **/
void
message_list_select_subthread (MessageList *message_list)
{
	select_thread (message_list, subthread_select_foreach);
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

	etsm = e_tree_get_selection_model (E_TREE (message_list));

	e_selection_model_invert_selection (etsm);
}

void
message_list_copy(MessageList *ml, gboolean cut)
{
	MessageListPrivate *p = ml->priv;
	GPtrArray *uids;

	clear_selection(ml, &p->clipboard);

	uids = message_list_get_selected(ml);

	if (uids->len > 0) {
		if (cut) {
			gint i;

			camel_folder_freeze(ml->folder);
			for (i=0;i<uids->len;i++)
				camel_folder_set_message_flags(ml->folder, uids->pdata[i],
							       CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED,
							       CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED);

			camel_folder_thaw(ml->folder);
		}

		p->clipboard.uids = uids;
		p->clipboard.folder = ml->folder;
		g_object_ref (p->clipboard.folder);
		p->clipboard.folder_uri = g_strdup(ml->folder_uri);
		gtk_selection_owner_set(p->invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
	} else {
		em_utils_uids_free(uids);
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
static gint
ml_column_count (ETreeModel *etm, gpointer data)
{
	return COL_LAST;
}

/*
 * SimpleTableModel::has_save_id
 */
static gboolean
ml_has_save_id (ETreeModel *etm, gpointer data)
{
	return TRUE;
}

/*
 * SimpleTableModel::get_save_id
 */
static gchar *
ml_get_save_id (ETreeModel *etm, ETreePath path, gpointer data)
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
ml_has_get_node_by_id (ETreeModel *etm, gpointer data)
{
	return TRUE;
}

/*
 * SimpleTableModel::get_save_id
 */
static ETreePath
ml_get_node_by_id (ETreeModel *etm, const gchar *save_id, gpointer data)
{
	MessageList *ml;

	ml = data;

	if (!strcmp (save_id, "root"))
		return e_tree_model_get_root (etm);

	return g_hash_table_lookup(ml->uid_nodemap, save_id);
}

static gpointer
ml_duplicate_value (ETreeModel *etm, gint col, gconstpointer value, gpointer data)
{
	switch (col) {
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
		return (gpointer) value;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_SENDER:
	case COL_RECIPIENTS:
	case COL_MIXED_SENDER:
	case COL_MIXED_RECIPIENTS:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
	case COL_LABELS:
		return g_strdup (value);
	default:
		g_warning ("This shouldn't be reached\n");
	}
	return NULL;
}

static void
ml_free_value (ETreeModel *etm, gint col, gpointer value, gpointer data)
{
	switch (col) {
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
	case COL_FROM_NORM:
	case COL_SUBJECT_NORM:
	case COL_TO_NORM:
	case COL_SUBJECT_TRIMMED:
	case COL_COLOUR:
		break;

	case COL_FROM:
	case COL_SUBJECT:
	case COL_TO:
	case COL_FOLLOWUP_FLAG:
	case COL_LOCATION:
	case COL_SENDER:
	case COL_RECIPIENTS:
	case COL_MIXED_SENDER:
	case COL_MIXED_RECIPIENTS:
	case COL_LABELS:
		g_free (value);
		break;
	default:
		g_warning ("%s: This shouldn't be reached (col:%d)", G_STRFUNC, col);
	}
}

static gpointer
ml_initialize_value (ETreeModel *etm, gint col, gpointer data)
{
	switch (col) {
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
	case COL_SENDER:
	case COL_RECIPIENTS:
	case COL_MIXED_SENDER:
	case COL_MIXED_RECIPIENTS:
	case COL_LABELS:
		return g_strdup ("");
	default:
		g_warning ("This shouldn't be reached\n");
	}

	return NULL;
}

static gboolean
ml_value_is_empty (ETreeModel *etm, gint col, gconstpointer value, gpointer data)
{
	switch (col) {
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
	case COL_SENDER:
	case COL_RECIPIENTS:
	case COL_MIXED_SENDER:
	case COL_MIXED_RECIPIENTS:
	case COL_LABELS:
		return !(value && *(gchar *)value);
	default:
		g_warning ("This shouldn't be reached\n");
		return FALSE;
	}
}

static const gchar *status_map[] = {
	N_("Unseen"),
	N_("Seen"),
	N_("Answered"),
	N_("Forwarded"),
	N_("Multiple Unseen Messages"),
	N_("Multiple Messages"),
};

static const gchar *score_map[] = {
	N_("Lowest"),
	N_("Lower"),
	N_("Low"),
	N_("Normal"),
	N_("High"),
	N_("Higher"),
	N_("Highest"),
};

static gchar *
ml_value_to_string (ETreeModel *etm, gint col, gconstpointer value, gpointer data)
{
	guint i;

	switch (col) {
	case COL_MESSAGE_STATUS:
		i = GPOINTER_TO_UINT(value);
		if (i > 5)
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
		return g_strdup_printf ("%u", GPOINTER_TO_UINT(value));

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
	case COL_SENDER:
	case COL_RECIPIENTS:
	case COL_MIXED_SENDER:
	case COL_MIXED_RECIPIENTS:
	case COL_LABELS:
		return g_strdup (value);
	default:
		g_warning ("This shouldn't be reached\n");
		return NULL;
	}
}

static GdkPixbuf *
ml_tree_icon_at (ETreeModel *etm, ETreePath path, gpointer model_data)
{
	/* we dont really need an icon ... */
	return NULL;
}

static void
for_node_and_subtree_if_collapsed (MessageList *ml, ETreePath node, CamelMessageInfo *mi,
		ETreePathFunc func, gpointer data)
{
	ETreeModel *etm = ml->model;
	ETreePath child;

	func (NULL, (ETreePath) mi, data);

	if (!node)
		return;

	child = e_tree_model_node_get_first_child (etm, node);
	if (child && !e_tree_node_is_expanded (E_TREE (ml), node))
		e_tree_model_node_traverse (etm, node, func, data);
}

static gboolean
unread_foreach (ETreeModel *etm, ETreePath node, gpointer data)
{
	gboolean *saw_unread = data;
	CamelMessageInfo *info;

	if (!etm)
		info = (CamelMessageInfo *)node;
	else
		info = e_tree_memory_node_get_data ((ETreeMemory *)etm, node);
	g_return_val_if_fail (info != NULL, FALSE);

	if (!(camel_message_info_flags (info) & CAMEL_MESSAGE_SEEN))
		*saw_unread = TRUE;

	return FALSE;
}

struct LatestData {
	gboolean sent;
	time_t latest;
};

static gboolean
latest_foreach (ETreeModel *etm, ETreePath node, gpointer data)
{
	struct LatestData *ld = data;
	CamelMessageInfo *info;
	time_t date;

	if (!etm)
		info = (CamelMessageInfo *)node;
	else
		info = e_tree_memory_node_get_data ((ETreeMemory *)etm, node);
	g_return_val_if_fail (info != NULL, FALSE);

	date = ld->sent ? camel_message_info_date_sent (info)
			: camel_message_info_date_received (info);

	if (ld->latest == 0 || date > ld->latest)
		ld->latest = date;

	return FALSE;
}

static gchar *
sanitize_recipients (const gchar *string)
{
	GString     *gstring;
	gboolean     quoted = FALSE;
	const gchar *p;
	GString *recipients = g_string_new ("");
	gchar *single_add;
	gchar **name;

	if (!string || !*string)
		return (gchar *) "";

	gstring = g_string_new ("");

	for (p = string; *p; p = g_utf8_next_char (p)) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"')
			quoted = ~quoted;
		else if (c == ',' && !quoted) {
			single_add = g_string_free (gstring, FALSE);
			name = g_strsplit(single_add,"<",2);
			g_string_append (recipients, *name);
			g_string_append (recipients, ",");
			g_free (single_add);
			g_strfreev (name);
			gstring = g_string_new ("");
			continue;
		}

		g_string_append_unichar (gstring, c);
	}

	single_add = g_string_free (gstring, FALSE);
	name = g_strsplit(single_add,"<",2);
	g_string_append (recipients, *name);
	g_free (single_add);
	g_strfreev (name);

	return g_string_free (recipients, FALSE);
}

struct LabelsData {
	EMailLabelListStore *store;
	GHashTable *labels_tag2iter;
};

static void
add_label_if_known (struct LabelsData *ld, const gchar *tag)
{
	GtkTreeIter label_defn;

	if (e_mail_label_list_store_lookup (ld->store, tag, &label_defn)) {
		g_hash_table_insert (ld->labels_tag2iter,
			/* Should be the same as the "tag" arg */
			e_mail_label_list_store_get_tag (ld->store, &label_defn),
			gtk_tree_iter_copy (&label_defn));
	}
}

static gboolean
add_all_labels_foreach (ETreeModel *etm, ETreePath node, gpointer data)
{
	struct LabelsData *ld = data;
	CamelMessageInfo *msg_info;
	const gchar *old_label;
	gchar *new_label;
	const CamelFlag *flag;

	if (!etm)
		msg_info = (CamelMessageInfo *)node;
	else
		msg_info = e_tree_memory_node_get_data ((ETreeMemory *)etm, node);
	g_return_val_if_fail (msg_info != NULL, FALSE);

	for (flag = camel_message_info_user_flags (msg_info); flag; flag = flag->next)
		add_label_if_known (ld, flag->name);

	old_label = camel_message_info_user_tag (msg_info, "label");
	if (old_label != NULL) {
		/* Convert old-style labels ("<name>") to "$Label<name>". */
		new_label = g_alloca (strlen (old_label) + 10);
		g_stpcpy (g_stpcpy (new_label, "$Label"), old_label);

		add_label_if_known (ld, new_label);
	}

	return FALSE;
}

static EMailLabelListStore *
ml_get_label_list_store (MessageList *message_list)
{
	EShellBackend *shell_backend;
	EShell *shell;
	EShellSettings *shell_settings;
	EMailLabelListStore *store;

	shell_backend = message_list_get_shell_backend (message_list);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);
	store = e_shell_settings_get_object (shell_settings, "mail-label-list-store");

	return store;
}

static const gchar *
get_trimmed_subject (CamelMessageInfo *info)
{
	const gchar *subject;
	const gchar *mlist;
	gint mlist_len = 0;
	gboolean found_mlist;

	subject = camel_message_info_subject (info);
	if (!subject || !*subject)
		return subject;

	mlist = camel_message_info_mlist (info);

	if (mlist && *mlist) {
		const gchar *mlist_end;

		mlist_end = strchr (mlist, '@');
		if (mlist_end)
			mlist_len = mlist_end - mlist;
		else
			mlist_len = strlen (mlist);
	}

	do {
		found_mlist = FALSE;

		while (!g_ascii_strncasecmp ((gchar *) subject, "Re:", 3)) {
			subject += 3;

			/* jump over any spaces */
			while (*subject && isspace ((gint) *subject))
				subject++;
		}

		if (mlist_len &&
		    *subject == '[' &&
		    !g_ascii_strncasecmp ((gchar *) subject + 1, mlist, mlist_len) &&
		    subject[1 + mlist_len] == ']') {
			subject += 1 + mlist_len + 1;  /* jump over "[mailing-list]" */
			found_mlist = TRUE;

			/* jump over any spaces */
			while (*subject && isspace ((gint) *subject))
				subject++;
		}
	} while (found_mlist);

	/* jump over any spaces */
	while (*subject && isspace ((gint) *subject))
		subject++;

	return subject;
}

static gpointer
ml_tree_value_at_ex (ETreeModel *etm,
                     ETreePath path,
                     gint col,
                     CamelMessageInfo *msg_info,
                     MessageList *message_list)
{
	const gchar *str;
	guint32 flags;

	g_return_val_if_fail (msg_info != NULL, NULL);

	switch (col) {
	case COL_MESSAGE_STATUS:
		flags = camel_message_info_flags(msg_info);
		if (flags & CAMEL_MESSAGE_ANSWERED)
			return GINT_TO_POINTER (2);
		else if (flags & CAMEL_MESSAGE_FORWARDED)
			return GINT_TO_POINTER (3);
		else if (flags & CAMEL_MESSAGE_SEEN)
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);
	case COL_FLAGGED:
		return GINT_TO_POINTER ((camel_message_info_flags(msg_info) & CAMEL_MESSAGE_FLAGGED) != 0);
	case COL_SCORE: {
		const gchar *tag;
		gint score = 0;

		tag = camel_message_info_user_tag(msg_info, "score");
		if (tag)
			score = atoi (tag);

		return GINT_TO_POINTER (score);
	}
	case COL_FOLLOWUP_FLAG_STATUS: {
		const gchar *tag, *cmp;

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
		const gchar *tag;
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
		return (gpointer)(str ? str : "");
	case COL_ATTACHMENT:
		if (camel_message_info_user_flag (msg_info, "$has_cal"))
			return GINT_TO_POINTER (2);
		return GINT_TO_POINTER ((camel_message_info_flags(msg_info) & CAMEL_MESSAGE_ATTACHMENTS) != 0);
	case COL_FROM:
		str = camel_message_info_from (msg_info);
		return (gpointer)(str ? str : "");
	case COL_FROM_NORM:
		return (gpointer) get_normalised_string (message_list, msg_info, col);
	case COL_SUBJECT:
		str = camel_message_info_subject (msg_info);
		return (gpointer)(str ? str : "");
	case COL_SUBJECT_TRIMMED:
		str = get_trimmed_subject (msg_info);
		return (gpointer)(str ? str : "");
	case COL_SUBJECT_NORM:
		return (gpointer) get_normalised_string (message_list, msg_info, col);
	case COL_SENT: {
		struct LatestData ld;
		ld.sent = TRUE;
		ld.latest = 0;

		for_node_and_subtree_if_collapsed (message_list, path, msg_info, latest_foreach, &ld);

		return GINT_TO_POINTER (ld.latest);
	}
	case COL_RECEIVED: {
		struct LatestData ld;
		ld.sent = FALSE;
		ld.latest = 0;

		for_node_and_subtree_if_collapsed (message_list, path, msg_info, latest_foreach, &ld);

		return GINT_TO_POINTER (ld.latest);
	}
	case COL_TO:
		str = camel_message_info_to (msg_info);
		return (gpointer)(str ? str : "");
	case COL_TO_NORM:
		return (gpointer) get_normalised_string (message_list, msg_info, col);
	case COL_SIZE:
		return GINT_TO_POINTER (camel_message_info_size(msg_info));
	case COL_DELETED:
		return GINT_TO_POINTER ((camel_message_info_flags(msg_info) & CAMEL_MESSAGE_DELETED) != 0);
	case COL_UNREAD: {
		gboolean saw_unread = FALSE;

		for_node_and_subtree_if_collapsed (message_list, path, msg_info, unread_foreach, &saw_unread);

		return GINT_TO_POINTER (saw_unread);
	}
	case COL_COLOUR: {
		const gchar *colour, *due_by, *completed, *followup;

		/* Priority: colour tag; label tag; important flag; due-by tag */

		/* This is astonisngly poorly written code */

		/* To add to the woes, what color to show when the user choose multiple labels ?
		Don't say that I need to have the new labels[with subject] column visible always */

		colour = camel_message_info_user_tag(msg_info, "color");
		due_by = camel_message_info_user_tag(msg_info, "due-by");
		completed = camel_message_info_user_tag(msg_info, "completed-on");
		followup = camel_message_info_user_tag(msg_info, "follow-up");
		if (colour == NULL) {
			/* Get all applicable labels. */
			struct LabelsData ld;

			ld.store = ml_get_label_list_store (message_list);
			ld.labels_tag2iter = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);
			for_node_and_subtree_if_collapsed (message_list, path, msg_info, add_all_labels_foreach, &ld);

			if (g_hash_table_size (ld.labels_tag2iter) == 1) {
				GHashTableIter iter;
				GtkTreeIter *label_defn;
				GdkColor colour_val;
				gchar *colour_alloced;

				/* Extract the single label from the hashtable. */
				g_hash_table_iter_init (&iter, ld.labels_tag2iter);
				g_hash_table_iter_next (&iter, NULL, (gpointer *) &label_defn);

				e_mail_label_list_store_get_color (ld.store, label_defn, &colour_val);

				/* XXX Hack to avoid returning an allocated string. */
				colour_alloced = gdk_color_to_string (&colour_val);
				colour = g_intern_string (colour_alloced);
				g_free (colour_alloced);
			} else if (camel_message_info_flags(msg_info) & CAMEL_MESSAGE_FLAGGED) {
				/* FIXME: extract from the important.xpm somehow. */
				colour = "#A7453E";
			} else if (((followup && *followup) || (due_by && *due_by)) && !(completed && *completed)) {
				time_t now = time (NULL);

				if ((followup && *followup) || now >= camel_header_decode_date (due_by, NULL))
					colour = "#A7453E";
			}

			g_hash_table_destroy (ld.labels_tag2iter);
			g_object_unref (ld.store);
		}

		return (gpointer) colour;
	}
	case COL_LOCATION: {
		/* Fixme : freeing memory stuff (mem leaks) */
		CamelFolder *folder;
		CamelURL *curl;
		EAccount *account;
		gchar *location = NULL;
		gchar *euri, *url;

		if (CAMEL_IS_VEE_FOLDER(message_list->folder)) {
			folder = camel_vee_folder_get_location((CamelVeeFolder *)message_list->folder, (CamelVeeMessageInfo *)msg_info, NULL);
		} else {
			folder = message_list->folder;
		}

		url = mail_tools_folder_to_url (folder);
		euri = em_uri_from_camel(url);

		account = mail_config_get_account_by_source_url (url);

		if (account) {
			curl = camel_url_new (url, NULL);
			location = g_strconcat (account->name, ":", curl->path, NULL);
		} else {
			/* Local account */
			euri = em_uri_from_camel(url);
			curl = camel_url_new (euri, NULL);
			if (curl->host && !strcmp(curl->host, "local") && curl->user && !strcmp(curl->user, "local"))
				location = g_strconcat (_("On This Computer"), ":",curl->path, NULL);
		}

		camel_url_free (curl);
		g_free (url);
		g_free (euri);

		return location;
	}
	case COL_MIXED_RECIPIENTS:
	case COL_RECIPIENTS:{
		str = camel_message_info_to (msg_info);

		return sanitize_recipients(str);
	}
	case COL_MIXED_SENDER:
	case COL_SENDER:{
		gchar **sender_name = NULL;
		str = camel_message_info_from (msg_info);
		if (str && str[0] != '\0') {
			gchar *res;
			sender_name = g_strsplit (str,"<",2);
			res = g_strdup (*sender_name);
			g_strfreev (sender_name);
			return (gpointer)(res);
		}
		else
			return (gpointer)("");
	}
	case COL_LABELS:{
		struct LabelsData ld;
		GString *result = g_string_new ("");

		ld.store = ml_get_label_list_store (message_list);
		ld.labels_tag2iter = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);
		for_node_and_subtree_if_collapsed (message_list, path, msg_info, add_all_labels_foreach, &ld);

		if (g_hash_table_size (ld.labels_tag2iter) > 0) {
			GHashTableIter iter;
			GtkTreeIter *label_defn;

			g_hash_table_iter_init (&iter, ld.labels_tag2iter);
			while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &label_defn)) {
				gchar *label_name, *label_name_clean;

				if (result->len > 0)
					g_string_append (result, ", ");

				label_name = e_mail_label_list_store_get_name (ld.store, label_defn);
				label_name_clean = e_str_without_underscores (label_name);

				g_string_append (result, label_name_clean);

				g_free(label_name_clean);
				g_free(label_name);
			}
		}

		g_hash_table_destroy (ld.labels_tag2iter);
		g_object_unref (ld.store);
		return (gpointer) g_string_free (result, FALSE);
	}
	default:
		g_warning ("%s: This shouldn't be reached (col:%d)", G_STRFUNC, col);
		return NULL;
	}
}

static gpointer
ml_tree_value_at (ETreeModel *etm, ETreePath path, gint col, gpointer model_data)
{
	MessageList *message_list = model_data;
	CamelMessageInfo *msg_info;

	if (e_tree_model_node_is_root (etm, path))
		return NULL;

	/* retrieve the message information array */
	msg_info = e_tree_memory_node_get_data (E_TREE_MEMORY(etm), path);
	g_return_val_if_fail (msg_info != NULL, NULL);

	return ml_tree_value_at_ex (etm, path, col, msg_info, message_list);
}

static gpointer
ml_tree_sort_value_at (ETreeModel *etm, ETreePath path, gint col, gpointer model_data)
{
	MessageList *message_list = model_data;
	struct LatestData ld;

	if (!(col == COL_SENT || col == COL_RECEIVED))
		return ml_tree_value_at (etm, path, col, model_data);

	if (e_tree_model_node_is_root (etm, path))
		return NULL;

	ld.sent = (col == COL_SENT);
	ld.latest = 0;

	latest_foreach (etm, path, &ld);
	if (message_list->priv->thread_latest)
		e_tree_model_node_traverse (etm, path, latest_foreach, &ld);

	return GINT_TO_POINTER (ld.latest);
}

static void
ml_tree_set_value_at (ETreeModel *etm, ETreePath path, gint col,
		      gconstpointer val, gpointer model_data)
{
	g_warning ("This shouldn't be reached\n");
}

static gboolean
ml_tree_is_cell_editable (ETreeModel *etm, ETreePath path, gint col, gpointer model_data)
{
	return FALSE;
}

static gchar *
filter_date (time_t date)
{
	time_t nowdate = time(NULL);
	time_t yesdate;
	struct tm then, now, yesterday;
	gchar buf[26];
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
		gint i;
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

static ECell * create_composite_cell (gint col)
{
	ECell *cell_vbox, *cell_hbox, *cell_sub, *cell_date, *cell_from, *cell_tree, *cell_attach;
	GConfClient *gconf;
	gchar *fixed_name = NULL;
	gboolean show_email;
	gint alt_col = (col == COL_FROM) ? COL_SENDER : COL_RECIPIENTS;
	gboolean same_font = FALSE;

	gconf = mail_config_get_gconf_client ();
	show_email = gconf_client_get_bool (gconf, "/apps/evolution/mail/display/show_email", NULL);
	same_font = gconf_client_get_bool (gconf, "/apps/evolution/mail/display/vertical_view_fonts", NULL);
	if (!same_font)
		fixed_name = gconf_client_get_string (gconf, "/desktop/gnome/interface/monospace_font_name", NULL);

	cell_vbox = e_cell_vbox_new ();

	cell_hbox = e_cell_hbox_new ();

	/* Exclude the meeting icon. */
	cell_attach = e_cell_toggle_new (attachment_icons, 2);

	cell_date = e_cell_date_new (NULL, GTK_JUSTIFY_RIGHT);
	e_cell_date_set_format_component (E_CELL_DATE (cell_date), "mail");
	g_object_set (G_OBJECT (cell_date),
		      "bold_column", COL_UNREAD,
		      "color_column", COL_COLOUR,
		      NULL);

	cell_from = e_cell_text_new(NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell_from),
		      "bold_column", COL_UNREAD,
		      "color_column", COL_COLOUR,
		      NULL);

	e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_from, show_email ? col : alt_col, 68);
	e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_attach, COL_ATTACHMENT, 5);
	e_cell_hbox_append (E_CELL_HBOX (cell_hbox), cell_date, COL_SENT, 27);

	cell_sub = e_cell_text_new(fixed_name? fixed_name:NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell_sub),
/*		      "bold_column", COL_UNREAD, */
		      "color_column", COL_COLOUR,
		      NULL);
	cell_tree = e_cell_tree_new (NULL, NULL, TRUE, cell_sub);
	e_cell_vbox_append (E_CELL_VBOX (cell_vbox), cell_hbox, COL_FROM);
	e_cell_vbox_append (E_CELL_VBOX (cell_vbox), cell_tree, COL_SUBJECT);

	g_object_set_data (G_OBJECT (cell_vbox), "cell_date", cell_date);
	g_object_set_data (G_OBJECT (cell_vbox), "cell_sub", cell_sub);
	g_object_set_data (G_OBJECT (cell_vbox), "cell_from", cell_from);

	g_free (fixed_name);

	return cell_vbox;
}

static void
composite_cell_set_strike_col (ECell *cell, gint col)
{
	g_object_set (G_OBJECT (g_object_get_data(G_OBJECT (cell), "cell_date")),  "strikeout_column", col, NULL);
	g_object_set (G_OBJECT (g_object_get_data(G_OBJECT (cell), "cell_from")),  "strikeout_column", col, NULL);
}

static ETableExtras *
message_list_create_extras (void)
{
	ETableExtras *extras;
	ECell *cell;

	extras = e_table_extras_new ();
	e_table_extras_add_icon_name (extras, "status", "mail-unread");
	e_table_extras_add_icon_name (extras, "score", "stock_score-higher");
	e_table_extras_add_icon_name (extras, "attachment", "mail-attachment");
	e_table_extras_add_icon_name (extras, "flagged", "emblem-important");
	e_table_extras_add_icon_name (extras, "followup", "stock_mail-flag-for-followup");

	e_table_extras_add_compare (extras, "address_compare", address_compare);

	cell = e_cell_toggle_new (
		status_icons, G_N_ELEMENTS (status_icons));
	e_table_extras_add_cell (extras, "render_message_status", cell);

	cell = e_cell_toggle_new (
		attachment_icons, G_N_ELEMENTS (attachment_icons));
	e_table_extras_add_cell (extras, "render_attachment", cell);

	cell = e_cell_toggle_new (
		flagged_icons, G_N_ELEMENTS (flagged_icons));
	e_table_extras_add_cell (extras, "render_flagged", cell);

	cell = e_cell_toggle_new (
		followup_icons, G_N_ELEMENTS (followup_icons));
	e_table_extras_add_cell (extras, "render_flag_status", cell);

	cell = e_cell_toggle_new (
		score_icons, G_N_ELEMENTS (score_icons));
	e_table_extras_add_cell (extras, "render_score", cell);

	/* date cell */
	cell = e_cell_date_new (NULL, GTK_JUSTIFY_LEFT);
	e_cell_date_set_format_component (E_CELL_DATE (cell), "mail");
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

	/* Composite cell for wide view */
	cell = create_composite_cell (COL_FROM);
	e_table_extras_add_cell (extras, "render_composite_from", cell);

	cell = create_composite_cell (COL_TO);
	e_table_extras_add_cell (extras, "render_composite_to", cell);

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "mail");

	return extras;
}

static void
save_tree_state(MessageList *ml)
{
	gchar *filename;

	if (ml->folder == NULL)
		return;

	filename = mail_config_folder_to_cachename(ml->folder, "et-expanded-");
	e_tree_save_expanded_state (E_TREE (ml), filename);
	g_free(filename);

	ml->priv->any_row_changed = FALSE;
}

static void
load_tree_state (MessageList *ml, xmlDoc *expand_state)
{
	if (ml->folder == NULL)
		return;

	if (expand_state) {
		e_tree_load_expanded_state_xml (E_TREE (ml), expand_state);
	} else {
		gchar *filename;

		filename = mail_config_folder_to_cachename (ml->folder, "et-expanded-");
		e_tree_load_expanded_state (E_TREE (ml), filename);
		g_free (filename);
	}

	ml->priv->any_row_changed = FALSE;
}

void
message_list_save_state (MessageList *ml)
{
	save_tree_state (ml);
}

static void
message_list_setup_etree (MessageList *message_list, gboolean outgoing)
{
	/* build the spec based on the folder, and possibly from a saved file */
	/* otherwise, leave default */
	if (message_list->folder) {
		CamelStore *parent_store;
		gchar *path;
		gchar *name;
		gint data = 1;
		struct stat st;
		ETableItem *item;

		item = e_tree_get_item (E_TREE (message_list));

		g_object_set (message_list, "uniform_row_height", TRUE, NULL);

		parent_store = camel_folder_get_parent_store (message_list->folder);
		name = camel_service_get_name (CAMEL_SERVICE (parent_store), TRUE);
		d(printf ("folder name is '%s'\n", name));

		path = mail_config_folder_to_cachename (message_list->folder, "et-expanded-");
		g_object_set_data (G_OBJECT (((GnomeCanvasItem *) item)->canvas), "freeze-cursor", &data);

		if (path && g_stat (path, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode)) {
			/* build based on saved file */
			e_tree_load_expanded_state (E_TREE (message_list), path);
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
		d(printf("setting text/plain selection for uids\n"));
		em_utils_selection_set_mailbox(data, selection->folder, selection->uids);
	} else {
		/* x-uid-list */
		d(printf("setting x-uid-list selection for uids\n"));
		em_utils_selection_set_uidlist(data, selection->folder_uri, selection->uids);
	}
}

static gboolean
ml_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, MessageList *ml)
{
	MessageListPrivate *p = ml->priv;

	clear_selection(ml, &p->clipboard);

	return TRUE;
}

static void
ml_selection_received (GtkWidget *widget,
                       GtkSelectionData *selection_data,
                       guint time,
                       MessageList *ml)
{
	GdkAtom target;

	target = gtk_selection_data_get_target (selection_data);

	if (target != gdk_atom_intern ("x-uid-list", FALSE)) {
		d(printf("Unknown selection received by message-list\n"));
		return;
	}

	em_utils_selection_get_uidlist (
		selection_data, ml->folder, FALSE, NULL);
}

static void
ml_tree_drag_data_get (ETree *tree, gint row, ETreePath path, gint col,
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

	em_utils_uids_free(uids);
}

/* TODO: merge this with the folder tree stuff via empopup targets */
/* Drop handling */
struct _drop_msg {
	MailMsg base;

	GdkDragContext *context;

	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	CamelFolder *folder;

	guint32 action;
	guint info;

	guint move:1;
	guint moved:1;
	guint aborted:1;
};

static gchar *
ml_drop_async_desc (struct _drop_msg *m)
{
	const gchar *full_name;

	full_name = camel_folder_get_full_name (m->folder);

	if (m->move)
		return g_strdup_printf(_("Moving messages into folder %s"), full_name);
	else
		return g_strdup_printf(_("Copying messages into folder %s"), full_name);
}

static void
ml_drop_async_exec (struct _drop_msg *m)
{
	switch (m->info) {
	case DND_X_UID_LIST:
		em_utils_selection_get_uidlist (
			m->selection, m->folder,
			m->action == GDK_ACTION_MOVE,
			&m->base.error);
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
ml_drop_async_done (struct _drop_msg *m)
{
	gboolean success, delete;

	/* ?? */
	if (m->aborted) {
		success = FALSE;
		delete = FALSE;
	} else {
		success = (m->base.error == NULL);
		delete = success && m->move && !m->moved;
	}

	gtk_drag_finish(m->context, success, delete, GDK_CURRENT_TIME);
}

static void
ml_drop_async_free (struct _drop_msg *m)
{
	g_object_unref (m->context);
	g_object_unref (m->folder);
	gtk_selection_data_free (m->selection);
}

static MailMsgInfo ml_drop_async_info = {
	sizeof (struct _drop_msg),
	(MailMsgDescFunc) ml_drop_async_desc,
	(MailMsgExecFunc) ml_drop_async_exec,
	(MailMsgDoneFunc) ml_drop_async_done,
	(MailMsgFreeFunc) ml_drop_async_free
};

static void
ml_drop_action(struct _drop_msg *m)
{
	m->move = m->action == GDK_ACTION_MOVE;
	mail_msg_unordered_push (m);
}

static void
ml_tree_drag_data_received (ETree *tree,
                            gint row,
                            ETreePath path,
                            gint col,
                            GdkDragContext *context,
                            gint x,
                            gint y,
                            GtkSelectionData *selection_data,
                            guint info,
                            guint time,
                            MessageList *ml)
{
	struct _drop_msg *m;

	if (ml->folder == NULL)
		return;

	if (gtk_selection_data_get_data (selection_data) == NULL)
		return;

	if (gtk_selection_data_get_length (selection_data) == -1)
		return;

	m = mail_msg_new(&ml_drop_async_info);
	m->context = context;
	g_object_ref(context);
	m->folder = ml->folder;
	g_object_ref (m->folder);
	m->action = gdk_drag_context_get_selected_action (context);
	m->info = info;

	/* need to copy, goes away once we exit */
	m->selection = gtk_selection_data_copy (selection_data);

	ml_drop_action(m);
}

struct search_child_struct {
	gboolean found;
	gconstpointer looking_for;
};

static void
search_child_cb (GtkWidget *widget, gpointer data)
{
	struct search_child_struct *search = (struct search_child_struct *) data;

	search->found = search->found || g_direct_equal (widget, search->looking_for);
}

static gboolean
is_tree_widget_children (ETree *tree, gconstpointer widget)
{
	struct search_child_struct search;

	search.found = FALSE;
	search.looking_for = widget;

	gtk_container_foreach (GTK_CONTAINER (tree), search_child_cb, &search);

	return search.found;
}

static gboolean
ml_tree_drag_motion(ETree *tree, GdkDragContext *context, gint x, gint y, guint time, MessageList *ml)
{
	GList *targets;
	GdkDragAction action, actions = 0;

	/* If drop target is name of the account/store and not actual folder, don't allow any action */
	if (!ml->folder) {
		gdk_drag_status (context, 0, time);
		return TRUE;
	}

	/* If source widget is packed under 'tree', don't allow any action */
	if (is_tree_widget_children (tree, gtk_drag_get_source_widget (context))) {
		gdk_drag_status (context, 0, time);
		return TRUE;
	}

	targets = gdk_drag_context_list_targets (context);
	while (targets != NULL) {
		gint i;

		d(printf("atom drop '%s'\n", gdk_atom_name(targets->data)));
		for (i = 0; i < G_N_ELEMENTS (ml_drag_info); i++)
			if (targets->data == (gpointer)ml_drag_info[i].atom)
				actions |= ml_drag_info[i].actions;

		targets = g_list_next (targets);
	}
	d(printf("\n"));

	actions &= gdk_drag_context_get_actions (context);
	action = gdk_drag_context_get_suggested_action (context);
	if (action == GDK_ACTION_COPY && (actions & GDK_ACTION_MOVE))
		action = GDK_ACTION_MOVE;

	gdk_drag_status(context, action, time);

	return action != 0;
}

static void
on_model_row_changed (ETableModel *model, gint row, MessageList *ml)
{
	ml->priv->any_row_changed = TRUE;
}

static gboolean
ml_tree_sorting_changed (ETreeTableAdapter *adapter, MessageList *ml)
{
	g_return_val_if_fail (ml != NULL, FALSE);

	if (ml->threaded && ml->frozen == 0) {
		if (ml->thread_tree) {
			/* free the previous thread_tree to recreate it fully */
			camel_folder_thread_messages_unref (ml->thread_tree);
			ml->thread_tree = NULL;
		}

		mail_regen_list (ml, ml->search, NULL, NULL);

		return TRUE;
	}

	return FALSE;
}

/*
 * GObject::init
 */

static void
message_list_set_shell_backend (MessageList *message_list,
                               EShellBackend *shell_backend)
{
	g_return_if_fail (message_list->priv->shell_backend == NULL);

	message_list->priv->shell_backend = g_object_ref (shell_backend);
}

static void
message_list_init (MessageList *message_list)
{
	MessageListPrivate *p;
	GtkTargetList *target_list;
	GdkAtom matom;

	message_list->priv = MESSAGE_LIST_GET_PRIVATE (message_list);

#if HAVE_CLUTTER
	message_list->priv->timeline = NULL;
	message_list->priv->search_texture = NULL;
#endif

	message_list->normalised_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) e_poolv_destroy);

	message_list->search = NULL;
	message_list->ensure_uid = NULL;

	message_list->uid_nodemap = g_hash_table_new (g_str_hash, g_str_equal);
	message_list->async_event = mail_async_event_new();

	message_list->cursor_uid = NULL;
	message_list->last_sel_single = FALSE;

	message_list->regen_lock = g_mutex_new ();

	/* TODO: Should this only get the selection if we're realised? */
	p = message_list->priv;
	p->invisible = gtk_invisible_new();
	p->destroyed = FALSE;
	g_object_ref_sink(p->invisible);
	p->any_row_changed = FALSE;

	matom = gdk_atom_intern ("x-uid-list", FALSE);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, matom, 0);
	gtk_selection_add_target(p->invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 2);

	g_signal_connect(p->invisible, "selection_get", G_CALLBACK(ml_selection_get), message_list);
	g_signal_connect(p->invisible, "selection_clear_event", G_CALLBACK(ml_selection_clear_event), message_list);
	g_signal_connect(p->invisible, "selection_received", G_CALLBACK(ml_selection_received), message_list);

	/* FIXME This is currently unused. */
	target_list = gtk_target_list_new (NULL, 0);
	message_list->priv->copy_target_list = target_list;

	/* FIXME This is currently unused. */
	target_list = gtk_target_list_new (NULL, 0);
	message_list->priv->paste_target_list = target_list;
}

static void
message_list_destroy(GtkObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	MessageListPrivate *p = message_list->priv;

	p->destroyed = TRUE;

	if (message_list->async_event) {
		mail_async_event_destroy(message_list->async_event);
		message_list->async_event = NULL;
	}

	if (message_list->folder) {
		mail_regen_cancel(message_list);

		if (message_list->uid_nodemap) {
			g_hash_table_foreach(message_list->uid_nodemap, (GHFunc)clear_info, message_list);
			g_hash_table_destroy (message_list->uid_nodemap);
			message_list->uid_nodemap = NULL;
		}

		g_signal_handlers_disconnect_by_func (
			message_list->folder, folder_changed, message_list);
		g_object_unref (message_list->folder);
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

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (parent_class)->destroy(object);
}

static void
message_list_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_BACKEND:
			message_list_set_shell_backend (
				MESSAGE_LIST (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
message_list_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value, message_list_get_copy_target_list (
				MESSAGE_LIST (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value, message_list_get_paste_target_list (
				MESSAGE_LIST (object)));
			return;

		case PROP_SHELL_BACKEND:
			g_value_set_object (
				value, message_list_get_shell_backend (
				MESSAGE_LIST (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
message_list_dispose (GObject *object)
{
	MessageListPrivate *priv;

	priv = MESSAGE_LIST_GET_PRIVATE (object);

	if (priv->shell_backend != NULL) {
		g_object_unref (priv->shell_backend);
		priv->shell_backend = NULL;
	}

	if (priv->copy_target_list != NULL) {
		gtk_target_list_unref (priv->copy_target_list);
		priv->copy_target_list = NULL;
	}

	if (priv->paste_target_list != NULL) {
		gtk_target_list_unref (priv->paste_target_list);
		priv->paste_target_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
message_list_finalize (GObject *object)
{
	MessageList *message_list = MESSAGE_LIST (object);
	MessageListPrivate *priv = message_list->priv;

	g_hash_table_destroy (message_list->normalised_hash);

	if (message_list->ensure_uid) {
		g_free (message_list->ensure_uid);
		message_list->ensure_uid = NULL;
	}

	if (message_list->thread_tree)
		camel_folder_thread_messages_unref(message_list->thread_tree);

	g_free(message_list->search);
	g_free(message_list->ensure_uid);
	g_free(message_list->frozen_search);
	g_free(message_list->cursor_uid);

	g_mutex_free (message_list->regen_lock);

	g_free(message_list->folder_uri);
	message_list->folder_uri = NULL;

	clear_selection(message_list, &priv->clipboard);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
message_list_selectable_update_actions (ESelectable *selectable,
                                        EFocusTracker *focus_tracker,
                                        GdkAtom *clipboard_targets,
                                        gint n_clipboard_targets)
{
	GtkAction *action;
	gboolean sensitive;

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = (e_tree_row_count (E_TREE (selectable)) > 0);
	gtk_action_set_tooltip (action, _("Select all visible messages"));
	gtk_action_set_sensitive (action, sensitive);
}

static void
message_list_selectable_select_all (ESelectable *selectable)
{
	message_list_select_all (MESSAGE_LIST (selectable));
}

static void
message_list_class_init (MessageListClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (ml_drag_info); i++)
		ml_drag_info[i].atom = gdk_atom_intern(ml_drag_info[i].target, FALSE);

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (MessageListPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = message_list_set_property;
	object_class->get_property = message_list_get_property;
	object_class->dispose = message_list_dispose;
	object_class->finalize = message_list_finalize;

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = message_list_destroy;

	class->message_list_built = NULL;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_SHELL_BACKEND,
		g_param_spec_object (
			"shell-backend",
			"Shell Backend",
			"The mail shell backend",
			E_TYPE_SHELL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

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
}

static void
message_list_selectable_init (ESelectableInterface *interface)
{
	interface->update_actions = message_list_selectable_update_actions;
	interface->select_all = message_list_selectable_select_all;
}

static gboolean
read_boolean_with_default (GConfClient *gconf, const gchar *key, gboolean def_value)
{
	GConfValue *value;
	gboolean res;

	g_return_val_if_fail (gconf != NULL, def_value);
	g_return_val_if_fail (key != NULL, def_value);

	value = gconf_client_get (gconf, key, NULL);
	if (!value)
		return def_value;

	res = gconf_value_get_bool (value);
	gconf_value_free (value);

	return res;
}

static void
message_list_construct (MessageList *message_list)
{
	AtkObject *a11y;
	gboolean constructed;
	gchar *etspecfile;
	GConfClient *gconf = mail_config_get_gconf_client ();

	message_list->model =
		e_tree_memory_callbacks_new (ml_tree_icon_at,

					     ml_column_count,

					     ml_has_save_id,
					     ml_get_save_id,

					     ml_has_get_node_by_id,
					     ml_get_node_by_id,

					     ml_tree_sort_value_at,
					     ml_tree_value_at,
					     ml_tree_set_value_at,
					     ml_tree_is_cell_editable,

					     ml_duplicate_value,
					     ml_free_value,
					     ml_initialize_value,
					     ml_value_is_empty,
					     ml_value_to_string,

					     message_list);

	e_tree_memory_set_expanded_default(E_TREE_MEMORY(message_list->model),
					   read_boolean_with_default (gconf,
								      "/apps/evolution/mail/display/thread_expand",
								      TRUE));

	message_list->priv->thread_latest = read_boolean_with_default (gconf, "/apps/evolution/mail/display/thread_latest", TRUE);

	/*
	 * The etree
	 */
	message_list->extras = message_list_create_extras ();

	etspecfile = g_build_filename (EVOLUTION_ETSPECDIR, "message-list.etspec", NULL);
	constructed = e_tree_construct_from_spec_file (
		E_TREE (message_list), message_list->model,
		message_list->extras, etspecfile, NULL);
	g_free (etspecfile);

	if (constructed)
		e_tree_root_node_set_visible (E_TREE (message_list), FALSE);

	if (atk_get_root() != NULL) {
		a11y = gtk_widget_get_accessible (GTK_WIDGET (message_list));
		atk_object_set_name(a11y, _("Messages"));
	}

	g_signal_connect (
		e_tree_get_table_adapter (E_TREE (message_list)),
		"model_row_changed",
		G_CALLBACK (on_model_row_changed), message_list);

	g_signal_connect (
		message_list, "cursor_activated",
		G_CALLBACK (on_cursor_activated_cmd), message_list);

	g_signal_connect (
		message_list, "click",
		G_CALLBACK (on_click), message_list);

	g_signal_connect (
		message_list, "selection_change",
		G_CALLBACK (on_selection_changed_cmd), message_list);

	e_tree_drag_source_set (
		E_TREE (message_list), GDK_BUTTON1_MASK,
		ml_drag_types, G_N_ELEMENTS (ml_drag_types),
		GDK_ACTION_MOVE|GDK_ACTION_COPY);

	g_signal_connect (
		message_list, "tree_drag_data_get",
		G_CALLBACK(ml_tree_drag_data_get), message_list);

	e_tree_drag_dest_set (
		E_TREE (message_list), GTK_DEST_DEFAULT_ALL,
		ml_drop_types, G_N_ELEMENTS (ml_drop_types),
		GDK_ACTION_MOVE|GDK_ACTION_COPY);

	g_signal_connect (
		message_list, "tree_drag_data_received",
		G_CALLBACK (ml_tree_drag_data_received), message_list);

	g_signal_connect (
		message_list, "drag-motion",
		G_CALLBACK (ml_tree_drag_motion), message_list);

	g_signal_connect (
		e_tree_get_table_adapter (E_TREE (message_list)),
		"sorting_changed",
		G_CALLBACK (ml_tree_sorting_changed), message_list);
}

GType
message_list_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (MessageListClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) message_list_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (MessageList),
			0,     /* n_preallocs */
			(GInstanceInitFunc) message_list_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo selectable_info = {
			(GInterfaceInitFunc) message_list_selectable_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			E_TREE_TYPE, "MessageList", &type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_SELECTABLE, &selectable_info);
	}

	return type;
}

/**
 * message_list_new:
 *
 * Creates a new message-list widget.
 *
 * Returns a new message-list widget.
 **/
GtkWidget *
message_list_new (EShellBackend *shell_backend)
{
	GtkWidget *message_list;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	message_list = g_object_new (
		message_list_get_type (),
		"shell-backend", shell_backend, NULL);

	message_list_construct (MESSAGE_LIST (message_list));

	return message_list;
}

EShellBackend *
message_list_get_shell_backend (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	return message_list->priv->shell_backend;
}

static void
clear_info(gchar *key, ETreePath *node, MessageList *ml)
{
	CamelMessageInfo *info;

	info = e_tree_memory_node_get_data((ETreeMemory *)ml->model, node);
	camel_folder_free_message_info(ml->folder, info);
	e_tree_memory_node_set_data((ETreeMemory *)ml->model, node, NULL);
}

static void
clear_tree (MessageList *ml, gboolean tfree)
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

	ml->priv->newest_read_date = 0;
	ml->priv->newest_read_uid = NULL;
	ml->priv->oldest_unread_date = 0;
	ml->priv->oldest_unread_uid = NULL;

	if (ml->tree_root) {
		/* we should be frozen already */
		e_tree_memory_node_remove (E_TREE_MEMORY(etm), ml->tree_root);
	}

	ml->tree_root = e_tree_memory_node_insert (E_TREE_MEMORY(etm), NULL, 0, NULL);
	if (tfree)
		e_tree_model_rebuilt (E_TREE_MODEL(etm));
#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Clearing tree took %ld.%03ld seconds\n", diff / 1000, diff % 1000);
#endif

}

static gboolean
folder_store_supports_vjunk_folder (CamelFolder *folder)
{
	CamelStore *store;

	g_return_val_if_fail (folder != NULL, FALSE);

	store = camel_folder_get_parent_store (folder);
	if (!store)
		return FALSE;

	return (store->flags & (CAMEL_STORE_VJUNK | CAMEL_STORE_REAL_JUNK_FOLDER)) != 0 || CAMEL_IS_VEE_FOLDER (folder);
}

/* Check if the given node is selectable in the current message list,
 * which depends on the type of the folder (normal, junk, trash). */
static gboolean
is_node_selectable (MessageList *ml, CamelMessageInfo *info)
{
	gboolean is_junk_folder;
	gboolean is_trash_folder;
	guint32 flags;
	gboolean flag_junk;
	gboolean flag_deleted;
	gboolean store_has_vjunk;

	g_return_val_if_fail (ml != NULL, FALSE);
	g_return_val_if_fail (ml->folder != NULL, FALSE);
	g_return_val_if_fail (info != NULL, FALSE);

	store_has_vjunk = folder_store_supports_vjunk_folder (ml->folder);

	/* check folder type */
	is_junk_folder = store_has_vjunk && (ml->folder->folder_flags & CAMEL_FOLDER_IS_JUNK) != 0;
	is_trash_folder = ml->folder->folder_flags & CAMEL_FOLDER_IS_TRASH;

	/* check flags set on current message */
	flags = camel_message_info_flags (info);
	flag_junk = store_has_vjunk && (flags & CAMEL_MESSAGE_JUNK) != 0;
	flag_deleted = flags & CAMEL_MESSAGE_DELETED;

	/* perform actions depending on folder type */
	if (is_junk_folder) {
		/* messages in a junk folder are selectable only if
		 * the message is marked as junk and if not deleted
		 * when hidedeleted is set */
		if (flag_junk && !(flag_deleted && ml->hidedeleted))
			return TRUE;

	} else if (is_trash_folder) {
		/* messages in a trash folder are selectable unless
		 * not deleted any more */
		if (flag_deleted)
			return TRUE;
	} else {
		/* in normal folders it depends on hidedeleted,
		 * hidejunk and the message flags */
		if (!(flag_junk && ml->hidejunk)
		    && !(flag_deleted && ml->hidedeleted))
			return TRUE;
	}

	return FALSE;
}

/* We try and find something that is selectable in our tree.  There is
 * actually no assurance that we'll find something that will still be
 * there next time, but its probably going to work most of the time. */
static gchar *
find_next_selectable (MessageList *ml)
{
	ETreePath node;
	gint last;
	gint vrow_orig;
	gint vrow;
	ETree *et = E_TREE (ml);
	CamelMessageInfo *info;

	node = g_hash_table_lookup (ml->uid_nodemap, ml->cursor_uid);
	if (node == NULL)
		return NULL;

	info = get_message_info (ml, node);
	if (info && is_node_selectable (ml, info))
		return NULL;

	last = e_tree_row_count (et);

	/* model_to_view_row etc simply dont work for sorted views.  Sigh. */
	vrow_orig = e_tree_row_of_node (et, node);

	/* We already checked this node. */
	vrow = vrow_orig + 1;

	while (vrow < last) {
		node = e_tree_node_at_row (et, vrow);
		info = get_message_info (ml, node);
		if (info && is_node_selectable (ml, info))
			return g_strdup (camel_message_info_uid (info));
		vrow++;
	}

	/* We didn't find any undeleted entries _below_ the currently selected one
         * so let's try to find one _above_ */
	vrow = vrow_orig - 1;

	while (vrow >= 0) {
		node = e_tree_node_at_row (et, vrow);
		info = get_message_info (ml, node);
		if (info && is_node_selectable (ml, info))
			return g_strdup (camel_message_info_uid (info));
		vrow--;
	}

	return NULL;
}

static ETreePath *
ml_uid_nodemap_insert (MessageList *message_list,
                       CamelMessageInfo *info,
                       ETreePath *parent_node,
                       gint row)
{
	ETreeMemory *tree;
	ETreePath *node;
	const gchar *uid;
	time_t date;
	guint flags;

	if (parent_node == NULL)
		parent_node = message_list->tree_root;

	tree = E_TREE_MEMORY (message_list->model);
	node = e_tree_memory_node_insert (tree, parent_node, row, info);

	uid = camel_message_info_uid (info);
	flags = camel_message_info_flags (info);
	date = camel_message_info_date_received (info);

	camel_folder_ref_message_info (message_list->folder, info);
	g_hash_table_insert (message_list->uid_nodemap, (gpointer) uid, node);

	/* Track the latest seen and unseen messages shown, used in
	 * fallback heuristics for automatic message selection. */
	if (flags & CAMEL_MESSAGE_SEEN) {
		if (date > message_list->priv->newest_read_date) {
			message_list->priv->newest_read_date = date;
			message_list->priv->newest_read_uid = uid;
		}
	} else {
		if (message_list->priv->oldest_unread_date == 0) {
			message_list->priv->oldest_unread_date = date;
			message_list->priv->oldest_unread_uid = uid;
		} else if (date < message_list->priv->oldest_unread_date) {
			message_list->priv->oldest_unread_date = date;
			message_list->priv->oldest_unread_uid = uid;
		}
	}

	return node;
}

static void
ml_uid_nodemap_remove (MessageList *message_list,
                       CamelMessageInfo *info)
{
	const gchar *uid;

	uid = camel_message_info_uid (info);

	if (uid == message_list->priv->newest_read_uid) {
		message_list->priv->newest_read_date = 0;
		message_list->priv->newest_read_uid = NULL;
	}

	if (uid == message_list->priv->oldest_unread_uid) {
		message_list->priv->oldest_unread_date = 0;
		message_list->priv->oldest_unread_uid = NULL;
	}

	g_hash_table_remove (message_list->uid_nodemap, uid);
	camel_folder_free_message_info (message_list->folder, info);
}

/* only call if we have a tree model */
/* builds the tree structure */

#define BROKEN_ETREE	/* avoid some broken code in etree(?) by not using the incremental update */

static void build_subtree (MessageList *ml, ETreePath parent, CamelFolderThreadNode *c, gint *row);

static void build_subtree_diff (MessageList *ml, ETreePath parent, ETreePath path, CamelFolderThreadNode *c, gint *row);

static void
build_tree (MessageList *ml, CamelFolderThread *thread, CamelFolderChangeInfo *changes)
{
	gint row = 0;
	ETreeModel *etm = ml->model;
#ifndef BROKEN_ETREE
	ETreePath *top;
#endif
	gchar *saveuid = NULL;
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
		saveuid = find_next_selectable (ml);

#ifndef BROKEN_ETREE
	top = e_tree_model_node_get_first_child(etm, ml->tree_root);
	if (top == NULL || changes == NULL) {
#else
		selected = message_list_get_selected(ml);
#endif
		e_tree_memory_freeze(E_TREE_MEMORY(etm));
		clear_tree (ml, FALSE);

		build_subtree(ml, ml->tree_root, thread->tree, &row);
		e_tree_memory_thaw(E_TREE_MEMORY(etm));
#ifdef BROKEN_ETREE
		message_list_set_selected(ml, selected);
		em_utils_uids_free(selected);
#else
	} else {
		static gint tree_equal(ETreeModel *etm, ETreePath ap, CamelFolderThreadNode *bp);

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
			e_tree_set_cursor (E_TREE (ml), node);
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
build_subtree (MessageList *ml, ETreePath parent, CamelFolderThreadNode *c, gint *row)
{
	ETreePath node;

	while (c) {
		/* phantom nodes no longer allowed */
		if (!c->message) {
			g_warning("c->message shouldn't be NULL\n");
			c = c->next;
			continue;
		}

		node = ml_uid_nodemap_insert (
			ml, (CamelMessageInfo *) c->message, parent, -1);

		if (c->child) {
			build_subtree(ml, node, c->child, row);
		}
		c = c->next;
	}
}

/* compares a thread tree node with the etable tree node to see if they point to
   the same object */
static gint
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
static gint
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
add_node_diff(MessageList *ml, ETreePath parent, ETreePath path, CamelFolderThreadNode *c, gint *row, gint myrow)
{
	CamelMessageInfo *info;
	ETreePath node;

	g_return_if_fail (c->message != NULL);

	/* XXX Casting away constness. */
	info = (CamelMessageInfo *) c->message;

	/* we just update the hashtable key */
	ml_uid_nodemap_remove (ml, info);
	node = ml_uid_nodemap_insert (ml, info, parent, myrow);
	(*row)++;

	if (c->child) {
		build_subtree_diff(ml, node, NULL, c->child, row);
	}
}

/* removes node, children recursively and all associated data */
static void
remove_node_diff(MessageList *ml, ETreePath node, gint depth)
{
	ETreeModel *etm = ml->model;
	ETreePath cp, cn;
	CamelMessageInfo *info;

	t(printf("Removing node: %s\n", (gchar *)e_tree_memory_node_get_data(etm, node)));

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

	g_return_if_fail (info);
	ml_uid_nodemap_remove (ml, info);
}

/* applies a new tree structure to an existing tree, but only by changing things
   that have changed */
static void
build_subtree_diff(MessageList *ml, ETreePath parent, ETreePath path, CamelFolderThreadNode *c, gint *row)
{
	ETreeModel *etm = ml->model;
	ETreePath ap, *ai, *at, *tmp;
	CamelFolderThreadNode *bp, *bi, *bt;
	gint i, j, myrow = 0;

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
				gpointer olduid, oldrow;
				/* if this is a message row, check/update the row id map */
				if (g_hash_table_lookup_extended(ml->uid_rowmap, camel_message_info_uid(bp->message), &olduid, &oldrow)) {
					if ((gint)oldrow != (*row)) {
						g_hash_table_insert(ml->uid_rowmap, olduid, (gpointer)(*row));
					}
				} else {
					g_warning("Cannot find uid %s in table?", camel_message_info_uid(bp->message));
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
	gchar *saveuid = NULL;
	gint i;
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
		saveuid = find_next_selectable (ml);

#ifndef BROKEN_ETREE
	if (changes) {
		build_flat_diff(ml, changes);
	} else {
#else
		selected = message_list_get_selected(ml);
#endif
		e_tree_memory_freeze(E_TREE_MEMORY(etm));
		clear_tree (ml, FALSE);
		for (i = 0; i < summary->len; i++) {
			CamelMessageInfo *info = summary->pdata[i];

			ml_uid_nodemap_insert (ml, info, NULL, -1);
		}
		e_tree_memory_thaw(E_TREE_MEMORY(etm));
#ifdef BROKEN_ETREE
		message_list_set_selected(ml, selected);
		em_utils_uids_free(selected);
#else
	}
#endif

	if (saveuid) {
		ETreePath node = g_hash_table_lookup(ml->uid_nodemap, saveuid);
		if (node == NULL) {
			g_free (ml->cursor_uid);
			ml->cursor_uid = NULL;
			g_signal_emit (ml, message_list_signals[MESSAGE_SELECTED], 0, NULL);
		} else {
			e_tree_set_cursor (E_TREE (ml), node);
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
		if (!e_tree_node_is_expanded (E_TREE (ml), node))
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
	gint i;
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
			ml_uid_nodemap_remove (ml, info);
		}
	}

	/* add new nodes? - just append to the end */
	d(printf("Adding messages to view:\n"));
	for (i=0;i<changes->uid_added->len;i++) {
		info = camel_folder_get_message_info (ml->folder, changes->uid_added->pdata[i]);
		if (info) {
			d(printf(" %s\n", (gchar *)changes->uid_added->pdata[i]));
			ml_uid_nodemap_insert (ml, info, NULL, -1);
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
#endif /* BROKEN_ETREE */

static void
mail_folder_hide_by_flag (CamelFolder *folder, MessageList *ml, CamelFolderChangeInfo **changes, gint flag)
{
	CamelFolderChangeInfo *newchanges, *oldchanges = *changes;
	CamelMessageInfo *info;
	gint i;

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
		if (info)
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
	gint i;

	/* may be NULL if we're in the process of being destroyed */
	if (ml->async_event == NULL)
		return;

	d(printf("folder changed event, changes = %p\n", changes));
	if (changes) {
		d(printf("changed = %d added = %d removed = %d\n",
			 changes->uid_changed->len, changes->uid_added->len, changes->uid_removed->len));

		for (i = 0; i < changes->uid_removed->len; i++)
			g_hash_table_remove (
				ml->normalised_hash,
				changes->uid_removed->pdata[i]);

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

			g_signal_emit(ml, message_list_signals[MESSAGE_LIST_BUILT], 0);
			return;
		}
	}

	mail_regen_list (ml, ml->search, NULL, changes);
}

static void
folder_changed (CamelFolder *folder,
                CamelFolderChangeInfo *info,
                MessageList *ml)
{
	CamelFolderChangeInfo *changes;

	if (ml->priv->destroyed)
		return;

	if (info != NULL) {
		changes = camel_folder_change_info_new();
		camel_folder_change_info_cat (changes, info);
	} else {
		changes = NULL;
	}

	mail_async_event_emit (
		ml->async_event, MAIL_ASYNC_GUI, (MailAsyncFunc)
		main_folder_changed, folder, changes, ml);
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
message_list_set_folder (MessageList *message_list, CamelFolder *folder, const gchar *uri, gboolean outgoing)
{
	ETreeModel *etm = message_list->model;
	gboolean hide_deleted;
	GConfClient *gconf;
	CamelStore *folder_store;

	g_return_if_fail (IS_MESSAGE_LIST (message_list));

	if (message_list->folder == folder)
		return;

	if (message_list->seen_id) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	/* reset the normalised sort performance hack */
	g_hash_table_remove_all (message_list->normalised_hash);

	mail_regen_cancel(message_list);

	if (message_list->folder != NULL) {
		save_tree_state (message_list);
	}

	e_tree_memory_freeze(E_TREE_MEMORY(etm));
	clear_tree (message_list, TRUE);
	e_tree_memory_thaw(E_TREE_MEMORY(etm));

	/* remove the cursor activate idle handler */
	if (message_list->idle_id != 0) {
		g_source_remove (message_list->idle_id);
		message_list->idle_id = 0;
	}

	if (message_list->folder) {
		g_signal_handlers_disconnect_by_func (
			message_list->folder, folder_changed, message_list);
		g_object_unref (message_list->folder);
		message_list->folder = NULL;
	}

	if (message_list->thread_tree) {
		camel_folder_thread_messages_unref(message_list->thread_tree);
		message_list->thread_tree = NULL;
	}

	if (message_list->folder_uri != uri) {
		g_free(message_list->folder_uri);
		message_list->folder_uri = uri ? g_strdup(uri):NULL;
	}

	if (message_list->cursor_uid) {
		g_free(message_list->cursor_uid);
		message_list->cursor_uid = NULL;
		g_signal_emit(message_list, message_list_signals[MESSAGE_SELECTED], 0, NULL);
	}

	if (folder) {
		gint strikeout_col = -1;
		ECell *cell;

		g_object_ref (folder);
		message_list->folder = folder;
		message_list->just_set_folder = TRUE;

		/* hide deleted messages also when the store has a real trash */
		folder_store = camel_folder_get_parent_store (folder);

		/* Setup the strikeout effect for non-trash folders */
		if (!(folder->folder_flags & CAMEL_FOLDER_IS_TRASH) || !(folder_store->flags & CAMEL_STORE_VTRASH))
			strikeout_col = COL_DELETED;

		cell = e_table_extras_get_cell (message_list->extras, "render_date");
		g_object_set (cell, "strikeout_column", strikeout_col, NULL);

		cell = e_table_extras_get_cell (message_list->extras, "render_text");
		g_object_set (cell, "strikeout_column", strikeout_col, NULL);

		cell = e_table_extras_get_cell (message_list->extras, "render_size");
		g_object_set (cell, "strikeout_column", strikeout_col, NULL);

		cell = e_table_extras_get_cell (message_list->extras, "render_composite_from");
		composite_cell_set_strike_col (cell, strikeout_col);

		cell = e_table_extras_get_cell (message_list->extras, "render_composite_to");
		composite_cell_set_strike_col (cell, strikeout_col);

		/* Build the etree suitable for this folder */
		message_list_setup_etree (message_list, outgoing);

		g_signal_connect (
			folder, "changed",
			G_CALLBACK (folder_changed), message_list);

		gconf = mail_config_get_gconf_client ();
		hide_deleted = !gconf_client_get_bool (gconf, "/apps/evolution/mail/display/show_deleted", NULL);
		message_list->hidedeleted = hide_deleted && (!(folder->folder_flags & CAMEL_FOLDER_IS_TRASH) || !(folder_store->flags & CAMEL_STORE_VTRASH));
		message_list->hidejunk = folder_store_supports_vjunk_folder (message_list->folder) && !(folder->folder_flags & CAMEL_FOLDER_IS_JUNK) && !(folder->folder_flags & CAMEL_FOLDER_IS_TRASH);

		if (message_list->frozen == 0)
			mail_regen_list (message_list, message_list->search, NULL, NULL);
	}
}

GtkTargetList *
message_list_get_copy_target_list (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	return message_list->priv->copy_target_list;
}

GtkTargetList *
message_list_get_paste_target_list (MessageList *message_list)
{
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), NULL);

	return message_list->priv->paste_target_list;
}

static gboolean
on_cursor_activated_idle (gpointer data)
{
	MessageList *message_list = data;
	ESelectionModel *esm;
	gint selected;

	esm = e_tree_get_selection_model (E_TREE (message_list));
	selected = e_selection_model_selected_count (esm);

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
on_cursor_activated_cmd (ETree *tree, gint row, ETreePath path, gpointer user_data)
{
	MessageList *message_list = MESSAGE_LIST (user_data);
	const gchar *new_uid;

	if (path == NULL)
		new_uid = NULL;
	else
		new_uid = get_message_uid (message_list, path);

	/* Do not check the cursor_uid and the new_uid values, because the selected item
	   (set in on_selection_changed_cmd) can be different from the one with a cursor
	   (when selecting with Ctrl, for example). This has a little side-effect, when
	   keeping list it that state, then changing folders forth and back will select
	   and move cursor to that selected item. Does anybody consider it as a bug? */
	if ((message_list->cursor_uid == NULL && new_uid == NULL)
	    || (message_list->last_sel_single && message_list->cursor_uid != NULL && new_uid != NULL))
		return;

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
	gchar *newuid;
	ETreePath cursor;

	/* not sure if we could just ignore this for the cursor, i think sometimes you
	   only get a selection changed when you should also get a cursor activated? */
	uids = message_list_get_selected(ml);
	if (uids->len == 1)
		newuid = uids->pdata[0];
	else if ((cursor = e_tree_get_cursor(tree)))
		newuid = (gchar *)camel_message_info_uid(e_tree_memory_node_get_data((ETreeMemory *)tree, cursor));
	else
		newuid = NULL;

	/* If the selection isn't empty, then we ignore the no-uid check, since this event
	   is also used for other updating.  If it is empty, it might just be a setup event
	   from etree which we do need to ignore */
	if ((newuid == NULL && ml->cursor_uid == NULL && uids->len == 0) ||
	    (ml->last_sel_single && uids->len == 1 && newuid != NULL && ml->cursor_uid != NULL && !strcmp (ml->cursor_uid, newuid))) {
		/* noop */
	} else {
		g_free(ml->cursor_uid);
		ml->cursor_uid = g_strdup(newuid);
		if (!ml->idle_id)
			ml->idle_id = g_idle_add_full (G_PRIORITY_LOW, on_cursor_activated_idle, ml, NULL);
	}

	ml->last_sel_single = uids->len == 1;

	em_utils_uids_free(uids);
}

static gint
on_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, MessageList *list)
{
	CamelMessageInfo *info;
	gboolean folder_is_trash;
	const gchar *uid;
	gint flag;
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

	folder_is_trash =
		((list->folder->folder_flags & CAMEL_FOLDER_IS_TRASH) != 0);

	/* If a message was marked as deleted and the user flags it as
	 * important or unread in a non-Trash folder, then undelete the
	 * message.  We avoid automatically undeleting messages while
	 * viewing a Trash folder because it would cause the message to
	 * suddenly disappear from the message list, which is confusing
	 * and alarming to the user. */
	if (!folder_is_trash && flags & CAMEL_MESSAGE_DELETED) {
		if (col == COL_FLAGGED && !(flags & CAMEL_MESSAGE_FLAGGED))
			flag |= CAMEL_MESSAGE_DELETED;

		if (col == COL_MESSAGE_STATUS && (flags & CAMEL_MESSAGE_SEEN))
			flag |= CAMEL_MESSAGE_DELETED;
	}

	uid = camel_message_info_uid (info);
	camel_folder_set_message_flags (list->folder, uid, flag, ~flags);

	/* Notify the folder tree model that the user has marked a message
	 * as unread so it doesn't mistake the event as new mail arriving. */
	if (col == COL_MESSAGE_STATUS && (flags & CAMEL_MESSAGE_SEEN)) {
		EMFolderTreeModel *model;

		model = em_folder_tree_model_get_default ();
		em_folder_tree_model_user_marked_unread (
			model, list->folder_uri, 1);
	}

	if (flag == CAMEL_MESSAGE_SEEN && list->seen_id) {
		g_source_remove (list->seen_id);
		list->seen_id = 0;
	}

	return TRUE;
}

struct _ml_selected_data {
	MessageList *ml;
	GPtrArray *uids;
};

static void
ml_getselected_cb(ETreePath path, gpointer user_data)
{
	struct _ml_selected_data *data = user_data;
	const gchar *uid;

	if (e_tree_model_node_is_root (data->ml->model, path))
		return;

	uid = get_message_uid(data->ml, path);
	g_return_if_fail(uid != NULL);
	g_ptr_array_add(data->uids, g_strdup(uid));
}

GPtrArray *
message_list_get_uids(MessageList *ml)
{
	struct _ml_selected_data data = {
		ml,
		g_ptr_array_new()
	};

	e_tree_path_foreach (E_TREE (ml), ml_getselected_cb, &data);

	if (ml->folder && data.uids->len)
		camel_folder_sort_uids (ml->folder, data.uids);

	return data.uids;
}

GPtrArray *
message_list_get_selected(MessageList *ml)
{
	struct _ml_selected_data data = {
		ml,
		g_ptr_array_new()
	};

	e_tree_selected_path_foreach (E_TREE (ml), ml_getselected_cb, &data);

	if (ml->folder && data.uids->len)
		camel_folder_sort_uids (ml->folder, data.uids);

	return data.uids;
}

void
message_list_set_selected(MessageList *ml, GPtrArray *uids)
{
	gint i;
	ETreeSelectionModel *etsm;
	ETreePath node;
	GPtrArray *paths = g_ptr_array_new();

	etsm = (ETreeSelectionModel *) e_tree_get_selection_model (E_TREE (ml));
	for (i=0; i<uids->len; i++) {
		node = g_hash_table_lookup(ml->uid_nodemap, uids->pdata[i]);
		if (node)
			g_ptr_array_add(paths, node);
	}

	e_tree_selection_model_select_paths(etsm, paths);
	g_ptr_array_free(paths, TRUE);
}

struct ml_count_data {
	MessageList *ml;
	guint count;
};

static void
ml_getcount_cb (ETreePath path, gpointer user_data)
{
	struct ml_count_data *data = user_data;

	if (!e_tree_model_node_is_root (data->ml->model, path))
		data->count++;
}

guint
message_list_count (MessageList *message_list)
{
	struct ml_count_data data = { message_list, 0 };

	g_return_val_if_fail (message_list != NULL, 0);
	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), 0);

	e_tree_path_foreach (E_TREE (message_list), ml_getcount_cb, &data);

	return data.count;
}

void
message_list_freeze(MessageList *ml)
{
	ml->frozen++;
}

void
message_list_thaw(MessageList *ml)
{
	g_return_if_fail (ml->frozen != 0);

	ml->frozen--;
	if (ml->frozen == 0) {
		mail_regen_list(ml, ml->frozen_search?ml->frozen_search:ml->search, NULL, NULL);
		g_free(ml->frozen_search);
		ml->frozen_search = NULL;
	}
}

/* set whether we are in threaded view or flat view */
void
message_list_set_threaded_expand_all (MessageList *ml)
{
	if (ml->threaded) {
		ml->expand_all = 1;

		if (ml->frozen == 0)
			mail_regen_list (ml, ml->search, NULL, NULL);
	}
}

void
message_list_set_threaded_collapse_all (MessageList *ml)
{
	if (ml->threaded) {
		ml->collapse_all = 1;

		if (ml->frozen == 0)
			mail_regen_list (ml, ml->search, NULL, NULL);
	}
}

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
message_list_set_search (MessageList *ml, const gchar *search)
{
#if HAVE_CLUTTER
	if (ml->priv->timeline == NULL) {
		ClutterActor *stage = g_object_get_data ((GObject *)ml, "stage");

		if (stage) {
			ClutterActor *texture = NULL;
			ClutterPath *path;
			ClutterBehaviour *behaviour;
			ClutterAlpha *alpha;
			GtkIconInfo *info;

			info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default(),
							"system-search",
							72,
							GTK_ICON_LOOKUP_NO_SVG);

			if (info) {
				texture = clutter_texture_new_from_file (gtk_icon_info_get_filename(info), NULL);
				gtk_icon_info_free(info);
			}
			clutter_container_add_actor ((ClutterContainer *)stage, texture);
			ml->priv->search_texture = texture;

			ml->priv->timeline = clutter_timeline_new (2 * 1000);
			alpha = clutter_alpha_new_full (ml->priv->timeline, CLUTTER_LINEAR);
			path = clutter_path_new();
			behaviour = clutter_behaviour_path_new (alpha, path);
			clutter_actor_hide (texture);
			clutter_path_clear (path);
			clutter_path_add_move_to (path, 100, 50);
			clutter_path_add_line_to (path, 200, 50);
			clutter_path_add_line_to (path, 200, 100);
			clutter_path_add_line_to (path, 100, 100);
			clutter_path_add_line_to (path, 100, 50);

			clutter_behaviour_apply (behaviour, texture);
			clutter_timeline_set_loop (ml->priv->timeline, TRUE);

			g_signal_connect_swapped (
				ml->priv->timeline, "started",
				G_CALLBACK (clutter_actor_show), texture);
			g_signal_connect (
				ml->priv->timeline, "paused",
				G_CALLBACK (clutter_actor_hide), texture);

			clutter_timeline_pause (ml->priv->timeline);
			clutter_timeline_stop (ml->priv->timeline);

		}
	}
#endif

	if (search == NULL || search[0] == '\0')
		if (ml->search == NULL || ml->search[0] == '\0')
			return;

	if (search != NULL && ml->search != NULL && strcmp (search, ml->search) == 0)
		return;

	if (ml->thread_tree) {
		camel_folder_thread_messages_unref(ml->thread_tree);
		ml->thread_tree = NULL;
	}

#if HAVE_CLUTTER
	if (ml->priv->timeline)
		clutter_timeline_start (ml->priv->timeline);
#endif

	if (ml->frozen == 0)
		mail_regen_list (ml, search, NULL, NULL);
	else {
		g_free(ml->frozen_search);
		ml->frozen_search = g_strdup(search);
	}
}

/* will ensure that the message with UID uid will be in the message list after the next rebuild */
void
message_list_ensure_message (MessageList *ml, const gchar *uid)
{
	g_return_if_fail (ml != NULL);

	g_free (ml->ensure_uid);
	ml->ensure_uid = g_strdup (uid);
}

struct sort_column_data {
	ETableCol *col;
	gboolean ascending;
};

struct sort_message_info_data {
	CamelMessageInfo *mi;
	GPtrArray *values; /* read values so far, in order of sort_array_data::sort_columns */
};

struct sort_array_data {
	MessageList *ml;
	GPtrArray *sort_columns; /* struct sort_column_data in order of sorting */
	GHashTable *message_infos; /* uid -> struct sort_message_info_data */
	gpointer cmp_cache;
};

static gint
cmp_array_uids (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const gchar *uid1 = *(const gchar **) a;
	const gchar *uid2 = *(const gchar **) b;
	struct sort_array_data *sort_data = user_data;
	gint i, res = 0;
	struct sort_message_info_data *md1, *md2;

	g_return_val_if_fail (sort_data != NULL, 0);

	md1 = g_hash_table_lookup (sort_data->message_infos, uid1);
	md2 = g_hash_table_lookup (sort_data->message_infos, uid2);

	g_return_val_if_fail (md1 != NULL, 0);
	g_return_val_if_fail (md1->mi != NULL, 0);
	g_return_val_if_fail (md2 != NULL, 0);
	g_return_val_if_fail (md2->mi != NULL, 0);

	for (i = 0; res == 0 && i < sort_data->sort_columns->len; i++) {
		gpointer v1, v2;
		struct sort_column_data *scol = g_ptr_array_index (sort_data->sort_columns, i);

		if (md1->values->len <= i) {
			v1 = ml_tree_value_at_ex (NULL, NULL, scol->col->compare_col, md1->mi, sort_data->ml);
			g_ptr_array_add (md1->values, v1);
		} else {
			v1 = g_ptr_array_index (md1->values, i);
		}

		if (md2->values->len <= i) {
			v2 = ml_tree_value_at_ex (NULL, NULL, scol->col->compare_col, md2->mi, sort_data->ml);
			g_ptr_array_add (md2->values, v2);
		} else {
			v2 = g_ptr_array_index (md2->values, i);
		}

		if (v1 != NULL && v2 != NULL) {
			res = (*scol->col->compare) (v1, v2, sort_data->cmp_cache);
		} else if (v1 != NULL || v2 != NULL) {
			res = v1 == NULL ? -1 : 1;
		}

		if (!scol->ascending)
			res = res * (-1);
	}

	if (res == 0)
		res = camel_folder_cmp_uids (sort_data->ml->folder, uid1, uid2);

	return res;
}

static void
free_message_info_data (gpointer uid, struct sort_message_info_data *data, struct sort_array_data *sort_data)
{
	if (data->values) {
		/* values in this array are not newly allocated, even ml_tree_value_at_ex
		   returns gpointer, not a gconstpointer */
		g_ptr_array_free (data->values, TRUE);
	}

	camel_folder_free_message_info (sort_data->ml->folder, data->mi);
	g_free (data);
}

static void
ml_sort_uids_by_tree (MessageList *ml, GPtrArray *uids)
{
	ETreeTableAdapter *adapter;
	ETableSortInfo *sort_info;
	ETableHeader *full_header;
	struct sort_array_data sort_data;
	guint i, len;

	g_return_if_fail (ml != NULL);
	g_return_if_fail (ml->folder != NULL);
	g_return_if_fail (uids != NULL);

	adapter = e_tree_get_table_adapter (E_TREE (ml));
	g_return_if_fail (adapter != NULL);

	sort_info = e_tree_table_adapter_get_sort_info (adapter);
	full_header = e_tree_table_adapter_get_header (adapter);

	if (!sort_info || uids->len == 0 || !full_header || e_table_sort_info_sorting_get_count (sort_info) == 0) {
		camel_folder_sort_uids (ml->folder, uids);
		return;
	}

	len = e_table_sort_info_sorting_get_count (sort_info);

	sort_data.ml = ml;
	sort_data.sort_columns = g_ptr_array_sized_new (len);
	sort_data.message_infos = g_hash_table_new (g_str_hash, g_str_equal);
	sort_data.cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	for (i = 0; i < len; i++) {
		ETableSortColumn scol;
		struct sort_column_data *data = g_new0 (struct sort_column_data, 1);

		scol = e_table_sort_info_sorting_get_nth (sort_info, i);

		data->ascending = scol.ascending;
		data->col = e_table_header_get_column_by_col_idx (full_header, scol.column);
		if (data->col == NULL)
			data->col = e_table_header_get_column (full_header, e_table_header_count (full_header) - 1);

		g_ptr_array_add (sort_data.sort_columns, data);
	}

	camel_folder_summary_prepare_fetch_all (ml->folder->summary, NULL);

	for (i = 0; i < uids->len; i++) {
		gchar *uid;
		struct sort_message_info_data *md = g_new0 (struct sort_message_info_data, 1);

		uid = g_ptr_array_index (uids, i);
		md->mi = camel_folder_get_message_info (ml->folder, uid);
		md->values = g_ptr_array_sized_new (len);

		g_hash_table_insert (sort_data.message_infos, uid, md);
	}

	g_qsort_with_data (uids->pdata, uids->len, sizeof (gpointer), cmp_array_uids, &sort_data);

	g_hash_table_foreach (sort_data.message_infos, (GHFunc) free_message_info_data, &sort_data);
	g_hash_table_destroy (sort_data.message_infos);

	g_ptr_array_foreach (sort_data.sort_columns, (GFunc) g_free, NULL);
	g_ptr_array_free (sort_data.sort_columns, TRUE);

	e_table_sorting_utils_free_cmp_cache (sort_data.cmp_cache);
}

/* ** REGENERATE MESSAGELIST ********************************************** */
struct _regen_list_msg {
	MailMsg base;

	gint complete;

	MessageList *ml;
	gchar *search;
	gchar *hideexpr;
	CamelFolderChangeInfo *changes;
	gboolean dotree;	/* we are building a tree */
	gboolean hidedel;	/* we want to/dont want to show deleted messages */
	gboolean hidejunk;	/* we want to/dont want to show junk messages */
	gboolean thread_subject;
	CamelFolderThread *tree;

	CamelFolder *folder;
	GPtrArray *summary;

	gint last_row; /* last selected (cursor) row */

	xmlDoc *expand_state; /* stored expanded state of the previous view */
};

/*
  maintain copy of summary

  any new messages added
  any removed removed, etc.

  use vfolder to implement searches ???

 */

static gchar *
regen_list_desc (struct _regen_list_msg *m)
{
	return g_strdup (_("Generating message list"));
}

static void
regen_list_exec (struct _regen_list_msg *m)
{
	GPtrArray *uids, *searchuids = NULL;
	CamelMessageInfo *info;
	ETreePath cursor;
	ETree *tree;
	gint i;
	gchar *expr = NULL;

	if (m->folder != m->ml->folder)
		return;

	tree = E_TREE (m->ml);
	cursor = e_tree_get_cursor (tree);
	if (cursor)
		m->last_row = e_tree_table_adapter_row_of_node (e_tree_get_table_adapter (tree), cursor);

	e_profile_event_emit("list.getuids", m->folder->full_name, 0);

	/* if we have hidedeleted on, use a search to find it out, merge with existing search if set */
	if (!camel_folder_has_search_capability(m->folder)) {
		/* if we have no search capability, dont let search or hide deleted work */
		expr = NULL;
	} else if (m->hidedel) {
		if (m->hidejunk) {
			if (m->search) {
				expr = g_alloca(strlen(m->search) + 92);
				sprintf(expr, "(and (match-all (and (not (system-flag \"deleted\")) (not (system-flag \"junk\"))))\n %s)", m->search);
			} else
				expr = (gchar *) "(match-all (and (not (system-flag \"deleted\")) (not (system-flag \"junk\"))))";
		} else {
			if (m->search) {
				expr = g_alloca(strlen(m->search) + 64);
				sprintf(expr, "(and (match-all (not (system-flag \"deleted\")))\n %s)", m->search);
			} else
				expr = (gchar *) "(match-all (not (system-flag \"deleted\")))";
		}
	} else {
		if (m->hidejunk) {
			if (m->search) {
				expr = g_alloca(strlen(m->search) + 64);
				sprintf(expr, "(and (match-all (not (system-flag \"junk\")))\n %s)", m->search);
			} else
				expr = (gchar *) "(match-all (not (system-flag \"junk\")))";
		} else {
			expr = m->search;
		}
	}

	if (expr == NULL) {
		uids = camel_folder_get_uids (m->folder);
	} else {
		gboolean store_has_vjunk = folder_store_supports_vjunk_folder (m->folder);

		searchuids = uids = camel_folder_search_by_expression (
			m->folder, expr, &m->base.error);
		/* If m->changes is not NULL, then it means we are called from folder_changed event,
		   thus we will keep the selected message to be sure it doesn't disappear because
		   it no longer belong to our search filter. */
		if (uids && ((m->changes && m->ml->cursor_uid) || m->ml->ensure_uid)) {
			const gchar *looking_for = m->ml->cursor_uid;
			/* ensure_uid has precedence of cursor_uid */
			if (m->ml->ensure_uid)
				looking_for = m->ml->ensure_uid;

			for (i = 0; i < uids->len; i++) {
				if (g_str_equal (looking_for, uids->pdata[i]))
					break;
			}

			/* cursor_uid has been filtered out */
			if (i == uids->len) {
				CamelMessageInfo *looking_info = camel_folder_get_message_info (m->folder, looking_for);

				if (looking_info) {
					gboolean is_deleted = (camel_message_info_flags (looking_info) & CAMEL_MESSAGE_DELETED) != 0;
					gboolean is_junk = store_has_vjunk && (camel_message_info_flags (looking_info) & CAMEL_MESSAGE_JUNK) != 0;

					/* I would really like to check for CAMEL_MESSAGE_FOLDER_FLAGGED on a message,
					   so I would know whether it was changed locally, and then just check the changes
					   struct whether change came from the server, but with periodical save it doesn't
					   matter. So here just check whether the file was deleted and we show it based
					   on the flag whether we can view deleted messages or not. */

					if ((!is_deleted || (is_deleted && !m->hidedel)) && (!is_junk || (is_junk && !m->hidejunk)))
						g_ptr_array_add (uids, (gpointer) camel_pstring_strdup (looking_for));

					camel_folder_free_message_info (m->folder, looking_info);
				}
			}
		}
	}

	if (m->base.error != NULL)
		return;

	e_profile_event_emit("list.threaduids", m->folder->full_name, 0);

	/* camel_folder_summary_prepare_fetch_all (m->folder->summary, NULL); */
	if (!camel_operation_cancel_check(m->base.cancel)) {
		/* update/build a new tree */
		if (m->dotree) {
			ml_sort_uids_by_tree (m->ml, uids);

			if (m->tree)
				camel_folder_thread_messages_apply (m->tree, uids);
			else
				m->tree = camel_folder_thread_messages_new (m->folder, uids, m->thread_subject);
		} else {
			camel_folder_sort_uids (m->ml->folder, uids);
			m->summary = g_ptr_array_new ();

			camel_folder_summary_prepare_fetch_all (
				m->folder->summary, NULL);

			for (i = 0; i < uids->len; i++) {
				info = camel_folder_get_message_info (m->folder, uids->pdata[i]);
				if (info)
					g_ptr_array_add(m->summary, info);
			}
		}

		m->complete = TRUE;
	}

	if (searchuids)
		camel_folder_search_free (m->folder, searchuids);
	else
		camel_folder_free_uids (m->folder, uids);
}

static void
regen_list_done (struct _regen_list_msg *m)
{
	ETree *tree;

	if (m->ml->priv->destroyed)
		return;

	if (!m->complete)
		return;

	if (camel_operation_cancel_check(m->base.cancel))
		return;

	if (m->ml->folder != m->folder)
		return;

	tree = E_TREE (m->ml);

	e_tree_show_cursor_after_reflow (tree);

	g_signal_handlers_block_by_func (e_tree_get_table_adapter (tree), ml_tree_sorting_changed, m->ml);

	e_profile_event_emit("list.buildtree", m->folder->full_name, 0);

	if (m->dotree) {
		gboolean forcing_expand_state = m->ml->expand_all || m->ml->collapse_all;

		if (m->ml->just_set_folder) {
			m->ml->just_set_folder = FALSE;
			if (m->expand_state) {
				/* rather load state from disk than use the memory data when changing folders */
				xmlFreeDoc (m->expand_state);
				m->expand_state = NULL;
			}
		}

		if (forcing_expand_state)
			e_tree_force_expanded_state (tree, m->ml->expand_all ? 1 : -1);

		build_tree (m->ml, m->tree, m->changes);
		if (m->ml->thread_tree)
			camel_folder_thread_messages_unref(m->ml->thread_tree);
		m->ml->thread_tree = m->tree;
		m->tree = NULL;

		if (forcing_expand_state) {
			if (m->ml->folder != NULL && tree != NULL)
				save_tree_state (m->ml);
			/* do not forget to set this back to use the default value... */
			e_tree_force_expanded_state (tree, 0);
		} else
			load_tree_state (m->ml, m->expand_state);

		m->ml->expand_all = 0;
		m->ml->collapse_all = 0;
	} else
		build_flat (m->ml, m->summary, m->changes);

        if (m->ml->search && m->ml->search != m->search)
                g_free (m->ml->search);
	m->ml->search = m->search;
	m->search = NULL;

	g_mutex_lock (m->ml->regen_lock);
	m->ml->regen = g_list_remove(m->ml->regen, m);
	g_mutex_unlock (m->ml->regen_lock);

	if (m->ml->regen == NULL && m->ml->pending_select_uid) {
		gchar *uid;
		gboolean with_fallback;

		uid = m->ml->pending_select_uid;
		m->ml->pending_select_uid = NULL;
		with_fallback = m->ml->pending_select_fallback;
		message_list_select_uid (m->ml, uid, with_fallback);
		g_free(uid);
	} else if (m->ml->regen == NULL && m->ml->cursor_uid == NULL && m->last_row != -1) {
		ETreeTableAdapter *etta = e_tree_get_table_adapter (tree);

		if (m->last_row >= e_table_model_row_count (E_TABLE_MODEL (etta)))
			m->last_row = e_table_model_row_count (E_TABLE_MODEL (etta)) - 1;

		if (m->last_row >= 0) {
			ETreePath path;

			path = e_tree_table_adapter_node_at_row (etta, m->last_row);
			if (path)
				select_path (m->ml, path);
		}
	}

	if (gtk_widget_get_visible (GTK_WIDGET (m->ml))) {
		if (e_tree_row_count (E_TREE (m->ml)) <= 0) {
			/* space is used to indicate no search too */
			if (m->ml->search && *m->ml->search && strcmp (m->ml->search, " ") != 0)
				e_tree_set_info_message (tree, _("No message satisfies your search criteria. Either clear search with Search->Clear menu item or change it."));
			else
				e_tree_set_info_message (tree, _("There are no messages in this folder."));
		} else
			e_tree_set_info_message (tree, NULL);
	}

	g_signal_handlers_unblock_by_func (e_tree_get_table_adapter (tree), ml_tree_sorting_changed, m->ml);

	g_signal_emit (m->ml, message_list_signals[MESSAGE_LIST_BUILT], 0);
	m->ml->priv->any_row_changed = FALSE;

#if HAVE_CLUTTER
	if (m->ml->priv->timeline && clutter_timeline_is_playing(m->ml->priv->timeline)) {
		clutter_timeline_pause (m->ml->priv->timeline);
		clutter_actor_hide (m->ml->priv->search_texture);
	} else {
		ClutterActor *pane = g_object_get_data ((GObject *)m->ml, "actor");

		if (pane) {
			clutter_actor_set_opacity (pane, 0);
			clutter_actor_animate (
				pane, CLUTTER_EASE_OUT_SINE,
				150, "opacity", 255, NULL);
		}
	}
#endif
}

static void
regen_list_free (struct _regen_list_msg *m)
{
	gint i;

	e_profile_event_emit("list.regenerated", m->folder->full_name, 0);

	if (m->summary) {
		for (i = 0; i < m->summary->len; i++)
			camel_folder_free_message_info (m->folder, m->summary->pdata[i]);
		g_ptr_array_free (m->summary, TRUE);
	}

	if (m->tree)
		camel_folder_thread_messages_unref (m->tree);

	g_free (m->search);
	g_free (m->hideexpr);

	g_object_unref (m->folder);

	if (m->changes)
		camel_folder_change_info_free (m->changes);

	/* we have to poke this here as well since we might've been cancelled and regened wont get called */
	g_mutex_lock (m->ml->regen_lock);
	m->ml->regen = g_list_remove(m->ml->regen, m);
	g_mutex_unlock (m->ml->regen_lock);

	if (m->expand_state)
		xmlFreeDoc (m->expand_state);

	g_object_unref(m->ml);
}

static MailMsgInfo regen_list_info = {
	sizeof (struct _regen_list_msg),
	(MailMsgDescFunc) regen_list_desc,
	(MailMsgExecFunc) regen_list_exec,
	(MailMsgDoneFunc) regen_list_done,
	(MailMsgFreeFunc) regen_list_free
};

static gboolean
ml_regen_timeout(struct _regen_list_msg *m)
{
	e_profile_event_emit("list.regenerate", m->folder->full_name, 0);

	g_mutex_lock (m->ml->regen_lock);
	m->ml->regen = g_list_prepend(m->ml->regen, m);
	g_mutex_unlock (m->ml->regen_lock);
	/* TODO: we should manage our own thread stuff, would make cancelling outstanding stuff easier */
	mail_msg_fast_ordered_push (m);

	m->ml->regen_timeout_msg = NULL;
	m->ml->regen_timeout_id = 0;

	return FALSE;
}

static void
mail_regen_cancel(MessageList *ml)
{
	/* cancel any outstanding regeneration requests, not we don't clear, they clear themselves */
	if (ml->regen) {
		GList *l;

		g_mutex_lock (ml->regen_lock);

		l = ml->regen;
		while (l) {
			MailMsg *mm = l->data;

			if (mm->cancel)
				camel_operation_cancel(mm->cancel);
			l = l->next;
		}

		g_mutex_unlock (ml->regen_lock);
	}

	/* including unqueued ones */
	if (ml->regen_timeout_id) {
		g_source_remove(ml->regen_timeout_id);
		ml->regen_timeout_id = 0;
		mail_msg_unref(ml->regen_timeout_msg);
		ml->regen_timeout_msg = NULL;
	}
}

static void
mail_regen_list (MessageList *ml, const gchar *search, const gchar *hideexpr, CamelFolderChangeInfo *changes)
{
	struct _regen_list_msg *m;
	GConfClient *gconf;

	/* report empty search as NULL, not as one/two-space string */
	if (search && (strcmp (search, " ") == 0 || strcmp (search, "  ") == 0))
		search = NULL;

	if (ml->folder == NULL) {
		if (ml->search != search) {
			g_free(ml->search);
			ml->search = g_strdup(search);
		}
		return;
	}

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

	m = mail_msg_new (&regen_list_info);
	m->ml = g_object_ref (ml);
	m->search = g_strdup (search);
	m->hideexpr = g_strdup (hideexpr);
	m->changes = changes;
	m->dotree = ml->threaded;
	m->hidedel = ml->hidedeleted;
	m->hidejunk = ml->hidejunk;
	m->thread_subject = gconf_client_get_bool (gconf, "/apps/evolution/mail/display/thread_subject", NULL);
	m->folder = g_object_ref (ml->folder);
	m->last_row = -1;
	m->expand_state = NULL;

	if ((!m->hidedel || !m->dotree) && ml->thread_tree) {
		camel_folder_thread_messages_unref(ml->thread_tree);
		ml->thread_tree = NULL;
	} else if (ml->thread_tree) {
		m->tree = ml->thread_tree;
		camel_folder_thread_messages_ref(m->tree);
	}

	if (e_tree_row_count (E_TREE (ml)) <= 0) {
		if (gtk_widget_get_visible (GTK_WIDGET (ml))) {
			/* there is some info why the message list is empty, let it be something useful */
			gchar *txt = g_strconcat (_("Generating message list"), "..." , NULL);

			e_tree_set_info_message (E_TREE (m->ml), txt);

			g_free (txt);
		}
	} else if (ml->priv->any_row_changed && m->dotree && !ml->just_set_folder && (!ml->search || g_str_equal (ml->search, " "))) {
		/* there has been some change on any row, if it was an expand state change,
		   then let it save; if not, then nothing happen. */
		message_list_save_state (ml);
	} else if (m->dotree && !ml->just_set_folder) {
		/* remember actual expand state and restore it after regen */
		m->expand_state = e_tree_save_expanded_state_xml (E_TREE (ml));
	}

	/* if we're busy already kick off timeout processing, so normal updates are immediate */
	if (ml->regen == NULL)
		ml_regen_timeout(m);
	else {
		ml->regen_timeout_msg = m;
		ml->regen_timeout_id = g_timeout_add(500, (GSourceFunc)ml_regen_timeout, m);
	}
}
