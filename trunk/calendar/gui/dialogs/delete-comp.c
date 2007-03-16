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
#include <gtk/gtk.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-icon-factory.h>
#include "e-util/e-error.h"
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
	const char *id;
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
			if (arg0)
				id = "calendar:prompt-delete-titled-appointment";
			else
				id = "calendar:prompt-delete-appointment";
			break;

		case E_CAL_COMPONENT_TODO:
			if (arg0)
				id = "calendar:prompt-delete-named-task";
			else
				id = "calendar:prompt-delete-task";
			break;

		case E_CAL_COMPONENT_JOURNAL:
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

static void
cb_toggled_cb (GtkWidget *toggle, gpointer data)
{
	gboolean active = FALSE;
	GtkWidget *entry = (GtkWidget *) data;

	active = GTK_TOGGLE_BUTTON (toggle)->active;
	gtk_widget_set_sensitive (entry, active);
}

gboolean 
prompt_retract_dialog (ECalComponent *comp, char **retract_text, GtkWidget *parent)
{
	char *message = NULL;
	ECalComponentVType type = E_CAL_COMPONENT_NO_TYPE;
	GtkMessageDialog *dialog = NULL;
	GtkWidget *cb, *label, *entry, *vbox, *sw, *frame;
	gboolean ret_val = FALSE;
	
	type = e_cal_component_get_vtype (comp);

	switch (type) {
		case E_CAL_COMPONENT_EVENT:
			message = g_strdup_printf (_("Are you sure you want to delete this meeting?"));
			break;
		case E_CAL_COMPONENT_TODO:
			message = g_strdup_printf (_("Are you sure you want to delete this task?"));
			break;
		case E_CAL_COMPONENT_JOURNAL:
			message = g_strdup_printf (_("Are you sure you want to delete this journal entry?"));
			break;
		default:
			g_message ("Retract: Unsupported object type \n");
			return FALSE;
	}

	dialog = (GtkMessageDialog *) gtk_message_dialog_new_with_markup 
		((GtkWindow *) gtk_widget_get_toplevel (parent), GTK_DIALOG_MODAL, 
		 GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "<b>%s</b>", message);
	g_free (message);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	
	gtk_box_set_spacing ((GtkBox *) (GTK_DIALOG (dialog)->vbox), 12);
	vbox = GTK_WIDGET (GTK_DIALOG (dialog)->vbox);

	cb = gtk_check_button_new_with_mnemonic (_("_Delete this item from all other recipient's mailboxes?"));
	gtk_container_add (GTK_CONTAINER (vbox), cb);

	label = gtk_label_new_with_mnemonic ("_Retract comment");
	
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget ((GtkFrame *) frame, label);
	gtk_frame_set_label_align ((GtkFrame *) frame, 0, 0);
	gtk_container_add (GTK_CONTAINER (vbox), frame);
	gtk_frame_set_shadow_type ((GtkFrame *)frame, GTK_SHADOW_NONE);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy ((GtkScrolledWindow *)sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		
	entry = gtk_text_view_new ();
	gtk_scrolled_window_add_with_viewport ((GtkScrolledWindow *)sw, entry);
	gtk_label_set_mnemonic_widget ((GtkLabel *)label, entry);
	gtk_container_add (GTK_CONTAINER (frame), sw);

	g_signal_connect ((GtkToggleButton *)cb, "toggled", G_CALLBACK (cb_toggled_cb), entry);

	gtk_widget_show_all ((GtkWidget *)dialog);

	ret_val = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);

	if (ret_val) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cb))) {
			GtkTextIter text_iter_start, text_iter_end;
			GtkTextBuffer *text_buffer;

			text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (entry));
			gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
			gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);

			*retract_text = gtk_text_buffer_get_text (text_buffer, &text_iter_start, 
					&text_iter_end, FALSE);
		}
	}

	gtk_widget_destroy ((GtkWidget *) dialog);

	return ret_val;
}
