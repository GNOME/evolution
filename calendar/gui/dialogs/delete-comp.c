/* Evolution calendar - Delete calendar component dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
#include "delete-comp.h"



/**
 * delete_component_dialog:
 * @comp: A calendar component.
 * @widget: A widget to use as a basis for conversion from UTF8 into font
 * encoding.
 * 
 * Pops up a dialog box asking the user whether he wants to delete a particular
 * calendar component.
 * 
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
delete_component_dialog (CalComponent *comp, GtkWidget *widget)
{
	CalComponentText summary;
	CalComponentVType vtype;
	char *str, *tmp;
	GtkWidget *dialog;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	vtype = cal_component_get_vtype (comp);
	cal_component_get_summary (comp, &summary);

	tmp = e_utf8_to_gtk_string (widget, summary.value);

	switch (vtype) {
	case CAL_COMPONENT_EVENT:
		if (tmp)
			str = g_strdup_printf (_("Are you sure you want to delete the appointment "
						 "`%s'?"), tmp);
		else
			str = g_strdup (_("Are you sure you want to delete this "
					  "untitled appointment?"));
		break;

	case CAL_COMPONENT_TODO:
		if (tmp)
			str = g_strdup_printf (_("Are you sure you want to delete the task "
						 "`%s'?"), tmp);
		else
			str = g_strdup (_("Are you sure you want to delete this "
					  "untitled task?"));
		break;

	case CAL_COMPONENT_JOURNAL:
		if (tmp)
			str = g_strdup_printf (_("Are you sure you want to delete the journal entry "
						 "`%s'?"), tmp);
		else
			str = g_strdup (_("Are you sure want to delete this "
					  "untitled journal entry?"));
		break;

	default:
		g_message ("delete_component_dialog(): Cannot handle object of type %d", vtype);
		return FALSE;
	}

	dialog = gnome_question_dialog_modal (str, NULL, NULL);
	g_free (tmp);
	g_free (str);

	if (gnome_dialog_run (GNOME_DIALOG (dialog)) == GNOME_YES)
		return TRUE;
	else
		return FALSE;
}
