/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-view-actions.c
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

#include "e-cal-shell-view-private.h"

static void
action_calendar_copy_cb (GtkAction *action,
                         ECalShellView *cal_shell_view)
{
}

static void
action_calendar_delete_cb (GtkAction *action,
                           ECalShellView *cal_shell_view)
{
}

static void
action_calendar_go_back_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
}

static void
action_calendar_go_forward_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
}

static void
action_calendar_go_today_cb (GtkAction *action,
                             ECalShellView *cal_shell_view)
{
}

static void
action_calendar_jump_to_cb (GtkAction *action,
                            ECalShellView *cal_shell_view)
{
}

static void
action_calendar_new_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
}

static void
action_calendar_print_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
}

static void
action_calendar_print_preview_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
}

static void
action_calendar_properties_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
}

static void
action_calendar_purge_cb (GtkAction *action,
                          ECalShellView *cal_shell_view)
{
}

static void
action_calendar_view_cb (GtkRadioAction *action,
                         GtkRadioAction *current,
                         ECalShellView *cal_shell_view)
{
}

static void
action_event_clipboard_copy_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
}

static void
action_event_clipboard_cut_cb (GtkAction *action,
                               ECalShellView *cal_shell_view)
{
}

static void
action_event_clipboard_paste_cb (GtkAction *action,
                                 ECalShellView *cal_shell_view)
{
}

static void
action_event_delete_cb (GtkAction *action,
                        ECalShellView *cal_shell_view)
{
}

static void
action_event_delete_occurrence_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
}

static void
action_event_delete_occurrence_all_cb (GtkAction *action,
                                       ECalShellView *cal_shell_view)
{
}

static void
action_event_open_cb (GtkAction *action,
                      ECalShellView *cal_shell_view)
{
}

static GtkActionEntry calendar_entries[] = {

	{ "calendar-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_copy_cb) },

	{ "calendar-delete",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_delete_cb) },

	{ "calendar-go-back",
	  GTK_STOCK_GO_BACK,
	  N_("Previous"),
	  NULL,
	  N_("Go Back"),
	  G_CALLBACK (action_calendar_go_back_cb) },

	{ "calendar-go-forward",
	  GTK_STOCK_GO_FORWARD,
	  N_("Next"),
	  NULL,
	  N_("Go Forward"),
	  G_CALLBACK (action_calendar_go_forward_cb) },

	{ "calendar-go-today",
	  "go-today",
	  N_("Select _Today"),
	  "<Control>t",
	  N_("Select today"),
	  G_CALLBACK (action_calendar_go_today_cb) },

	{ "calendar-jump-to",
	  GTK_STOCK_JUMP_TO,
	  N_("Select _Date"),
	  "<Control>g",
	  N_("Select a specific date"),
	  G_CALLBACK (action_calendar_jump_to_cb) },

	{ "calendar-new",
	  "x-office-calendar",
	  N_("_New Calendar"),
	  NULL,
	  N_("Create a new calendar"),
	  G_CALLBACK (action_calendar_new_cb) },

	{ "calendar-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print this calendar"),
	  G_CALLBACK (action_calendar_print_cb) },

	{ "calendar-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the calendar to be printed"),
	  G_CALLBACK (action_calendar_print_preview_cb) },

	{ "calendar-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_properties_cb) },

	{ "calendar-purge",
	  NULL,
	  N_("Purg_e"),
	  "<Control>e",
	  N_("Purge old appointments and meetings"),
	  G_CALLBACK (action_calendar_purge_cb) },

	{ "event-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy the selection"),
	  G_CALLBACK (action_event_clipboard_copy_cb) },

	{ "event-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut the selection"),
	  G_CALLBACK (action_event_clipboard_cut_cb) },

	{ "event-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste the clipboard"),
	  G_CALLBACK (action_event_clipboard_paste_cb) },

	{ "event-delete",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  N_("Delete the appointment"),
	  G_CALLBACK (action_event_delete_cb) },

	{ "event-delete-occurrence",
	  GTK_STOCK_DELETE,
	  N_("Delete This _Occurrence"),
	  NULL,
	  N_("Delete this occurrence"),
	  G_CALLBACK (action_event_delete_occurrence_cb) },

	{ "event-delete-occurrence-all",
	  GTK_STOCK_DELETE,
	  N_("Delete _All Occurrences"),
	  NULL,
	  N_("Delete all occurrences"),
	  G_CALLBACK (action_event_delete_occurrence_all_cb) },

	{ "event-open",
	  NULL,
	  N_("_Open Appointment"),
	  "<Control>o",
	  N_("View the current appointment"),
	  G_CALLBACK (action_event_open_cb) }
};

static GtkRadioActionEntry calendar_view_entries[] = {

	{ "calendar-view-day",
	  "view-calendar-day",
	  N_("Day"),
	  NULL,
	  N_("Show one day"),
	  GNOME_CAL_DAY_VIEW },

	{ "calendar-view-list",
	  "view-calendar-list",
	  N_("List"),
	  NULL,
	  N_("Show as list"),
	  GNOME_CAL_LIST_VIEW },

	{ "calendar-view-month",
	  "view-calendar-month",
	  N_("Month"),
	  NULL,
	  N_("Show one month"),
	  GNOME_CAL_MONTH_VIEW },

	{ "calendar-view-week",
	  "view-calendar-week",
	  N_("Week"),
	  NULL,
	  N_("Show one week"),
	  GNOME_CAL_WEEK_VIEW },

	{ "calendar-view-workweek",
	  "view-calendar-workweek",
	  N_("Work Week"),
	  NULL,
	  N_("Show one work week"),
	  GNOME_CAL_WORK_WEEK_VIEW }
};

void
e_cal_shell_view_actions_init (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	e_load_ui_definition (manager, "evolution-calendars.ui");

	action_group = cal_shell_view->priv->calendar_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, calendar_entries,
		G_N_ELEMENTS (calendar_entries), cal_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, calendar_view_entries,
		G_N_ELEMENTS (calendar_view_entries), GNOME_CAL_DAY_VIEW,
		G_CALLBACK (action_calendar_view_cb), cal_shell_view);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
}
