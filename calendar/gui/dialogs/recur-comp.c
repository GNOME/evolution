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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <gal/widgets/e-unicode.h>
#include "widgets/misc/e-messagebox.h"
#include "recur-comp.h"



CalObjModType
recur_component_dialog (CalComponent *comp,
			GtkWidget *widget)
{
	char *str;
	GtkWidget *dialog;
	CalComponentVType vtype;
	gint button;
	
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), CALOBJ_MOD_THIS);
	g_return_val_if_fail (widget != NULL, CALOBJ_MOD_THIS);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), CALOBJ_MOD_THIS);


	vtype = cal_component_get_vtype (comp);
	
	switch (vtype) {
	case CAL_COMPONENT_EVENT:
		str = g_strdup_printf (_("You are modifying a recurring event, what would you like to modify?"));
		break;

	case CAL_COMPONENT_TODO:
		str = g_strdup_printf (_("You are modifying a recurring task, what would you like to modify?"));
		break;

	case CAL_COMPONENT_JOURNAL:
		str = g_strdup_printf (_("You are modifying a recurring journal, what would you like to modify?"));
		break;

	default:
		g_message ("recur_component_dialog(): Cannot handle object of type %d", vtype);
		return CALOBJ_MOD_THIS;
	}

	dialog = e_message_box_new (str, E_MESSAGE_BOX_QUESTION,
				    _("This Instance Only"),
				    _("This and Prior"),
				    _("This and Future"),
				    _("All"),
				    NULL);
	g_free (str);

	gtk_widget_hide (e_message_box_get_checkbox (E_MESSAGE_BOX (dialog)));

	button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	
	return MIN (1 << button, 0x07);
}
