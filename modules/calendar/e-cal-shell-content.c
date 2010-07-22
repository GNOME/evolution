/*
 * e-cal-shell-content.c
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

#include "e-cal-shell-content.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-binding.h"
#include "e-util/gconf-bridge.h"
#include "widgets/menus/gal-view-etable.h"
#include "widgets/misc/e-paned.h"
#include "widgets/misc/e-selectable.h"

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/calendar-view.h"
#include "calendar/gui/e-cal-list-view.h"
#include "calendar/gui/e-cal-model-calendar.h"
#include "calendar/gui/e-calendar-view.h"
#include "calendar/gui/e-day-view.h"
#include "calendar/gui/e-week-view.h"

#include "e-cal-shell-view-private.h"

#define E_CAL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_CONTENT, ECalShellContentPrivate))

struct _ECalShellContentPrivate {
	GtkWidget *hpaned;
	GtkWidget *notebook;
	GtkWidget *vpaned;

	GtkWidget *calendar;
	GtkWidget *task_table;
	GtkWidget *memo_table;

	GalViewInstance *view_instance;

	guint paned_binding_id;
};

enum {
	PROP_0,
	PROP_CALENDAR,
	PROP_MEMO_TABLE,
	PROP_TASK_TABLE
};

/* Used to indicate who has the focus within the calendar view. */
typedef enum {
	FOCUS_CALENDAR,
	FOCUS_MEMO_TABLE,
	FOCUS_TASK_TABLE,
	FOCUS_OTHER
} FocusLocation;

static gpointer parent_class;
static GType cal_shell_content_type;

static void
cal_shell_content_display_view_cb (ECalShellContent *cal_shell_content,
                                   GalView *gal_view)
{
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;

	/* XXX This is confusing: we have CalendarView and ECalendarView.
	 *     ECalendarView is an abstract base class for calendar view
	 *     widgets (day view, week view, etc).  CalendarView is a
	 *     simple GalView subclass that represents a calendar view. */

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	if (GAL_IS_VIEW_ETABLE (gal_view)) {
		ECalendarView *calendar_view;

		view_type = GNOME_CAL_LIST_VIEW;
		calendar_view = gnome_calendar_get_calendar_view (
			calendar, view_type);
		gal_view_etable_attach_table (
			GAL_VIEW_ETABLE (gal_view),
			E_CAL_LIST_VIEW (calendar_view)->table);
	} else {
		view_type = calendar_view_get_view_type (
			CALENDAR_VIEW (gal_view));
	}

	gnome_calendar_display_view (calendar, view_type);
}

static void
cal_shell_content_notify_view_id_cb (ECalShellContent *cal_shell_content)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	GConfBridge *bridge;
	GtkWidget *paned;
	guint binding_id;
	const gchar *key;
	const gchar *view_id;

	bridge = gconf_bridge_get ();
	paned = cal_shell_content->priv->hpaned;
	binding_id = cal_shell_content->priv->paned_binding_id;

	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	view_id = e_shell_view_get_view_id (shell_view);

	if (binding_id > 0)
		gconf_bridge_unbind (bridge, binding_id);

	if (view_id != NULL && strcmp (view_id, "Month_View") == 0)
		key = "/apps/evolution/calendar/display/month_hpane_position";
	else
		key = "/apps/evolution/calendar/display/hpane_position";

	binding_id = gconf_bridge_bind_property_delayed (
		bridge, key, G_OBJECT (paned), "hposition");

	cal_shell_content->priv->paned_binding_id = binding_id;
}

static gchar *
cal_shell_content_get_pad_state_filename (EShellContent *shell_content,
                                          ETable *table)
{
	EShellBackend *shell_backend;
	EShellView *shell_view;
	const gchar *config_dir, *nick = NULL;

	g_return_val_if_fail (shell_content != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE (table), NULL);

	if (E_IS_TASK_TABLE (table))
		nick = "TaskPad";
	else if (E_IS_MEMO_TABLE (table))
		nick = "MemoPad";

	g_return_val_if_fail (nick != NULL, NULL);

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	config_dir = e_shell_backend_get_config_dir (shell_backend);

	return g_build_filename (config_dir, nick, NULL);
}

static void
cal_shell_content_save_table_state (EShellContent *shell_content,
                                    ETable *table)
{
	gchar *filename;

	filename = cal_shell_content_get_pad_state_filename (
		shell_content, table);
	g_return_if_fail (filename != NULL);

	e_table_save_state (table, filename);
	g_free (filename);
}

static void
cal_shell_content_load_table_state (EShellContent *shell_content,
                                    ETable *table)
{
	gchar *filename;

	filename = cal_shell_content_get_pad_state_filename (
		shell_content, table);
	g_return_if_fail (filename != NULL);

	e_table_load_state (table, filename);
	g_free (filename);
}

void
e_cal_shell_content_save_state (ECalShellContent *cal_shell_content)
{
	ECalShellContentPrivate *priv;

	g_return_if_fail (cal_shell_content != NULL);
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (cal_shell_content);

	if (priv->task_table != NULL)
		cal_shell_content_save_table_state (
			E_SHELL_CONTENT (cal_shell_content),
			E_TABLE (priv->task_table));

	if (priv->memo_table != NULL)
		cal_shell_content_save_table_state (
			E_SHELL_CONTENT (cal_shell_content),
			E_TABLE (priv->memo_table));
}

static void
cal_shell_content_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_content_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CALENDAR:
			g_value_set_object (
				value, e_cal_shell_content_get_calendar (
				E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_MEMO_TABLE:
			g_value_set_object (
				value, e_cal_shell_content_get_memo_table (
				E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_TASK_TABLE:
			g_value_set_object (
				value, e_cal_shell_content_get_task_table (
				E_CAL_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_content_dispose (GObject *object)
{
	ECalShellContentPrivate *priv;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->hpaned != NULL) {
		g_object_unref (priv->hpaned);
		priv->hpaned = NULL;
	}

	if (priv->notebook != NULL) {
		g_object_unref (priv->notebook);
		priv->notebook = NULL;
	}

	if (priv->vpaned != NULL) {
		g_object_unref (priv->vpaned);
		priv->vpaned = NULL;
	}

	if (priv->calendar != NULL) {
		gnome_calendar_dispose (GNOME_CALENDAR (priv->calendar));
		g_object_unref (priv->calendar);
		priv->calendar = NULL;
	}

	if (priv->task_table != NULL) {
		g_object_unref (priv->task_table);
		priv->task_table = NULL;
	}

	if (priv->memo_table != NULL) {
		g_object_unref (priv->memo_table);
		priv->memo_table = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static time_t
gc_get_default_time (ECalModel *model, gpointer user_data)
{
	GnomeCalendar *gcal = user_data;
	time_t res = 0, end;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (GNOME_IS_CALENDAR (user_data), 0);

	gnome_calendar_get_current_time_range (gcal, &res, &end);

	return res;
}

static void
cal_shell_content_constructed (GObject *object)
{
	ECalShellContentPrivate *priv;
	ECalendarView *calendar_view;
	ECalModel *memo_model=NULL;
	ECalModel *task_model=NULL;
	EShell *shell;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *foreign_content;
	EShellView *foreign_view;
	GnomeCalendar *calendar;
	GalViewInstance *view_instance;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *key;
	gchar *markup;
	gint ii;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);

	/* We borrow the memopad and taskpad models from the memo
	 * and task views, loading the views if necessary. */
	if (!e_shell_get_express_mode (shell)) {
		foreign_view = e_shell_window_get_shell_view (shell_window, "memos");
		foreign_content = e_shell_view_get_shell_content (foreign_view);
		g_object_get (foreign_content, "model", &memo_model, NULL);

		foreign_view = e_shell_window_get_shell_view (shell_window, "tasks");
		foreign_content = e_shell_view_get_shell_content (foreign_view);
		g_object_get (foreign_content, "model", &task_model, NULL);
	}

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	if (!e_shell_get_express_mode (shell)) {
		widget = e_paned_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_container_add (GTK_CONTAINER (container), widget);
		priv->hpaned = g_object_ref (widget);
		gtk_widget_show (widget);

		container = priv->hpaned;
	}

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	if (!e_shell_get_express_mode (shell))
		gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	else
		gtk_container_add (GTK_CONTAINER (container), widget);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	if (!e_shell_get_express_mode (shell)) {
		/* FIXME Need to deal with saving and restoring the position.
		 *       Month view has its own position. */
		widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
		e_paned_set_fixed_resize (E_PANED (widget), FALSE);
		gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, TRUE);
		priv->vpaned = g_object_ref (widget);
		gtk_widget_show (widget);
	}

	container = priv->notebook;

	/* Add views in the order defined by GnomeCalendarViewType, such
	 * that the notebook page number corresponds to the view type. */

	priv->calendar = gnome_calendar_new ();
	calendar = GNOME_CALENDAR (priv->calendar);

	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
		calendar_view = gnome_calendar_get_calendar_view (calendar, ii);

		gtk_notebook_append_page (
			GTK_NOTEBOOK (container),
			GTK_WIDGET (calendar_view), NULL);
		gtk_widget_show (GTK_WIDGET (calendar_view));
	}

	e_binding_new (
		priv->calendar, "view",
		priv->notebook, "page");

	container = priv->vpaned;

	if (!e_shell_get_express_mode (shell)) {
		widget = gtk_vbox_new (FALSE, 0);
		gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, TRUE);
		gtk_widget_show (widget);

		container = widget;

		widget = gtk_hseparator_new ();
		gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
		gtk_widget_show (widget);

		widget = gtk_label_new (NULL);
		markup = g_strdup_printf ("<b>%s</b>", _("Tasks"));
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
		gtk_widget_show (widget);
		g_free (markup);

		widget = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW (widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (
			GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
		gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
		gtk_widget_show (widget);

		container = widget;

		widget = e_task_table_new (shell_view, task_model);
		gtk_container_add (GTK_CONTAINER (container), widget);
		priv->task_table = g_object_ref (widget);
		gtk_widget_show (widget);

		cal_shell_content_load_table_state (
			shell_content, E_TABLE (widget));

		g_signal_connect_swapped (
			widget, "open-component",
			G_CALLBACK (e_cal_shell_view_taskpad_open_task),
			shell_view);

		container = priv->vpaned;

		widget = gtk_vbox_new (FALSE, 0);
		gtk_paned_pack2 (GTK_PANED (container), widget, TRUE, TRUE);
		gtk_widget_show (widget);

		container = widget;

		widget = gtk_label_new (NULL);
		markup = g_strdup_printf ("<b>%s</b>", _("Memos"));
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
		gtk_widget_show (widget);
		g_free (markup);

		widget = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW (widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (
			GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
		gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
		gtk_widget_show (widget);

		container = widget;

		widget = e_memo_table_new (shell_view, memo_model);
		gtk_container_add (GTK_CONTAINER (container), widget);
		priv->memo_table = g_object_ref (widget);
		gtk_widget_show (widget);

		cal_shell_content_load_table_state (
			shell_content, E_TABLE (widget));

		e_cal_model_set_default_time_func (
			memo_model, gc_get_default_time, calendar);

		g_signal_connect_swapped (
			widget, "open-component",
			G_CALLBACK (e_cal_shell_view_memopad_open_memo),
			shell_view);
	}

	/* Load the view instance. */

	view_instance = e_shell_view_new_view_instance (shell_view, NULL);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (cal_shell_content_display_view_cb),
		object);
	/* XXX Actually, don't load the view instance just yet.
	 *     The GtkWidget::map() callback below explains why. */
	priv->view_instance = view_instance;

	if (!e_shell_get_express_mode (shell)) {
		g_signal_connect_swapped (
			shell_view, "notify::view-id",
			G_CALLBACK (cal_shell_content_notify_view_id_cb),
			object);

		bridge = gconf_bridge_get ();

		object = G_OBJECT (priv->vpaned);
		key = "/apps/evolution/calendar/display/tag_vpane_position";
		gconf_bridge_bind_property_delayed (bridge, key, object, "proportion");
	}

	if (memo_model)
		g_object_unref (memo_model);
	if (task_model)
		g_object_unref (task_model);
}

static void
cal_shell_content_map (GtkWidget *widget)
{
	ECalShellContentPrivate *priv;

	/* XXX Delay loading the GalViewInstance until after ECalShellView
	 *     has a chance to install the sidebar's date navigator into
	 *     GnomeCalendar, since loading the GalViewInstance triggers a
	 *     callback in GnomeCalendar that requires the date navigator.
	 *     Ordinarily we would do this at the end of constructed(), but
	 *     that's too soon in this case.  (This feels kind of kludgy.) */
	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (widget);
	gal_view_instance_load (priv->view_instance);

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
cal_shell_content_focus_search_results (EShellContent *shell_content)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	gtk_widget_grab_focus (GTK_WIDGET (calendar_view));
}

static void
cal_shell_content_class_init (ECalShellContentClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	EShellContentClass *shell_content_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_shell_content_set_property;
	object_class->get_property = cal_shell_content_get_property;
	object_class->dispose = cal_shell_content_dispose;
	object_class->constructed = cal_shell_content_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = cal_shell_content_map;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->focus_search_results = cal_shell_content_focus_search_results;

	g_object_class_install_property (
		object_class,
		PROP_CALENDAR,
		g_param_spec_object (
			"calendar",
			NULL,
			NULL,
			GNOME_TYPE_CALENDAR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MEMO_TABLE,
		g_param_spec_object (
			"memo-table",
			NULL,
			NULL,
			E_TYPE_MEMO_TABLE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_TASK_TABLE,
		g_param_spec_object (
			"task-table",
			NULL,
			NULL,
			E_TYPE_TASK_TABLE,
			G_PARAM_READABLE));
}

static void
cal_shell_content_init (ECalShellContent *cal_shell_content)
{
	cal_shell_content->priv =
		E_CAL_SHELL_CONTENT_GET_PRIVATE (cal_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_cal_shell_content_get_type (void)
{
	return cal_shell_content_type;
}

void
e_cal_shell_content_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (ECalShellContentClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_shell_content_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (ECalShellContent),
		0,     /* n_preallocs */
		(GInstanceInitFunc) cal_shell_content_init,
		NULL   /* value_table */
	};

	cal_shell_content_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_CONTENT,
		"ECalShellContent", &type_info, 0);
}

GtkWidget *
e_cal_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

ECalModel *
e_cal_shell_content_get_model (ECalShellContent *cal_shell_content)
{
	GnomeCalendar *calendar;

	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	return gnome_calendar_get_model (calendar);
}

GnomeCalendar *
e_cal_shell_content_get_calendar (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return GNOME_CALENDAR (cal_shell_content->priv->calendar);
}

EMemoTable *
e_cal_shell_content_get_memo_table (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return E_MEMO_TABLE (cal_shell_content->priv->memo_table);
}

ETaskTable *
e_cal_shell_content_get_task_table (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return E_TASK_TABLE (cal_shell_content->priv->task_table);
}

EShellSearchbar *
e_cal_shell_content_get_searchbar (ECalShellContent *cal_shell_content)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *widget;

	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	widget = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (widget);
}

GalViewInstance *
e_cal_shell_content_get_view_instance (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return cal_shell_content->priv->view_instance;
}
