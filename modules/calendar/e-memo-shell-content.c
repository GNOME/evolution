/*
 * e-memo-shell-content.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-memo-shell-content.h"

#include <glib/gi18n.h>

#include "e-util/e-binding.h"
#include "e-util/e-selection.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell-utils.h"
#include "widgets/menus/gal-view-etable.h"
#include "widgets/misc/e-paned.h"

#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-cal-model-memos.h"
#include "calendar/gui/e-memo-table.h"

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

	ECalModel *memo_model;
	GalViewInstance *view_instance;
	GtkOrientation orientation;

	gchar *current_uid;

	guint preview_visible	: 1;
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_ORIENTATION,
	PROP_PREVIEW_VISIBLE
};

enum {
	TARGET_VCALENDAR
};

static GtkTargetEntry drag_types[] = {
	{ (gchar *) "text/calendar", 0, TARGET_VCALENDAR },
	{ (gchar *) "text/x-calendar", 0, TARGET_VCALENDAR }
};

static gpointer parent_class;
static GType memo_shell_content_type;

static void
memo_shell_content_display_view_cb (EMemoShellContent *memo_shell_content,
                                    GalView *gal_view)
{
	EMemoTable *memo_table;
	ETable *table;

	if (!GAL_IS_VIEW_ETABLE (gal_view))
		return;

	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	table = e_memo_table_get_table (memo_table);

	gal_view_etable_attach_table (GAL_VIEW_ETABLE (gal_view), table);
}

static void
memo_shell_content_table_foreach_cb (gint model_row,
                                     gpointer user_data)
{
	ECalModelComponent *comp_data;
	icalcomponent *clone;
	icalcomponent *vcal;
	gchar *string;

	struct {
		ECalModel *model;
		GSList *list;
	} *foreach_data = user_data;

	comp_data = e_cal_model_get_component_at (
		foreach_data->model, model_row);

	vcal = e_cal_util_new_top_level ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
	icalcomponent_add_component (vcal, clone);

	/* String is owned by libical; do not free. */
	string = icalcomponent_as_ical_string (vcal);
	if (string != NULL) {
		ESource *source;
		const gchar *source_uid;

		source = e_cal_get_source (comp_data->client);
		source_uid = e_source_peek_uid (source);

		foreach_data->list = g_slist_prepend (
			foreach_data->list,
			g_strdup_printf ("%s\n%s", source_uid, string));
	}

	icalcomponent_free (vcal);
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
	EMemoTable *memo_table;
	ETable *table;

	struct {
		ECalModel *model;
		GSList *list;
	} foreach_data;

	if (info != TARGET_VCALENDAR)
		return;

	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	table = e_memo_table_get_table (memo_table);

	foreach_data.model = e_memo_table_get_model (memo_table);
	foreach_data.list = NULL;

	e_table_selected_row_foreach (
		table, memo_shell_content_table_foreach_cb,
		&foreach_data);

	if (foreach_data.list != NULL) {
		cal_comp_selection_set_string_list (
			selection_data, foreach_data.list);
		g_slist_foreach (foreach_data.list, (GFunc) g_free, NULL);
		g_slist_free (foreach_data.list);
	}
}

static void
memo_shell_content_table_drag_data_delete_cb (EMemoShellContent *memo_shell_content,
                                              gint row,
                                              gint col,
                                              GdkDragContext *context)
{
	/* Moved components are deleted from source immediately when moved,
	 * because some of them can be part of destination source, and we
	 * don't want to delete not-moved memos.  There is no such information
	 * which event has been moved and which not, so skip this method. */
}

static void
memo_shell_content_cursor_change_cb (EMemoShellContent *memo_shell_content,
                                     gint row,
                                     ETable *table)
{
	ECalComponentPreview *memo_preview;
	EMemoTable *memo_table;
	ECalModel *memo_model;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	const gchar *uid;

	memo_model = e_memo_shell_content_get_memo_model (memo_shell_content);
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	memo_preview = e_memo_shell_content_get_memo_preview (memo_shell_content);

	if (e_table_selected_count (table) != 1) {
		e_cal_component_preview_clear (memo_preview);
		return;
	}

	row = e_table_get_cursor_row (table);
	comp_data = e_cal_model_get_component_at (memo_model, row);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		comp, icalcomponent_new_clone (comp_data->icalcomp));
	e_cal_component_preview_display (
		memo_preview, comp_data->client, comp);

	e_cal_component_get_uid (comp, &uid);
	g_free (memo_shell_content->priv->current_uid);
	memo_shell_content->priv->current_uid = g_strdup (uid);

	g_object_unref (comp);
}

static void
memo_shell_content_selection_change_cb (EMemoShellContent *memo_shell_content,
                                        ETable *table)
{
	ECalComponentPreview *memo_preview;

	memo_preview = e_memo_shell_content_get_memo_preview (memo_shell_content);

	/* XXX Old code emits a "selection-changed" signal here. */

	if (e_table_selected_count (table) != 1)
		e_cal_component_preview_clear (memo_preview);
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

	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);
	table = e_memo_table_get_table (memo_table);

	memo_shell_content_cursor_change_cb (memo_shell_content, 0, table);
}

static GtkOrientation
memo_shell_content_get_orientation (EMemoShellContent *memo_shell_content)
{
	return memo_shell_content->priv->orientation;
}

static void
memo_shell_content_set_orientation (EMemoShellContent *memo_shell_content,
                                    GtkOrientation orientation)
{
	memo_shell_content->priv->orientation = orientation;

	g_object_notify (G_OBJECT (memo_shell_content), "orientation");
}

static void
memo_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ORIENTATION:
			memo_shell_content_set_orientation (
				E_MEMO_SHELL_CONTENT (object),
				g_value_get_enum (value));
			return;

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
		case PROP_MODEL:
			g_value_set_object (
				value,
				e_memo_shell_content_get_memo_model (
				E_MEMO_SHELL_CONTENT (object)));
			return;

		case PROP_ORIENTATION:
			g_value_set_enum (
				value,
				memo_shell_content_get_orientation (
				E_MEMO_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				e_memo_shell_content_get_preview_visible (
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

	if (priv->memo_model != NULL) {
		g_object_unref (priv->memo_model);
		priv->memo_model = NULL;
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
	EShell *shell;
	EShellView *shell_view;
	EShellSettings *shell_settings;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellTaskbar *shell_taskbar;
	GalViewInstance *view_instance;
	icaltimezone *timezone;
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
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	priv->memo_model = e_cal_model_memos_new (shell_settings);

	timezone = e_shell_settings_get_pointer (
		shell_settings, "cal-timezone");

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_new (object, "orientation", widget, "orientation");

	container = widget;

	widget = e_memo_table_new (shell_view, priv->memo_model);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->memo_table = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	gtk_widget_show (widget);

	e_binding_new (object, "preview-visible", widget, "visible");

	container = widget;

	widget = e_cal_component_preview_new ();
	e_cal_component_preview_set_default_timezone (
		E_CAL_COMPONENT_PREVIEW (widget), timezone);
	e_shell_configure_web_view (shell, E_WEB_VIEW (widget));
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->memo_preview = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar);

	/* Configure the memo table. */

	widget = E_MEMO_TABLE (priv->memo_table)->etable;
	table = e_table_scrolled_get_table (E_TABLE_SCROLLED (widget));

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
		priv->memo_model, "model-row-changed",
		G_CALLBACK (memo_shell_content_model_row_changed_cb),
		object);

	/* Load the view instance. */

	view_instance = e_shell_view_new_view_instance (shell_view, NULL);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (memo_shell_content_display_view_cb),
		object);
	gal_view_instance_load (view_instance);
	priv->view_instance = view_instance;

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/calendar/display/memo_hpane_position";
	gconf_bridge_bind_property_delayed (bridge, key, object, "hposition");

	object = G_OBJECT (priv->paned);
	key = "/apps/evolution/calendar/display/memo_vpane_position";
	gconf_bridge_bind_property_delayed (bridge, key, object, "vposition");
}

static guint32
memo_shell_content_check_state (EShellContent *shell_content)
{
	EMemoShellContent *memo_shell_content;
	EMemoTable *memo_table;
	ETable *table;
	GSList *list, *iter;
	GtkClipboard *clipboard;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gint n_selected;
	guint32 state = 0;

	memo_shell_content = E_MEMO_SHELL_CONTENT (shell_content);
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	table = e_memo_table_get_table (memo_table);
	n_selected = e_table_selected_count (table);

	list = e_memo_table_get_selected (memo_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		editable &= !read_only;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);
	}
	g_slist_free (list);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	if (n_selected == 1)
		state |= E_MEMO_SHELL_CONTENT_SELECTION_SINGLE;
	if (n_selected > 1)
		state |= E_MEMO_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (editable)
		state |= E_MEMO_SHELL_CONTENT_SELECTION_CAN_EDIT;
	if (has_url)
		state |= E_MEMO_SHELL_CONTENT_SELECTION_HAS_URL;
	if (e_clipboard_wait_is_calendar_available (clipboard))
		state |= E_MEMO_SHELL_CONTENT_CLIPBOARD_HAS_CALENDAR;

	return state;
}

static void
memo_shell_content_class_init (EMemoShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = memo_shell_content_set_property;
	object_class->get_property = memo_shell_content_get_property;
	object_class->dispose = memo_shell_content_dispose;
	object_class->finalize = memo_shell_content_finalize;
	object_class->constructed = memo_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = memo_shell_content_check_state;

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			_("Model"),
			_("The memo table model"),
			E_TYPE_CAL_MODEL,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			_("Preview is Visible"),
			_("Whether the preview pane is visible"),
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_override_property (
		object_class, PROP_ORIENTATION, "orientation");
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
	return memo_shell_content_type;
}

void
e_memo_shell_content_register_type (GTypeModule *type_module)
{
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

	static const GInterfaceInfo orientable_info = {
		(GInterfaceInitFunc) NULL,
		(GInterfaceFinalizeFunc) NULL,
		NULL  /* interface_data */
	};

	memo_shell_content_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_CONTENT,
		"EMemoShellContent", &type_info, 0);

	g_type_module_add_interface (
		type_module, memo_shell_content_type,
		GTK_TYPE_ORIENTABLE, &orientable_info);
}

GtkWidget *
e_memo_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MEMO_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

ECalModel *
e_memo_shell_content_get_memo_model (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return memo_shell_content->priv->memo_model;
}

ECalComponentPreview *
e_memo_shell_content_get_memo_preview (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return E_CAL_COMPONENT_PREVIEW (
		memo_shell_content->priv->memo_preview);
}

EMemoTable *
e_memo_shell_content_get_memo_table (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return E_MEMO_TABLE (memo_shell_content->priv->memo_table);
}

gboolean
e_memo_shell_content_get_preview_visible (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), FALSE);

	return memo_shell_content->priv->preview_visible;
}

void
e_memo_shell_content_set_preview_visible (EMemoShellContent *memo_shell_content,
                                          gboolean preview_visible)
{
	g_return_if_fail (E_IS_MEMO_SHELL_CONTENT (memo_shell_content));

	memo_shell_content->priv->preview_visible = preview_visible;

	g_object_notify (G_OBJECT (memo_shell_content), "preview-visible");
}

GalViewInstance *
e_memo_shell_content_get_view_instance (EMemoShellContent *memo_shell_content)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_CONTENT (memo_shell_content), NULL);

	return memo_shell_content->priv->view_instance;
}
