/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-content.h
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

#include "e-memo-shell-content.h"

#include <glib/gi18n.h>

#include "e-util/gconf-bridge.h"

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/e-memo-table.h"
#include "calendar/gui/e-memo-table-config.h"

#include "widgets/menus/gal-view-etable.h"

#define E_MEMO_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_CONTENT, EMemoShellContentPrivate))

#define E_MEMO_TABLE_DEFAULT_STATE \
	"<?xml version=\"1.0\"?>" \
	"<ETableState>" \
	"  <column source=\"1\"/>" \
	"  <column source=\"0\"/>" \
	"  <column source=\"2\"/>" \
	"  <grouping/>" \
	"</ETableState>"

struct _EMemoShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *memo_table;
	GtkWidget *memo_preview;

	EMemoTableConfig *table_config;
	GalViewInstance *view_instance;

	gchar *current_uid;
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE
};

enum {
	TARGET_VCALENDAR
};

static GtkTargetEntry drag_types[] = {
	{ "text/calendar", 0, TARGET_VCALENDAR },
	{ "text/x-calendar", 0, TARGET_VCALENDAR }
};

static gpointer parent_class;

static void
memo_shell_content_changed_cb (EMemoShellContent *memo_shell_content,
                               GalViewInstance *view_instance)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	gchar *view_id;

	shell_content = E_SHELL_CONTENT (memo_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	view_id = gal_view_instance_get_current_view_id (view_instance);
	e_shell_view_set_view_id (shell_view, view_id);
	g_free (view_id);
}

static void
memo_shell_content_display_view_cb (EMemoShellContent *memo_shell_content,
                                    GalView *gal_view)
{
	EMemoTable *memo_table;
	ETable *table;

	if (!GAL_IS_VIEW_ETABLE (gal_view))
		return;

	memo_table = E_MEMO_TABLE (memo_shell_content->priv->memo_table);
	table = e_memo_table_get_table (memo_table);

	gal_view_etable_attach_table (GAL_VIEW_ETABLE (gal_view), table);
}

static void
memo_shell_content_table_drag_data_get_cb (EMemoShellContent *memo_shell_content,
                                           gint row,
                                           gint col,
                                           GdkDragContext *context,
                                           GtkSelectionData *selection_data,
                                           guint info,
                                           guint time)
{
	/* FIXME */
}

static void
memo_shell_content_table_drag_data_delete_cb (EMemoShellContent *memo_shell_content,
                                              gint row,
                                              gint col,
                                              GdkDragContext *context)
{
	/* Moved components are deleted from source immediately when moved,
	 * because some of them can be part of destination source, and we
	 * don't want to delete not-moved tasks.  There is no such information
	 * which event has been moved and which not, so skip this method. */
}

static void
memo_shell_content_cursor_change_cb (EMemoShellContent *memo_shell_content,
                                     gint row,
                                     ETable *table)
{
	EMemoPreview *memo_preview;
	EMemoTable *memo_table;
	ECalModel *model;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	const gchar *uid;

	memo_preview = E_MEMO_PREVIEW (memo_shell_content->priv->memo_preview);
	memo_table = E_MEMO_TABLE (memo_shell_content->priv->memo_table);

	if (e_table_selected_count (table) != 1) {
		e_memo_preview_clear (memo_preview);
		return;
	}

	model = e_memo_table_get_model (memo_table);
	row = e_table_get_cursor_row (table);
	comp_data = e_cal_model_get_component_at (model, row);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		comp, icalcomponent_new_clone (comp_data->icalcomp));
	e_memo_preview_display (memo_preview, comp_data->client, comp);

	e_cal_component_get_uid (comp, &uid);
	g_free (memo_shell_content->priv->current_uid);
	memo_shell_content->priv->current_uid = g_strdup (uid);

	g_object_unref (comp);
}

static void
memo_shell_content_selection_change_cb (EMemoShellContent *memo_shell_content,
                                        ETable *table)
{
	EMemoPreview *memo_preview;

	memo_preview = E_MEMO_PREVIEW (memo_shell_content->priv->memo_preview);

	/* XXX Old code emits a "selection-changed" signal here. */

	if (e_table_selected_count (table) != 1)
		e_memo_preview_clear (memo_preview);
}

static void
memo_shell_content_model_row_changed_cb (EMemoShellContent *memo_shell_content,
                                         gint row,
                                         ETableModel *model)
{
	ECalModelComponent *comp_data;
	EMemoTable *memo_table;
	ETable *table;
	const gchar *current_uid;
	const gchar *uid;

	current_uid = memo_shell_content->priv->current_uid;
	if (current_uid == NULL)
		return;

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (comp_data == NULL)
		return;

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	if (g_strcmp0 (uid, current_uid) != 0)
		return;

	memo_table = E_MEMO_TABLE (memo_shell_content->priv->memo_table);
	table = e_memo_table_get_table (memo_table);

	memo_shell_content_cursor_change_cb (memo_shell_content, 0, table);
}

static void
memo_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			e_memo_shell_content_set_preview_visible (
				E_MEMO_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value, e_memo_shell_content_get_preview_visible (
				E_MEMO_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_shell_content_dispose (GObject *object)
{
	EMemoShellContentPrivate *priv;

	priv = E_MEMO_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->memo_table != NULL) {
		g_object_unref (priv->memo_table);
		priv->memo_table = NULL;
	}

	if (priv->memo_preview != NULL) {
		g_object_unref (priv->memo_preview);
		priv->memo_preview = NULL;
	}

	if (priv->table_config != NULL) {
		g_object_unref (priv->table_config);
		priv->table_config = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
memo_shell_content_finalize (GObject *object)
{
	EMemoShellContentPrivate *priv;

	priv = E_MEMO_SHELL_CONTENT_GET_PRIVATE (object);

	g_free (priv->current_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
memo_shell_content_constructed (GObject *object)
{
	EMemoShellContentPrivate *priv;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	ECalModel *model;
	ETable *table;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *key;

	priv = E_MEMO_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_memo_table_new (shell_view);
	gtk_paned_add1 (GTK_PANED (container), widget);
	priv->memo_table = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add2 (GTK_PANED (container), widget);

	container = widget;

	widget = e_memo_preview_new ();
	e_memo_preview_set_default_timezone (
		E_MEMO_PREVIEW (widget),
		calendar_config_get_icaltimezone ());
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->memo_preview = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Configure the memo table. */

	widget = E_MEMO_TABLE (priv->memo_table)->etable;
	table = e_table_scrolled_get_table (E_TABLE_SCROLLED (widget));
	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memo_table));

	priv->table_config = e_memo_table_config_new (
		E_MEMO_TABLE (priv->memo_table));

	e_table_set_state (table, E_MEMO_TABLE_DEFAULT_STATE);

	e_table_drag_source_set (
		table, GDK_BUTTON1_MASK,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK);

	g_signal_connect_swapped (
		table, "table-drag-data-get",
		G_CALLBACK (memo_shell_content_table_drag_data_get_cb),
		object);

	g_signal_connect_swapped (
		table, "table-drag-data-delete",
		G_CALLBACK (memo_shell_content_table_drag_data_delete_cb),
		object);

	g_signal_connect_swapped (
		table, "cursor-change",
		G_CALLBACK (memo_shell_content_cursor_change_cb),
		object);

	g_signal_connect_swapped (
		table, "selection-change",
		G_CALLBACK (memo_shell_content_selection_change_cb),
		object);

	g_signal_connect_swapped (
		model, "model-row-changed",
		G_CALLBACK (memo_shell_content_model_row_changed_cb),
		object);

	/* Load the view instance. */

	view_instance = gal_view_instance_new (view_collection, NULL);
	g_signal_connect_swapped (
		view_instance, "changed",
		G_CALLBACK (memo_shell_content_changed_cb),
		object);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (memo_shell_content_display_view_cb),
		object);
	gal_view_instance_load (view_instance);
	priv->view_instance = view_instance;

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/calendar/display/memo_vpane_position";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");
}

static void
memo_shell_content_class_init (EMemoShellContentClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = memo_shell_content_set_property;
	object_class->get_property = memo_shell_content_get_property;
	object_class->dispose = memo_shell_content_dispose;
	object_class->finalize = memo_shell_content_finalize;
	object_class->constructed = memo_shell_content_constructed;

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			_("Preview is Visible"),
			_("Whether the preview pane is visible"),
			TRUE,
			G_PARAM_READWRITE));
}

static void
memo_shell_content_init (EMemoShellContent *memo_shell_content)
{
	memo_shell_content->priv =
		E_MEMO_SHELL_CONTENT_GET_PRIVATE (memo_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_memo_shell_content_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMemoShellContentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) memo_shell_content_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMemoShellContent),
			0,     /* n_preallocs */
			(GInstanceInitFunc) memo_shell_content_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_CONTENT, "EMemoShellContent",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_memo_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MEMO_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

EMemoPreview *
e_memo_shell_content_get_memo_preview (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return E_MEMO_PREVIEW (memo_shell_content->priv->memo_preview);
}

EMemoTable *
e_memo_shell_content_get_memo_table (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return E_MEMO_TABLE (memo_shell_content->priv->memo_table);
}

GalViewInstance *
e_memo_shell_content_get_view_instance (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return memo_shell_content->priv->view_instance;
}

gboolean
e_memo_shell_content_get_preview_visible (EMemoShellContent *memo_shell_content)
{
	GtkPaned *paned;
	GtkWidget *child;

	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), FALSE);

	paned = GTK_PANED (memo_shell_content->priv->paned);
	child = gtk_paned_get_child2 (paned);

	return GTK_WIDGET_VISIBLE (child);
}

void
e_memo_shell_content_set_preview_visible (EMemoShellContent *memo_shell_content,
                                          gboolean preview_visible)
{
	GtkPaned *paned;
	GtkWidget *child;

	g_return_if_fail (E_IS_MEMO_SHELL_CONTENT (memo_shell_content));

	paned = GTK_PANED (memo_shell_content->priv->paned);
	child = gtk_paned_get_child2 (paned);

	if (preview_visible)
		gtk_widget_show (child);
	else
		gtk_widget_hide (child);

	g_object_notify (G_OBJECT (memo_shell_content), "preview-visible");
}
