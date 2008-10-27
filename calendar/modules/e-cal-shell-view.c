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

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

GType e_cal_shell_view_type = 0;
static gpointer parent_class;

static void
cal_shell_view_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value, e_cal_shell_view_get_source_list (
				E_CAL_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

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
cal_shell_view_update_actions (EShellView *shell_view)
{
#if 0
	ECalShellViewPrivate *priv;
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellWindow *shell_window;
	GnomeCalendar *calendar;
	ECalModel *model;
	ESourceSelector *selector;
	ESource *source;
	GtkAction *action;
	GtkWidget *widget;
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
	widget = gnome_calendar_get_current_view_widget (calendar);
	model = e_calendar_view_get_model (E_CALENDAR_VIEW (widget));

	cal_shell_sidebar = priv->cal_shell_sidebar;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	list = e_calendar_view_get_selected_events (E_CALENDAR_VIEW (widget));
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
	object_class->get_property = cal_shell_view_get_property;
	object_class->dispose = cal_shell_view_dispose;
	object_class->finalize = cal_shell_view_finalize;
	object_class->constructed = cal_shell_view_constructed;

	shell_view_class = E_SHELL_VIEW_CLASS (class);
	shell_view_class->label = _("Calendar");
	shell_view_class->icon_name = "x-office-calendar";
	shell_view_class->ui_definition = "evolution-calendars.ui";
	shell_view_class->search_options = "/calendar-search-options";
	shell_view_class->search_rules = "caltypes.xml";
	shell_view_class->type_module = type_module;
	shell_view_class->new_shell_sidebar = e_cal_shell_sidebar_new;
	shell_view_class->update_actions = cal_shell_view_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			_("Source List"),
			_("The registry of calendars"),
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
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
e_cal_shell_view_get_type (GTypeModule *type_module)
{
	if (e_cal_shell_view_type == 0) {
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

		e_cal_shell_view_type =
			g_type_module_register_type (
				type_module, E_TYPE_SHELL_VIEW,
				"ECalShellView", &type_info, 0);
	}

	return e_cal_shell_view_type;
}

GnomeCalendar *
e_cal_shell_view_get_calendar (ECalShellView *cal_shell_view)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view), NULL);

	/* FIXME */
	return NULL;
}

ESourceList *
e_cal_shell_view_get_source_list (ECalShellView *cal_shell_view)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view), NULL);

	return cal_shell_view->priv->source_list;
}
