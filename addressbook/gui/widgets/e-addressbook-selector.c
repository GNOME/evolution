/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-addressbook-selector.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-addressbook-selector.h"

#include <eab-book-util.h>
#include <eab-contact-merging.h>

#define E_ADDRESSBOOK_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_SELECTOR, EAddressbookSelectorPrivate))

typedef struct _MergeContext MergeContext;

struct _EAddressbookSelectorPrivate {
	gint dummy_value;
};

struct _MergeContext {
	EBook *source_book;
	EBook *target_book;

	EContact *current_contact;
	GList *remaining_contacts;
	guint pending_removals;

	gint remove_from_source : 1;
	gint copy_done          : 1;
};

enum {
	DND_TARGET_TYPE_VCARD,
	DND_TARGET_TYPE_SOURCE_VCARD
};

static GtkTargetEntry drag_types[] = {
	{ "text/x-vcard", 0, DND_TARGET_TYPE_VCARD },
	{ "text/x-source-vcard", 0, DND_TARGET_TYPE_SOURCE_VCARD }
};

static gpointer parent_class;

static void
merge_context_next (MergeContext *merge_context)
{
	GList *list;

	list = merge_context->remaining_contacts;
	merge_context->current_contact = list->data;
	list = g_list_delete_link (list, list);
	merge_context->remaining_contacts = list;
}

static MergeContext *
merge_context_new (EBook *source_book,
                   EBook *target_book,
                   GList *contact_list)
{
	MergeContext *merge_context;

	merge_context = g_slice_new0 (MergeContext);
	merge_context->source_book = source_book;
	merge_context->target_book = target_book;
	merge_context->remaining_contacts = contact_list;
	merge_context_next (merge_context);

	return merge_context;
}

static void
merge_context_free (MergeContext *merge_context)
{
	if (merge_context->source_book != NULL)
		g_object_unref (merge_context->source_book);

	if (merge_context->target_book != NULL)
		g_object_unref (merge_context->target_book);

	g_slice_free (MergeContext, merge_context);
}

static void
addressbook_selector_removed_cb (EBook *book,
                                 EBookStatus status,
                                 MergeContext *merge_context)
{
	merge_context->pending_removals--;

	if (merge_context->remaining_contacts != NULL)
		return;

	if (merge_context->pending_removals > 0)
		return;

	merge_context_free (merge_context);
}

static void
addressbook_selector_merge_next_cb (EBook *book,
                                    EBookStatus status,
                                    const gchar *id,
                                    MergeContext *merge_context)
{
	if (merge_context->remove_from_source && status == E_BOOK_ERROR_OK) {
		/* Remove previous contact from source. */
		e_book_async_remove_contact (
			merge_context->source_book,
			merge_context->current_contact,
			(EBookCallback) addressbook_selector_removed_cb,
			merge_context);
		merge_context->pending_removals++;
	}

	g_object_unref (merge_context->current_contact);

	if (merge_context->remaining_contacts != NULL) {
		merge_context_next (merge_context);
		eab_merging_book_add_contact (
			merge_context->target_book,
			merge_context->current_contact,
			(EBookIdCallback) addressbook_selector_merge_next_cb,
			merge_context);

	} else if (merge_context->pending_removals == 0)
		merge_context_free (merge_context);
}

static void
addressbook_selector_drag_leave (GtkWidget *widget,
                                 GdkDragContext *context,
                                 guint time_)
{
	GtkTreeView *tree_view;
	GtkTreeViewDropPosition pos;

	tree_view = GTK_TREE_VIEW (widget);
	pos = GTK_TREE_VIEW_DROP_BEFORE;

	gtk_tree_view_set_drag_dest_row (tree_view, NULL, pos);
}

static gboolean
addressbook_selector_drag_motion (GtkWidget *widget,
                                  GdkDragContext *context,
                                  gint x,
                                  gint y,
                                  guint time_)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = 0;
	gpointer object;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, 0, &object, -1);

	if (!E_IS_SOURCE (object) || e_source_get_readonly (object))
		goto exit;

	gtk_tree_view_set_drag_dest_row (
		tree_view, path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);

	pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
	gtk_tree_view_set_drag_dest_row (tree_view, path, pos);

	if (context->actions & GDK_ACTION_MOVE)
		action = GDK_ACTION_MOVE;
	else
		action = context->suggested_action;

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (object != NULL)
		g_object_ref (object);

	gdk_drag_status (context, action, GDK_CURRENT_TIME);

	return TRUE;
}

static gboolean
addressbook_selector_drag_drop (GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                guint time_)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean drop_zone;
	gboolean valid;
	gpointer object;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_path_at_pos (
		tree_view, x, y, &path, NULL, NULL, NULL))
		return FALSE;

	valid = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	g_return_val_if_fail (valid, FALSE);

	gtk_tree_model_get (model, &iter, 0, &object, -1);
	drop_zone = !E_IS_SOURCE_GROUP (object);
	g_object_unref (object);

	return drop_zone;
}

static void
addressbook_selector_drag_data_received (GtkWidget *widget,
                                         GdkDragContext *context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData *selection_data,
                                         guint info,
                                         guint time_)
{
	MergeContext *merge_context;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	EBook *source_book;
	EBook *target_book;
	GList *list;
	const gchar *string;
	gboolean remove_from_source;
	gboolean success = FALSE;
	gpointer object;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	string = (gchar *) selection_data->data;
	remove_from_source = (context->action == GDK_ACTION_MOVE);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, 0, &object, -1);

	if (!E_IS_SOURCE (object) || e_source_get_readonly (object))
		goto exit;

	target_book = e_book_new (object, NULL);
	if (target_book == NULL)
		goto exit;

	e_book_open (target_book, FALSE, NULL);

	eab_book_and_contact_list_from_string (string, &source_book, &list);
	if (list == NULL)
		goto exit;

	/* XXX Get the currently selected EBook. */

	merge_context = merge_context_new (source_book, target_book, list);
	merge_context->remove_from_source = remove_from_source;

	eab_merging_book_add_contact (
		target_book, merge_context->current_contact,
		(EBookIdCallback) addressbook_selector_merge_next_cb,
		merge_context);

	success = TRUE;

exit:
	if (path)
		gtk_tree_path_free (path);

	if (object)
		g_object_unref (object);

	gtk_drag_finish (context, success, remove_from_source, time_);
}

static void
addressbook_selector_class_init (EAddressbookSelectorClass *class)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAddressbookSelectorPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->drag_leave = addressbook_selector_drag_leave;
	widget_class->drag_motion = addressbook_selector_drag_motion;
	widget_class->drag_drop = addressbook_selector_drag_drop;
	widget_class->drag_data_received = addressbook_selector_drag_data_received;
}

static void
addressbook_selector_init (EAddressbookSelector *selector)
{
	selector->priv = E_ADDRESSBOOK_SELECTOR_GET_PRIVATE (selector);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

GType
e_addressbook_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EAddressbookSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) addressbook_selector_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAddressbookSelector),
			0,     /* n_preallocs */
			(GInstanceInitFunc) addressbook_selector_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SOURCE_SELECTOR, "EAddressbookSelector",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_addressbook_selector_new (ESourceList *source_list)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	return g_object_new (
		E_TYPE_ADDRESSBOOK_SELECTOR,
		"source-list", source_list, NULL);
}
