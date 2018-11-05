/*
 * evolution-module-settings.c
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
 */

#include "e-settings-cal-model.h"
#include "e-settings-calendar-item.h"
#include "e-settings-calendar-view.h"
#include "e-settings-client-cache.h"
#include "e-settings-content-editor.h"
#include "e-settings-date-edit.h"
#include "e-settings-deprecated.h"
#include "e-settings-mail-browser.h"
#include "e-settings-mail-formatter.h"
#include "e-settings-mail-part-headers.h"
#include "e-settings-mail-reader.h"
#include "e-settings-mail-session.h"
#include "e-settings-meeting-store.h"
#include "e-settings-meeting-time-selector.h"
#include "e-settings-message-list.h"
#include "e-settings-name-selector-entry.h"
#include "e-settings-spell-checker.h"
#include "e-settings-spell-entry.h"
#include "e-settings-weekday-chooser.h"

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
	e_settings_content_editor_type_register (type_module);
	e_settings_date_edit_type_register (type_module);
	e_settings_deprecated_type_register (type_module);
	e_settings_content_editor_type_register (type_module);
	e_settings_mail_browser_type_register (type_module);
	e_settings_mail_formatter_type_register (type_module);
	e_settings_mail_part_headers_type_register (type_module);
	e_settings_mail_reader_type_register (type_module);
	e_settings_mail_session_type_register (type_module);
	e_settings_meeting_store_type_register (type_module);
	e_settings_meeting_time_selector_type_register (type_module);
	e_settings_message_list_type_register (type_module);
	e_settings_name_selector_entry_type_register (type_module);
	e_settings_spell_checker_type_register (type_module);
	e_settings_spell_entry_type_register (type_module);
	e_settings_weekday_chooser_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

