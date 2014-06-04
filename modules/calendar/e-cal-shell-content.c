/*
 * e-cal-shell-content.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cal-shell-content.h"

#include <string.h>
#include <glib/gi18n.h>

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

G_DEFINE_DYNAMIC_TYPE (
	ECalShellContent,
	e_cal_shell_content,
	E_TYPE_SHELL_CONTENT)

static void
cal_shell_content_display_view_cb (ECalShellContent *cal_shell_content,
                                   GalView *gal_view)
{
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;
	GType gal_view_type;

	gal_view_type = G_OBJECT_TYPE (gal_view);
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	if (gal_view_type == GAL_TYPE_VIEW_ETABLE) {
		ECalendarView *calendar_view;

		view_type = GNOME_CAL_LIST_VIEW;
		calendar_view = gnome_calendar_get_calendar_view (
			calendar, view_type);
		gal_view_etable_attach_table (
			GAL_VIEW_ETABLE (gal_view),
			E_CAL_LIST_VIEW (calendar_view)->table);

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_DAY) {
		view_type = GNOME_CAL_DAY_VIEW;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_WORK_WEEK) {
		view_type = GNOME_CAL_WORK_WEEK_VIEW;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_WEEK) {
		view_type = GNOME_CAL_WEEK_VIEW;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_MONTH) {
		view_type = GNOME_CAL_MONTH_VIEW;

	} else {
		g_return_if_reached ();
	}

	gnome_calendar_display_view (calendar, view_type);
}

static void
cal_shell_content_notify_view_id_cb (ECalShellContent *cal_shell_content)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	GSettings *settings;
	GtkWidget *paned;
	const gchar *key;
	const gchar *view_id;

	settings = g_settings_new ("org.gnome.evolution.calendar");
	paned = cal_shell_content->priv->hpaned;

	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	view_id = e_shell_view_get_view_id (shell_view);

	if (view_id != NULL && strcmp (view_id, "Month_View") == 0)
		key = "month-hpane-position";
	else
		key = "hpane-position";

	g_settings_unbind (paned, "hposition");

	g_settings_bind (
		settings, key,
		paned, "hposition",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);
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

	priv = cal_shell_content->priv;

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

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_shell_content_parent_class)->dispose (object);
}

static time_t
gc_get_default_time (ECalModel *model,
                     gpointer user_data)
{
	GnomeCalendar *gcal = user_data;
	time_t res = 0, end;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (GNOME_IS_CALENDAR (user_data), 0);

	gnome_calendar_get_current_time_range (gcal, &res, &end);

	return res;
}

static void
cal_shell_content_is_editing_changed_cb (gpointer cal_view_tasks_memos_table,
                                         GParamSpec *param,
                                         EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	e_shell_view_update_actions (shell_view);
}

static void
cal_shell_content_constructed (GObject *object)
{
	ECalShellContentPrivate *priv;
	ECalendarView *calendar_view;
	ECalModel *memo_model = NULL;
	ECalModel *task_model = NULL;
	EShell *shell;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *foreign_content;
	EShellView *foreign_view;
	GnomeCalendar *calendar;
	ESourceRegistry *registry;
	GalViewInstance *view_instance;
	GSettings *settings;
	GtkWidget *container;
	GtkWidget *widget;
	gchar *markup;
	gint ii;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_shell_content_parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);

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

	widget = e_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->hpaned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->hpaned;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	/* FIXME Need to deal with saving and restoring the position.
	 *       Month view has its own position. */
	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	e_paned_set_fixed_resize (E_PANED (widget), FALSE);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, TRUE);
	priv->vpaned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->notebook;

	/* Add views in the order defined by GnomeCalendarViewType, such
	 * that the notebook page number corresponds to the view type. */

	registry = e_shell_get_registry (shell);
	priv->calendar = gnome_calendar_new (registry);
	calendar = GNOME_CALENDAR (priv->calendar);

	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
		calendar_view = gnome_calendar_get_calendar_view (calendar, ii);

		e_signal_connect_notify (
			calendar_view, "notify::is-editing",
			G_CALLBACK (cal_shell_content_is_editing_changed_cb), shell_view);

		gtk_notebook_append_page (
			GTK_NOTEBOOK (container),
			GTK_WIDGET (calendar_view), NULL);
		gtk_widget_show (GTK_WIDGET (calendar_view));
	}

	g_object_bind_property (
		priv->calendar, "view",
		priv->notebook, "page",
		G_BINDING_SYNC_CREATE);

	container = priv->vpaned;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, TRUE);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
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

	e_signal_connect_notify (
		widget, "notify::is-editing",
		G_CALLBACK (cal_shell_content_is_editing_changed_cb), shell_view);

	container = priv->vpaned;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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

	e_signal_connect_notify (
		widget, "notify::is-editing",
		G_CALLBACK (cal_shell_content_is_editing_changed_cb), shell_view);

	/* Load the view instance. */

	view_instance = e_shell_view_new_view_instance (shell_view, NULL);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (cal_shell_content_display_view_cb),
		object);
	/* XXX Actually, don't load the view instance just yet.
	 *     The GtkWidget::map() callback below explains why. */
	e_shell_view_set_view_instance (shell_view, view_instance);
	g_object_unref (view_instance);

	e_signal_connect_notify_swapped (
		shell_view, "notify::view-id",
		G_CALLBACK (cal_shell_content_notify_view_id_cb),
		object);

	settings = g_settings_new ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "tag-vpane-position",
		priv->vpaned, "proportion",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	if (memo_model)
		g_object_unref (memo_model);
	if (task_model)
		g_object_unref (task_model);
}

static void
cal_shell_content_map (GtkWidget *widget)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GalViewInstance *view_instance;

	shell_content = E_SHELL_CONTENT (widget);
	shell_view = e_shell_content_get_shell_view (shell_content);
	view_instance = e_shell_view_get_view_instance (shell_view);

	/* XXX Delay loading the GalViewInstance until after ECalShellView
	 *     has a chance to install the sidebar's date navigator into
	 *     GnomeCalendar, since loading the GalViewInstance triggers a
	 *     callback in GnomeCalendar that requires the date navigator.
	 *     Ordinarily we would do this at the end of constructed(), but
	 *     that's too soon in this case.  (This feels kind of kludgy.) */
	gal_view_instance_load (view_instance);

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (e_cal_shell_content_parent_class)->map (widget);
}

/* Helper for cal_shell_content_check_state() */
static icalproperty *
cal_shell_content_get_attendee_prop (icalcomponent *icalcomp,
                                     const gchar *address)
{
	icalproperty *prop;

	if (address == NULL || *address == '\0')
		return NULL;

	prop = icalcomponent_get_first_property (
		icalcomp, ICAL_ATTENDEE_PROPERTY);

	while (prop != NULL) {
		const gchar *attendee;

		attendee = icalproperty_get_attendee (prop);

		if (g_str_equal (itip_strip_mailto (attendee), address))
			return prop;

		prop = icalcomponent_get_next_property (
			icalcomp, ICAL_ATTENDEE_PROPERTY);
	}

	return NULL;
}

/* Helper for cal_shell_content_check_state() */
static gboolean
cal_shell_content_icalcomp_is_delegated (icalcomponent *icalcomp,
                                         const gchar *user_email)
{
	icalproperty *prop;
	icalparameter *param;
	const gchar *delto = NULL;
	gboolean is_delegated = FALSE;

	prop = cal_shell_content_get_attendee_prop (icalcomp, user_email);

	if (prop != NULL) {
		param = icalproperty_get_first_parameter (
			prop, ICAL_DELEGATEDTO_PARAMETER);
		if (param != NULL) {
			delto = icalparameter_get_delegatedto (param);
			delto = itip_strip_mailto (delto);
		}
	} else
		return FALSE;

	prop = cal_shell_content_get_attendee_prop (icalcomp, delto);

	if (prop != NULL) {
		const gchar *delfrom = NULL;
		icalparameter_partstat status = ICAL_PARTSTAT_NONE;

		param = icalproperty_get_first_parameter (
			prop, ICAL_DELEGATEDFROM_PARAMETER);
		if (param != NULL) {
			delfrom = icalparameter_get_delegatedfrom (param);
			delfrom = itip_strip_mailto (delfrom);
		}
		param = icalproperty_get_first_parameter (
			prop, ICAL_PARTSTAT_PARAMETER);
		if (param != NULL)
			status = icalparameter_get_partstat (param);
		is_delegated =
			(status != ICAL_PARTSTAT_DECLINED) &&
			(g_strcmp0 (delfrom, user_email) == 0);
	}

	return is_delegated;
}

static guint32
cal_shell_content_check_state (EShellContent *shell_content)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	ESourceRegistry *registry;
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	ECalendarView *calendar_view;
	GnomeCalendarViewType view_type;
	gboolean selection_is_editable = FALSE;
	gboolean selection_is_instance = FALSE;
	gboolean selection_is_meeting = FALSE;
	gboolean selection_is_organizer = FALSE;
	gboolean selection_is_recurring = FALSE;
	gboolean selection_can_delegate = FALSE;
	guint32 state = 0;
	GList *selected;
	GList *link;
	guint n_selected;

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	selected = e_calendar_view_get_selected_events (calendar_view);
	n_selected = g_list_length (selected);

	/* If we have a selection, assume it's
	 * editable until we learn otherwise. */
	if (n_selected > 0)
		selection_is_editable = TRUE;

	for (link = selected; link != NULL; link = g_list_next (link)) {
		ECalendarViewEvent *event = link->data;
		ECalClient *client;
		ECalComponent *comp;
		gchar *user_email;
		icalcomponent *icalcomp;
		const gchar *capability;
		gboolean cap_delegate_supported;
		gboolean cap_delegate_to_many;
		gboolean icalcomp_is_delegated;
		gboolean read_only;

		if (!is_comp_data_valid (event))
			continue;

		client = event->comp_data->client;
		icalcomp = event->comp_data->icalcomp;

		read_only = e_client_is_readonly (E_CLIENT (client));
		selection_is_editable &= !read_only;

		selection_is_instance |=
			e_cal_util_component_is_instance (icalcomp);

		selection_is_meeting =
			(n_selected == 1) &&
			e_cal_util_component_has_attendee (icalcomp);

		selection_is_recurring |=
			e_cal_util_component_is_instance (icalcomp) ||
			e_cal_util_component_has_recurrences (icalcomp);

		/* XXX The rest of this is rather expensive and
		 *     only applies if a single event is selected,
		 *     so continue with the loop iteration if the
		 *     rest of this is not applicable. */
		if (n_selected > 1)
			continue;

		/* XXX This probably belongs in comp-util.c. */

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (
			comp, icalcomponent_new_clone (icalcomp));
		user_email = itip_get_comp_attendee (
			registry, comp, client);

		selection_is_organizer =
			e_cal_util_component_has_organizer (icalcomp) &&
			itip_organizer_is_user (registry, comp, client);

		capability = CAL_STATIC_CAPABILITY_DELEGATE_SUPPORTED;
		cap_delegate_supported =
			e_client_check_capability (
			E_CLIENT (client), capability);

		capability = CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY;
		cap_delegate_to_many =
			e_client_check_capability (
			E_CLIENT (client), capability);

		icalcomp_is_delegated =
			(user_email != NULL) &&
			cal_shell_content_icalcomp_is_delegated (
			icalcomp, user_email);

		selection_can_delegate =
			cap_delegate_supported &&
			(cap_delegate_to_many ||
			(!selection_is_organizer &&
			 !icalcomp_is_delegated));

		g_free (user_email);
		g_object_unref (comp);
	}

	g_list_free (selected);

	if (n_selected == 1)
		state |= E_CAL_SHELL_CONTENT_SELECTION_SINGLE;
	if (n_selected > 1)
		state |= E_CAL_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (selection_is_editable)
		state |= E_CAL_SHELL_CONTENT_SELECTION_IS_EDITABLE;
	if (selection_is_instance)
		state |= E_CAL_SHELL_CONTENT_SELECTION_IS_INSTANCE;
	if (selection_is_meeting)
		state |= E_CAL_SHELL_CONTENT_SELECTION_IS_MEETING;
	if (selection_is_organizer)
		state |= E_CAL_SHELL_CONTENT_SELECTION_IS_ORGANIZER;
	if (selection_is_recurring)
		state |= E_CAL_SHELL_CONTENT_SELECTION_IS_RECURRING;
	if (selection_can_delegate)
		state |= E_CAL_SHELL_CONTENT_SELECTION_CAN_DELEGATE;

	return state;
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
e_cal_shell_content_class_init (ECalShellContentClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	EShellContentClass *shell_content_class;

	g_type_class_add_private (class, sizeof (ECalShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_shell_content_set_property;
	object_class->get_property = cal_shell_content_get_property;
	object_class->dispose = cal_shell_content_dispose;
	object_class->constructed = cal_shell_content_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = cal_shell_content_map;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = cal_shell_content_check_state;
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
e_cal_shell_content_class_finalize (ECalShellContentClass *class)
{
}

static void
e_cal_shell_content_init (ECalShellContent *cal_shell_content)
{
	cal_shell_content->priv =
		E_CAL_SHELL_CONTENT_GET_PRIVATE (cal_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

void
e_cal_shell_content_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_shell_content_register_type (type_module);
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

