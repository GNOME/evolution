/* Evolution calendar - Send calendar component dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-uidefs.h>
#include <gal/widgets/e-unicode.h>
#include "changed-comp.h"



/**
 * changed_component_dialog:
 * @comp: A calendar component
 * @deleted: Whether the object is being deleted or updated
 * @changed: Whether or not the user has made changes
 * 
 * Pops up a dialog box asking the user whether changes made (if any)
 * should be thrown away because the item has been updated elsewhere
 * 
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
changed_component_dialog (CalComponent *comp, gboolean deleted, gboolean changed)
{
	GtkWidget *dialog;
	CalComponentVType vtype;
	char *str;

	vtype = cal_component_get_vtype (comp);

	if (deleted) {
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			str = _("This event has been deleted.");
			break;

		case CAL_COMPONENT_TODO:
			str = _("This task has been deleted.");
			break;

		case CAL_COMPONENT_JOURNAL:
			str = _("This journal entry has been deleted.");
			break;

		default:
			g_message ("changed_component_dialog(): "
				   "Cannot handle object of type %d", vtype);
			return FALSE;
		}
		if (changed)
			str = g_strdup_printf (_("%s  You have made changes. Forget those changes and close the editor?"), str);
		else
			str = g_strdup_printf (_("%s  You have made no changes, close the editor?"), str); 

	} else {
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			str = _("This event has been changed.");
			break;

		case CAL_COMPONENT_TODO:
			str = _("This task has been changed.");
			break;

		case CAL_COMPONENT_JOURNAL:
			str = _("This journal entry has been changed.");
			break;

		default:
			g_message ("changed_component_dialog(): "
				   "Cannot handle object of type %d", vtype);
			return FALSE;
		}
		if (changed)
			str = g_strdup_printf (_("%s  You have made changes. Forget those changes and update the editor?"), str);
		else
			str = g_strdup_printf (_("%s  You have made no changes, update the editor?"), str); 
	}
	
	dialog = gnome_question_dialog_modal (str, NULL, NULL);

	if (gnome_dialog_run (GNOME_DIALOG (dialog)) == GNOME_YES)
		return TRUE;
	else
		return FALSE;
}
