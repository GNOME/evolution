/*
 * evolution-module-calendar.c
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

#include "e-cal-attachment-handler.h"

#include "e-cal-config-hook.h"
#include "e-cal-event-hook.h"

#include "e-cal-shell-backend.h"
#include "e-cal-shell-content.h"
#include "e-cal-shell-sidebar.h"
#include "e-cal-shell-view.h"

#include "e-memo-shell-backend.h"
#include "e-memo-shell-content.h"
#include "e-memo-shell-sidebar.h"
#include "e-memo-shell-view.h"

#include "e-task-shell-backend.h"
#include "e-task-shell-content.h"
#include "e-task-shell-sidebar.h"
#include "e-task-shell-view.h"

#include "e-calendar-preferences.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register dynamically loaded types. */

	e_cal_attachment_handler_register_type (type_module);

	e_cal_config_hook_register_type (type_module);
	e_cal_event_hook_register_type (type_module);

	e_cal_shell_view_type_register (type_module);
	e_cal_shell_backend_type_register (type_module);
	e_cal_shell_content_type_register (type_module);
	e_cal_shell_sidebar_type_register (type_module);

	e_memo_shell_view_type_register (type_module);
	e_memo_shell_backend_type_register (type_module);
	e_memo_shell_content_type_register (type_module);
	e_memo_shell_sidebar_type_register (type_module);

	e_task_shell_view_type_register (type_module);
	e_task_shell_backend_type_register (type_module);
	e_task_shell_content_type_register (type_module);
	e_task_shell_sidebar_type_register (type_module);

	e_calendar_preferences_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
