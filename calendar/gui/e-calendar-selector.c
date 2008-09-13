/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-calendar-selector.c
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

#include "e-calendar-selector.h"

#include <libecal/e-cal.h>
#include "common/authentication.h"

#define E_CALENDAR_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CALENDAR_SELECTOR, ECalendarSelectorPrivate))

struct _ECalendarSelectorPrivate {
	gint dummy_value;
};

enum {
	DND_TARGET_TYPE_CALENDAR_LIST
};

static GtkTargetEntry drag_types[] = {
	{ "text/calendar", 0, DND_TARGET_TYPE_CALENDAR_LIST },
	{ "text/x-calendar", 0, DND_TARGET_TYPE_CALENDAR_LIST }
};

static gpointer parent_class;

static void
calendar_selector_drag_leave (GtkWidget *widget,
                              GdkDragContext *context,
                              guint time_)
{
	/* XXX This is exactly the same as in EAddressbookSelector.
	 *     Consider merging this callback into ESourceSelector. */

	GtkTreeView *tree_view;
	GtkTreeViewDropPosition pos;

	tree_view = GTK_TREE_VIEW (widget);
	pos = GTK_TREE_VIEW_DROP_BEFORE;

	gtk_tree_view_set_drag_dest_row (tree_view, NULL, pos);
}

static gboolean
calendar_selector_drag_motion (GtkWidget *widget,
                               GdkDragContext *context,
                               gint x,
                               gint y,
                               guint time_)
{
	/* XXX This is exactly the same as in EAddressbookSelector.
	 *     Consider merging this callback into ESourceSelector. */

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

	if (E_IS_SOURCE_GROUP (object) || e_source_get_readonly (object))
		goto exit;

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
		g_object_unref (object);

	gdk_drag_status (context, action, time_);

	return TRUE;
}

static gboolean
calendar_selector_drag_drop (GtkWidget *widget,
                             GdkDragContext *context,
                             gint x,
                             gint y,
                             guint time_)
{
	/* XXX This is exactly the same as in EAddressbookSelector.
	 *     Consider merging this callback into ESourceSelector. */

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
	drop_zone = E_IS_SOURCE (object);
	g_object_unref (object);

	return drop_zone;
}

static void
calendar_selector_drag_data_received (GtkWidget *widget,
                                      GdkDragContext *context,
                                      gint x,
                                      gint y,
                                      GtkSelectionData *selection_data,
                                      guint info,
                                      guint time_)
{
	/* XXX This is NEARLY the same as in EAddressbookSelector.
	 *     Consider merging this callback into ESourceSelector.
	 *     Use a callback to allow subclasses to handle the
	 *     received selection data. */

	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	ECal *client;
	icalcomponent *icalcomp;
	const gchar *string;
	gboolean remove_from_source;
	gboolean success = FALSE;
	gpointer object;

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	string = (const gchar *) selection_data->data;
	remove_from_source = (context->action == GDK_ACTION_MOVE);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &path, NULL))
		goto exit;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto exit;

	gtk_tree_model_get (model, &iter, 0, &object, -1);

	if (!E_IS_SOURCE (object) || e_source_get_readonly (object))
		goto exit;

	icalcomp = icalparser_parse_string (string);

	if (icalcomp == NULL)
		goto exit;

	/* FIXME Deal with GDK_ACTION_ASK. */
	if (context->action == GDK_ACTION_COPY) {
		gchar *uid;

		uid = e_cal_component_gen_uid ();
		icalcomponent_set_uid (icalcomp, uid);
	}

	client = auth_new_cal_from_source (
		E_SOURCE (object), E_CAL_SOURCE_TYPE_EVENT);

	if (client != NULL) {
		if (e_cal_open (client, TRUE, NULL)) {
			success = TRUE;
			update_objects (client, icalcomp);
		}

		g_object_unref (client);
	}

	icalcomponent_free (icalcomp);

exit:
	if (path != NULL)
		gtk_tree_path_free (path);

	if (object != NULL)
		g_object_unref (object);

	gtk_drag_finish (context, success, remove_from_source, time_);
}

static void
calendar_selector_class_init (ECalendarSelectorClass *class)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalendarSelectorPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->drag_leave = calendar_selector_drag_leave;
	widget_class->drag_motion = calendar_selector_drag_motion;
	widget_class->drag_drop = calendar_selector_drag_drop;
	widget_class->drag_data_received = calendar_selector_drag_data_received;
}

static void
calendar_selector_init (ECalendarSelector *selector)
{
	selector->priv = E_CALENDAR_SELECTOR_GET_PRIVATE (selector);

	gtk_drag_dest_set (
		GTK_WIDGET (selector), GTK_DEST_DEFAULT_ALL,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

GType
e_calendar_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (ECalendarSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) calendar_selector_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalendarSelector),
			0,     /* n_preallocs */
			(GInstanceInitFunc) calendar_selector_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SOURCE_SELECTOR, "ECalendarSelector",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_calendar_selector_new (ESourceList *source_list)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	return g_object_new (
		E_TYPE_CALENDAR_SELECTOR,
		"source-list", source_list, NULL);
}
