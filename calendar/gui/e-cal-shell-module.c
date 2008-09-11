/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-module.c
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

#include <glib/gi18n.h>

#include <e-shell.h>
#include <e-shell-module.h>
#include <e-shell-window.h>

#include <e-cal-shell-view.h>

#define MODULE_NAME		"calendar"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		"calendar"
#define MODULE_SEARCHES		"caltypes.xml"
#define MODULE_SORT_ORDER	400

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
action_appointment_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
}

static void
action_appointment_all_day_new_cb (GtkAction *action,
                                   EShellWindow *shell_window)
{
}

static void
action_meeting_new_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
}

static void
action_calendar_new_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
}

static GtkActionEntry item_entries[] = {

	{ "appointment-new",
	  "appointment-new",
	  N_("_Appointment"),  /* XXX Need C_() here */
	  "<Control>a",
	  N_("Create a new appointment"),
	  G_CALLBACK (action_appointment_new_cb) },

	{ "appointment-all-day-new",
	  "stock_new-24h-appointment",
	  N_("All Day A_ppointment"),
	  NULL,
	  N_("Create a new all-day appointment"),
	  G_CALLBACK (action_appointment_all_day_new_cb) },

	{ "meeting-new",
	  "stock_new-meeting",
	  N_("M_eeting"),
	  "<Control>e",
	  N_("Create a new meeting request"),
	  G_CALLBACK (action_meeting_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "calendar-new",
	  "x-office-calendar",
	  N_("Cale_ndar"),
	  NULL,
	  N_("Create a new calendar"),
	  G_CALLBACK (action_calendar_new_cb) }
};

static gboolean
cal_module_handle_uri (EShellModule *shell_module,
                       const gchar *uri)
{
	/* FIXME */
	return FALSE;
}

static void
cal_module_window_created (EShellModule *shell_module,
                           EShellWindow *shell_window)
{
	const gchar *module_name;

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		shell_window, module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		shell_window, module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SEARCHES,
	MODULE_SORT_ORDER
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	e_cal_shell_view_get_type (type_module);
	e_shell_module_set_info (shell_module, &module_info);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (cal_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (cal_module_window_created), shell_module);
}
