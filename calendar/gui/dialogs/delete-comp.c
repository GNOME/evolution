/* Evolution calendar - Delete calendar component dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
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

#include <glib.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-icon-factory.h>
#include "widgets/misc/e-error.h"
#include "../calendar-config.h"
#include "delete-comp.h"



/**
 * delete_component_dialog:
 * @comp: A calendar component if a single component is to be deleted, or NULL
 * if more that one component is to be deleted.
 * @consider_as_untitled: If deleting more than one component, this is ignored.
 * Otherwise, whether to consider the component as not having a summary; if
 * FALSE then the component's summary string will be used.
 * @n_comps: Number of components that are to be deleted.
 * @vtype: Type of the components that are to be deleted.  This is ignored
 * if only one component is to be deleted, and the vtype is extracted from
 * the component instead.
 * @widget: A widget to use as a basis for conversion from UTF8 into font
 * encoding.
 * 
 * Pops up a dialog box asking the user whether he wants to delete a number of
 * calendar components.  The dialog will not appear, however, if the
 * configuration option for confirmation is turned off.
 * 
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.  If the
 * configuration option for confirmation is turned off, this function will
 * unconditionally return TRUE.
 **/
gboolean
delete_component_dialog (ECalComponent *comp,
			 gboolean consider_as_untitled,
			 int n_comps, ECalComponentVType vtype,
			 GtkWidget *widget)
{
	const char *stock_icon, *id;
	char *arg0 = NULL;
	int response;
	
	if (comp) {
		g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
		g_return_val_if_fail (n_comps == 1, FALSE);
	} else {
		g_return_val_if_fail (n_comps > 1, FALSE);
		g_return_val_if_fail (vtype != E_CAL_COMPONENT_NO_TYPE, FALSE);
	}

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	if (!calendar_config_get_confirm_delete ())
		return TRUE;

	if (comp) {
		ECalComponentText summary;
		
		vtype = e_cal_component_get_vtype (comp);
		
		if (!consider_as_untitled) {
			e_cal_component_get_summary (comp, &summary);
			arg0 = g_strdup (summary.value);
		}
		
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			stock_icon = "stock_calendar";
			if (arg0)
				id = "calendar:prompt-delete-titled-appointment";
			else
				id = "calendar:prompt-delete-appointment";
			break;

		case E_CAL_COMPONENT_TODO:
			stock_icon = "stock_todo";
			if (arg0)
				id = "calendar:prompt-delete-named-task";
			else
				id = "calendar:prompt-delete-task";
			break;

		case E_CAL_COMPONENT_JOURNAL:
			stock_icon = "stock_calendar";
			if (arg0)
				id = "calendar:prompt-delete-named-journal";
			else
				id = "calendar:prompt-delete-journal";
			break;

		default:
			g_message ("delete_component_dialog(): Cannot handle object of type %d",
				   vtype);
			g_free (arg0);
			return FALSE;
		}
	} else {
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			if (n_comps == 1)
				id = "calendar:prompt-delete-appointment";
			else
				id = "calendar:prompt-delete-appointments";
			break;

		case E_CAL_COMPONENT_TODO:
			if (n_comps == 1)
				id = "calendar:prompt-delete-task";
			else
				id = "calendar:prompt-delete-tasks";
			break;

		case E_CAL_COMPONENT_JOURNAL:
			if (n_comps == 1)
				id = "calendar:prompt-delete-journal";
			else
				id = "calendar:prompt-delete-journals";
			break;

		default:
			g_message ("delete_component_dialog(): Cannot handle objects of type %d",
				   vtype);
			return FALSE;
		}
		
		if (n_comps > 1)
			arg0 = g_strdup_printf ("%d", n_comps);
	}
	
	response = e_error_run ((GtkWindow *) gtk_widget_get_toplevel (widget), id, arg0, NULL);
	g_free (arg0);
	
	return response == GTK_RESPONSE_YES;
}
