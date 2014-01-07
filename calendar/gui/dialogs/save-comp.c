/*
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
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "save-comp.h"
#include "comp-editor.h"

/**
 * save_component_dialog:
 * @parent: Window to use as the transient dialog's parent.
 * @comp: Pointer to the EcalComponent
 *
 * Pops up a dialog box asking the user whether he wants to save changes for
 * a calendar component.
 *
 * Return value: the response_id of the button selected.
 **/

GtkResponseType
save_component_dialog (GtkWindow *parent,
                       ECalComponent *comp)
{
	ECalComponentVType vtype = e_cal_component_get_vtype (comp);
	CompEditorFlags flags;

	switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			flags = comp_editor_get_flags (COMP_EDITOR (parent));
			if (flags & COMP_EDITOR_MEETING)
				return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-meeting", NULL);
			else
				return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-appointment", NULL);
		case E_CAL_COMPONENT_TODO:
			return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-task", NULL);
		case E_CAL_COMPONENT_JOURNAL:
			return e_alert_run_dialog_for_args (parent, "calendar:prompt-save-memo", NULL);
		default:
			return GTK_RESPONSE_NO;
	}
}
