/*
 * e-cal-shell-view.c
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

#include "e-cal-shell-view-private.h"

static gpointer parent_class;
static GType cal_shell_view_type;

static void
cal_shell_view_dispose (GObject *object)
{
	e_cal_shell_view_private_dispose (E_CAL_SHELL_VIEW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_view_finalize (GObject *object)
{
	e_cal_shell_view_private_finalize (E_CAL_SHELL_VIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cal_shell_view_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	e_cal_shell_view_private_constructed (E_CAL_SHELL_VIEW (object));
}

static void
cal_shell_view_execute_search (EShellView *shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GnomeCalendar *calendar;
	ECalendar *date_navigator;
	GtkRadioAction *action;
	time_t start_range;
	time_t end_range;
	time_t now_time;
	gboolean range_search;
	gchar *start, *end;
	gchar *query;
	gchar *temp;
	gint value;

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);
	cal_shell_sidebar = E_CAL_SHELL_SIDEBAR (shell_sidebar);

	searchbar = e_cal_shell_content_get_searchbar (cal_shell_content);

	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	now_time = time_day_begin (icaltime_as_timet (icaltime_current_time_with_zone (e_cal_model_get_timezone (gnome_calendar_get_model (calendar)))));

	action = GTK_RADIO_ACTION (ACTION (CALENDAR_SEARCH_ANY_FIELD_CONTAINS));
	value = gtk_radio_action_get_current_value (action);

	if (value == CALENDAR_SEARCH_ADVANCED) {
		query = e_shell_view_get_search_query (shell_view);

		if (!query)
			query = g_strdup ("");
	} else {
		const gchar *format;
		const gchar *text;
		GString *string;

		text = e_shell_searchbar_get_search_text (searchbar);

		if (text == NULL || *text == '\0') {
			text = "";
			value = CALENDAR_SEARCH_SUMMARY_CONTAINS;
		}

		switch (value) {
			default:
				text = "";
				/* fall through */

			case CALENDAR_SEARCH_SUMMARY_CONTAINS:
				format = "(contains? \"summary\" %s)";
				break;

			case CALENDAR_SEARCH_DESCRIPTION_CONTAINS:
				format = "(contains? \"description\" %s)";
				break;

			case CALENDAR_SEARCH_ANY_FIELD_CONTAINS:
				format = "(contains? \"any\" %s)";
				break;
		}

		/* Build the query. */
		string = g_string_new ("");
		e_sexp_encode_string (string, text);
		query = g_strdup_printf (format, string->str);
		g_string_free (string, TRUE);
	}

	range_search = FALSE;
	start_range = end_range = 0;

	/* Apply selected filter. */
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	value = e_action_combo_box_get_current_value (combo_box);
	switch (value) {
		case CALENDAR_FILTER_ANY_CATEGORY:
			break;

		case CALENDAR_FILTER_UNMATCHED:
			temp = g_strdup_printf (
				"(and (has-categories? #f) %s)", query);
			g_free (query);
			query = temp;
			break;

		case CALENDAR_FILTER_ACTIVE_APPOINTMENTS:
			/* Show a year's worth of appointments. */
			start_range = now_time;
			end_range = time_day_end (time_add_day (start_range, 365));
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (occur-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")))",
				query, start, end);
			g_free (query);
			query = temp;

			range_search = TRUE;
			break;

		case CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS:
			start_range = now_time;
			end_range = time_day_end (time_add_day (start_range, 7));
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			temp = g_strdup_printf (
				"(and %s (occur-in-time-range? "
				"(make-time \"%s\") (make-time \"%s\")))",
				query, start, end);
			g_free (query);
			query = temp;

			range_search = TRUE;
			break;

		default:
		{
			GList *categories;
			const gchar *category_name;

			categories = e_util_get_searchable_categories ();
			category_name = g_list_nth_data (categories, value);
			g_list_free (categories);

			temp = g_strdup_printf (
				"(and (has-categories? \"%s\") %s)",
				category_name, query);
			g_free (query);
			query = temp;
			break;
		}
	}

	date_navigator = e_cal_shell_sidebar_get_date_navigator (cal_shell_sidebar);

	if (range_search) {
		/* Switch to list view and hide the date navigator. */
		action = GTK_RADIO_ACTION (ACTION (CALENDAR_VIEW_LIST));
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		gtk_widget_hide (GTK_WIDGET (date_navigator));
	} else {
		/* Ensure the date navigator is visible. */
		gtk_widget_show (GTK_WIDGET (date_navigator));
	}

	/* Submit the query. */
	gnome_calendar_set_search_query (
		calendar, query, range_search, start_range, end_range);
	g_free (query);
}

static icalproperty *
get_attendee_prop (icalcomponent *icalcomp, const gchar *address)
{

	icalproperty *prop;

	if (!(address && *address))
		return NULL;

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
			prop;
			prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee = icalproperty_get_attendee (prop);

		if (g_str_equal (itip_strip_mailto (attendee), address)) {
			return prop;
		}
	}
	return NULL;
}

static gboolean
is_delegated (icalcomponent *icalcomp, const gchar *user_email)
{
	icalproperty *prop;
	icalparameter *param;
	const gchar *delto = NULL;

	prop = get_attendee_prop (icalcomp, user_email);

	if (prop) {
		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDTO_PARAMETER);
		if (param)
			delto = icalparameter_get_delegatedto (param);
	} else
		return FALSE;

	prop = get_attendee_prop (icalcomp, itip_strip_mailto (delto));

	if (prop) {
		const gchar *delfrom = NULL;
		icalparameter_partstat status = ICAL_PARTSTAT_NONE;

		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
		if (param)
			delfrom = icalparameter_get_delegatedfrom (param);
		param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		if (param)
			status = icalparameter_get_partstat (param);
		if ((delfrom && *delfrom) && g_str_equal (itip_strip_mailto (delfrom), user_email)
				&& status != ICAL_PARTSTAT_DECLINED)
			return TRUE;
	}

	return FALSE;
}

static void
cal_shell_view_update_actions (EShellView *shell_view)
{
	ECalShellViewPrivate *priv;
	ECalShellContent *cal_shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShell *shell;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *view;
	GtkAction *action;
	GList *list, *iter;
	gboolean sensitive;
	gboolean visible;
	guint32 state;
	gint n_selected;

	/* Be descriptive. */
	gboolean can_delete_primary_source;
	gboolean editable = TRUE;
	gboolean has_primary_source;
	gboolean recurring = FALSE;
	gboolean is_instance = FALSE;
	gboolean is_meeting = FALSE;
	gboolean is_delegatable = FALSE;
	gboolean refresh_supported = FALSE;

	/* Chain up to parent's update_actions() method. */
	E_SHELL_VIEW_CLASS (parent_class)->update_actions (shell_view);

	priv = E_CAL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	if (e_shell_get_express_mode (shell)) {
		GtkWidget *widget;

		/* Hide the New button on the toolbar. */
		widget = e_shell_window_get_managed_widget (
			shell_window, "/main-toolbar");
		widget = (GtkWidget *)gtk_toolbar_get_nth_item (
			GTK_TOOLBAR (widget), 0);
		gtk_widget_hide (widget);

		/* Hide the main menu. */
		widget = e_shell_window_get_managed_widget (
			shell_window, "/main-menu");
		gtk_widget_hide (widget);
	}

	cal_shell_content = priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);

	list = e_calendar_view_get_selected_events (view);
	n_selected = g_list_length (list);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalendarViewEvent *event = iter->data;
		ECal *client;
		ECalComponent *comp;
		icalcomponent *icalcomp;
		gchar *user_email = NULL;
		gboolean user_org = FALSE;
		gboolean read_only = TRUE;

		if (!is_comp_data_valid (event))
			continue;

		client = event->comp_data->client;
		icalcomp = event->comp_data->icalcomp;

		e_cal_is_read_only (client, &read_only, NULL);
		editable = editable && !read_only;

		is_instance |= e_cal_util_component_is_instance (icalcomp);

		recurring |=
			e_cal_util_component_is_instance (icalcomp) ||
			e_cal_util_component_has_recurrences (icalcomp);

		/* The following tests only apply if one event is selected. */
		if (iter != list || n_selected > 1)
			continue;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (
			comp, icalcomponent_new_clone (icalcomp));
		user_email = itip_get_comp_attendee (comp, client);

		is_meeting = e_cal_util_component_has_attendee (icalcomp);

		user_org =
			e_cal_util_component_has_organizer (icalcomp) &&
			itip_organizer_is_user (comp, client);

		is_delegatable =
			e_cal_get_static_capability (
				client, CAL_STATIC_CAPABILITY_DELEGATE_SUPPORTED) &&
			((e_cal_get_static_capability (
				client, CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) ||
			(!user_org && !is_delegated (icalcomp, user_email)));

		g_free (user_email);
		g_object_unref (comp);
	}

	g_list_free (list);

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	state = e_shell_sidebar_check_state (shell_sidebar);

	has_primary_source =
		(state & E_CAL_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE);
	can_delete_primary_source =
		(state & E_CAL_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE);
	refresh_supported =
		(state & E_CAL_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH);

	action = ACTION (CALENDAR_COPY);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_DELETE);
	sensitive = can_delete_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_PROPERTIES);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_REFRESH);
	sensitive = refresh_supported;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_RENAME);
	sensitive = can_delete_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELEGATE);
	sensitive =
		(n_selected == 1) && editable &&
		is_delegatable && is_meeting;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE);
	sensitive = (n_selected > 0) && editable && !recurring;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE_OCCURRENCE);
	sensitive = (n_selected > 0) && editable && recurring;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_DELETE_OCCURRENCE_ALL);
	sensitive = (n_selected > 0) && editable && recurring;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_FORWARD);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_OCCURRENCE_MOVABLE);
	sensitive =
		(n_selected == 1) && editable &&
		recurring && is_instance;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_PRINT);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_SCHEDULE);
	sensitive = (n_selected == 1) && editable && !is_meeting;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_SCHEDULE_APPOINTMENT);
	sensitive = (n_selected == 1) && editable && is_meeting;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_REPLY);
	sensitive = (n_selected == 1) && is_meeting;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_REPLY_ALL);
	sensitive = (n_selected == 1) && is_meeting;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_MEETING_NEW);
	visible = itip_addresses_get_default () != NULL;
	gtk_action_set_visible (action, visible);
}

static void
cal_shell_view_class_init (ECalShellViewClass *class,
                            GTypeModule *type_module)
{
	GObjectClass *object_class;
	EShellViewClass *shell_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = cal_shell_view_dispose;
	object_class->finalize = cal_shell_view_finalize;
	object_class->constructed = cal_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Calendar");
	shell_view_class->icon_name = "x-office-calendar";
	shell_view_class->ui_definition = "evolution-calendars.ui";
	shell_view_class->ui_manager_id = "org.gnome.evolution.calendars";
	shell_view_class->search_options = "/calendar-search-options";
	shell_view_class->search_rules = "caltypes.xml";
	shell_view_class->new_shell_content = e_cal_shell_content_new;
	shell_view_class->new_shell_sidebar = e_cal_shell_sidebar_new;
	shell_view_class->execute_search = cal_shell_view_execute_search;
	shell_view_class->update_actions = cal_shell_view_update_actions;

	/* XXX This is an unusual place to need an EShell instance.
	 *     Would be cleaner to implement a method that either
	 *     chains up or does nothing based on express mode. */
	if (e_shell_get_express_mode (e_shell_get_default ()))
		shell_view_class->construct_searchbar = NULL;

}

static void
cal_shell_view_init (ECalShellView *cal_shell_view,
                     EShellViewClass *shell_view_class)
{
	cal_shell_view->priv =
		E_CAL_SHELL_VIEW_GET_PRIVATE (cal_shell_view);

	e_cal_shell_view_private_init (cal_shell_view, shell_view_class);
}

GType
e_cal_shell_view_get_type (void)
{
	return cal_shell_view_type;
}

void
e_cal_shell_view_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (ECalShellViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_shell_view_class_init,
		(GClassFinalizeFunc) NULL,
		type_module,
		sizeof (ECalShellView),
		0,    /* n_preallocs */
		(GInstanceInitFunc) cal_shell_view_init,
		NULL  /* value_table */
	};

	cal_shell_view_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_VIEW,
		"ECalShellView", &type_info, 0);
}
