/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <e-util/e-dialog-utils.h>

#include "cal-editor-utils.h"

#include "e-comp-editor-registry.h"
#include "dialogs/event-editor.h"
#include "dialogs/task-editor.h"
#include "dialogs/memo-editor.h"

#ifdef G_OS_WIN32
const ECompEditorRegistry * const comp_editor_get_registry();
#define comp_editor_registry comp_editor_get_registry()
#else
extern ECompEditorRegistry *comp_editor_registry;
#endif

/**
 * open_component_editor:
 * @client: Already opened #ECal, where to store the component
 * @comp: #ECalComponent component to be stored
 * @is_new: Whether the @comp is a new component or an existing
 * @error: #GError for possible error reporting
 *
 * Opens component editor for the event stored in the comp component.
 * If such component exists in the client already (with the same UID),
 * then there's opened already stored event, instead of the comp.
 *
 * It blocks until finished and should be called in the main thread.
 **/
void
open_component_editor (ECal *client, ECalComponent *comp, gboolean is_new, GError **error)
{
	ECalComponentId *id;
	CompEditorFlags flags = 0;
	CompEditor *editor = NULL;

	g_return_if_fail (client != NULL);
	g_return_if_fail (comp != NULL);

	id = e_cal_component_get_id (comp);
	g_return_if_fail (id != NULL);
	g_return_if_fail (id->uid != NULL);

	if (is_new) {
		flags |= COMP_EDITOR_NEW_ITEM;
	} else {
		editor = e_comp_editor_registry_find (comp_editor_registry, id->uid);
	}

	if (!editor) {
		if (itip_organizer_is_user (comp, client))
			flags |= COMP_EDITOR_USER_ORG;

		switch (e_cal_component_get_vtype (comp)) {
		case E_CAL_COMPONENT_EVENT:
			if (e_cal_component_has_attendees (comp))
				flags |= COMP_EDITOR_MEETING;

			editor = event_editor_new (client, flags);

			if (flags & COMP_EDITOR_MEETING)
				event_editor_show_meeting (EVENT_EDITOR (editor));
			break;
		case E_CAL_COMPONENT_TODO:
			if (e_cal_component_has_attendees (comp))
				flags |= COMP_EDITOR_IS_ASSIGNED;

			editor = task_editor_new (client, flags);

			if (flags & COMP_EDITOR_IS_ASSIGNED)
				task_editor_show_assignment (TASK_EDITOR (editor));
			break;
		case E_CAL_COMPONENT_JOURNAL:
			if (e_cal_component_has_organizer (comp))
				flags |= COMP_EDITOR_IS_SHARED;

			editor = memo_editor_new (client, flags);
			break;
		default:
			if (error)
				*error = g_error_new (E_CALENDAR_ERROR, E_CALENDAR_STATUS_INVALID_OBJECT, "%s", _("Invalid object"));
			break;
		}

		if (editor) {
			comp_editor_edit_comp (editor, comp);

			/* request save for new events */
			comp_editor_set_changed (editor, is_new);

			e_comp_editor_registry_add (comp_editor_registry, editor, TRUE);
		}
	}

	if (editor)
		gtk_window_present (GTK_WINDOW (editor));

	e_cal_component_free_id (id);
}
