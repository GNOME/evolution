/*
 * evolution-module-settings.c
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
 */

#include "e-settings-cal-model.h"
#include "e-settings-calendar-item.h"
#include "e-settings-calendar-view.h"
#include "e-settings-client-cache.h"
#include "e-settings-comp-editor.h"
#include "e-settings-date-edit.h"
#include "e-settings-mail-formatter.h"
#include "e-settings-mail-reader.h"
#include "e-settings-meeting-store.h"
#include "e-settings-meeting-time-selector.h"
#include "e-settings-name-selector-entry.h"
#include "e-settings-web-view.h"
#include "e-settings-web-view-gtkhtml.h"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_settings_cal_model_type_register (type_module);
	e_settings_calendar_item_type_register (type_module);
	e_settings_calendar_view_type_register (type_module);
	e_settings_client_cache_type_register (type_module);
	e_settings_comp_editor_type_register (type_module);
	e_settings_date_edit_type_register (type_module);
	e_settings_mail_formatter_type_register (type_module);
	e_settings_mail_reader_type_register (type_module);
	e_settings_meeting_store_type_register (type_module);
	e_settings_meeting_time_selector_type_register (type_module);
	e_settings_name_selector_entry_type_register (type_module);
	e_settings_web_view_type_register (type_module);
	e_settings_web_view_gtkhtml_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

