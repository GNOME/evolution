/* Evolution calendar - Copy source dialog
 *
 * Copyright (C) 2003 Novell, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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

#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <bonobo/bonobo-i18n.h>
#include <widgets/misc/e-source-option-menu.h>
#include "copy-source-dialog.h"
#include "common/authentication.h"

typedef struct {
	GtkWidget *dialog;
	GtkWidget *selector;
	ESourceList *source_list;
	GConfClient *conf_client;
	ESource *orig_source;
	ECalSourceType obj_type;
	ESource *selected_source;
} CopySourceDialogData;

static void
source_selected_cb (ESourceOptionMenu *menu, ESource *selected_source, gpointer user_data)
{
	CopySourceDialogData *csdd = user_data;

	csdd->selected_source = selected_source;
	if (selected_source) {
		if (selected_source != csdd->orig_source)
			gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd->dialog), GTK_RESPONSE_OK, TRUE);
		else {
			gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd->dialog), GTK_RESPONSE_OK, FALSE);
			csdd->selected_source = NULL;
		}
	} else
		gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd->dialog), GTK_RESPONSE_OK, FALSE);
}

static void
show_error (GtkWindow *parent, const char *msg)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (parent, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
copy_source (CopySourceDialogData *csdd)
{
	char *uri;
	ECal *source_client, *dest_client;
	gboolean read_only = TRUE;
	GList *obj_list = NULL;
	gboolean result = FALSE;

	if (!csdd->selected_source)
		return FALSE;

	/* open the source */
	source_client = auth_new_cal_from_source (csdd->orig_source, csdd->obj_type);
	if (!e_cal_open (source_client, TRUE, NULL)) {
		show_error (GTK_WINDOW (csdd->dialog), _("Could not open source"));
		g_object_unref (source_client);
		return FALSE;
	}

	/* open the destination */
	dest_client = auth_new_cal_from_source (csdd->selected_source, csdd->obj_type);
	if (!e_cal_open (dest_client, FALSE, NULL)) {
		show_error (GTK_WINDOW (csdd->dialog), _("Could not open destination"));
		g_object_unref (dest_client);
		g_object_unref (source_client);
		return FALSE;
	}

	/* check if the destination is read only */
	e_cal_is_read_only (dest_client, &read_only, NULL);
	if (read_only) {
		show_error (GTK_WINDOW (csdd->dialog), _("Destination is read only"));
	} else {
		if (e_cal_get_object_list (source_client, "#t", &obj_list, NULL)) {
			GList *l;
			const char *uid;
			icalcomponent *icalcomp;

			for (l = obj_list; l != NULL; l = l->next) {
				/* FIXME: process recurrences */
				/* FIXME: process errors */
				if (e_cal_get_object (dest_client, icalcomponent_get_uid (l->data), NULL,
						      &icalcomp, NULL)) {
					e_cal_modify_object (dest_client, icalcomp, CALOBJ_MOD_ALL, NULL);
				} else {
					e_cal_create_object (dest_client, l->data, (char **) &uid, NULL);
					g_free ((gpointer) uid);
				}
			}

			e_cal_free_object_list (obj_list);
		}
	}

	/* free memory */
	g_object_unref (dest_client);
	g_object_unref (source_client);

	return result;
}

/**
 * copy_source_dialog
 *
 * Implements the Copy command for sources, allowing the user to select a target
 * source to copy to.
 */
gboolean
copy_source_dialog (GtkWindow *parent, ESource *source, ECalSourceType obj_type)
{
	CopySourceDialogData csdd;
	gboolean result = FALSE;
	const char *gconf_key;
	GtkWidget *label;
	gchar *label_text;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (obj_type == E_CAL_SOURCE_TYPE_EVENT)
		gconf_key = "/apps/evolution/calendar/sources";
	else if (obj_type == E_CAL_SOURCE_TYPE_TODO)
		gconf_key = "/apps/evolution/tasks/sources";
	else
		return FALSE;

	csdd.orig_source = source;
	csdd.selected_source = NULL;
	csdd.obj_type = obj_type;

	/* create the dialog */
	csdd.dialog = gtk_dialog_new_with_buttons (_("Copy"), parent, 0,
						   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						   GTK_STOCK_OK, GTK_RESPONSE_OK,
						   NULL);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd.dialog), GTK_RESPONSE_OK, FALSE);

	label_text = g_strdup_printf (_("Select destination %s"),
				      obj_type == E_CAL_SOURCE_TYPE_EVENT ?
				      _("calendar") : _("task list"));
	label = gtk_label_new (label_text);
	g_free (label_text);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (csdd.dialog)->vbox), label, FALSE, FALSE, 6);

	csdd.conf_client = gconf_client_get_default ();
	csdd.source_list = e_source_list_new_for_gconf (csdd.conf_client, gconf_key);
	csdd.selector = e_source_option_menu_new (csdd.source_list);
	g_signal_connect (G_OBJECT (csdd.selector), "source_selected",
			  G_CALLBACK (source_selected_cb), &csdd);
	gtk_widget_show (csdd.selector);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (csdd.dialog)->vbox), csdd.selector, FALSE, FALSE, 6);

	if (gtk_dialog_run (GTK_DIALOG (csdd.dialog)) == GTK_RESPONSE_OK) {
		result = copy_source (&csdd);
	}

	/* free memory */
	g_object_unref (csdd.conf_client);
	g_object_unref (csdd.source_list);
	gtk_widget_destroy (csdd.dialog);

	return result;
}
