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
	GnomeCalendar *calendar;
	ECalendar *date_navigator;
	GtkRadioAction *action;
	GString *string;
	EFilterRule *rule;
	const gchar *format;
	const gchar *text;
	time_t start_range;
	time_t end_range;
	gboolean range_search;
	gchar *start, *end;
	gchar *query;
	gchar *temp;
	gint value;

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	action = GTK_RADIO_ACTION (ACTION (CALENDAR_SEARCH_ANY_FIELD_CONTAINS));
	value = gtk_radio_action_get_current_value (action);

	text = e_shell_content_get_search_text (shell_content);

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

	range_search = FALSE;
	start_range = end_range = 0;

	/* Apply selected filter. */
	value = e_shell_content_get_filter_value (shell_content);
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
			start_range = time (NULL);
			end_range = time_add_day (start_range, 365);
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
			start_range = time (NULL);
			end_range = time_add_day (start_range, 7);
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

			categories = e_categories_get_list ();
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

	/* XXX This is wrong.  We need to programmatically construct an
	 *     EFilterRule, tell it to build code, and pass the resulting
	 *     expressing string to ECalModel. */
	rule = e_filter_rule_new ();
	e_shell_content_set_search_rule (shell_content, rule);
	g_object_unref (rule);

	cal_shell_sidebar = E_CAL_SHELL_SIDEBAR (shell_sidebar);
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
	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	gnome_calendar_set_search_query (
		calendar, query, range_search, start_range, end_range);
	g_free (query);
}

static void
cal_shell_view_update_actions (EShellView *shell_view)
{
#if 0
	ECalShellViewPrivate *priv;
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellWindow *shell_window;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalendarView *view;
	ECalModel *model;
	ESourceSelector *selector;
	ESource *source;
	GtkAction *action;
	GList *list, *iter;
	const gchar *uri = NULL;
	gboolean user_created_source;
	gboolean editable = TRUE;
	gboolean recurring = FALSE;
	gboolean sensitive;
	gint n_selected;

	priv = E_CAL_SHELL_VIEW_GET_PRIVATE (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);
	model = e_calendar_view_get_model (view);

	cal_shell_sidebar = priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	list = e_calendar_view_get_selected_events (view);
	n_selected = g_list_length (list);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		editable &= !read_only;

		if (e_cal_util_component_has_recurrences (comp_data->icalcomp))
			recurring |= TRUE;
		else if (e_cal_util_component_is_instance (comp_data->icalcomp))
			recurring |= TRUE;
	}

	source = e_source_selector_peek_primary_selection (selector);
	if (source != NULL)
		uri = e_source_peek_relative_uri (source);
	user_created_source = (uri != NULL && strcmp (uri, "system") != 0);

	action = ACTION (CALENDAR_COPY);
	sensitive = (source != NULL);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_DELETE);
	sensitive = user_created_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_PROPERTIES);
	sensitive = (source != NULL);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_RENAME);
	sensitive = has_primary_source;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_CLIPBOARD_COPY);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_CLIPBOARD_CUT);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (EVENT_CLIPBOARD_PASTE);
	sensitive = editable;
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

	action = ACTION (EVENT_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);
#endif
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
