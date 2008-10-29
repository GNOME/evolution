/*
 * e-cal-shell-content.h
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

#include <glib/gi18n.h>

#include "e-util/gconf-bridge.h"

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/e-cal-list-view-config.h"
#include "calendar/gui/e-cal-model-calendar.h"
#include "calendar/gui/e-calendar-table.h"
#include "calendar/gui/e-calendar-table-config.h"
#include "calendar/gui/e-day-view-config.h"
#include "calendar/gui/e-memo-table-config.h"
#include "calendar/gui/e-week-view-config.h"

#include "widgets/menus/gal-view-etable.h"

#define E_CAL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_CONTENT, ECalShellContentPrivate))

struct _ECalShellContentPrivate {
	GtkWidget *hpaned;
	GtkWidget *notebook;
	GtkWidget *vpaned;

	GtkWidget *day_view;
	GtkWidget *work_week_view;
	GtkWidget *week_view;
	GtkWidget *month_view;
	GtkWidget *list_view;
	GtkWidget *task_table;
	GtkWidget *memo_table;

	EDayViewConfig *day_view_config;
	EDayViewConfig *work_week_view_config;
	EWeekViewConfig *week_view_config;
	EWeekViewConfig *month_view_config;
	ECalListViewConfig *list_view_config;
	ECalendarTableConfig *task_table_config;
	EMemoTableConfig *memo_table_config;

	GalViewInstance *view_instance;

	guint paned_binding_id;
};

enum {
	PROP_0
};

/* Used to indicate who has the focus within the calendar view. */
typedef enum {
	FOCUS_CALENDAR,
	FOCUS_MEMO_TABLE,
	FOCUS_TASK_TABLE,
	FOCUS_OTHER
} FocusLocation;

static gpointer parent_class;

static void
cal_shell_content_changed_cb (ECalShellContent *cal_shell_content,
                              GalViewInstance *view_instance)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	gchar *view_id;

	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	view_id = gal_view_instance_get_current_view_id (view_instance);
	e_shell_view_set_view_id (shell_view, view_id);
	g_free (view_id);
}

static void
cal_shell_content_display_view_cb (ECalShellContent *cal_shell_content,
                                   GalView *gal_view)
{
}

static FocusLocation
cal_shell_content_get_focus_location (ECalShellContent *cal_shell_content)
{
        return FOCUS_OTHER;
#if 0  /* TEMPORARILY DISABLED */
	GtkWidget *widget;
	GnomeCalendar *calendar;
	ECalendarTable *task_table;
	EMemoTable *memo_table;
	ETable *table;
	ECalendarView *calendar_view;

	calendar = GNOME_CALENDAR (cal_shell_content->priv->calendar);
	widget = gnome_calendar_get_current_view_widget (calendar);

	memo_table = E_MEMO_TABLE (cal_shell_content->priv->memo_table);
	task_table = E_CALENDAR_TABLE (cal_shell_content->priv->task_table);

	table = e_memo_table_get_table (memo_table);
	if (GTK_WIDGET_HAS_FOCUS (table->table_canvas))
		return FOCUS_MEMO_TABLE;

	table = e_calendar_table_get_table (task_table);
	if (GTK_WIDGET_HAS_FOCUS (table->table_canvas))
		return FOCUS_TASK_TABLE;

	if (E_IS_DAY_VIEW (widget)) {
		EDayView *view = E_DAY_VIEW (widget);

		if (GTK_WIDGET_HAS_FOCUS (view->top_canvas))
			return FOCUS_CALENDAR;

		if (GNOME_CANVAS (view->top_canvas)->focused_item != NULL)
			return FOCUS_CALENDAR;

		if (GTK_WIDGET_HAS_FOCUS (view->main_canvas))
			return FOCUS_CALENDAR;

		if (GNOME_CANVAS (view->main_canvas)->focused_item != NULL)
			return FOCUS_CALENDAR;

	} else if (E_IS_WEEK_VIEW (widget)) {
		EWeekView *view = E_WEEK_VIEW (widget);

		if (GTK_WIDGET_HAS_FOCUS (view->main_canvas))
			return FOCUS_CALENDAR;

		if (GNOME_CANVAS (view->main_canvas)->focused_item != NULL)
			return FOCUS_CALENDAR;

	} else if (E_IS_CAL_LIST_VIEW (widget)) {
		ECalListView *view = E_CAL_LIST_VIEW (widget);

		table = e_table_scrolled_get_table (view->table_scrolled);
		if (GTK_WIDGET_HAS_FOCUS (table))
			return FOCUS_CALENDAR;
	}

	return FOCUS_OTHER;
#endif
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

	if (priv->day_view != NULL) {
		g_object_unref (priv->day_view);
		priv->day_view = NULL;
	}

	if (priv->work_week_view != NULL) {
		g_object_unref (priv->work_week_view);
		priv->work_week_view = NULL;
	}

	if (priv->week_view != NULL) {
		g_object_unref (priv->week_view);
		priv->week_view = NULL;
	}

	if (priv->month_view != NULL) {
		g_object_unref (priv->month_view);
		priv->month_view = NULL;
	}

	if (priv->list_view != NULL) {
		g_object_unref (priv->list_view);
		priv->list_view = NULL;
	}

	if (priv->task_table != NULL) {
		g_object_unref (priv->task_table);
		priv->task_table = NULL;
	}

	if (priv->memo_table != NULL) {
		g_object_unref (priv->memo_table);
		priv->memo_table = NULL;
	}

	if (priv->day_view_config != NULL) {
		g_object_unref (priv->day_view_config);
		priv->day_view_config = NULL;
	}

	if (priv->work_week_view_config != NULL) {
		g_object_unref (priv->work_week_view_config);
		priv->work_week_view_config = NULL;
	}

	if (priv->week_view_config != NULL) {
		g_object_unref (priv->week_view_config);
		priv->week_view_config = NULL;
	}

	if (priv->month_view_config != NULL) {
		g_object_unref (priv->month_view_config);
		priv->month_view_config = NULL;
	}

	if (priv->list_view_config != NULL) {
		g_object_unref (priv->list_view_config);
		priv->list_view_config = NULL;
	}

	if (priv->task_table_config != NULL) {
		g_object_unref (priv->task_table_config);
		priv->task_table_config = NULL;
	}

	if (priv->memo_table_config != NULL) {
		g_object_unref (priv->memo_table_config);
		priv->memo_table_config = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_content_finalize (GObject *object)
{
	ECalShellContentPrivate *priv;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cal_shell_content_constructed (GObject *object)
{
	ECalShellContentPrivate *priv;
	ECalModelCalendar *cal_model;
	ECalModel *memo_model;
	ECalModel *task_model;
	EShellContent *shell_content;
	EShellModule *shell_module;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellViewClass *shell_view_class;
	EShellContent *foreign_content;
	EShellView *foreign_view;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *config_dir;
	gchar *filename;
	gchar *markup;
	gint page_num;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;

	shell_module = e_shell_view_get_shell_module (shell_view);
	config_dir = e_shell_module_get_config_dir (shell_module);

	/* Calendar model for the views. */
	cal_model = e_cal_model_calendar_new ();
	e_cal_model_set_flags (
		E_CAL_MODEL (cal_model),
		E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES);

	/* We borrow the memopad and taskpad models from the memo
	 * and task views, loading the views if necessary. */

	foreign_view = e_shell_window_get_shell_view (shell_window, "memos");
	foreign_content = e_shell_view_get_shell_content (foreign_view);
	g_object_get (foreign_content, "model", &memo_model, NULL);

	foreign_view = e_shell_window_get_shell_view (shell_window, "tasks");
	foreign_content = e_shell_view_get_shell_content (foreign_view);
	g_object_get (foreign_content, "model", &task_model, NULL);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	/* FIXME Need to deal with saving and restoring the position.
	 *       Month view has its own position. */
	widget = gtk_hpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->hpaned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->hpaned;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack1 (GTK_PANED (container), widget, FALSE, TRUE);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	/* FIXME Need to deal with saving and restoring the position.
	 *       Month view has its own position. */
	widget = gtk_vpaned_new ();
	gtk_paned_pack2 (GTK_PANED (container), widget, TRUE, TRUE);
	priv->vpaned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->notebook;

	/* Add views in the order defined by GnomeCalendarViewType, such
	 * that the notebook page number corresponds to the view type.
	 * The assertions below ensure that stays true. */

#if 0  /* Not so fast... get the memo/task pads working first. */
	/* FIXME Need to establish a calendar and timezone first. */
	widget = e_day_view_new (E_CAL_MODEL (cal_model));
	e_calendar_view_set_calendar (
		E_CALENDAR_VIEW (widget), GNOME_CALENDAR (priv->calendar));
	e_calendar_view_set_timezone (
		E_CALENDAR_VIEW (widget), priv->timezone);
	page_num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (widget));
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	g_return_if_fail (page_num == GNOME_CAL_DAY_VIEW);
	priv->day_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* FIXME Need to establish a calendar and timezone first. */
	widget = e_day_view_new (E_CAL_MODEL (cal_model));
	e_day_view_set_work_week_view (E_DAY_VIEW (widget), TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (widget), 5);
	e_calendar_view_set_calendar (
		E_CALENDAR_VIEW (widget), GNOME_CALENDAR (priv->calendar));
	e_calendar_view_set_timezone (
		E_CALENDAR_VIEW (widget), priv->timezone);
	page_num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (widget));
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	g_return_if_fail (page_num == GNOME_CAL_WORK_WEEK_VIEW);
	priv->work_week_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* FIXME Need to establish a calendar and timezone first. */
	widget = e_week_view_new (E_CAL_MODEL (cal_model));
	e_calendar_view_set_calendar (
		E_CALENDAR_VIEW (widget), GNOME_CALENDAR (priv->calendar));
	e_calendar_view_set_timezone (
		E_CALENDAR_VIEW (widget), priv->timezone);
	page_num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (widget));
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	g_return_if_fail (page_num == GNOME_CAL_WEEK_VIEW);
	priv->week_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* FIXME Need to establish a calendar and timezone first. */
	widget = e_week_view_new (E_CAL_MODEL (cal_model));
	e_week_view_set_multi_week_view (E_WEEK_VIEW (widget), TRUE);
	e_week_view_set_weeks_shown (E_WEEK_VIEW (widget), 6);
	e_calendar_view_set_calendar (
		E_CALENDAR_VIEW (widget), GNOME_CALENDAR (priv->calendar));
	e_calendar_view_set_timezone (
		E_CALENDAR_VIEW (widget), priv->timezone);
	page_num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (widget));
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	g_return_if_fail (page_num == GNOME_CAL_MONTH_VIEW);
	priv->month_view = g_object_ref (widget);
	gtk_widget_show (widget);

	/* FIXME Need to establish a calendar and timezone first. */
	widget = e_cal_list_view_new (E_CAL_MODEL (cal_model));
	e_calendar_view_set_calendar (
		E_CALENDAR_VIEW (widget), GNOME_CALENDAR (priv->calendar));
	e_calendar_view_set_timezone (
		E_CALENDAR_VIEW (widget), priv->timezone);
	page_num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (widget));
	gtk_notebook_append_page (GTK_NOTEBOOK (container), widget, NULL);
	g_return_if_fail (page_num == GNOME_CAL_LIST_VIEW);
	priv->list_view = g_object_ref (widget);
	gtk_widget_show (widget);
#endif

	container = priv->vpaned;

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

	widget = e_calendar_table_new (shell_view, task_model);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->task_table = g_object_ref (widget);
	gtk_widget_show (widget);

	filename = g_build_filename (config_dir, "TaskPad", NULL);
	e_calendar_table_load_state (E_CALENDAR_TABLE (widget), filename);
	g_free (filename);

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

	widget = e_memo_table_new (shell_view, memo_model);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->memo_table = g_object_ref (widget);
	gtk_widget_show (widget);

	filename = g_build_filename (config_dir, "MemoPad", NULL);
	e_memo_table_load_state (E_MEMO_TABLE (widget), filename);
	g_free (filename);

	/* Configuration managers for views and tables. */
	priv->day_view_config = e_day_view_config_new (
		E_DAY_VIEW (priv->day_view));
	priv->work_week_view_config = e_day_view_config_new (
		E_DAY_VIEW (priv->work_week_view));
	priv->week_view_config = e_week_view_config_new (
		E_WEEK_VIEW (priv->week_view));
	priv->month_view_config = e_week_view_config_new (
		E_WEEK_VIEW (priv->month_view));
	priv->list_view_config = e_cal_list_view_config_new (
		E_CAL_LIST_VIEW (priv->list_view));
	priv->task_table_config = e_calendar_table_config_new (
		E_CALENDAR_TABLE (priv->task_table));
	priv->memo_table_config = e_memo_table_config_new (
		E_MEMO_TABLE (priv->memo_table));

	/* Load the view instance. */

	view_instance = gal_view_instance_new (view_collection, NULL);
	g_signal_connect_swapped (
		view_instance, "changed",
		G_CALLBACK (cal_shell_content_changed_cb),
		object);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (cal_shell_content_display_view_cb),
		object);
	gal_view_instance_load (view_instance);
	priv->view_instance = view_instance;

	g_object_unref (memo_model);
	g_object_unref (task_model);
}

static void
cal_shell_content_class_init (ECalShellContentClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_shell_content_set_property;
	object_class->get_property = cal_shell_content_get_property;
	object_class->dispose = cal_shell_content_dispose;
	object_class->finalize = cal_shell_content_finalize;
	object_class->constructed = cal_shell_content_constructed;
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
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
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

		type = g_type_register_static (
			E_TYPE_SHELL_CONTENT, "ECalShellContent",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_cal_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

GnomeCalendar *
e_cal_shell_content_get_calendar (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

        /* FIXME */
	/*return GNOME_CALENDAR (cal_shell_content->priv->calendar);*/
        return NULL;
}

EMemoTable *
e_cal_shell_content_get_memo_table (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return E_MEMO_TABLE (cal_shell_content->priv->memo_table);
}

ECalendarTable *
e_cal_shell_content_get_task_table (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return E_CALENDAR_TABLE (cal_shell_content->priv->task_table);
}

icaltimezone *
e_cal_shell_content_get_timezone (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

        /* FIXME */
	/*return cal_shell_content->priv->timezone;*/
        return NULL;
}

GalViewInstance *
e_cal_shell_content_get_view_instance (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return cal_shell_content->priv->view_instance;
}

void
e_cal_shell_content_copy_clipboard (ECalShellContent *cal_shell_content)
{
#if 0
	GnomeCalendar *calendar;
	EMemoTable *memo_table;
	ECalendarTable *task_table;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	switch (cal_shell_content_get_focus_location (cal_shell_content)) {
		case FOCUS_CALENDAR:
			gnome_calendar_copy_clipboard (calendar);
			break;

		case FOCUS_MEMO_TABLE:
			e_memo_table_copy_clipboard (memo_table);
			break;

		case FOCUS_TASK_TABLE:
			e_calendar_table_copy_clipboard (task_table);
			break;

		default:
			g_return_if_reached ();
	}
#endif
}

void
e_cal_shell_content_cut_clipboard (ECalShellContent *cal_shell_content)
{
#if 0
	GnomeCalendar *calendar;
	EMemoTable *memo_table;
	ECalendarTable *task_table;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	switch (cal_shell_content_get_focus_location (cal_shell_content)) {
		case FOCUS_CALENDAR:
			gnome_calendar_cut_clipboard (calendar);
			break;

		case FOCUS_MEMO_TABLE:
			e_memo_table_copy_clipboard (memo_table);
			break;

		case FOCUS_TASK_TABLE:
			e_calendar_table_copy_clipboard (task_table);
			break;

		default:
			g_return_if_reached ();
	}
#endif
}

void
e_cal_shell_content_paste_clipboard (ECalShellContent *cal_shell_content)
{
#if 0
	GnomeCalendar *calendar;
	EMemoTable *memo_table;
	ECalendarTable *task_table;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	switch (cal_shell_content_get_focus_location (cal_shell_content)) {
		case FOCUS_CALENDAR:
			gnome_calendar_paste_clipboard (calendar);
			break;

		case FOCUS_MEMO_TABLE:
			e_memo_table_copy_clipboard (memo_table);
			break;

		case FOCUS_TASK_TABLE:
			e_calendar_table_copy_clipboard (task_table);
			break;

		default:
			g_return_if_reached ();
	}
#endif
}

void
e_cal_shell_content_delete_selection (ECalShellContent *cal_shell_content)
{
#if 0
	GnomeCalendar *calendar;
	EMemoTable *memo_table;
	ECalendarTable *task_table;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	switch (cal_shell_content_get_focus_location (cal_shell_content)) {
		case FOCUS_CALENDAR:
			gnome_calendar_delete_selection (calendar);
			break;

		case FOCUS_MEMO_TABLE:
			e_memo_table_delete_selected (memo_table);
			break;

		case FOCUS_TASK_TABLE:
			e_calendar_table_delete_selected (task_table);
			break;

		default:
			g_return_if_reached ();
	}
#endif
}

void
e_cal_shell_content_delete_selected_occurrence (ECalShellContent *cal_shell_content)
{
#if 0
	GnomeCalendar *calendar;
	FocusLocation focus;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	focus = cal_shell_content_get_focus_location (cal_shell_content);
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	if (focus == FOCUS_CALENDAR)
		gnome_calendar_delete_selected_occurrence (calendar);
#endif
}
