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
#include <gtk/gtkstock.h>
#include <bonobo/bonobo-i18n.h>
#include <widgets/misc/e-source-selector.h>
#include "copy-source-dialog.h"

typedef struct {
	GtkWidget *dialog;
	GtkWidget *selector;
	ESourceList *source_list;
	GConfClient *conf_client;
	ESource *orig_source;
	CalObjType obj_type;
} CopySourceDialogData;

static void
primary_selection_changed_cb (ESourceSelector *selector, CopySourceDialogData *csdd)
{
	ESource *source;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (csdd->selector));
	if (source) {
		if (source != csdd->orig_source)
			gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd->dialog), GTK_RESPONSE_OK, TRUE);
		else
			gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd->dialog), GTK_RESPONSE_OK, FALSE);
	} else
		gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd->dialog), GTK_RESPONSE_OK, FALSE);
}

static gboolean
copy_source (CopySourceDialogData *csdd)
{
	char *uri;
	ESource *dest_source;
	ECal *source_client, *dest_client;
	GList *obj_list = NULL;
	gboolean result = FALSE;

	dest_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (csdd->selector));
	if (!dest_source)
		return FALSE;

	/* open the source */
	source_client = e_cal_new (csdd->orig_source, csdd->obj_type);
	if (!e_cal_open (source_client, TRUE, NULL)) {
		g_object_unref (source_client);
		g_warning (G_STRLOC ": Could not open source");
		return FALSE;
	}

	/* open the destination */
	dest_client = e_cal_new (dest_source, csdd->obj_type);
	if (!e_cal_open (dest_client, FALSE, NULL)) {
		g_object_unref (dest_client);
		g_object_unref (source_client);
		g_warning (G_STRLOC ": Could not open destination");
		return FALSE;
	}

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

	return result;
}

/**
 * copy_source_dialog
 *
 * Implements the Copy command for sources, allowing the user to select a target
 * source to copy to.
 */
gboolean
copy_source_dialog (GtkWindow *parent, ESource *source, CalObjType obj_type)
{
	CopySourceDialogData csdd;
	gboolean result = FALSE;
	const char *gconf_key;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (obj_type == CALOBJ_TYPE_EVENT)
		gconf_key = "/apps/evolution/calendar/sources";
	else if (obj_type == CALOBJ_TYPE_TODO)
		gconf_key = "/apps/evolution/tasks/sources";
	else
		return FALSE;

	csdd.orig_source = source;
	csdd.obj_type = obj_type;

	/* create the dialog */
	csdd.dialog = gtk_dialog_new_with_buttons (_("Copy"), parent, 0,
						   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						   GTK_STOCK_OK, GTK_RESPONSE_OK,
						   NULL);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (csdd.dialog), GTK_RESPONSE_OK, FALSE);

	csdd.conf_client = gconf_client_get_default ();
	csdd.source_list = e_source_list_new_for_gconf (csdd.conf_client, gconf_key);
	csdd.selector = e_source_selector_new (csdd.source_list);
	g_signal_connect (G_OBJECT (csdd.selector), "primary_selection_changed",
			  G_CALLBACK (primary_selection_changed_cb), &csdd);
	gtk_widget_show (csdd.selector);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (csdd.dialog)->vbox), csdd.selector, TRUE, TRUE, 6);

	if (gtk_dialog_run (GTK_DIALOG (csdd.dialog)) == GTK_RESPONSE_OK) {
		result = copy_source (&csdd);
	}

	/* free memory */
	g_object_unref (csdd.conf_client);
	g_object_unref (csdd.source_list);
	gtk_widget_destroy (csdd.dialog);

	return result;
}
