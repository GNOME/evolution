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
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <e-util/e-icon-factory.h>
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

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("This event has been changed, but has not been saved.\n\n"
					   "Do you wish to save your changes?"));
       
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Discard Changes"),GTK_RESPONSE_NO,
				GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_YES,
				NULL);

	gtk_window_set_icon (GTK_WINDOW (dialog), e_icon_factory_get_icon("stock_calendar", 32));
	gtk_window_set_title (GTK_WINDOW (dialog), _("Save Event"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

	r = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return r;

}
