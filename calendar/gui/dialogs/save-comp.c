/* Evolution calendar - Delete calendar component dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "widgets/misc/e-error.h"
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
save_component_dialog (GtkWindow *parent, ECalComponent *comp)
{
	ECalComponentVType vtype = e_cal_component_get_vtype(comp);

	switch(vtype) {
		case E_CAL_COMPONENT_EVENT:
			return e_error_run (parent, "calendar:prompt-save-appointment", NULL);
		case E_CAL_COMPONENT_TODO:
			return e_error_run (parent, "calendar:prompt-save-task", NULL);
		default:
			return GTK_RESPONSE_NO;
	}
}
