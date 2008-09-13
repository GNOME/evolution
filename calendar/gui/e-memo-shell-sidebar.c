/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-sidebar.c
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

#include "e-memo-shell-sidebar.h"

#include <glib/gi18n.h>
#include <libedataserverui/e-source-selector.h>

#include <e-memos.h>
#include <e-memo-shell-view.h>
#include <e-calendar-selector.h>

#define E_MEMO_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebarPrivate))

struct _EMemoShellSidebarPrivate {
	GtkWidget *selector;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

static gpointer parent_class;

static void
memo_shell_sidebar_update (EShellSidebar *shell_sidebar)
{
	EShellView *shell_view;
	EMemos *memos;
	ETable *table;
	ECalModel *model;
	EMemoTable *memo_table;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	memos = e_memo_shell_view_get_memos (E_MEMO_SHELL_VIEW (shell_view));
	memo_table = e_memos_get_calendar_table (memos);
	model = e_memo_table_get_model (memo_table);
	table = e_memo_table_get_table (memo_table);

	n_rows = e_table_model_get_row_count (model);
	n_selected = e_table_selected_count (table);

	string = g_string_sized_new (64);

	format = ngettext ("%d memo", "%d memos", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}

static void
memo_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_memo_shell_sidebar_get_selector (
				E_MEMO_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_shell_sidebar_dispose (GObject *object)
{
	EMemoShellSidebarPrivate *priv;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
memo_shell_sidebar_constructed (GObject *object)
{
	EMemoShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMemoShellView *memo_shell_view;
	ESourceList *source_list;
	GtkContainer *container;
	GtkWidget *widget;
	EMemos *memos;
	ETable *table;
	ECalModel *model;
	EMemoTable *memo_table;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	memo_shell_view = E_MEMO_SHELL_VIEW (shell_view);
	source_list = e_memo_shell_view_get_source_list (memo_shell_view);

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	widget = e_calendar_selector_new (source_list);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Setup signal handlers. */

	memos = e_memo_shell_view_get_memos (memo_shell_view);
	memo_table = e_memos_get_calendar_table (memos);
	model = e_memo_table_get_model (memo_table);
	table = e_memo_table_get_table (memo_table);

	g_signal_connect_swapped (
		model, "model-changed",
		G_CALLBACK (memo_shell_sidebar_update),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "model-rows-deleted",
		G_CALLBACK (memo_shell_sidebar_update),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "model-rows-inserted",
		G_CALLBACK (memo_shell_sidebar_update),
		shell_sidebar);

	g_signal_connect_swapped (
		model, "selection-change",
		G_CALLBACK (memo_shell_sidebar_update),
		shell_sidebar);

	memo_shell_sidebar_update (shell_sidebar);
}

static void
memo_shell_sidebar_class_init (EMemoShellSidebarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = memo_shell_sidebar_get_property;
	object_class->dispose = memo_shell_sidebar_dispose;
	object_class->constructed = memo_shell_sidebar_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of memo lists"),
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));
}

static void
memo_shell_sidebar_init (EMemoShellSidebar *memo_shell_sidebar)
{
	memo_shell_sidebar->priv =
		E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (memo_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_memo_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMemoShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) memo_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMemoShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) memo_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "EMemoShellSidebar",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_memo_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MEMO_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

GtkWidget *
e_memo_shell_sidebar_get_selector (EMemoShellSidebar *memo_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	return memo_shell_sidebar->priv->selector;
}
