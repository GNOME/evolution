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

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <gal/widgets/e-unicode.h>
#include "save-comp.h"



/**
 * save_component_dialog:
 * @parent: Window to use as the transient dialog's parent.
 * 
 * Pops up a dialog box asking the user whether he wants to save changes for
 * a calendar component.
 * 
 * Return value: the response_id of the button selected.
 **/
GtkResponseType
save_component_dialog (GtkWindow *parent)
{
	GtkWidget *dialog;
	gint r;

	dialog = gnome_message_box_new (_("Do you want to save changes?"),
					GNOME_MESSAGE_BOX_QUESTION,
					GNOME_STOCK_BUTTON_CANCEL,
					GNOME_STOCK_BUTTON_NO,
					GNOME_STOCK_BUTTON_YES,
					NULL);

	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	gnome_dialog_grab_focus (GNOME_DIALOG (dialog), 0);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), parent);

	r = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	if (r == 1)
		return GTK_RESPONSE_NO;
	else if (r == 2)
		return GTK_RESPONSE_YES;

	return GTK_RESPONSE_CANCEL;
}
